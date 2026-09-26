# Hardware validation

## 2026-09-26 — MICROLINE 82A + OkiGraph I

Physical test configuration:

- Okidata MICROLINE 82A
- validated OkiGraph I firmware installed
- Linux host
- CUPS 2.3.3op2 for the final driver tests
- USB-to-parallel adapter
- direct raw access through `/dev/usb/lp0`
- final CUPS transport through `parallel:/dev/usb/lp0`

## Native geometry calibration

The 49-band calibration pattern printed correctly using the native OkiGraph I
seven-pin path.

The distance from the top-pin baseline in band 0 to the matching top-pin
baseline in band 48 was measured with calipers at **5.000 inches**.

Therefore the firmware-derived graphics feed is physically validated:

```text
48 feeds = 5.000 inches
1 feed   = 5/48 inch
         = 15/144 inch
         = 0.1041666667 inch
```

That is **9.6 native graphics bands per inch**.

The horizontal ruler printed at approximately the expected 8-inch ML82A
graphics width using 480 native columns, supporting the **60 columns/inch**
horizontal model.

## Seam and registration test

The dedicated seam pattern showed regular paper advance and carriage-return
registration. The vertical reference remained essentially straight through
successive bands, with no visible cumulative staircase, skipped feed, or
doubled feed. Repeated horizontal pin-pair marks remained uniformly spaced.

These results are sufficient to use the recovered native feed geometry as the
basis of the Linux raster mapper.

## Confirmed native geometry

```text
horizontal graphics pitch: 60 columns/inch
native band feed:           15/144 inch
native bands/inch:          9.6
graphics pins per band:     7
```

The seven exposed dot rows within a band and the inter-band motion must be
modeled separately. A generic uniform 72-DPI vertical raster is not an
accurate representation of OkiGraph I motion.

The mapper currently models the physical pin-to-pin spacing inside a band as
1/72 inch. The 15/144-inch band-origin advance, unlike that intra-band model,
was independently measured by the 48-feed/5.000-inch test.

## Source-raster mapper validation

The deterministic source image is 2880 x 2160 pixels at 360 DPI, or exactly
8 x 6 inches. The mapper converts it to 480 native ML82A columns and the
nonuniform seven-pin vertical sequence.

The real print showed:

- correct overall physical proportions
- regular one-inch grid spacing
- clean diagonal geometry
- the two intentional density/coverage blocks at upper left
- no systematic white seams between graphics bands
- no missing raster rows

Together with the 5.000-inch feed measurement, this validates the standalone
source-image-to-OkiGraph mapper on physical hardware.

## End-to-end CUPS validation

The first application-level test used a PDF containing a very large JPEG.
It printed with the correct aspect ratio, filled the expected printable area,
and showed no missing lines, repeating band gaps, or obvious geometry
distortion.

That validated:

```text
PDF/application
    -> CUPS rasterization
    -> rastertookigraph1
    -> okigraph1-raster mapper
    -> native OkiGraph I encoder
    -> printer
```

## USB backend finding

The first CUPS queue used `usb://Unknown/Printer`. CUPS treated the generic
bridge as a bidirectional USB printer. The graphics filter completed normally,
but the libusb/backend ownership and backchannel behavior could leave jobs held
until the USB device was detached and reattached to the VirtualBox guest.

Forcing `usb-unidir` removed the backchannel wait, but the robust fix was to
use the Linux `usblp` character device through:

```text
parallel:/dev/usb/lp0
```

After that change:

- CUPS test output completed normally
- multiple pages printed consecutively
- repeated jobs did not require USB detach/reattach
- the printer remained usable between jobs

## Windows client validation

The original mixed text/graphics document was then printed from Windows to the
shared CUPS queue. It printed correctly using the same Linux filter, physical
mapper, OkiGraph encoder, and `parallel:/dev/usb/lp0` transport.

This is the final v1.0 end-to-end validation path:

```text
Windows or Linux application
    -> shared/local CUPS queue
    -> pdftopdf / gstoraster
    -> rastertookigraph1
    -> okigraph1-raster
    -> okigraph1
    -> parallel:/dev/usb/lp0
    -> Linux usblp
    -> USB-to-parallel bridge
    -> MICROLINE 82A + OkiGraph I
```

## ML83A release status

The ML83A v1.0 PPD uses the same protocol encoder, physical mapper, and CUPS
filter. The model-specific rendering difference is the 792-column carriage
limit (13.2 inches at 60 columns/inch) plus its wider paper definition.

The detailed v1.0 CUPS end-to-end validation recorded here was performed on the
ML82A. The ML83A is released from the shared implementation and established
OkiGraph I geometry rather than from a separate duplicate renderer.
