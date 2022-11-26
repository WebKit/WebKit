//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// angleutils.h: Common ANGLE utilities.

#ifndef COMMON_ANGLEUTILS_H_
#define COMMON_ANGLEUTILS_H_

#include "common/platform.h"

#if defined(ANGLE_USE_ABSEIL)
#    include "absl/container/flat_hash_map.h"
#    include "absl/container/flat_hash_set.h"
#endif  // defined(ANGLE_USE_ABSEIL)

#if defined(ANGLE_WITH_LSAN)
#    include <sanitizer/lsan_interface.h>
#endif  // defined(ANGLE_WITH_LSAN)

#include <climits>
#include <cstdarg>
#include <cstddef>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// A helper class to disallow copy and assignment operators
namespace angle
{

#if defined(ANGLE_ENABLE_D3D9) || defined(ANGLE_ENABLE_D3D11)
using Microsoft::WRL::ComPtr;
#endif  // defined(ANGLE_ENABLE_D3D9) || defined(ANGLE_ENABLE_D3D11)

#if defined(ANGLE_USE_ABSEIL)
template <typename Key,
          typename T,
          class Hash = absl::container_internal::hash_default_hash<Key>,
          class Eq   = absl::container_internal::hash_default_eq<Key>>
using HashMap = absl::flat_hash_map<Key, T, Hash, Eq>;
template <typename Key,
          class Hash = absl::container_internal::hash_default_hash<Key>,
          class Eq   = absl::container_internal::hash_default_eq<Key>>
using HashSet = absl::flat_hash_set<Key, Hash, Eq>;
#else
template <typename Key,
          typename T,
          class Hash     = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>>
using HashMap = std::unordered_map<Key, T, Hash, KeyEqual>;
template <typename Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
using HashSet = std::unordered_set<Key, Hash, KeyEqual>;
#endif  // defined(ANGLE_USE_ABSEIL)

class NonCopyable
{
  protected:
    constexpr NonCopyable() = default;
    ~NonCopyable()          = default;

  private:
    NonCopyable(const NonCopyable &)    = delete;
    void operator=(const NonCopyable &) = delete;
};

extern const uintptr_t DirtyPointer;

struct SaveFileHelper
{
  public:
    // We always use ios::binary to avoid inconsistent line endings when captured on Linux vs Win.
    SaveFileHelper(const std::string &filePathIn);
    ~SaveFileHelper();

    template <typename T>
    SaveFileHelper &operator<<(const T &value)
    {
        mOfs << value;
        checkError();
        return *this;
    }

    void write(const uint8_t *data, size_t size);

  private:
    void checkError();

    std::ofstream mOfs;
    std::string mFilePath;
};

// AMD_performance_monitor helpers.
constexpr char kPerfMonitorExtensionName[] = "GL_AMD_performance_monitor";

struct PerfMonitorCounter
{
    PerfMonitorCounter();
    ~PerfMonitorCounter();

    std::string name;
    uint64_t value;
};
using PerfMonitorCounters = std::vector<PerfMonitorCounter>;

struct PerfMonitorCounterGroup
{
    PerfMonitorCounterGroup();
    ~PerfMonitorCounterGroup();

    std::string name;
    PerfMonitorCounters counters;
};
using PerfMonitorCounterGroups = std::vector<PerfMonitorCounterGroup>;

uint32_t GetPerfMonitorCounterIndex(const PerfMonitorCounters &counters, const std::string &name);
const PerfMonitorCounter &GetPerfMonitorCounter(const PerfMonitorCounters &counters,
                                                const std::string &name);
PerfMonitorCounter &GetPerfMonitorCounter(PerfMonitorCounters &counters, const std::string &name);
uint32_t GetPerfMonitorCounterGroupIndex(const PerfMonitorCounterGroups &groups,
                                         const std::string &name);
const PerfMonitorCounterGroup &GetPerfMonitorCounterGroup(const PerfMonitorCounterGroups &groups,
                                                          const std::string &name);
PerfMonitorCounterGroup &GetPerfMonitorCounterGroup(PerfMonitorCounterGroups &groups,
                                                    const std::string &name);

struct PerfMonitorTriplet
{
    uint32_t group;
    uint32_t counter;
    uint64_t value;
};

#define ANGLE_VK_PERF_COUNTERS_X(FN)               \
    FN(commandQueueSubmitCallsTotal)               \
    FN(commandQueueSubmitCallsPerFrame)            \
    FN(vkQueueSubmitCallsTotal)                    \
    FN(vkQueueSubmitCallsPerFrame)                 \
    FN(renderPasses)                               \
    FN(writeDescriptorSets)                        \
    FN(flushedOutsideRenderPassCommandBuffers)     \
    FN(swapchainResolveInSubpass)                  \
    FN(swapchainResolveOutsideSubpass)             \
    FN(resolveImageCommands)                       \
    FN(colorLoadOpClears)                          \
    FN(colorLoadOpLoads)                           \
    FN(colorLoadOpNones)                           \
    FN(colorStoreOpStores)                         \
    FN(colorStoreOpNones)                          \
    FN(colorClearAttachments)                      \
    FN(depthLoadOpClears)                          \
    FN(depthLoadOpLoads)                           \
    FN(depthLoadOpNones)                           \
    FN(depthStoreOpStores)                         \
    FN(depthStoreOpNones)                          \
    FN(depthClearAttachments)                      \
    FN(stencilLoadOpClears)                        \
    FN(stencilLoadOpLoads)                         \
    FN(stencilLoadOpNones)                         \
    FN(stencilStoreOpStores)                       \
    FN(stencilStoreOpNones)                        \
    FN(stencilClearAttachments)                    \
    FN(colorAttachmentUnresolves)                  \
    FN(depthAttachmentUnresolves)                  \
    FN(stencilAttachmentUnresolves)                \
    FN(colorAttachmentResolves)                    \
    FN(depthAttachmentResolves)                    \
    FN(stencilAttachmentResolves)                  \
    FN(readOnlyDepthStencilRenderPasses)           \
    FN(pipelineCreationCacheHits)                  \
    FN(pipelineCreationCacheMisses)                \
    FN(pipelineCreationTotalCacheHitsDurationNs)   \
    FN(pipelineCreationTotalCacheMissesDurationNs) \
    FN(descriptorSetAllocations)                   \
    FN(descriptorSetCacheTotalSize)                \
    FN(descriptorSetCacheKeySizeBytes)             \
    FN(uniformsAndXfbDescriptorSetCacheHits)       \
    FN(uniformsAndXfbDescriptorSetCacheMisses)     \
    FN(uniformsAndXfbDescriptorSetCacheTotalSize)  \
    FN(textureDescriptorSetCacheHits)              \
    FN(textureDescriptorSetCacheMisses)            \
    FN(textureDescriptorSetCacheTotalSize)         \
    FN(shaderResourcesDescriptorSetCacheHits)      \
    FN(mutableTexturesUploaded)                    \
    FN(shaderResourcesDescriptorSetCacheMisses)    \
    FN(shaderResourcesDescriptorSetCacheTotalSize) \
    FN(buffersGhosted)                             \
    FN(vertexArraySyncStateCalls)                  \
    FN(allocateNewBufferBlockCalls)                \
    FN(dynamicBufferAllocations)                   \
    FN(framebufferCacheSize)

#define ANGLE_DECLARE_PERF_COUNTER(COUNTER) uint64_t COUNTER;

struct VulkanPerfCounters
{
    ANGLE_VK_PERF_COUNTERS_X(ANGLE_DECLARE_PERF_COUNTER)
};

#undef ANGLE_DECLARE_PERF_COUNTER

}  // namespace angle

template <typename T, size_t N>
constexpr inline size_t ArraySize(T (&)[N])
{
    return N;
}

template <typename T>
class WrappedArray final : angle::NonCopyable
{
  public:
    template <size_t N>
    constexpr WrappedArray(const T (&data)[N]) : mArray(&data[0]), mSize(N)
    {}

    constexpr WrappedArray() : mArray(nullptr), mSize(0) {}
    constexpr WrappedArray(const T *data, size_t size) : mArray(data), mSize(size) {}

    WrappedArray(WrappedArray &&other) : WrappedArray()
    {
        std::swap(mArray, other.mArray);
        std::swap(mSize, other.mSize);
    }

    ~WrappedArray() {}

    constexpr const T *get() const { return mArray; }
    constexpr size_t size() const { return mSize; }

  private:
    const T *mArray;
    size_t mSize;
};

template <typename T, unsigned int N>
void SafeRelease(T (&resourceBlock)[N])
{
    for (unsigned int i = 0; i < N; i++)
    {
        SafeRelease(resourceBlock[i]);
    }
}

template <typename T>
void SafeRelease(T &resource)
{
    if (resource)
    {
        resource->Release();
        resource = nullptr;
    }
}

template <typename T>
void SafeDelete(T *&resource)
{
    delete resource;
    resource = nullptr;
}

template <typename T>
void SafeDeleteContainer(T &resource)
{
    for (auto &element : resource)
    {
        SafeDelete(element);
    }
    resource.clear();
}

template <typename T>
void SafeDeleteArray(T *&resource)
{
    delete[] resource;
    resource = nullptr;
}

// Provide a less-than function for comparing structs
// Note: struct memory must be initialized to zero, because of packing gaps
template <typename T>
inline bool StructLessThan(const T &a, const T &b)
{
    return (memcmp(&a, &b, sizeof(T)) < 0);
}

// Provide a less-than function for comparing structs
// Note: struct memory must be initialized to zero, because of packing gaps
template <typename T>
inline bool StructEquals(const T &a, const T &b)
{
    return (memcmp(&a, &b, sizeof(T)) == 0);
}

template <typename T>
inline void StructZero(T *obj)
{
    memset(obj, 0, sizeof(T));
}

template <typename T>
inline bool IsMaskFlagSet(T mask, T flag)
{
    // Handles multibit flags as well
    return (mask & flag) == flag;
}

inline const char *MakeStaticString(const std::string &str)
{
    // On the heap so that no destructor runs on application exit.
    static std::set<std::string> *strings = new std::set<std::string>;
    std::set<std::string>::iterator it    = strings->find(str);
    if (it != strings->end())
    {
        return it->c_str();
    }

    return strings->insert(str).first->c_str();
}

std::string ArrayString(unsigned int i);

// Indices are stored in vectors with the outermost index in the back. In the output of the function
// the indices are reversed.
std::string ArrayIndexString(const std::vector<unsigned int> &indices);

inline std::string Str(int i)
{
    std::stringstream strstr;
    strstr << i;
    return strstr.str();
}

template <typename T>
std::string ToString(const T &value)
{
    std::ostringstream o;
    o << value;
    return o.str();
}

inline bool IsLittleEndian()
{
    constexpr uint32_t kEndiannessTest = 1;
    const bool isLittleEndian          = *reinterpret_cast<const uint8_t *>(&kEndiannessTest) == 1;
    return isLittleEndian;
}

// Helper class to use a mutex with the control of boolean.
class ConditionalMutex final : angle::NonCopyable
{
  public:
    ConditionalMutex() : mUseMutex(true) {}
    void init(bool useMutex) { mUseMutex = useMutex; }
    void lock()
    {
        if (mUseMutex)
        {
            mMutex.lock();
        }
    }
    void unlock()
    {
        if (mUseMutex)
        {
            mMutex.unlock();
        }
    }

  private:
    std::mutex mMutex;
    bool mUseMutex;
};

// snprintf is not defined with MSVC prior to to msvc14
#if defined(_MSC_VER) && _MSC_VER < 1900
#    define snprintf _snprintf
#endif

#define GL_A1RGB5_ANGLEX 0x6AC5
#define GL_BGRX8_ANGLEX 0x6ABA
#define GL_BGR565_ANGLEX 0x6ABB
#define GL_BGRA4_ANGLEX 0x6ABC
#define GL_BGR5_A1_ANGLEX 0x6ABD
#define GL_INT_64_ANGLEX 0x6ABE
#define GL_UINT_64_ANGLEX 0x6ABF
#define GL_BGRA8_SRGB_ANGLEX 0x6AC0
#define GL_BGR10_A2_ANGLEX 0x6AF9

// These are fake formats used to fit typeless D3D textures that can be bound to EGL pbuffers into
// the format system (for extension EGL_ANGLE_d3d_texture_client_buffer):
#define GL_RGBA8_TYPELESS_ANGLEX 0x6AC1
#define GL_RGBA8_TYPELESS_SRGB_ANGLEX 0x6AC2
#define GL_BGRA8_TYPELESS_ANGLEX 0x6AC3
#define GL_BGRA8_TYPELESS_SRGB_ANGLEX 0x6AC4

#define GL_R8_SSCALED_ANGLEX 0x6AC6
#define GL_RG8_SSCALED_ANGLEX 0x6AC7
#define GL_RGB8_SSCALED_ANGLEX 0x6AC8
#define GL_RGBA8_SSCALED_ANGLEX 0x6AC9
#define GL_R8_USCALED_ANGLEX 0x6ACA
#define GL_RG8_USCALED_ANGLEX 0x6ACB
#define GL_RGB8_USCALED_ANGLEX 0x6ACC
#define GL_RGBA8_USCALED_ANGLEX 0x6ACD

#define GL_R16_SSCALED_ANGLEX 0x6ACE
#define GL_RG16_SSCALED_ANGLEX 0x6ACF
#define GL_RGB16_SSCALED_ANGLEX 0x6AD0
#define GL_RGBA16_SSCALED_ANGLEX 0x6AD1
#define GL_R16_USCALED_ANGLEX 0x6AD2
#define GL_RG16_USCALED_ANGLEX 0x6AD3
#define GL_RGB16_USCALED_ANGLEX 0x6AD4
#define GL_RGBA16_USCALED_ANGLEX 0x6AD5

#define GL_R32_SSCALED_ANGLEX 0x6AD6
#define GL_RG32_SSCALED_ANGLEX 0x6AD7
#define GL_RGB32_SSCALED_ANGLEX 0x6AD8
#define GL_RGBA32_SSCALED_ANGLEX 0x6AD9
#define GL_R32_USCALED_ANGLEX 0x6ADA
#define GL_RG32_USCALED_ANGLEX 0x6ADB
#define GL_RGB32_USCALED_ANGLEX 0x6ADC
#define GL_RGBA32_USCALED_ANGLEX 0x6ADD

#define GL_R32_SNORM_ANGLEX 0x6ADE
#define GL_RG32_SNORM_ANGLEX 0x6ADF
#define GL_RGB32_SNORM_ANGLEX 0x6AE0
#define GL_RGBA32_SNORM_ANGLEX 0x6AE1
#define GL_R32_UNORM_ANGLEX 0x6AE2
#define GL_RG32_UNORM_ANGLEX 0x6AE3
#define GL_RGB32_UNORM_ANGLEX 0x6AE4
#define GL_RGBA32_UNORM_ANGLEX 0x6AE5

#define GL_R32_FIXED_ANGLEX 0x6AE6
#define GL_RG32_FIXED_ANGLEX 0x6AE7
#define GL_RGB32_FIXED_ANGLEX 0x6AE8
#define GL_RGBA32_FIXED_ANGLEX 0x6AE9

#define GL_RGB10_A2_SINT_ANGLEX 0x6AEA
#define GL_RGB10_A2_SNORM_ANGLEX 0x6AEB
#define GL_RGB10_A2_SSCALED_ANGLEX 0x6AEC
#define GL_RGB10_A2_USCALED_ANGLEX 0x6AED

// EXT_texture_type_2_10_10_10_REV
#define GL_RGB10_UNORM_ANGLEX 0x6AEE

// These are fake formats for OES_vertex_type_10_10_10_2
#define GL_A2_RGB10_UNORM_ANGLEX 0x6AEF
#define GL_A2_RGB10_SNORM_ANGLEX 0x6AF0
#define GL_A2_RGB10_USCALED_ANGLEX 0x6AF1
#define GL_A2_RGB10_SSCALED_ANGLEX 0x6AF2
#define GL_X2_RGB10_UINT_ANGLEX 0x6AF3
#define GL_X2_RGB10_SINT_ANGLEX 0x6AF4
#define GL_X2_RGB10_USCALED_ANGLEX 0x6AF5
#define GL_X2_RGB10_SSCALED_ANGLEX 0x6AF6
#define GL_X2_RGB10_UNORM_ANGLEX 0x6AF7
#define GL_X2_RGB10_SNORM_ANGLEX 0x6AF8

#define ANGLE_CHECK_GL_ALLOC(context, result) \
    ANGLE_CHECK(context, result, "Failed to allocate host memory", GL_OUT_OF_MEMORY)

#define ANGLE_CHECK_GL_MATH(context, result) \
    ANGLE_CHECK(context, result, "Integer overflow.", GL_INVALID_OPERATION)

#define ANGLE_GL_UNREACHABLE(context) \
    UNREACHABLE();                    \
    ANGLE_CHECK(context, false, "Unreachable Code.", GL_INVALID_OPERATION)

#if defined(ANGLE_WITH_LSAN)
#    define ANGLE_SCOPED_DISABLE_LSAN() __lsan::ScopedDisabler lsanDisabler
#else
#    define ANGLE_SCOPED_DISABLE_LSAN()
#endif

#if defined(ANGLE_WITH_MSAN)
class MsanScopedDisableInterceptorChecks final : angle::NonCopyable
{
  public:
    MsanScopedDisableInterceptorChecks() { __msan_scoped_disable_interceptor_checks(); }
    ~MsanScopedDisableInterceptorChecks() { __msan_scoped_enable_interceptor_checks(); }
};
#    define ANGLE_SCOPED_DISABLE_MSAN() \
        MsanScopedDisableInterceptorChecks msanScopedDisableInterceptorChecks
#else
#    define ANGLE_SCOPED_DISABLE_MSAN()
#endif

// The ANGLE_NO_SANITIZE_MEMORY macro suppresses MemorySanitizer checks for
// use-of-uninitialized-data. It can be used to decorate functions with known
// false positives.
#ifdef __clang__
#    define ANGLE_NO_SANITIZE_MEMORY __attribute__((no_sanitize_memory))
#else
#    define ANGLE_NO_SANITIZE_MEMORY
#endif

// Similar to the above, but for thread sanitization.
#ifdef __clang__
#    define ANGLE_NO_SANITIZE_THREAD __attribute__((no_sanitize_thread))
#else
#    define ANGLE_NO_SANITIZE_THREAD
#endif

// The below inlining code lifted from V8.
#if defined(__clang__) || (defined(__GNUC__) && defined(__has_attribute))
#    define ANGLE_HAS_ATTRIBUTE_ALWAYS_INLINE (__has_attribute(always_inline))
#    define ANGLE_HAS___FORCEINLINE 0
#elif defined(_MSC_VER)
#    define ANGLE_HAS_ATTRIBUTE_ALWAYS_INLINE 0
#    define ANGLE_HAS___FORCEINLINE 1
#else
#    define ANGLE_HAS_ATTRIBUTE_ALWAYS_INLINE 0
#    define ANGLE_HAS___FORCEINLINE 0
#endif

#if defined(NDEBUG) && ANGLE_HAS_ATTRIBUTE_ALWAYS_INLINE
#    define ANGLE_INLINE inline __attribute__((always_inline))
#elif defined(NDEBUG) && ANGLE_HAS___FORCEINLINE
#    define ANGLE_INLINE __forceinline
#else
#    define ANGLE_INLINE inline
#endif

#if defined(__clang__) || (defined(__GNUC__) && defined(__has_attribute))
#    if __has_attribute(noinline)
#        define ANGLE_NOINLINE __attribute__((noinline))
#    else
#        define ANGLE_NOINLINE
#    endif
#elif defined(_MSC_VER)
#    define ANGLE_NOINLINE __declspec(noinline)
#else
#    define ANGLE_NOINLINE
#endif

#if defined(__clang__) || (defined(__GNUC__) && defined(__has_attribute))
#    if __has_attribute(format)
#        define ANGLE_FORMAT_PRINTF(fmt, args) __attribute__((format(__printf__, fmt, args)))
#    else
#        define ANGLE_FORMAT_PRINTF(fmt, args)
#    endif
#else
#    define ANGLE_FORMAT_PRINTF(fmt, args)
#endif

ANGLE_FORMAT_PRINTF(1, 0)
size_t FormatStringIntoVector(const char *fmt, va_list vararg, std::vector<char> &buffer);

// Format messes up the # inside the macro.
// clang-format off
#ifndef ANGLE_STRINGIFY
#    define ANGLE_STRINGIFY(x) #x
#endif
// clang-format on

#ifndef ANGLE_MACRO_STRINGIFY
#    define ANGLE_MACRO_STRINGIFY(x) ANGLE_STRINGIFY(x)
#endif

// The ANGLE_MAYBE_UNUSED_PRIVATE_FIELD can be used to hint 'unused private field'
// instead of 'maybe_unused' attribute for the compatibility with GCC because
// GCC doesn't have '-Wno-unused-private-field' whereas Clang has.
#if defined(__clang__) || defined(_MSC_VER)
#    define ANGLE_MAYBE_UNUSED_PRIVATE_FIELD [[maybe_unused]]
#else
#    define ANGLE_MAYBE_UNUSED_PRIVATE_FIELD
#endif

#if __has_cpp_attribute(clang::require_constant_initialization)
#    define ANGLE_REQUIRE_CONSTANT_INIT [[clang::require_constant_initialization]]
#else
#    define ANGLE_REQUIRE_CONSTANT_INIT
#endif  // __has_cpp_attribute(require_constant_initialization)

// Compiler configs.
inline bool IsASan()
{
#if defined(ANGLE_WITH_ASAN)
    return true;
#else
    return false;
#endif  // defined(ANGLE_WITH_ASAN)
}

inline bool IsMSan()
{
#if defined(ANGLE_WITH_MSAN)
    return true;
#else
    return false;
#endif  // defined(ANGLE_WITH_MSAN)
}

inline bool IsTSan()
{
#if defined(ANGLE_WITH_TSAN)
    return true;
#else
    return false;
#endif  // defined(ANGLE_WITH_TSAN)
}

inline bool IsUBSan()
{
#if defined(ANGLE_WITH_UBSAN)
    return true;
#else
    return false;
#endif  // defined(ANGLE_WITH_UBSAN)
}
#endif  // COMMON_ANGLEUTILS_H_
