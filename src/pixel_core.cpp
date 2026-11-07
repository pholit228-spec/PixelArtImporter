#include "pixel_core.hpp"
#include <algorithm>
#include <cmath>

namespace pixelart {
Image resizeNearest(const Image& src, int maxW, int maxH) {
    if (!src.valid() || maxW < 1 || maxH < 1) return {};
    const double scale = std::min({1.0, double(maxW) / src.width, double(maxH) / src.height});
    const int w = std::max(1, int(std::floor(src.width * scale)));
    const int h = std::max(1, int(std::floor(src.height * scale)));
    Image out{w, h, std::vector<RGBA>(size_t(w) * h)};
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        int sx = std::min(src.width - 1, int((double(x) + .5) * src.width / w));
        int sy = std::min(src.height - 1, int((double(y) + .5) * src.height / h));
        out.pixels[size_t(y) * w + x] = src.pixels[size_t(sy) * src.width + sx];
    }
    return out;
}
std::vector<PixelCell> toCells(const Image& img, bool skipTransparent) {
    std::vector<PixelCell> out;
    if (!img.valid()) return out;
    out.reserve(img.pixels.size());
    for (int y = 0; y < img.height; ++y) for (int x = 0; x < img.width; ++x) {
        const auto c = img.pixels[size_t(y) * img.width + x];
        if (skipTransparent && c.a == 0) continue;
        out.push_back({x, y, c});
    }
    return out;
}
FrameDelta diffFrames(const Image& a, const Image& b, bool skipTransparent) {
    FrameDelta out;
    if (!a.valid() || !b.valid() || a.width != b.width || a.height != b.height) return out;
    for (int y = 0; y < b.height; ++y) for (int x = 0; x < b.width; ++x) {
        const auto p = size_t(y) * b.width + x;
        const auto& n = b.pixels[p];
        const auto& o = a.pixels[p];
        if (skipTransparent && n.a == 0) continue;
        if (n.r != o.r || n.g != o.g || n.b != o.b || n.a != o.a) out.changed.push_back({x, y, n});
    }
    return out;
}
Image quantize(const Image& src, int levels) {
    if (!src.valid() || levels < 2) return {};
    Image out = src;
    const int step = 255 / (levels - 1);
    auto q = [step](uint8_t v) -> uint8_t {
        return static_cast<uint8_t>(std::clamp(int(std::round(double(v) / step) * step), 0, 255));
    };
    for (auto& p : out.pixels) { p.r = q(p.r); p.g = q(p.g); p.b = q(p.b); }
    return out;
}
}
