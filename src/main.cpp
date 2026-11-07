#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/async.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
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

    int created = 0;
    const cocos2d::CCPoint origin{150.f, 150.f};
    for (int y = 0; y < outHeight; ++y) {
        const int sy = std::min(height - 1, static_cast<int>((y + .5) * height / outHeight));
        for (int x = 0; x < outWidth; ++x) {
            const int sx = std::min(width - 1, static_cast<int>((x + .5) * width / outWidth));
            const auto index = (static_cast<size_t>(sy) * width + sx) * bytesPerPixel;
            const uint8_t r = pixels[index];
            const uint8_t g = pixels[index + 1];
            const uint8_t b = pixels[index + 2];
            const uint8_t a = channels == 4 ? pixels[index + 3] : 255;
            if (a <= kAlphaThreshold) continue;

            // LevelEditorLayer owns object creation and inserts it into the editor's
            // object collections. GameObject::createObject / LevelEditorLayer::addObject
            // are not part of the Geometry Dash 2.2081 bindings.
            auto object = editor->createObject(
                kPixelObjectId,
                {origin.x + x * kPixelWorldSize, origin.y + (outHeight - 1 - y) * kPixelWorldSize},
                false
            );
            if (!object) continue;
            const cocos2d::ccColor3B pixelColor{r, g, b};
            object->setObjectColor(pixelColor);
            // Apply the tint to the main color node too. The default block (ID 1)
            // is not the colorable flat square used for pixel art.
            if (auto mainColor = object->getMainColor()) {
                mainColor->setColor(pixelColor);
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
