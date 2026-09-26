# Okidata MICROLINE 82A/83A + OkiGraph I

Firmware provenance, OkiGraph I protocol analysis, native raster conversion,
and a CUPS 2.x printer driver for the Okidata MICROLINE 82A and 83A with
OkiGraph I firmware.

## Version 1.0

Version 1.0 is the first public driver release. The ML82A path has been
validated on physical hardware from raw OkiGraph test streams through CUPS
2.3.3op2, multi-page jobs, and a Windows client printing through a shared CUPS
queue. The ML83A uses the same encoder, raster mapper, and CUPS filter; its PPD
changes the carriage limit to 792 native columns and adds the wide fanfold page.

The engineering history, firmware findings, geometry derivation, raster
algorithm, CUPS architecture, and hardware validation are documented in
[`docs/ENGINEERING-REPORT-1.0.md`](docs/ENGINEERING-REPORT-1.0.md).

## Build from source

On Linux:

```sh
make
make check
make cups
```

The main executables are:

```text
build/okigraph1-test
build/okigraph1-pbm
build/okigraph1-mktest
build/rastertookigraph1
```

The CUPS filter requires the CUPS development headers. On Debian Bullseye,
`libcups2-dev` supplies the headers and `cups-config`.

Install the filter and both PPDs with:

```sh
sudo sh scripts/install-cups.sh
```

Then create a queue. For the USB-to-parallel adapters validated during this
project, the reliable path is the Linux `usblp` device through CUPS' parallel
backend:

```sh
sudo sh scripts/configure-cups-queue.sh 82a
```

That defaults to:

```text
parallel:/dev/usb/lp0
```

For an ML83A:

```sh
sudo sh scripts/configure-cups-queue.sh 83a
```

A different queue name or device URI can be supplied as the second and third
arguments. See `docs/CUPS-DRIVER.md`.

## Prebuilt release bundle

The v1.0.0 GitHub release includes a Debian 11/Bullseye amd64 binary bundle
built against the CUPS 2.3.x development interface. The release bundle already
contains `build/rastertookigraph1`; `scripts/install-cups.sh` detects the
prebuilt bundle and installs it without rebuilding.

Source builds remain the portable path for other Linux architectures and
distributions.

## Native OkiGraph I geometry

The firmware work and physical measurements established the geometry used by
the driver:

```text
horizontal graphics pitch: 60 columns/inch
native graphics band feed: 15/144 inch
native bands/inch:         9.6
host graphics pins/band:   7
ML82A carriage:            480 columns = 8.0 inches
ML83A carriage:            792 columns = 13.2 inches
```

The vertical output is not a uniform square-DPI raster. The current mapper
models adjacent physical head pins at 1/72 inch while band origins advance by
15/144 inch. It therefore uses the actual nonuniform target positions instead
of pretending the printer is a generic 72-DPI Epson-compatible device.

## Generate a raw calibration stream

```sh
./build/okigraph1-test \
  --model 82a \
  --pattern calibration \
  -o ml82a-calibration.oki
```

The default calibration page contains 49 native seven-pin bands. Band 0 and
band 48 are exactly 48 graphics-feed commands apart. On the physical ML82A,
that distance measured exactly 5.000 inches with calipers.

Useful variants:

```sh
./build/okigraph1-test --model 82a --pattern seam  -o ml82a-seam.oki
./build/okigraph1-test --model 82a --pattern ruler -o ml82a-ruler.oki
```

## Convert a conventional raster image

With `--source-dpi`, `okigraph1-pbm` converts a normal square-DPI PBM into
native OkiGraph geometry while preserving physical size:

```sh
./build/okigraph1-pbm \
  --model 82a \
  --source-dpi 360 \
  --threshold 50 \
  -o image.oki \
  image.pbm
```

A deterministic 360-DPI regression image and native stream can be produced
with:

```sh
make test-raster-stream
```

See `docs/RASTER-MAPPER.md` for the mapping algorithm.

## CUPS architecture

The v1.0 path is:

```text
application / Windows client
          |
          v
        CUPS
          |
          v
pdftopdf / gstoraster
          |
          v
application/vnd.cups-raster
          |
          v
rastertookigraph1
          |
          v
okigraph1-raster physical mapper
          |
          v
okigraph1 native stream encoder
          |
          v
parallel:/dev/usb/lp0
          |
          v
Linux usblp -> USB/parallel bridge -> ML82A/83A
```

The CUPS-facing filter deliberately does not duplicate the printer geometry.
Both standalone conversion and CUPS use the same `okigraph1-raster.c` mapper.

## USB-to-parallel note

During validation, CUPS also discovered the generic bridge as
`usb://Unknown/Printer`. That libusb path advertised bidirectional protocol
2, started a backchannel read thread, and became unreliable under VirtualBox
USB passthrough. Forcing `usb-unidir` helped, but the robust solution was to
use the kernel `usblp` device through:

```text
parallel:/dev/usb/lp0
```

With that backend the printer completed consecutive multi-page jobs and jobs
submitted from Windows without requiring USB detach/reattach cycles.

Other adapters and non-virtualized hosts may expose different reliable device
URIs; the documented `parallel:/dev/usb/lp0` path is the hardware-validated
configuration for this project.

## Documentation

- `docs/ENGINEERING-REPORT-1.0.md` - start-to-finish engineering report
- `docs/OKIGRAPH1-PROTOCOL.md` - protocol semantics recovered from firmware
- `docs/RASTER-MAPPER.md` - physical raster mapping algorithm
- `docs/HARDWARE-VALIDATION.md` - measurement and print validation
- `docs/CUPS-DRIVER.md` - build, install, queue, and troubleshooting guide
- `firmware-analysis/` - provenance/disassembly framework

## Firmware images

Raw EPROM images are intentionally not redistributed in this repository.
Public material contains original analysis, metadata, hashes, structural
findings, source maps, and derived protocol information.
