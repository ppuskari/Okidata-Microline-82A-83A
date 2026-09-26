# Hardware validation

## 2026-09-26 — MICROLINE 82A + OkiGraph I

Physical test configuration:

- Okidata MICROLINE 82A
- Validated OkiGraph I firmware installed
- Linux host
- USB-to-parallel adapter exposed as `/dev/usb/lp0`
- Raw output written directly to the device
- Test generator: `build/okigraph1-test`

### Calibration result

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

The horizontal ruler also printed at approximately the expected 8-inch ML82A
graphics width using 480 native columns, supporting the **60 columns/inch**
horizontal model.

### Seam/registration result

The dedicated seam pattern showed very regular paper advance and carriage
return registration. The vertical reference line remains essentially straight
through the successive bands, with no visible cumulative staircase, skipped
feed, or doubled feed. The repeated horizontal pin-pair marks remain uniformly
spaced.

This is the expected quality level for a mechanically healthy dot-matrix
printer and is sufficient to use the native feed geometry as the basis for the
Linux raster mapper.

### Confirmed native geometry

```text
Horizontal graphics pitch: 60 columns/inch
Native band feed:           15/144 inch
Native bands/inch:          9.6
Graphics pins per band:     7
```

The seven physical dot rows within a band and the 15/144-inch inter-band motion
must be modeled explicitly. Generic Epson/Okidata drivers that assume a simple
72-dpi vertical raster are not an appropriate geometry model for OkiGraph I.

### Test-pattern correction

The original calibration generator drew ruler columns at 0, 60, 120, ... but
did not explicitly draw the last physical column. On the ML82A this produced a
three-sided reference frame: top, left, and bottom were present, while the
right edge at column 479 was absent.

The generator now explicitly draws `width - 1` as a seven-pin vertical
reference column so future calibration sheets have a closed frame.
