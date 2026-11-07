#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace pixelart {
struct RGBA {
    uint8_t r = 0, g = 0, b = 0, a = 255;
};
struct Image {
    int width = 0;
    int height = 0;
    std::vector<RGBA> pixels;
    bool valid() const { return width > 0 && height > 0 && pixels.size() == static_cast<size_t>(width) * height; }
};
struct PixelCell {
    int x = 0, y = 0;
    RGBA color;
};
struct FrameDelta {
    std::vector<PixelCell> changed;
};

// Nearest-neighbour downsampling. The returned image never exceeds maxWidth/maxHeight.
Image resizeNearest(const Image& source, int maxWidth, int maxHeight);
// Convert an image into non-transparent pixel cells. Coordinates use a top-left origin.
std::vector<PixelCell> toCells(const Image& image, bool skipTransparent = true);
// Compare frames; only changed pixels are returned for efficient recolour animation.
FrameDelta diffFrames(const Image& previous, const Image& current, bool skipTransparent = true);
// Quantize RGB to a limited palette to reduce unique colour count and level complexity.
Image quantize(const Image& source, int levelsPerChannel);
}
