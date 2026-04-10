#include <cstdlib>

import std;

import boost.ut;
import conv;

using namespace boost::ut;
using namespace conv::io;
using namespace conv::core;
using namespace conv::cpu;

namespace {
auto testCoreTypes()
{
  "Type wrapper ctor and accessors"_test = [] {
    auto const v {1920};
    auto const w {Width(v)};
    expect(w.get() == v);
    expect(static_cast<std::uint32_t>(w) == v);

    Width const w_copy {w};
    expect(w == w_copy);

    Width const w_other {1000};
    expect(w != w_other);
    expect(w > w_other);
  };
}

auto testImage()
{
  "Valid construction and dimension getters"_test = [] {
    auto const dim {10};
    std::vector<Px> const data(static_cast<std::size_t>(dim * dim), 255);
    Image img(Width(dim), Height(dim), data);

    expect(img.getW().get() == 10_u);
    expect(img.getH().get() == 10_u);
    expect(img.view().extent(0) == 10_u);
    expect(img.view().extent(1) == 10_u);
  };

  "Invalid construction throws"_test = [] {
    auto const img_size {10};
    std::vector<Px> data(img_size, 0);

    auto const less_dim {img_size / 2};
    expect(throws<std::invalid_argument>([&] {
      Image img(Width(less_dim), Height(less_dim), data);
    })) << "Should detect dimension mismatch";

    auto const greater_dim {img_size + 1};
    expect(throws<std::invalid_argument>([&] {
      Image img(Width(greater_dim), Height(greater_dim), data);
    })) << "Should detect dimension mismatch";
  };

  "Move semantics"_test = [] {
    auto const dim {2};
    auto const val {128};
    std::vector<Px> data(static_cast<long>(dim) * dim, val);
    Image img1((Width(dim)), Height(dim), std::vector<Px>(data));

    Image img2(std::move(img1));
    expect(img2.view()[0, 0] == val);

    Image img3(Width(1), Height(1), std::vector<Px> {0});
    img3 = std::move(img2);
    expect(img3.span().size() == static_cast<long>(dim) * dim);
    expect(img3.getW().get() == dim);
  };

  "Const vs Non-Const View Access"_test = [] {
    Image img(Width(2), Height(2), std::vector<Px> {1, 2, 3, 4});
    auto view = img.view();

    auto const val {99};
    view[0, 0] = val; // Mutate via non-const view

    auto const& c_img = img;
    auto c_view       = c_img.view();
    expect(c_view[0, 0] == val);
  };
}

auto testKernel()
{
  "Valid odd dimension kernel"_test = [] {
    auto const dim {3};
    DynKernel const k(dim, {1., 2., 3., 4., 5., 6., 7., 8., 9.});
    expect(k.getDim() == dim);
    expect(k.view().extent(0) == dim);
  };

  "Even dimension throws"_test = [] {
    expect(throws<std::invalid_argument>([] {
      DynKernel const k(2, {1., 2., 3., 4.});
    })) << "Even dimensions are mathematically invalid for strict center-pixel "
           "convolution";
  };

  "Data size mismatch throws"_test = [] {
    expect(throws<std::invalid_argument>(
        [] { DynKernel const k(3, {1., 2., 3.}); }));
  };

  "Predefined Gaussian Kernel validates"_test = [] {
    auto const& k = kernel::kGaussian5x5;
    expect(k.getDim() == 5_u);
    expect(k.view().extent(0) == 5_u);
    expect(k.view()[2, 2] > k.view()[0, 0])
        << "Center weight should be the largest";
  };
}

auto testConvolution()
{
  auto const k_id    = DynKernel(3, {0., 0., 0., 0., 1., 0., 0., 0., 0.});
  auto const k_large = DynKernel(5, std::vector<double>(25, 0.2));

  "Parameterized Identity Convolution"_test = [&](auto border_type) {
    std::vector<Px> const data {10, 20, 30, 40};
    Image img(Width(2), Height(2), data);

    auto out = convolveSeq(img, k_id, border_type);

    expect(out.view()[0, 0] == data[0]);
    expect(out.view()[0, 1] == data[1]);
    expect(out.view()[1, 0] == data[2]);
    expect(out.view()[1, 1] == data[3]);
  } | std::tuple {BorderType::Zero, BorderType::Repeat};

  "Boundary Clamping (Overflow & Underflow)"_test = [] {
    DynKernel const k_big(3, {-10., -10., -10., 0., 50., 0., 10., 10., 10.});
    auto const dim {3};
    Image const img(Width(dim), Height(dim),
                    std::vector<Px>(static_cast<long>(dim) * dim, 100));
    auto out = convolveSeq(img, k_big, BorderType::Zero);

    for (auto val : out.span()) {
      expect(val >= 0_i and val <= 255_i)
          << "Pixel value leaked out of [0, 255] bounds";
    }
  };

  "Kernel larger than image"_test = [&] {
    auto const dim {2};
    Image const img(Width(dim), Height(dim),
                    std::vector<Px>(static_cast<std::size_t>(dim * dim), 128));

    for (auto border : {BorderType::Repeat, BorderType::Zero}) {
      auto result = convolveSeq(img, kernel::kGaussian5x5, border);
      expect(result.getW().get() == dim && result.getH().get() == dim);
    }
  };

  "1x1 Image"_test = [] {
    auto const val {43};
    auto const coef {9};
    Image const img(Width(1), Height(1), {val});
    std::vector const data(coef, 1. / coef);
    DynKernel const k(coef / 3, data);

    auto result = convolveSeq(img, k, BorderType::Repeat);
    expect(result.span().size() == 1_u);
    expect(result.span()[0] == val) << "Single pixel must remain unchanged";

    result = convolveSeq(img, k, BorderType::Zero);
    expect(result.span().size() == 1_u);
    auto const ans {(val / coef) + 1};
    expect(result.span()[0] == ans);
  };

  "1x1 Kernel on 1x1 Image"_test = [] {
    DynKernel const k1(1, {5.});
    Image const img(Width(1), Height(1), std::vector<Px> {10});

    auto out = convolveSeq(img, k1, BorderType::Zero);
    expect(out.view()[0, 0] == 50_i);
  };

  "Parallel & Sequential matches"_test = [&](auto border_type) {
    auto const dim {10};
    std::vector<Px> data(static_cast<long>(dim) * dim);
    std::ranges::iota(data, 0);
    Image img(Width(dim), Height(dim), data);

    auto seq_out = convolveSeq(img, kernel::kGaussian5x5, border_type);
    auto par_out = convolvePar(img, kernel::kGaussian5x5, border_type);

    expect(seq_out.span().size() == par_out.span().size());
    bool identical = std::ranges::equal(seq_out.span(), par_out.span());
    expect(identical) << "Parallel execution yielded different results than "
                         "sequential execution";
  } | std::tuple {BorderType::Zero, BorderType::Repeat};
}

auto testIO()
{
  namespace fs = std::filesystem;

  auto tmp_dir {fs::temp_directory_path()};

  "Save and Load valid image"_test = [&] {
    auto test_file = tmp_dir / "conv_coverage_test.png";

    Width w(4);
    Height h(4);
    std::vector<Px> const data = {0,  10, 20,  30,  40,  50,  60,  70,
                                  80, 90, 100, 110, 120, 130, 140, 150};

    Image img(w, h, data);

    expect(nothrow([&] { savePNG(test_file, img); }));

    auto loaded = loadGrayscaleImage(test_file);
    expect(loaded.getW() == w);
    expect(loaded.getH() == h);

    bool matched = std::ranges::equal(img.span(), loaded.span());
    expect(matched) << "Loaded image data does not match saved image data";

    std::filesystem::remove(test_file);
  };

  "Save on invalid path throws"_test = [] {
    auto const path {fs::path {"/this_folder_doesnt_exist/img.png"}};
    auto const img {Image(Width(2), Height(2), {0, 0, 0, 0})};
    expect(throws<std::runtime_error>([&] { savePNG(path, img); }));
  };

  "Load nonexistent file throws"_test = [] {
    expect(throws<std::runtime_error>(
        [] { loadGrayscaleImage("this_file_doesnt_exist.png"); }));
  };
}

auto testPipeline()
{
  namespace fs = std::filesystem;

  "Empty file list runs without error"_test = [] {
    std::vector<fs::path> files;
    auto out_dir = fs::temp_directory_path();
    expect(nothrow([&] {
      conv::pipeline::runConvPipeline(files, out_dir, kernel::kGaussian5x5, BorderType::Zero, 2);
    }));
  };

  "Valid end-to-end pipeline execution"_test = [] {
    auto base_dir = fs::temp_directory_path() / "conv_pipeline_test_valid";
    auto in_dir   = base_dir / "in";
    auto out_dir  = base_dir / "out";
    
    fs::create_directories(in_dir);
    fs::create_directories(out_dir);

    // Create 2 valid dummy images
    Image const img1(Width(2), Height(2), {10, 20, 30, 40});
    Image const img2(Width(2), Height(2), {50, 60, 70, 80});

    auto path1 = in_dir / "img1.png";
    auto path2 = in_dir / "img2.png";
    savePNG(path1, img1);
    savePNG(path2, img2);

    std::vector<fs::path> files {path1.string(), path2.string()};

    expect(nothrow([&] {
      conv::pipeline::runConvPipeline(files, out_dir, kernel::kGaussian5x5, BorderType::Zero, 2);
    }));

    expect(fs::exists(out_dir / "img1.png")) << "Pipeline failed to write img1.png";
    expect(fs::exists(out_dir / "img2.png")) << "Pipeline failed to write img2.png";

    auto res = loadGrayscaleImage(out_dir / "img1.png");
    expect(res.getW().get() == 2_u);
    expect(res.getH().get() == 2_u);

    fs::remove_all(base_dir);
  };

  "Graceful handling of missing input files"_test = [] {
    auto out_dir = fs::temp_directory_path();
    std::vector<fs::path> files {"/this/file/definitely/doesnt/exist.png"};
    expect(nothrow([&] {
      conv::pipeline::runConvPipeline(files, out_dir, kernel::kGaussian5x5, BorderType::Zero,  1);
    })) << "Pipeline should catch reader exceptions and not crash.";
  };

  "Graceful handling of invalid output directory"_test = [] {
    auto in_dir  = fs::temp_directory_path() / "conv_pipeline_test_bad_out";
    auto out_dir = "/this/output/dir/doesnt/exist/either";
    fs::create_directories(in_dir);

    Image const img(Width(2), Height(2), {10, 20, 30, 40});
    auto path = in_dir / "img_bad_out.png";
    savePNG(path, img);

    std::vector<fs::path> files {path.string()};
    expect(nothrow([&] {
      conv::pipeline::runConvPipeline(files, out_dir, kernel::kGaussian5x5, BorderType::Zero,  2);
    })) << "Pipeline should catch writer exceptions and not crash.";

    fs::remove_all(in_dir);
  };
}
} // namespace

auto main() -> int
{
  try {
    testCoreTypes();
    testImage();
    testKernel();
    testConvolution();
    testIO();
    testPipeline();
  }
  catch (std::exception const& e) {
    std::cerr << "Unhandled exception in main: " << e.what() << '\n';
    return EXIT_FAILURE;
  }
  catch (...) {
    std::cerr << "Unknown unhandled exception in main" << '\n';
    return EXIT_FAILURE;
  }
}
