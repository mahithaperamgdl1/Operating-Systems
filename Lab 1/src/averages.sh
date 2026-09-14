#!/bin/bash

set -u

IMAGE_DIR=../images
OUT_DIR=$(mktemp -d)

trap 'rm -rf "$OUT_DIR"' EXIT

if command -v make > /dev/null 2>&1
then
	make build-sharpen || exit 1
else
	g++ -g image_sharpener.cpp libppm.cpp -o a.out || exit 1
fi

printf "\n%-6s %-14s %10s %11s %11s %11s %11s %11s %12s\n" \
       "Image" "Dimensions" "Pixels" "Read" "S1" "S2" "S3" "Write" "Total"
printf -- "-------------------------------------------------------------------------------------------------------\n"

for img in "$IMAGE_DIR"/*.ppm
do
	name=$(basename "$img" .ppm)

	case "$name" in *_app_output) continue ;; esac

	./a.out "$img" "$OUT_DIR/out.ppm" | awk -v name="$name" '
		$1 == "Image:" { w = $2; h = $4 }
		$1 == "Avg" {
			printf "%-6s %-14s %10d %11s %11s %11s %11s %11s %12s\n",
			       name, w " x " h, w*h, $2, $3, $4, $5, $6, $7
		}'
done

printf -- "-------------------------------------------------------------------------------------------------------\n"
printf "Each value is the average of 5 runs, in microseconds.\n\n"
