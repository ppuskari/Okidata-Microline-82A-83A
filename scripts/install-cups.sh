#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

if ! command -v cups-config >/dev/null 2>&1; then
    echo "ERROR: cups-config not found." >&2
    echo "Install the CUPS development package first (Debian: libcups2-dev)." >&2
    exit 1
fi

if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: run this installer as root, for example:" >&2
    echo "  sudo sh scripts/install-cups.sh" >&2
    exit 1
fi

make cups

SERVERBIN=$(cups-config --serverbin)
DATADIR=$(cups-config --datadir)
FILTERDIR="$SERVERBIN/filter"
MODELDIR="$DATADIR/model/okigraph1"

install -d -m 755 "$FILTERDIR"
install -d -m 755 "$MODELDIR"

install -m 755 build/rastertookigraph1 \
    "$FILTERDIR/rastertookigraph1"

install -m 644 ppd/okidata-ml82a-okigraph1.ppd \
    "$MODELDIR/okidata-ml82a-okigraph1.ppd"
install -m 644 ppd/okidata-ml83a-okigraph1.ppd \
    "$MODELDIR/okidata-ml83a-okigraph1.ppd"

if command -v cupstestppd >/dev/null 2>&1; then
    cupstestppd ppd/okidata-ml82a-okigraph1.ppd
    cupstestppd ppd/okidata-ml83a-okigraph1.ppd
fi

if command -v systemctl >/dev/null 2>&1; then
    systemctl try-restart cups.service >/dev/null 2>&1 || true
elif command -v service >/dev/null 2>&1; then
    service cups restart >/dev/null 2>&1 || true
fi

echo
echo "Installed CUPS filter:"
echo "  $FILTERDIR/rastertookigraph1"
echo
echo "Installed PPDs:"
echo "  $MODELDIR/okidata-ml82a-okigraph1.ppd"
echo "  $MODELDIR/okidata-ml83a-okigraph1.ppd"
echo
echo "Next:"
echo "  lpinfo -v"
echo "  lpinfo -m | grep -i 'OkiGraph'"
echo
echo "Then create the queue using the device URI reported by lpinfo -v."
