# OkiGraph I CUPS Driver 1.0.0

First stable public release of the native OkiGraph I Linux/CUPS path for
Okidata MICROLINE 82A and 83A printers.

## Highlights

- Native OkiGraph I seven-pin encoder derived from firmware analysis.
- Physical 60-column/inch horizontal mapping.
- Firmware-derived 15/144-inch graphics band advance.
- Nonuniform vertical raster mapper using exact integer physical coordinates.
- 360-DPI CUPS source raster with 50% physical-cell coverage threshold.
- ML82A PPD: 480 native columns.
- ML83A PPD: 792 native columns plus wide fanfold page definition.
- Classic CUPS 2.x `rastertookigraph1` filter.
- Source and prebuilt installation paths.
- Queue configuration helper.
- Full engineering report covering firmware-to-driver derivation.

## Hardware validation

The ML82A path was validated on physical hardware with:

- 48 native graphics feeds measuring exactly 5.000 inches;
- direct native calibration streams;
- deterministic 360-DPI source-image mapping;
- large-image PDF printing through CUPS 2.3.3op2;
- CUPS test material;
- consecutive multi-page jobs;
- a Windows client printing mixed text/graphics through the shared CUPS queue.

The ML83A release uses the same encoder, physical mapper, and CUPS filter with
the 792-column carriage limit and wide-page PPD definitions.

## Important USB-to-parallel setup note

The validated VirtualBox/Linux setup is stable with:

```text
parallel:/dev/usb/lp0
```

using Linux `usblp`.

The generic adapter was also discovered as `usb://Unknown/Printer`, but that
libusb path advertised bidirectional protocol 2 and could leave jobs held until
the USB device was detached/reattached. `usb-unidir` helped diagnose the
problem; the parallel backend is the reliable configuration for the tested
bridge.

## Release assets

- Debian 11/Bullseye amd64 prebuilt driver bundle
- standalone prebuilt `rastertookigraph1` binary
- SHA-256 checksum file
- GitHub-generated source archives

The prebuilt bundle contains `build/rastertookigraph1`, both PPDs,
installation/configuration scripts, source, tests, and documentation.

## Quick start — source

```sh
make
make check
make cups
sudo sh scripts/install-cups.sh
sudo sh scripts/configure-cups-queue.sh 82a
```

## Quick start — prebuilt release bundle

Extract the bundle and run:

```sh
sudo sh scripts/install-cups.sh
sudo sh scripts/configure-cups-queue.sh 82a
```

For an ML83A, replace `82a` with `83a`.

See `docs/ENGINEERING-REPORT-1.0.md` and `docs/CUPS-DRIVER.md` for details.
