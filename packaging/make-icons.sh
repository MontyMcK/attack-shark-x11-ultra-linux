#!/bin/sh
# Turn a logo image into the circular icon set the app installs.
#
#   ./packaging/make-icons.sh path/to/logo.png [background]
#
# The source is trimmed to its artwork first, so a logo sitting in a sea of
# whitespace still fills its circle instead of becoming a speck in the middle.
# Writes app/resources/icons/<size>/attackshark-ultra.png plus appicon.png,
# which is the copy compiled into the binary as the window icon.
set -e

src="$1"
bg="${2:-white}"
[ -n "$src" ] || { echo "usage: $0 <source image> [background]" >&2; exit 1; }
[ -f "$src" ] || { echo "no such file: $src" >&2; exit 1; }

root=$(cd "$(dirname "$0")/.." && pwd)
out="$root/app/resources/icons"
mkdir -p "$out"

canvas=1024
# Fraction of the circle's diameter the artwork spans. A wide mark has to stay
# under roughly 0.85 or its corners touch the rim.
scale=76

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# 1. Cut the surrounding whitespace away.
magick "$src" -fuzz 6% -trim +repage "$tmp/mark.png"
w=$(magick identify -format "%w" "$tmp/mark.png")
h=$(magick identify -format "%h" "$tmp/mark.png")

# 2. Scale the artwork to its share of the canvas, longest side wins so a wide
#    logo is not blown up past the circle.
target=$((canvas * scale / 100))
if [ "$w" -ge "$h" ]; then
    magick "$tmp/mark.png" -filter Lanczos -resize "${target}x" "$tmp/scaled.png"
else
    magick "$tmp/mark.png" -filter Lanczos -resize "x${target}" "$tmp/scaled.png"
fi

# 3. Centre it on a square of the background colour.
magick "$tmp/scaled.png" -background "$bg" -gravity center \
    -extent "${canvas}x${canvas}" "$tmp/square.png"

# 4. Circular mask, antialiased, transparent outside the circle.
magick -size "${canvas}x${canvas}" xc:none -fill white \
    -draw "circle $((canvas/2)),$((canvas/2)) $((canvas/2)),2" "$tmp/mask.png"
magick "$tmp/square.png" "$tmp/mask.png" \
    -alpha off -compose CopyOpacity -composite "$tmp/round.png"

for s in 16 24 32 48 64 128 256 512; do
    mkdir -p "$out/$s"
    magick "$tmp/round.png" -filter Lanczos -resize "${s}x${s}" \
        -strip "$out/$s/attackshark-ultra.png"
done

cp "$out/256/attackshark-ultra.png" "$root/app/resources/appicon.png"
echo "artwork ${w}x${h} -> 8 circular sizes in app/resources/icons/"
