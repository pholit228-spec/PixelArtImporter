# PixelArt Importer — Geode development project

A cross-platform starter project for a Geometry Dash editor mod that converts raster images/video frames into coloured square objects.

## Current state (read this first)

This ZIP is a **development scaffold**, not a finished importer. It contains:

- a Geode C++ mod entry point and metadata;
- a platform matrix GitHub Actions workflow (Windows, macOS, Android 32/64-bit);
- a platform-independent pixel-processing core: nearest-neighbour resizing, transparent-pixel filtering, colour quantization, and per-frame pixel diffing.

The in-game file picker, image decoder integration, editor object creation/serialization, and video playback/trigger generation are not implemented yet. Consequently, this initial build only verifies the Geode project and conversion core; it does not yet add a working Import button or create level objects. The APIs for these pieces must be implemented and tested against the exact Geometry Dash/Geode version before calling the feature complete.

## Build with GitHub Actions

1. Push this project to a GitHub repository, keeping the `.github/workflows` directory.
2. Open **Actions** and enable workflows if GitHub asks.
3. Run **Build Geode mod** manually, or push a commit.
4. Open the completed workflow run and download its platform artifacts. The Geode build action produces `.geode` package output for each target.

The workflow uses the official `geode-sdk/build-geode-mod` action and currently builds Win64, MacOS, Android32 and Android64 as separate artifacts.

## Local build

Install the Geode SDK/CLI for your platform, then run:

```sh
geode build
```

## Planned implementation

1. Add a desktop/mobile file picker and decode PNG/JPEG/WebP (and video frame extraction).
2. Downsample before allocating game objects; enforce the `max-pixels` cap.
3. Convert pixels into editor block objects and colour channels, with batched insertion and undo support.
4. Video mode A: pre-create pixel objects and animate position/visibility with move/alpha triggers.
5. Video mode B: pre-create pixel objects and animate colour using colour triggers, emitting only changed pixels per frame.
6. Add duration/FPS controls, preview, cancel/progress UI, and memory limits suitable for phones.

## Performance notes

A full-colour image with one object per pixel can create thousands of objects. Start with a low resolution (for example 32×32), use palette quantization, skip transparent pixels, and cap frame count. Video frame-by-frame colour changes can generate a very large trigger graph, so frame differencing and a configurable FPS are essential.

## References

- Geode template collection: https://github.com/geode-sdk/template-mods
- Geode build action: https://github.com/geode-sdk/build-geode-mod
- Geode SDK: https://github.com/geode-sdk/geode
