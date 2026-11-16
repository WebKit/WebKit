#pragma once

#include <namespace.h>

#if defined(__has_include)
#if __has_include(<version>)
#include <version>
#endif
#endif

#if defined(__cpp_lib_span) && __cpp_lib_span >= 202002L
#include <span>
namespace SFRAME_NAMESPACE {
using std::span;
} // namespace SFRAME_NAMESPACE
#elif ((defined(__cplusplus) && __cplusplus >= 202002L) ||                     \
       (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)) &&                      \
  defined(__has_include) && __has_include(<span>)
#include <span>
namespace SFRAME_NAMESPACE {
using std::span;
} // namespace SFRAME_NAMESPACE
#else
#include <gsl-lite/gsl-lite.hpp>
namespace SFRAME_NAMESPACE {
using gsl_lite::span;
} // namespace SFRAME_NAMESPACE
#endif
