#!/bin/bash

set -u

IMAGE_DIR=../images
OUT_DIR=$(mktemp -d)
RESULTS=results.csv
ALLRUNS=all_runs.csv

trap 'rm -rf "$OUT_DIR"' EXIT

if command -v make > /dev/null 2>&1
then
	make build-sharpen || exit 1
else
	g++ -g image_sharpener.cpp libppm.cpp -o a.out || exit 1
fi

echo "image,run,width,height,pixels,read_us,s1_us,s2_us,s3_us,write_us" > "$ALLRUNS"
echo "image,width,height,pixels,read_us,s1_us,s2_us,s3_us,write_us" > "$RESULTS"

for img in "$IMAGE_DIR"/*.ppm
do
	name=$(basename "$img" .ppm)

	case "$name" in *_app_output) continue ;; esac

	out="$OUT_DIR/${name}_app_output.ppm"

	./a.out "$img" "$out" > "$OUT_DIR/output.txt" || exit 1

	cat "$OUT_DIR/output.txt"

	awk -v name="$name" '
		$1 == "Image:" { w = $2; h = $4 }
		$1 ~ /^[1-5]$/ { printf "%s,%s,%d,%d,%d,%s,%s,%s,%s,%s\n",
		                 name, $1, w, h, w*h, $2, $3, $4, $5, $6 }
	' "$OUT_DIR/output.txt" >> "$ALLRUNS"

	awk -v name="$name" '
		$1 == "Image:" { w = $2; h = $4 }
		$1 == "Avg"    { printf "%s,%d,%d,%d,%s,%s,%s,%s,%s\n",
		                 name, w, h, w*h, $2, $3, $4, $5, $6 }
	' "$OUT_DIR/output.txt" >> "$RESULTS"
done

echo
echo "Per-run readings written to $ALLRUNS"
echo "Averages written to $RESULTS"
