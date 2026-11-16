"""Run the production compositor with CPU-backed Vulkan blits (no GPU required).

Run from a compiler environment: python tests/libretro/test_scanout.py
Uses cl on Windows or $CXX / c++ elsewhere. Vulkan synchronization and driver
behavior still need on-device validation; these tests cover pixel ownership.
"""

import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "src/Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.cpp"

PRELUDE = r"""
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <vector>
#include "libretro/ScreenLayout.h"
using uint32 = uint32_t;
struct Pixels { int width, height; std::vector<int> data; };
std::map<VkImage, Pixels> images;
uintptr_t nextImage = 1;
VkImage allocate(int width, int height) {
    auto image = reinterpret_cast<VkImage>(nextImage++);
    images[image] = {width, height, std::vector<int>(width * height, -1)};
    return image;
}
void VKAPI_CALL vkCmdBlitImage(VkCommandBuffer, VkImage src, VkImageLayout,
    VkImage dst, VkImageLayout, uint32 count, const VkImageBlit* regions, VkFilter) {
    auto& from = images.at(src);
    auto& to = images.at(dst);
    for (uint32 i = 0; i < count; ++i) {
        const auto& r = regions[i];
        assert(r.srcSubresource.baseArrayLayer == 0);
        for (int y = r.dstOffsets[0].y; y < r.dstOffsets[1].y; ++y)
            for (int x = r.dstOffsets[0].x; x < r.dstOffsets[1].x; ++x) {
                int sx = (x - r.dstOffsets[0].x) * r.srcOffsets[1].x /
                    (r.dstOffsets[1].x - r.dstOffsets[0].x);
                int sy = (y - r.dstOffsets[0].y) * r.srcOffsets[1].y /
                    (r.dstOffsets[1].y - r.dstOffsets[0].y);
                to.data.at(y * to.width + x) = from.data.at(sy * from.width + sx);
            }
    }
}
void VKAPI_CALL vkCmdClearColorImage(VkCommandBuffer, VkImage image,
    VkImageLayout, const VkClearColorValue*, uint32, const VkImageSubresourceRange*) {
    auto& pixels = images.at(image).data;
    std::fill(pixels.begin(), pixels.end(), 0);
}
struct ImageObject {
    VkImage m_image;
    void flagForCurrentCommandBuffer() {}
};
struct LatteTextureVk {
    ImageObject object;
    VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    LatteTextureVk(int w = 16, int h = 9) : object{allocate(w, h)} {}
    ImageObject* GetImageObj() { return &object; }
    void GetEffectiveSize(int& w, int& h, int) {
        w = images.at(object.m_image).width; h = images.at(object.m_image).height;
    }
    VkImageLayout GetImageLayout(const VkImageSubresourceRange&) { return layout; }
    void paint(int color) {
        auto& pixels = images.at(object.m_image).data;
        std::fill(pixels.begin(), pixels.end(), color);
    }
};
struct LatteTextureViewVk {
    LatteTextureVk* texture;
    int firstSlice = 0;
    LatteTextureVk* GetBaseImage() { return texture; }
};
namespace Libretro {
    enum class ScreenViewMode { TV, Pad, SideBySide };
    ScreenViewMode mode = ScreenViewMode::SideBySide;
    bool stacked = false;
    bool IsDualScreenStacked() { return stacked; }
    ScreenViewMode GetScreenViewMode() { return mode; }
    struct Interface {
        void* handle = nullptr;
        uint32 (*get_sync_index)(void*) = nullptr;
        void (*wait_sync_index)(void*) = nullptr;
    } interfaceInstance;
    Interface* vulkan_iface = &interfaceInstance;
    VkImage presented{};
    void SetVulkanImage(VkImageView, VkImage image, VkFormat, uint32, uint32,
        VkImageLayout, VkSemaphore) { presented = image; }
}
struct VulkanRenderer {
    enum { TRANSFER_READ = 1, TRANSFER_WRITE = 2, IMAGE_READ = 4, IMAGE_WRITE = 8 };
    struct { VkCommandBuffer currentCommandBuffer = reinterpret_cast<VkCommandBuffer>(1); } m_state;
    struct LibretroOutputImage {
        VkImage image{}; VkImageView view{}; VkSemaphore semaphore{};
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        uint32 width = 0, height = 0;
    };
    static constexpr uint32 kLibretroOutputImages = 3;
    LibretroOutputImage m_libretroOutputs[kLibretroOutputImages];
    LibretroOutputImage m_libretroTVScanout, m_libretroPadScanout;
    int m_libretroPendingIndex = -1, waits = 0;
    void draw_endRenderPass() {}
    void Flush(bool wait) { assert(wait); ++waits; }
    bool EnsureLibretroImage(LibretroOutputImage& output, uint32 w, uint32 h) {
        if (output.image && output.width == w && output.height == h) return true;
        if (output.image) images.erase(output.image);
        output.image = allocate(w, h); output.width = w; output.height = h;
        output.layout = VK_IMAGE_LAYOUT_UNDEFINED;
        return true;
    }
    bool EnsureLibretroOutput(uint32 index, uint32 w, uint32 h) {
        return EnsureLibretroImage(m_libretroOutputs[index], w, h);
    }
    template<uint32, uint32>
    void barrier_image(VkImage, VkImageSubresourceRange&, VkImageLayout, VkImageLayout) {}
    template<uint32, uint32>
    void barrier_image(LatteTextureVk* texture, VkImageSubresourceLayers&, VkImageLayout layout) {
        texture->layout = layout;
    }
    bool BlitLibretroOutput(LatteTextureViewVk*, bool);
};
void expect(int left, int right) {
    const auto& p = images.at(Libretro::presented);
    for (int y = 0; y < p.height; ++y)
        for (int x = 0; x < p.width; ++x)
            assert(p.data.at(y * p.width + x) == (x < p.width / 2 ? left : right));
}
void scan(VulkanRenderer& renderer, LatteTextureVk& texture, bool pad, int color) {
    texture.paint(color);
    LatteTextureViewVk view{&texture};
    assert(renderer.BlitLibretroOutput(&view, pad));
    assert(texture.layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}
"""

TESTS = r"""
int main() {
    // Shared source, TV first. Repainting it must not change the saved TV pixels.
    VulkanRenderer a; LatteTextureVk reused;
    scan(a, reused, false, 11); expect(11, 0);
    scan(a, reused, true, 22); expect(11, 22);
    scan(a, reused, false, 33); expect(33, 22);
    // The opposite submission order and repeated GamePad updates.
    VulkanRenderer b;
    scan(b, reused, true, 44); expect(0, 44);
    scan(b, reused, false, 55); expect(55, 44);
    scan(b, reused, true, 66); expect(55, 66);
    // Capture while TV-only; switch before DRC submits another frame.
    Libretro::mode = Libretro::ScreenViewMode::TV;
    scan(a, reused, true, 77);
    Libretro::mode = Libretro::ScreenViewMode::Pad;
    scan(a, reused, false, 88); expect(77, 77);
    Libretro::mode = Libretro::ScreenViewMode::SideBySide;
    scan(a, reused, false, 99); expect(99, 77);
    // Destroying the game's texture must not discard captured pixels.
    images.erase(reused.object.m_image);
    LatteTextureVk differentSize(8, 4);
    scan(a, differentSize, true, 111); expect(99, 111);
    assert(a.waits == 1);
    // Resizing TV safely replaces its snapshot and adjusts composition size.
    LatteTextureVk larger(32, 18);
    scan(a, larger, false, 222); expect(222, 111);
    assert(a.waits == 2);
    assert(images.at(Libretro::presented).width == 64);
    // An absent GamePad must be blank, never a fallback copy of the TV.
    VulkanRenderer c;
    Libretro::mode = Libretro::ScreenViewMode::Pad;
    scan(c, larger, false, 123); expect(0, 0);
    // Landscape: TV fills the top two thirds; half-width/height DRC is centered below.
    Libretro::mode = Libretro::ScreenViewMode::SideBySide;
    Libretro::stacked = true;
    scan(a, larger, false, 333);
    const auto& stacked = images.at(Libretro::presented);
    assert(stacked.width == 32 && stacked.height == 27);
    for (int y = 0; y < stacked.height; ++y)
        for (int x = 0; x < stacked.width; ++x) {
            int expected = y < 18 ? 333 : (x >= 8 && x < 24 ? 111 : 0);
            assert(stacked.data.at(y * stacked.width + x) == expected);
        }
    // Rotating back rewrites the entire output with the original horizontal layout.
    Libretro::stacked = false;
    scan(a, larger, false, 444); expect(444, 111);
    std::cout << "PASS: shared textures, both orders, mode changes, missing DRC, release, resize\n";
}
"""


def main():
    production = SOURCE.read_text(encoding="utf-8")
    start = production.index("bool VulkanRenderer::BlitLibretroOutput(")
    end = production.index("\nvoid VulkanRenderer::DrawBackbufferQuad(", start)
    compiler = os.environ.get("CXX", "cl" if os.name == "nt" else "c++")
    include = ROOT / "dependencies/Vulkan-Headers/include"
    with tempfile.TemporaryDirectory(prefix="cemu-scanout-") as directory:
        work = Path(directory)
        source = work / "scanout.cpp"
        executable = work / ("scanout.exe" if os.name == "nt" else "scanout")
        source.write_text(PRELUDE + production[start:end] + TESTS, encoding="utf-8")
        if Path(compiler).stem.lower() in ("cl", "clang-cl"):
            command = [compiler, "/nologo", "/std:c++20", "/EHsc", f"/I{include}", f"/I{ROOT / 'src'}",
                       str(source), f"/Fe:{executable}"]
        else:
            command = [compiler, "-std=c++20", "-I", str(include), "-I", str(ROOT / 'src'), str(source),
                       "-o", str(executable)]
        subprocess.run(command, cwd=work, check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
