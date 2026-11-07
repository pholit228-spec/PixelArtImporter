# PixelArtImporter

A Geometry Dash Geode editor mod for importing images as coloured square blocks.

## Implemented in v0.2.0

- Adds an Import Image button to the level editor.
- Uses Geode's cross-platform native file picker.
- Decodes PNG, JPG/JPEG and WebP through Cocos2d's image decoder.
- Preserves aspect ratio and downsamples large images to a maximum of 2,500 source samples.
- Skips near-transparent pixels and creates colour-tinted flat coloured-square objects (object ID 207) in the open level.
- Uses Geode 5.11.2 or newer to avoid an SDK/bindings compatibility issue in the previous 5.0.0 build.

Imported blocks are placed starting at level coordinate (150, 150), with one 30x30 flat coloured square per sampled pixel. Import into a test level first; dense artwork can still make the editor slow.

## Video status

MP4, WebM and GIF frame import and trigger-based animation are not implemented in this version. The pixel-diff core is only a processing primitive; it does not create animation triggers. Video decoding requires a separate cross-platform decoder and the editor's trigger/object serialization needs to be implemented and tested. Do not treat video support as available yet.

## Build

The GitHub Actions workflow builds Win64, macOS, Android32 and Android64 artifacts. Open the Actions tab and download the artifact from a successful run.

Local build:
- Install Geode CLI.
- Run geode sdk install.
- Run geode sdk update 5.11.2.
- Run geode build.
