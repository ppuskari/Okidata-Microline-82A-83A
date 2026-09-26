#!/bin/sh
set -eu

if ! command -v cups-config >/dev/null 2>&1; then
    echo "ERROR: cups-config not found." >&2
    exit 1
fi

if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: run this uninstaller as root." >&2
    exit 1
fi

SERVERBIN=$(cups-config --serverbin)
DATADIR=$(cups-config --datadir)
FILTER="$SERVERBIN/filter/rastertookigraph1"
MODELDIR="$DATADIR/model/okigraph1"

rm -f "$FILTER"
rm -f "$MODELDIR/okidata-ml82a-okigraph1.ppd"
rm -f "$MODELDIR/okidata-ml83a-okigraph1.ppd"
rmdir "$MODELDIR" 2>/dev/null || true

if command -v systemctl >/dev/null 2>&1; then
    systemctl try-restart cups.service >/dev/null 2>&1 || true
elif command -v service >/dev/null 2>&1; then
    service cups restart >/dev/null 2>&1 || true
fi

echo "Removed OkiGraph I CUPS filter and PPD files."
