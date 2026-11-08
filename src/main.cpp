#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/async.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <map>
#include <limits>
#include <tuple>
#include <cctype>
using namespace geode::prelude;

namespace {
constexpr int kMaxPixels = 2500;
// Geometry Dash object 207 is the colorable flat square, unlike the default solid block (ID 1).
constexpr int kPixelObjectId = 207;
constexpr float kPixelWorldSize = 30.f;
constexpr uint8_t kAlphaThreshold = 32;

void importImage(std::filesystem::path const& path) {
    auto editor = LevelEditorLayer::get();
    if (!editor) {
        FLAlertLayer::create("PixelArt Importer", "Open a level in the editor before importing.", "OK")->show();
        return;
    }

    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    auto format = cocos2d::CCImage::kFmtPng;
    if (extension == ".jpg" || extension == ".jpeg") format = cocos2d::CCImage::kFmtJpg;
    else if (extension == ".webp") format = cocos2d::CCImage::kFmtWebp;
    else if (extension != ".png") {
        FLAlertLayer::create("PixelArt Importer", "Unsupported format. Use PNG, JPG or WebP.", "OK")->show();
        return;
    }

    auto image = new cocos2d::CCImage();
    if (!image->initWithImageFileThreadSafe(path.string().c_str(), format)) {
        image->release();
        FLAlertLayer::create("PixelArt Importer", "Could not decode this image. Try PNG, JPG or WebP.", "OK")->show();
        return;
    }

    const int width = image->getWidth();
    const int height = image->getHeight();
    const int channels = image->hasAlpha() ? 4 : 3;
    const int bytesPerChannel = std::max(1, image->getBitsPerComponent() / 8);
    const int bytesPerPixel = channels * bytesPerChannel;
    const auto* pixels = image->getData();

    if (!pixels || width <= 0 || height <= 0 || bytesPerChannel != 1) {
        image->release();
        FLAlertLayer::create("PixelArt Importer", "Unsupported image pixel format.", "OK")->show();
        return;
    }

    const double scale = std::max(1.0, std::sqrt(
        (static_cast<double>(width) * height) / static_cast<double>(kMaxPixels)
    ));
    const int outWidth = std::max(1, static_cast<int>(std::floor(width / scale)));
    const int outHeight = std::max(1, static_cast<int>(std::floor(height / scale)));

    auto manager = editor->m_effectManager;
    if (!manager) {
        image->release();
        FLAlertLayer::create("PixelArt Importer", "Could not access the level color manager.", "OK")->show();
        return;
    }

    // Use the full Geometry Dash custom-color range. Preserve exact RGB values
    // until the level runs out of free channels; any remaining colors are mapped
    // to the nearest allocated palette entry instead of aborting the import.
    using RGB = std::tuple<uint8_t, uint8_t, uint8_t>;
    std::map<RGB, int> palette;
    int created = 0;
    // getNextColorChannel() reports the first available channel. Calling it again
    // before adding a channel action can return that same ID, so reserve a local
    // monotonically increasing range for this import.
    int nextChannelID = editor->getNextColorChannel();
    const cocos2d::CCPoint origin{150.f, 150.f};

    auto nearestChannel = [&palette](uint8_t r, uint8_t g, uint8_t b) -> int {
        int bestChannel = 0;
        unsigned int bestDistance = std::numeric_limits<unsigned int>::max();
        for (auto const& [rgb, channel] : palette) {
            const auto [pr, pg, pb] = rgb;
            const int dr = static_cast<int>(r) - pr;
            const int dg = static_cast<int>(g) - pg;
            const int db = static_cast<int>(b) - pb;
            const unsigned int distance = static_cast<unsigned int>(dr * dr + dg * dg + db * db);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestChannel = channel;
            }
        }
        return bestChannel;
    };

    for (int y = 0; y < outHeight; ++y) {
        const int sy = std::min(height - 1, static_cast<int>((y + .5) * height / outHeight));
        for (int x = 0; x < outWidth; ++x) {
            const int sx = std::min(width - 1, static_cast<int>((x + .5) * width / outWidth));
            const auto index = (static_cast<size_t>(sy) * width + sx) * bytesPerPixel;
            const uint8_t a = channels == 4 ? pixels[index + 3] : 255;
            if (a <= kAlphaThreshold) continue;

            const uint8_t r = pixels[index];
            const uint8_t g = pixels[index + 1];
            const uint8_t b = pixels[index + 2];
            const RGB key{r, g, b};

            auto it = palette.find(key);
            if (it == palette.end()) {
                const int channelID = nextChannelID;
                if (channelID > 0 && channelID <= 999) {
                    const cocos2d::ccColor3B color{r, g, b};
                    auto action = ColorAction::create();
                    if (!action) continue;
                    action->m_colorID = channelID;
                    action->m_color = color;
                    action->m_fromColor = color;
                    action->m_toColor = color;
                    action->m_playerColor = 0;
                    manager->setColorAction(action, channelID);
                    ++nextChannelID;
                    it = palette.emplace(key, channelID).first;
                } else {
                    // The level has no more free custom channels. Reuse the closest
                    // already-created channel so the image still imports completely.
                    const int fallbackChannel = nearestChannel(r, g, b);
                    if (fallbackChannel == 0) continue;
                    it = palette.emplace(key, fallbackChannel).first;
                }
            }

            auto object = editor->createObject(
                kPixelObjectId,
                {origin.x + x * kPixelWorldSize, origin.y + (outHeight - 1 - y) * kPixelWorldSize},
                false
            );
            if (!object) continue;
            object->setMainColorMode(it->second);

            // IMPORTANT: updateMainColor(ccColor3B) is the direct-color overload.
            // The no-argument overload resolves m_activeMainColorID through the
            // effect manager and previously crashed on Android during early setup.
            // setObjectColor() alone does not refresh the colorable object sprite.
            const cocos2d::ccColor3B pixelColor{r, g, b};
            if (object->getMainColor()) {
                object->updateMainColor(pixelColor);
            } else {
                log::warn("PixelArt Importer: object {} has no main color component", kPixelObjectId);
            }
            ++created;
        }
    }

    image->release();
    Notification::create(
        fmt::format("Imported {} blocks ({}x{})", created, outWidth, outHeight),
        NotificationIcon::Success
    )->show();
}

void pickAndImportImage() {
    file::FilePickOptions options;
    options.filters.push_back({"Images", {"*.png", "*.jpg", "*.jpeg", "*.webp"}});
    async::spawn(
        file::pick(file::PickMode::OpenFile, std::move(options)),
        [](file::PickResult result) {
            if (result.isErr()) {
                log::error("Image picker failed");
                return;
            }
            auto selected = result.unwrap();
            if (!selected) return;
            importImage(std::filesystem::path(selected->string()));
        }
    );
}
}

class $modify(PixelArtEditorUI, EditorUI) {
    bool init(LevelEditorLayer* layer) {
        if (!EditorUI::init(layer)) return false;
        auto menu = CCMenu::create();
        if (!menu) return true;
        menu->setPosition({0.f, 0.f});
        menu->setID("pixelart-importer-menu"_spr);
        auto button = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Import Image", "goldFont.fnt", "GJ_button_01.png", .65f),
            this,
            menu_selector(PixelArtEditorUI::onPixelArtImport)
        );
        if (!button) return true;
        auto size = CCDirector::sharedDirector()->getWinSize();
        button->setPosition({size.width - 62.f, size.height - 42.f});
        menu->addChild(button);
        this->addChild(menu);
        return true;
    }
    void onPixelArtImport(CCObject*) {
        pickAndImportImage();
    }
};
