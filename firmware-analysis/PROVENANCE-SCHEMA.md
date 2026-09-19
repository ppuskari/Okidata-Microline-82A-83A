# EPROM provenance record schema

Each analyzed EPROM image should have a record with, at minimum:

| Field | Purpose |
|---|---|
| `machine` | `82A` or `83A` |
| `family` | `Stock`, `OkiGraphI`, `IBMPnP`, or `Unknown` |
| `socket` | `Q4`, `Q5`, `Q6`, etc. |
| `filename` | Local source filename; no binary is required in the public repo |
| `size` | Byte count |
| `crc32` | Fast archival checksum |
| `sha256` | Canonical cryptographic identity |
| `part_number` | Marked EPROM/ROM part number when known |
| `revision` | Marked or inferred revision |
| `canonical_match` | Canonical image or set to which this dump matches |
| `similarity` | Similarity metric when not byte-identical |
| `structural_signature` | Parser/table/code structural fingerprint |
| `classification_basis` | Evidence supporting the family/revision assignment |
| `notes` | Physical provenance, reader, date, anomalies, repairs, etc. |

## Canonical analysis layers

The disassembly package should maintain separate accounting for:

* `CODE`
* `TABLE`
* `LITERAL/DATA`
* `FILL/RESERVED`
* `UNRESOLVED`

That separation keeps confidence explicit and prevents unresolved bytes from
being silently promoted to code or data.

## Cross-family symbol dictionary

Symbols should be conceptual first and address-specific second.  For example,
a parser state or printer mechanism routine can keep one canonical conceptual
name even when its ROM address differs between 82A, 83A, stock, OkiGraph I,
and IBM PnP branches.
