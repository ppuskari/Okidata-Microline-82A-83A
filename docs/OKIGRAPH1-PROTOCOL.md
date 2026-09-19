# OkiGraph I graphics notes used by the test generator

This document records the working protocol assumptions derived from the
MICROLINE 82A/83A OkiGraph I firmware disassembly project.

## Native graphics data

* OkiGraph I graphics exposes seven host-addressable pins per column.
* Host bits 0 through 6 map from the top graphics pin downward.
* Host bit 7 is not part of the seven-dot image.  The generator keeps it set
  (`$80..$FF`) so binary graphics data cannot be confused with the ETX command
  prefix.
* Native horizontal graphics pitch is 60 columns/inch.  Firmware carriage
  motion uses two 120-step/inch carriage steps per graphics column.

The firmware stores the received host graphics byte internally as
`(~host) & $7F`.  This is consistent with the active-low printhead drive path;
the host-side convention used by this project remains **1 bit = print dot**.

## Graphics-state commands

The graphics command prefix is ETX (`$03`).

| Sequence | Meaning |
|---|---|
| `$03` | Enter graphics/data state |
| `$03 $02` | Exit graphics |
| `$03 $0A` | Text/print line feed + carriage return |
| `$03 $0E` | Graphics line feed + carriage return |
| `$03 $12` | Text/print line feed without carriage return |
| `$03 $14` | Graphics line feed without carriage return |

The first raw test generator uses `$03 $0E` between bands and remains in
OkiGraph graphics state until `$03 $02` at the end of the page.

## Native band advance

The firmware's graphics-feed path advances 15 line-feed motor steps.  The
current firmware/mechanical model is 144 line-feed steps/inch, therefore one
native graphics feed is:

    15 / 144 inch = 0.1041666667 inch

Forty-eight native graphics feeds are therefore exactly 5 inches.  The test
program deliberately prints band 0 and band 48 as physical reference bands so
that this can be measured on the real printer.

## Why seven pins first

The ML82A/83A print head is physically nine-pin, but OkiGraph I host graphics
uses only seven host data bits.  Firmware analysis maps those seven data bits
to P25 and P10..P15.  The remaining two physical head outputs are not exposed
by the normal OkiGraph I graphics-byte path.  A future firmware extension can
investigate nine-pin raster output separately.
