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

The executable is created as:

```text
build/okigraph1-test
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

## Convert a monochrome PBM image

A second utility converts a binary PBM (P4) bitmap directly to the same native
OkiGraph I stream:

```sh
./build/okigraph1-pbm \
  --model 83a \
  -o image.oki \
  image.pbm
```

This converter intentionally performs **no hidden scaling**. One PBM pixel is
one native 60-column/in graphics column horizontally, and each group of seven
PBM rows becomes one seven-pin print band. That makes it useful for protocol
and geometry experiments before CUPS resampling is introduced.

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

After the raw stream is physically validated, the core encoder will be kept
independent of CUPS and wrapped by a monochrome raster filter.  That filter can
then perform explicit source-raster-to-native-band mapping instead of relying
on generic Epson/Okidata 72-dpi assumptions.

See `docs/OKIGRAPH1-PROTOCOL.md` for the current protocol model.

## Firmware analysis

`firmware-analysis/` is reserved for the 82A/83A stock/OkiGraph I/IBM PnP
provenance and disassembly package.  Public artifacts should contain original
analysis, hashes, metadata, source maps, scripts, and annotations while raw ROM
images remain private unless redistribution rights are established.
