#!/bin/sh
set -eu

usage()
{
    echo "Usage: sudo sh scripts/configure-cups-queue.sh 82a|83a [queue-name] [device-uri]" >&2
    exit 2
}

[ "$#" -ge 1 ] && [ "$#" -le 3 ] || usage

if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: run this script as root." >&2
    exit 1
fi

MODEL=$1

case "$MODEL" in
    82a|82A)
        DEFAULT_QUEUE=ML82A-OkiGraphI
        PPD_FILE=okidata-ml82a-okigraph1.ppd
        ;;
    83a|83A)
        DEFAULT_QUEUE=ML83A-OkiGraphI
        PPD_FILE=okidata-ml83a-okigraph1.ppd
        ;;
    *)
        usage
        ;;
esac

QUEUE=${2:-$DEFAULT_QUEUE}
DEVICE_URI=${3:-parallel:/dev/usb/lp0}

if [ "$#" -lt 3 ] && [ ! -e /dev/usb/lp0 ]; then
    echo "ERROR: /dev/usb/lp0 does not exist." >&2
    echo "Attach the USB-to-parallel adapter or supply an explicit device URI." >&2
    exit 1
fi

if command -v cups-config >/dev/null 2>&1; then
    DATADIR=$(cups-config --datadir)
else
    DATADIR=${CUPS_DATADIR:-/usr/share/cups}
fi

PPD="$DATADIR/model/okigraph1/$PPD_FILE"

if [ ! -f "$PPD" ]; then
    echo "ERROR: installed PPD not found: $PPD" >&2
    echo "Run scripts/install-cups.sh first." >&2
    exit 1
fi

if command -v lpadmin >/dev/null 2>&1; then
    LPADMIN=$(command -v lpadmin)
elif [ -x /usr/sbin/lpadmin ]; then
    LPADMIN=/usr/sbin/lpadmin
else
    echo "ERROR: lpadmin not found." >&2
    exit 1
fi

"$LPADMIN" \
    -p "$QUEUE" \
    -E \
    -v "$DEVICE_URI" \
    -P "$PPD"

if command -v cupsaccept >/dev/null 2>&1; then
    cupsaccept "$QUEUE" || true
fi
if command -v cupsenable >/dev/null 2>&1; then
    cupsenable "$QUEUE" || true
fi

echo
echo "Configured queue:"
echo "  name:   $QUEUE"
echo "  model:  $MODEL"
echo "  device: $DEVICE_URI"
echo
echo "The validated generic USB-to-parallel configuration uses:"
echo "  parallel:/dev/usb/lp0"
echo
echo "Verify with:"
echo "  lpstat -t"
