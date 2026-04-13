#include <fuzzer/FuzzedDataProvider.h>

import conv;
import std;

// https://github.com/llvm/llvm-project/issues/55018
extern "C" int LLVMFuzzerTestOneInput(std::uint8_t const*, std::size_t);
extern "C" int LLVMFuzzerTestOneInput(std::uint8_t const* data,
                                      std::size_t size)
{
  using namespace conv;
  FuzzedDataProvider fdp(data, size);

  auto const k_dim {(fdp.ConsumeIntegralInRange<uint32_t>(0, 5) * 2) + 1};
  auto const k_size {k_dim * k_dim};

  std::vector<core::KValueT> k_data;
  k_data.reserve(k_size);
  for (size_t i {0}; i < k_size; ++i) {
    auto value {fdp.ConsumeFloatingPoint<core::KValueT>()};
    k_data.push_back(value);
  }

  auto const border {fdp.ConsumeBool() ? core::BorderType::Zero
                                       : core::BorderType::Repeat};

  auto const w {fdp.ConsumeIntegralInRange<uint32_t>(1, 128)};
  auto const h {fdp.ConsumeIntegralInRange<uint32_t>(1, 128)};
  auto const img_size {w * h};

  auto const img_data {fdp.ConsumeBytes<core::Px>(img_size)};
  if (img_data.size() != img_size) {
    return 0;
  }

  auto const k {core::DynKernel(k_dim, k_data)};
  auto const img {core::Image(core::Width(w), core::Height(h), img_data)};

  auto const seq_res {cpu::convolveSeq(img, k, border)};
  auto const par_res {cpu::convolvePar(img, k, border)};

  auto const seq_span {seq_res.span()};
  auto const par_span {par_res.span()};
  if (!std::ranges::equal(seq_span, par_span)) {
    __builtin_trap();
  }

  return 0;
}
