# OkiGraph I CUPS driver — version 1.0

This document covers the CUPS 2.x wrapper around the hardware-validated
OkiGraph I raster mapper.

The filter converts a CUPS monochrome raster page into the packed bitmap used
by `okigraph1-raster.c`, then emits the native OkiGraph I byte stream through
`okigraph1.c`.

## Supported models

### MICROLINE 82A + OkiGraph I

- 480 native graphics columns
- 8.0-inch native graphics width at 60 columns/inch
- US Letter and US Legal PPD page sizes

### MICROLINE 83A + OkiGraph I

- 792 native graphics columns
- 13.2-inch native graphics width at 60 columns/inch
- US Letter, US Legal, and 14 x 11 fanfold PPD page sizes

Both models use the same renderer, raster mapper, and OkiGraph encoder. The
PPD-selected carriage width is the main model-specific rendering parameter.

## Raster model

The CUPS PPD requests a 360 x 360 DPI monochrome source raster. That source is
not sent directly to the printer. It is resampled into the OkiGraph physical
grid:

```text
horizontal graphics pitch: 60 columns/inch
current intra-band model:   1/72 inch between adjacent pins
band-origin advance:        15/144 inch
graphics pins per band:     7
coverage threshold:         50 percent
```

The 15/144-inch band advance is directly firmware-derived and was physically
confirmed on the ML82A: 48 graphics feeds measured exactly 5.000 inches.
The 1/72-inch intra-band pin pitch is the current mechanical model and has
produced correct real output; it should not be confused with the independently
measured band advance.

## Build from source

On Debian Bullseye:

```sh
make
make check
make cups
```

If `cups-config` is missing, install the CUPS development package first
(`libcups2-dev` on Debian Bullseye).

Validate the PPDs:

```sh
cupstestppd ppd/okidata-ml82a-okigraph1.ppd
cupstestppd ppd/okidata-ml83a-okigraph1.ppd
```

Both v1.0 PPDs pass `cupstestppd` on CUPS 2.3.3op2.

## Install

From a source checkout:

```sh
sudo sh scripts/install-cups.sh
```

The script builds `build/rastertookigraph1` when necessary, then installs:

```text
<CUPS serverbin>/filter/rastertookigraph1
<CUPS datadir>/model/okigraph1/okidata-ml82a-okigraph1.ppd
<CUPS datadir>/model/okigraph1/okidata-ml83a-okigraph1.ppd
```

The v1.0 prebuilt release bundle contains `PREBUILT-BUNDLE` and an already
built `build/rastertookigraph1`. In that bundle the same installer uses the
prebuilt binary instead of requiring the compiler and CUPS development headers.

## Create a queue

For the generic USB-to-parallel bridge used during development and validation,
the stable configuration is the Linux `usblp` device through the CUPS
parallel backend.

ML82A:

```sh
sudo sh scripts/configure-cups-queue.sh 82a
```

ML83A:

```sh
sudo sh scripts/configure-cups-queue.sh 83a
```

The default URI is:

```text
parallel:/dev/usb/lp0
```

The script accepts optional queue name and URI arguments:

```sh
sudo sh scripts/configure-cups-queue.sh \
  82a My-Oki-82A parallel:/dev/usb/lp0
```

Verify:

```sh
lpstat -t
lpoptions -p ML82A-OkiGraphI -l
```

## Why not `usb://Unknown/Printer` on the validated setup?

CUPS 2.3.3op2 discovered the generic bridge as:

```text
usb://Unknown/Printer
```

That selects the CUPS libusb backend. The adapter advertised USB printer
protocol 2 (bidirectional), so the backend opened a backchannel reader.
`usb-unidir=yes` prevented one class of hang, but with VirtualBox USB
passthrough the device could still end up requiring a detach/reattach before
held jobs would continue.

Switching the queue to:

```text
parallel:/dev/usb/lp0
```

keeps ownership with Linux `usblp` and eliminated the reattachment problem.
Multiple pages and consecutive jobs then printed normally.

This does not mean every `usb://` printer URI is invalid. It documents the
known-good transport for the generic USB-to-Centronics adapter used by this
project.

## First print

A local PDF:

```sh
lp -d ML82A-OkiGraphI test.pdf
```

CUPS' built-in test page is also useful once the queue backend is correct.

For temporary diagnostics:

```sh
sudo cupsctl --debug-logging
sudo tail -f /var/log/cups/error_log
```

Disable debug logging afterward:

```sh
sudo cupsctl --no-debug-logging
```

The filter writes page geometry and mapper statistics with normal CUPS
`DEBUG:` records.

## CUPS filter pipeline

On the validated Debian/CUPS 2.3.3op2 system, a PDF job followed:

```text
application/pdf
    -> pdftopdf
    -> application/vnd.cups-pdf
    -> gstoraster
    -> application/vnd.cups-raster
    -> rastertookigraph1
    -> printer byte stream
```

The custom filter itself receives only the CUPS raster and does not need to
understand PDF, PostScript, JPEG, Windows GDI output, or application formats.

## Raster formats accepted by `rastertookigraph1`

The PPD requests one-bit K raster. The filter also accepts:

- one-bit K
- one-bit W/SW
- eight-bit K
- eight-bit W/SW

All accepted formats are normalized to:

```text
packed bitmap, MSB first, 1 = black
```

K and W/SW have opposite polarity conventions, and the filter handles that
explicitly.

## Multi-page behavior

The first page begins an OkiGraph job with CAN, enters graphics mode, renders
the native bands, exits graphics, and emits FF. Subsequent pages re-enter
graphics without sending another CAN. Each page is therefore independently
terminated and ejected while the CUPS filter remains alive for the complete
job.

Multiple-page output was physically validated on the ML82A.

## Windows/shared-CUPS validation

A Windows client printed the original mixed text/graphics test document through
the shared CUPS queue successfully after the queue was changed to the
`parallel:/dev/usb/lp0` backend. This validated the normal network-client
path in addition to local Linux jobs.

## Uninstall

```sh
sudo sh scripts/uninstall-cups.sh
```

Queue deletion remains a separate CUPS administration action.
