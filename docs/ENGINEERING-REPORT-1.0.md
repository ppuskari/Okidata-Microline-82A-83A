# Engineering report — OkiGraph I CUPS driver 1.0

## 1. Purpose

This report documents the engineering path from Okidata MICROLINE 82A/83A
EPROM provenance and firmware disassembly to a working Linux CUPS driver for
printers equipped with OkiGraph I firmware.

The important result is not simply that a raster can be made to print. The
project recovered enough of the printer's native graphics semantics to explain
why generic compatibility drivers produced poor geometry, built a renderer
from the recovered motion model, measured that model on physical hardware, and
then wrapped the same validated renderer in a conventional CUPS filter.

Version 1.0 therefore has four distinct layers of evidence:

1. firmware provenance and disassembly;
2. native protocol and motion reconstruction;
3. direct physical calibration on a real ML82A;
4. end-to-end CUPS and Windows-client printing.

Raw EPROM images are not required by the runtime driver and are intentionally
not redistributed with this repository.

## 2. Project origin

The initial goal was to understand the ROM sets found in several MICROLINE
82A/83A machines and to reconstruct the functional differences between stock
firmware, OkiGraph I, and IBM Plug-n-Play variants.

One important provenance result was that installing OkiGraph I does not require
a different Q4 ROM on either the 82A or 83A. The OkiGraph transformation is in
the Q5/Q6 portion of the set. Keeping Q4 outside the stock-to-OkiGraph delta
made cross-family comparisons much cleaner and prevented ordinary base
firmware from being misidentified as part of the graphics option.

The IBM Plug-n-Play branch was useful as an additional comparison family.
It helped separate ordinary model/mechanism differences from parser and
graphics additions. The CUPS driver does not depend on IBM mode, but the
comparison work helped establish which behaviors really belonged to OkiGraph I.

The public `firmware-analysis/` directory records the provenance methodology:
canonical identities, checksums, part and revision metadata, conceptual
symbols, structural signatures, and explicit CODE/TABLE/DATA/FILL/UNRESOLVED
accounting. The driver documentation contains only the protocol and mechanical
facts needed to explain the derived implementation.

## 3. Why an ordinary compatibility driver was not enough

Before the native geometry was understood, generic Okidata/Epson-style
graphics paths were attractive because they already existed in Ghostscript,
Gutenprint, and older printer stacks.

They were the wrong abstraction for OkiGraph I.

The visible symptom was horizontal band separation: the carriage data could be
recognizable while the paper advance accumulated the wrong vertical geometry.
Treating OkiGraph I as a uniform 72-DPI vertical raster caused repeated gaps or
seams because the firmware's band advance is not seven times a simple pin
pitch.

That observation changed the engineering question from:

> Which existing Epson/Okidata mode is closest?

to:

> What does this firmware actually command the mechanism to do?

That second question is what made the driver possible.

## 4. Firmware findings that define the native stream

### 4.1 ETX graphics state

The firmware disassembly identified ETX (`$03`) as the graphics command
prefix/state entry.

The working command set used by v1.0 is:

| Sequence | Firmware-derived meaning |
| --- | --- |
| `$03` | enter graphics/data state |
| `$03 $02` | exit graphics state |
| `$03 $0A` | text/print line feed + carriage return |
| `$03 $0E` | graphics line feed + carriage return |
| `$03 $12` | text/print line feed without carriage return |
| `$03 $14` | graphics line feed without carriage return |

The renderer uses `$03 $0E` between native graphics bands and stays in the
graphics state until the page is complete.

### 4.2 Seven host-addressable graphics pins

The printer mechanism is nine-pin, but the ordinary OkiGraph I host graphics
byte does not expose nine independent raster dots. Firmware analysis maps
seven host data bits to the graphics output path (P25 and P10..P15 in the
disassembly nomenclature).

Host bits 0 through 6 correspond to the seven graphics positions from top to
bottom.

Bit 7 is deliberately set on transmitted graphics bytes:

```c
wire_byte = 0x80 | (dot_mask & 0x7f);
```

That keeps graphics data in `$80..$FF` and prevents a data byte from being
confused with ETX.

### 4.3 Active-low internal representation

The firmware stores the received host graphics byte internally in the form:

```text
(~host) & $7F
```

That is consistent with an active-low printhead drive path. The host-side API
therefore uses the intuitive convention:

```text
1 bit = print this dot
```

and leaves the electrical inversion inside the printer firmware.

### 4.4 Horizontal motion

The firmware carriage path advances two 120-step/inch carriage units for each
graphics column.

Therefore:

```text
2 / 120 inch = 1 / 60 inch
```

and the native horizontal raster is 60 graphics columns per inch.

This immediately explains the two carriage limits used by the v1.0 PPDs:

```text
ML82A: 480 / 60 = 8.0 inches
ML83A: 792 / 60 = 13.2 inches
```

### 4.5 Vertical graphics feed

The decisive firmware discovery was the graphics line-feed path: one native
graphics feed advances 15 line-feed motor steps.

The mechanism model is 144 feed steps per inch, so:

```text
15 / 144 inch = 0.1041666667 inch per graphics band
```

or:

```text
144 / 15 = 9.6 graphics bands/inch
```

This is not equivalent to pretending that every exposed graphics row is one
uniform 72-DPI step.

### 4.6 The crucial distinction: pins versus band origins

The current physical head model places adjacent pins at the normal 1/72-inch
mechanical pitch. OkiGraph I exposes seven of those positions.

But after the seven-pin band is printed, the firmware advances the *next band
origin* by 15/144 inch.

Expressed in 1/144-inch units, the target centers are:

```text
band 0:  0,  2,  4,  6,  8, 10, 12
band 1: 15, 17, 19, 21, 23, 25, 27
band 2: 30, 32, 34, 36, 38, 40, 42
...
```

The three-unit transition from 12 to 15 is exactly why a simple rectangular
vertical DPI model is wrong.

The 15/144-inch band movement is directly firmware-derived and physically
measured. The 1/72-inch intra-band pin spacing remains the mapper's mechanical
head model; the successful real prints provide empirical support, but that
pin-to-pin distance was not established by the same 48-feed caliper test.

## 5. Building a direct protocol test before CUPS

The first software layer was intentionally small:

- `src/okigraph1.c`
- `src/okigraph1.h`
- `src/okigraph1-test.c`

The encoder has no CUPS dependency. It manages only native printer state:

```text
job begin
  -> optional CAN
  -> enter graphics with ETX
  -> write seven-bit columns as $80..$FF
  -> ETX $0E between bands
  -> ETX $02 to leave graphics
  -> optional FF
```

This separation was important. If the first tests had gone through
Ghostscript and CUPS, a bad result could have come from document rendering,
CUPS page geometry, a PPD, raster polarity, the transport backend, or the
printer protocol. The raw generator reduced the first hardware question to
one byte stream and one printer.

## 6. Physical calibration

### 6.1 The 48-feed experiment

The calibration generator prints reference marks in band 0 and band 48.
There are therefore exactly 48 native graphics feed commands between the two
matching band origins.

The firmware model predicted:

```text
48 * 15/144 inch = 5.000 inches
```

On the real ML82A, the measured separation was **5.000 inches with calipers**.

That was the turning point from a plausible disassembly interpretation to a
physically validated motion model.

### 6.2 Horizontal calibration

The ML82A test generator uses 480 columns. The printed span was approximately
8 inches, consistent with:

```text
480 columns / 60 columns per inch = 8 inches
```

The ML83A width used by the same code is 792 columns, or 13.2 inches at the
same native pitch.

### 6.3 Seam and registration patterns

Additional patterns concentrated ink around successive band boundaries and
included vertical references.

The resulting prints showed:

- regular feed;
- no cumulative left/right staircase;
- no skipped or doubled graphics feed;
- consistent pin ordering;
- no systematic inter-band white gaps when using the recovered feed cadence.

This gave enough confidence to build a conventional raster mapper on top of
the native encoder.

## 7. The physical raster mapper

The mapper is implemented in:

- `src/okigraph1-raster.h`
- `src/okigraph1-raster.c`

Its input is a conventional rectangular bitmap with an explicit source DPI.
Its output is not another rectangular bitmap. It directly emits native
seven-pin bands through the encoder.

### 7.1 Exact integer coordinate system

The mapper uses 1/288 inch as the vertical unit:

```text
1/72 inch   = 4/288
15/144 inch = 30/288
```

A target dot row is identified by a native band and a pin:

```text
band = dot_row / 7
pin  = dot_row % 7
```

and its physical center is:

```text
center_y = band * 30 + pin * 4
```

in 1/288-inch units.

Using integers avoids floating-point accumulation and makes every target center
repeatable regardless of page height.

### 7.2 Horizontal target cells

Native X centers occur every 1/60 inch.

For each output column, the mapper computes the physical midpoint to neighboring
native centers, then finds the source pixels whose centers fall within that
cell.

The requested number of native columns is:

```text
ceil(source_width * 60 / source_dpi_x)
```

and is clipped to the model carriage limit from the PPD.

### 7.3 Vertical target cells

The same midpoint rule is applied to the nonuniform Y centers.

This is the key architectural decision: source pixels are assigned according
to physical space, not according to an assumed logical row number.

### 7.4 Coverage threshold

For each native target dot, the mapper counts black source pixels in that
physical cell.

The default decision is:

```text
black_pixels * 100 >= total_pixels * 50
```

so a cell prints when at least 50 percent of its source coverage is black.

The threshold is configurable. Lower values preserve finer dark features;
higher values suppress isolated source pixels.

### 7.5 Band assembly

For one native band the mapper evaluates seven target Y positions for every
output X position, builds a seven-bit mask, and stores one byte per native
carriage column.

It then sends the whole band through `okg1_write_columns()` and emits
`$03 $0E` before the next band.

The CUPS wrapper and the standalone PBM converter both call this same routine.
There is only one implementation of the physical geometry.

## 8. Deterministic 360-DPI source test

`src/okigraph1-mktest.c` generates a fixed 2880 x 2160 PBM at 360 DPI:

```text
8 inches x 6 inches
```

The pattern includes:

- an outer frame;
- one-inch horizontal and vertical references;
- diagonals;
- two intentional density/coverage blocks.

On the ML82A it maps to:

```text
480 native columns
404 target dot rows
58 native seven-pin bands
```

The real print preserved the intended physical geometry and showed no recurring
band seams. That validated the generic source-raster mapper before CUPS was
introduced.

## 9. CUPS filter architecture

The CUPS-specific layer is `src/rastertookigraph1.c`.

Its design rule is simple:

> CUPS owns document conversion; the OkiGraph filter owns only raster
> normalization, physical mapping, and native printer output.

### 9.1 Input contract

The filter follows the classic CUPS filter invocation:

```text
rastertookigraph1 job-id user title copies options [file]
```

It reads CUPS raster with:

- `cupsRasterOpen`
- `cupsRasterReadHeader2`
- `cupsRasterReadPixels`

### 9.2 PPD-derived configuration

The PPDs provide two custom attributes:

```text
OkiGraphMaxColumns
OkiGraphThreshold
```

For v1.0:

```text
ML82A max columns = 480
ML83A max columns = 792
threshold         = 50
```

The filter therefore has no duplicated per-model geometry beyond selecting the
carriage width.

### 9.3 Raster polarity normalization

The PPD requests one-bit K output, but the filter accepts diagnostic variants:

- 1-bit K;
- 1-bit W/SW;
- 8-bit K;
- 8-bit W/SW.

CUPS K and luminance spaces have opposite black/white polarity. The filter
normalizes all accepted inputs into one internal format:

```text
packed MSB-first bitmap, 1 = black
```

Only after that normalization does it call the physical mapper.

### 9.4 Page buffering

Each CUPS page is buffered into the packed bitmap before physical resampling.

At the v1.0 360-DPI monochrome settings this is modest in size and greatly
simplifies the mapping algorithm because any target physical cell can inspect
the source pixels it owns.

### 9.5 Multi-page state

The first page begins the native job with CAN. Each page then:

1. enters graphics mode;
2. renders native bands;
3. exits graphics mode;
4. emits FF.

The next page re-enters graphics without another CAN.

This was validated with consecutive multi-page output on the physical ML82A.

## 10. PPD design

The v1.0 PPDs request:

```text
360 x 360 DPI source raster
monochrome K
1 bit per color
chunked order
```

The ML82A PPD exposes Letter and Legal with an 8-inch imageable width.

The ML83A PPD adds the wide 14 x 11 fanfold definition and sets its native
carriage limit to 792 columns.

Both PPDs pass `cupstestppd` on the CUPS 2.3.3op2 validation system.

The PPD DPI is the *source raster resolution*. It does not claim that the
physical printer mechanism is a 360-DPI device. The custom mapper preserves
physical size while transforming that convenient high-resolution source into
the native 60-column/nonuniform-seven-pin geometry.

## 11. What CUPS actually does before the custom filter

A debug trace on CUPS 2.3.3op2 showed a PDF job using:

```text
application/pdf
    -> pdftopdf
    -> application/vnd.cups-pdf
    -> gstoraster
    -> application/vnd.cups-raster
    -> rastertookigraph1
```

This is useful because it defines the boundary of responsibility.

PDF layout, fonts, images, PostScript-level drawing, and Windows-originated
documents are rasterized upstream. The OkiGraph filter sees only the final
page bitmap and its physical source resolution.

That separation is also why a large JPEG embedded in a PDF, a CUPS test page,
and a Windows mixed text/graphics document can all use the same driver without
application-specific code.

## 12. The USB-to-parallel transport problem

The final non-rendering defect was not in the renderer.

CUPS discovered the generic USB-to-parallel adapter as:

```text
usb://Unknown/Printer
```

Using that URI selects CUPS' libusb backend. The bridge advertised printer
protocol 2, so CUPS treated it as bidirectional and opened a backchannel read
thread.

The custom filter completed rendering and exited successfully, yet the backend
could remain in a state where jobs were held until the USB device was detached
from and reattached to the VirtualBox guest.

Forcing:

```text
usb-unidir=yes
```

removed the backchannel dependency and demonstrated what was wrong, but it did
not fully solve the VirtualBox/libusb ownership problem between jobs.

The stable solution was to stop having CUPS claim the interface through libusb
and instead use the Linux kernel `usblp` device:

```text
parallel:/dev/usb/lp0
```

The resulting path is:

```text
CUPS parallel backend
    -> /dev/usb/lp0
    -> Linux usblp
    -> VirtualBox USB passthrough
    -> USB-to-Centronics adapter
    -> printer
```

After this change, multiple pages and repeated jobs completed without
detach/reattach cycles.

The lesson is transport-specific rather than OkiGraph-specific: a generic USB
bridge may advertise more bidirectional capability than is useful when feeding
a vintage Centronics printer through virtualization.

## 13. End-to-end validation matrix

### Direct native stream

**PASS**

The raw test generator printed native OkiGraph bands directly to
`/dev/usb/lp0`.

### Vertical feed metrology

**PASS**

48 native graphics feeds measured exactly 5.000 inches.

### Horizontal scale

**PASS**

480 ML82A columns produced the expected approximately 8-inch graphics span.

### Deterministic 360-DPI mapper pattern

**PASS**

Physical dimensions, diagonals, grid, density blocks, and band continuity were
correct.

### Large image through CUPS

**PASS**

A PDF containing a very large JPEG filled the expected page area at the
correct aspect ratio with no missing raster lines or recurring seams.

### CUPS built-in test material

**PASS after transport correction**

The rendering/filter path completed correctly. The remaining hold behavior
was isolated to the `usb://` backend and eliminated by using
`parallel:/dev/usb/lp0`.

### Multiple pages / repeated jobs

**PASS**

Consecutive pages printed normally with the parallel backend.

### Windows client through shared CUPS queue

**PASS**

The original mixed text/graphics test document printed correctly from Windows
through the shared CUPS queue.

## 14. ML82A and ML83A relationship in v1.0

Version 1.0 intentionally shares one implementation between both printers.

The common pieces are:

- OkiGraph protocol encoder;
- seven-bit pin mapping;
- graphics feed semantics;
- physical raster mapper;
- CUPS raster normalization;
- page state handling;
- threshold algorithm.

Model selection changes:

- maximum native carriage columns;
- PPD imageable area/page definitions.

The detailed CUPS validation in this report was executed on the ML82A. The
ML83A is promoted in v1.0 because it uses the same recovered OkiGraph protocol
and renderer with the already-defined 792-column carriage geometry, not because
a second independent filter was written.

## 15. Source layout

```text
src/okigraph1.c
src/okigraph1.h
    Native OkiGraph byte-stream state and commands.

src/okigraph1-raster.c
src/okigraph1-raster.h
    Physical source-raster -> nonuniform native band mapper.

src/rastertookigraph1.c
    CUPS raster input, normalization, page handling, and logging.

src/okigraph1-test.c
    Native hardware calibration generator.

src/okigraph1-pbm.c
    Standalone PBM/native and physical-DPI conversion front end.

src/okigraph1-mktest.c
    Deterministic 360-DPI source-image generator.

tests/test-raster.c
    Geometry and clipping regression tests.

ppd/
    ML82A and ML83A CUPS PPDs.

scripts/install-cups.sh
    Source/prebuilt installation.

scripts/configure-cups-queue.sh
    Queue creation with the validated parallel backend default.

docs/
    Protocol, mapper, hardware, CUPS, and engineering documentation.

firmware-analysis/
    Public provenance/disassembly methodology; raw ROMs excluded.
```

## 16. Release packaging

The v1.0.0 release publishes normal GitHub source archives plus a prebuilt
Debian 11/Bullseye amd64 bundle.

The prebuilt bundle is built in a Debian Bullseye environment, runs the raster
regression tests, builds the CUPS filter, validates both PPDs, and records build
metadata and SHA-256 checksums.

The bundle contains the prebuilt filter in `build/rastertookigraph1`, so the
normal installer can deploy it without requiring a compiler or CUPS development
headers.

Other architectures and Linux distributions should build from source.

## 17. Deliberate limits of version 1.0

Version 1.0 does not attempt to:

- expose nine independent host graphics pins;
- emulate IBM Graphics Printer or Epson modes;
- redistribute copyrighted ROM images;
- replace modern driverless IPP infrastructure;
- infer mechanical paper alignment that remains under the printer/operator's
  control;
- claim that the physical mechanism is a uniform 360-, 72-, or 66-DPI raster.

The driver instead preserves the recovered OkiGraph I semantics and lets CUPS
supply a convenient high-resolution source bitmap.

## 18. Engineering conclusion

The successful driver came from treating the firmware as the primary hardware
specification.

The critical chain was:

```text
EPROM provenance
    -> isolate OkiGraph-specific firmware
    -> recover ETX graphics state and seven-bit data semantics
    -> recover 60-column carriage motion
    -> recover 15/144-inch graphics feed
    -> generate native calibration streams
    -> measure 48 feeds = 5.000 inches
    -> model the nonuniform physical raster
    -> validate a 360-DPI source mapper
    -> wrap that mapper in CUPS
    -> isolate transport/backend behavior separately
    -> validate local, multi-page, and Windows-client printing
```

That separation of firmware semantics, physical geometry, raster conversion,
CUPS integration, and transport is the central design principle of version 1.0.
