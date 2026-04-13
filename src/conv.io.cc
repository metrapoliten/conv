module;
#define STBI_FAILURE_USERMSG
#define STB_IMAGE_IMPLEMENTATION
#if defined(_WIN32)
#define STBI_WINDOWS_UTF8
#endif
#include <stb/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

module conv;
import :io;

import std;

using namespace conv;

auto io::loadGrayscaleImage(std::filesystem::path const& path) -> core::Image
{
  int w {};
  int h {};
  int ch {};
#if defined(_WIN32)
  auto path_native {path.string()};
  auto const* filename {path_native.c_str()};
#else
  auto const* filename {path.c_str()};
#endif
  std::unique_ptr<unsigned char, void (*)(void*)> data {
      stbi_load(filename, &w, &h, &ch, static_cast<int>(sizeof(core::Px))),
      stbi_image_free};
  if (data == nullptr) {
    throw std::runtime_error("Failed to open image: " + path.string() +
                             ". Reason: " + stbi_failure_reason());
  }

  auto const size {static_cast<std::size_t>(w * h)};
  auto pxs {std::views::counted(data.get(), size) |
            std::ranges::to<std::vector<core::Px>>()};
  return core::Image {core::Width(static_cast<std::uint32_t>(w)),
                      core::Height(static_cast<std::uint32_t>(h)), pxs};
}

auto io::savePNG(std::filesystem::path const& path, core::Image const& img)
    -> void
{
  auto const w_raw {static_cast<int>(img.getW().get())};
  auto const h_raw {static_cast<int>(img.getH().get())};
  auto const* const data_ptr {img.view().data_handle()};
  auto const stride_bytes {w_raw * static_cast<int>(sizeof(core::Px))};

#if defined(_WIN32)
  auto path_native {path.string()};
  auto const* filename {path_native.c_str()};
#else
  auto const* filename {path.c_str()};
#endif

  auto const res {
      stbi_write_png(filename, w_raw, h_raw, 1, data_ptr, stride_bytes)};

  if (res == 0) {
    throw std::runtime_error("Failed to save PNG: " + path.string());
  }
}
