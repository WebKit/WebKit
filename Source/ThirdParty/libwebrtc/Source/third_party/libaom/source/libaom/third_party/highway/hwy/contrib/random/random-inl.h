/*
 * Original implementation written in 2019
 * by David Blackman and Sebastiano Vigna (vigna@acm.org)
 * Available at https://prng.di.unimi.it/ with creative commons license:
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 * See <http://creativecommons.org/publicdomain/zero/1.0/>.
 *
 * This implementation is a Vector port of the original implementation
 * written by Marco Barbone (m.barbone19@imperial.ac.uk).
 * I take no credit for the original implementation.
 * The code is provided as is and the original license applies.
 */

#if defined(HIGHWAY_HWY_CONTRIB_RANDOM_RANDOM_H_) == \
    defined(HWY_TARGET_TOGGLE)  // NOLINT
#ifdef HIGHWAY_HWY_CONTRIB_RANDOM_RANDOM_H_
#undef HIGHWAY_HWY_CONTRIB_RANDOM_RANDOM_H_
#else
#define HIGHWAY_HWY_CONTRIB_RANDOM_RANDOM_H_
#endif

#include <stddef.h>

#include <array>
#include <cstdint>
#include <limits>

#include "third_party/highway/hwy/aligned_allocator.h"
#include "third_party/highway/hwy/contrib/sort/vqsort.h"  // Fill16BytesSecure
#include "third_party/highway/hwy/highway.h"
#include "third_party/highway/hwy/timer.h"

HWY_BEFORE_NAMESPACE();  // required if not using HWY_ATTR

namespace hwy {

namespace HWY_NAMESPACE {  // required: unique per target
namespace internal {

#if HWY_HAVE_FLOAT64
// C++ < 17 does not support hexfloat
#if __cpp_hex_float > 201603L
constexpr double kMulConst = 0x1.0p-53;
#else
constexpr double kMulConst =
    0.00000000000000011102230246251565404236316680908203125;
#endif  // __cpp_hex_float

#endif  // HWY_HAVE_FLOAT64

constexpr std::uint64_t kJump[] = {0x180ec6d33cfd0aba, 0xd5a61266f0c9392c,
                                   0xa9582618e03fc9aa, 0x39abdc4529b1661c};

constexpr std::uint64_t kLongJump[] = {0x76e15d3efefdcbbf, 0xc5004e441c522fb3,
                                       0x77710069854ee241, 0x39109bb02acbe635};

class SplitMix64 {
 public:
  constexpr explicit SplitMix64(const std::uint64_t state) noexcept
      : state_(state) {}

  HWY_CXX14_CONSTEXPR std::uint64_t operator()() {
    std::uint64_t z = (state_ += 0x9e3779b97f4a7c15);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    return z ^ (z >> 31);
  }

 private:
  std::uint64_t state_;
};

class Xoshiro {
 public:
  HWY_CXX14_CONSTEXPR explicit Xoshiro(const std::uint64_t seed) noexcept
      : state_{} {
    SplitMix64 splitMix64{seed};
    for (auto &element : state_) {
      element = splitMix64();
    }
  }

  HWY_CXX14_CONSTEXPR explicit Xoshiro(const std::uint64_t seed,
                                       const std::uint64_t thread_id) noexcept
      : Xoshiro(seed) {
    for (auto i = UINT64_C(0); i < thread_id; ++i) {
      Jump();
    }
  }

  HWY_CXX14_CONSTEXPR std::uint64_t operator()() noexcept { return Next(); }

#if HWY_HAVE_FLOAT64
  HWY_CXX14_CONSTEXPR double Uniform() noexcept {
    return static_cast<double>(Next() >> 11) * kMulConst;
  }
#endif

  HWY_CXX14_CONSTEXPR std::array<std::uint64_t, 4> GetState() const {
    return {state_[0], state_[1], state_[2], state_[3]};
  }

  HWY_CXX17_CONSTEXPR void SetState(
      std::array<std::uint64_t, 4> state) noexcept {
    state_[0] = state[0];
    state_[1] = state[1];
    state_[2] = state[2];
    state_[3] = state[3];
  }

  static constexpr std::uint64_t StateSize() noexcept { return 4; }

  /* This is the jump function for the generator. It is equivalent to 2^128
   * calls to next(); it can be used to generate 2^128 non-overlapping
   * subsequences for parallel computations. */
  HWY_CXX14_CONSTEXPR void Jump() noexcept { Jump(kJump); }

  /* This is the long-jump function for the generator. It is equivalent to 2^192
   * calls to next(); it can be used to generate 2^64 starting points, from each
   * of which jump() will generate 2^64 non-overlapping subsequences for
   * parallel distributed computations. */
  HWY_CXX14_CONSTEXPR void LongJump() noexcept { Jump(kLongJump); }

 private:
  std::uint64_t state_[4];

  static constexpr std::uint64_t Rotl(const std::uint64_t x, int k) noexcept {
    return (x << k) | (x >> (64 - k));
  }

  HWY_CXX14_CONSTEXPR std::uint64_t Next() noexcept {
    const std::uint64_t result = Rotl(state_[0] + state_[3], 23) + state_[0];
    const std::uint64_t t = state_[1] << 17;

    state_[2] ^= state_[0];
    state_[3] ^= state_[1];
    state_[1] ^= state_[2];
    state_[0] ^= state_[3];

    state_[2] ^= t;

    state_[3] = Rotl(state_[3], 45);

    return result;
  }

  HWY_CXX14_CONSTEXPR void Jump(const std::uint64_t (&jumpArray)[4]) noexcept {
    std::uint64_t s0 = 0;
    std::uint64_t s1 = 0;
    std::uint64_t s2 = 0;
    std::uint64_t s3 = 0;

    for (const std::uint64_t i : jumpArray)
      for (std::uint_fast8_t b = 0; b < 64; b++) {
        if (i & std::uint64_t{1UL} << b) {
          s0 ^= state_[0];
          s1 ^= state_[1];
          s2 ^= state_[2];
          s3 ^= state_[3];
        }
        Next();
      }

    state_[0] = s0;
    state_[1] = s1;
    state_[2] = s2;
    state_[3] = s3;
  }
};

}  // namespace internal

class VectorXoshiro {
 private:
  using VU64 = Vec<ScalableTag<std::uint64_t>>;
  using StateType = AlignedNDArray<std::uint64_t, 2>;
#if HWY_HAVE_FLOAT64
  using VF64 = Vec<ScalableTag<double>>;
#endif

 public:
  explicit VectorXoshiro(const std::uint64_t seed,
                         const std::uint64_t threadNumber = 0)
      : state_{{internal::Xoshiro::StateSize(),
                Lanes(ScalableTag<std::uint64_t>{})}},
        streams{state_.shape().back()} {
    internal::Xoshiro xoshiro{seed};

    for (std::uint64_t i = 0; i < threadNumber; ++i) {
      xoshiro.LongJump();
    }

    for (size_t i = 0UL; i < streams; ++i) {
      const auto state = xoshiro.GetState();
      for (size_t j = 0UL; j < internal::Xoshiro::StateSize(); ++j) {
        state_[{j}][i] = state[j];
      }
      xoshiro.Jump();
    }
  }

  HWY_INLINE VU64 operator()() noexcept { return Next(); }

  AlignedVector<std::uint64_t> operator()(const size_t n) {
    AlignedVector<std::uint64_t> result(n);
    const ScalableTag<std::uint64_t> tag{};
    auto s0 = Load(tag, state_[{0}].data());
    auto s1 = Load(tag, state_[{1}].data());
    auto s2 = Load(tag, state_[{2}].data());
    auto s3 = Load(tag, state_[{3}].data());
    for (std::uint64_t i = 0; i < n; i += Lanes(tag)) {
      const auto next = Update(s0, s1, s2, s3);
      Store(next, tag, result.data() + i);
    }
    Store(s0, tag, state_[{0}].data());
    Store(s1, tag, state_[{1}].data());
    Store(s2, tag, state_[{2}].data());
    Store(s3, tag, state_[{3}].data());
    return result;
  }

  template <std::uint64_t N>
  std::array<std::uint64_t, N> operator()() noexcept {
    alignas(HWY_ALIGNMENT) std::array<std::uint64_t, N> result;
    const ScalableTag<std::uint64_t> tag{};
    auto s0 = Load(tag, state_[{0}].data());
    auto s1 = Load(tag, state_[{1}].data());
    auto s2 = Load(tag, state_[{2}].data());
    auto s3 = Load(tag, state_[{3}].data());
    for (std::uint64_t i = 0; i < N; i += Lanes(tag)) {
      const auto next = Update(s0, s1, s2, s3);
      Store(next, tag, result.data() + i);
    }
    Store(s0, tag, state_[{0}].data());
    Store(s1, tag, state_[{1}].data());
    Store(s2, tag, state_[{2}].data());
    Store(s3, tag, state_[{3}].data());
    return result;
  }

  std::uint64_t StateSize() const noexcept {
    return streams * internal::Xoshiro::StateSize();
  }

  const StateType &GetState() const { return state_; }

#if HWY_HAVE_FLOAT64

  HWY_INLINE VF64 Uniform() noexcept {
    const ScalableTag<double> real_tag{};
    const auto MUL_VALUE = Set(real_tag, internal::kMulConst);
    const auto bits = ShiftRight<11>(Next());
    const auto real = ConvertTo(real_tag, bits);
    return Mul(real, MUL_VALUE);
  }

  AlignedVector<double> Uniform(const size_t n) {
    AlignedVector<double> result(n);
    const ScalableTag<std::uint64_t> tag{};
    const ScalableTag<double> real_tag{};
    const auto MUL_VALUE = Set(real_tag, internal::kMulConst);

    auto s0 = Load(tag, state_[{0}].data());
    auto s1 = Load(tag, state_[{1}].data());
    auto s2 = Load(tag, state_[{2}].data());
    auto s3 = Load(tag, state_[{3}].data());

    for (std::uint64_t i = 0; i < n; i += Lanes(real_tag)) {
      const auto next = Update(s0, s1, s2, s3);
      const auto bits = ShiftRight<11>(next);
      const auto real = ConvertTo(real_tag, bits);
      const auto uniform = Mul(real, MUL_VALUE);
      Store(uniform, real_tag, result.data() + i);
    }

    Store(s0, tag, state_[{0}].data());
    Store(s1, tag, state_[{1}].data());
    Store(s2, tag, state_[{2}].data());
    Store(s3, tag, state_[{3}].data());
    return result;
  }

  template <std::uint64_t N>
  std::array<double, N> Uniform() noexcept {
    alignas(HWY_ALIGNMENT) std::array<double, N> result;
    const ScalableTag<std::uint64_t> tag{};
    const ScalableTag<double> real_tag{};
    const auto MUL_VALUE = Set(real_tag, internal::kMulConst);

    auto s0 = Load(tag, state_[{0}].data());
    auto s1 = Load(tag, state_[{1}].data());
    auto s2 = Load(tag, state_[{2}].data());
    auto s3 = Load(tag, state_[{3}].data());

    for (std::uint64_t i = 0; i < N; i += Lanes(real_tag)) {
      const auto next = Update(s0, s1, s2, s3);
      const auto bits = ShiftRight<11>(next);
      const auto real = ConvertTo(real_tag, bits);
      const auto uniform = Mul(real, MUL_VALUE);
      Store(uniform, real_tag, result.data() + i);
    }

    Store(s0, tag, state_[{0}].data());
    Store(s1, tag, state_[{1}].data());
    Store(s2, tag, state_[{2}].data());
    Store(s3, tag, state_[{3}].data());
    return result;
  }

#endif

 private:
  StateType state_;
  const std::uint64_t streams;

  HWY_INLINE static VU64 Update(VU64 &s0, VU64 &s1, VU64 &s2,
                                VU64 &s3) noexcept {
    const auto result = Add(RotateRight<41>(Add(s0, s3)), s0);
    const auto t = ShiftLeft<17>(s1);
    s2 = Xor(s2, s0);
    s3 = Xor(s3, s1);
    s1 = Xor(s1, s2);
    s0 = Xor(s0, s3);
    s2 = Xor(s2, t);
    s3 = RotateRight<19>(s3);
    return result;
  }

  HWY_INLINE VU64 Next() noexcept {
    const ScalableTag<std::uint64_t> tag{};
    auto s0 = Load(tag, state_[{0}].data());
    auto s1 = Load(tag, state_[{1}].data());
    auto s2 = Load(tag, state_[{2}].data());
    auto s3 = Load(tag, state_[{3}].data());
    auto result = Update(s0, s1, s2, s3);
    Store(s0, tag, state_[{0}].data());
    Store(s1, tag, state_[{1}].data());
    Store(s2, tag, state_[{2}].data());
    Store(s3, tag, state_[{3}].data());
    return result;
  }
};

template <std::uint64_t size = 1024>
class CachedXoshiro {
 public:
  using result_type = std::uint64_t;

  static constexpr result_type(min)() {
    return (std::numeric_limits<result_type>::min)();
  }

  static constexpr result_type(max)() {
    return (std::numeric_limits<result_type>::max)();
  }

  explicit CachedXoshiro(const result_type seed,
                         const result_type threadNumber = 0)
      : generator_{seed, threadNumber},
        cache_{generator_.operator()<size>()},
        index_{0} {}

  result_type operator()() noexcept {
    if (HWY_UNLIKELY(index_ == size)) {
      cache_ = std::move(generator_.operator()<size>());
      index_ = 0;
    }
    return cache_[index_++];
  }

 private:
  VectorXoshiro generator_;
  alignas(HWY_ALIGNMENT) std::array<result_type, size> cache_;
  size_t index_;

  static_assert((size & (size - 1)) == 0 && size != 0,
                "only power of 2 are supported");
};

// Non-cryptographic 64-bit pseudo-random number generator. Supports random or
// deterministic seeding.
//
// Based on 5-round AES-CTR. Supports 2^64 streams, each with period 2^64. This
// is useful for parallel sampling. Each thread can generate the stream for a
// particular task, without caring about prior/subsequent generations.
class alignas(16) AesCtrEngine {
  // "Large-scale randomness study of security margins for 100+ cryptographic
  // functions": at least four.
  // "Parallel Random Numbers: As Easy as 1, 2, 3": four not Crush-resistant.
  static constexpr size_t kRounds = 5;

 public:
  // If `deterministic` is true, uses a fixed seed; otherwise, attempts to
  // grab entropy from the OS.
  explicit AesCtrEngine(bool deterministic) {
    // Pi-based nothing up my sleeve numbers from Randen.
    key_[0] = 0x243F6A8885A308D3ull;
    key_[1] = 0x13198A2E03707344ull;

    if (!deterministic) {  // want random seed
      if (!hwy::Fill16BytesSecure(key_)) {
        HWY_WARN("Failed to fill RNG key with secure random bits");
        // Entropy not available. The test requires that we inject some
        // differences relative to the deterministic seeds.
        key_[0] ^= reinterpret_cast<uint64_t>(this);
        key_[1] ^= hwy::timer::Start();
      }
    }

    // Simple key schedule: swap and add constant (also from Randen).
    for (size_t i = 0; i < kRounds; ++i) {
      key_[2 + 2 * i + 0] = key_[2 * i + 1] + 0xA4093822299F31D0ull;
      key_[2 + 2 * i + 1] = key_[2 * i + 0] + 0x082EFA98EC4E6C89ull;
    }
  }

  // Pure and thread safe; typically called via `RngStream`, which increments
  // `counter`. Throughput is about 100M/s on 3 GHz Skylake. It could be
  // increased 4x via unrolling by the AES latency (4-7 cycles), but because
  // users generally call once at a time, this requires buffering, which is not
  // worth the complexity in this application.
  uint64_t operator()(uint64_t stream, uint64_t counter) const {
#if HWY_TARGET != HWY_SCALAR
    using D = Full128<uint8_t>;  // 128 bits for AES
    using V = Vec<D>;
    const Repartition<uint64_t, D> d64;

    auto LoadKey = [](const uint64_t* ptr) HWY_ATTR -> V {
      return Load(D(), reinterpret_cast<const uint8_t*>(ptr));
    };

    V state = BitCast(D(), Dup128VecFromValues(d64, counter, stream));
    state = Xor(state, LoadKey(key_));  // initial whitening

    static_assert(kRounds == 5 && sizeof(key_) == 12 * sizeof(uint64_t), "");
    state = AESRound(state, LoadKey(key_ + 2));
    state = AESRound(state, LoadKey(key_ + 4));
    state = AESRound(state, LoadKey(key_ + 6));
    state = AESRound(state, LoadKey(key_ + 8));
    // Final round: fine to use another AESRound, including MixColumns.
    state = AESRound(state, LoadKey(key_ + 10));

    // Return lower 64 bits of the u8 vector.
    return GetLane(BitCast(d64, state));
#else
    HWY_DASSERT(0);  // Not supported.
    (void)stream;
    (void)counter;
    return 0;
#endif  // HWY_TARGET != HWY_SCALAR
  }

 private:
  uint64_t key_[2 * (1 + kRounds)];
};

// Flyweight per-thread adapter that maintains the counter. Conforms to C++
// `UniformRandomBitGenerator`.
class RngStream {
 public:
  RngStream() = default;  // Allow C arrays with subsequent initialization.

  // Binds to an engine, which holds the seed and must outlive this object.
  // Sets the stream; any other `RngStream` with the same `counter_rng` and
  // `stream` will return the same sequence. This is typically the task ID, so
  // that threads can independently generate values for each task.
  RngStream(const AesCtrEngine& counter_rng, uint64_t stream)
      : engine_(&counter_rng), stream_(stream), counter_(0) {}

  using result_type = uint64_t;
  static constexpr result_type min() { return 0; }
  static constexpr result_type max() { return ~result_type{0}; }
  result_type operator()() { return (*engine_)(stream_, counter_++); }

 private:
  const AesCtrEngine* engine_ = nullptr;
  uint64_t stream_ = 0;  // immutable after ctor
  uint64_t counter_ = 0;
  // Prevent false sharing if used by multiple threads.
  HWY_MEMBER_VAR_MAYBE_UNUSED uint8_t
      padding_[HWY_ALIGNMENT - 16 - sizeof(engine_)];
};

// Returns normalized float in [-1, 1).
HWY_INLINE float RandomNormalizedFloat(RngStream& rng) {
  const uint32_t exp = hwy::BitCastScalar<uint32_t>(1.0f);
  const uint32_t mantissa_mask = hwy::MantissaMask<float>();
  const uint32_t mantissa =
      static_cast<uint32_t>(rng() & static_cast<uint64_t>(mantissa_mask));
  const uint32_t representation = exp | mantissa;
  const float f12 = hwy::BitCastScalar<float>(representation);
  HWY_DASSERT(1.0f <= f12 && f12 < 2.0f);  // exponent is 2^0, only mantissa
  const float f = (2.0f * (f12 - 1.0f)) - 1.0f;
  HWY_DASSERT(-1.0f <= f && f < 1.0f);
  return f;
}

}  // namespace HWY_NAMESPACE
}  // namespace hwy

HWY_AFTER_NAMESPACE();

#endif  // HIGHWAY_HWY_CONTRIB_RANDOM_RANDOM_H_
