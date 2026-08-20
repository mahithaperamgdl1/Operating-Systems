# Image Sharpening Pipeline with Per-Stage Performance Profiling

A serial C++ implementation of an unsharp-masking image sharpener over PPM images, instrumented to
measure the cost of each stage of the pipeline, with a Bash harness that benchmarks it across a set
of images of increasing size.

Built for the Operating Systems lab at IIT Dharwad.

## What it does

The sharpening runs as a three-stage pipeline:

| Stage | Operation | Description |
|-------|-----------|-------------|
| **S1** | Smoothen | 3×3 neighbourhood mean filter over each channel, with clamped edges so border pixels average only over valid neighbours |
| **S2** | Find details | Per-channel difference `input − smoothened`, clamped to `[0, 255]` |
| **S3** | Sharpen | Per-channel sum `input + details`, clamped to `[0, 255]` |

Images are held as a dynamically allocated 3-D buffer (`height × width × 3` of `uint8_t`), built and
torn down explicitly by `create()` and `destroy()`.

## Performance measurement

`image_sharpener.cpp` wraps every stage — read, S1, S2, S3, write — in `std::chrono::steady_clock`
timers, repeats the whole pipeline over several runs, and reports both per-run and averaged
microsecond timings:

```
Image: 1920 x 1080   (5 runs, all times in microseconds)

  Run       Read         S1         S2         S3      Write       Total
--------------------------------------------------------------------------
    1       ...        ...        ...        ...        ...         ...
  Avg       ...        ...        ...        ...        ...         ...
```

`averages.sh` drives this across every `.ppm` in the image directory, parses the averaged row out of
each run with `awk`, and prints one table of per-stage cost against pixel count — so the scaling
behaviour of each stage is directly comparable.

## Build and run

```bash
cd src

make build-sharpen                                  # compile
make run-sharpen INPUT=<name> OUTPUT=<name>         # single image
make averages                                       # benchmark all images
make clean
```

`averages.sh` expects PPM images in `../images/` and falls back to invoking `g++` directly if `make`
is unavailable. Output is written to a temporary directory that is removed on exit via a `trap`.

## Layout

```
src/
  image_sharpener.cpp   pipeline stages, timing harness, reporting
  libppm.h / libppm.cpp PPM read/write helpers
  averages.sh           multi-image benchmark driver
  makefile              build and run targets
```

## Notes

Coursework, done as a two-person lab pair. The PPM read/write helpers (`libppm`) came with the lab
handout; the pipeline stages, memory management, timing instrumentation and benchmark harness are
our own.
