# OkiGraph I raster mapper

The native OkiGraph I graphics geometry is not a simple rectangular DPI grid.

## Confirmed physical model

The ML82A hardware test established:

```text
horizontal carriage pitch: 60 columns/inch
graphics band feed:        15/144 inch
bands/inch:                9.6
graphics pins/band:        7
```

The current head model uses the normal 9-pin mechanical pitch of 1/72 inch
between adjacent pins. OkiGraph I exposes seven of those pins to the host
graphics path.

That gives target dot-center positions, measured in 1/144-inch units:

```text
band 0:  0, 2, 4, 6, 8, 10, 12
band 1: 15,17,19,21,23,25,27
band 2: 30,32,34,36,38,40,42
...
```

So the spacing is 1/72 inch inside each seven-pin band, followed by a 3/144
inch step from the bottom pin of one band to the top pin of the next band.
The average row density is therefore not the same thing as a uniform vertical
DPI.

## Why the mapper uses 1/288-inch units

The implementation in `src/okigraph1-raster.c` uses 1/288 inch as its exact
integer vertical coordinate system:

```text
1/72 inch     = 4/288
15/144 inch   = 30/288
```

This lets the raster mapper avoid floating-point accumulation error and keeps
every native pin center and band origin exact.

## Source raster conversion

A conventional square-DPI source bitmap can be supplied to
`okigraph1-pbm`:

```sh
./build/okigraph1-pbm \
  --model 82a \
  --source-dpi 360 \
  --threshold 50 \
  -o output.oki \
  source.pbm
```

The mapper preserves physical size:

- source X is reduced to the native 60-column/inch carriage grid;
- source Y is sampled against the nonuniform physical pin-center grid;
- each target dot owns the source pixels whose centers fall between the
  midpoints to its neighboring target dots;
- a target dot fires when the black coverage in that cell reaches the selected
  threshold.

The default threshold is 50 percent. Lower values preserve thinner source
features but darken output; higher values suppress isolated source pixels.

## Native PBM mode

For protocol work, omitting `--source-dpi` keeps the original direct mode.
In that mode:

```text
1 PBM column = 1 native 60-column/inch printer column
7 PBM rows   = 1 native seven-pin printer band
```

Those seven PBM rows are logical pin rows, not a conventional rectangular
vertical DPI raster.

## Deterministic 360 DPI test

The repository includes a source-raster generator:

```sh
make test-raster-stream
```

It creates:

```text
build/okigraph1-360-test.pbm
build/ml82a-360-test.oki
```

The PBM is exactly 2880 x 2160 pixels at 360 DPI, or 8 x 6 inches. The
conversion should report:

```text
480 native columns
404 physical dot rows
58 seven-pin bands
```

The generated page contains an 8 x 6 inch frame, one-inch rulers, diagonals,
and several coverage patterns. It is intended to validate physical-size
preservation before the same mapper is wrapped by a CUPS raster filter.

## Regression tests

Run:

```sh
make check
```

The current tests verify that:

- a 360 x 360 source bitmap at 360 DPI becomes 60 native columns across one
  physical inch;
- the same one-inch source height maps to the expected nonuniform native
  dot-row sequence;
- an 8.5-inch-wide source raster clips from 510 requested native columns to the
  ML82A carriage limit of 480 columns.
