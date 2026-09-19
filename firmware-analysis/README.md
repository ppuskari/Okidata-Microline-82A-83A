# Firmware provenance/disassembly package

This directory is reserved for the MICROLINE 82A/83A firmware provenance and
disassembly material developed alongside the OkiGraph I driver work.

The public package should contain original research artifacts such as:

* canonical Q5/Q6 source maps;
* ROM metadata and cryptographic hash manifests;
* part/revision and canonical-match records;
* structural signatures and similarity/classification results;
* conceptual cross-family symbol dictionary;
* byte accounting (`CODE`, `TABLE`, `LITERAL/DATA`, `FILL/RESERVED`,
  `UNRESOLVED`);
* unresolved-items ledger;
* analysis scripts and reproducible reports;
* archive taxonomy for `82A/83A -> Stock/OkiGraphI/IBMPnP/Unknown`.

## ROM images

Raw EPROM/ROM binary dumps are intentionally excluded from this public source
tree.  The repository can document filenames, sizes, CRC32/SHA-256 values,
part numbers, revisions, and analysis results without redistributing firmware
binaries.  Local/private dumps can be placed under `firmware-analysis/roms/`;
that directory is ignored by Git.
