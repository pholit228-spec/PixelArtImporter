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
constexpr int kBaseMaxPixels = 2500;
// NOTE: unverified that 207 has a colorable main/base sprite. The diagnostics
// below log base/detail/main presence for the first objects to settle this.
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

    // User-configurable output quality: each step doubles the pixel budget.
    // Keep the cap conservative for mobile editors (up to 20,000 objects).
    const int quality = std::clamp(
        static_cast<int>(Mod::get()->getSettingValue<int64_t>("quality")), 1, 4
    );
    const int maxPixels = kBaseMaxPixels * (1 << (quality - 1));
    const double downsample = std::max(1.0, std::sqrt(
        (static_cast<double>(width) * height) / static_cast<double>(maxPixels)
    ));
    const int outWidth = std::max(1, static_cast<int>(std::floor(width / downsample)));
    const int outHeight = std::max(1, static_cast<int>(std::floor(height / downsample)));

    // Geometry Dash's square object is 30 world units at scale 1.0.
    // Smaller scale also reduces the spacing, so the pixel grid stays seamless.
    const float pixelScale = std::clamp(
        static_cast<float>(Mod::get()->getSettingValue<double>("pixel-scale")), 0.10f, 0.50f
    );
    const float pixelStep = kPixelWorldSize * pixelScale;

    auto manager = editor->m_effectManager;
    if (!manager) {
        image->release();
        FLAlertLayer::create("PixelArt Importer", "Could not access the level color manager.", "OK")->show();
        return;
    }

    // The editor may read channel defaults from the level settings' own effect
    // manager instead of the layer's one. Write every channel color to both so
    // whichever one is used has the color.
    GJEffectManager* settingsManager = nullptr;
    if (editor->m_levelSettings) settingsManager = editor->m_levelSettings->m_effectManager;
    const bool separateSettingsManager = settingsManager && settingsManager != manager;
    if (!separateSettingsManager) settingsManager = nullptr;

    using RGB = std::tuple<uint8_t, uint8_t, uint8_t>;
    std::map<RGB, int> palette;
    int created = 0;
    int nextChannelID = editor->getNextColorChannel();
    const int firstChannelID = nextChannelID;
    const cocos2d::CCPoint origin{150.f, 150.f};

    log::info("PixelArt Importer: start, firstFreeChannel={}, separateSettingsManager={}, out={}x{}",
        firstChannelID, separateSettingsManager, outWidth, outHeight);

    auto registerChannel = [](GJEffectManager* mgr, int channelID, cocos2d::ccColor3B color) -> bool {
        auto action = ColorAction::create();
        if (!action) return false;
        action->m_colorID = channelID;
        action->m_color = color;
        action->m_fromColor = color;
        action->m_toColor = color;
        action->m_playerColor = 0;
        mgr->setColorAction(action, channelID);
        return true;
    };

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
                    if (!registerChannel(manager, channelID, color)) continue;
                    if (settingsManager) registerChannel(settingsManager, channelID, color);
                    ++nextChannelID;
                    it = palette.emplace(key, channelID).first;
                } else {
                    const int fallbackChannel = nearestChannel(r, g, b);
                    if (fallbackChannel == 0) continue;
                    it = palette.emplace(key, fallbackChannel).first;
                }
            }

            auto object = editor->createObject(
                kPixelObjectId,
                {origin.x + x * pixelStep, origin.y + (outHeight - 1 - y) * pixelStep},
                false
            );
            if (!object) continue;
            object->setScale(pixelScale);

            const int channel = it->second;
            const cocos2d::ccColor3B pixelColor{r, g, b};

            // Point every color component the object has at the channel directly,
            // so it does not matter whether 207 uses its base or its detail color.
            object->setMainColorMode(channel);
            if (object->m_baseColor) {
                object->m_baseColor->m_colorID = channel;
                object->m_baseColor->m_defaultColorID = channel;
            }
            if (object->m_detailColor) {
                object->m_detailColor->m_colorID = channel;
                object->m_detailColor->m_defaultColorID = channel;
            }

            // Immediate visual tint (not saved; the channel is what persists).
            object->setObjectColor(pixelColor);
            if (object->getMainColor()) {
                object->updateMainColor(pixelColor);
            }

            if (created < 5) {
                log::info("PixelArt Importer: obj id={} channel={} activeMain={} base={} detail={} main={}",
                    object->m_objectID, channel, object->m_activeMainColorID,
                    object->m_baseColor != nullptr, object->m_detailColor != nullptr,
                    object->getMainColor() != nullptr);
            }
            ++created;
        }
    }

    log::info("PixelArt Importer: done, created={}, channelsUsed={}, firstChannel={}",
        created, palette.size(), firstChannelID);

    image->release();
    Notification::create(
        fmt::format("Imported {} blocks ({}x{}, scale {}%, quality {}/4)", created, outWidth, outHeight,
            static_cast<int>(std::lround(pixelScale * 100.f)), quality),
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
