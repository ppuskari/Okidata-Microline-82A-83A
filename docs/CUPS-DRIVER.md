# CUPS driver installation and first queue

This is the first CUPS wrapper around the already hardware-validated OkiGraph I
raster mapper.

The filter does not duplicate printer geometry. It converts the CUPS page
bitmap into the packed monochrome bitmap consumed by `okigraph1-raster.c`,
then uses the existing native OkiGraph I encoder.

## Initial target

The first CUPS configuration is intentionally conservative:

- classic CUPS 2.x / PPD driver path;
- 360 x 360 DPI source raster;
- monochrome output;
- ML82A carriage limit: 480 native columns (8 inches);
- ML83A carriage limit: 792 native columns (13.2 inches);
- 50 percent mapper coverage threshold;
- native OkiGraph I seven-pin output.

The physical output remains the validated geometry:

```text
horizontal graphics pitch: 60 columns/inch
pin pitch inside a band:    1/72 inch
band-origin advance:        15/144 inch
graphics pins per band:     7
```

## Build prerequisites

On Debian Bullseye the filter needs the CUPS development headers in addition
to the normal C compiler:

```sh
cups-config --version
```

If `cups-config` is missing, install the CUPS development package
(`libcups2-dev` on Debian Bullseye) before running `make cups`.

## Build

```sh
make
make check
make cups
```

The CUPS filter is:

```text
build/rastertookigraph1
```

## Validate the PPDs

If `cupstestppd` is installed:

```sh
cupstestppd ppd/okidata-ml82a-okigraph1.ppd
cupstestppd ppd/okidata-ml83a-okigraph1.ppd
```

## Install

The scripts are stored in Git as ordinary text files, so this form works
regardless of their executable bit:

```sh
sudo sh scripts/install-cups.sh
```

The script asks `cups-config` for the system CUPS directories, installs the
filter into the CUPS filter directory, and installs both PPDs below the CUPS
model directory.

After installation:

```sh
lpinfo -m | grep -i OkiGraph
lpinfo -v
```

Use the actual device URI reported by `lpinfo -v`. A USB-to-parallel adapter
may appear as a `usb://...` URI or through another CUPS backend depending on
the adapter and kernel configuration. Do not substitute a guessed URI when
CUPS reports a concrete one.

## Create the ML82A queue

First identify the installed PPD path:

```sh
cups-config --datadir
```

On a typical Debian system it will be under:

```text
/usr/share/cups/model/okigraph1/okidata-ml82a-okigraph1.ppd
```

Then create the queue, replacing `DEVICE_URI` with the URI from
`lpinfo -v`:

```sh
sudo lpadmin \
  -p ML82A-OkiGraphI \
  -E \
  -v 'DEVICE_URI' \
  -P /usr/share/cups/model/okigraph1/okidata-ml82a-okigraph1.ppd
```

Verify:

```sh
lpstat -t
lpoptions -p ML82A-OkiGraphI -l
```

## First CUPS print

Use a simple one-page PDF first:

```sh
lp -d ML82A-OkiGraphI test.pdf
```

Watch the CUPS log in another terminal:

```sh
sudo tail -f /var/log/cups/error_log
```

For additional filter diagnostics, temporarily enable debug logging:

```sh
sudo cupsctl --debug-logging
```

and turn it back off after the test:

```sh
sudo cupsctl --no-debug-logging
```

The filter writes page geometry and mapper statistics using normal CUPS
`DEBUG:` records.

## Page geometry

The ML82A PPD exposes US Letter and US Legal with an 8-inch imageable width.
The ML83A PPD additionally exposes a 14 x 11 fanfold page whose imageable width
is 13.2 inches.

The PPD imageable-area margins define which part of the application page CUPS
rasterizes. The printer's physical left/top paper alignment remains mechanical,
as expected for this generation of tractor/single-sheet dot-matrix printer.

## Raster formats accepted by the filter

The PPD requests one-bit K raster data. For diagnostic robustness the filter
also accepts:

- one-bit K;
- one-bit W/SW, with luminance polarity converted;
- eight-bit K;
- eight-bit W/SW.

Internally all of these are normalized to the mapper convention:

```text
packed bitmap, MSB first, 1 = black
```

## Uninstall

```sh
sudo sh scripts/uninstall-cups.sh
```
