# Okidata MICROLINE 82A/83A + OkiGraph I

Firmware provenance, OkiGraph I protocol analysis, and Linux printing tools
for the Okidata MICROLINE 82A and 83A.

The first executable in this repository is a deliberately small raw OkiGraph I
calibration generator.  It bypasses Ghostscript/CUPS geometry assumptions so
the real 82A/83A printer mechanics can be validated before the CUPS raster
filter is written.

## Build

On a Linux system with a C compiler:

```sh
make
```

The main test/conversion executables are:

```text
build/okigraph1-test
build/okigraph1-pbm
build/okigraph1-mktest
```

Run the dependency-free regression tests with:

```sh
make check
```

## Generate a calibration stream

```sh
./build/okigraph1-test \
  --model 82a \
  --pattern calibration \
  -o ml82a-calibration.oki
```

The default page contains 49 native seven-pin graphics bands.  Band 0 and band
48 are exactly 48 native graphics-feed commands apart.  The firmware-derived
motion model predicts a 5.000-inch separation between their top-pin baselines.

Additional useful streams:

```sh
./build/okigraph1-test --model 82a --pattern seam  -o ml82a-seam.oki
./build/okigraph1-test --model 82a --pattern ruler -o ml82a-ruler.oki
```

## Convert a conventional raster image

`okigraph1-pbm` now has two modes.

Without a source DPI it retains the original direct/native mapping used for
protocol experiments. With `--source-dpi`, it converts an ordinary square-DPI
PBM into the physically validated OkiGraph geometry:

```sh
./build/okigraph1-pbm \
  --model 82a \
  --source-dpi 360 \
  --threshold 50 \
  -o image.oki \
  image.pbm
```

The mapper preserves physical size rather than pretending the printer has a
uniform 72-DPI vertical raster. Horizontally it targets 60 columns/inch.
Vertically it places the seven head pins at their physical 1/72-inch pitch
while successive band origins advance by the validated 15/144 inch.

A deterministic 360-DPI source-raster test can be built with:

```sh
make test-raster-stream
```

This produces an 8 x 6 inch test source and the corresponding ML82A OkiGraph
stream in `build/`. See `docs/RASTER-MAPPER.md` for the exact mapping model.

## Native CUPS driver

The repository now includes a classic CUPS 2.x raster filter and PPDs for both
printers:

```text
src/rastertookigraph1.c
ppd/okidata-ml82a-okigraph1.ppd
ppd/okidata-ml83a-okigraph1.ppd
```

Build and install them with:

```sh
make cups
sudo sh scripts/install-cups.sh
```

See `docs/CUPS-DRIVER.md` for queue creation and first-print instructions.

The ML82A driver path has now been validated end-to-end on CUPS 2.3.3op2 with
a PDF containing a large JPEG: page fill and aspect ratio were correct, with no
missing raster lines or visible band-gap defects.

## Send raw data through CUPS

Create a CUPS queue that points at the USB-to-parallel adapter, then send the
file without format conversion:

```sh
lp -d ML83A -o raw ml83a-calibration.oki
```

When a direct Linux parallel character device is available, a raw stream can
also be tested without the CUPS raster/filter path:

```sh
cat ml83a-calibration.oki > /dev/usb/lp0
```

Use the CUPS raw queue first if the USB adapter is already managed by CUPS.

## What to inspect on paper

1. **Horizontal scale** - vertical ruler marks are spaced every 60 graphics
   columns, nominally one inch.
2. **Band seams** - the dense field at the left makes unwanted horizontal gaps
   between native seven-pin bands easy to see.
3. **Five-inch feed check** - measure from the top-dot baseline of band 0 to
   the same baseline in band 48.
4. **Pin ordering** - the diagonal field walks through bits/pins 0..6 and makes
   reversed or shifted bit mappings obvious.

The ML82A hardware validation completed on 2026-09-26: 48 native graphics
feeds measured exactly 5.000 inches with calipers, and the horizontal ruler
confirmed the expected 60-column/inch geometry. See
`docs/HARDWARE-VALIDATION.md`.

## Next phase

The source-raster-to-native-band mapper remains independent of CUPS and is
covered by regression tests. The initial `rastertookigraph1` CUPS 2.x wrapper
now feeds that same mapper, so the hardware-validated geometry is not
duplicated in the CUPS-facing code.

See `docs/OKIGRAPH1-PROTOCOL.md` for the current protocol model.

## Firmware analysis

`firmware-analysis/` is reserved for the 82A/83A stock/OkiGraph I/IBM PnP
provenance and disassembly package.  Public artifacts should contain original
analysis, hashes, metadata, source maps, scripts, and annotations while raw ROM
images remain private unless redistribution rights are established.
