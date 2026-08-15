# cpp-lsm-db

A learning implementation of an LSM-tree (Log-Structured Merge-tree) key-value
store in C++17, written from scratch with no external dependencies.

LSM-trees are the storage architecture behind LevelDB, RocksDB and Cassandra.
The core idea is to turn random writes into sequential ones: buffer writes in a
sorted in-memory structure, and periodically flush that buffer to disk as an
immutable sorted file rather than updating data in place.

This implements the write path of that design — WAL, memtable, and SSTable
flush. It does **not** yet implement compaction, bloom filters, or deletes; see
[Limitations](#limitations), which is the most useful section of this README.

## Build

```bash
make        # -> ./lsm_db
make run    # runs the demo driver in main.cpp
```

Requires a C++17 compiler. No dependencies.

## How it works

**Write path.** Every `put` is first appended to a write-ahead log, then
inserted into an in-memory `std::map` (the memtable). The WAL exists so that
writes buffered in memory can be replayed if the process dies before they reach
an SSTable.

**Flush.** When the memtable exceeds a size threshold (4 KB here, deliberately
small so the demo triggers flushes), it is written out in sorted order as an
immutable `data_N.sst` file, the memtable is cleared, and the WAL is rotated.
Because the map is already sorted, this write is sequential.

**Read path.** `get` checks the memtable first. On a miss it scans SSTables
newest-to-oldest, so a newer value for a key shadows an older one.

**Recovery.** On startup the WAL is replayed back into the memtable.

```
put(k,v) ──> WAL append ──> memtable (std::map)
                                │  when > 4 KB
                                └──> data_N.sst  (immutable, sorted)

get(k)   ──> memtable ──miss──> data_N.sst, data_N-1.sst, ... (newest first)
```

## Measured behaviour

20,000 keys of ~50 bytes, `-O2`, Linux, warm page cache:

| Operation | Latency |
|---|---|
| `get` hit in memtable | 0.12 µs |
| `get` hit in oldest SSTable | 2,787 µs |
| `get` miss (scans everything) | 2,814 µs |

Those numbers are the honest picture and they point straight at the two
limitations below: reads scan whole files linearly, and nothing merges the 284
SSTables that 20,000 keys produced.

## Limitations

Being specific about what isn't here, because these are the parts that make an
LSM-tree fast and durable, and I'd rather list them than imply they exist.

**No compaction.** SSTables accumulate and are never merged. 20,000 keys
produced 284 files. Compaction is what bounds the number of files a read has to
consult and reclaims space from overwritten keys — it's the defining feature of
an LSM tree and the biggest thing missing here.

**Reads are O(total data size), not O(log N).** SSTables are written in sorted
order but the read path scans them linearly and doesn't stop early. It has no
index and no bloom filter, so a lookup for a key that doesn't exist reads every
byte of every SSTable. That's the 2,814 µs above.

**Durability is weaker than "the data is on disk".** The WAL is flushed with
`ofstream::flush()`, which pushes bytes into the OS page cache — not onto the
physical device. That survives a process crash but not a power failure; the
latter needs `fsync`. Additionally, flush deletes the WAL immediately after
writing an SSTable, without fsyncing it first, so a power loss in that window
could lose both copies.

**WAL recovery doesn't validate records.** A crash mid-write leaves a partial
trailing record. Recovery doesn't detect this, and because it reuses its parse
buffers across records, a truncated record can commit the *previous* record's
value under the new key rather than simply being dropped. Real engines
length-prefix and checksum every record and stop at the first one that fails to
validate.

**No deletes.** There's no `del()`. In an LSM tree deletion means writing a
*tombstone* — a marker that shadows the key — because you can't modify an
immutable file; the tombstone and the key it shadows are dropped during
compaction.

**Empty values are indistinguishable from missing keys.** The SSTable search
returns `""` to signal "not found", so a key genuinely stored with an empty
value reads back as absent.

**Concurrency is serialized, not concurrent.** A single mutex wraps all
operations, and it is held across full-disk scans, so a read blocks all writers
for its duration. It is thread-safe, but there is no parallelism.

## Roadmap

Roughly in order of value-per-effort:

1. Length-prefixed, CRC32-checksummed WAL records; stop recovery at the first
   record that fails to validate. Plus a test that truncates the WAL at every
   byte offset and asserts recovery never returns a wrong value.
2. `fsync` the WAL on append; fsync the SSTable and its directory before
   deleting the WAL.
3. Return `std::optional` throughout the read path so empty values work.
4. A bloom filter per SSTable — the cheapest large win, since it lets a lookup
   skip a file without touching the disk at all.
5. A sparse index in each SSTable footer (every Nth key plus its offset) so a
   lookup binary-searches and seeks instead of scanning.
6. Tombstones and `del()`.
7. Size-tiered or leveled compaction.

## Layout

```
src/db.h      KVStore interface
src/db.cpp    WAL, memtable, flush, recovery, SSTable search
src/main.cpp  demo driver
```
