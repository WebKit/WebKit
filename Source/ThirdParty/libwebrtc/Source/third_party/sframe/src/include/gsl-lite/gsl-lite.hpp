//
// For more information see https://github.com/gsl-lite/gsl-lite
//
// gsl-lite is originally based on Microsoft GSL, which is an implementation of the C++ Core Guidelines Support Library:
// https://github.com/microsoft/GSL
//
// Copyright (c) 2015-2019 Martin Moene
// Copyright (c) 2019-2026 Moritz Beutel
// Copyright (c) 2015 Microsoft Corporation. All rights reserved.
//
// This code is licensed under the MIT License (MIT).
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef GSL_LITE_GSL_LITE_HPP_INCLUDED
#define GSL_LITE_GSL_LITE_HPP_INCLUDED

#include <exception> // for exception, terminate(), uncaught_exceptions()
#include <limits>
#include <memory>    // for addressof(), unique_ptr<>, shared_ptr<>
#include <iosfwd>    // for basic_ostream<>
#include <stdexcept> // for logic_error
#include <utility>   // for move(), forward<>(), swap()
#include <cstddef>   // for size_t, ptrdiff_t, nullptr_t
#include <cstdlib>   // for abort()

#if defined( __has_include )
# if __has_include( <version> )
#  include <version>
# endif
#endif

#define  gsl_lite_MAJOR  1
#define  gsl_lite_MINOR  1
#define  gsl_lite_PATCH  0

#define  gsl_lite_VERSION  gsl_STRINGIFY(gsl_lite_MAJOR) "." gsl_STRINGIFY(gsl_lite_MINOR) "." gsl_STRINGIFY(gsl_lite_PATCH)

#define gsl_STRINGIFY(  x )  gsl_STRINGIFY_( x )
#define gsl_STRINGIFY_( x )  #x
#define gsl_CONCAT_(  a, b )  gsl_CONCAT2_( a, b )
#define gsl_CONCAT2_( a, b )  a##b
#define gsl_EVALF_( f )  f()

// Configuration argument checking:

#define gsl_DETAIL_CFG_TOGGLE_VALUE_1  1
#define gsl_DETAIL_CFG_TOGGLE_VALUE_0  1
#define gsl_DETAIL_CFG_DEFAULTS_VERSION_VALUE_1  1
#define gsl_DETAIL_CFG_DEFAULTS_VERSION_VALUE_0  1
#define gsl_DETAIL_CFG_STD_VALUE_98  1
#define gsl_DETAIL_CFG_STD_VALUE_0   1
#define gsl_DETAIL_CFG_STD_VALUE_3   1
#define gsl_DETAIL_CFG_STD_VALUE_03  1
#define gsl_DETAIL_CFG_STD_VALUE_11  1
#define gsl_DETAIL_CFG_STD_VALUE_14  1
#define gsl_DETAIL_CFG_STD_VALUE_17  1
#define gsl_DETAIL_CFG_STD_VALUE_20  1
#define gsl_DETAIL_CFG_STD_VALUE_23  1
#define gsl_DETAIL_CFG_STD_VALUE_26  1
#define gsl_DETAIL_CFG_NO_VALUE_   1
#define gsl_DETAIL_CFG_NO_VALUE_1  1 // many compilers treat the command-line parameter "-Dfoo" as equivalent to "-Dfoo=1", so we tolerate that
#define gsl_CHECK_CFG_TOGGLE_VALUE_( x )  gsl_CONCAT_( gsl_DETAIL_CFG_TOGGLE_VALUE_, x )
#define gsl_CHECK_CFG_DEFAULTS_VERSION_VALUE_( x )  gsl_CONCAT_( gsl_DETAIL_CFG_DEFAULTS_VERSION_VALUE_, x )
#define gsl_CHECK_CFG_STD_VALUE_( x )  gsl_CONCAT_( gsl_DETAIL_CFG_STD_VALUE_, x )
#define gsl_CHECK_CFG_NO_VALUE_( x )  gsl_CONCAT_( gsl_DETAIL_CFG_NO_VALUE, gsl_CONCAT_( _, x ) )

// gsl-lite backward compatibility:

#if defined( gsl_CONFIG_ACKNOWLEDGE_NONSTANDARD_ABI )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_ACKNOWLEDGE_NONSTANDARD_ABI )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_ACKNOWLEDGE_NONSTANDARD_ABI=" gsl_STRINGIFY(gsl_CONFIG_ACKNOWLEDGE_NONSTANDARD_ABI) "; macro must be defined without value")
# endif
#endif

#if defined( gsl_CONFIG_DEFAULTS_VERSION )
# if ! gsl_CHECK_CFG_DEFAULTS_VERSION_VALUE_( gsl_CONFIG_DEFAULTS_VERSION )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_DEFAULTS_VERSION=" gsl_STRINGIFY(gsl_CONFIG_DEFAULTS_VERSION) ", must be 0 or 1")
# endif
#else
# define gsl_CONFIG_DEFAULTS_VERSION  gsl_lite_MAJOR  // default
#endif
#define  gsl_CONFIG_DEFAULTS_VERSION_()  gsl_CONFIG_DEFAULTS_VERSION

#if defined( gsl_CONFIG_ALLOWS_SPAN_CONTAINER_CTOR )
# if ! gsl_CHECK_CFG_TOGGLE_VALUE_( gsl_CONFIG_ALLOWS_SPAN_CONTAINER_CTOR )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_ALLOWS_SPAN_CONTAINER_CTOR=" gsl_STRINGIFY(gsl_CONFIG_ALLOWS_SPAN_CONTAINER_CTOR) ", must be 0 or 1")
# endif
# define gsl_CONFIG_ALLOWS_UNCONSTRAINED_SPAN_CONTAINER_CTOR  gsl_CONFIG_ALLOWS_SPAN_CONTAINER_CTOR
# pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: configuration macro gsl_CONFIG_ALLOWS_SPAN_CONTAINER_CTOR is deprecated since gsl-lite 0.7; if the constrained container constructor is not usable, define gsl_CONFIG_ALLOWS_UNCONSTRAINED_SPAN_CONTAINER_CTOR instead")
#endif

#if defined( gsl_CONFIG_CONTRACT_LEVEL_ON )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_CONTRACT_LEVEL_ON )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_CONTRACT_LEVEL_ON=" gsl_STRINGIFY(gsl_CONFIG_CONTRACT_LEVEL_ON) "; macro must be defined without value")
# endif
# pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: configuration macro gsl_CONFIG_CONTRACT_LEVEL_ON is deprecated since gsl-lite v0.36; define gsl_CONFIG_CONTRACT_CHECKING_ON instead")
# define gsl_CONFIG_CONTRACT_CHECKING_ON
#endif
#if defined( gsl_CONFIG_CONTRACT_LEVEL_OFF )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_CONTRACT_LEVEL_OFF )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_CONTRACT_LEVEL_OFF=" gsl_STRINGIFY(gsl_CONFIG_CONTRACT_LEVEL_OFF) "; macro must be defined without value")
# endif
# pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: configuration macro gsl_CONFIG_CONTRACT_LEVEL_OFF is deprecated since gsl-lite v0.36; define gsl_CONFIG_CONTRACT_CHECKING_OFF instead")
# define gsl_CONFIG_CONTRACT_CHECKING_OFF
#endif
#if   defined( gsl_CONFIG_CONTRACT_LEVEL_EXPECTS_ONLY )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_CONTRACT_LEVEL_EXPECTS_ONLY )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_CONTRACT_LEVEL_EXPECTS_ONLY=" gsl_STRINGIFY(gsl_CONFIG_CONTRACT_LEVEL_EXPECTS_ONLY) "; macro must be defined without value")
# endif
# pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: configuration macro gsl_CONFIG_CONTRACT_LEVEL_EXPECTS_ONLY is deprecated since gsl-lite v0.36; define gsl_CONFIG_CONTRACT_CHECKING_ENSURES_OFF and gsl_CONFIG_CONTRACT_CHECKING_ASSERT_OFF instead")
# define gsl_CONFIG_CONTRACT_CHECKING_ON
# define gsl_CONFIG_CONTRACT_CHECKING_ENSURES_OFF
# define gsl_CONFIG_CONTRACT_CHECKING_ASSERT_OFF
#elif defined( gsl_CONFIG_CONTRACT_LEVEL_ENSURES_ONLY )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_CONTRACT_LEVEL_ENSURES_ONLY )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_CONTRACT_LEVEL_ENSURES_ONLY=" gsl_STRINGIFY(gsl_CONFIG_CONTRACT_LEVEL_ENSURES_ONLY) "; macro must be defined without value")
# endif
# pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: configuration macro gsl_CONFIG_CONTRACT_LEVEL_ENSURES_ONLY is deprecated since gsl-lite v0.36; define gsl_CONFIG_CONTRACT_CHECKING_EXPECTS_OFF and gsl_CONFIG_CONTRACT_CHECKING_ASSERT_OFF instead")
# define gsl_CONFIG_CONTRACT_CHECKING_ON
# define gsl_CONFIG_CONTRACT_CHECKING_EXPECTS_OFF
# define gsl_CONFIG_CONTRACT_CHECKING_ASSERT_OFF
#endif

// M-GSL compatibility:

#if defined( GSL_THROW_ON_CONTRACT_VIOLATION )
# pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: do not use legacy Microsoft GSL configuration macro GSL_THROW_ON_CONTRACT_VIOLATION to configure gsl-lite; define gsl_CONFIG_CONTRACT_VIOLATION_THROWS instead")
# if ! gsl_CHECK_CFG_NO_VALUE_( GSL_THROW_ON_CONTRACT_VIOLATION )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value GSL_THROW_ON_CONTRACT_VIOLATION=" gsl_STRINGIFY(GSL_THROW_ON_CONTRACT_VIOLATION) "; macro must be defined without value")
# endif
# define gsl_CONFIG_CONTRACT_VIOLATION_THROWS
#endif

#if defined( GSL_TERMINATE_ON_CONTRACT_VIOLATION )
# pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: do not use legacy Microsoft GSL configuration macro GSL_TERMINATE_ON_CONTRACT_VIOLATION to configure gsl-lite; define gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES instead")
# if ! gsl_CHECK_CFG_NO_VALUE_( GSL_TERMINATE_ON_CONTRACT_VIOLATION )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value GSL_TERMINATE_ON_CONTRACT_VIOLATION=" gsl_STRINGIFY(GSL_TERMINATE_ON_CONTRACT_VIOLATION) "; macro must be defined without value")
# endif
# define gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES
#endif

#if defined( GSL_UNENFORCED_ON_CONTRACT_VIOLATION )
# pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: do not use legacy Microsoft GSL configuration macro GSL_UNENFORCED_ON_CONTRACT_VIOLATION to configure gsl-lite; define gsl_CONFIG_CONTRACT_CHECKING_OFF instead")
# if ! gsl_CHECK_CFG_NO_VALUE_( GSL_UNENFORCED_ON_CONTRACT_VIOLATION )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value GSL_UNENFORCED_ON_CONTRACT_VIOLATION=" gsl_STRINGIFY(GSL_UNENFORCED_ON_CONTRACT_VIOLATION) "; macro must be defined without value")
# endif
# define gsl_CONFIG_CONTRACT_CHECKING_OFF
#endif

// Configuration: Features

#if defined( gsl_FEATURE_GSL_COMPATIBILITY_MODE )
# if ! gsl_CHECK_CFG_TOGGLE_VALUE_( gsl_FEATURE_GSL_COMPATIBILITY_MODE )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_FEATURE_GSL_COMPATIBILITY_MODE=" gsl_STRINGIFY(gsl_FEATURE_OWNER_MACRO) ", must be 0 or 1")
# endif
#else
# define gsl_FEATURE_GSL_COMPATIBILITY_MODE  (gsl_CONFIG_DEFAULTS_VERSION == 0)  // default
#endif
#define  gsl_FEATURE_GSL_COMPATIBILITY_MODE_()  gsl_FEATURE_GSL_COMPATIBILITY_MODE

#if defined( gsl_FEATURE_WITH_CONTAINER_TO_STD )
# if ! gsl_CHECK_CFG_STD_VALUE_( gsl_FEATURE_WITH_CONTAINER_TO_STD )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_FEATURE_WITH_CONTAINER_TO_STD=" gsl_STRINGIFY(gsl_FEATURE_WITH_CONTAINER_TO_STD) ", must be 98, 3, 11, 14, 17, or 20")
# endif
#else
# if gsl_CONFIG_DEFAULTS_VERSION >= 1
#  define gsl_FEATURE_WITH_CONTAINER_TO_STD  0   // version-1 default
# else // gsl_CONFIG_DEFAULTS_VERSION == 0
#  define gsl_FEATURE_WITH_CONTAINER_TO_STD  99  // version-0 default
# endif // gsl_CONFIG_DEFAULTS_VERSION >= 1
#endif
#define gsl_FEATURE_WITH_CONTAINER_TO_STD_()  gsl_FEATURE_WITH_CONTAINER_TO_STD

#if defined( gsl_FEATURE_MAKE_SPAN_TO_STD )
# if ! gsl_CHECK_CFG_STD_VALUE_( gsl_FEATURE_MAKE_SPAN_TO_STD )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_FEATURE_MAKE_SPAN_TO_STD=" gsl_STRINGIFY(gsl_FEATURE_MAKE_SPAN_TO_STD) ", must be 98, 3, 11, 14, 17, or 20")
# endif
#else
# define gsl_FEATURE_MAKE_SPAN_TO_STD  99  // default
#endif
#define  gsl_FEATURE_MAKE_SPAN_TO_STD_()  gsl_FEATURE_MAKE_SPAN_TO_STD

#if defined( gsl_FEATURE_BYTE_SPAN_TO_STD )
# if ! gsl_CHECK_CFG_STD_VALUE_( gsl_FEATURE_BYTE_SPAN_TO_STD )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_FEATURE_BYTE_SPAN_TO_STD=" gsl_STRINGIFY(gsl_FEATURE_BYTE_SPAN_TO_STD) ", must be 98, 3, 11, 14, 17, or 20")
# endif
#else
# define gsl_FEATURE_BYTE_SPAN_TO_STD  99  // default
#endif
#define  gsl_FEATURE_BYTE_SPAN_TO_STD_()  gsl_FEATURE_BYTE_SPAN_TO_STD

#if defined( gsl_FEATURE_IMPLICIT_MACRO )
# if ! gsl_CHECK_CFG_TOGGLE_VALUE_( gsl_FEATURE_IMPLICIT_MACRO )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_FEATURE_IMPLICIT_MACRO=" gsl_STRINGIFY(gsl_FEATURE_IMPLICIT_MACRO) ", must be 0 or 1")
# endif
# if gsl_FEATURE_IMPLICIT_MACRO
#  error configuration value gsl_FEATURE_IMPLICIT_MACRO=1 is no longer supported since gsl-lite v1.0
# endif // gsl_FEATURE_IMPLICIT_MACRO
#else
# define gsl_FEATURE_IMPLICIT_MACRO  0
#endif
#define  gsl_FEATURE_IMPLICIT_MACRO_()  gsl_FEATURE_IMPLICIT_MACRO

#if defined( gsl_FEATURE_OWNER_MACRO )
# if ! gsl_CHECK_CFG_TOGGLE_VALUE_( gsl_FEATURE_OWNER_MACRO )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_FEATURE_OWNER_MACRO=" gsl_STRINGIFY(gsl_FEATURE_OWNER_MACRO) ", must be 0 or 1")
# endif
# if gsl_FEATURE_OWNER_MACRO
#  error configuration value gsl_FEATURE_OWNER_MACRO=1 is no longer supported since gsl-lite v1.0
# endif // gsl_FEATURE_OWNER_MACRO
#else
# define gsl_FEATURE_OWNER_MACRO  0
#endif
#define  gsl_FEATURE_OWNER_MACRO_()  gsl_FEATURE_OWNER_MACRO

#if defined( gsl_FEATURE_SPAN )
# if ! gsl_CHECK_CFG_TOGGLE_VALUE_( gsl_FEATURE_SPAN )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_FEATURE_SPAN=" gsl_STRINGIFY(gsl_FEATURE_SPAN) ", must be 0 or 1")
# endif
#else
# define gsl_FEATURE_SPAN  1  // default
#endif
#define  gsl_FEATURE_SPAN_()  gsl_FEATURE_SPAN

#if defined( gsl_FEATURE_STRING_SPAN )
# if ! gsl_CHECK_CFG_TOGGLE_VALUE_( gsl_FEATURE_STRING_SPAN )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_FEATURE_STRING_SPAN=" gsl_STRINGIFY(gsl_FEATURE_STRING_SPAN) ", must be 0 or 1")
# endif
# if gsl_FEATURE_STRING_SPAN && ! gsl_FEATURE_SPAN
#  error configuration value gsl_FEATURE_STRING_SPAN=1 is incompatible with gsl_FEATURE_SPAN=0
# endif
#elif gsl_FEATURE_SPAN
# define gsl_FEATURE_STRING_SPAN  (gsl_CONFIG_DEFAULTS_VERSION == 0)  // default
#else
# define gsl_FEATURE_STRING_SPAN  0  // gsl_FEATURE_SPAN == 0 implies gsl_FEATURE_STRING_SPAN == 0
#endif
#define  gsl_FEATURE_STRING_SPAN_()  gsl_FEATURE_STRING_SPAN

#if defined( gsl_FEATURE_BYTE )
# if ! gsl_CHECK_CFG_TOGGLE_VALUE_( gsl_FEATURE_BYTE )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_FEATURE_BYTE=" gsl_STRINGIFY(gsl_FEATURE_BYTE) ", must be 0 or 1")
# endif
#else
# define gsl_FEATURE_BYTE  (gsl_CONFIG_DEFAULTS_VERSION == 0)  // default
#endif
#define  gsl_FEATURE_BYTE_()  gsl_FEATURE_BYTE

#if defined( gsl_FEATURE_EXPERIMENTAL_RETURN_GUARD )
# if ! gsl_CHECK_CFG_TOGGLE_VALUE_( gsl_FEATURE_EXPERIMENTAL_RETURN_GUARD )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_FEATURE_EXPERIMENTAL_RETURN_GUARD=" gsl_STRINGIFY(gsl_FEATURE_EXPERIMENTAL_RETURN_GUARD) ", must be 0 or 1")
# endif
#else
# define gsl_FEATURE_EXPERIMENTAL_RETURN_GUARD  0 // default
#endif
#define  gsl_FEATURE_EXPERIMENTAL_RETURN_GUARD_()  gsl_FEATURE_EXPERIMENTAL_RETURN_GUARD

#if defined( gsl_FEATURE_GSL_LITE_NAMESPACE )
# if ! gsl_CHECK_CFG_TOGGLE_VALUE_( gsl_FEATURE_GSL_LITE_NAMESPACE )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_FEATURE_GSL_LITE_NAMESPACE=" gsl_STRINGIFY(gsl_FEATURE_GSL_LITE_NAMESPACE) ", must be 0 or 1")
# endif
# if ! gsl_FEATURE_GSL_LITE_NAMESPACE
#  error gsl_FEATURE_GSL_LITE_NAMESPACE is a required feature since gsl-lite v1.0 and cannot be switched off
# endif
#else
# define gsl_FEATURE_GSL_LITE_NAMESPACE  1
#endif
#define  gsl_FEATURE_GSL_LITE_NAMESPACE_()  gsl_FEATURE_GSL_LITE_NAMESPACE

// Configuration: Other

#if defined( gsl_CONFIG_TRANSPARENT_NOT_NULL )
# if ! gsl_CHECK_CFG_TOGGLE_VALUE_( gsl_CONFIG_TRANSPARENT_NOT_NULL )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_TRANSPARENT_NOT_NULL=" gsl_STRINGIFY(gsl_CONFIG_TRANSPARENT_NOT_NULL) ", must be 0 or 1")
# endif
# if gsl_CONFIG_TRANSPARENT_NOT_NULL && defined( gsl_CONFIG_NOT_NULL_GET_BY_CONST_REF )
#  error configuration option gsl_CONFIG_NOT_NULL_GET_BY_CONST_REF is meaningless if gsl_CONFIG_TRANSPARENT_NOT_NULL=1
# endif
#else
# define gsl_CONFIG_TRANSPARENT_NOT_NULL  (gsl_CONFIG_DEFAULTS_VERSION >= 1)  // default
#endif
#define  gsl_CONFIG_TRANSPARENT_NOT_NULL_()  gsl_CONFIG_TRANSPARENT_NOT_NULL

#if ! defined( gsl_CONFIG_DEPRECATE_TO_LEVEL )
# if gsl_CONFIG_DEFAULTS_VERSION >= 1
#  define gsl_CONFIG_DEPRECATE_TO_LEVEL  9
# else
#  define gsl_CONFIG_DEPRECATE_TO_LEVEL  0
# endif
#endif

#if defined( gsl_CONFIG_SPAN_INDEX_TYPE )
# if ! defined( gsl_CONFIG_ACKNOWLEDGE_NONSTANDARD_ABI )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: defining a custom gsl_CONFIG_SPAN_INDEX_TYPE changes the ABI of gsl-lite, which may lead to ODR violations and undefined behavior; define the macro gsl_CONFIG_ACKNOWLEDGE_NONSTANDARD_ABI to explicitly acknowledge that you are using gsl-lite with a non-standard ABI and that you control the build flags of all components linked into your target")
# endif
#else // ! defined( gsl_CONFIG_SPAN_INDEX_TYPE )
# define gsl_CONFIG_SPAN_INDEX_TYPE  std::size_t
#endif
#define  gsl_CONFIG_SPAN_INDEX_TYPE_()  gsl_CONFIG_SPAN_INDEX_TYPE

#if defined( gsl_CONFIG_INDEX_TYPE )
# if ! defined( gsl_CONFIG_ACKNOWLEDGE_NONSTANDARD_ABI )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: defining a custom gsl_CONFIG_INDEX_TYPE changes the ABI of gsl-lite, which may lead to ODR violations and undefined behavior; define the macro gsl_CONFIG_ACKNOWLEDGE_NONSTANDARD_ABI to explicitly acknowledge that you are using gsl-lite with a non-standard ABI and that you control the build flags of all components linked into your target")
# endif
#else // ! defined( gsl_CONFIG_INDEX_TYPE )
# if gsl_CONFIG_DEFAULTS_VERSION >= 1
// p0122r3 uses std::ptrdiff_t
#  define gsl_CONFIG_INDEX_TYPE  std::ptrdiff_t
# else
#  if ! defined( gsl_CONFIG_ACKNOWLEDGE_NONSTANDARD_ABI )
#   pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: with version-0 defaults, gsl_CONFIG_INDEX_TYPE defaults to gsl_CONFIG_SPAN_INDEX_TYPE, which changes the ABI of gsl-lite and may lead to ODR violations and undefined behavior; use version-1 defaults or define the macro gsl_CONFIG_ACKNOWLEDGE_NONSTANDARD_ABI to explicitly acknowledge that you are using gsl-lite with a non-standard ABI and that you control the build flags of all components linked into your target")
#  endif
#  define gsl_CONFIG_INDEX_TYPE  gsl_CONFIG_SPAN_INDEX_TYPE
# endif
#endif
#define   gsl_CONFIG_INDEX_TYPE_()  gsl_CONFIG_INDEX_TYPE

#if defined( gsl_CONFIG_NOT_NULL_EXPLICIT_CTOR )
# if ! gsl_CHECK_CFG_TOGGLE_VALUE_( gsl_CONFIG_NOT_NULL_EXPLICIT_CTOR )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_NOT_NULL_EXPLICIT_CTOR=" gsl_STRINGIFY(gsl_CONFIG_NOT_NULL_EXPLICIT_CTOR) ", must be 0 or 1")
# endif
#else
# define gsl_CONFIG_NOT_NULL_EXPLICIT_CTOR  (gsl_CONFIG_DEFAULTS_VERSION >= 1)  // default
#endif
#define  gsl_CONFIG_NOT_NULL_EXPLICIT_CTOR_()  gsl_CONFIG_NOT_NULL_EXPLICIT_CTOR

#if defined( gsl_CONFIG_NOT_NULL_GET_BY_CONST_REF )
# if ! gsl_CHECK_CFG_TOGGLE_VALUE_( gsl_CONFIG_NOT_NULL_GET_BY_CONST_REF )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_NOT_NULL_GET_BY_CONST_REF=" gsl_STRINGIFY(gsl_CONFIG_NOT_NULL_GET_BY_CONST_REF) ", must be 0 or 1")
# endif
#else
# define gsl_CONFIG_NOT_NULL_GET_BY_CONST_REF  0  // default
#endif
#define  gsl_CONFIG_NOT_NULL_GET_BY_CONST_REF_()  gsl_CONFIG_NOT_NULL_GET_BY_CONST_REF

#if defined( gsl_CONFIG_CONFIRMS_COMPILATION_ERRORS )
# if ! gsl_CHECK_CFG_TOGGLE_VALUE_( gsl_CONFIG_CONFIRMS_COMPILATION_ERRORS )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_CONFIRMS_COMPILATION_ERRORS=" gsl_STRINGIFY(gsl_CONFIG_CONFIRMS_COMPILATION_ERRORS) ", must be 0 or 1")
# endif
#else
# define gsl_CONFIG_CONFIRMS_COMPILATION_ERRORS  0  // default
#endif
#define  gsl_CONFIG_CONFIRMS_COMPILATION_ERRORS_()  gsl_CONFIG_CONFIRMS_COMPILATION_ERRORS

#if defined( gsl_CONFIG_ALLOWS_SPAN_COMPARISON )
# if ! gsl_CHECK_CFG_TOGGLE_VALUE_( gsl_CONFIG_ALLOWS_SPAN_COMPARISON )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_ALLOWS_SPAN_COMPARISON=" gsl_STRINGIFY(gsl_CONFIG_ALLOWS_SPAN_COMPARISON) ", must be 0 or 1")
# endif
#else
# define gsl_CONFIG_ALLOWS_SPAN_COMPARISON  (gsl_CONFIG_DEFAULTS_VERSION == 0)  // default
#endif
#define  gsl_CONFIG_ALLOWS_SPAN_COMPARISON_()  gsl_CONFIG_ALLOWS_SPAN_COMPARISON

#if defined( gsl_CONFIG_ALLOWS_NONSTRICT_SPAN_COMPARISON )
# if ! gsl_CHECK_CFG_TOGGLE_VALUE_( gsl_CONFIG_ALLOWS_NONSTRICT_SPAN_COMPARISON )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_ALLOWS_NONSTRICT_SPAN_COMPARISON=" gsl_STRINGIFY(gsl_CONFIG_ALLOWS_NONSTRICT_SPAN_COMPARISON) ", must be 0 or 1")
# endif
#else
# define gsl_CONFIG_ALLOWS_NONSTRICT_SPAN_COMPARISON  1  // default
#endif
#define  gsl_CONFIG_ALLOWS_NONSTRICT_SPAN_COMPARISON_()  gsl_CONFIG_ALLOWS_NONSTRICT_SPAN_COMPARISON

#if defined( gsl_CONFIG_ALLOWS_UNCONSTRAINED_SPAN_CONTAINER_CTOR )
# if ! gsl_CHECK_CFG_TOGGLE_VALUE_( gsl_CONFIG_ALLOWS_UNCONSTRAINED_SPAN_CONTAINER_CTOR )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_ALLOWS_UNCONSTRAINED_SPAN_CONTAINER_CTOR=" gsl_STRINGIFY(gsl_CONFIG_ALLOWS_UNCONSTRAINED_SPAN_CONTAINER_CTOR) ", must be 0 or 1")
# endif
#else
# define gsl_CONFIG_ALLOWS_UNCONSTRAINED_SPAN_CONTAINER_CTOR  0  // default
#endif
#define  gsl_CONFIG_ALLOWS_UNCONSTRAINED_SPAN_CONTAINER_CTOR_()  gsl_CONFIG_ALLOWS_UNCONSTRAINED_SPAN_CONTAINER_CTOR

#if defined( gsl_CONFIG_NARROW_THROWS_ON_TRUNCATION )
# if ! gsl_CHECK_CFG_TOGGLE_VALUE_( gsl_CONFIG_NARROW_THROWS_ON_TRUNCATION )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_NARROW_THROWS_ON_TRUNCATION=" gsl_STRINGIFY(gsl_CONFIG_NARROW_THROWS_ON_TRUNCATION) ", must be 0 or 1")
# endif
# if ! gsl_CONFIG_NARROW_THROWS_ON_TRUNCATION && ! defined( gsl_CONFIG_ACKNOWLEDGE_NONSTANDARD_ABI )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: the configuration value gsl_CONFIG_NARROW_THROWS_ON_TRUNCATION=0 changes the ABI of gsl-lite, which may lead to ODR violations and undefined behavior; define the macro gsl_CONFIG_ACKNOWLEDGE_NONSTANDARD_ABI to explicitly acknowledge that you are using gsl-lite with a non-standard ABI and that you control the build flags of all components linked into your target")
# endif
#else
# define gsl_CONFIG_NARROW_THROWS_ON_TRUNCATION  (gsl_CONFIG_DEFAULTS_VERSION >= 1)  // default
# if gsl_CONFIG_DEFAULTS_VERSION < 1 && ! defined( gsl_CONFIG_ACKNOWLEDGE_NONSTANDARD_ABI )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: with version-0 defaults, gsl_CONFIG_NARROW_THROWS_ON_TRUNCATION defaults to 0, which changes the ABI of gsl-lite and may lead to ODR violations and undefined behavior; use version-1 defaults or define the macro gsl_CONFIG_ACKNOWLEDGE_NONSTANDARD_ABI to explicitly acknowledge that you are using gsl-lite with a non-standard ABI and that you control the build flags of all components linked into your target")
# endif
#endif
#define  gsl_CONFIG_NARROW_THROWS_ON_TRUNCATION_()  gsl_CONFIG_NARROW_THROWS_ON_TRUNCATION

#if defined( gsl_CONFIG_VALIDATES_UNENFORCED_CONTRACT_EXPRESSIONS )
# if ! gsl_CHECK_CFG_TOGGLE_VALUE_( gsl_CONFIG_VALIDATES_UNENFORCED_CONTRACT_EXPRESSIONS )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_VALIDATES_UNENFORCED_CONTRACT_EXPRESSIONS=" gsl_STRINGIFY(gsl_CONFIG_VALIDATES_UNENFORCED_CONTRACT_EXPRESSIONS) ", must be 0 or 1")
# endif
#else
# define gsl_CONFIG_VALIDATES_UNENFORCED_CONTRACT_EXPRESSIONS  1  // default
#endif
#define  gsl_CONFIG_VALIDATES_UNENFORCED_CONTRACT_EXPRESSIONS_()  gsl_CONFIG_VALIDATES_UNENFORCED_CONTRACT_EXPRESSIONS

#if defined( gsl_CONFIG_USE_CRT_ASSERTION_HANDLER )
# if ! gsl_CHECK_CFG_TOGGLE_VALUE_( gsl_CONFIG_USE_CRT_ASSERTION_HANDLER )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_USE_CRT_ASSERTION_HANDLER=" gsl_STRINGIFY(gsl_CONFIG_USE_CRT_ASSERTION_HANDLER) ", must be 0 or 1")
# endif
#else
// gsl_CONFIG_USE_CRT_ASSERTION_HANDLER=1: Call the CRT assertion handler (`_assert()` for MSVC, `__assert_fail()` for Linux, `__assert()` otherwise).
// gsl_CONFIG_USE_CRT_ASSERTION_HANDLER=0: Use the `assert()` macro if available, or call `abort()` directly if not.
// We default to 1 if we can be sure that the CRT assertion handler is accessible, and 0 otherwise.
# if defined( _MSC_VER ) || defined( __linux__ )
#  define gsl_CONFIG_USE_CRT_ASSERTION_HANDLER  1
# else
#  define gsl_CONFIG_USE_CRT_ASSERTION_HANDLER  0
# endif
#endif
#define  gsl_CONFIG_USE_CRT_ASSERTION_HANDLER_()  gsl_CONFIG_USE_CRT_ASSERTION_HANDLER

#if defined( gsl_CONFIG_CONTRACT_CHECKING_EXPECTS_OFF )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_CONTRACT_CHECKING_EXPECTS_OFF )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_CONTRACT_CHECKING_EXPECTS_OFF=" gsl_STRINGIFY(gsl_CONFIG_CONTRACT_CHECKING_EXPECTS_OFF) "; macro must be defined without value")
# endif
#endif
#if defined( gsl_CONFIG_CONTRACT_CHECKING_ENSURES_OFF )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_CONTRACT_CHECKING_ENSURES_OFF )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_CONTRACT_CHECKING_ENSURES_OFF=" gsl_STRINGIFY(gsl_CONFIG_CONTRACT_CHECKING_ENSURES_OFF) "; macro must be defined without value")
# endif
#endif
#if defined( gsl_CONFIG_CONTRACT_CHECKING_ASSERT_OFF )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_CONTRACT_CHECKING_ASSERT_OFF )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_CONTRACT_CHECKING_ASSERT_OFF=" gsl_STRINGIFY(gsl_CONFIG_CONTRACT_CHECKING_ASSERT_OFF) "; macro must be defined without value")
# endif
#endif
#if defined( gsl_CONFIG_CONTRACT_CHECKING_AUDIT )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_CONTRACT_CHECKING_AUDIT )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_CONTRACT_CHECKING_AUDIT=" gsl_STRINGIFY(gsl_CONFIG_CONTRACT_CHECKING_AUDIT) "; macro must be defined without value")
# endif
#endif
#if defined( gsl_CONFIG_CONTRACT_CHECKING_ON )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_CONTRACT_CHECKING_ON )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_CONTRACT_CHECKING_ON=" gsl_STRINGIFY(gsl_CONFIG_CONTRACT_CHECKING_ON) "; macro must be defined without value")
# endif
#endif
#if defined( gsl_CONFIG_CONTRACT_CHECKING_OFF )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_CONTRACT_CHECKING_OFF )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_CONTRACT_CHECKING_OFF=" gsl_STRINGIFY(gsl_CONFIG_CONTRACT_CHECKING_OFF) "; macro must be defined without value")
# endif
#endif
#if defined( gsl_CONFIG_DEVICE_CONTRACT_CHECKING_AUDIT )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_DEVICE_CONTRACT_CHECKING_AUDIT )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_DEVICE_CONTRACT_CHECKING_AUDIT=" gsl_STRINGIFY(gsl_CONFIG_DEVICE_CONTRACT_CHECKING_AUDIT) "; macro must be defined without value")
# endif
#endif
#if defined( gsl_CONFIG_DEVICE_CONTRACT_CHECKING_ON )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_DEVICE_CONTRACT_CHECKING_ON )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_DEVICE_CONTRACT_CHECKING_ON=" gsl_STRINGIFY(gsl_CONFIG_DEVICE_CONTRACT_CHECKING_ON) "; macro must be defined without value")
# endif
#endif
#if defined( gsl_CONFIG_DEVICE_CONTRACT_CHECKING_OFF )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_DEVICE_CONTRACT_CHECKING_OFF )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_DEVICE_CONTRACT_CHECKING_OFF=" gsl_STRINGIFY(gsl_CONFIG_DEVICE_CONTRACT_CHECKING_OFF) "; macro must be defined without value")
# endif
#endif
#if defined( gsl_CONFIG_CONTRACT_VIOLATION_THROWS )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_CONTRACT_VIOLATION_THROWS )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_CONTRACT_VIOLATION_THROWS=" gsl_STRINGIFY(gsl_CONFIG_CONTRACT_VIOLATION_THROWS) "; macro must be defined without value")
# endif
#endif
#if defined( gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES=" gsl_STRINGIFY(gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES) "; macro must be defined without value")
# endif
#endif
#if defined( gsl_CONFIG_CONTRACT_VIOLATION_ASSERTS )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_CONTRACT_VIOLATION_ASSERTS )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_CONTRACT_VIOLATION_ASSERTS=" gsl_STRINGIFY(gsl_CONFIG_CONTRACT_VIOLATION_ASSERTS) "; macro must be defined without value")
# endif
#endif
#if defined( gsl_CONFIG_CONTRACT_VIOLATION_TRAPS )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_CONTRACT_VIOLATION_TRAPS )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_CONTRACT_VIOLATION_TRAPS=" gsl_STRINGIFY(gsl_CONFIG_CONTRACT_VIOLATION_TRAPS) "; macro must be defined without value")
# endif
#endif
#if defined( gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES_WITH_STACKTRACE )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES_WITH_STACKTRACE )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES_WITH_STACKTRACE=" gsl_STRINGIFY(gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES_WITH_STACKTRACE) "; macro must be defined without value")
# endif
#endif
#if defined( gsl_CONFIG_CONTRACT_VIOLATION_CALLS_HANDLER )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_CONTRACT_VIOLATION_CALLS_HANDLER )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_CONTRACT_VIOLATION_CALLS_HANDLER=" gsl_STRINGIFY(gsl_CONFIG_CONTRACT_VIOLATION_CALLS_HANDLER) "; macro must be defined without value")
# endif
#endif
#if defined( gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_ASSERTS )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_ASSERTS )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_ASSERTS=" gsl_STRINGIFY(gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_ASSERTS) "; macro must be defined without value")
# endif
#endif
#if defined( gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_TRAPS )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_TRAPS )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_TRAPS=" gsl_STRINGIFY(gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_TRAPS) "; macro must be defined without value")
# endif
#endif
#if defined( gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_CALLS_HANDLER )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_CALLS_HANDLER )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_CALLS_HANDLER=" gsl_STRINGIFY(gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_CALLS_HANDLER) "; macro must be defined without value")
# endif
#endif
#if defined( gsl_CONFIG_UNENFORCED_CONTRACTS_ASSUME )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_UNENFORCED_CONTRACTS_ASSUME )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_UNENFORCED_CONTRACTS_ASSUME=" gsl_STRINGIFY(gsl_CONFIG_UNENFORCED_CONTRACTS_ASSUME) "; macro must be defined without value")
# endif
#endif
#if defined( gsl_CONFIG_UNENFORCED_CONTRACTS_ELIDE )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_UNENFORCED_CONTRACTS_ELIDE )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_UNENFORCED_CONTRACTS_ELIDE=" gsl_STRINGIFY(gsl_CONFIG_UNENFORCED_CONTRACTS_ELIDE) "; macro must be defined without value")
# endif
#endif
#if defined( gsl_CONFIG_DEVICE_UNENFORCED_CONTRACTS_ASSUME )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_DEVICE_UNENFORCED_CONTRACTS_ASSUME )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_DEVICE_UNENFORCED_CONTRACTS_ASSUME=" gsl_STRINGIFY(gsl_CONFIG_DEVICE_UNENFORCED_CONTRACTS_ASSUME) "; macro must be defined without value")
# endif
#endif
#if defined( gsl_CONFIG_DEVICE_UNENFORCED_CONTRACTS_ELIDE )
# if ! gsl_CHECK_CFG_NO_VALUE_( gsl_CONFIG_DEVICE_UNENFORCED_CONTRACTS_ELIDE )
#  pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: invalid configuration value gsl_CONFIG_DEVICE_UNENFORCED_CONTRACTS_ELIDE=" gsl_STRINGIFY(gsl_CONFIG_DEVICE_UNENFORCED_CONTRACTS_ELIDE) "; macro must be defined without value")
# endif
#endif

#if defined( gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_THROWS )
# error cannot use gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_THROWS because exceptions are not supported in device code; use gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_ASSERTS or gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_TRAPS
#endif
#if defined( gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_TERMINATES )
# error gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_TERMINATES is not supported; use gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_ASSERTS or gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_TRAPS
#endif

#if 1 < defined( gsl_CONFIG_CONTRACT_CHECKING_AUDIT ) + defined( gsl_CONFIG_CONTRACT_CHECKING_ON ) + defined( gsl_CONFIG_CONTRACT_CHECKING_OFF )
# error only one of gsl_CONFIG_CONTRACT_CHECKING_AUDIT, gsl_CONFIG_CONTRACT_CHECKING_ON, and gsl_CONFIG_CONTRACT_CHECKING_OFF may be defined
#endif
#if 1 < defined( gsl_CONFIG_DEVICE_CONTRACT_CHECKING_AUDIT ) + defined( gsl_CONFIG_DEVICE_CONTRACT_CHECKING_ON ) + defined( gsl_CONFIG_DEVICE_CONTRACT_CHECKING_OFF )
# error only one of gsl_CONFIG_DEVICE_CONTRACT_CHECKING_AUDIT, gsl_CONFIG_DEVICE_CONTRACT_CHECKING_ON, and gsl_CONFIG_DEVICE_CONTRACT_CHECKING_OFF may be defined
#endif
#if 1 < defined( gsl_CONFIG_CONTRACT_VIOLATION_THROWS ) + defined( gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES ) + defined( gsl_CONFIG_CONTRACT_VIOLATION_ASSERTS ) + defined( gsl_CONFIG_CONTRACT_VIOLATION_TRAPS ) + defined( gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES_WITH_STACKTRACE ) + defined( gsl_CONFIG_CONTRACT_VIOLATION_CALLS_HANDLER )
# error only one of gsl_CONFIG_CONTRACT_VIOLATION_THROWS, gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES, gsl_CONFIG_CONTRACT_VIOLATION_ASSERTS, gsl_CONFIG_CONTRACT_VIOLATION_TRAPS, gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES_WITH_STACKTRACE, and gsl_CONFIG_CONTRACT_VIOLATION_CALLS_HANDLER may be defined
#endif
#if 1 < defined( gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_ASSERTS ) + defined( gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_TRAPS ) + defined( gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_CALLS_HANDLER )
# error only one of gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_ASSERTS, gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_TRAPS, and gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_CALLS_HANDLER may be defined
#endif
#if 1 < defined( gsl_CONFIG_UNENFORCED_CONTRACTS_ASSUME ) + defined( gsl_CONFIG_UNENFORCED_CONTRACTS_ELIDE )
# error only one of gsl_CONFIG_UNENFORCED_CONTRACTS_ASSUME and gsl_CONFIG_UNENFORCED_CONTRACTS_ELIDE may be defined
#endif
#if 1 < defined( gsl_CONFIG_DEVICE_UNENFORCED_CONTRACTS_ASSUME ) + defined( gsl_CONFIG_DEVICE_UNENFORCED_CONTRACTS_ELIDE )
# error only one of gsl_CONFIG_DEVICE_UNENFORCED_CONTRACTS_ASSUME and gsl_CONFIG_UNENFORCED_DEVICE_CONTRACTS_ELIDE may be defined
#endif

#if 0 == defined( gsl_CONFIG_CONTRACT_CHECKING_AUDIT ) + defined( gsl_CONFIG_CONTRACT_CHECKING_ON ) + defined( gsl_CONFIG_CONTRACT_CHECKING_OFF )
// select default
# define gsl_CONFIG_CONTRACT_CHECKING_ON
#endif
#if 0 == defined( gsl_CONFIG_DEVICE_CONTRACT_CHECKING_AUDIT ) + defined( gsl_CONFIG_DEVICE_CONTRACT_CHECKING_ON ) + defined( gsl_CONFIG_DEVICE_CONTRACT_CHECKING_OFF )
// select default
# if defined( gsl_CONFIG_CONTRACT_CHECKING_AUDIT )
#  define gsl_CONFIG_DEVICE_CONTRACT_CHECKING_AUDIT
# elif defined( gsl_CONFIG_CONTRACT_CHECKING_OFF )
#  define gsl_CONFIG_DEVICE_CONTRACT_CHECKING_OFF
# else
#  define gsl_CONFIG_DEVICE_CONTRACT_CHECKING_ON
# endif
#endif
#if 0 == defined( gsl_CONFIG_CONTRACT_VIOLATION_THROWS ) + defined( gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES ) + defined( gsl_CONFIG_CONTRACT_VIOLATION_ASSERTS ) + defined( gsl_CONFIG_CONTRACT_VIOLATION_TRAPS ) + defined( gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES_WITH_STACKTRACE ) + defined( gsl_CONFIG_CONTRACT_VIOLATION_CALLS_HANDLER )
// select default
# if gsl_CONFIG_DEFAULTS_VERSION >= 1
#  define gsl_CONFIG_CONTRACT_VIOLATION_ASSERTS     // version-1 default
# else // gsl_CONFIG_DEFAULTS_VERSION == 0
#  define gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES  // version-0 default
# endif // gsl_CONFIG_DEFAULTS_VERSION >= 1
#endif
#if 0 == defined( gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_ASSERTS ) + defined( gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_TRAPS ) + defined( gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_CALLS_HANDLER )
// select default
# if defined( gsl_CONFIG_CONTRACT_VIOLATION_CALLS_HANDLER )
#  define gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_CALLS_HANDLER
# elif defined( gsl_CONFIG_CONTRACT_VIOLATION_TRAPS )
#  define gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_TRAPS
# else
#  define gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_ASSERTS
# endif
#endif
#if 0 == defined( gsl_CONFIG_UNENFORCED_CONTRACTS_ASSUME ) + defined( gsl_CONFIG_UNENFORCED_CONTRACTS_ELIDE )
// select default
# define gsl_CONFIG_UNENFORCED_CONTRACTS_ELIDE
#endif
#if 0 == defined( gsl_CONFIG_DEVICE_UNENFORCED_CONTRACTS_ASSUME ) + defined( gsl_CONFIG_DEVICE_UNENFORCED_CONTRACTS_ELIDE )
// select default
# if defined( gsl_CONFIG_UNENFORCED_CONTRACTS_ASSUME )
#  define gsl_CONFIG_DEVICE_UNENFORCED_CONTRACTS_ASSUME
# else
#  define gsl_CONFIG_DEVICE_UNENFORCED_CONTRACTS_ELIDE
# endif
#endif

// C++ language version detection:
// Note: VC14.0/1900 (VS2015) lacks too much from C++14.

#ifndef   gsl_CPLUSPLUS
# if defined(_MSVC_LANG ) && !defined(__clang__)
#  define gsl_CPLUSPLUS  ( _MSC_VER == 1900 ? 201103L : _MSVC_LANG )
# else
#  define gsl_CPLUSPLUS  __cplusplus
# endif
#endif

// C++ standard library version:

#ifndef  gsl_CPLUSPLUS_STDLIB
# define gsl_CPLUSPLUS_STDLIB  gsl_CPLUSPLUS
#endif

#define gsl_CPP98_OR_GREATER  ( gsl_CPLUSPLUS >= 199711L )
#define gsl_CPP11_OR_GREATER  ( gsl_CPLUSPLUS >= 201103L )
#define gsl_CPP14_OR_GREATER  ( gsl_CPLUSPLUS >= 201402L )
#define gsl_CPP17_OR_GREATER  ( gsl_CPLUSPLUS >= 201703L )
#define gsl_CPP20_OR_GREATER  ( gsl_CPLUSPLUS >= 202002L )
#define gsl_CPP23_OR_GREATER  ( gsl_CPLUSPLUS >= 202302L )
#define gsl_CPP26_OR_GREATER  ( gsl_CPLUSPLUS >  202302L )  // not finalized yet

// C++ language version (represent 98 as 3):

#define gsl_CPLUSPLUS_V  ( gsl_CPLUSPLUS / 100 - (gsl_CPLUSPLUS > 200000 ? 2000 : 1994) )

// half-open range [lo..hi):
#define gsl_BETWEEN( v, lo, hi ) ( (lo) <= (v) && (v) < (hi) )

// Compiler versions:

// MSVC++  6.0  _MSC_VER == 1200  gsl_COMPILER_MSVC_VERSION ==  60  (Visual Studio 6.0)
// MSVC++  7.0  _MSC_VER == 1300  gsl_COMPILER_MSVC_VERSION ==  70  (Visual Studio .NET 2002)
// MSVC++  7.1  _MSC_VER == 1310  gsl_COMPILER_MSVC_VERSION ==  71  (Visual Studio .NET 2003)
// MSVC++  8.0  _MSC_VER == 1400  gsl_COMPILER_MSVC_VERSION ==  80  (Visual Studio 2005)
// MSVC++  9.0  _MSC_VER == 1500  gsl_COMPILER_MSVC_VERSION ==  90  (Visual Studio 2008)
// MSVC++ 10.0  _MSC_VER == 1600  gsl_COMPILER_MSVC_VERSION == 100  (Visual Studio 2010)
// MSVC++ 11.0  _MSC_VER == 1700  gsl_COMPILER_MSVC_VERSION == 110  (Visual Studio 2012)
// MSVC++ 12.0  _MSC_VER == 1800  gsl_COMPILER_MSVC_VERSION == 120  (Visual Studio 2013)
// MSVC++ 14.0  _MSC_VER == 1900  gsl_COMPILER_MSVC_VERSION == 140  (Visual Studio 2015)
// MSVC++ 14.1  _MSC_VER >= 1910  gsl_COMPILER_MSVC_VERSION == 141  (Visual Studio 2017)
// MSVC++ 14.2  _MSC_VER >= 1920  gsl_COMPILER_MSVC_VERSION == 142  (Visual Studio 2019)
// MSVC++ 14.3  _MSC_VER >= 1930  gsl_COMPILER_MSVC_VERSION == 143  (Visual Studio 2022)
// MSVC++ 14.5  _MSC_VER >= 1950  gsl_COMPILER_MSVC_VERSION == 145  (Visual Studio 2026)

#if defined( _MSC_VER )
# define gsl_COMPILER_MS_STL_VER           (_MSC_VER )
# define gsl_COMPILER_MS_STL_VERSION       (_MSC_VER / 10 - 10 * ( 5 + (_MSC_VER < 1900 ) ) )
# define gsl_COMPILER_MS_STL_VERSION_FULL  (_MSC_VER - 100 * ( 5 + (_MSC_VER < 1900 ) ) )
#else
# define gsl_COMPILER_MS_STL_VER           0
# define gsl_COMPILER_MS_STL_VERSION       0
# define gsl_COMPILER_MS_STL_VERSION_FULL  0
#endif

#if defined( _MSC_VER ) && ! defined( __clang__ )
# define gsl_COMPILER_MSVC_VER           gsl_COMPILER_MS_STL_VER
# define gsl_COMPILER_MSVC_VERSION       gsl_COMPILER_MS_STL_VERSION
# define gsl_COMPILER_MSVC_VERSION_FULL  gsl_COMPILER_MS_STL_VERSION_FULL
#else
# define gsl_COMPILER_MSVC_VER           0
# define gsl_COMPILER_MSVC_VERSION       0
# define gsl_COMPILER_MSVC_VERSION_FULL  0
#endif

#define gsl_COMPILER_VERSION( major, minor, patch ) ( 10 * ( 10 * (major) + (minor) ) + (patch) )

// AppleClang  7.0.0  __apple_build_version__ ==  7000172  gsl_COMPILER_APPLECLANG_VERSION ==  700  (Xcode 7.0, 7.0.1)               (LLVM  3.7.0)
// AppleClang  7.0.0  __apple_build_version__ ==  7000176  gsl_COMPILER_APPLECLANG_VERSION ==  700  (Xcode 7.1)                      (LLVM  3.7.0)
// AppleClang  7.0.2  __apple_build_version__ ==  7000181  gsl_COMPILER_APPLECLANG_VERSION ==  702  (Xcode 7.2, 7.2.1)               (LLVM  3.7.0)
// AppleClang  7.3.0  __apple_build_version__ ==  7030029  gsl_COMPILER_APPLECLANG_VERSION ==  730  (Xcode 7.3)                      (LLVM  3.8.0)
// AppleClang  7.3.0  __apple_build_version__ ==  7030031  gsl_COMPILER_APPLECLANG_VERSION ==  730  (Xcode 7.3.1)                    (LLVM  3.8.0)
// AppleClang  8.0.0  __apple_build_version__ ==  8000038  gsl_COMPILER_APPLECLANG_VERSION ==  800  (Xcode 8.0)                      (LLVM  3.9.0)
// AppleClang  8.0.0  __apple_build_version__ ==  8000042  gsl_COMPILER_APPLECLANG_VERSION ==  800  (Xcode 8.1, 8.2, 8.2.1)          (LLVM  3.9.0)
// AppleClang  8.1.0  __apple_build_version__ ==  8020038  gsl_COMPILER_APPLECLANG_VERSION ==  810  (Xcode 8.3)                      (LLVM  3.9.0)
// AppleClang  8.1.0  __apple_build_version__ ==  8020041  gsl_COMPILER_APPLECLANG_VERSION ==  810  (Xcode 8.3.1)                    (LLVM  3.9.0)
// AppleClang  8.1.0  __apple_build_version__ ==  8020042  gsl_COMPILER_APPLECLANG_VERSION ==  810  (Xcode 8.3.2, 8.3.3)             (LLVM  3.9.0)
// AppleClang  9.0.0  __apple_build_version__ ==  9000037  gsl_COMPILER_APPLECLANG_VERSION ==  900  (Xcode 9.0)                      (LLVM  4.0.0)
// AppleClang  9.0.0  __apple_build_version__ ==  9000038  gsl_COMPILER_APPLECLANG_VERSION ==  900  (Xcode 9.1)                      (LLVM  4.0.0)
// AppleClang  9.0.0  __apple_build_version__ ==  9000039  gsl_COMPILER_APPLECLANG_VERSION ==  900  (Xcode 9.2)                      (LLVM  4.0.0)
// AppleClang  9.1.0  __apple_build_version__ ==  9020039  gsl_COMPILER_APPLECLANG_VERSION ==  910  (Xcode 9.3, 9.3.1)               (LLVM  5.0.2)
// AppleClang  9.1.0  __apple_build_version__ ==  9020039  gsl_COMPILER_APPLECLANG_VERSION ==  910  (Xcode 9.4, 9.4.1)               (LLVM  5.0.2)
// AppleClang 10.0.0  __apple_build_version__ == 10001145  gsl_COMPILER_APPLECLANG_VERSION == 1000  (Xcode 10.0, 10.1)               (LLVM  6.0.1)
// AppleClang 10.0.1  __apple_build_version__ == 10010046  gsl_COMPILER_APPLECLANG_VERSION == 1001  (Xcode 10.2, 10.2.1, 10.3)       (LLVM  7.0.0)
// AppleClang 11.0.0  __apple_build_version__ == 11000033  gsl_COMPILER_APPLECLANG_VERSION == 1100  (Xcode 11.1, 11.2, 11.3, 11.3.1) (LLVM  8.0.0)
// AppleClang 11.0.3  __apple_build_version__ == 11030032  gsl_COMPILER_APPLECLANG_VERSION == 1103  (Xcode 11.4, 11.4.1, 11.5, 11.6) (LLVM  9.0.0)
// AppleClang 12.0.0  __apple_build_version__ == 12000032  gsl_COMPILER_APPLECLANG_VERSION == 1200  (Xcode 12.0–12.4)                (LLVM 10.0.0)
// AppleClang 12.0.5  __apple_build_version__ == 12050022  gsl_COMPILER_APPLECLANG_VERSION == 1205  (Xcode 12.5)                     (LLVM 11.1.0)
// AppleClang 13.0.0  __apple_build_version__ == 13000029  gsl_COMPILER_APPLECLANG_VERSION == 1300  (Xcode 13.0–13.2.1)              (LLVM 12.0.0)
// AppleClang 13.1.6  __apple_build_version__ == 13160021  gsl_COMPILER_APPLECLANG_VERSION == 1316  (Xcode 13.3–13.4.1)              (LLVM 13.0.0)
// AppleClang 14.0.0  __apple_build_version__ == 14000029  gsl_COMPILER_APPLECLANG_VERSION == 1400  (Xcode 14.0–14.2)                (LLVM 14.0.0)
// AppleClang 14.0.3  __apple_build_version__ == 14030022  gsl_COMPILER_APPLECLANG_VERSION == 1403  (Xcode 14.3)                     (LLVM 15.0.0)
// AppleClang 15.0.0  __apple_build_version__ == 15000040  gsl_COMPILER_APPLECLANG_VERSION == 1500  (Xcode 15.0)                     (LLVM 16.0.0)
// AppleClang 15.0.0  __apple_build_version__ == 15000100  gsl_COMPILER_APPLECLANG_VERSION == 1500  (Xcode 15.1–15.2)                (LLVM 16.0.0)
// AppleClang 15.0.0  __apple_build_version__ == 15000309  gsl_COMPILER_APPLECLANG_VERSION == 1500  (Xcode 15.3–15.4)                (LLVM 16.0.0)
// AppleClang 16.0.0  __apple_build_version__ == 16000026  gsl_COMPILER_APPLECLANG_VERSION == 1600  (Xcode 16.0–16.2)                (LLVM 17.0.6)
// AppleClang 17.0.0  __apple_build_version__ == 17000013  gsl_COMPILER_APPLECLANG_VERSION == 1700  (Xcode 16.3–16.4)                (LLVM 19.1.4)
// AppleClang 17.0.0  __apple_build_version__ == 17000319  gsl_COMPILER_APPLECLANG_VERSION == 1700  (Xcode 26.0)                     (LLVM 19.1.5)
// AppleClang 17.0.0  __apple_build_version__ == 17000404  gsl_COMPILER_APPLECLANG_VERSION == 1700  (Xcode 26.1)                     (LLVM 19.1.5)
// AppleClang 17.0.0  __apple_build_version__ == 17000603  gsl_COMPILER_APPLECLANG_VERSION == 1700  (Xcode 26.2)                     (LLVM 19.1.5)

#if defined( __apple_build_version__ )
# define gsl_COMPILER_APPLECLANG_VERSION  gsl_COMPILER_VERSION( __clang_major__, __clang_minor__, __clang_patchlevel__ )
# define gsl_COMPILER_CLANG_VERSION       0
#elif defined( __clang__ )
# define gsl_COMPILER_APPLECLANG_VERSION  0
# define gsl_COMPILER_CLANG_VERSION       gsl_COMPILER_VERSION( __clang_major__, __clang_minor__, __clang_patchlevel__ )
#else
# define gsl_COMPILER_APPLECLANG_VERSION  0
# define gsl_COMPILER_CLANG_VERSION       0
#endif

#if defined( __GNUC__ ) && ! defined( __clang__ ) && ! defined( __NVCOMPILER )
# define gsl_COMPILER_GNUC_VERSION  gsl_COMPILER_VERSION( __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__ )
#else
# define gsl_COMPILER_GNUC_VERSION  0
#endif

#if defined( __NVCC__ )
# define gsl_COMPILER_NVCC_VERSION  ( __CUDACC_VER_MAJOR__ * 10 + __CUDACC_VER_MINOR__ )
#else
# define gsl_COMPILER_NVCC_VERSION  0
#endif

// NVHPC 21.2  gsl_COMPILER_NVHPC_VERSION == 2120
#if defined( __NVCOMPILER )
# define gsl_COMPILER_NVHPC_VERSION  gsl_COMPILER_VERSION( __NVCOMPILER_MAJOR__, __NVCOMPILER_MINOR__, __NVCOMPILER_PATCHLEVEL__ )
#else
# define gsl_COMPILER_NVHPC_VERSION  0
#endif

#if defined( __ARMCC_VERSION )
# define gsl_COMPILER_ARMCC_VERSION       ( __ARMCC_VERSION / 10000 )
# define gsl_COMPILER_ARMCC_VERSION_FULL  __ARMCC_VERSION
#else
# define gsl_COMPILER_ARMCC_VERSION       0
# define gsl_COMPILER_ARMCC_VERSION_FULL  0
#endif


// Compiler non-strict aliasing:

#if defined(__clang__) || defined(__GNUC__)
# define gsl_may_alias  __attribute__((__may_alias__))
#else
# define gsl_may_alias
#endif

// Presence of gsl, language and library features:

#define gsl_IN_STD( v )  ( ((v) == 98 ? 3 : (v)) >= gsl_CPLUSPLUS_V )

#define gsl_DEPRECATE_TO_LEVEL( level )  ( level <= gsl_CONFIG_DEPRECATE_TO_LEVEL )
#define gsl_FEATURE_TO_STD(   feature )  gsl_IN_STD( gsl_FEATURE( feature##_TO_STD ) )
#define gsl_FEATURE(          feature )  gsl_EVALF_( gsl_FEATURE_##feature##_ )
#define gsl_CONFIG(           feature )  gsl_EVALF_( gsl_CONFIG_##feature##_ )
#define gsl_HAVE(             feature )  gsl_EVALF_( gsl_HAVE_##feature##_ )

// Presence of wide character support:

#if defined(__DJGPP__) || (defined(_LIBCPP_VERSION) && defined(_LIBCPP_HAS_NO_WIDE_CHARACTERS))
# define gsl_HAVE_WCHAR  0
#else
# define gsl_HAVE_WCHAR  1
#endif
#define  gsl_HAVE_WCHAR_()  gsl_HAVE_WCHAR

// Compiling device code:

#if defined( __CUDACC__ ) && defined( __CUDA_ARCH__ )
# define gsl_DEVICE_CODE  1
#else
# define gsl_DEVICE_CODE  0
#endif


// Presence of language & library features:

#if gsl_COMPILER_CLANG_VERSION || gsl_COMPILER_APPLECLANG_VERSION
# ifdef __OBJC__
   // There are a bunch of inconsistencies about __EXCEPTIONS and __has_feature(cxx_exceptions) in Clang 3.4/3.5/3.6.
   // We're interested in C++ exceptions, which can be checked by __has_feature(cxx_exceptions) in 3.5+.
   // In pre-3.5, __has_feature(cxx_exceptions) can be true if ObjC exceptions are enabled, but C++ exceptions are disabled.
   // The recommended way to check is `__EXCEPTIONS && __has_feature(cxx_exceptions)`.
   // See https://releases.llvm.org/3.6.0/tools/clang/docs/ReleaseNotes.html#the-exceptions-macro
   // Note: this is only relevant in Objective-C++, thus the ifdef.
#  if __EXCEPTIONS && __has_feature(cxx_exceptions)
#   define gsl_HAVE_EXCEPTIONS  1
#  else
#   define gsl_HAVE_EXCEPTIONS  0
#  endif // __EXCEPTIONS && __has_feature(cxx_exceptions)
# else
   // clang-cl doesn't define __EXCEPTIONS for MSVC compatibility (see https://reviews.llvm.org/D4065).
   // Neither does Clang in MS-compatiblity mode.
   // Let's hope no one tries to build Objective-C++ code using MS-compatibility mode or clang-cl.
#  if __has_feature(cxx_exceptions)
#   define gsl_HAVE_EXCEPTIONS  1
#  else
#   define gsl_HAVE_EXCEPTIONS  0
#  endif
# endif
#elif defined( __GNUC__ )
# if __GNUC__ < 5
#  ifdef __EXCEPTIONS
#   define gsl_HAVE_EXCEPTIONS  1
#  else
#   define gsl_HAVE_EXCEPTIONS  0
#  endif // __EXCEPTIONS
# else
#  ifdef __cpp_exceptions
#   define gsl_HAVE_EXCEPTIONS  1
#  else
#   define gsl_HAVE_EXCEPTIONS  0
#  endif // __cpp_exceptions
# endif // __GNUC__ < 5
#elif gsl_COMPILER_MSVC_VERSION
# ifdef _CPPUNWIND
#  define  gsl_HAVE_EXCEPTIONS  1
# else
#  define  gsl_HAVE_EXCEPTIONS  0
# endif // _CPPUNWIND
#else
// For all other compilers, assume exceptions are always enabled.
# define   gsl_HAVE_EXCEPTIONS  1
#endif
#define    gsl_HAVE_EXCEPTIONS_()  gsl_HAVE_EXCEPTIONS

#if defined( gsl_CONFIG_CONTRACT_VIOLATION_THROWS ) && ! gsl_HAVE( EXCEPTIONS )
# error Cannot use gsl_CONFIG_CONTRACT_VIOLATION_THROWS if exceptions are disabled.
#endif // defined( gsl_CONFIG_CONTRACT_VIOLATION_THROWS ) && !gsl_HAVE( EXCEPTIONS )

#ifdef _HAS_CPP0X
# define gsl_HAS_CPP0X  _HAS_CPP0X
#else
# define gsl_HAS_CPP0X  0
#endif

#define gsl_CPP11_100  (gsl_CPP11_OR_GREATER || gsl_COMPILER_MS_STL_VER >= 1600)
#define gsl_CPP11_110  (gsl_CPP11_OR_GREATER || gsl_COMPILER_MS_STL_VER >= 1700)
#define gsl_CPP11_120  (gsl_CPP11_OR_GREATER || gsl_COMPILER_MS_STL_VER >= 1800)
#define gsl_CPP11_140  (gsl_CPP11_OR_GREATER || gsl_COMPILER_MS_STL_VER >= 1900)

#define gsl_CPP14_000  (gsl_CPP14_OR_GREATER)
#define gsl_CPP14_120  (gsl_CPP14_OR_GREATER || gsl_COMPILER_MS_STL_VER >= 1800)
#define gsl_CPP14_140  (gsl_CPP14_OR_GREATER || gsl_COMPILER_MS_STL_VER >= 1900)

#define gsl_CPP17_000  (gsl_CPP17_OR_GREATER)
#define gsl_CPP17_140  (gsl_CPP17_OR_GREATER || gsl_COMPILER_MS_STL_VER >= 1900)

#define gsl_CPP11_140_CPP0X_90   (gsl_CPP11_140 || (gsl_COMPILER_MS_STL_VER >= 1500 && gsl_HAS_CPP0X))
#define gsl_CPP11_140_CPP0X_100  (gsl_CPP11_140 || (gsl_COMPILER_MS_STL_VER >= 1600 && gsl_HAS_CPP0X))

// Presence of C++11 language features:

#define gsl_HAVE_C99_PREPROCESSOR          gsl_CPP11_140
#define gsl_HAVE_AUTO                      gsl_CPP11_100
#define gsl_HAVE_RVALUE_REFERENCE          gsl_CPP11_100
#define gsl_HAVE_FUNCTION_REF_QUALIFIER    ( gsl_CPP11_140 && ! gsl_BETWEEN( gsl_COMPILER_GNUC_VERSION, 1, 481 ) )
#define gsl_HAVE_ENUM_CLASS                gsl_CPP11_110
#define gsl_HAVE_ALIAS_TEMPLATE            gsl_CPP11_120
#define gsl_HAVE_DEFAULT_FUNCTION_TEMPLATE_ARG  gsl_CPP11_120
#define gsl_HAVE_EXPLICIT                  gsl_CPP11_120
#define gsl_HAVE_VARIADIC_TEMPLATE         gsl_CPP11_120
#define gsl_HAVE_IS_DELETE                 gsl_CPP11_120
#define gsl_HAVE_CONSTEXPR_11              gsl_CPP11_140
#define gsl_HAVE_IS_DEFAULT                gsl_CPP11_140
#define gsl_HAVE_NOEXCEPT                  gsl_CPP11_140
#define gsl_HAVE_NORETURN                  ( gsl_CPP11_140 && ! gsl_BETWEEN( gsl_COMPILER_GNUC_VERSION, 1, 480 ) )
#define gsl_HAVE_EXPRESSION_SFINAE         gsl_CPP11_140
#define gsl_HAVE_OVERRIDE_FINAL            gsl_CPP11_110

#define gsl_HAVE_C99_PREPROCESSOR_()       gsl_HAVE_C99_PREPROCESSOR
#define gsl_HAVE_AUTO_()                   gsl_HAVE_AUTO
#define gsl_HAVE_RVALUE_REFERENCE_()       gsl_HAVE_RVALUE_REFERENCE
#define gsl_HAVE_FUNCTION_REF_QUALIFIER_()  gsl_HAVE_FUNCTION_REF_QUALIFIER
#define gsl_HAVE_ENUM_CLASS_()             gsl_HAVE_ENUM_CLASS
#define gsl_HAVE_ALIAS_TEMPLATE_()         gsl_HAVE_ALIAS_TEMPLATE
#define gsl_HAVE_DEFAULT_FUNCTION_TEMPLATE_ARG_()  gsl_HAVE_DEFAULT_FUNCTION_TEMPLATE_ARG
#define gsl_HAVE_EXPLICIT_()               gsl_HAVE_EXPLICIT
#define gsl_HAVE_VARIADIC_TEMPLATE_()      gsl_HAVE_VARIADIC_TEMPLATE
#define gsl_HAVE_IS_DELETE_()              gsl_HAVE_IS_DELETE
#define gsl_HAVE_CONSTEXPR_11_()           gsl_HAVE_CONSTEXPR_11
#define gsl_HAVE_IS_DEFAULT_()             gsl_HAVE_IS_DEFAULT
#define gsl_HAVE_NOEXCEPT_()               gsl_HAVE_NOEXCEPT
#define gsl_HAVE_NORETURN_()               gsl_HAVE_NORETURN
#define gsl_HAVE_EXPRESSION_SFINAE_()      gsl_HAVE_EXPRESSION_SFINAE
#define gsl_HAVE_OVERRIDE_FINAL_()         gsl_HAVE_OVERRIDE_FINAL

// Presence of C++14 language features:

#define gsl_HAVE_CONSTEXPR_14              ( gsl_CPP14_000 && ! gsl_BETWEEN( gsl_COMPILER_GNUC_VERSION, 1, 600 ) )
#define gsl_HAVE_DECLTYPE_AUTO             gsl_CPP14_140
#define gsl_HAVE_DEPRECATED                ( gsl_CPP14_140 && ! gsl_BETWEEN( gsl_COMPILER_MSVC_VERSION, 1, 142 ) )

#define gsl_HAVE_CONSTEXPR_14_()           gsl_HAVE_CONSTEXPR_14
#define gsl_HAVE_DECLTYPE_AUTO_()          gsl_HAVE_DECLTYPE_AUTO
#define gsl_HAVE_DEPRECATED_()             gsl_HAVE_DEPRECATED

// Presence of C++17 language features:
// MSVC: template parameter deduction guides since Visual Studio 2017 v15.7

#define gsl_HAVE_ENUM_CLASS_CONSTRUCTION_FROM_UNDERLYING_TYPE  gsl_CPP17_000
#define gsl_HAVE_DEDUCTION_GUIDES          ( gsl_CPP17_000 && ! gsl_BETWEEN( gsl_COMPILER_MSVC_VERSION_FULL, 1, 1414 ) )
#define gsl_HAVE_NODISCARD                 gsl_CPP17_000
#define gsl_HAVE_CONSTEXPR_17              gsl_CPP17_OR_GREATER

#define gsl_HAVE_ENUM_CLASS_CONSTRUCTION_FROM_UNDERLYING_TYPE_()  gsl_HAVE_ENUM_CLASS_CONSTRUCTION_FROM_UNDERLYING_TYPE
#define gsl_HAVE_DEDUCTION_GUIDES_()       gsl_HAVE_DEDUCTION_GUIDES
#define gsl_HAVE_NODISCARD_()              gsl_HAVE_NODISCARD
#define gsl_HAVE_MAYBE_UNUSED_()           gsl_CPP17_OR_GREATER
#define gsl_HAVE_CONSTEXPR_17_()           gsl_HAVE_CONSTEXPR_17

// Presence of C++20 language features:

#define gsl_HAVE_CONSTEXPR_20              gsl_CPP20_OR_GREATER
#define gsl_HAVE_CONSTEXPR_20_()           gsl_HAVE_CONSTEXPR_20

// Presence of C++23 language features:

#define gsl_HAVE_CONSTEXPR_23              gsl_CPP23_OR_GREATER
#define gsl_HAVE_CONSTEXPR_23_()           gsl_HAVE_CONSTEXPR_23

// Presence of C++26 language features:

#define gsl_HAVE_CONSTEXPR_26              gsl_CPP26_OR_GREATER
#define gsl_HAVE_CONSTEXPR_26_()           gsl_HAVE_CONSTEXPR_26

// Presence of C++ library features:

#if gsl_BETWEEN( gsl_COMPILER_ARMCC_VERSION, 1, 600 )
// Some versions of the ARM compiler apparently ship without a C++11 standard library despite having some C++11 support.
# define gsl_STDLIB_CPP98_OR_GREATER  gsl_CPP98_OR_GREATER
# define gsl_STDLIB_CPP11_OR_GREATER  0
# define gsl_STDLIB_CPP14_OR_GREATER  0
# define gsl_STDLIB_CPP17_OR_GREATER  0
# define gsl_STDLIB_CPP20_OR_GREATER  0
# define gsl_STDLIB_CPP23_OR_GREATER  0
# define gsl_STDLIB_CPP26_OR_GREATER  0
#else
# define gsl_STDLIB_CPP98_OR_GREATER  gsl_CPP98_OR_GREATER
# define gsl_STDLIB_CPP11_OR_GREATER  gsl_CPP11_OR_GREATER
# define gsl_STDLIB_CPP14_OR_GREATER  gsl_CPP14_OR_GREATER
# define gsl_STDLIB_CPP17_OR_GREATER  gsl_CPP17_OR_GREATER
# define gsl_STDLIB_CPP20_OR_GREATER  gsl_CPP20_OR_GREATER
# define gsl_STDLIB_CPP23_OR_GREATER  gsl_CPP23_OR_GREATER
# define gsl_STDLIB_CPP26_OR_GREATER  gsl_CPP26_OR_GREATER
#endif

#define gsl_STDLIB_CPP11_100  (gsl_STDLIB_CPP11_OR_GREATER || gsl_COMPILER_MS_STL_VER >= 1600)
#define gsl_STDLIB_CPP11_110  (gsl_STDLIB_CPP11_OR_GREATER || gsl_COMPILER_MS_STL_VER >= 1700)
#define gsl_STDLIB_CPP11_120  (gsl_STDLIB_CPP11_OR_GREATER || gsl_COMPILER_MS_STL_VER >= 1800)
#define gsl_STDLIB_CPP11_140  (gsl_STDLIB_CPP11_OR_GREATER || gsl_COMPILER_MS_STL_VER >= 1900)

#define gsl_STDLIB_CPP14_000  (gsl_STDLIB_CPP14_OR_GREATER)
#define gsl_STDLIB_CPP14_120  (gsl_STDLIB_CPP14_OR_GREATER || gsl_COMPILER_MS_STL_VER >= 1800)
#define gsl_STDLIB_CPP14_140  (gsl_STDLIB_CPP14_OR_GREATER || gsl_COMPILER_MS_STL_VER >= 1900)

#define gsl_STDLIB_CPP17_000  (gsl_STDLIB_CPP17_OR_GREATER)
#define gsl_STDLIB_CPP17_140  (gsl_STDLIB_CPP17_OR_GREATER || gsl_COMPILER_MS_STL_VER >= 1900)

#define gsl_STDLIB_CPP11_140_CPP0X_90   (gsl_STDLIB_CPP11_140 || (gsl_COMPILER_MS_STL_VER >= 1500 && gsl_HAS_CPP0X))
#define gsl_STDLIB_CPP11_140_CPP0X_100  (gsl_STDLIB_CPP11_140 || (gsl_COMPILER_MS_STL_VER >= 1600 && gsl_HAS_CPP0X))

#define gsl_HAVE_ADDRESSOF                 gsl_STDLIB_CPP17_000
#define gsl_HAVE_ARRAY                     gsl_STDLIB_CPP11_110
#define gsl_HAVE_TYPE_TRAITS               gsl_STDLIB_CPP11_110
#define gsl_HAVE_TR1_TYPE_TRAITS           gsl_STDLIB_CPP11_110
#define gsl_HAVE_CONTAINER_DATA_METHOD     gsl_STDLIB_CPP11_140_CPP0X_90
#define gsl_HAVE_STD_DATA                  gsl_STDLIB_CPP17_000
#ifdef __cpp_lib_ssize
# define gsl_HAVE_STD_SSIZE                1
#else
# define gsl_HAVE_STD_SSIZE                ( gsl_COMPILER_GNUC_VERSION >= 1000 && __cplusplus > 201703L )
#endif
#define gsl_HAVE_HASH                      gsl_STDLIB_CPP11_120
#define gsl_HAVE_SIZED_TYPES               gsl_STDLIB_CPP11_140
#define gsl_HAVE_MAKE_SHARED               gsl_STDLIB_CPP11_140_CPP0X_100
#define gsl_HAVE_SHARED_PTR                gsl_STDLIB_CPP11_140_CPP0X_100
#define gsl_HAVE_UNIQUE_PTR                gsl_STDLIB_CPP11_140_CPP0X_100
#define gsl_HAVE_MAKE_UNIQUE               gsl_STDLIB_CPP14_120
#define gsl_HAVE_MOVE_FORWARD              gsl_STDLIB_CPP11_100
#define gsl_HAVE_NULLPTR                   gsl_STDLIB_CPP11_100
#define gsl_HAVE_UNCAUGHT_EXCEPTIONS       gsl_STDLIB_CPP17_140
#define gsl_HAVE_ADD_CONST                 gsl_HAVE_TYPE_TRAITS
#define gsl_HAVE_INITIALIZER_LIST          gsl_STDLIB_CPP11_120
#define gsl_HAVE_INTEGRAL_CONSTANT         gsl_HAVE_TYPE_TRAITS
#define gsl_HAVE_REMOVE_CONST              gsl_HAVE_TYPE_TRAITS
#define gsl_HAVE_REMOVE_REFERENCE          gsl_HAVE_TYPE_TRAITS
#define gsl_HAVE_REMOVE_CVREF              gsl_STDLIB_CPP20_OR_GREATER
#define gsl_HAVE_TR1_ADD_CONST             gsl_HAVE_TR1_TYPE_TRAITS
#define gsl_HAVE_TR1_INTEGRAL_CONSTANT     gsl_HAVE_TR1_TYPE_TRAITS
#define gsl_HAVE_TR1_REMOVE_CONST          gsl_HAVE_TR1_TYPE_TRAITS
#define gsl_HAVE_TR1_REMOVE_REFERENCE      gsl_HAVE_TR1_TYPE_TRAITS

#define gsl_HAVE_ADDRESSOF_()              gsl_HAVE_ADDRESSOF
#define gsl_HAVE_ARRAY_()                  gsl_HAVE_ARRAY
#define gsl_HAVE_TYPE_TRAITS_()            gsl_HAVE_TYPE_TRAITS
#define gsl_HAVE_TR1_TYPE_TRAITS_()        gsl_HAVE_TR1_TYPE_TRAITS
#define gsl_HAVE_CONTAINER_DATA_METHOD_()  gsl_HAVE_CONTAINER_DATA_METHOD
#define gsl_HAVE_HASH_()                   gsl_HAVE_HASH
#define gsl_HAVE_STD_DATA_()               gsl_HAVE_STD_DATA
#define gsl_HAVE_STD_SSIZE_()              gsl_HAVE_STD_SSIZE
#define gsl_HAVE_SIZED_TYPES_()            gsl_HAVE_SIZED_TYPES
#define gsl_HAVE_MAKE_SHARED_()            gsl_HAVE_MAKE_SHARED
#define gsl_HAVE_MOVE_FORWARD_()           gsl_HAVE_MOVE_FORWARD
#define gsl_HAVE_NULLPTR_()                gsl_HAVE_NULLPTR  // It's a language feature but needs library support, so we list it as a library feature.
#define gsl_HAVE_SHARED_PTR_()             gsl_HAVE_SHARED_PTR
#define gsl_HAVE_UNIQUE_PTR_()             gsl_HAVE_UNIQUE_PTR
#define gsl_HAVE_MAKE_UNIQUE_()            gsl_HAVE_MAKE_UNIQUE
#define gsl_HAVE_UNCAUGHT_EXCEPTIONS_()    gsl_HAVE_UNCAUGHT_EXCEPTIONS
#define gsl_HAVE_ADD_CONST_()              gsl_HAVE_ADD_CONST
#define gsl_HAVE_INITIALIZER_LIST_()       gsl_HAVE_INITIALIZER_LIST  // It's a language feature but needs library support, so we list it as a library feature.
#define gsl_HAVE_INTEGRAL_CONSTANT_()      gsl_HAVE_INTEGRAL_CONSTANT
#define gsl_HAVE_REMOVE_CONST_()           gsl_HAVE_REMOVE_CONST
#define gsl_HAVE_REMOVE_REFERENCE_()       gsl_HAVE_REMOVE_REFERENCE
#define gsl_HAVE_REMOVE_CVREF_()           gsl_HAVE_REMOVE_CVREF
#define gsl_HAVE_TR1_ADD_CONST_()          gsl_HAVE_TR1_ADD_CONST
#define gsl_HAVE_TR1_INTEGRAL_CONSTANT_()  gsl_HAVE_TR1_INTEGRAL_CONSTANT
#define gsl_HAVE_TR1_REMOVE_CONST_()       gsl_HAVE_TR1_REMOVE_CONST
#define gsl_HAVE_TR1_REMOVE_REFERENCE_()   gsl_HAVE_TR1_REMOVE_REFERENCE

// C++ feature usage:

#if gsl_HAVE( ADDRESSOF )
# define gsl_ADDRESSOF(x)  std::addressof(x)
#else
# define gsl_ADDRESSOF(x)  (&x)
#endif

#if gsl_HAVE( CONSTEXPR_11 )
# define gsl_constexpr  constexpr
#else
# define gsl_constexpr  /*constexpr*/
#endif

#if gsl_HAVE( CONSTEXPR_14 )
# define gsl_constexpr14  constexpr
#else
# define gsl_constexpr14  /*constexpr*/
#endif

#if gsl_HAVE( CONSTEXPR_17 )
# define gsl_constexpr17  constexpr
#else
# define gsl_constexpr17  /*constexpr*/
#endif

#if gsl_HAVE( CONSTEXPR_20 )
# define gsl_constexpr20  constexpr
#else
# define gsl_constexpr20  /*constexpr*/
#endif

#if gsl_HAVE( CONSTEXPR_23 )
# define gsl_constexpr23  constexpr
#else
# define gsl_constexpr23  /*constexpr*/
#endif

#if gsl_HAVE( CONSTEXPR_26 )
# define gsl_constexpr26  constexpr
#else
# define gsl_constexpr26  /*constexpr*/
#endif

#if gsl_HAVE( EXPLICIT )
# define gsl_explicit  explicit
#else
# define gsl_explicit  /*explicit*/
#endif

#if gsl_HAVE( IS_DELETE )
# define gsl_is_delete  = delete
#else
# define gsl_is_delete
#endif

#if gsl_HAVE( IS_DELETE )
# define gsl_is_delete_access  public
#else
# define gsl_is_delete_access  private
#endif

#if gsl_HAVE( NOEXCEPT )
# define gsl_noexcept             noexcept
# define gsl_noexcept_if( expr )  noexcept( expr )
#else
# define gsl_noexcept             throw()
# define gsl_noexcept_if( expr )  /*noexcept( expr )*/
#endif
#if defined( gsl_TESTING_ )
# define gsl_noexcept_not_testing
#else
# define gsl_noexcept_not_testing  gsl_noexcept
#endif

#if gsl_HAVE( NULLPTR )
# define gsl_nullptr  nullptr
#else
# define gsl_nullptr  NULL
#endif

#if gsl_HAVE( NODISCARD )
# define gsl_NODISCARD  [[nodiscard]]
#else
# define gsl_NODISCARD
#endif

#if gsl_HAVE( NORETURN )
# define gsl_NORETURN  [[noreturn]]
#elif defined( _MSC_VER )
# define gsl_NORETURN  __declspec(noreturn)
#elif defined( __GNUC__ ) || gsl_COMPILER_ARMCC_VERSION
# define gsl_NORETURN  __attribute__((noreturn))
#else
# define gsl_NORETURN
#endif

#if gsl_CPP20_OR_GREATER
# if defined( _MSC_VER ) // MSVC or MSVC-compatible compiler
#  if gsl_COMPILER_MSVC_VER >= 1929 || gsl_COMPILER_CLANG_VERSION >= 1800 // VS2019 v16.10 and later, or Clang 18 and later
#   define gsl_NO_UNIQUE_ADDRESS    [[msvc::no_unique_address]]
#  endif
# else // ! defined( _MSC_VER )
#  define  gsl_NO_UNIQUE_ADDRESS    [[no_unique_address]]
# endif // defined( _MSC_VER )
#endif // gsl_CPP20_OR_GREATER

#if gsl_HAVE( MAYBE_UNUSED )
# define gsl_MAYBE_UNUSED           [[maybe_unused]]
# if gsl_COMPILER_GNUC_VERSION
// GCC currently ignores the [[maybe_unused]] attribute on data members and warns accordingly (cf. https://stackoverflow.com/a/65633590).
# define gsl_MAYBE_UNUSED_MEMBER
# else // ! gsl_COMPILER_GNUC_VERSION
# define gsl_MAYBE_UNUSED_MEMBER    [[maybe_unused]]
# endif // gsl_COMPILER_GNUC_VERSION
#else
# define gsl_MAYBE_UNUSED
# define gsl_MAYBE_UNUSED_MEMBER
#endif

#if gsl_HAVE( DEPRECATED ) && ! defined( gsl_TESTING_ )
# define gsl_DEPRECATED             [[deprecated]]
# define gsl_DEPRECATED_MSG( msg )  [[deprecated( msg )]]
#else
# define gsl_DEPRECATED
# define gsl_DEPRECATED_MSG( msg )
#endif

#if gsl_HAVE( C99_PREPROCESSOR )
# ifdef __cpp_lib_concepts
#  define gsl_CONSTRAINT(...)  __VA_ARGS__
# else
#  define gsl_CONSTRAINT(...)  typename
# endif
#else
# define  gsl_CONSTRAINT( x )  typename
#endif

#if gsl_HAVE( TYPE_TRAITS )
# define gsl_STATIC_ASSERT_( cond, msg )  static_assert( cond, msg )
#else
# define gsl_STATIC_ASSERT_( cond, msg )  ( static_cast<void>( sizeof( char[1 - 2*!( cond ) ] ) ) )
#endif

#if _MSC_VER >= 1900  // Visual Studio 2015 and newer, or Clang emulating a corresponding MSVC
# define gsl_EMPTY_BASES_  __declspec(empty_bases)
#else
# define gsl_EMPTY_BASES_
#endif

#if gsl_HAVE( TYPE_TRAITS )

# define gsl_DEFINE_ENUM_BITMASK_OPERATORS_( ENUM )                        \
    gsl_MAYBE_UNUSED gsl_NODISCARD gsl_api inline gsl_constexpr bool       \
    operator!( ENUM val ) gsl_noexcept                                     \
    {                                                                      \
        return val == ENUM( );                                             \
    }                                                                      \
    gsl_MAYBE_UNUSED gsl_NODISCARD gsl_api inline gsl_constexpr ENUM       \
    operator~( ENUM val ) gsl_noexcept                                     \
    {                                                                      \
        typedef ::gsl_lite::std11::underlying_type<ENUM>::type U;          \
        return ENUM( ~U( val ) );                                          \
    }                                                                      \
    gsl_MAYBE_UNUSED gsl_NODISCARD gsl_api inline gsl_constexpr ENUM       \
    operator|( ENUM lhs, ENUM rhs ) gsl_noexcept                           \
    {                                                                      \
        typedef ::gsl_lite::std11::underlying_type<ENUM>::type U;          \
        return ENUM( U( lhs ) | U( rhs ) );                                \
    }                                                                      \
    gsl_MAYBE_UNUSED gsl_NODISCARD gsl_api inline gsl_constexpr ::gsl_lite::flags<ENUM>  \
    operator&( ENUM lhs, ENUM rhs ) gsl_noexcept                           \
    {                                                                      \
        typedef ::gsl_lite::std11::underlying_type<ENUM>::type U;          \
        return ENUM( U( lhs ) & U( rhs ) );                                \
    }                                                                      \
    gsl_MAYBE_UNUSED gsl_NODISCARD gsl_api inline gsl_constexpr ::gsl_lite::flags<ENUM>  \
    operator^( ENUM lhs, ENUM rhs ) gsl_noexcept                           \
    {                                                                      \
        typedef ::gsl_lite::std11::underlying_type<ENUM>::type U;          \
        return ENUM( U( lhs ) ^ U( rhs ) );                                \
    }                                                                      \
    gsl_MAYBE_UNUSED gsl_api inline gsl_constexpr14 ENUM &                 \
    operator|=( ENUM & lhs, ENUM rhs ) gsl_noexcept                        \
    {                                                                      \
        return lhs = lhs | rhs;                                            \
    }                                                                      \
    gsl_MAYBE_UNUSED gsl_api inline gsl_constexpr14 ENUM &                 \
    operator&=( ENUM & lhs, ENUM rhs ) gsl_noexcept                        \
    {                                                                      \
        return lhs = lhs & rhs;                                            \
    }                                                                      \
    gsl_MAYBE_UNUSED gsl_api inline gsl_constexpr14 ENUM &                 \
    operator^=( ENUM & lhs, ENUM rhs ) gsl_noexcept                        \
    {                                                                      \
        return lhs = lhs ^ rhs;                                            \
    }

# if defined( __cpp_lib_three_way_comparison )
#  define gsl_DEFINE_ENUM_RELATIONAL_OPERATORS_( ENUM )                                      \
    [[maybe_unused, nodiscard]] gsl_api inline constexpr std::strong_ordering                \
    operator<=>( ENUM lhs, ENUM rhs ) gsl_noexcept                                           \
    {                                                                                        \
        return std::underlying_type_t<ENUM>( lhs ) <=> std::underlying_type_t<ENUM>( rhs );  \
    }
# else // ! defined( __cpp_lib_three_way_comparison )
#  define gsl_DEFINE_ENUM_RELATIONAL_OPERATORS_( ENUM )                    \
    gsl_MAYBE_UNUSED gsl_NODISCARD gsl_api inline gsl_constexpr bool       \
    operator<( ENUM lhs, ENUM rhs ) gsl_noexcept                           \
    {                                                                      \
        typedef ::gsl_lite::std11::underlying_type<ENUM>::type U;          \
        return U( lhs ) < U( rhs );                                        \
    }                                                                      \
    gsl_MAYBE_UNUSED gsl_NODISCARD gsl_api inline gsl_constexpr bool       \
    operator>( ENUM lhs, ENUM rhs ) gsl_noexcept                           \
    {                                                                      \
        typedef ::gsl_lite::std11::underlying_type<ENUM>::type U;          \
        return U( lhs ) > U( rhs );                                        \
    }                                                                      \
    gsl_MAYBE_UNUSED gsl_NODISCARD gsl_api inline gsl_constexpr bool       \
    operator<=( ENUM lhs, ENUM rhs ) gsl_noexcept                          \
    {                                                                      \
        typedef ::gsl_lite::std11::underlying_type<ENUM>::type U;          \
        return U( lhs ) <= U( rhs );                                       \
    }                                                                      \
    gsl_MAYBE_UNUSED gsl_NODISCARD gsl_api inline gsl_constexpr bool       \
    operator>=( ENUM lhs, ENUM rhs ) gsl_noexcept                          \
    {                                                                      \
        typedef ::gsl_lite::std11::underlying_type<ENUM>::type U;          \
        return U( lhs ) >= U( rhs );                                       \
    }
# endif // defined( __cpp_lib_three_way_comparison )

    //
    // Defines bitmask operators `|`, `&`, `^`, `~`, `|=`, `&=`, and `^=` for the given enum type.
    //
    //     enum class Vegetables { tomato = 0b001, onion = 0b010, eggplant = 0b100 };
    //     gsl_DEFINE_ENUM_BITMASK_OPERATORS( Vegetables )
    //
# define gsl_DEFINE_ENUM_BITMASK_OPERATORS( ENUM ) gsl_DEFINE_ENUM_BITMASK_OPERATORS_( ENUM )

    //
    // Defines relational operators `<=>`, `<`, `>`, `<=`, `>=` for the given enum type.
    //
    //     enum class OperatorPrecedence { additive = 0, multiplicative = 1, power = 2 };
    //     gsl_DEFINE_ENUM_RELATIONAL_OPERATORS( OperatorPrecedence )
    //
# define gsl_DEFINE_ENUM_RELATIONAL_OPERATORS( ENUM ) gsl_DEFINE_ENUM_RELATIONAL_OPERATORS_( ENUM )

#endif // gsl_HAVE( TYPE_TRAITS )

#define gsl_DIMENSION_OF( a ) ( sizeof(a) / sizeof(0[a]) )


// Method enabling (C++98, VC120 (VS2013) cannot use __VA_ARGS__)

#if gsl_HAVE( EXPRESSION_SFINAE )
# define gsl_TRAILING_RETURN_TYPE_(T)  auto
# define gsl_TRAILING_RETURN_TYPE_2_(T, U)  auto
# define gsl_RETURN_DECLTYPE_(EXPR)    -> decltype( EXPR )
#else
# define gsl_TRAILING_RETURN_TYPE_(T)  T
# define gsl_TRAILING_RETURN_TYPE_2_(T, U)  T, U
# define gsl_RETURN_DECLTYPE_(EXPR)
#endif

// NOTE: When using SFINAE in gsl-lite, please note that overloads of function templates must always use SFINAE with non-type default arguments
//       as explained in https://en.cppreference.com/w/cpp/types/enable_if#Notes. `gsl_ENABLE_IF_()` implements graceful fallback to default
//       type arguments (for compilers that don't support non-type default arguments); please verify that this is appropriate in the given
//       situation, and add additional checks if necessary.
//
//       Also, please note that `gsl_ENABLE_IF_()` doesn't enforce the constraint at all if no compiler/library support is available (i.e. pre-C++11).

#if gsl_HAVE( TYPE_TRAITS )
# define gsl_ENABLE_IF_R_( VA, T )  typename std::enable_if< ( VA ), T >::type
#else // ! gsl_HAVE( TYPE_TRAITS )
# define gsl_ENABLE_IF_R_( VA, T )  T
#endif // gsl_HAVE( TYPE_TRAITS )

#if gsl_HAVE( TYPE_TRAITS ) && gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG )
# define  gsl_ENABLE_IF_NTTP_(VA)   , typename std::enable_if< ( VA ), int >::type = 0
# if ! gsl_BETWEEN( gsl_COMPILER_MSVC_VERSION, 1, 140 ) // VS 2013 seems to have trouble with SFINAE for default non-type arguments
#  define gsl_ENABLE_IF_(VA)        , typename std::enable_if< ( VA ), int >::type = 0
# else
#  define gsl_ENABLE_IF_(VA)        , typename = typename std::enable_if< ( VA ), ::gsl_lite::detail::enabler >::type
# endif
#else
# define  gsl_ENABLE_IF_NTTP_(VA)
# define  gsl_ENABLE_IF_(VA)
#endif


// Link-time ABI incompatibility detection (MSVC only):

#if defined( _MSC_VER ) && _MSC_VER >= 1900 // VS 2015 and newer
# if gsl_CONFIG( NARROW_THROWS_ON_TRUNCATION )
#  define gsl_CONFIG_NARROW_THROWS_ON_TRUNCATION_VAL_ 1
# else
#  define gsl_CONFIG_NARROW_THROWS_ON_TRUNCATION_VAL_ 0
# endif
# if defined( gsl_api )
#  define gsl_GSL_API_VAL_ gsl_api
# else
#  define gsl_GSL_API_VAL_ default
# endif
//# pragma message( "v" gsl_STRINGIFY( gsl_lite_MAJOR ) " gsl_api:" gsl_STRINGIFY( gsl_GSL_API_VAL_ ) " gsl_CONFIG_SPAN_INDEX_TYPE:" gsl_STRINGIFY( gsl_CONFIG_SPAN_INDEX_TYPE ) " gsl_CONFIG_INDEX_TYPE:" gsl_STRINGIFY( gsl_CONFIG_INDEX_TYPE ) " gsl_CONFIG_NARROW_THROWS_ON_TRUNCATION:" gsl_STRINGIFY( gsl_CONFIG_NARROW_THROWS_ON_TRUNCATION_VAL_ ) )
# pragma detect_mismatch( "gsl-lite", "v" gsl_STRINGIFY( gsl_lite_MAJOR ) " gsl_api:" gsl_STRINGIFY( gsl_GSL_API_VAL_ ) " gsl_CONFIG_SPAN_INDEX_TYPE:" gsl_STRINGIFY( gsl_CONFIG_SPAN_INDEX_TYPE ) " gsl_CONFIG_INDEX_TYPE:" gsl_STRINGIFY( gsl_CONFIG_INDEX_TYPE ) " gsl_CONFIG_NARROW_THROWS_ON_TRUNCATION:" gsl_STRINGIFY( gsl_CONFIG_NARROW_THROWS_ON_TRUNCATION_VAL_ ) )
# undef gsl_CONFIG_NARROW_THROWS_ON_TRUNCATION_VAL_
# undef gsl_GSL_API_VAL_
#endif // defined( _MSC_VER ) && _MSC_VER >= 1900


// Other features:

#define gsl_HAVE_CONSTRAINED_SPAN_CONTAINER_CTOR       ( gsl_HAVE_DEFAULT_FUNCTION_TEMPLATE_ARG && gsl_HAVE_CONTAINER_DATA_METHOD )
#define gsl_HAVE_CONSTRAINED_SPAN_CONTAINER_CTOR_()    gsl_HAVE_CONSTRAINED_SPAN_CONTAINER_CTOR

#define gsl_HAVE_UNCONSTRAINED_SPAN_CONTAINER_CTOR     ( gsl_CONFIG_ALLOWS_UNCONSTRAINED_SPAN_CONTAINER_CTOR && gsl_COMPILER_NVCC_VERSION == 0 )
#define gsl_HAVE_UNCONSTRAINED_SPAN_CONTAINER_CTOR_()  gsl_HAVE_UNCONSTRAINED_SPAN_CONTAINER_CTOR

// GSL API (e.g. for CUDA platform):

// Guidelines for using `gsl_api`:
//
// NVCC imposes the restriction that a function annotated `__host__ __device__` cannot call host-only or device-only functions.
// This makes `gsl_api` inappropriate for generic functions that call unknown code, e.g. the template constructors of `span<>`
// or functions like `finally()` which accept an arbitrary  function object.
// It is often preferable to annotate functions only with `gsl_constexpr` or `gsl_constexpr14`. The "extended constexpr" mode
// of NVCC (currently an experimental feature) will implicitly consider constexpr functions `__host__ __device__` functions
// but tolerates calls to host-only or device-only functions.

#if ! defined( gsl_api )
# ifdef   __CUDACC__
#  define gsl_api  __host__ __device__
# else
#  define gsl_api  /*gsl_api*/
# endif
#endif

// Additional includes:

#if gsl_FEATURE( STRING_SPAN )
# include <ios>    // for ios_base, streamsize
#endif
#if gsl_FEATURE( STRING_SPAN ) || defined( gsl_CONFIG_CONTRACT_VIOLATION_THROWS ) || ( defined( gsl_CONFIG_CONTRACT_VIOLATION_ASSERTS ) && gsl_CONFIG( USE_CRT_ASSERTION_HANDLER ) && ! ( gsl_COMPILER_MS_STL_VERSION && ! defined( _DEBUG ) ) && ! defined( __linux__ ) )
# include <string>
#endif
#if defined( gsl_CONFIG_CONTRACT_VIOLATION_ASSERTS ) && ! gsl_CONFIG( USE_CRT_ASSERTION_HANDLER )
# include <cstdio>
#endif

#if ! gsl_CPP11_OR_GREATER
# include <algorithm> // for swap() before C++11
#endif

#if gsl_HAVE( ARRAY )
# include <array> // indirectly includes reverse_iterator<>
#endif

#if ! gsl_HAVE( ARRAY )
# include <iterator> // for reverse_iterator<>
#endif

#ifdef __cpp_lib_three_way_comparison
# include <compare>
#endif

#ifdef __cpp_lib_concepts
# include <concepts>
#endif

#if ! gsl_HAVE( CONSTRAINED_SPAN_CONTAINER_CTOR ) || ! gsl_HAVE( AUTO )
# include <vector>
#endif

#if gsl_HAVE( INITIALIZER_LIST )
# include <initializer_list>
#endif

#if defined( gsl_CONFIG_CONTRACT_VIOLATION_ASSERTS ) || gsl_DEVICE_CODE
# include <cassert>
#endif

#if defined( gsl_CONFIG_CONTRACT_VIOLATION_TRAPS ) && gsl_COMPILER_MS_STL_VERSION >= 110 // __fastfail() supported by VS 2012 and later
# include <intrin.h>
#endif

#if gsl_HAVE( ENUM_CLASS ) && ( gsl_COMPILER_ARMCC_VERSION || gsl_COMPILER_NVHPC_VERSION ) && !defined( _WIN32 )
# include <endian.h>
#endif

#if gsl_HAVE( TYPE_TRAITS )
# include <type_traits> // for enable_if<>,
                        // add_const<>, add_pointer<>, common_type<>, make_signed<>, remove_cv<>, remove_const<>, remove_volatile<>, remove_reference<>, remove_cvref<>, remove_pointer<>, underlying_type<>,
                        // is_assignable<>, is_constructible<>, is_const<>, is_convertible<>, is_integral<>, is_pointer<>, is_signed<>,
                        // integral_constant<>, declval()
#elif gsl_HAVE( TR1_TYPE_TRAITS )
# include <tr1/type_traits> // for add_const<>, remove_cv<>, remove_const<>, remove_volatile<>, remove_reference<>, integral_constant<>
#endif

#if gsl_CONFIG( USE_CRT_ASSERTION_HANDLER ) && gsl_COMPILER_MS_STL_VERSION && defined( _DEBUG )
# include <crtdbg.h>
#endif

#ifdef __cpp_lib_source_location
# include <source_location>
#endif

#if defined( gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES_WITH_STACKTRACE )
# include <iostream>
# ifdef __cpp_lib_stacktrace
#  include <stacktrace>
# else
#  error gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES_WITH_STACKTRACE: requires C++23 for std::stacktrace
# endif
#endif

// We have to keep some older compilers from taking the modern route even though they technically support C++20:
// - Clang 18 has a weird codegen error that leads to the creation of temporaries when `std::source_location` is used, cf. https://gcc.godbolt.org/z/Ead5bMWhK.
// - MSVC 14.33 and earlier may wrongly default-construct `not_null<>` when `std::source_location` is used, cf. https://gcc.godbolt.org/z/xcWboMjoj.
#if defined( __cpp_lib_concepts ) && defined( __cpp_lib_source_location ) && defined( __cpp_lib_three_way_comparison ) && defined( __cpp_lib_integer_comparison_functions ) && \
    ! gsl_BETWEEN( gsl_COMPILER_CLANG_VERSION, 1, 1900 ) && ! gsl_BETWEEN( gsl_COMPILER_MSVC_VER, 1, 1934 )
// If a C++20 baseline can be assumed, many things can be simplified, and we can omit many the compatibility hacks for outdated compilers.
# define gsl_BASELINE_CPP20_  1
#else
# define gsl_BASELINE_CPP20_  0
#endif

// Declare __cxa_get_globals() or equivalent in namespace gsl_lite::detail for uncaught_exceptions():

#if ! gsl_HAVE( UNCAUGHT_EXCEPTIONS )
# if gsl_COMPILER_MS_STL_VERSION
namespace gsl_lite { namespace detail { extern "C" char * __cdecl _getptd(); } }
# elif gsl_COMPILER_CLANG_VERSION || gsl_COMPILER_GNUC_VERSION || gsl_COMPILER_APPLECLANG_VERSION || gsl_COMPILER_NVHPC_VERSION
#  if defined( __GLIBCXX__ ) || defined( __GLIBCPP__ )             // libstdc++: prototype from cxxabi.h
#   include  <cxxabi.h>
#  elif ! defined( BOOST_CORE_UNCAUGHT_EXCEPTIONS_HPP_INCLUDED_ )  // libc++: prototype from Boost?
#   if defined( __FreeBSD__ ) || defined( __OpenBSD__ )
namespace __cxxabiv1 { struct __cxa_eh_globals; extern "C" __cxa_eh_globals * __cxa_get_globals(); }
#   else
namespace __cxxabiv1 { struct __cxa_eh_globals; extern "C" __cxa_eh_globals * __cxa_get_globals() gsl_noexcept; }
#   endif
#  endif
    namespace gsl_lite { namespace detail { using ::__cxxabiv1::__cxa_get_globals; } }
# endif
#endif // ! gsl_HAVE( UNCAUGHT_EXCEPTIONS )


// Warning suppression macros:

#if gsl_COMPILER_MSVC_VERSION >= 140 && ! gsl_COMPILER_NVCC_VERSION
# define gsl_SUPPRESS_MSGSL_WARNING(expr)        [[gsl::suppress(expr)]]
# define gsl_SUPPRESS_MSVC_WARNING(code, descr)  __pragma(warning(suppress: code) )
# define gsl_DISABLE_MSVC_WARNINGS(codes)        __pragma(warning(push))  __pragma(warning(disable: codes))
# define gsl_RESTORE_MSVC_WARNINGS()             __pragma(warning(pop ))
#else
// TODO: define for Clang
# define gsl_SUPPRESS_MSGSL_WARNING(expr)
# define gsl_SUPPRESS_MSVC_WARNING(code, descr)
# define gsl_DISABLE_MSVC_WARNINGS(codes)
# define gsl_RESTORE_MSVC_WARNINGS()
#endif

// Warning suppressions:

#if gsl_COMPILER_CLANG_VERSION || gsl_COMPILER_APPLECLANG_VERSION
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wweak-vtables"  // because of `fail_fast` and `narrowing_error`
#endif // gsl_COMPILER_CLANG_VERSION || gsl_COMPILER_APPLECLANG_VERSION

#if gsl_COMPILER_GNUC_VERSION
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wuseless-cast"  // we use `static_cast<>()` in several places where it is possibly redundant depending on the configuration of the library
#endif // gsl_COMPILER_GNUC_VERSION

// Suppress the following MSVC GSL warnings:
// - C26432: gsl::c.21  : if you define or delete any default operation in the type '...', define or delete them all
// - C26410: gsl::r.32  : the parameter 'ptr' is a reference to const unique pointer, use const T* or const T& instead
// - C26415: gsl::r.30  : smart pointer parameter 'ptr' is used only to access contained pointer. Use T* or T& instead
// - C26418: gsl::r.36  : shared pointer parameter 'ptr' is not copied or moved. Use T* or T& instead
// - C26472: gsl::t.1   : don't use a static_cast for arithmetic conversions;
//                        use brace initialization, gsl_lite::narrow_cast or gsl_lite::narrow
// - C26439: gsl::f.6   : special function 'function' can be declared 'noexcept'
// - C26440: gsl::f.6   : function 'function' can be declared 'noexcept'
// - C26455: gsl::f.6   : default constructor may not throw. Declare it 'noexcept'
// - C26473: gsl::t.1   : don't cast between pointer types where the source type and the target type are the same
// - C26481: gsl::b.1   : don't use pointer arithmetic. Use span instead
// - C26482: gsl::b.2   : only index into arrays using constant expressions
// - C26446: gsl::b.4   : prefer to use gsl_lite::at() instead of unchecked subscript operator
// - C26490: gsl::t.1   : don't use reinterpret_cast
// - C26487: gsl::l.4   : don't return a pointer '(<some number>'s result)' that may be invalid
// - C26434: gsl::c.128 : function 'symbol_1' hides a non-virtual function 'symbol_2' (false positive for compiler-generated functions such as constructors)
// - C26456: gsl::c.128 : operator 'symbol_1' hides a non-virtual operator 'symbol_2' (false positive for compiler-generated operators)
// - C26457: es.48      : (void) should not be used to ignore return values, use 'std::ignore =' instead

gsl_DISABLE_MSVC_WARNINGS( 26432 26410 26415 26418 26472 26439 26440 26455 26473 26481 26482 26446 26490 26487 26434 26456 26457 )
#if gsl_BETWEEN( gsl_COMPILER_MSVC_VERSION, 110, 140 )  // VS 2012 and 2013
# pragma warning(disable: 4127)  // conditional expression is constant
#endif // gsl_BETWEEN( gsl_COMPILER_MSVC_VERSION, 110, 140 )
#if gsl_COMPILER_MSVC_VERSION == 140  // VS 2015
# pragma warning(disable: 4577)  // 'noexcept' used with no exception handling mode specified; termination on exception is not guaranteed. Specify /EHsc
#endif // gsl_COMPILER_MSVC_VERSION == 140

namespace gsl_lite {

#if gsl_FEATURE( SPAN )
// forward declare span<>:

#if gsl_CPP17_OR_GREATER
inline
#endif // gsl_CPP17_OR_GREATER
gsl_constexpr const gsl_CONFIG_SPAN_INDEX_TYPE dynamic_extent = static_cast<gsl_CONFIG_SPAN_INDEX_TYPE>( -1 );

template< class T, gsl_CONFIG_SPAN_INDEX_TYPE Extent = dynamic_extent >
class span;
#endif // gsl_FEATURE( SPAN )

namespace detail {

#if gsl_FEATURE( STRING_SPAN ) || ( gsl_FEATURE( SPAN ) && gsl_CONFIG( ALLOWS_SPAN_COMPARISON ) )
// We implement `equal()` and `lexicographical_compare()` here to avoid having to pull in the <algorithm> header.
template< class InputIt1, class InputIt2 >
bool equal( InputIt1 first1, InputIt1 last1, InputIt2 first2 )
{
    // Implementation borrowed from https://en.cppreference.com/w/cpp/algorithm/equal.
    for ( ; first1 != last1; ++first1, ++first2 )
    {
        if ( ! (*first1 == *first2 ) ) return false;
    }
    return true;
}
template< class InputIt1, class InputIt2 >
bool lexicographical_compare( InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2 )
{
    // Implementation borrowed from https://en.cppreference.com/w/cpp/algorithm/lexicographical_compare.
    for ( ; first1 != last1 && first2 != last2; ++first1, static_cast< void >( ++first2 ) )
    {
        if ( *first1 < *first2 ) return true;
        if ( *first2 < *first1 ) return false;
    }
    return first1 == last1 && first2 != last2;
}
#endif // gsl_FEATURE( STRING_SPAN ) || ( gsl_FEATURE( SPAN ) && gsl_CONFIG( ALLOWS_SPAN_COMPARISON ) )

} // namespace detail

// C++11 emulation:

namespace std11 {

template< class T > struct add_const { typedef const T type; };

template< class T > struct remove_const          { typedef T type; };
template< class T > struct remove_const<T const> { typedef T type; };

template< class T > struct remove_volatile             { typedef T type; };
template< class T > struct remove_volatile<T volatile> { typedef T type; };

template< class T >
struct remove_cv
{
    typedef typename remove_volatile<typename remove_const<T>::type>::type type;
};

template< class T > struct remove_reference { typedef T type; };
template< class T > struct remove_reference<T&> { typedef T type; };
#if gsl_HAVE( RVALUE_REFERENCE )
template< class T > struct remove_reference<T&&> { typedef T type; };
#endif

#if gsl_HAVE( ALIAS_TEMPLATE )
template< class T > using add_const_t = typename add_const<T>::type;
template< class T > using remove_const_t = typename remove_const<T>::type;
template< class T > using remove_volatile_t = typename remove_volatile<T>::type;
template< class T > using remove_cv_t = typename remove_cv<T>::type;
template< class T > using remove_reference_t = typename remove_reference<T>::type;
#endif // gsl_HAVE( ALIAS_TEMPLATE )


#if gsl_HAVE( INTEGRAL_CONSTANT )

using std::integral_constant;
using std::true_type;
using std::false_type;

#elif gsl_HAVE( TR1_INTEGRAL_CONSTANT )

using std::tr1::integral_constant;
using std::tr1::true_type;
using std::tr1::false_type;

#else

template< class T, T v > struct integral_constant { enum { value = v }; };
typedef integral_constant< bool, true  > true_type;
typedef integral_constant< bool, false > false_type;

#endif

#if gsl_HAVE( TYPE_TRAITS )

using std::underlying_type;

#elif gsl_HAVE( TR1_TYPE_TRAITS )

using std::tr1::underlying_type;

#else

// We could try to define `underlying_type<>` for pre-C++11 here, but let's not until someone actually needs it.

#endif

} // namespace std11

// C++14 emulation:

namespace std14 {

#if gsl_HAVE( UNIQUE_PTR )
# if gsl_HAVE( MAKE_UNIQUE )

using std::make_unique;

# elif gsl_HAVE( VARIADIC_TEMPLATE )

template< class T, class... Args >
gsl_NODISCARD std::unique_ptr<T>
make_unique( Args &&... args )
{
#  if gsl_HAVE( TYPE_TRAITS )
    static_assert( !std::is_array<T>::value, "make_unique<T[]>() is not part of C++14" );
#  endif
    return std::unique_ptr<T>( new T( std::forward<Args>( args )... ) );
}

# endif // gsl_HAVE( MAKE_UNIQUE ), gsl_HAVE( VARIADIC_TEMPLATE )
#endif // gsl_HAVE( UNIQUE_PTR )

} // namespace std14

namespace detail {

#if gsl_HAVE( VARIADIC_TEMPLATE )

template< bool V0, class T0, class... Ts > struct conjunction_ { using type = T0; };
template< class T0, class T1, class... Ts > struct conjunction_<true, T0, T1, Ts...> : conjunction_<T1::value, T1, Ts...> { };
template< bool V0, class T0, class... Ts > struct disjunction_ { using type = T0; };
template< class T0, class T1, class... Ts > struct disjunction_<false, T0, T1, Ts...> : disjunction_<T1::value, T1, Ts...> { };

#else // a.k.a. ! gsl_HAVE( VARIADIC_TEMPLATE )

template< bool V0, class T0, class T1 > struct conjunction_ { typedef T0 type; };
template< class T0, class T1 > struct conjunction_<true, T0, T1> { typedef T1 type; };
template< bool V0, class T0, class T1 > struct disjunction_ { typedef T0 type; };
template< class T0, class T1 > struct disjunction_<false, T0, T1> { typedef T1 type; };

#endif // gsl_HAVE( VARIADIC_TEMPLATE )


template< class > struct dependent_false : std11::integral_constant<bool, false> { };

} // namespace detail

// C++17 emulation:

namespace std17 {

template< bool v > struct bool_constant : std11::integral_constant<bool, v>{};

template< class T > struct negation : std11::integral_constant<bool, !T::value> { };

#if gsl_CPP11_120

template< class... Ts > struct conjunction;
template< > struct conjunction< > : std11::true_type { };
template< class T0, class... Ts > struct conjunction<T0, Ts...> : detail::conjunction_<T0::value, T0, Ts...>::type { };
template< class... Ts > struct disjunction;
template< > struct disjunction< > : std11::false_type { };
template< class T0, class... Ts > struct disjunction<T0, Ts...> : detail::disjunction_<T0::value, T0, Ts...>::type { };

# if gsl_CPP14_OR_GREATER

template< class... Ts > constexpr bool conjunction_v = conjunction<Ts...>::value;
template< class... Ts > constexpr bool disjunction_v = disjunction<Ts...>::value;
template< class T > constexpr bool negation_v = negation<T>::value;

# endif // gsl_CPP14_OR_GREATER

template< class... Ts >
struct make_void { typedef void type; };

template< class... Ts >
using void_t = typename make_void< Ts... >::type;

#else // a.k.a. ! gsl_CPP11_120

// For C++98, define simpler binary variants of `conjunction<>` and `disjunction<>`.
template< class T0, class T1 > struct conjunction : detail::conjunction_<T0::value, T0, T1>::type { };
template< class T0, class T1 > struct disjunction : detail::disjunction_<T0::value, T0, T1>::type { };

#endif // gsl_CPP11_120

#if gsl_HAVE( STD_SSIZE )

using std::size;
using std::data;

#elif gsl_HAVE( CONSTRAINED_SPAN_CONTAINER_CTOR )

template< class T, size_t N >
gsl_NODISCARD gsl_api inline gsl_constexpr auto
size( T const(&)[N] ) gsl_noexcept -> size_t
{
    return N;
}

template< class C >
gsl_NODISCARD inline gsl_constexpr auto
size( C const & cont ) -> decltype( cont.size() )
{
    return cont.size();
}

template< class T, size_t N >
gsl_NODISCARD gsl_api inline gsl_constexpr auto
data( T(&arr)[N] ) gsl_noexcept -> T*
{
    return &arr[0];
}

template< class C >
gsl_NODISCARD inline gsl_constexpr auto
data( C & cont ) -> decltype( cont.data() )
{
    return cont.data();
}

template< class C >
gsl_NODISCARD inline gsl_constexpr auto
data( C const & cont ) -> decltype( cont.data() )
{
    return cont.data();
}

# if gsl_HAVE( INITIALIZER_LIST )
template< class E >
gsl_NODISCARD inline gsl_constexpr E const *
data( std::initializer_list<E> il ) gsl_noexcept
{
    return il.begin();
}
# endif // gsl_HAVE( INITIALIZER_LIST )

#endif // gsl_HAVE( CONSTRAINED_SPAN_CONTAINER_CTOR )

} // namespace std17

#if gsl_HAVE( CONSTRAINED_SPAN_CONTAINER_CTOR )
using std17::data;
using std17::size;
#endif // gsl_HAVE( CONSTRAINED_SPAN_CONTAINER_CTOR )

// C++20 emulation:

namespace std20 {

#if gsl_CPP11_100

struct identity
{
    template< class T >
    gsl_constexpr T && operator ()( T && arg ) const gsl_noexcept
    {
        return std::forward<T>( arg );
    }
};

# if gsl_HAVE( ENUM_CLASS )
enum class endian
{
#  if defined( _WIN32 )
    little = 0,
    big    = 1,
    native = little
#  elif gsl_COMPILER_GNUC_VERSION || gsl_COMPILER_CLANG_VERSION || gsl_COMPILER_APPLECLANG_VERSION
    little = __ORDER_LITTLE_ENDIAN__,
    big    = __ORDER_BIG_ENDIAN__,
    native = __BYTE_ORDER__
#  elif gsl_COMPILER_ARMCC_VERSION || gsl_COMPILER_NVHPC_VERSION
    // from <endian.h> header file
    little = __LITTLE_ENDIAN,
    big    = __BIG_ENDIAN,
    native = __BYTE_ORDER
#  else
// Do not define any endianness constants for unknown compilers.
#  endif
};
# endif // gsl_HAVE( ENUM_CLASS )

#endif // gsl_CPP11_100

template< class T >
struct type_identity
{
    typedef T type;
};
#if gsl_HAVE( ALIAS_TEMPLATE )
template< class T >
using type_identity_t = typename type_identity<T>::type;
#endif // gsl_HAVE( ALIAS_TEMPLATE )

#if gsl_HAVE( STD_SSIZE )

using std::ssize;

#elif gsl_HAVE( CONSTRAINED_SPAN_CONTAINER_CTOR )

template< class C >
gsl_NODISCARD gsl_constexpr auto
ssize( C const & c )
    -> typename std::common_type<std::ptrdiff_t, typename std::make_signed<decltype(c.size())>::type>::type
{
    using R = typename std::common_type<std::ptrdiff_t, typename std::make_signed<decltype(c.size())>::type>::type;
    return static_cast<R>( c.size() );
}

template< class T, std::size_t N >
gsl_NODISCARD gsl_constexpr auto
ssize( T const(&)[N] ) gsl_noexcept -> std::ptrdiff_t
{
    return std::ptrdiff_t( N );
}

#endif // gsl_HAVE( STD_SSIZE )

template< class T > struct remove_cvref { typedef typename std11::remove_cv< typename std11::remove_reference< T >::type >::type type; };
#if gsl_HAVE( ALIAS_TEMPLATE )
template< class T > using remove_cvref_t = typename remove_cvref<T>::type;
#endif // gsl_HAVE( ALIAS_TEMPLATE )

} // namespace std20

#if gsl_HAVE( STD_SSIZE ) || gsl_HAVE( CONSTRAINED_SPAN_CONTAINER_CTOR )
using std20::ssize;
#endif // gsl_HAVE( STD_SSIZE ) || gsl_HAVE( CONSTRAINED_SPAN_CONTAINER_CTOR )

#if gsl_CPP11_100
using std20::identity;
#endif // gsl_CPP11_100
using std20::type_identity;
#if gsl_HAVE( ALIAS_TEMPLATE )
using std20::type_identity_t;
#endif // gsl_HAVE( ALIAS_TEMPLATE )

// C++23 emulation:

namespace std23 {

#if gsl_HAVE( TYPE_TRAITS ) || gsl_HAVE( TR1_TYPE_TRAITS )
template< class EnumT >
gsl_NODISCARD gsl_constexpr typename std11::underlying_type<EnumT>::type
to_underlying( EnumT value ) gsl_noexcept
{
    return static_cast<typename std11::underlying_type<EnumT>::type>( value );
}
#endif // gsl_HAVE( TYPE_TRAITS ) || gsl_HAVE( TR1_TYPE_TRAITS )

} // namespace std23

#if gsl_HAVE( TYPE_TRAITS ) || gsl_HAVE( TR1_TYPE_TRAITS )
using std23::to_underlying;
#endif // gsl_HAVE( TYPE_TRAITS ) || gsl_HAVE( TR1_TYPE_TRAITS )

// C++26 emulation:

namespace std26 {

} // namespace std26

namespace detail {

/// for gsl_ENABLE_IF_()

/*enum*/ class enabler{};

template< class C >
struct is_char : std11::false_type{};
template<>
struct is_char<char> : std11::true_type{};
#if gsl_HAVE( WCHAR )
template<>
struct is_char<wchar_t> : std11::true_type{};
#endif
#ifdef __cpp_char8_t  // C++20
template<>
struct is_char<char8_t> : std11::true_type{};
#endif
#if gsl_CPP11_140
template<>
struct is_char<char16_t> : std11::true_type{};
template<>
struct is_char<char32_t> : std11::true_type{};
#endif
template< class T, class C >
struct is_czstring_of : std11::false_type{};
template< class C >
struct is_czstring_of< C const *, C > : is_char<C>{};

#if gsl_FEATURE( SPAN ) && gsl_HAVE( TYPE_TRAITS )

template< class Q >
struct is_span_oracle : std::false_type{};

template< class T, gsl_CONFIG_SPAN_INDEX_TYPE Extent >
struct is_span_oracle< span<T, Extent > > : std::true_type{};

template< class Q >
struct is_span : is_span_oracle< typename std::remove_cv<Q>::type >{};

template< class Q >
struct is_std_array_oracle : std::false_type{};

# if gsl_HAVE( ARRAY )

template< class T, std::size_t Extent >
struct is_std_array_oracle< std::array<T, Extent> > : std::true_type{};

# endif

template< class Q >
struct is_std_array : is_std_array_oracle< typename std::remove_cv<Q>::type >{};

template< class Q >
struct is_array : std::false_type{};

template< class T >
struct is_array<T[]> : std::true_type{};

template< class T, std::size_t N >
struct is_array<T[N]> : std::true_type{};

# if gsl_CPP11_140 && ! gsl_BETWEEN( gsl_COMPILER_GNUC_VERSION, 1, 500 )

template< class, class = void >
struct has_size_and_data : std::false_type{};

template< class C >
struct has_size_and_data
<
    C, std17::void_t<
        decltype( std17::size(std::declval<C>()) ),
        decltype( std17::data(std::declval<C>()) ) >
> : std::true_type{};

template< class, class, class = void >
struct is_compatible_element : std::false_type {};

template< class C, class E >
struct is_compatible_element
<
    C, E, std17::void_t<
        decltype( std17::data(std::declval<C>()) ),
        typename std::remove_pointer<decltype( std17::data( std::declval<C&>() ) )>::type(*)[] >
> : std::is_convertible< typename std::remove_pointer<decltype( std17::data( std::declval<C&>() ) )>::type(*)[], E(*)[] >{};

template< class C >
struct is_container : std17::bool_constant
<
    ! is_span< C >::value
    && ! is_array< C >::value
    && ! is_std_array< C >::value
    &&   has_size_and_data< C >::value
>{};

template< class C, class E >
struct is_compatible_container : std17::bool_constant
<
    is_container<C>::value
    && is_compatible_element<C,E>::value
>{};

# else // ^^^ gsl_CPP11_140 && ! gsl_BETWEEN( gsl_COMPILER_GNUC_VERSION, 1, 500 ) ^^^ / vvv ! gsl_CPP11_140 || gsl_BETWEEN( gsl_COMPILER_GNUC_VERSION, 1, 500 ) vvv

template<
    class C, class E
        , typename = typename std::enable_if<
            ! is_span< C >::value
            && ! is_array< C >::value
            && ! is_std_array< C >::value
            && ( std::is_convertible< typename std::remove_pointer<decltype( std17::data( std::declval<C&>() ) )>::type(*)[], E(*)[] >::value)
        //  &&   has_size_and_data< C >::value
        , enabler>::type
        , class = decltype( std17::size(std::declval<C>()) )
        , class = decltype( std17::data(std::declval<C>()) )
>
#  if gsl_BETWEEN( gsl_COMPILER_MSVC_VERSION, 1, 140 )
// VS2013 has insufficient support for expression SFINAE; we cannot make `is_compatible_container<>` a proper type trait here
struct is_compatible_container : std::true_type { };
#  else
struct is_compatible_container_r { is_compatible_container_r(int); };
template< class C, class E >
std::true_type  is_compatible_container_f( is_compatible_container_r<C, E> );
template< class C, class E >
std::false_type is_compatible_container_f( ... );

template< class C, class E >
struct is_compatible_container : decltype( is_compatible_container_f< C, E >( 0 ) ) { };
#  endif // gsl_BETWEEN( gsl_COMPILER_MSVC_VERSION, 1, 140 )

# endif // gsl_CPP11_140 && ! gsl_BETWEEN( gsl_COMPILER_GNUC_VERSION, 1, 500 )

#endif // gsl_HAVE( TYPE_TRAITS )

} // namespace detail

//
// GSL.util: utilities
//

// Integer type for indices (e.g. in a loop).
typedef gsl_CONFIG_INDEX_TYPE index;

// Integer type for dimensions.
typedef gsl_CONFIG_INDEX_TYPE dim;

// Integer type for array strides.
typedef gsl_CONFIG_INDEX_TYPE stride;

// Integer type for pointer, iterator, or index differences.
typedef gsl_CONFIG_INDEX_TYPE diff;

//
// GSL.owner: ownership pointers
//
#if gsl_HAVE( SHARED_PTR )
using std::unique_ptr;
using std::shared_ptr;
#endif

#if gsl_HAVE( ALIAS_TEMPLATE )
# if defined( __cpp_lib_concepts )
  template< class T >
  requires std::is_pointer_v<T>
  using owner = T;
# elif gsl_HAVE( TYPE_TRAITS )
  template< class T, typename = typename std::enable_if< std::is_pointer<T>::value >::type >
  using owner = T;
# else // ! gsl_HAVE( TYPE_TRAITS )
  template< class T >
  using owner = T;
# endif
#endif

#define gsl_HAVE_OWNER_TEMPLATE     gsl_HAVE_ALIAS_TEMPLATE
#define gsl_HAVE_OWNER_TEMPLATE_()  gsl_HAVE_OWNER_TEMPLATE

//
// GSL.assert: assertions
//

#define gsl_NO_OP_()      ( static_cast<void>( 0 ) )
#if gsl_HAVE( TYPE_TRAITS ) && gsl_CONFIG( VALIDATES_UNENFORCED_CONTRACT_EXPRESSIONS )
# define gsl_ELIDE_( x )  static_assert( ::std::is_constructible<bool, decltype( x )>::value, "argument of contract check must be convertible to bool" )
#else
# define gsl_ELIDE_( x )  gsl_NO_OP_()
#endif

// Suppress "controlling expression is constant" warning when using `gsl_Expects()`, `gsl_Ensures()`, `gsl_Assert()`, etc.
#if gsl_COMPILER_NVHPC_VERSION
# define gsl_DIAG_SUPPRESS_236_  _Pragma("diag_suppress 236")
# define gsl_DIAG_RESTORE_236_   _Pragma("diag_default 236")
#elif gsl_BETWEEN( gsl_COMPILER_MSVC_VERSION, 110, 140 )  // VS 2012 and 2013
# define gsl_DIAG_SUPPRESS_236_  gsl_DISABLE_MSVC_WARNINGS( 4127 )
# define gsl_DIAG_RESTORE_236_   gsl_RESTORE_MSVC_WARNINGS()
#else
# define gsl_DIAG_SUPPRESS_236_
# define gsl_DIAG_RESTORE_236_
#endif

#if gsl_DEVICE_CODE
# if defined( gsl_CONFIG_DEVICE_UNENFORCED_CONTRACTS_ASSUME )
#  if gsl_COMPILER_NVCC_VERSION >= 113
#   define gsl_ASSUME_( x )           ( __builtin_assume( !!( x ) ) )
#   define gsl_ASSUME_UNREACHABLE_()  __builtin_unreachable()
#  else  // unknown device compiler
#   error  gsl_CONFIG_DEVICE_UNENFORCED_CONTRACTS_ASSUME: gsl-lite does not know how to generate UB optimization hints in device code for this compiler; use gsl_CONFIG_DEVICE_UNENFORCED_CONTRACTS_ELIDE instead
#  endif
#  define  gsl_CONTRACT_UNENFORCED_( x )  gsl_ASSUME_( x )
# else // defined( gsl_CONFIG_DEVICE_UNENFORCED_CONTRACTS_ELIDE ) [default]
#  define  gsl_CONTRACT_UNENFORCED_( x )  gsl_ELIDE_( x )
# endif
# if gsl_COMPILER_NVCC_VERSION
#  define  gsl_TRAP_()  __trap()
# elif defined(__has_builtin)
#  if __has_builtin(__builtin_trap)
#   define gsl_TRAP_()  __builtin_trap()
#  endif
# endif
# if !defined( gsl_TRAP_ )
#  error   gsl-lite does not know how to generate a trap instruction for this device compiler
# endif
# define   gsl_TRAP_FALLBACK_()  gsl_TRAP_()
#else // host code
# if defined( gsl_CONFIG_UNENFORCED_CONTRACTS_ASSUME )
#  if gsl_COMPILER_MSVC_VERSION >= 140
#   define  gsl_ASSUME_( x )           __assume( x )
#   define  gsl_ASSUME_UNREACHABLE_()  __assume( 0 )
#  elif gsl_COMPILER_GNUC_VERSION
#   define  gsl_ASSUME_( x )           ( ( x ) ? static_cast<void>(0) : __builtin_unreachable() )
#   define  gsl_ASSUME_UNREACHABLE_()  __builtin_unreachable()
#  elif defined(__has_builtin)
#   if __has_builtin(__builtin_unreachable)
#    define gsl_ASSUME_( x )           ( ( x ) ? static_cast<void>(0) : __builtin_unreachable() )
#    define gsl_ASSUME_UNREACHABLE_()  __builtin_unreachable()
#   else
#    error  gsl_CONFIG_UNENFORCED_CONTRACTS_ASSUME: gsl-lite does not know how to generate UB optimization hints for this compiler; use gsl_CONFIG_UNENFORCED_CONTRACTS_ELIDE instead
#   endif
#  else
#   error   gsl_CONFIG_UNENFORCED_CONTRACTS_ASSUME: gsl-lite does not know how to generate UB optimization hints for this compiler; use gsl_CONFIG_UNENFORCED_CONTRACTS_ELIDE instead
#  endif
#  define   gsl_CONTRACT_UNENFORCED_( x )  gsl_ASSUME_( x )
# else // defined( gsl_CONFIG_UNENFORCED_CONTRACTS_ELIDE ) [default]
#  define   gsl_CONTRACT_UNENFORCED_( x )  gsl_ELIDE_( x )
# endif
# if gsl_COMPILER_MS_STL_VERSION >= 110 // __fastfail() supported by VS 2012 and later
#  define   gsl_TRAP_()  __fastfail( 5 ) /* failure code for invalid arguments, cf. winnt.h, "Fast fail failure codes" */
# elif gsl_COMPILER_GNUC_VERSION
#  define   gsl_TRAP_()  __builtin_trap()
# elif defined(__has_builtin)
#  if __has_builtin(__builtin_trap)
#   define  gsl_TRAP_()  __builtin_trap()
#  endif
# endif
# if defined( gsl_TRAP_ )
#  if defined( _MSC_VER )
#  define   gsl_TRAP_FALLBACK_()  ( ::gsl_lite::detail::fail_fast_terminate() )
#  else // !defined( _MSC_VER )
#  define   gsl_TRAP_FALLBACK_()  gsl_TRAP_()
#  endif
# else
#  define   gsl_TRAP_FALLBACK_()  ( ::gsl_lite::detail::fail_fast_terminate() )
# endif
#endif // gsl_DEVICE_CODE

#if gsl_CPP11_100
# define gsl_FUNC_  __func__
#else
# define gsl_FUNC_  ""
#endif

#if gsl_DEVICE_CODE
# if defined( gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_TRAPS )
#  define  gsl_CONTRACT_CHECK_( x )            ( gsl_DIAG_SUPPRESS_236_ ( x ) ? static_cast<void>(0) : gsl_TRAP_() gsl_DIAG_RESTORE_236_ )
#  define  gsl_CONTRACT_VERIFY_( x )           ( gsl_DIAG_SUPPRESS_236_ ( x ) ? true : ( gsl_TRAP_(), false ) gsl_DIAG_RESTORE_236_ )
#  define  gsl_CONTRACT_CHECK_MSG_( str, x )   gsl_CONTRACT_CHECK_( x )
#  define  gsl_CONTRACT_VERIFY_MSG_( str, x )  gsl_CONTRACT_VERIFY_( x )
#  define  gsl_FAILFAST_()                     gsl_TRAP_()
#  define  gsl_CONTRACT_CHECK_AT_( loc, x )    gsl_CONTRACT_CHECK_( ( static_cast<void>( loc ), x ) )
#  define  gsl_CONTRACT_VERIFY_AT_( loc, x )   gsl_CONTRACT_VERIFY_( ( static_cast<void>( loc ), x ) )
#  define  gsl_FAILFAST_AT_( loc )             ( static_cast<void>( loc ), gsl_FAILFAST_() )
# elif defined( gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_CALLS_HANDLER )
#  define  gsl_CONTRACT_CHECK_( x )            ( gsl_DIAG_SUPPRESS_236_ ( x ) ? static_cast<void>(0) : ::gsl_lite::fail_fast_assert_handler( #x, "", __FILE__, __LINE__ ) gsl_DIAG_RESTORE_236_ )
#  define  gsl_CONTRACT_VERIFY_( x )           ( gsl_DIAG_SUPPRESS_236_ ( x ) ? true : ( ::gsl_lite::fail_fast_assert_handler( #x, "", __FILE__, __LINE__ ), false ) gsl_DIAG_RESTORE_236_ )
#  define  gsl_FAILFAST_()                     ( ::gsl_lite::fail_fast_assert_handler( "unreachable", "", __FILE__, __LINE__ ), gsl_TRAP_() ) /* do not let the custom assertion handler continue execution */
#  define  gsl_CONTRACT_CHECK_MSG_( str, x )   ( gsl_DIAG_SUPPRESS_236_ ( x ) ? static_cast<void>(0) : ::gsl_lite::fail_fast_assert_handler( #x, str, __FILE__, __LINE__ ) gsl_DIAG_RESTORE_236_ )
#  define  gsl_CONTRACT_VERIFY_MSG_( str, x )  ( gsl_DIAG_SUPPRESS_236_ ( x ) ? true : ( ::gsl_lite::fail_fast_assert_handler( #x, str, __FILE__, __LINE__ ), false ) gsl_DIAG_RESTORE_236_ )
#  define  gsl_CONTRACT_CHECK_AT_( loc, x )    ( gsl_DIAG_SUPPRESS_236_ ( x ) ? static_cast<void>(0) : ::gsl_lite::fail_fast_assert_handler( #x, gsl_FUNC_, loc.file_name(), static_cast<int>( loc.line() ) ) gsl_DIAG_RESTORE_236_ )
#  define  gsl_CONTRACT_VERIFY_AT_( loc, x )   ( gsl_DIAG_SUPPRESS_236_ ( x ) ? true : ( ::gsl_lite::fail_fast_assert_handler( #x, gsl_FUNC_, loc.file_name(), static_cast<int>( loc.line() ) ), false ) gsl_DIAG_RESTORE_236_ )
#  define  gsl_FAILFAST_AT_( loc )             ( ::gsl_lite::fail_fast_assert_handler( "unreachable", gsl_FUNC_, loc.file_name(), static_cast<int>( loc.line() ) ), gsl_TRAP_() ) /* do not let the custom assertion handler continue execution */
# else // defined( gsl_CONFIG_DEVICE_CONTRACT_VIOLATION_ASSERTS ) [default]
#  if ! defined( NDEBUG )
#  define  gsl_CONTRACT_CHECK_( x )            assert( x )
#  define  gsl_CONTRACT_VERIFY_( x )           ( ( x ) ? true : ( assert( false && #x ), false ) )
#  define  gsl_CONTRACT_CHECK_MSG_( str, x )   gsl_CONTRACT_CHECK_( x )
#  define  gsl_CONTRACT_VERIFY_MSG_( str, x )  gsl_CONTRACT_VERIFY_( x )
#  define  gsl_CONTRACT_CHECK_AT_( loc, x )    gsl_CONTRACT_CHECK_( ( static_cast<void>( loc ), x ) )
#  define  gsl_CONTRACT_VERIFY_AT_( loc, x )   gsl_CONTRACT_VERIFY_( ( static_cast<void>( loc ), x ) )
#  else
#  define  gsl_CONTRACT_CHECK_( x )            ( gsl_DIAG_SUPPRESS_236_ ( x ) ? static_cast<void>(0) : gsl_TRAP_() gsl_DIAG_RESTORE_236_ )
#  define  gsl_CONTRACT_VERIFY_( x )           ( gsl_DIAG_SUPPRESS_236_ ( x ) ? true : ( gsl_TRAP_(), false ) gsl_DIAG_RESTORE_236_ )
#  define  gsl_CONTRACT_CHECK_MSG_( str, x )   gsl_CONTRACT_CHECK_( x )
#  define  gsl_CONTRACT_VERIFY_MSG_( str, x )  gsl_CONTRACT_VERIFY_( x )
#  define  gsl_CONTRACT_CHECK_AT_( loc, x )    gsl_CONTRACT_CHECK_( ( static_cast<void>( loc ), x ) )
#  define  gsl_CONTRACT_VERIFY_AT_( loc, x )   gsl_CONTRACT_VERIFY_( ( static_cast<void>( loc ), x ) )
#  endif
#  define  gsl_FAILFAST_()                     gsl_TRAP_()
#  define  gsl_FAILFAST_AT_( loc )             ( static_cast<void>( loc ), gsl_FAILFAST_() )
# endif
#else // host code
# if defined( gsl_CONFIG_CONTRACT_VIOLATION_TRAPS )
#  if !defined( gsl_TRAP_ )
#   error    gsl_CONFIG_CONTRACT_VIOLATION_TRAPS: gsl-lite does not know how to generate a trap instruction for this compiler; use gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES instead
#  endif
#  define    gsl_CONTRACT_CHECK_( x )            ( gsl_DIAG_SUPPRESS_236_ ( x ) ? static_cast<void>(0) : gsl_TRAP_() gsl_DIAG_RESTORE_236_ )
#  define    gsl_CONTRACT_VERIFY_( x )           ( gsl_DIAG_SUPPRESS_236_ ( x ) ? true : ( gsl_TRAP_(), false ) gsl_DIAG_RESTORE_236_ )
#  define    gsl_FAILFAST_()                     gsl_TRAP_FALLBACK_()
#  define    gsl_CONTRACT_CHECK_MSG_( str, x )   gsl_CONTRACT_CHECK_( x )
#  define    gsl_CONTRACT_VERIFY_MSG_( str, x )  gsl_CONTRACT_VERIFY_( x )
#  define    gsl_CONTRACT_CHECK_AT_( loc, x )    gsl_CONTRACT_CHECK_( ( static_cast<void>( loc ), x ) )
#  define    gsl_CONTRACT_VERIFY_AT_( loc, x )   gsl_CONTRACT_VERIFY_( ( static_cast<void>( loc ), x ) )
#  define    gsl_FAILFAST_AT_( loc )             ( static_cast<void>( loc ), gsl_FAILFAST_() )
# elif defined( gsl_CONFIG_CONTRACT_VIOLATION_CALLS_HANDLER )
#  define    gsl_CONTRACT_CHECK_( x )            ( gsl_DIAG_SUPPRESS_236_ ( x ) ? static_cast<void>(0) : ::gsl_lite::fail_fast_assert_handler( #x, "", __FILE__, __LINE__ ) gsl_DIAG_RESTORE_236_ )
#  define    gsl_CONTRACT_VERIFY_( x )           ( gsl_DIAG_SUPPRESS_236_ ( x ) ? true : ( ::gsl_lite::fail_fast_assert_handler( #x, "", __FILE__, __LINE__ ), false ) gsl_DIAG_RESTORE_236_ )
#  define    gsl_FAILFAST_()                     ( ::gsl_lite::fail_fast_assert_handler( "unreachable", "", __FILE__, __LINE__ ), gsl_TRAP_FALLBACK_() ) /* do not let the custom assertion handler continue execution */
#  define    gsl_CONTRACT_CHECK_MSG_( str, x )   ( gsl_DIAG_SUPPRESS_236_ ( x ) ? static_cast<void>(0) : ::gsl_lite::fail_fast_assert_handler( #x, str, __FILE__, __LINE__ ) gsl_DIAG_RESTORE_236_ )
#  define    gsl_CONTRACT_VERIFY_MSG_( str, x )  ( gsl_DIAG_SUPPRESS_236_ ( x ) ? true : ( ::gsl_lite::fail_fast_assert_handler( #x, str, __FILE__, __LINE__ ), false ) gsl_DIAG_RESTORE_236_ )
#  define    gsl_CONTRACT_CHECK_AT_( loc, x )    ( gsl_DIAG_SUPPRESS_236_ ( x ) ? static_cast<void>(0) : ::gsl_lite::fail_fast_assert_handler( #x, gsl_FUNC_, loc.file_name(), static_cast<int>( loc.line() ) ) gsl_DIAG_RESTORE_236_ )
#  define    gsl_CONTRACT_VERIFY_AT_( loc, x )   ( gsl_DIAG_SUPPRESS_236_ ( x ) ? true : ( ::gsl_lite::fail_fast_assert_handler( #x, gsl_FUNC_, loc.file_name(), static_cast<int>( loc.line() ) ), false ) gsl_DIAG_RESTORE_236_ )
#  define    gsl_FAILFAST_AT_( loc )             ( ::gsl_lite::fail_fast_assert_handler( "unreachable", gsl_FUNC_, loc.file_name(), static_cast<int>( loc.line() ) ), gsl_TRAP_FALLBACK_() ) /* do not let the custom assertion handler continue execution */
# elif defined( gsl_CONFIG_CONTRACT_VIOLATION_ASSERTS ) // [default]
#  if gsl_CONFIG( USE_CRT_ASSERTION_HANDLER ) && gsl_COMPILER_MS_STL_VERSION && defined( _DEBUG )
#   define   gsl_CONTRACT_CHECK_( x )            ( gsl_DIAG_SUPPRESS_236_ ( x ) ? static_cast<void>(0) : static_cast<void>( ::_CrtDbgReport( _CRT_ASSERT, __FILE__, __LINE__, gsl_nullptr, "`%s'", #x ) == 1 && ( _CrtDbgBreak(), false ) ) gsl_DIAG_RESTORE_236_ )
#   define   gsl_CONTRACT_VERIFY_( x )           ( gsl_DIAG_SUPPRESS_236_ ( x ) ? true : ( ::_CrtDbgReport( _CRT_ASSERT, __FILE__, __LINE__, gsl_nullptr, "`%s'", #x ) == 1 && ( _CrtDbgBreak(), false ) ) gsl_DIAG_RESTORE_236_ )
#   define   gsl_FAILFAST_()                     ( static_cast<void>( ::_CrtDbgReport( _CRT_ASSERT, __FILE__, __LINE__, gsl_nullptr, "unreachable" ) == 1 && ( _CrtDbgBreak(), false ) ), gsl_TRAP_FALLBACK_() )
#   define   gsl_CONTRACT_CHECK_MSG_( str, x )   ( gsl_DIAG_SUPPRESS_236_ ( x ) ? static_cast<void>(0) : static_cast<void>( ::_CrtDbgReport( _CRT_ASSERT, __FILE__, __LINE__, gsl_nullptr, "%s: `%s'", str, #x ) == 1 && ( _CrtDbgBreak(), false ) ) gsl_DIAG_RESTORE_236_ )
#   define   gsl_CONTRACT_VERIFY_MSG_( str, x )  ( gsl_DIAG_SUPPRESS_236_ ( x ) ? true : ( ::_CrtDbgReport( _CRT_ASSERT, __FILE__, __LINE__, gsl_nullptr, "%s: `%s'", str, #x ) == 1 && ( _CrtDbgBreak(), false ) ) gsl_DIAG_RESTORE_236_ )
#   define   gsl_CONTRACT_CHECK_AT_( loc, x )    ( gsl_DIAG_SUPPRESS_236_ ( x ) ? static_cast<void>(0) : static_cast<void>( ::_CrtDbgReport( _CRT_ASSERT, loc.file_name(), static_cast<int>( loc.line() ), gsl_nullptr, "%s: `%s'", gsl_FUNC_, #x ) == 1 && ( _CrtDbgBreak(), false ) ) gsl_DIAG_RESTORE_236_ )
#   define   gsl_CONTRACT_VERIFY_AT_( loc, x )   ( gsl_DIAG_SUPPRESS_236_ ( x ) ? true : ( ::_CrtDbgReport( _CRT_ASSERT, loc.file_name(), static_cast<int>( loc.line() ), gsl_nullptr, "%s: `%s'", gsl_FUNC_, #x ) == 1 && ( _CrtDbgBreak(), false ) ) gsl_DIAG_RESTORE_236_ )
#   define   gsl_FAILFAST_AT_( loc )             ( static_cast<void>( ::_CrtDbgReport( _CRT_ASSERT, loc.file_name(), static_cast<int>( loc.line() ), gsl_nullptr, "%s: unreachable", gsl_FUNC_ ) == 1 && ( _CrtDbgBreak(), false ) ), gsl_TRAP_FALLBACK_() )
#  else
#   define   gsl_CONTRACT_CHECK_( x )            ( gsl_DIAG_SUPPRESS_236_ ( x ) ? static_cast<void>(0) : ::gsl_lite::detail::fail_fast_assert( #x, gsl_nullptr, __FILE__, __LINE__ ) gsl_DIAG_RESTORE_236_ )
#   define   gsl_CONTRACT_VERIFY_( x )           ( gsl_DIAG_SUPPRESS_236_ ( x ) ? true : ( ::gsl_lite::detail::fail_fast_assert( #x, gsl_nullptr, __FILE__, __LINE__ ), false ) gsl_DIAG_RESTORE_236_ )
#   define   gsl_FAILFAST_()                     ( ::gsl_lite::detail::fail_fast_assert( "unreachable", gsl_nullptr, __FILE__, __LINE__ ) )
#   define   gsl_CONTRACT_CHECK_MSG_( str, x )   ( gsl_DIAG_SUPPRESS_236_ ( x ) ? static_cast<void>(0) : ::gsl_lite::detail::fail_fast_assert( #x, str, __FILE__, __LINE__ ) gsl_DIAG_RESTORE_236_ )
#   define   gsl_CONTRACT_VERIFY_MSG_( str, x )  ( gsl_DIAG_SUPPRESS_236_ ( x ) ? true : ( ::gsl_lite::detail::fail_fast_assert( #x, str, __FILE__, __LINE__ ), false ) gsl_DIAG_RESTORE_236_ )
#   define   gsl_CONTRACT_CHECK_AT_( loc, x )    ( gsl_DIAG_SUPPRESS_236_ ( x ) ? static_cast<void>(0) : ::gsl_lite::detail::fail_fast_assert( #x, gsl_FUNC_, loc.file_name(), loc.line() ) gsl_DIAG_RESTORE_236_ )
#   define   gsl_CONTRACT_VERIFY_AT_( loc, x )   ( gsl_DIAG_SUPPRESS_236_ ( x ) ? true : ( ::gsl_lite::detail::fail_fast_assert( #x, gsl_FUNC_, loc.file_name(), loc.line() ), false ) gsl_DIAG_RESTORE_236_ )
#   define   gsl_FAILFAST_AT_( loc )             ( ::gsl_lite::detail::fail_fast_assert( "unreachable", gsl_FUNC_, loc.file_name(), loc.line() ) )
#   endif
# elif defined( gsl_CONFIG_CONTRACT_VIOLATION_THROWS )
#  define    gsl_CONTRACT_CHECK_MSG_( str, x )   ( gsl_DIAG_SUPPRESS_236_ ( x ) ? static_cast<void>(0) : ::gsl_lite::detail::fail_fast_throw( #x, str, __FILE__, __LINE__ ) gsl_DIAG_RESTORE_236_ )
#  define    gsl_CONTRACT_VERIFY_MSG_( str, x )  ( gsl_DIAG_SUPPRESS_236_ ( x ) ? true : ( ::gsl_lite::detail::fail_fast_throw( #x, str, __FILE__, __LINE__ ), false ) gsl_DIAG_RESTORE_236_ )
#  define    gsl_CONTRACT_CHECK_( x )            gsl_CONTRACT_CHECK_MSG_( gsl_nullptr, x )
#  define    gsl_CONTRACT_VERIFY_( x )           gsl_CONTRACT_VERIFY_MSG_( gsl_nullptr, x )
#  define    gsl_FAILFAST_()                     ( ::gsl_lite::detail::fail_fast_throw( "unreachable", gsl_nullptr, __FILE__, __LINE__ ) )
#  define    gsl_CONTRACT_CHECK_AT_( loc, x )    ( gsl_DIAG_SUPPRESS_236_ ( x ) ? static_cast<void>(0) : ::gsl_lite::detail::fail_fast_throw( #x, gsl_FUNC_, loc.file_name(), loc.line() ) gsl_DIAG_RESTORE_236_ )
#  define    gsl_CONTRACT_VERIFY_AT_( loc, x )   ( gsl_DIAG_SUPPRESS_236_ ( x ) ? true : ( ::gsl_lite::detail::fail_fast_throw( #x, gsl_FUNC_, loc.file_name(), loc.line() ), false ) gsl_DIAG_RESTORE_236_ )
#  define    gsl_FAILFAST_AT_( loc )             ( ::gsl_lite::detail::fail_fast_throw( "unreachable", gsl_FUNC_, loc.file_name(), loc.line() ) )
# elif defined( gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES_WITH_STACKTRACE )
#  define    gsl_CONTRACT_CHECK_( x )            ( gsl_DIAG_SUPPRESS_236_ ( x ) ? static_cast<void>(0) : ::gsl_lite::detail::fail_fast_trace( #x, gsl_nullptr ) gsl_DIAG_RESTORE_236_ )
#  define    gsl_CONTRACT_VERIFY_( x )           ( gsl_DIAG_SUPPRESS_236_ ( x ) ? true : ( ::gsl_lite::detail::fail_fast_trace( #x, gsl_nullptr ), false ) gsl_DIAG_RESTORE_236_ )
#  define    gsl_FAILFAST_()                     ( ::gsl_lite::detail::fail_fast_trace( "unreachable", gsl_nullptr ) )
#  define    gsl_CONTRACT_CHECK_MSG_( str, x )   ( gsl_DIAG_SUPPRESS_236_ ( x ) ? static_cast<void>(0) : ::gsl_lite::detail::fail_fast_trace( #x, str ) gsl_DIAG_RESTORE_236_ )
#  define    gsl_CONTRACT_VERIFY_MSG_( str, x )  ( gsl_DIAG_SUPPRESS_236_ ( x ) ? true : ( ::gsl_lite::detail::fail_fast_trace( #x, str ), false ) gsl_DIAG_RESTORE_236_ )
#  define    gsl_CONTRACT_CHECK_AT_( loc, x )    ( gsl_DIAG_SUPPRESS_236_ ( x ) ? static_cast<void>(0) : ::gsl_lite::detail::fail_fast_trace( ( static_cast<void>( loc ), #x ), gsl_FUNC_ ) gsl_DIAG_RESTORE_236_ )
#  define    gsl_CONTRACT_VERIFY_AT_( loc, x )   ( gsl_DIAG_SUPPRESS_236_ ( x ) ? true : ( ::gsl_lite::detail::fail_fast_trace( ( static_cast<void>( loc ), #x ), gsl_FUNC_ ), false ) gsl_DIAG_RESTORE_236_ )
#  define    gsl_FAILFAST_AT_( loc )             ( ::gsl_lite::detail::fail_fast_trace( ( static_cast<void>( loc ), "unreachable" ), gsl_FUNC_ ) )
# else // defined( gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES )
#  define    gsl_CONTRACT_CHECK_( x )            ( gsl_DIAG_SUPPRESS_236_ ( x ) ? static_cast<void>(0) : ::gsl_lite::detail::fail_fast_terminate() gsl_DIAG_RESTORE_236_ )
#  define    gsl_CONTRACT_VERIFY_( x )           ( gsl_DIAG_SUPPRESS_236_ ( x ) ? true : ( ::gsl_lite::detail::fail_fast_terminate(), false ) gsl_DIAG_RESTORE_236_ )
#  define    gsl_FAILFAST_()                     ( ::gsl_lite::detail::fail_fast_terminate() )
#   define   gsl_CONTRACT_CHECK_MSG_( str, x )   gsl_CONTRACT_CHECK_( x )
#   define   gsl_CONTRACT_VERIFY_MSG_( str, x )  gsl_CONTRACT_VERIFY_( x )
#  define    gsl_CONTRACT_CHECK_AT_( loc, x )    gsl_CONTRACT_CHECK_( ( static_cast<void>( loc ), x ) )
#  define    gsl_CONTRACT_VERIFY_AT_( loc, x )   gsl_CONTRACT_VERIFY_( ( static_cast<void>( loc ), x ) )
#  define    gsl_FAILFAST_AT_( loc )             ( static_cast<void>( loc ), gsl_FAILFAST_() )
# endif
#endif // gsl_DEVICE_CODE

#if ( !gsl_DEVICE_CODE && defined( gsl_CONFIG_CONTRACT_CHECKING_OFF ) ) || ( gsl_DEVICE_CODE && defined( gsl_CONFIG_DEVICE_CONTRACT_CHECKING_OFF ) )
# define  gsl_CHECK_CONTRACTS_        0
# define  gsl_CHECK_DEBUG_CONTRACTS_  0
# define  gsl_CHECK_AUDIT_CONTRACTS_  0
#elif ( !gsl_DEVICE_CODE && defined( gsl_CONFIG_CONTRACT_CHECKING_AUDIT ) ) || ( gsl_DEVICE_CODE && defined( gsl_CONFIG_DEVICE_CONTRACT_CHECKING_AUDIT ) )
# define  gsl_CHECK_CONTRACTS_        1
# define  gsl_CHECK_DEBUG_CONTRACTS_  1
# define  gsl_CHECK_AUDIT_CONTRACTS_  1
#else // gsl_CONFIG_[DEVICE_]CONTRACT_CHECKING_ON [default]
# define  gsl_CHECK_CONTRACTS_        1
# if !defined( NDEBUG )
#  define gsl_CHECK_DEBUG_CONTRACTS_  1
# else // defined( NDEBUG )
#  define gsl_CHECK_DEBUG_CONTRACTS_  0
# endif
# define  gsl_CHECK_AUDIT_CONTRACTS_  0
#endif

#if gsl_CHECK_CONTRACTS_ && !defined( gsl_CONFIG_CONTRACT_CHECKING_EXPECTS_OFF )
# define  gsl_Expects( x )       gsl_CONTRACT_CHECK_MSG_( "precondition", x )
#else
# define  gsl_Expects( x )       gsl_CONTRACT_UNENFORCED_( x )
#endif
#if gsl_CHECK_DEBUG_CONTRACTS_ && !defined( gsl_CONFIG_CONTRACT_CHECKING_EXPECTS_OFF )
# define  gsl_ExpectsDebug( x )  gsl_CONTRACT_CHECK_MSG_( "precondition", x )
#else
# define  gsl_ExpectsDebug( x )  gsl_ELIDE_( x )
#endif
#if gsl_CHECK_AUDIT_CONTRACTS_ && !defined( gsl_CONFIG_CONTRACT_CHECKING_EXPECTS_OFF )
# define  gsl_ExpectsAudit( x )  gsl_CONTRACT_CHECK_MSG_( "precondition", x )
#else
# define  gsl_ExpectsAudit( x )  gsl_ELIDE_( x )
#endif
#if gsl_CHECK_CONTRACTS_ && !defined( gsl_CONFIG_CONTRACT_CHECKING_ENSURES_OFF )
# define  gsl_Ensures( x )       gsl_CONTRACT_CHECK_MSG_( "postcondition", x )
#else
# define  gsl_Ensures( x )       gsl_CONTRACT_UNENFORCED_( x )
#endif
#if gsl_CHECK_DEBUG_CONTRACTS_ && !defined( gsl_CONFIG_CONTRACT_CHECKING_ENSURES_OFF )
# define  gsl_EnsuresDebug( x )  gsl_CONTRACT_CHECK_MSG_( "postcondition", x )
#else
# define  gsl_EnsuresDebug( x )  gsl_ELIDE_( x )
#endif
#if gsl_CHECK_AUDIT_CONTRACTS_ && !defined( gsl_CONFIG_CONTRACT_CHECKING_ENSURES_OFF )
# define  gsl_EnsuresAudit( x )  gsl_CONTRACT_CHECK_MSG_( "postcondition", x )
#else
# define  gsl_EnsuresAudit( x )  gsl_ELIDE_( x )
#endif
#if gsl_CHECK_CONTRACTS_ && !defined( gsl_CONFIG_CONTRACT_CHECKING_ASSERT_OFF )
# define  gsl_Assert( x )        gsl_CONTRACT_CHECK_( x )
#else
# define  gsl_Assert( x )        gsl_CONTRACT_UNENFORCED_( x )
#endif
#if gsl_CHECK_DEBUG_CONTRACTS_ && !defined( gsl_CONFIG_CONTRACT_CHECKING_ASSERT_OFF )
# define  gsl_AssertDebug( x )   gsl_CONTRACT_CHECK_( x )
#else
# define  gsl_AssertDebug( x )   gsl_ELIDE_( x )
#endif
#if gsl_CHECK_AUDIT_CONTRACTS_ && !defined( gsl_CONFIG_CONTRACT_CHECKING_ASSERT_OFF )
# define  gsl_AssertAudit( x )   gsl_CONTRACT_CHECK_( x )
#else
# define  gsl_AssertAudit( x )   gsl_ELIDE_( x )
#endif
#if gsl_CHECK_CONTRACTS_ && !defined( gsl_CONFIG_CONTRACT_CHECKING_ASSERT_OFF )
# define  gsl_Verify( x )        gsl_CONTRACT_VERIFY_( x )
#else
# define  gsl_Verify( x )        ( !!( x ) )
#endif
#define   gsl_FailFast()         gsl_FAILFAST_()

#ifdef __cpp_lib_source_location
# if gsl_CHECK_CONTRACTS_ && !defined( gsl_CONFIG_CONTRACT_CHECKING_ASSERT_OFF )
#  define gsl_AssertAt( loc, x )        gsl_CONTRACT_CHECK_AT_( loc, x )
# else
#  define gsl_AssertAt( loc, x )        gsl_CONTRACT_UNENFORCED_( ( static_cast<void>( loc ), ( x ) ) )
# endif
# if gsl_CHECK_DEBUG_CONTRACTS_ && !defined( gsl_CONFIG_CONTRACT_CHECKING_ASSERT_OFF )
#  define gsl_AssertAtDebug( loc, x )   gsl_CONTRACT_CHECK_AT_( loc, x )
# else
#  define gsl_AssertAtDebug( loc, x )   gsl_ELIDE_( ( static_cast<void>( loc ), ( x ) ) )
# endif
# if gsl_CHECK_AUDIT_CONTRACTS_ && !defined( gsl_CONFIG_CONTRACT_CHECKING_ASSERT_OFF )
#  define gsl_AssertAtAudit( loc, x )   gsl_CONTRACT_CHECK_AT_( loc, x )
# else
#  define gsl_AssertAtAudit( loc, x )   gsl_ELIDE_( ( static_cast<void>( loc ), ( x ) ) )
# endif
# if gsl_CHECK_CONTRACTS_ && !defined( gsl_CONFIG_CONTRACT_CHECKING_ASSERT_OFF )
#  define gsl_VerifyAt( loc, x )        gsl_CONTRACT_VERIFY_AT_( loc, x )
# else
#  define gsl_VerifyAt( loc, x )        ( static_cast<void>( loc ), !!( x ) )
# endif
# define  gsl_FailFastAt( loc )        gsl_FAILFAST_AT_( loc )
#endif // __cpp_lib_source_location

#undef gsl_CHECK_CONTRACTS_
#undef gsl_CHECK_DEBUG_CONTRACTS_
#undef gsl_CHECK_AUDIT_CONTRACTS_


struct fail_fast : public std::logic_error
{
    explicit fail_fast( char const * message )
    : std::logic_error( message ) {}
};

namespace detail {

#if defined( gsl_CONFIG_CONTRACT_VIOLATION_ASSERTS )
# if gsl_CONFIG( USE_CRT_ASSERTION_HANDLER )
#  if gsl_COMPILER_MS_STL_VERSION && ! defined( _DEBUG )
// According to the UCRT documentation, the `_assert()` function is exported by the UCRT but not defined in a header file.
// (cf. https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/assert-macro-assert-wassert)
extern "C"
#  ifdef _DLL
__declspec( dllimport )
#  endif
void __cdecl _assert( char const * expr, char const * file, unsigned line );
#  elif defined( __linux__ )
// The `__assert_fail()` function is exported by the CRT on Linux according to Linux Standard Base but not necessarily
// declared in a header file.
// (cf. https://refspecs.linuxbase.org/LSB_5.0.0/LSB-Core-generic/LSB-Core-generic/baselib---assert-fail-1.html)
extern "C" void __assert_fail( char const * expr, char const * file, unsigned line, char const * function );
#  else
// We assume that the function `__assert()` is exported by the CRT.
// This function seems to be widely available but is not really documented, so it not used by default unless
// `gsl_CONFIG_USE_CRT_ASSERTION_HANDLER` is explicitly set to 1.
extern "C" void __assert( char const * expr, char const * file, int line );
#  endif
# endif // gsl_CONFIG( USE_CRT_ASSERTION_HANDLER )
# if ! gsl_COMPILER_MS_STL_VERSION || ! defined( _DEBUG )
gsl_NORETURN
#  if defined( _MSC_VER )
__declspec( noinline )
#  elif defined( __GNUC__ )
__attribute__(( noinline ))
#  endif
inline void fail_fast_assert( char const * expression, char const * message, char const * filename, unsigned line )
{
#  if gsl_CONFIG( USE_CRT_ASSERTION_HANDLER )
#   if defined( __linux__ )
    detail::__assert_fail( expression, filename, line, message );
#   else
    if ( message && message[0] != '\0' )
    {
        std::string s = message;
        s += ": ";
        s += expression;
#    if gsl_COMPILER_MS_STL_VERSION
        detail::_assert( s.c_str(), filename, line );
#    else // ! gsl_COMPILER_MS_STL_VERSION
        detail::__assert( s.c_str(), filename, static_cast<int>( line ) );
#    endif
    }
    else
    {
#    if gsl_COMPILER_MS_STL_VERSION
        detail::_assert( expression, filename, line );
#    else // ! gsl_COMPILER_MS_STL_VERSION
        detail::__assert( expression, filename, static_cast<int>( line ) );
#    endif
    }
#   endif
#  else // ! gsl_CONFIG( USE_CRT_ASSERTION_HANDLER )
    bool haveMessage = message && message[0] != '\0';
    std::fprintf( stderr,
        haveMessage ? "%s:%u: Assertion failed: `%s: %s'\n" : "%s:%u: Assertion failed: `%s%s'\n",
        filename, line, haveMessage ? message : "", expression );
    std::fflush( stderr );
#  endif
    std::abort();
}
# endif // ! gsl_COMPILER_MS_STL_VERSION || ! defined( _DEBUG )
#endif // defined( gsl_CONFIG_CONTRACT_VIOLATION_ASSERTS )
#if defined( gsl_CONFIG_CONTRACT_VIOLATION_THROWS )
gsl_NORETURN
# if defined( _MSC_VER )
__declspec( noinline )
# elif defined( __GNUC__ )
__attribute__(( noinline ))
# endif
inline void fail_fast_throw( char const * expression, char const * message, char const * filename, unsigned line )
{
    // At the expense of having to assemble the string at runtime, we avoid bloating the executable by keeping filename and message strings separate.
    std::string s = "assertion failed: `";
    if ( message && message[0] )
    {
        s += message;
        s += ": ";
    }
    s += expression;
    s += "' at ";
    s += filename;
    s += ":";
#if gsl_CPP11_110
    s += std::to_string( line );
#else
    // The C++98 standard library offers no reasonable way to convert an integer to a string.
    // We'd rather avoid calling `sprintf()`, so we implement simple base-10 conversion here.
    std::string::size_type pos = s.length();
    if ( line == 0 )
    {
        s += '0';
    }
    else
    {
        while ( line != 0 )
        {
            unsigned digit = line % 10;
            line /= 10;
            s.insert( pos, 1, static_cast<char>( '0' + digit ) );
        }
    }
#endif
    throw fail_fast( s.c_str() );
}
#endif // defined( gsl_CONFIG_CONTRACT_VIOLATION_THROWS )
#if defined( gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES_WITH_STACKTRACE )
gsl_NORETURN
# if defined( _MSC_VER )
__declspec( noinline )
# elif defined( __GNUC__ )
__attribute__(( noinline ))
# endif
inline void fail_fast_trace( char const * expression, char const * message ) gsl_noexcept
{
    std::cerr << "assertion failed: ";
    if ( message && message[0] )
    {
        std::cerr << message << ": ";
    }
    std::cerr << "`" << expression << "' at:\n";
    std::size_t skip;
# if defined( _MSC_VER ) || defined( __GNUC__ )
    skip = 1;  // the function has a noinline attribute, so we can skip it in the call stack
# else
    skip = 0;  // don't know how to generate a noinline attribute for this compiler, so play it safe
# endif
    std::cerr << std::stacktrace::current( skip ) << std::endl;
    std::terminate();
}
#endif // defined( gsl_CONFIG_CONTRACT_VIOLATION_TERMINATES_WITH_STACKTRACE )
gsl_NORETURN inline void fail_fast_terminate() gsl_noexcept
{
    std::terminate();
}
gsl_NORETURN inline void fail_fast_abort() gsl_noexcept
{
    std::abort();
}

} // namespace detail

// Should be defined by user
gsl_api void fail_fast_assert_handler( char const * expression, char const * message, char const * file, int line );

//
// GSL.util: utilities
//

// Add uncaught_exceptions() for pre-2017 MSVC, GCC and Clang

namespace std17 {

#if gsl_HAVE( UNCAUGHT_EXCEPTIONS )

inline int uncaught_exceptions() gsl_noexcept
{
    return std::uncaught_exceptions();
}

#else // ! gsl_HAVE( UNCAUGHT_EXCEPTIONS )
# if gsl_COMPILER_MS_STL_VERSION

inline int uncaught_exceptions() gsl_noexcept
{
    return static_cast<int>( *reinterpret_cast<unsigned const*>( detail::_getptd() + (sizeof(void *) == 8 ? 0x100 : 0x90 ) ) );
}

# elif gsl_COMPILER_CLANG_VERSION || gsl_COMPILER_GNUC_VERSION || gsl_COMPILER_APPLECLANG_VERSION || gsl_COMPILER_NVHPC_VERSION

inline int uncaught_exceptions() gsl_noexcept
{
    return ( static_cast<int>( *reinterpret_cast<unsigned const *>( reinterpret_cast<unsigned char const *>(detail::__cxa_get_globals()) + sizeof(void *) ) ) );
}

# endif
#endif

} // namespace std17

namespace std11 {

#if gsl_HAVE( UNCAUGHT_EXCEPTIONS ) || gsl_COMPILER_MS_STL_VERSION || gsl_COMPILER_CLANG_VERSION || gsl_COMPILER_GNUC_VERSION || gsl_COMPILER_APPLECLANG_VERSION || gsl_COMPILER_NVHPC_VERSION
// Retain alias for backward compatibility
using ::gsl_lite::std17::uncaught_exceptions;
#endif

} // namespace std11

template< class EnumT >
struct flags
{
    typedef typename std11::remove_reference< EnumT >::type value_type;

    EnumT value;

    gsl_api gsl_constexpr flags( EnumT _value )
        : value( _value )
    {
    }

    gsl_api gsl_constexpr operator EnumT() const gsl_noexcept
    {
        return value;
    }

    gsl_api gsl_explicit gsl_constexpr operator bool() const gsl_noexcept
    {
        return value != value_type( );
    }
};

#if gsl_STDLIB_CPP11_110

gsl_DISABLE_MSVC_WARNINGS( 4702 ) // unreachable code

template< class F >
class final_action
# if gsl_HAVE( OVERRIDE_FINAL )
final
# endif
{
public:
    explicit final_action( F action ) gsl_noexcept
        : action_( std::move( action ) ), invoke_( true )
    {
    }

        // We only provide the move constructor for legacy defaults, or if we cannot rely on C++17 guaranteed copy elision.
# if ! gsl_CPP17_OR_GREATER
    final_action( final_action && other ) gsl_noexcept
        : action_( std::move( other.action_ ) )
        , invoke_( other.invoke_ )
    {
        other.invoke_ = false;
    }
# endif // ! gsl_CPP17_OR_GREATER

    gsl_SUPPRESS_MSGSL_WARNING(f.6)
    ~final_action() gsl_noexcept
    {
            // Let the optimizer figure out that this check is redundant.
        if ( invoke_ )
        {
            action_();
        }
    }

gsl_is_delete_access:
    final_action( final_action const & ) gsl_is_delete;
    final_action & operator=( final_action const & ) gsl_is_delete;
    final_action & operator=( final_action && ) gsl_is_delete;

private:
    F action_;
    gsl_MAYBE_UNUSED_MEMBER bool invoke_;  // member is defined unconditionally so as not to have ABI depend on C++ language support
};

template< class F >
gsl_NODISCARD inline final_action<typename std::decay<F>::type>
finally( F && action ) gsl_noexcept
{
    return final_action<typename std::decay<F>::type>( std::forward<F>( action ) );
}

# if gsl_FEATURE( EXPERIMENTAL_RETURN_GUARD )

template< class F >
class final_action_return
#  if gsl_HAVE( OVERRIDE_FINAL )
final
#  endif
{
public:
    explicit final_action_return( F action ) gsl_noexcept
        : action_( std::move( action ) )
        , exception_count_( std17::uncaught_exceptions() )
    {
    }

        // We only provide the move constructor if we cannot rely on C++17 guaranteed copy elision.
#  if ! gsl_CPP17_OR_GREATER
    final_action_return( final_action_return && other ) gsl_noexcept
        : action_( std::move( other.action_ ) )
        , exception_count_( other.exception_count_ )
    {
        other.exception_count_ = -1;  // abuse member as special "no-invoke" marker
    }
#  endif // ! gsl_CPP17_OR_GREATER

    gsl_SUPPRESS_MSGSL_WARNING(f.6)
    ~final_action_return() gsl_noexcept
    {
        if ( std17::uncaught_exceptions() == exception_count_ )  // always false if `exception_count_ == -1`
        {
            action_();
        }
    }

gsl_is_delete_access:
    final_action_return( final_action_return const & ) gsl_is_delete;
    final_action_return & operator=( final_action_return const & ) gsl_is_delete;
    final_action_return & operator=( final_action_return && ) gsl_is_delete;

private:
    F action_;
    int exception_count_;
};
template< class F >
class final_action_error
#  if gsl_HAVE( OVERRIDE_FINAL )
final
#  endif
{
public:
    explicit final_action_error( F action ) gsl_noexcept
        : action_( std::move( action ) )
        , exception_count_( std17::uncaught_exceptions() )
    {
    }

        // We only provide the move constructor if we cannot rely on C++17 guaranteed copy elision.
#  if ! gsl_CPP17_OR_GREATER
    final_action_error( final_action_error && other ) gsl_noexcept
        : action_( std::move( other.action_ ) )
        , exception_count_( other.exception_count_ )
    {
        other.exception_count_ = -1;  // abuse member as special "no-invoke" marker
    }
#  endif // ! gsl_CPP17_OR_GREATER

    gsl_SUPPRESS_MSGSL_WARNING(f.6)
    ~final_action_error() gsl_noexcept
    {
        if ( exception_count_ != -1 )  // abuse member as special "no-invoke" marker
        {
            if ( std17::uncaught_exceptions() != exception_count_ )
            {
                action_();
            }
        }
    }

gsl_is_delete_access:
    final_action_error( final_action_error const & ) gsl_is_delete;
    final_action_error & operator=( final_action_error const & ) gsl_is_delete;
    final_action_error & operator=( final_action_error && ) gsl_is_delete;

private:
    F action_;
    int exception_count_;
};

template< class F >
gsl_NODISCARD inline final_action_return<typename std::decay<F>::type>
on_return( F && action ) gsl_noexcept
{
    return final_action_return<typename std::decay<F>::type>( std::forward<F>( action ) );
}

template< class F >
gsl_NODISCARD inline final_action_error<typename std::decay<F>::type>
on_error( F && action ) gsl_noexcept
{
    return final_action_error<typename std::decay<F>::type>( std::forward<F>( action ) );
}

# endif // gsl_FEATURE( EXPERIMENTAL_RETURN_GUARD )

gsl_RESTORE_MSVC_WARNINGS()

#endif // gsl_STDLIB_CPP11_110

#if gsl_STDLIB_CPP11_120

template< class T, class U >
gsl_NODISCARD gsl_api inline gsl_constexpr T
narrow_cast( U && u ) gsl_noexcept
{
    return static_cast<T>( std::forward<U>( u ) );
}

#else // ! gsl_STDLIB_CPP11_120

template< class T, class U >
gsl_api inline T
narrow_cast( U u ) gsl_noexcept
{
    return static_cast<T>( u );
}

#endif // gsl_STDLIB_CPP11_120

struct narrowing_error : public std::exception
{
    char const * what() const gsl_noexcept
#if gsl_HAVE( OVERRIDE_FINAL )
    override
#endif
    {
        return "narrowing_error";
    }
};


#if gsl_CONFIG( NARROW_THROWS_ON_TRUNCATION )
# define  gsl_NARROW_FAIL_()  throw narrowing_error()
#else // ! gsl_CONFIG( NARROW_THROWS_ON_TRUNCATION )
# if gsl_DEVICE_CODE
#  define gsl_NARROW_FAIL_()  gsl_TRAP_()
# else // host code
#  define gsl_NARROW_FAIL_()  ::gsl_lite::detail::fail_fast_terminate()
# endif
#endif // gsl_CONFIG( NARROW_THROWS_ON_TRUNCATION )

#if gsl_CONFIG( NARROW_THROWS_ON_TRUNCATION )
# define gsl_NARROW_API_
#else // ! gsl_CONFIG( NARROW_THROWS_ON_TRUNCATION )
# define gsl_NARROW_API_  gsl_api
#endif // gsl_CONFIG( NARROW_THROWS_ON_TRUNCATION )

namespace detail {

#if gsl_HAVE( TYPE_TRAITS )
template< class T, bool IsEnum = std::is_enum<T>::value >
struct unwrap_enum { typedef T type; };
template< class T >
struct unwrap_enum<T, true> : std::underlying_type<T> { };
#endif // gsl_HAVE( TYPE_TRAITS )

} // namespace detail

#if gsl_BASELINE_CPP20_

namespace detail {

template< class T, class U >
concept static_castable = requires( U value ) { static_cast<T>( value ); };

template< class T, class U >
constexpr bool are_all_values_representable_impl()
{
    if constexpr ( std::same_as<T, U> ) return true;
    else if constexpr ( std::integral<T> && std::integral<U> )
    {
        return ( std::is_signed_v<T> == std::is_signed_v<U> && std::numeric_limits<T>::digits >= std::numeric_limits<U>::digits )
            || ( std::is_signed_v<T> /*&& ! std::is_signed_v<U>*/ && std::numeric_limits<T>::digits > std::numeric_limits<U>::digits );
    }
    else return false;  // no assumptions can be made about other types, including floating-point types, because the standard does not mandate a specific representation
}

template< class T, class U >
constexpr bool are_all_values_representable = are_all_values_representable_impl<T, U>();

} // namespace detail

template< class T, class U >
requires detail::static_castable<T, U> && detail::static_castable<U, T>
[[nodiscard]] gsl_NARROW_API_ constexpr inline T
narrow( U u )
{
    static_assert( ! ( std::same_as<T, bool> || std::same_as<U, bool> ), "narrow<>() does not support bool" );
# if ! gsl_HAVE( EXCEPTIONS ) && gsl_CONFIG( NARROW_THROWS_ON_TRUNCATION )
    static_assert( detail::dependent_false< T >::value,
        "According to the GSL specification, narrow<>() throws an exception of type narrowing_error on truncation. Therefore "
        "it cannot be used if exceptions are disabled. Consider using narrow_failfast<>() instead which raises a precondition "
        "violation if the given value cannot be represented in the target type." );
# endif

    using TT = typename detail::unwrap_enum<T>::type;
    using UU = typename detail::unwrap_enum<U>::type;

    T t = static_cast<T>( u );
    if constexpr ( ! detail::are_all_values_representable<TT, UU> )
    {
        if constexpr ( std::is_signed_v<TT> && std::is_signed_v<UU> )
        {
            if ( static_cast<U>( t ) != u || ( ( TT( t ) < 0 ) != ( UU( u ) < 0 ) ) ) gsl_NARROW_FAIL_();
        }
        else if constexpr ( std::is_signed_v<TT> )
        {
            if ( static_cast<U>( t ) != u || TT( t ) < 0 ) gsl_NARROW_FAIL_();
        }
        else if constexpr ( std::is_signed_v<UU> )
        {
            if ( static_cast<U>( t ) != u || UU( u ) < 0 ) gsl_NARROW_FAIL_();
        }
        else
        {
            if ( static_cast<U>( t ) != u ) gsl_NARROW_FAIL_();
        }
    }
    return t;
}

template< class T, class U >
requires detail::static_castable<T, U> && detail::static_castable<U, T>
[[nodiscard]] gsl_api constexpr inline T
narrow_failfast( U u, [[maybe_unused]] std::source_location const & loc = std::source_location::current() )
{
    static_assert( ! ( std::same_as<T, bool> || std::same_as<U, bool> ), "narrow_failfast<>() does not support bool" );

    using TT = typename detail::unwrap_enum<T>::type;
    using UU = typename detail::unwrap_enum<U>::type;

    T t = static_cast<T>( u );
    if constexpr ( ! detail::are_all_values_representable<TT, UU> )
    {
        if constexpr ( std::is_signed_v<TT> && std::is_signed_v<UU> )
        {
            gsl_AssertAt( loc, static_cast<U>( t ) == u && ( ( TT( t ) < 0 ) == ( UU( u ) < 0 ) ) );
        }
        else if constexpr ( std::is_signed_v<TT> )
        {
            gsl_AssertAt( loc, static_cast<U>( t ) == u && TT( t ) >= 0 );
        }
        else if constexpr ( std::is_signed_v<UU> )
        {
            gsl_AssertAt( loc, static_cast<U>( t ) == u && UU( u ) >= 0 );
        }
        else
        {
            gsl_AssertAt( loc, static_cast<U>( t ) == u );
        }
    }
    return t;
}

#else // ! gsl_BASELINE_CPP20_

# if gsl_HAVE( TYPE_TRAITS )

namespace detail {

template< class T, class U >
struct is_same_signedness : public std::integral_constant<bool, std::is_signed<T>::value == std::is_signed<U>::value> {};

#  if gsl_COMPILER_NVCC_VERSION || gsl_COMPILER_NVHPC_VERSION
// We do this to circumvent NVCC warnings about pointless unsigned comparisons with 0.
template< class T >
gsl_constexpr gsl_api bool is_negative( T value, std::true_type /*isSigned*/ ) gsl_noexcept
{
    return value < T();
}
template< class T >
gsl_constexpr gsl_api bool is_negative( T /*value*/, std::false_type /*isUnsigned*/ ) gsl_noexcept
{
    return false;
}
template< class T, class U >
gsl_constexpr gsl_api bool have_same_sign( T, U, std::true_type /*isSameSignedness*/ ) gsl_noexcept
{
    return true;
}
template< class T, class U >
gsl_constexpr gsl_api bool have_same_sign( T t, U u, std::false_type /*isSameSignedness*/ ) gsl_noexcept
{
    return detail::is_negative( t, std::is_signed<T>() ) == detail::is_negative( u, std::is_signed<U>() );
}
#  endif // gsl_COMPILER_NVCC_VERSION || gsl_COMPILER_NVHPC_VERSION

} // namespace detail

# endif // gsl_HAVE( TYPE_TRAITS )

template< class T, class U >
gsl_NODISCARD gsl_constexpr14 gsl_NARROW_API_ inline
gsl_ENABLE_IF_R_( ( std::is_arithmetic<T>::value || std::is_enum<T>::value ) && ( std::is_arithmetic<U>::value || std::is_enum<U>::value ), T )
narrow( U u )
{
# if ! gsl_HAVE( EXCEPTIONS ) && gsl_CONFIG( NARROW_THROWS_ON_TRUNCATION )
    gsl_STATIC_ASSERT_( detail::dependent_false< T >::value,
        "According to the GSL specification, narrow<>() throws an exception of type narrowing_error on truncation. Therefore "
        "it cannot be used if exceptions are disabled. Consider using narrow_failfast<>() instead which raises a precondition "
        "violation if the given value cannot be represented in the target type." );
# endif

    T t = static_cast<T>( u );

# if gsl_HAVE( TYPE_TRAITS )
    typedef typename detail::unwrap_enum<T>::type TT;
    typedef typename detail::unwrap_enum<U>::type UU;

#  if gsl_COMPILER_NVCC_VERSION || gsl_COMPILER_NVHPC_VERSION
    if ( static_cast<U>( t ) != u
        || ! detail::have_same_sign( TT( t ), UU( u ), detail::is_same_signedness<TT, UU>() ) )
#  else
    gsl_SUPPRESS_MSVC_WARNING( 4127, "conditional expression is constant" )
    if ( static_cast<U>( t ) != u
        || ( ! detail::is_same_signedness<TT, UU>::value && ( TT( t ) < TT() ) != ( UU( u ) < UU() ) ) )
#  endif
# else
    // Don't assume T() works:
    gsl_SUPPRESS_MSVC_WARNING( 4127, "conditional expression is constant" )
#  if gsl_COMPILER_NVHPC_VERSION
    // Suppress: pointless comparison of unsigned integer with zero.
#   pragma diag_suppress 186
#  endif
    if ( static_cast<U>( t ) != u
        || ( t < 0 ) != ( u < 0 ) )
#  if gsl_COMPILER_NVHPC_VERSION
    // Restore: pointless comparison of unsigned integer with zero.
#   pragma diag_default 186
#  endif

# endif
    {
# if gsl_CONFIG( NARROW_THROWS_ON_TRUNCATION )
        throw narrowing_error();
# else // ! gsl_CONFIG( NARROW_THROWS_ON_TRUNCATION )
#  if gsl_DEVICE_CODE
        gsl_TRAP_();
#  else // host code
        std::terminate();
#  endif
# endif // gsl_CONFIG( NARROW_THROWS_ON_TRUNCATION )
    }

    return t;
}
# if gsl_HAVE( TYPE_TRAITS )
template< class T, class U  >
gsl_NODISCARD gsl_constexpr14 gsl_NARROW_API_ inline
gsl_ENABLE_IF_R_( ! ( ( std::is_arithmetic<T>::value || std::is_enum<T>::value ) && ( std::is_arithmetic<U>::value || std::is_enum<U>::value ) ), T )
narrow( U u )
{
#  if ! gsl_HAVE( EXCEPTIONS ) && gsl_CONFIG( NARROW_THROWS_ON_TRUNCATION )
    gsl_STATIC_ASSERT_( detail::dependent_false< T >::value,
        "According to the GSL specification, narrow<>() throws an exception of type narrowing_error on truncation. Therefore "
        "it cannot be used if exceptions are disabled. Consider using narrow_failfast<>() instead which raises a precondition "
        "violation if the given value cannot be represented in the target type." );
#  endif

    T t = static_cast<T>( u );

    if ( static_cast<U>( t ) != u )
    {
#  if gsl_CONFIG( NARROW_THROWS_ON_TRUNCATION )
        throw narrowing_error();
#  else // ! gsl_CONFIG( NARROW_THROWS_ON_TRUNCATION )
#   if gsl_DEVICE_CODE
        gsl_TRAP_();
#   else // host code
        std::terminate();
#   endif
#  endif // gsl_CONFIG( NARROW_THROWS_ON_TRUNCATION )
    }

    return t;
}
# endif // gsl_HAVE( TYPE_TRAITS )

template< class T, class U >
gsl_NODISCARD gsl_api gsl_constexpr14 inline
gsl_ENABLE_IF_R_( ( std::is_arithmetic<T>::value || std::is_enum<T>::value ) && ( std::is_arithmetic<U>::value || std::is_enum<U>::value ), T )
narrow_failfast( U u )
{
    T t = static_cast<T>( u );

# if gsl_HAVE( TYPE_TRAITS )
    typedef typename detail::unwrap_enum<T>::type TT;
    typedef typename detail::unwrap_enum<U>::type UU;

#  if gsl_COMPILER_NVCC_VERSION || gsl_COMPILER_NVHPC_VERSION
    gsl_Assert( static_cast<U>( t ) == u
        && ::gsl_lite::detail::have_same_sign( TT( t ), UU( u ), ::gsl_lite::detail::is_same_signedness<TT, UU>() ) );
#  else
    gsl_SUPPRESS_MSVC_WARNING( 4127, "conditional expression is constant" )
    gsl_Assert( static_cast<U>( t ) == u
        && ( ::gsl_lite::detail::is_same_signedness<TT, UU>::value || ( TT( t ) < TT() ) == ( UU( u ) < UU() ) ) );
#  endif
# else
    // Don't assume T() works:
    gsl_SUPPRESS_MSVC_WARNING( 4127, "conditional expression is constant" )
#  if gsl_COMPILER_NVHPC_VERSION
    // Suppress: pointless comparison of unsigned integer with zero.
#   pragma diag_suppress 186
#  endif
    gsl_Assert( static_cast<U>( t ) == u
        && ( t < 0 ) == ( u < 0 ) );
#  if gsl_COMPILER_NVHPC_VERSION
    // Restore: pointless comparison of unsigned integer with zero.
#   pragma diag_default 186
#  endif
# endif

    return t;
}
# if gsl_HAVE( TYPE_TRAITS )
template< class T, class U >
gsl_NODISCARD gsl_api gsl_constexpr14 inline
gsl_ENABLE_IF_R_( ! ( ( std::is_arithmetic<T>::value || std::is_enum<T>::value ) && ( std::is_arithmetic<U>::value || std::is_enum<U>::value ) ), T )
narrow_failfast( U u )
{
    T t = static_cast<T>( u );
    gsl_Assert( static_cast<U>( t ) == u );
    return t;
}
# endif // gsl_HAVE( TYPE_TRAITS )

#endif // gsl_BASELINE_CPP20_

#undef gsl_NARROW_FAIL_
#undef gsl_NARROW_API_

//
// at() - Bounds-checked way of accessing static arrays, std::array, std::vector.
//

#if gsl_BASELINE_CPP20_

template< class T >
[[nodiscard]] gsl_api inline constexpr auto
at( T&& range, size_t pos, std::source_location const & loc = std::source_location::current() )
-> decltype( static_cast<T&&>( range )[pos] )
{
    gsl_AssertAt( loc, pos < std::size( range ) );
    return static_cast<T&&>( range )[pos];
}
template< class T >
gsl_NODISCARD gsl_api inline const gsl_constexpr14 T
at( std::initializer_list<T> list, size_t pos, std::source_location const & loc = std::source_location::current() )
{
    gsl_AssertAt( loc, pos < list.size() );
    return *( list.begin() + pos );
}

#else // ! gsl_BASELINE_CPP20_

template< class T, size_t N >
gsl_NODISCARD gsl_api inline gsl_constexpr14 T &
at( T(&arr)[N], size_t pos )
{
    gsl_Expects( pos < N );
    return arr[pos];
}

template< class Container >
gsl_NODISCARD gsl_api inline gsl_constexpr14 typename Container::value_type &
at( Container & cont, size_t pos )
{
    gsl_Expects( pos < cont.size() );
    return cont[pos];
}

template< class Container >
gsl_NODISCARD gsl_api inline gsl_constexpr14 typename Container::value_type const &
at( Container const & cont, size_t pos )
{
    gsl_Expects( pos < cont.size() );
    return cont[pos];
}

# if gsl_HAVE( INITIALIZER_LIST )
template< class T >
gsl_NODISCARD gsl_api inline const gsl_constexpr14 T
at( std::initializer_list<T> cont, size_t pos )
{
    gsl_Expects( pos < cont.size() );
    return *( cont.begin() + pos );
}
# endif

#if gsl_FEATURE( SPAN )
template< class T, gsl_CONFIG_SPAN_INDEX_TYPE Extent >
gsl_NODISCARD gsl_api inline gsl_constexpr14 T &
at( span<T, Extent> s, size_t pos )
{
    return s[ pos ];
}
#endif

#endif // gsl_USE_MODERN_IMPLEMENTATION_

//
// GSL.views: views
//

//
// not_null<> - Wrap any indirection and enforce non-null.
//

#if __cpp_lib_concepts
template< class T > constexpr bool is_nullable_v = std::is_assignable_v<std::remove_cv_t<T>&, std::nullptr_t>;
template< class T > struct is_nullable : std::bool_constant<is_nullable_v<T>> { };
template< class T > concept nullable = is_nullable_v<T>;
#else // ! __cpp_lib_concepts
# if gsl_HAVE( TYPE_TRAITS )
template< class T > struct is_nullable : std::is_assignable< typename std::remove_cv< T >::type &, std::nullptr_t > { };
#  if gsl_CPP14_OR_GREATER
template< class T > constexpr bool is_nullable_v = is_nullable< T >::value;
#  endif // gsl_CPP14_OR_GREATER
# endif // gsl_HAVE( TYPE_TRAITS )
#endif

template< gsl_CONSTRAINT( nullable ) T >
class not_null;

namespace detail {

// helper class to figure out whether a pointer has an element type
#if gsl_STDLIB_CPP11_OR_GREATER && gsl_HAVE( EXPRESSION_SFINAE )
// Avoid SFINAE for unary `operator*` (doesn't work for `std::unique_ptr<>` and the like) if an `element_type` member exists.
template< class T, class E = void >
struct has_element_typedef : std11::false_type { };
template< class T >
struct has_element_typedef< T, std17::void_t< typename T::element_type > > : std11::true_type { };
template< class T, class E = void >
struct has_element_type_ : std11::false_type { };
template< class T >
struct has_element_type_< T, std17::void_t< decltype( *std::declval<T>() ) > > : std11::true_type { };
template< class T, class E = void >
struct has_element_type : std17::disjunction< has_element_typedef< T >, has_element_type_< T > > { };
# else // a.k.a. ! ( gsl_STDLIB_CPP11_OR_GREATER && gsl_HAVE( EXPRESSION_SFINAE ) )
// Without C++11 and expression SFINAE, just assume that non-pointer types (e.g. smart pointers) have an `element_type` member
template< class T, class E = void >
struct has_element_type : std11::true_type { };
# endif // gsl_STDLIB_CPP11_OR_GREATER && gsl_HAVE( EXPRESSION_SFINAE )

// helper class to figure out the pointed-to type of a pointer
#if gsl_STDLIB_CPP11_OR_GREATER
template< class T, class E = void >
struct element_type_helper
{
    // For types without a member element_type (this could handle typed raw pointers but not `void*`)
    typedef typename std::remove_reference< decltype( *std::declval<T>() ) >::type type;
};
template< class T >
struct element_type_helper< T, std17::void_t< typename T::element_type > >
{
    // For types with a member element_type
    typedef typename T::element_type type;
};
#else // ! gsl_STDLIB_CPP11_OR_GREATER
// Pre-C++11, we cannot have `decltype`, so we cannot handle non-pointer types without a member `element_type`
template< class T, class E = void >
struct element_type_helper
{
    typedef typename T::element_type type;
};
#endif // gsl_STDLIB_CPP11_OR_GREATER
template< class T >
struct element_type_helper< T * >
{
    typedef T type;
};
#if gsl_HAVE( ALIAS_TEMPLATE )
template< class T >
using element_type_t = typename element_type_helper<T>::type;
#endif

template< class T >
struct is_not_null_or_bool_oracle : std11::false_type { };
template< class T >
struct is_not_null_or_bool_oracle< not_null<T> > : std11::true_type { };
template<>
struct is_not_null_or_bool_oracle< bool > : std11::true_type { };
#if gsl_CPP14_OR_GREATER
template< class T >
constexpr bool is_not_null_or_bool_oracle_v = is_not_null_or_bool_oracle<T>::value;
#endif

#if gsl_BASELINE_CPP20_
template< class T >
constexpr bool has_trivial_move_v = std::is_trivially_move_constructible_v< T > && std::is_trivially_move_assignable_v< T >;
template< class T >
struct has_trivial_move : std::bool_constant<has_trivial_move_v<T>> { };
#elif gsl_HAVE( TYPE_TRAITS )
template< class T >
struct has_trivial_move : std17::conjunction< std::is_trivially_move_constructible< T >, std::is_trivially_move_assignable< T > > { };
# else // ! gsl_HAVE( TYPE_TRAITS )
template< class T >
struct has_trivial_move : std11::false_type { };
template< class T >
struct has_trivial_move<T*> : std11::true_type { };
#endif

#if gsl_HAVE( TYPE_TRAITS )
template< class T >
struct is_copyable : std17::conjunction< std::is_copy_constructible<T>, std::is_copy_assignable<T> > { };
# if gsl_HAVE( UNIQUE_PTR ) && gsl_BETWEEN( gsl_COMPILER_MSVC_VERSION, 1, 140 )
// Type traits are buggy in VC++ 2013, so we explicitly declare `unique_ptr<>` non-copyable.
template< class T, class Deleter >
struct is_copyable< std::unique_ptr< T, Deleter > > : std11::false_type { };
# endif
#else // ! gsl_HAVE( TYPE_TRAITS )
template< class T >
struct is_copyable : std11::true_type { };
#endif

template< class T > struct is_void : std11::false_type { };
template< > struct is_void< void > : std11::true_type { };

template< class T > struct is_void_ptr : is_void< typename detail::element_type_helper< T >::type > { };

template< class T > struct is_dereferencable : std17::conjunction< has_element_type< T >, std17::negation< is_void_ptr< T > > > { };
#if gsl_CPP14_OR_GREATER
template< class T > constexpr bool is_dereferencable_v = is_dereferencable<T>::value;
#endif // gsl_CPP14_OR_GREATER

#if ! gsl_BASELINE_CPP20_
template< class T, bool IsCopyable = detail::is_copyable< T >::value, bool HasTrivialMove = detail::has_trivial_move< T >::value >
struct not_null_data;
# if gsl_HAVE( MOVE_FORWARD )
template< class T >
struct not_null_data< T, false, false >
{
    T ptr_;

    gsl_api gsl_constexpr14 not_null_data( T && _ptr ) gsl_noexcept
    : ptr_( std::move( _ptr ) )
    {
    }

    gsl_api gsl_constexpr14 not_null_data( not_null_data && other )
    gsl_noexcept_not_testing  // we want to be nothrow-movable despite the assertion
    : ptr_( std::move( other.ptr_ ) )
    {
        gsl_Assert( ptr_ != gsl_nullptr );
    }
    gsl_api gsl_constexpr14 not_null_data & operator=( not_null_data && other )
    gsl_noexcept_not_testing  // we want to be nothrow-movable despite the assertion
    {
        gsl_Assert( other.ptr_ != gsl_nullptr || &other == this );
        ptr_ = std::move( other.ptr_ );
        return *this;
    }

gsl_is_delete_access:
    not_null_data( not_null_data const & ) gsl_is_delete;
    not_null_data & operator=( not_null_data const & ) gsl_is_delete;
};
# endif // gsl_HAVE( MOVE_FORWARD )
template< class T >
struct not_null_data< T, true, false >
{
    T ptr_;

    gsl_api gsl_constexpr14 not_null_data( T const & _ptr ) gsl_noexcept
    : ptr_( _ptr )
    {
    }

# if gsl_HAVE( MOVE_FORWARD )
    gsl_api gsl_constexpr14 not_null_data( T && _ptr ) gsl_noexcept
    : ptr_( std::move( _ptr ) )
    {
    }

    gsl_api gsl_constexpr14 not_null_data( not_null_data && other )
    gsl_noexcept_not_testing  // we want to be nothrow-movable despite the assertion
    : ptr_( std::move( other.ptr_ ) )
    {
        gsl_Assert( ptr_ != gsl_nullptr );
    }
    gsl_api gsl_constexpr14 not_null_data & operator=( not_null_data && other )
    gsl_noexcept_not_testing  // we want to be nothrow-movable despite the assertion
    {
        gsl_Assert( other.ptr_ != gsl_nullptr || &other == this );
        ptr_ = std::move( other.ptr_ );
        return *this;
    }
# endif // gsl_HAVE( MOVE_FORWARD )

    gsl_api gsl_constexpr14 not_null_data( not_null_data const & other )
    : ptr_( other.ptr_ )
    {
        gsl_Assert( ptr_ != gsl_nullptr );
    }
    gsl_api gsl_constexpr14 not_null_data & operator=( not_null_data const & other )
    {
        gsl_Assert( other.ptr_ != gsl_nullptr );
        ptr_ = other.ptr_;
        return *this;
    }
};
template< class T, bool IsCopyable >
struct not_null_data< T, IsCopyable, true >
{
    T ptr_;

    gsl_api gsl_constexpr14 not_null_data( T _ptr ) gsl_noexcept
    : ptr_( _ptr )
    {
    }
};
#endif // ! gsl_BASELINE_CPP20_

template< class T >
struct not_null_accessor;

#if gsl_BASELINE_CPP20_
template< class T, bool HasElementType = has_element_type<T>::value >
struct not_null_base
{
    using element_type = element_type_t<T>;
};
template< class T >
struct not_null_base< T, false >
{
};
#else // ! gsl_BASELINE_CPP20_
template< class T, class Derived, bool HasElementType = has_element_type< T >::value >
struct not_null_elem
{
    typedef typename element_type_helper<T>::type element_type;

#if gsl_CONFIG( TRANSPARENT_NOT_NULL )
    gsl_NODISCARD gsl_api gsl_constexpr14 element_type *
    get() const
    {
        return not_null_accessor<T>::get_checked( static_cast< Derived const & >( *this ) ).get();
    }
#endif // gsl_CONFIG( TRANSPARENT_NOT_NULL )
};
template< class T, class Derived >
struct not_null_elem< T, Derived, false >
{
};
template< class T, class Derived = not_null<T>, bool IsDereferencable = is_dereferencable< T >::value >
struct gsl_EMPTY_BASES_ not_null_base
    : not_null_elem< T, Derived >
{
    gsl_NODISCARD gsl_api gsl_constexpr14 typename element_type_helper<T>::type &
    operator*() const
    {
        return *not_null_accessor<T>::get_checked( static_cast< Derived const & >( *this ) );
    }
};
template< class T, class Derived >
struct gsl_EMPTY_BASES_ not_null_base< T, Derived, false >  // e.g. `void*`, `std::function<>`
    : not_null_elem< T, Derived >
{
};
#endif // ! gsl_BASELINE_CPP20_

} // namespace detail

template< gsl_CONSTRAINT( nullable ) T >
class
gsl_EMPTY_BASES_  // not strictly needed, but will become necessary if we add more base classes
not_null : public detail::not_null_base<T>
{
private:
#if gsl_BASELINE_CPP20_
    T ptr_;
#else // ! gsl_BASELINE_CPP20_
    detail::not_null_data<T> data_;
#endif

    // need to access `not_null<U>::data_`
    template< class U >
    friend struct detail::not_null_accessor;
    template< gsl_CONSTRAINT( nullable ) U >
    friend class not_null;

#if ! gsl_BASELINE_CPP20_
    typedef detail::not_null_accessor<T> accessor;
#endif // ! gsl_BASELINE_CPP20_

#if gsl_HAVE( TYPE_TRAITS )
    static_assert( ! std::is_reference<T>::value, "T may not be a reference type" );
    static_assert( ! std::is_const<T>::value && ! std::is_volatile<T>::value, "T may not be cv-qualified" );
# ifndef __cpp_lib_concepts
    static_assert( is_nullable<T>::value, "T must be a nullable type" );
# endif
#endif

public:
#if gsl_BASELINE_CPP20_
    template< class U >
    requires std::is_constructible_v<T, U> && ( ! std::is_convertible_v<U, T> )
    gsl_api constexpr explicit not_null( U other, [[maybe_unused]] std::source_location const & loc = std::source_location::current() )
    : ptr_( std::move( other ) )
    {
        if constexpr ( nullable<U> )
        {
            gsl_AssertAt( loc, ptr_ != nullptr );
        }
    }
#endif // gsl_BASELINE_CPP20_

#if gsl_CONFIG( NOT_NULL_EXPLICIT_CTOR )
# if gsl_BASELINE_CPP20_
    template< class U >
    requires std::is_convertible_v<U, T> && std::is_function_v<U>
    gsl_api constexpr /*implicit*/ not_null( U const & other, std::source_location const & = std::source_location::current() ) noexcept
    : ptr_( other )
    {
    }
    template< class U >
    requires std::is_convertible_v<U, T> && ( ! std::is_function_v<U> ) && nullable<U>
    gsl_api constexpr explicit not_null( U other, [[maybe_unused]] std::source_location const & loc = std::source_location::current() )
    : ptr_( std::move( other ) )
    {
        gsl_AssertAt( loc, ptr_ != nullptr );
    }
    template< class U >
    requires std::is_convertible_v<U, T> && ( ! std::is_function_v<U> ) && ( ! nullable<U> )
    gsl_api constexpr /*implicit*/ not_null( U other, [[maybe_unused]] std::source_location const & loc = std::source_location::current() )
    : ptr_( std::move( other ) )
    {
    }
# elif gsl_HAVE( MOVE_FORWARD )
    template< class U
    // In Clang 3.x, `is_constructible<not_null<unique_ptr<X>>, unique_ptr<X>>` tries to instantiate the copy constructor of `unique_ptr<>`, triggering an error.
    // Note that Apple Clang's `__clang_major__` etc. are different from regular Clang.
#  if gsl_HAVE( TYPE_TRAITS ) && gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG ) && ! gsl_BETWEEN( gsl_COMPILER_CLANG_VERSION, 1, 400 ) && ! gsl_BETWEEN( gsl_COMPILER_APPLECLANG_VERSION, 1, 1001 )
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( std::is_constructible<T, U>::value && is_nullable<U>::value ))
#  endif
    >
    gsl_api gsl_constexpr14 explicit not_null( U other )
    : data_( T( std::move( other ) ) )
    {
        gsl_Expects( data_.ptr_ != gsl_nullptr );
    }
#  if gsl_HAVE( TYPE_TRAITS ) && gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG ) && ! gsl_BETWEEN( gsl_COMPILER_CLANG_VERSION, 1, 400 ) && ! gsl_BETWEEN( gsl_COMPILER_APPLECLANG_VERSION, 1, 1001 )
    // Define the non-explicit constructors for non-nullable arguments only if the explicit constructor has a SFINAE constraint.
    template< class U
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( std::is_constructible<T, U>::value && std::is_function<U>::value ))
    >
    gsl_api gsl_constexpr14 /*implicit*/ not_null( U const & other )
    : data_( T( other ) )
    {
    }
    template< class U
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( std::is_constructible<T, U>::value && ! std::is_function<U>::value && ! is_nullable<U>::value ))
    >
    gsl_api gsl_constexpr14 /*implicit*/ not_null( U other )
    : data_( T( std::move( other ) ) )
    {
    }
#  endif // gsl_HAVE( TYPE_TRAITS ) && gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG ) && ! gsl_BETWEEN( gsl_COMPILER_CLANG_VERSION, 1, 400 ) && ! gsl_BETWEEN( gsl_COMPILER_APPLECLANG_VERSION, 1, 1001 )
# else // a.k.a. ! gsl_HAVE( MOVE_FORWARD )
    template< class U >
    gsl_api gsl_constexpr14 explicit not_null( U const & other )
    : data_( T( other ) )
    {
        gsl_Expects( data_.ptr_ != gsl_nullptr );
    }
# endif // gsl_HAVE( MOVE_FORWARD )
#else // a.k.a. !gsl_CONFIG( NOT_NULL_EXPLICIT_CTOR )
# if gsl_BASELINE_CPP20_
    template< class U >
    requires std::is_convertible_v<U, T>
    gsl_api constexpr /*implicit*/ not_null( U other, [[maybe_unused]] std::source_location const & loc = std::source_location::current() )
    : ptr_( std::move( other ) )
    {
        if constexpr ( nullable<U> )
        {
            gsl_AssertAt( loc, ptr_ != nullptr );
        }
    }
# elif gsl_HAVE( MOVE_FORWARD )
    // In Clang 3.x, `is_constructible<not_null<unique_ptr<X>>, unique_ptr<X>>` tries to instantiate the copy constructor of `unique_ptr<>`, triggering an error.
    // Note that Apple Clang's `__clang_major__` etc. are different from regular Clang.
#  if gsl_HAVE( TYPE_TRAITS ) && gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG ) && ! gsl_BETWEEN( gsl_COMPILER_CLANG_VERSION, 1, 400 ) && ! gsl_BETWEEN( gsl_COMPILER_APPLECLANG_VERSION, 1, 1001 )
    template< class U
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( std::is_constructible<T, U>::value && !std::is_convertible<U, T>::value ))
    >
    gsl_api gsl_constexpr14 explicit not_null( U other )
    : data_( T( std::move( other ) ) )
    {
        gsl_Expects( data_.ptr_ != gsl_nullptr );
    }

    template< class U
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( std::is_convertible<U, T>::value ))
    >
    gsl_api gsl_constexpr14 not_null( U other )
    : data_( std::move( other ) )
    {
        gsl_Expects( data_.ptr_ != gsl_nullptr );
    }
#  else // a.k.a. !( gsl_HAVE( TYPE_TRAITS ) && gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG ) && ! gsl_BETWEEN( gsl_COMPILER_CLANG_VERSION, 1, 400 ) && ! gsl_BETWEEN( gsl_COMPILER_APPLECLANG_VERSION, 1, 1001 )
    // If type_traits are not available, then we can't distinguish `is_convertible<>` and `is_constructible<>`, so we unconditionally permit implicit construction.
    template< class U >
    gsl_api gsl_constexpr14 not_null( U other )
    : data_( T( std::move( other ) ) )
    {
        gsl_Expects( data_.ptr_ != gsl_nullptr );
    }
#  endif // gsl_HAVE( TYPE_TRAITS ) && gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG ) && ! gsl_BETWEEN( gsl_COMPILER_CLANG_VERSION, 1, 400 ) && ! gsl_BETWEEN( gsl_COMPILER_APPLECLANG_VERSION, 1, 1001 )
# else // a.k.a. ! gsl_HAVE( MOVE_FORWARD )
    template< class U >
    gsl_api gsl_constexpr14 not_null( U const & other )
    : data_( T( other ) )
    {
        gsl_Expects( data_.ptr_ != gsl_nullptr );
    }
# endif // gsl_HAVE( MOVE_FORWARD )
#endif // gsl_CONFIG( NOT_NULL_EXPLICIT_CTOR )
#if gsl_BASELINE_CPP20_
    template< class C, std::size_t N >
    requires detail::is_czstring_of<T, C>::value
    gsl_api constexpr /*implicit*/ not_null( C const ( & literal )[N], std::source_location const & = std::source_location::current() ) noexcept
        : ptr_( literal )
    {
    }
#else // ! gsl_BASELINE_CPP20_
    template< class C, std::size_t N
        gsl_ENABLE_IF_NTTP_(( detail::is_czstring_of<T, C>::value ))
    >
    gsl_api gsl_constexpr14 /*implicit*/ not_null( C const ( & literal )[N] ) gsl_noexcept
        : data_( literal )
    {
    }
#endif // gsl_BASELINE_CPP20_

#if gsl_BASELINE_CPP20_
    template< class U >
    requires std::is_constructible_v<T, U> && ( ! std::is_convertible_v<U, T> )
    gsl_api constexpr explicit not_null( not_null<U> other, std::source_location const & loc = std::source_location::current() )
    : ptr_( std::move( other.ptr_ ) )
    {
        if constexpr ( ! detail::has_trivial_move_v<U> )
        {
            gsl_AssertAt( loc, ptr_ != nullptr );
        }
    }
    template< class U >
    requires std::is_convertible_v<U, T>
    gsl_api constexpr /*implicit*/ not_null( not_null<U> other, std::source_location const & loc = std::source_location::current() )
    : ptr_( std::move( other.ptr_ ) )
    {
        if constexpr ( ! detail::has_trivial_move_v<U> )
        {
            gsl_AssertAt( loc, ptr_ != nullptr );
        }
    }
#elif gsl_HAVE( MOVE_FORWARD )
    // In Clang 3.x, `is_constructible<not_null<unique_ptr<X>>, unique_ptr<X>>` tries to instantiate the copy constructor of `unique_ptr<>`, triggering an error.
    // Note that Apple Clang's `__clang_major__` etc. are different from regular Clang.
# if gsl_HAVE( TYPE_TRAITS ) && gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG ) && ! gsl_BETWEEN( gsl_COMPILER_CLANG_VERSION, 1, 400 ) && ! gsl_BETWEEN( gsl_COMPILER_APPLECLANG_VERSION, 1, 1001 )
    template< class U
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( std::is_constructible<T, U>::value && !std::is_convertible<U, T>::value ))
    >
    gsl_api gsl_constexpr14 explicit not_null( not_null<U> other )
    : data_( T( detail::not_null_accessor<U>::get_checked( std::move( other ) ) ) )
    {
    }

    template< class U
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( std::is_convertible<U, T>::value ))
    >
    gsl_api gsl_constexpr14 not_null( not_null<U> other )
    : data_( T( detail::not_null_accessor<U>::get_checked( std::move( other ) ) ) )
    {
    }
# else // a.k.a. ! ( gsl_HAVE( TYPE_TRAITS ) && gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG ) && ! gsl_BETWEEN( gsl_COMPILER_CLANG_VERSION, 1, 400 ) && ! gsl_BETWEEN( gsl_COMPILER_APPLECLANG_VERSION, 1, 1001 )
    // If type_traits are not available, then we can't distinguish `is_convertible<>` and `is_constructible<>`, so we unconditionally permit implicit construction.
    template< class U >
    gsl_api gsl_constexpr14 not_null( not_null<U> other )
    : data_( T( detail::not_null_accessor<U>::get_checked( std::move( other ) ) ) )
    {
        gsl_Expects( data_.ptr_ != gsl_nullptr );
    }
    template< class U >
    gsl_api gsl_constexpr14 not_null<T>& operator=( not_null<U> other )
    {
        data_.ptr_ = detail::not_null_accessor<U>::get_checked( std::move( other ) );
        return *this;
    }
# endif // gsl_HAVE( TYPE_TRAITS ) && gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG ) && ! gsl_BETWEEN( gsl_COMPILER_CLANG_VERSION, 1, 400 ) && ! gsl_BETWEEN( gsl_COMPILER_APPLECLANG_VERSION, 1, 1001 )
#else // a.k.a. ! gsl_HAVE( MOVE_FORWARD )
    template< class U >
    gsl_api gsl_constexpr14 not_null( not_null<U> const & other )
    : data_( T( detail::not_null_accessor<U>::get_checked( other ) ) )
    {
    }
    template< class U >
    gsl_api gsl_constexpr14 not_null<T>& operator=( not_null<U> const & other )
    {
        data_.ptr_ = detail::not_null_accessor<U>::get_checked( other );
        return *this;
    }
#endif // gsl_HAVE( MOVE_FORWARD )

#if gsl_BASELINE_CPP20_
# if gsl_CONFIG( TRANSPARENT_NOT_NULL )
    [[nodiscard]] gsl_api constexpr decltype( auto )
    get( [[maybe_unused]] std::source_location const & loc = std::source_location::current() ) const
    requires detail::has_element_typedef<T>::value
    {
        if constexpr ( ! detail::has_trivial_move_v<T> )
        {
            gsl_AssertAt( loc, ptr_ != nullptr );
        }
        return ptr_.get();
    }
# else // ! gsl_CONFIG( TRANSPARENT_NOT_NULL )
#  if gsl_CONFIG( NOT_NULL_GET_BY_CONST_REF )
    [[nodiscard]] gsl_api constexpr T const &
    get( [[maybe_unused]] std::source_location const & loc = std::source_location::current() ) const
    {
        if constexpr ( ! detail::has_trivial_move_v<T> )
        {
            gsl_AssertAt( loc, ptr_ != nullptr );
        }
        return ptr_;
    }
#  else // a.k.a. ! gsl_CONFIG( NOT_NULL_GET_BY_CONST_REF )
    [[nodiscard]] gsl_api constexpr T
    get( [[maybe_unused]] std::source_location const & loc = std::source_location::current() ) const
    {
        if constexpr ( ! detail::has_trivial_move_v<T> )
        {
            gsl_AssertAt( loc, ptr_ != nullptr );
        }
        return ptr_;
    }
#  endif // gsl_CONFIG( NOT_NULL_GET_BY_CONST_REF )
# endif
#else // ! gsl_BASELINE_CPP20_
# if ! gsl_CONFIG( TRANSPARENT_NOT_NULL )
#  if gsl_CONFIG( NOT_NULL_GET_BY_CONST_REF )
    gsl_NODISCARD gsl_api gsl_constexpr14 T const &
    get() const
    {
        return accessor::get_checked( *this );
    }
#  else // a.k.a. ! gsl_CONFIG( NOT_NULL_GET_BY_CONST_REF )
    gsl_NODISCARD gsl_api gsl_constexpr14 T
    get() const
    {
        return accessor::get_checked( *this );
    }
#  endif // gsl_CONFIG( NOT_NULL_GET_BY_CONST_REF )
# endif // ! gsl_CONFIG( TRANSPARENT_NOT_NULL )
#endif // gsl_BASELINE_CPP20_

#if gsl_BASELINE_CPP20_
    [[nodiscard]] gsl_api constexpr decltype( auto )
    operator*() const noexcept( detail::has_trivial_move_v<T> )
    requires detail::is_dereferencable_v<T>  // do not define for `void*`, `std::function<>` etc.
    {
        if constexpr ( ! detail::has_trivial_move_v<T> )
        {
            gsl_Assert( ptr_ != nullptr );
        }
        return *ptr_;
    }
#endif // gsl_BASELINE_CPP20_

    // We want an implicit conversion operator that can be used to convert from both lvalues (by
    // const reference or by copy) and rvalues (by move). So it seems like we could define
    //
    //     template< class U >
    //     operator U const &() const & { ... }
    //     template< class U >
    //     operator U &&() && { ... }
    //
    // However, having two conversion operators with different return types renders the assignment
    // operator of the result type ambiguous:
    //
    //     not_null<std::unique_ptr<T>> p( ... );
    //     std::unique_ptr<U> q;
    //     q = std::move( p ); // ambiguous
    //
    // To avoid this ambiguity, we have both overloads of the conversion operator return `U`
    // rather than `U const &` or `U &&`. This implies that converting an lvalue always induces
    // a copy, which can cause unnecessary copies or even fail to compile in some situations:
    //
    //     not_null<std::shared_ptr<T>> sp( ... );
    //     std::shared_ptr<U> const & rs = sp; // unnecessary copy
    //     std::unique_ptr<U> const & ru = p; // error: cannot copy `unique_ptr<T>`
    //
    // However, these situations are rather unusual, and the following, more frequent situations
    // remain unimpaired:
    //
    //     std::shared_ptr<U> vs = sp; // no extra copy
    //     std::unique_ptr<U> vu = std::move( p );

#if gsl_BASELINE_CPP20_
    // explicit conversion operator

    template< class U >
    requires std::is_constructible_v<U, T const &> && ( ! std::is_convertible_v<T, U> && ! detail::is_not_null_or_bool_oracle_v<U> )
    [[nodiscard]] gsl_api constexpr explicit
    operator U() const &
    {
        if constexpr ( ! detail::has_trivial_move_v<T> )
        {
            gsl_Assert( ptr_ != nullptr );
        }
        return U( ptr_ );
    }
    template< class U >
    requires std::is_constructible_v<U, T const &> && ( ! std::is_convertible_v<T, U> && ! detail::is_not_null_or_bool_oracle_v<U> )
    [[nodiscard]] gsl_api constexpr explicit
    operator U() &
    {
        if constexpr ( ! detail::has_trivial_move_v<T> )
        {
            gsl_Assert( ptr_ != nullptr );
        }
        return U( ptr_ );
    }
    template< class U >
    requires std::is_constructible_v<U, T const &> && ( ! std::is_convertible_v<T, U> && ! detail::is_not_null_or_bool_oracle_v<U> )
    [[nodiscard]] gsl_api constexpr explicit
    operator U() const &&
    {
        if constexpr ( ! detail::has_trivial_move_v<T> )
        {
            gsl_Assert( ptr_ != nullptr );
        }
        return U( ptr_ );
    }
    template< class U >
    requires std::is_constructible_v<U, T> && ( ! std::is_convertible_v<T, U> && ! detail::is_not_null_or_bool_oracle_v<U> )
    [[nodiscard]] gsl_api constexpr explicit
    operator U() &&
    {
        if constexpr ( ! detail::has_trivial_move_v<T> )
        {
            gsl_Assert( ptr_ != nullptr );
        }
        return U( std::move( ptr_ ) );
    }

    // implicit conversion operator
    [[nodiscard]] gsl_api constexpr
    operator T() const &
    {
        if constexpr ( ! detail::has_trivial_move_v<T> )
        {
            gsl_Assert( ptr_ != nullptr );
        }
        return ptr_;
    }
    [[nodiscard]] gsl_api constexpr
    operator T() &
    {
        if constexpr ( ! detail::has_trivial_move_v<T> )
        {
            gsl_Assert( ptr_ != nullptr );
        }
        return ptr_;
    }
    [[nodiscard]] gsl_api constexpr
    operator T() const &&
    {
        if constexpr ( ! detail::has_trivial_move_v<T> )
        {
            gsl_Assert( ptr_ != nullptr );
        }
        return ptr_;
    }
    [[nodiscard]] gsl_api constexpr
    operator T() &&
    {
        if constexpr ( ! detail::has_trivial_move_v<T> )
        {
            gsl_Assert( ptr_ != nullptr );
        }
        return std::move( ptr_ );
    }

    operator bool() const & = delete;
    operator bool() & = delete;
    operator bool() && = delete;
    operator bool() const && = delete;

    template< class U >
    requires std::is_constructible_v<U, T const &> && std::is_convertible_v<T, U> && ( ! detail::is_not_null_or_bool_oracle_v<U> )
    [[nodiscard]] gsl_api constexpr
    operator U() const &
    {
        if constexpr ( ! detail::has_trivial_move_v<T> )
        {
            gsl_Assert( ptr_ != nullptr );
        }
        return U( ptr_ );
    }
    template< class U >
    requires std::is_constructible_v<U, T const &> && std::is_convertible_v<T, U> && ( ! detail::is_not_null_or_bool_oracle_v<U> )
    [[nodiscard]] gsl_api constexpr
    operator U() &
    {
        if constexpr ( ! detail::has_trivial_move_v<T> )
        {
            gsl_Assert( ptr_ != nullptr );
        }
        return U( ptr_ );
    }
    template< class U >
    requires std::is_constructible_v<U, T const &> && std::is_convertible_v<T, U> && ( ! detail::is_not_null_or_bool_oracle_v<U> )
    [[nodiscard]] gsl_api constexpr
    operator U() const &&
    {
        if constexpr ( ! detail::has_trivial_move_v<T> )
        {
            gsl_Assert( ptr_ != nullptr );
        }
        return U( ptr_ );
    }
    template< class U >
    requires std::is_constructible_v<U, T> && std::is_convertible_v<T, U> && ( ! detail::is_not_null_or_bool_oracle_v<U> )
    [[nodiscard]] gsl_api constexpr
    operator U() &&
    {
        if constexpr ( ! detail::has_trivial_move_v<T> )
        {
            gsl_Assert( ptr_ != nullptr );
        }
        return U( std::move( ptr_ ) );
    }

#elif gsl_HAVE( MOVE_FORWARD ) && gsl_HAVE( TYPE_TRAITS ) && gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG ) && gsl_HAVE( EXPLICIT )
    // explicit conversion operator

    template< class U
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( std::is_constructible<U, T const &>::value && !std::is_convertible<T, U>::value && !detail::is_not_null_or_bool_oracle<U>::value ))
    >
    gsl_NODISCARD gsl_api gsl_constexpr14 explicit
    operator U() const
# if gsl_HAVE( FUNCTION_REF_QUALIFIER )
    &
# endif
    {
        return U( accessor::get_checked( *this ) );
    }
# if gsl_HAVE( FUNCTION_REF_QUALIFIER )
    template< class U
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( std::is_constructible<U, T const &>::value && !std::is_convertible<T, U>::value && !detail::is_not_null_or_bool_oracle<U>::value ))
    >
    gsl_NODISCARD gsl_api gsl_constexpr14 explicit
    operator U() &
    {
        return U( accessor::get_checked( *this ) );
    }
    template< class U
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( std::is_constructible<U, T>::value && !std::is_convertible<T, U>::value && !detail::is_not_null_or_bool_oracle<U>::value ))
    >
    gsl_NODISCARD gsl_api gsl_constexpr14 explicit
    operator U() &&
    {
        return U( accessor::get_checked( std::move( *this ) ) );
    }
    template< class U
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( std::is_constructible<U, T const &>::value && !std::is_convertible<T, U>::value && !detail::is_not_null_or_bool_oracle<U>::value ))
    >
    gsl_NODISCARD gsl_api gsl_constexpr14 explicit
    operator U() const &&
    {
        return U( accessor::get_checked( *this ) );
    }
# endif

# if gsl_HAVE( FUNCTION_REF_QUALIFIER )
    // implicit conversion operator
    gsl_NODISCARD gsl_api gsl_constexpr14
    operator T() const &
    {
        return accessor::get_checked( *this );
    }
    gsl_NODISCARD gsl_api gsl_constexpr14
    operator T() &
    {
        return accessor::get_checked( *this );
    }
    gsl_NODISCARD gsl_api gsl_constexpr14
    operator T() &&
    {
        return accessor::get_checked( std::move( *this ) );
    }
    gsl_NODISCARD gsl_api gsl_constexpr14
    operator T() const &&
    {
        return accessor::get_checked( *this );
    }
# endif
# if gsl_HAVE( IS_DELETE )
#  if gsl_HAVE( FUNCTION_REF_QUALIFIER )
    operator bool() const & = delete;
    operator bool() & = delete;
    operator bool() && = delete;
    operator bool() const && = delete;
#  else
    operator bool() const = delete;
#  endif
# endif
    template< class U
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( std::is_constructible<U, T const &>::value && std::is_convertible<T, U>::value && !detail::is_not_null_or_bool_oracle<U>::value ))
    >
    gsl_NODISCARD gsl_api gsl_constexpr14
    operator U() const
# if gsl_HAVE( FUNCTION_REF_QUALIFIER )
    &
# endif
    {
        return accessor::get_checked( *this );
    }
# if gsl_HAVE( FUNCTION_REF_QUALIFIER )
    template< class U
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( std::is_constructible<U, T const &>::value && std::is_convertible<T, U>::value && !detail::is_not_null_or_bool_oracle<U>::value ))
    >
    gsl_NODISCARD gsl_api gsl_constexpr14
    operator U() &
    {
        return accessor::get_checked( *this );
    }
    template< class U
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( std::is_convertible<T, U>::value && !detail::is_not_null_or_bool_oracle<U>::value ))
    >
    gsl_NODISCARD gsl_api gsl_constexpr14
    operator U() &&
    {
        return accessor::get_checked( std::move( *this ) );
    }
    template< class U
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( std::is_convertible<T const &, U>::value && !detail::is_not_null_or_bool_oracle<U>::value ))
    >
    gsl_NODISCARD gsl_api gsl_constexpr14
    operator U() const &&
    {
        return accessor::get_checked( *this );
    }
# endif
#else // a.k.a. #if !( gsl_HAVE( MOVE_FORWARD ) && gsl_HAVE( TYPE_TRAITS ) && gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG ) && gsl_HAVE( EXPLICIT ) )
    template< class U >
    gsl_NODISCARD gsl_api gsl_constexpr14
    operator U() const
    {
        return U( accessor::get_checked( *this ) );
    }
#endif // gsl_HAVE( MOVE_FORWARD ) && gsl_HAVE( TYPE_TRAITS ) && gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG ) && gsl_HAVE( EXPLICIT )

    gsl_NODISCARD gsl_api gsl_constexpr14 T const &
    operator->() const
    {
#if gsl_BASELINE_CPP20_
        if constexpr ( ! detail::has_trivial_move_v<T> )
        {
            gsl_Assert( ptr_ != nullptr );
        }
        return ptr_;
#else // ! gsl_BASELINE_CPP20_
        return accessor::get_checked( *this );
#endif
    }

#if gsl_BASELINE_CPP20_
    gsl_api constexpr not_null( not_null && other, [[maybe_unused]] std::source_location const & loc = std::source_location::current() )
    gsl_noexcept_not_testing  // we want to be nothrow-movable despite the assertion
    : ptr_( std::move( other.ptr_ ) )
    {
        if constexpr ( ! detail::has_trivial_move_v<T> )
        {
            gsl_AssertAt( loc, ptr_ != nullptr );
        }
    }
    gsl_api gsl_constexpr14 not_null & operator=( not_null && other )
    gsl_noexcept_not_testing // we want to be nothrow-movable despite the assertion
    {
        if constexpr ( ! detail::has_trivial_move_v<T> )
        {
            gsl_Assert( other.ptr_ != nullptr || &other == this );
        }
        ptr_ = std::move( other.ptr_ );
        return *this;
    }
#elif gsl_HAVE( MOVE_FORWARD )
    // Visual C++ 2013 doesn't generate default move constructors, so we declare them explicitly.
    gsl_api gsl_constexpr14 not_null( not_null && other )
    gsl_noexcept_not_testing  // we want to be nothrow-movable despite the assertion
    : data_( std::move( other.data_ ) )
    {
    }
    gsl_api gsl_constexpr14 not_null & operator=( not_null && other )
    gsl_noexcept_not_testing // we want to be nothrow-movable despite the assertion
    {
        data_ = std::move( other.data_ );
        return *this;
    }
#endif // gsl_HAVE( MOVE_FORWARD )

#if gsl_BASELINE_CPP20_
    gsl_api constexpr not_null( not_null const & other, [[maybe_unused]] std::source_location const & loc = std::source_location::current() )
    requires std::copy_constructible<T>
    : ptr_( other.ptr_ )
    {
        if constexpr ( ! detail::has_trivial_move_v<T> )
        {
            gsl_AssertAt( loc, ptr_ != nullptr );
        }
    }
    gsl_api gsl_constexpr14 not_null & operator=( not_null const & other )
    requires std::copy_constructible<T>
    {
        if constexpr ( ! detail::has_trivial_move_v<T> )
        {
            gsl_Assert( other.ptr_ != nullptr || &other == this );
        }
        ptr_ = other.ptr_;
        return *this;
    }
#elif gsl_HAVE( IS_DEFAULT )
    not_null( not_null const & ) = default;
    not_null & operator=( not_null const & ) = default;
#endif

#if gsl_BASELINE_CPP20_
    gsl_api constexpr friend void swap( not_null & lhs, not_null & rhs, [[maybe_unused]] std::source_location const & loc = std::source_location::current() )
    gsl_noexcept_not_testing // we want to be nothrow-swappable despite the precondition check
    {
        if constexpr ( ! detail::has_trivial_move_v<T> )
        {
            gsl_AssertAt( loc, lhs.ptr_ != nullptr );
            gsl_AssertAt( loc, rhs.ptr_ != nullptr );
        }
        using std::swap;
        swap( lhs.ptr_, rhs.ptr_ );
    }
#else // ! gsl_BASELINE_CPP20_
    gsl_api gsl_constexpr20 friend void swap( not_null & lhs, not_null & rhs )
    gsl_noexcept_not_testing // we want to be nothrow-swappable despite the precondition check
    {
        accessor::check( lhs );
        accessor::check( rhs );
        using std::swap;
        swap( lhs.data_.ptr_, rhs.data_.ptr_ );
    }
#endif

gsl_is_delete_access:
    not_null() gsl_is_delete;
    // prevent compilation when initialized with a nullptr or literal 0:
#if gsl_HAVE( NULLPTR )
    not_null(             std::nullptr_t ) gsl_is_delete;
    not_null & operator=( std::nullptr_t ) gsl_is_delete;
#else
    not_null(             int ) gsl_is_delete;
    not_null & operator=( int ) gsl_is_delete;
#endif

#if gsl_BASELINE_CPP20_
    template< class... Ts >
    gsl_api constexpr auto
    operator()( Ts&&... args ) const
# if ! gsl_COMPILER_NVCC_VERSION && ! gsl_COMPILER_NVHPC_VERSION
        // NVCC and NVHPC think that Substitution Failure Is An Error here
        -> decltype( ptr_( std::forward<Ts>( args )... ) )
# endif // ! gsl_COMPILER_NVCC_VERSION && ! gsl_COMPILER_NVHPC_VERSION
    {
        if constexpr ( ! detail::has_trivial_move_v<T> )
        {
            gsl_Assert( ptr_ != nullptr );
        }
        return ptr_( static_cast<Ts&&>( args )... );
    }
#elif gsl_STDLIB_CPP11_140 && ( gsl_CPP14_OR_GREATER || ( ! gsl_COMPILER_NVCC_VERSION && ! gsl_COMPILER_NVHPC_VERSION ) )
    template< class... Ts >
    gsl_api gsl_constexpr14 auto
    operator()( Ts&&... args ) const
# if ! gsl_COMPILER_NVCC_VERSION && ! gsl_COMPILER_NVHPC_VERSION
        // NVCC and NVHPC think that Substitution Failure Is An Error here
        -> decltype( data_.ptr_( std::forward<Ts>( args )... ) )
# endif // ! gsl_COMPILER_NVCC_VERSION && ! gsl_COMPILER_NVHPC_VERSION
    {
        return accessor::get_checked( *this )( static_cast<Ts&&>( args )... );
    }
#endif

    // unwanted operators...pointers only point to single objects!
    // TODO: revise for not_null<czstring> and the like?
    not_null & operator++() gsl_is_delete;
    not_null & operator--() gsl_is_delete;
    not_null   operator++( int ) gsl_is_delete;
    not_null   operator--( int ) gsl_is_delete;
    not_null & operator+ ( size_t ) gsl_is_delete;
    not_null & operator+=( size_t ) gsl_is_delete;
    not_null & operator- ( size_t ) gsl_is_delete;
    not_null & operator-=( size_t ) gsl_is_delete;
    not_null & operator+=( std::ptrdiff_t ) gsl_is_delete;
    not_null & operator-=( std::ptrdiff_t ) gsl_is_delete;
    void       operator[]( std::ptrdiff_t ) const gsl_is_delete;
};
#if gsl_HAVE( DEDUCTION_GUIDES )
template< class U >
not_null( U ) -> not_null<U>;
template< class U >
not_null( not_null<U> ) -> not_null<U>;
#endif

#if gsl_HAVE( NULLPTR )
void make_not_null( std::nullptr_t ) gsl_is_delete;
#endif // gsl_HAVE( NULLPTR )
#if gsl_BASELINE_CPP20_
template< nullable U >
[[nodiscard]] gsl_api constexpr not_null<U>
make_not_null( U u, std::source_location const & loc = std::source_location::current() )
{
    return not_null<U>( std::move( u ), loc );
}
template< nullable U >
gsl_NODISCARD gsl_api gsl_constexpr14 not_null<U>
make_not_null( not_null<U> u )
{
    return std::move( u );
}
#elif gsl_HAVE( MOVE_FORWARD )
template< class U >
gsl_NODISCARD gsl_api gsl_constexpr14 not_null<U>
make_not_null( U u )
{
    return not_null<U>( std::move( u ) );
}
template< class U >
gsl_NODISCARD gsl_api gsl_constexpr14 not_null<U>
make_not_null( not_null<U> u )
{
    return std::move( u );
}
#else // a.k.a. ! gsl_HAVE( MOVE_FORWARD )
template< class U >
gsl_NODISCARD gsl_api not_null<U>
make_not_null( U const & u )
{
    return not_null<U>( u );
}
template< class U >
gsl_NODISCARD gsl_api not_null<U>
make_not_null( not_null<U> const & u )
{
    return u;
}
#endif // gsl_HAVE( MOVE_FORWARD )

namespace detail {

template< class T >
struct as_nullable_helper
{
    typedef T type;
};
template< class T >
struct as_nullable_helper< not_null<T> >
{
};

template< class T >
struct not_null_accessor
{
#if gsl_BASELINE_CPP20_
    static gsl_api bool is_valid( not_null<T> const & p ) noexcept
    {
        return p.ptr_ != nullptr;
    }
#else // ! gsl_BASELINE_CPP20_
# if gsl_HAVE( MOVE_FORWARD )
    static gsl_api T get( not_null<T>&& p ) gsl_noexcept
    {
        return std::move( p.data_.ptr_ );
    }
    static gsl_api T get_checked( not_null<T>&& p )
    {
        gsl_Assert( p.data_.ptr_ != gsl_nullptr );
        return std::move( p.data_.ptr_ );
    }
# endif
    static gsl_api T const & get( not_null<T> const & p ) gsl_noexcept
    {
        return p.data_.ptr_;
    }
    static gsl_api bool is_valid( not_null<T> const & p ) gsl_noexcept
    {
        return p.data_.ptr_ != gsl_nullptr;
    }
    static gsl_api void check( not_null<T> const & p )
    {
        gsl_Assert( p.data_.ptr_ != gsl_nullptr );
    }
    static gsl_api T const & get_checked( not_null<T> const & p )
    {
        gsl_Assert( p.data_.ptr_ != gsl_nullptr );
        return p.data_.ptr_;
    }
#endif // ! gsl_BASELINE_CPP20_
};
#if ! gsl_BASELINE_CPP20_
template< class T >
struct not_null_accessor< T * >
{
    static gsl_api T * const & get( not_null< T * > const & p ) gsl_noexcept
    {
        return p.data_.ptr_;
    }
    static gsl_api bool is_valid( not_null< T * > const & /*p*/ ) gsl_noexcept
    {
        return true;
    }
    static gsl_api void check( not_null< T * > const & /*p*/ )
    {
    }
    static gsl_api T * const & get_checked( not_null< T * > const & p ) gsl_noexcept
    {
        return p.data_.ptr_;
    }
};
#endif // ! gsl_BASELINE_CPP20_

namespace no_adl {

#if gsl_HAVE( MOVE_FORWARD )
template< class T >
gsl_NODISCARD gsl_api gsl_constexpr auto as_nullable( T && p )
gsl_noexcept_if( std::is_nothrow_move_constructible<T>::value )
-> typename detail::as_nullable_helper<typename std20::remove_cvref<T>::type>::type
{
    return std::move( p );
}
template< class T >
gsl_NODISCARD gsl_api gsl_constexpr14 T as_nullable( not_null<T> && p )
{
#if gsl_BASELINE_CPP20_
    return std::move( p );
#else // ! gsl_BASELINE_CPP20_
    return detail::not_null_accessor<T>::get_checked( std::move( p ) );
#endif
}
#else // ! gsl_HAVE( MOVE_FORWARD )
template< class T >
gsl_NODISCARD gsl_api gsl_constexpr T const &
as_nullable( T const & p ) gsl_noexcept
{
    return p;
}
#endif // gsl_HAVE( MOVE_FORWARD )
template< class T >
gsl_NODISCARD gsl_api gsl_constexpr14 T const &
as_nullable( not_null<T> const & p )
{
    return p.operator ->();
}

template< class T >
gsl_NODISCARD gsl_api gsl_constexpr bool
is_valid( not_null<T> const & p )
{
    return detail::not_null_accessor<T>::is_valid( p );
}

#if gsl_HAVE( EXPRESSION_SFINAE )
template< class P >
gsl_NODISCARD gsl_api gsl_constexpr14 auto
get( not_null<P> const & p ) -> decltype( gsl_lite::make_not_null( p.operator->().get() ) )
{
    return gsl_lite::make_not_null( p.operator->().get() );
}
template< class T >
gsl_NODISCARD gsl_api gsl_constexpr14 not_null< T * >
get( not_null<T *> const & p )
{
    return p;
}
template< class P >
gsl_NODISCARD gsl_api gsl_constexpr14 auto
get( P const & p ) -> typename std::enable_if< !detail::is_not_null_or_bool_oracle<P>::value, decltype( p.get() )>::type
{
    return p.get();
}
template< class T >
gsl_NODISCARD gsl_api gsl_constexpr14 T *
get( T * const & p )
{
    return p;
}

template< class S >
gsl_NODISCARD gsl_api gsl_constexpr14 auto
c_str( S const & str ) -> decltype( gsl_lite::make_not_null( str.c_str() ) )
{
    return gsl_lite::make_not_null( str.c_str() );
}
template< class C >
gsl_NODISCARD gsl_api gsl_constexpr14 typename std::enable_if< detail::is_char<typename std::remove_cv<C>::type>::value, not_null< C const * > >::type
c_str( not_null< C * > const & zstr )
{
    return zstr;
}
template< class C, std::size_t N >
gsl_NODISCARD gsl_api gsl_constexpr14 typename std::enable_if< detail::is_char<C>::value, not_null< C const * > >::type
c_str( C const ( & literal )[ N ] )
{
    return literal;
}
template< class C >
gsl_NODISCARD gsl_api gsl_constexpr14 typename std::enable_if< detail::is_char<C>::value, C const * >::type
c_str( C const * const & str )
{
    return str;
}
#endif // gsl_HAVE( EXPRESSION_SFINAE )

} // namespace no_adl
} // namespace detail

using namespace detail::no_adl;

// not_null with implicit constructor, allowing copy-initialization:

template< class T >
class not_null_ic : public not_null<T>
{
public:
#if gsl_BASELINE_CPP20_
    template< class U >
    requires std::is_constructible_v<T, U>
#else // ! gsl_BASELINE_CPP20_
    template< class U
        gsl_ENABLE_IF_(( std::is_constructible<T, U>::value ))
    >
#endif
    gsl_api gsl_constexpr14
#if gsl_HAVE( MOVE_FORWARD )
    not_null_ic( U u )
    : not_null<T>( std::move( u ) )
#else // ! gsl_HAVE( MOVE_FORWARD )
    not_null_ic( U const & u )
    : not_null<T>( u )
#endif // gsl_HAVE( MOVE_FORWARD )
    {}
};

// more not_null unwanted operators

template< class T, class U >
std::ptrdiff_t operator-( not_null<T> const &, not_null<U> const & ) gsl_is_delete;

template< class T >
not_null<T> operator-( not_null<T> const &, std::ptrdiff_t ) gsl_is_delete;

template< class T >
not_null<T> operator+( not_null<T> const &, std::ptrdiff_t ) gsl_is_delete;

template< class T >
not_null<T> operator+( std::ptrdiff_t, not_null<T> const & ) gsl_is_delete;

// not_null comparisons

#if gsl_HAVE( NULLPTR ) && gsl_HAVE( IS_DELETE )
template< class T >
gsl_constexpr bool
operator==( not_null<T> const &, std::nullptr_t ) = delete;
template< class T >
gsl_constexpr bool
operator==( std::nullptr_t , not_null<T> const & ) = delete;
template< class T >
gsl_constexpr bool
operator!=( not_null<T> const &, std::nullptr_t ) = delete;
template< class T >
gsl_constexpr bool
operator!=( std::nullptr_t , not_null<T> const & ) = delete;
#endif // gsl_HAVE( NULLPTR ) && gsl_HAVE( IS_DELETE )

template< class T, class U >
gsl_NODISCARD inline gsl_api gsl_constexpr gsl_TRAILING_RETURN_TYPE_( bool )
operator==( not_null<T> const & l, not_null<U> const & r )
gsl_RETURN_DECLTYPE_( l.operator->() == r.operator->() )
{
    return l.operator->() == r.operator->();
}
template< class T, class U >
gsl_NODISCARD inline gsl_api gsl_constexpr gsl_TRAILING_RETURN_TYPE_( bool )
operator==( not_null<T> const & l, U const & r )
gsl_RETURN_DECLTYPE_(l.operator->() == r )
{
    return l.operator->() == r;
}
#ifndef __cpp_lib_three_way_comparison
template< class T, class U >
gsl_NODISCARD inline gsl_api gsl_constexpr gsl_TRAILING_RETURN_TYPE_( bool )
operator==( T const & l, not_null<U> const & r )
gsl_RETURN_DECLTYPE_( l == r.operator->() )
{
    return l == r.operator->();
}
#endif

// The C++ Core Guidelines discourage the use of pointer arithmetic, and gsl-lite consequently refrains from defining operators
// for pointer arithmetic or the subscript operator in `not_null<>`. However, comparison of `not_null<>` objects is supported;
// although the standard does not mandate a certain ordering for objects with two exceptions (objects from the same array;
// data members of the same class, as required for `offsetof()`), it does require that `operator<` establishes a total ordering
// of pointers, as implied by https://eel.is/c++draft/expr.rel#5. Among other things, this guarantees that a list of pointers
// can be sorted and searched, or that pointers can be used as a key in a relational container such as `std::map<>`.
// Therefore, we also define relational comparison operators for `not_null<>`.

#ifdef __cpp_impl_three_way_comparison
template< class T, class U >
[[nodiscard]] inline gsl_api constexpr auto
operator<=>( not_null<T> const & l, not_null<U> const & r )
-> decltype( l.operator->() <=> r.operator->() )
{
    return l.operator->() <=> r.operator->();
}
template< class T, class U >
[[nodiscard]] inline gsl_api constexpr auto
operator<=>( not_null<T> const & l, U const & r )
-> decltype( l.operator->() <=> r )
{
    return l.operator->() <=> r;
}
#endif
template< class T, class U >
gsl_NODISCARD inline gsl_api gsl_constexpr gsl_TRAILING_RETURN_TYPE_( bool )
operator<( not_null<T> const & l, not_null<U> const & r )
gsl_RETURN_DECLTYPE_( l.operator->() < r.operator->() )
{
    return l.operator->() < r.operator->();
}
template< class T, class U >
gsl_NODISCARD inline gsl_api gsl_constexpr gsl_TRAILING_RETURN_TYPE_( bool )
operator<( not_null<T> const & l, U const & r )
gsl_RETURN_DECLTYPE_( l.operator->() < r )
{
    return l.operator->() < r;
}
template< class T, class U >
gsl_NODISCARD inline gsl_api gsl_constexpr gsl_TRAILING_RETURN_TYPE_( bool )
operator<( T const & l, not_null<U> const & r )
gsl_RETURN_DECLTYPE_( l < r.operator->() )
{
    return l < r.operator->();
}

#ifndef __cpp_lib_three_way_comparison
template< class T, class U >
gsl_NODISCARD inline gsl_api gsl_constexpr gsl_TRAILING_RETURN_TYPE_( bool )
operator!=( not_null<T> const & l, not_null<U> const & r )
gsl_RETURN_DECLTYPE_( !( l == r ) )
{
    return !( l == r );
}
template< class T, class U >
gsl_NODISCARD inline gsl_api gsl_constexpr gsl_TRAILING_RETURN_TYPE_( bool )
operator!=( not_null<T> const & l, U const & r )
gsl_RETURN_DECLTYPE_( !( l == r ) )
{
    return !( l == r );
}
template< class T, class U >
gsl_NODISCARD inline gsl_api gsl_constexpr gsl_TRAILING_RETURN_TYPE_( bool )
operator!=( T const & l, not_null<U> const & r )
gsl_RETURN_DECLTYPE_( !( l == r ) )
{
    return !( l == r );
}
#endif

template< class T, class U >
gsl_NODISCARD inline gsl_api gsl_constexpr gsl_TRAILING_RETURN_TYPE_( bool )
operator<=( not_null<T> const & l, not_null<U> const & r )
gsl_RETURN_DECLTYPE_( !( r < l ) )
{
    return !( r < l );
}
template< class T, class U >
gsl_NODISCARD inline gsl_api gsl_constexpr gsl_TRAILING_RETURN_TYPE_( bool )
operator<=( not_null<T> const & l, U const & r )
gsl_RETURN_DECLTYPE_( !( r < l ) )
{
    return !( r < l );
}
template< class T, class U >
gsl_NODISCARD inline gsl_api gsl_constexpr gsl_TRAILING_RETURN_TYPE_( bool )
operator<=( T const & l, not_null<U> const & r )
gsl_RETURN_DECLTYPE_( !( r < l ) )
{
    return !( r < l );
}

template< class T, class U >
gsl_NODISCARD inline gsl_api gsl_constexpr gsl_TRAILING_RETURN_TYPE_( bool )
operator>( not_null<T> const & l, not_null<U> const & r )
gsl_RETURN_DECLTYPE_( r < l )
{
    return r < l;
}
template< class T, class U >
gsl_NODISCARD inline gsl_api gsl_constexpr gsl_TRAILING_RETURN_TYPE_( bool )
operator>( not_null<T> const & l, U const & r )
gsl_RETURN_DECLTYPE_( r < l )
{
    return r < l;
}
template< class T, class U >
gsl_NODISCARD inline gsl_api gsl_constexpr gsl_TRAILING_RETURN_TYPE_( bool )
operator>( T const & l, not_null<U> const & r )
gsl_RETURN_DECLTYPE_( r < l )
{
    return r < l;
}

template< class T, class U >
gsl_NODISCARD inline gsl_api gsl_constexpr gsl_TRAILING_RETURN_TYPE_( bool )
operator>=( not_null<T> const & l, not_null<U> const & r )
gsl_RETURN_DECLTYPE_( !( l < r ) )
{
    return !( l < r );
}
template< class T, class U >
gsl_NODISCARD inline gsl_api gsl_constexpr gsl_TRAILING_RETURN_TYPE_( bool )
operator>=( not_null<T> const & l, U const & r )
gsl_RETURN_DECLTYPE_( !( l < r ) )
{
    return !( l < r );
}
template< class T, class U >
gsl_NODISCARD inline gsl_api gsl_constexpr gsl_TRAILING_RETURN_TYPE_( bool )
operator>=( T const & l, not_null<U> const & r )
gsl_RETURN_DECLTYPE_( !( l < r ) )
{
    return !( l < r );
}

// print not_null

template< class CharType, class Traits, class T >
gsl_TRAILING_RETURN_TYPE_2_( std::basic_ostream< CharType, Traits > & )
operator<<( std::basic_ostream< CharType, Traits > & os, not_null<T> const & p )
gsl_RETURN_DECLTYPE_( os << p.operator->() )
{
    return os << p.operator->();
}


#if gsl_HAVE( VARIADIC_TEMPLATE )
# if gsl_HAVE( UNIQUE_PTR )
template< class T, class... Args >
gsl_NODISCARD not_null<std::unique_ptr<T>>
make_unique( Args &&... args )
{
#  if gsl_HAVE( TYPE_TRAITS )
    static_assert( !std::is_array<T>::value, "gsl_lite::make_unique<T>() returns `gsl_lite::not_null<std::unique_ptr<T>>`, which is not "
        "defined for array types because the Core Guidelines advise against pointer arithmetic, cf. \"Bounds safety profile\"." );
#  endif
    return not_null<std::unique_ptr<T>>( new T( std::forward<Args>( args )... ) );
}
# endif // gsl_HAVE( UNIQUE_PTR )
# if gsl_HAVE( SHARED_PTR )
template< class T, class... Args >
gsl_NODISCARD not_null<std::shared_ptr<T>>
make_shared( Args &&... args )
{
#  if gsl_HAVE( TYPE_TRAITS )
    static_assert( !std::is_array<T>::value, "gsl_lite::make_shared<T>() returns `gsl_lite::not_null<std::shared_ptr<T>>`, which is not "
        "defined for array types because the Core Guidelines advise against pointer arithmetic, cf. \"Bounds safety profile\"." );
#  endif
    return not_null<std::shared_ptr<T>>( std::make_shared<T>( std::forward<Args>( args )... ) );
}
# endif // gsl_HAVE( SHARED_PTR )
#endif // gsl_HAVE( VARIADIC_TEMPLATE )


#if gsl_FEATURE( BYTE )
//
// Byte-specific type.
//
# if gsl_HAVE( ENUM_CLASS_CONSTRUCTION_FROM_UNDERLYING_TYPE )
  enum class gsl_may_alias byte : unsigned char {};
# else
#  if ! defined( gsl_CONFIG_ACKNOWLEDGE_NONSTANDARD_ABI )
#   pragma message (__FILE__ "(" gsl_STRINGIFY( __LINE__ ) "): warning: in pre-C++11 mode, byte is defined as a struct rather than an enum, which changes the ABI of gsl-lite and may lead to ODR violations and undefined behavior; define the macro gsl_CONFIG_ACKNOWLEDGE_NONSTANDARD_ABI to explicitly acknowledge that you are using gsl-lite with a non-standard ABI and that you control the build flags of all components linked into your target")
#  endif
  struct gsl_may_alias byte { typedef unsigned char type; type v; };
# endif

template< class T >
gsl_NODISCARD gsl_api inline gsl_constexpr byte
to_byte( T v ) gsl_noexcept
{
# if    gsl_HAVE( ENUM_CLASS_CONSTRUCTION_FROM_UNDERLYING_TYPE )
    return static_cast<byte>( v );
# elif  gsl_HAVE( CONSTEXPR_11 )
    return { static_cast<typename byte::type>( v ) };
# else
    byte b = { static_cast<typename byte::type>( v ) }; return b;
# endif
}

template< class IntegerType  gsl_ENABLE_IF_(( std::is_integral<IntegerType>::value )) >
gsl_NODISCARD gsl_api inline gsl_constexpr IntegerType
to_integer( byte b ) gsl_noexcept
{
# if gsl_HAVE( ENUM_CLASS_CONSTRUCTION_FROM_UNDERLYING_TYPE )
    return static_cast<typename std::underlying_type<byte>::type>( b );
# else
    return b.v;
# endif
}

gsl_NODISCARD gsl_api inline gsl_constexpr unsigned char
to_uchar( byte b ) gsl_noexcept
{
    return to_integer<unsigned char>( b );
}

gsl_NODISCARD gsl_api inline gsl_constexpr unsigned char
to_uchar( int i ) gsl_noexcept
{
    return static_cast<unsigned char>( i );
}

template< class IntegerType  gsl_ENABLE_IF_(( std::is_integral<IntegerType>::value )) >
gsl_api inline gsl_constexpr14 byte &
operator<<=( byte & b, IntegerType shift ) gsl_noexcept
{
# if gsl_HAVE( ENUM_CLASS_CONSTRUCTION_FROM_UNDERLYING_TYPE )
    return b = ::gsl_lite::to_byte( ::gsl_lite::to_uchar( b ) << shift );
# else
    b.v = ::gsl_lite::to_uchar( b.v << shift ); return b;
# endif
}

template< class IntegerType  gsl_ENABLE_IF_(( std::is_integral<IntegerType>::value )) >
gsl_NODISCARD gsl_api inline gsl_constexpr byte
operator<<( byte b, IntegerType shift ) gsl_noexcept
{
    return ::gsl_lite::to_byte( ::gsl_lite::to_uchar( b ) << shift );
}

template< class IntegerType  gsl_ENABLE_IF_(( std::is_integral<IntegerType>::value )) >
gsl_NODISCARD gsl_api inline gsl_constexpr14 byte &
operator>>=( byte & b, IntegerType shift ) gsl_noexcept
{
# if gsl_HAVE( ENUM_CLASS_CONSTRUCTION_FROM_UNDERLYING_TYPE )
    return b = ::gsl_lite::to_byte( ::gsl_lite::to_uchar( b ) >> shift );
# else
    b.v = ::gsl_lite::to_uchar( b.v >> shift ); return b;
# endif
}

template< class IntegerType  gsl_ENABLE_IF_(( std::is_integral<IntegerType>::value )) >
gsl_NODISCARD gsl_api inline gsl_constexpr byte
operator>>( byte b, IntegerType shift ) gsl_noexcept
{
    return ::gsl_lite::to_byte( ::gsl_lite::to_uchar( b ) >> shift );
}

# if gsl_HAVE( ENUM_CLASS_CONSTRUCTION_FROM_UNDERLYING_TYPE )
gsl_DEFINE_ENUM_BITMASK_OPERATORS( byte )
gsl_DEFINE_ENUM_RELATIONAL_OPERATORS( byte )
# else // a.k.a. !gsl_HAVE( ENUM_CLASS_CONSTRUCTION_FROM_UNDERLYING_TYPE )
gsl_api inline gsl_constexpr bool operator==( byte l, byte r ) gsl_noexcept
{
    return l.v == r.v;
}

gsl_api inline gsl_constexpr bool operator!=( byte l, byte r ) gsl_noexcept
{
    return !( l == r );
}

gsl_api inline gsl_constexpr bool operator< ( byte l, byte r ) gsl_noexcept
{
    return l.v < r.v;
}

gsl_api inline gsl_constexpr bool operator<=( byte l, byte r ) gsl_noexcept
{
    return !( r < l );
}

gsl_api inline gsl_constexpr bool operator> ( byte l, byte r ) gsl_noexcept
{
    return ( r < l );
}

gsl_api inline gsl_constexpr bool operator>=( byte l, byte r ) gsl_noexcept
{
    return !( l < r );
}

gsl_api inline gsl_constexpr14 byte & operator|=( byte & l, byte r ) gsl_noexcept
{
    l.v |= r.v; return l;
}

gsl_api inline gsl_constexpr byte operator|( byte l, byte r ) gsl_noexcept
{
    return ::gsl_lite::to_byte( l.v | r.v );
}

gsl_api inline gsl_constexpr14 byte & operator&=( byte & l, byte r ) gsl_noexcept
{
    l.v &= r.v; return l;
}

gsl_api inline gsl_constexpr byte operator&( byte l, byte r ) gsl_noexcept
{
    return ::gsl_lite::to_byte( l.v & r.v );
}

gsl_api inline gsl_constexpr14 byte & operator^=( byte & l, byte r ) gsl_noexcept
{
    l.v ^= r.v; return l;
}

gsl_api inline gsl_constexpr byte operator^( byte l, byte r ) gsl_noexcept
{
    return ::gsl_lite::to_byte( l.v ^ r.v );
}

gsl_api inline gsl_constexpr byte operator~( byte b ) gsl_noexcept
{
    return ::gsl_lite::to_byte( ~b.v );
}
# endif // gsl_HAVE( ENUM_CLASS_CONSTRUCTION_FROM_UNDERLYING_TYPE )
#endif // gsl_FEATURE( BYTE )

#if gsl_FEATURE_TO_STD( WITH_CONTAINER )

// Tag to select span constructor taking a container:

struct with_container_t { gsl_constexpr with_container_t( ) gsl_noexcept { } };
const  gsl_constexpr   with_container_t with_container; // TODO: this can lead to ODR violations because the symbol will be defined in multiple translation units

#endif


#if gsl_FEATURE( SPAN )
// The span<> and span_iterator<> implementation below was mostly borrowed from Microsoft GSL.


# if gsl_COMPILER_MSVC_VER
#  pragma warning( push )

// turn off some warnings that are noisy about our Expects statements
#  pragma warning( disable : 4127 ) // conditional expression is constant
#  pragma warning( disable : 4146 ) // unary minus operator applied to unsigned type, result still unsigned
#  pragma warning( disable : 4702 ) // unreachable code

// Turn off MSVC /analyze rules that generate too much noise
#  pragma warning( disable : 26495 ) // uninitialized member when constructor calls constructor
#  pragma warning( disable : 26446 ) // parser bug does not allow attributes on some templates

# endif // gsl_COMPILER_MSVC_VER

// GCC 7 does not like the signed unsigned mismatch (size_t ptrdiff_t)
// While there is a conversion from signed to unsigned, it happens at
// compiletime, so the compiler wouldn't have to warn indiscriminately, but
// could check if the source value actually doesn't fit into the target type
// and only warn in those cases.
# if defined( __GNUC__ ) && __GNUC__ > 6
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wsign-conversion"
# endif // defined( __GNUC__ ) && __GNUC__ > 6

// Turn off clang unsafe buffer warnings as all accessed are guarded by runtime checks
# if defined( __clang__ )
#  if __has_warning( "-Wunsafe-buffer-usage" )
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#  endif // __has_warning( "-Wunsafe-buffer-usage" )
# endif // defined( __clang__ )

namespace detail {

template< gsl_CONFIG_SPAN_INDEX_TYPE From, gsl_CONFIG_SPAN_INDEX_TYPE To >
struct is_allowed_extent_conversion
    : std17::bool_constant< From == To || To == dynamic_extent >
{
};

# if gsl_HAVE( TYPE_TRAITS ) && gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG )
template< class From, class To >
struct is_allowed_element_type_conversion
    : std17::bool_constant< std::is_convertible< From (*)[], To (*)[] >::value >
{
};
# endif // gsl_HAVE( TYPE_TRAITS ) && gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG )

template< class T >
class span_iterator
{
public:
# if gsl_STDLIB_CPP20_OR_GREATER
    typedef typename std::contiguous_iterator_tag iterator_concept;
# endif // gsl_STDLIB_CPP20_OR_GREATER
    typedef typename std::random_access_iterator_tag iterator_category;
    typedef typename std11::remove_cv< T >::type value_type;
    typedef std::ptrdiff_t difference_type;
    typedef T * pointer;
    typedef T & reference;

# ifdef gsl_COMPILER_MS_STL_VERSION
    typedef pointer _Unchecked_type;
    typedef span_iterator _Prevent_inheriting_unwrap;
# endif // gsl_COMPILER_MS_STL_VERSION
# if gsl_HAVE( IS_DEFAULT )
    gsl_constexpr span_iterator() = default;
# else // ! gsl_HAVE( IS_DEFAULT )
    gsl_api gsl_constexpr span_iterator() gsl_noexcept
        : begin_( gsl_nullptr ), end_( gsl_nullptr ), current_( gsl_nullptr )
    {
    }
# endif // gsl_HAVE( IS_DEFAULT )

    gsl_api gsl_constexpr14 span_iterator( pointer begin, pointer end, pointer current )
        : begin_( begin ), end_( end ), current_( current )
    {
        gsl_ExpectsDebug( begin_ <= current_ && current <= end_ );
    }

    gsl_api gsl_constexpr14 operator span_iterator< T const >() const gsl_noexcept
    {
        return span_iterator< T const >( begin_, end_, current_ );
    }

    gsl_api gsl_constexpr14 reference operator*() const
    {
        gsl_ExpectsDebug( current_ != end_ );
        return *current_;
    }

    gsl_api gsl_constexpr14 pointer operator->() const
    {
        gsl_ExpectsDebug( current_ != end_ );
        return current_;
    }
    gsl_api gsl_constexpr14 span_iterator& operator++()
    {
        gsl_ExpectsDebug( current_ != end_ );
        gsl_SUPPRESS_MSGSL_WARNING(bounds.1)
        ++current_;
        return *this;
    }

    gsl_api gsl_constexpr14 span_iterator operator++( int )
    {
        span_iterator ret = *this;
        ++*this;
        return ret;
    }

    gsl_api gsl_constexpr14 span_iterator& operator--()
    {
        gsl_ExpectsDebug( begin_ != current_ );
        --current_;
        return *this;
    }

    gsl_api gsl_constexpr14 span_iterator operator--( int )
    {
        span_iterator ret = *this;
        --*this;
        return ret;
    }

    gsl_api gsl_constexpr14 span_iterator& operator+=( difference_type const n )
    {
        if ( n != 0 ) gsl_ExpectsDebug( begin_ && current_ && end_ );
        if ( n > 0 )  gsl_ExpectsDebug( end_ - current_ >= n );
        if ( n < 0 )  gsl_ExpectsDebug( current_ - begin_ >= -n );
        gsl_SUPPRESS_MSGSL_WARNING(bounds.1)
        current_ += n;
        return *this;
    }

    gsl_api gsl_constexpr14 span_iterator operator+( difference_type const n ) const
    {
        span_iterator ret = *this;
        ret += n;
        return ret;
    }

    friend gsl_api gsl_constexpr14 span_iterator operator+( difference_type const n,
                                                            span_iterator const & rhs )
    {
        return rhs + n;
    }

    gsl_api gsl_constexpr14 span_iterator& operator-=( difference_type const n )
    {
        if ( n != 0 ) gsl_ExpectsDebug( begin_ && current_ && end_ );
        if ( n > 0 )  gsl_ExpectsDebug( current_ - begin_ >= n );
        if ( n < 0 )  gsl_ExpectsDebug( end_ - current_ >= -n );
        gsl_SUPPRESS_MSGSL_WARNING(bounds .1)
        current_ -= n;
        return *this;
    }

    gsl_api gsl_constexpr14 span_iterator operator-( difference_type const n ) const
    {
        span_iterator ret = *this;
        ret -= n;
        return ret;
    }

    template<
        class U
        gsl_ENABLE_IF_(( std::is_same< typename std::remove_cv< U >::type, value_type >::value ))
    >
    gsl_api gsl_constexpr14 difference_type operator-( span_iterator< U > const & rhs ) const
    {
        gsl_ExpectsDebug( begin_ == rhs.begin_ && end_ == rhs.end_ );
        return current_ - rhs.current_;
    }

    gsl_api gsl_constexpr14 reference operator[]( difference_type const n ) const
    {
        return *( *this + n );
    }

    template<
        class U
        gsl_ENABLE_IF_(( std::is_same< typename std::remove_cv< U >::type, value_type >::value ))
    >
    gsl_api gsl_constexpr14 bool operator==( span_iterator< U > const & rhs ) const
    {
        gsl_ExpectsDebug( begin_ == rhs.begin_ && end_ == rhs.end_ );
        return current_ == rhs.current_;
    }

    template<
        class U
        gsl_ENABLE_IF_(( std::is_same< typename std::remove_cv< U >::type, value_type >::value ))
    >
    gsl_api gsl_constexpr14 bool operator!=( span_iterator< U > const & rhs ) const
    {
        return !( *this == rhs );
    }

    template<
        class U
        gsl_ENABLE_IF_(( std::is_same< typename std::remove_cv< U >::type, value_type >::value ))
    >
    gsl_api gsl_constexpr14 bool operator<( span_iterator< U > const & rhs ) const
    {
        gsl_ExpectsDebug( begin_ == rhs.begin_ && end_ == rhs.end_ );
        return current_ < rhs.current_;
    }

    template<
        class U
        gsl_ENABLE_IF_(( std::is_same< typename std::remove_cv< U >::type, value_type >::value ))
    >
    gsl_api gsl_constexpr14 bool operator>( span_iterator< U > const & rhs ) const
    {
        return rhs < *this;
    }

    template<
        class U
        gsl_ENABLE_IF_(( std::is_same< typename std::remove_cv< U >::type, value_type >::value ))
    >
    gsl_api gsl_constexpr14 bool operator<=( span_iterator< U > const & rhs ) const
    {
        return !( rhs < *this );
    }

    template<
        class U
        gsl_ENABLE_IF_(( std::is_same< typename std::remove_cv< U >::type, value_type >::value ))
    >
    gsl_api gsl_constexpr14 bool operator>=( span_iterator< U > const & rhs ) const
    {
        return !( *this < rhs );
    }

# ifdef gsl_COMPILER_MS_STL_VERSION
    // MSVC++ iterator debugging support; allows STL algorithms in 15.8+
    // to unwrap span_iterator to a pointer type after a range check in STL
    // algorithm calls.
    friend gsl_api gsl_constexpr14 void _Verify_range( span_iterator lhs, span_iterator rhs ) gsl_noexcept
    {
        // test that [lhs, rhs) forms a valid range inside an STL algorithm
        gsl_Expects( lhs.begin_ == rhs.begin_ // range spans have to match
                && lhs.end_ == rhs.end_ &&
                lhs.current_ <= rhs.current_ ); // range must not be transposed
    }

    gsl_api gsl_constexpr14 void _Verify_offset( difference_type const n ) const gsl_noexcept
    {
        // test that *this + n is within the range of this call
        if ( n != 0 ) gsl_Expects( begin_ && current_ && end_ );
        if ( n > 0 )  gsl_Expects( end_ - current_ >= n );
        if ( n < 0 )  gsl_Expects( current_ - begin_ >= -n );
    }

    gsl_SUPPRESS_MSGSL_WARNING(bounds.1)
    gsl_api gsl_constexpr14 pointer _Unwrapped() const gsl_noexcept
    {
        // after seeking *this to a high water mark, or using one of the
        // _Verify_xxx functions above, unwrap this span_iterator to a raw
        // pointer
        return current_;
    }

    // Tell the STL that span_iterator should not be unwrapped if it can't
    // validate in advance, even in release / optimized builds:
#  if gsl_CPP17_OR_GREATER
    static constexpr bool _Unwrap_when_unverified = false;
#  else
    static gsl_constexpr const bool _Unwrap_when_unverified = false;
#  endif
    gsl_SUPPRESS_MSGSL_WARNING(con.3) // TODO: false positive
    gsl_api gsl_constexpr14 void _Seek_to( pointer const p ) gsl_noexcept
    {
        // adjust the position of *this to previously verified location p
        // after _Unwrapped
        current_ = p;
    }
# endif

# if gsl_HAVE( IS_DEFAULT )
    pointer begin_ = nullptr;
    pointer end_ = nullptr;
    pointer current_ = nullptr;
# else // ! gsl_HAVE( IS_DEFAULT )
    pointer begin_;
    pointer end_;
    pointer current_;
# endif // gsl_HAVE( IS_DEFAULT )

# if gsl_STDLIB_CPP11_OR_GREATER
    template< class Ptr >
    friend struct std::pointer_traits;
# endif // gsl_STDLIB_CPP11_OR_GREATER
};

template< gsl_CONFIG_SPAN_INDEX_TYPE Ext >
class extent_type
{
public:
    typedef gsl_CONFIG_SPAN_INDEX_TYPE size_type;

# if gsl_HAVE( IS_DEFAULT )
    gsl_constexpr extent_type() gsl_noexcept = default;
# else // ! gsl_HAVE( IS_DEFAULT )
    gsl_constexpr extent_type() gsl_noexcept { }
# endif // gsl_HAVE( IS_DEFAULT )

    gsl_api gsl_constexpr14 gsl_explicit extent_type( extent_type< dynamic_extent > );

    gsl_api gsl_constexpr14 gsl_explicit extent_type( size_type size ) { gsl_Expects( size == Ext ); }

    gsl_api gsl_constexpr size_type size() const gsl_noexcept { return Ext; }

private:
# if gsl_CPP17_OR_GREATER
    static constexpr size_type size_ = Ext; // static size equal to Ext
# else
    static gsl_constexpr const size_type size_ = Ext; // static size equal to Ext
# endif
};

template<>
class extent_type< dynamic_extent >
{
public:
    typedef gsl_CONFIG_SPAN_INDEX_TYPE size_type;

    template< size_type Other >
    gsl_api gsl_constexpr gsl_explicit extent_type( extent_type< Other > ext )
        : size_( ext.size() )
    {
    }

    gsl_api gsl_constexpr14 gsl_explicit extent_type( size_type size )
        : size_( size )
    {
        gsl_Expects( size != dynamic_extent );
    }

    gsl_api gsl_constexpr size_type size() const gsl_noexcept { return size_; }

private:
    size_type size_;
};

template< gsl_CONFIG_SPAN_INDEX_TYPE Ext >
gsl_api gsl_constexpr14 extent_type< Ext >::extent_type( extent_type< dynamic_extent > ext )
{
    gsl_Expects( ext.size() == Ext );
}

template< class T, gsl_CONFIG_SPAN_INDEX_TYPE Extent, gsl_CONFIG_SPAN_INDEX_TYPE Offset, gsl_CONFIG_SPAN_INDEX_TYPE Count >
struct calculate_subspan_type
{
    typedef span< T, Count != dynamic_extent
                        ? Count
                        : ( Extent != dynamic_extent ? Extent - Offset : Extent ) > type;
};

template< class T, class U, gsl_CONFIG_SPAN_INDEX_TYPE Extent, bool StaticExtent >
struct calculate_recast_span_type_0
{
    typedef span< U, dynamic_extent > type;
};
template< class T, class U, gsl_CONFIG_SPAN_INDEX_TYPE Extent >
struct calculate_recast_span_type_0< T, U, Extent, true >
{
    typedef span< U, Extent * sizeof( T ) / sizeof( U ) > type;
};
template< class T, class U, gsl_CONFIG_SPAN_INDEX_TYPE Extent >
struct calculate_recast_span_type : calculate_recast_span_type_0< T, U, Extent, Extent != dynamic_extent >
{
};

} // namespace detail

// [span], class template span
template< class T, gsl_CONFIG_SPAN_INDEX_TYPE Extent >
class span
{
public:
    // constants and types
    typedef T element_type;
    typedef typename std11::remove_cv< T >::type value_type;
    typedef gsl_CONFIG_SPAN_INDEX_TYPE size_type;
    typedef gsl_CONFIG_SPAN_INDEX_TYPE index_type;
    typedef element_type * pointer;
    typedef element_type const * const_pointer;
    typedef element_type & reference;
    typedef element_type const & const_reference;
    typedef std::ptrdiff_t difference_type;

    typedef detail::span_iterator< T > iterator;
    typedef detail::span_iterator< T const > const_iterator;
    typedef std::reverse_iterator< iterator > reverse_iterator;
    typedef std::reverse_iterator< const_iterator > const_reverse_iterator;

# if gsl_CPP17_OR_GREATER
    static constexpr size_type extent{ Extent };
# else
    static gsl_constexpr const size_type extent = Extent;
# endif

    // [span.cons], span constructors, copy, assignment, and destructor
# if gsl_HAVE( TYPE_TRAITS ) && gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG )
    template<
        gsl_CONFIG_SPAN_INDEX_TYPE MyExtent = Extent
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( detail::is_allowed_extent_conversion< 0, MyExtent >::value ))
    >
# endif // gsl_HAVE( TYPE_TRAITS ) && gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG )
    gsl_api gsl_constexpr span() gsl_noexcept
        : storage_( detail::extent_type< 0 >( ) )
    {
    }

# if gsl_HAVE( TYPE_TRAITS ) && gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG ) && ! gsl_BETWEEN( gsl_COMPILER_MSVC_VERSION, 1, 140 )
    template<
        gsl_CONFIG_SPAN_INDEX_TYPE MyExtent = Extent
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( MyExtent != dynamic_extent ))
    >
    gsl_api gsl_constexpr14 gsl_explicit span( pointer ptr, size_type count )
        : storage_( ptr, count )
    {
        gsl_Expects( count == Extent );
    }
    template<
        gsl_CONFIG_SPAN_INDEX_TYPE MyExtent = Extent
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( MyExtent == dynamic_extent ))
    >
    gsl_api gsl_constexpr14 span( pointer ptr, size_type count )
        : storage_( ptr, count )
    {
    }

    template<
        gsl_CONFIG_SPAN_INDEX_TYPE MyExtent = Extent
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( MyExtent != dynamic_extent ))
    >
    gsl_api gsl_constexpr14 gsl_explicit span( iterator it, size_type count )
        : storage_( it.current_, count )
    {
        gsl_Expects( count == Extent );
        gsl_Expects( it.end_ - it.current_ == static_cast< difference_type >( Extent ) );
    }
    template<
        gsl_CONFIG_SPAN_INDEX_TYPE MyExtent = Extent
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( MyExtent == dynamic_extent ))
    >
    gsl_api gsl_constexpr14 span( iterator it, size_type count )
        : storage_( it.current_, count )
    {
        gsl_Expects( it.end_ - it.current_ >= static_cast< difference_type >( count ) );
    }

    template<
        gsl_CONFIG_SPAN_INDEX_TYPE MyExtent = Extent
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( MyExtent != dynamic_extent ))
    >
    gsl_api gsl_constexpr14 gsl_explicit span( pointer firstElem, pointer lastElem )
        : storage_( firstElem, gsl_lite::narrow_cast< size_type >( lastElem - firstElem ) )
    {
        gsl_Expects( lastElem - firstElem == static_cast< difference_type >( Extent ) );
    }
    template<
        gsl_CONFIG_SPAN_INDEX_TYPE MyExtent = Extent
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( MyExtent == dynamic_extent ))
    >
    gsl_api gsl_constexpr14 span( pointer firstElem, pointer lastElem )
        : storage_( firstElem, gsl_lite::narrow_cast< size_type >( lastElem - firstElem ) )
    {
        gsl_Expects( firstElem <= lastElem );
    }

    template<
        gsl_CONFIG_SPAN_INDEX_TYPE MyExtent = Extent
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( MyExtent != dynamic_extent ))
    >
    gsl_api gsl_constexpr14 gsl_explicit span( iterator firstElem, iterator lastElem )
        : storage_( firstElem.current_, gsl_lite::narrow_cast< size_type >( lastElem - firstElem ) )
    {
        gsl_Expects( lastElem - firstElem == static_cast< difference_type >( Extent ) );
    }
    template<
        gsl_CONFIG_SPAN_INDEX_TYPE MyExtent = Extent
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( MyExtent == dynamic_extent ))
    >
    gsl_api gsl_constexpr14 span( iterator firstElem, iterator lastElem )
        : storage_( firstElem.current_, gsl_lite::narrow_cast< size_type >( lastElem - firstElem ) )
    {
        gsl_Expects( firstElem <= lastElem );
    }
# else // !( gsl_HAVE( TYPE_TRAITS ) && gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG ) && ! gsl_BETWEEN( gsl_COMPILER_MSVC_VERSION, 1, 140 ) )
    gsl_api gsl_constexpr14 span( pointer ptr, size_type count )
        : storage_( ptr, count )
    {
        gsl_Expects( Extent == dynamic_extent || count == Extent );
    }
    gsl_api gsl_constexpr14 span( iterator it, size_type count )
        : storage_( it.current_, count )
    {
        gsl_Expects( Extent == dynamic_extent || count == Extent );
        gsl_Expects( it.end_ - it.current_ >= static_cast< difference_type >( count ) );
    }
    gsl_api gsl_constexpr14 span( pointer firstElem, pointer lastElem )
        : storage_( firstElem, gsl_lite::narrow_cast< size_type >( lastElem - firstElem ) )
    {
        gsl_Expects( firstElem <= lastElem );
        gsl_Expects( Extent == dynamic_extent || lastElem - firstElem == static_cast< difference_type >( Extent ) );
    }
    gsl_api gsl_constexpr14 span( iterator firstElem, iterator lastElem )
        : storage_( firstElem.current_, gsl_lite::narrow_cast< size_type >( lastElem - firstElem ) )
    {
        gsl_Expects( firstElem <= lastElem );
        gsl_Expects( Extent == dynamic_extent || lastElem - firstElem == static_cast< difference_type >( Extent ) );
    }
# endif // gsl_HAVE( TYPE_TRAITS ) && gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG ) && ! gsl_BETWEEN( gsl_COMPILER_MSVC_VERSION, 1, 140 )

    template<
        class U, std::size_t N
        gsl_ENABLE_IF_(( detail::is_allowed_extent_conversion< N, Extent >::value &&
                         detail::is_allowed_element_type_conversion< U, element_type >::value ))
    >
    gsl_api gsl_constexpr span( U (&arr)[N] ) gsl_noexcept
        : storage_( known_not_null( arr ), detail::extent_type< N >( ) )
    {
    }

# if gsl_HAVE( ARRAY )
    template<
        class U, std::size_t N
        gsl_ENABLE_IF_(( detail::is_allowed_extent_conversion< N, Extent >::value &&
                         detail::is_allowed_element_type_conversion< U, element_type >::value ))
    >
    gsl_api gsl_constexpr span( std::array<U, N> & arr ) gsl_noexcept
        : storage_( known_not_null( arr.data() ), detail::extent_type< N >( ) )
    {
    }
    template<
        class U, std::size_t N
        gsl_ENABLE_IF_(( detail::is_allowed_extent_conversion< N, Extent >::value &&
                         detail::is_allowed_element_type_conversion< U const , element_type >::value ))
    >
    gsl_api gsl_constexpr span( std::array<U, N> const & arr ) gsl_noexcept
        : storage_( known_not_null( arr.data() ), detail::extent_type< N >( ) )
    {
    }
# endif // gsl_HAVE( ARRAY )

# if gsl_HAVE( CONSTRAINED_SPAN_CONTAINER_CTOR )
#  if ! gsl_BETWEEN( gsl_COMPILER_MSVC_VERSION, 1, 140 )
    template< class Container, gsl_CONFIG_SPAN_INDEX_TYPE MyExtent = Extent
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( MyExtent != dynamic_extent &&
                              detail::is_compatible_container< Container, element_type >::value ))
    >
    gsl_api gsl_constexpr gsl_explicit span( Container & cont ) gsl_noexcept
        : storage_( std17::data( cont ), std17::size( cont ) )
    {
    }
    template< class Container, gsl_CONFIG_SPAN_INDEX_TYPE MyExtent = Extent
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( MyExtent == dynamic_extent &&
                              detail::is_compatible_container< Container, element_type >::value ))
    >
    gsl_api gsl_constexpr span( Container & cont ) gsl_noexcept
        : storage_( std17::data( cont ), std17::size( cont ) )
    {
    }

    template< class Container, gsl_CONFIG_SPAN_INDEX_TYPE MyExtent = Extent
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( MyExtent != dynamic_extent &&
                              std::is_const< element_type >::value &&
                              detail::is_compatible_container< Container, element_type >::value ))
    >
    gsl_api gsl_constexpr gsl_explicit span( Container const & cont ) gsl_noexcept
        : storage_( std17::data( cont ), std17::size( cont ) )
    {
    }
    template< class Container, gsl_CONFIG_SPAN_INDEX_TYPE MyExtent = Extent
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( MyExtent == dynamic_extent &&
                              std::is_const< element_type >::value &&
                              detail::is_compatible_container< Container, element_type >::value ))
    >
    gsl_api gsl_constexpr span( Container const & cont ) gsl_noexcept
        : storage_( std17::data( cont ), std17::size( cont ) )
    {
    }
#  else // gsl_BETWEEN( gsl_COMPILER_MSVC_VERSION, 1, 140 )
    template< class Container
        gsl_ENABLE_IF_(( detail::is_compatible_container< Container, element_type >::value ))
    >
    gsl_api gsl_constexpr span( Container & cont ) gsl_noexcept
        : storage_( std17::data( cont ), std17::size( cont ) )
    {
    }
    template< class Container, gsl_CONFIG_SPAN_INDEX_TYPE MyExtent = Extent
        gsl_ENABLE_IF_(( std::is_const< element_type >::value &&
                         detail::is_compatible_container< Container, element_type >::value ))
    >
    gsl_api gsl_constexpr span( Container const & cont ) gsl_noexcept
        : storage_( std17::data( cont ), std17::size( cont ) )
    {
    }
#  endif // ! gsl_BETWEEN( gsl_COMPILER_MSVC_VERSION, 1, 140 )
# elif gsl_HAVE( UNCONSTRAINED_SPAN_CONTAINER_CTOR )
    template< class Container >
    gsl_constexpr span( Container & cont )
        : storage_( cont.size() == 0 ? gsl_nullptr : gsl_ADDRESSOF( cont[0] ), cont.size() )
    {
    }
    template< class Container >
    gsl_constexpr span( Container const & cont )
        : storage_( cont.size() == 0 ? gsl_nullptr : gsl_ADDRESSOF( cont[0] ), cont.size() )
    {
    }
# endif

# if gsl_FEATURE_TO_STD( WITH_CONTAINER )
    template< class Container >
    gsl_constexpr span( with_container_t, Container & cont )
        : storage_( cont.size() == 0 ? gsl_nullptr : gsl_ADDRESSOF( cont[0] ), cont.size() )
    {
    }
    template< class Container >
    gsl_constexpr span( with_container_t, Container const & cont )
        : storage_( cont.size() == 0 ? gsl_nullptr : gsl_ADDRESSOF( cont[0] ), cont.size() )
    {
    }
# endif

# if gsl_HAVE( IS_DEFAULT ) && ! gsl_BETWEEN( gsl_COMPILER_GNUC_VERSION, 430, 600)
    gsl_constexpr span( span && ) gsl_noexcept = default;
    gsl_constexpr span( span const & ) = default;
# else
    gsl_api gsl_constexpr span( span const & other )
        : storage_( other.storage_ )
    {
    }
# endif

# if gsl_HAVE( IS_DEFAULT )
    gsl_constexpr14 span & operator=( span && ) gsl_noexcept = default;
    gsl_constexpr14 span & operator=( span const & ) gsl_noexcept = default;
# else
    gsl_constexpr14 span & operator=( span const & other ) gsl_noexcept
    {
        storage_ = other.storage_;
        return *this;
    }
# endif

# if gsl_HAVE( TYPE_TRAITS ) && gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG ) && ! gsl_BETWEEN( gsl_COMPILER_MSVC_VERSION, 1, 140 )
    template<
        class OtherElementType, gsl_CONFIG_SPAN_INDEX_TYPE OtherExtent, gsl_CONFIG_SPAN_INDEX_TYPE MyExtent = Extent
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( ( MyExtent == dynamic_extent || MyExtent == OtherExtent ) &&
                              detail::is_allowed_element_type_conversion< OtherElementType, element_type >::value ))
    >
    gsl_api gsl_constexpr span( span< OtherElementType, OtherExtent > const & other ) gsl_noexcept
        : storage_( other.data(), detail::extent_type< OtherExtent >( other.size() ) )
    {
    }
    template<
        class OtherElementType, gsl_CONFIG_SPAN_INDEX_TYPE OtherExtent, gsl_CONFIG_SPAN_INDEX_TYPE MyExtent = Extent
        // We *have* to use SFINAE with an NTTP arg here, otherwise the overload is ambiguous.
        gsl_ENABLE_IF_NTTP_(( MyExtent != dynamic_extent && OtherExtent == dynamic_extent &&
                              detail::is_allowed_element_type_conversion< OtherElementType, element_type >::value ))
    >
    gsl_api gsl_constexpr gsl_explicit span( span< OtherElementType, OtherExtent > const & other ) gsl_noexcept
        : storage_( other.data(), detail::extent_type< OtherExtent >( other.size() ) )
    {
    }
# else // !( gsl_HAVE( TYPE_TRAITS ) && gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG ) && ! gsl_BETWEEN( gsl_COMPILER_MSVC_VERSION, 1, 140 ) )
    template< class OtherElementType, gsl_CONFIG_SPAN_INDEX_TYPE OtherExtent >
    gsl_api gsl_constexpr14 span( span< OtherElementType, OtherExtent > const & other )
        : storage_( other.data(), detail::extent_type< OtherExtent >( other.size() ) )
    {
        gsl_STATIC_ASSERT_(
            Extent == dynamic_extent || OtherExtent == dynamic_extent || Extent == OtherExtent,
            "attempting copy-construction from incompatible span" );
        gsl_Expects( Extent == dynamic_extent || other.size() == Extent );
    }
# endif // gsl_HAVE( TYPE_TRAITS ) && gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG ) && ! gsl_BETWEEN( gsl_COMPILER_MSVC_VERSION, 1, 140 )

    // [span.sub], span subviews

    template< gsl_CONFIG_SPAN_INDEX_TYPE Count >
    gsl_NODISCARD gsl_api gsl_constexpr14 span< element_type, Count >
    first() const
    {
        gsl_STATIC_ASSERT_(
            Extent == dynamic_extent || Count <= Extent,
            "first() cannot extract more elements from a span than it contains" );
        gsl_Expects( static_cast<std::size_t>( Count ) <= static_cast<std::size_t>( size() ) );
        return span< element_type, Count >( data(), Count );
    }

    template< gsl_CONFIG_SPAN_INDEX_TYPE Count >
    gsl_SUPPRESS_MSGSL_WARNING(bounds.1)
    gsl_NODISCARD gsl_api gsl_constexpr14 span< element_type, Count >
    last() const
    {
        gsl_STATIC_ASSERT_(
            Extent == dynamic_extent || Count <= Extent,
            "last() cannot extract more elements from a span than it contains" );
        gsl_Expects( static_cast<std::size_t>( Count ) <= static_cast<std::size_t>( size() ) );
        return span< element_type, Count >( data() + ( size() - Count ), Count );
    }

    template< gsl_CONFIG_SPAN_INDEX_TYPE Offset, gsl_CONFIG_SPAN_INDEX_TYPE Count >
    gsl_SUPPRESS_MSGSL_WARNING(bounds.1)
    gsl_NODISCARD gsl_api gsl_constexpr14 typename detail::calculate_subspan_type< T, Extent, Offset, Count >::type
    subspan() const
    {
        gsl_STATIC_ASSERT_(
            Extent == dynamic_extent || ( Extent >= Offset && ( Count == dynamic_extent ||
                                                                Count <= Extent - Offset ) ),
            "subspan() cannot extract more elements from a span than it contains.");
        gsl_Expects( static_cast<std::size_t>( size() ) >= static_cast<std::size_t>( Offset ) &&
                     ( Count == dynamic_extent || static_cast<std::size_t>( Count ) <= static_cast<std::size_t>( size() ) - static_cast<std::size_t>( Offset ) ) );
        typedef typename detail::calculate_subspan_type< T, Extent, Offset, Count >::type type;
        return type( data() + Offset, Count == dynamic_extent ? size() - Offset : Count );
    }
    template< gsl_CONFIG_SPAN_INDEX_TYPE Offset >
    gsl_SUPPRESS_MSGSL_WARNING(bounds.1)
    gsl_NODISCARD gsl_api gsl_constexpr14 typename detail::calculate_subspan_type< T, Extent, Offset, dynamic_extent >::type
    subspan() const
    {
        gsl_STATIC_ASSERT_(
            Extent == dynamic_extent || Extent >= Offset,
            "subspan() cannot extract more elements from a span than it contains.");
        gsl_Expects( static_cast<std::size_t>( size() ) >= static_cast<std::size_t>( Offset ) );
        typedef typename detail::calculate_subspan_type< T, Extent, Offset, dynamic_extent >::type type;
        return type( data() + Offset, size() - Offset );
    }

    gsl_NODISCARD gsl_api gsl_constexpr14 span< element_type, dynamic_extent >
    first( size_type count ) const
    {
        gsl_Expects( static_cast<std::size_t>( count ) <= static_cast<std::size_t>( size() ) );
        return span< element_type, dynamic_extent >( data(), count );
    }
    gsl_NODISCARD gsl_api gsl_constexpr14 span< element_type, dynamic_extent >
    last( size_type count ) const
    {
        gsl_Expects( static_cast<std::size_t>( count ) <= static_cast<std::size_t>( size() ) );
        return make_subspan( size() - count, dynamic_extent, subspan_selector< Extent >() );
    }

    gsl_NODISCARD gsl_api gsl_constexpr14 span<element_type, dynamic_extent>
    subspan(size_type offset, size_type count = dynamic_extent) const
    {
        return make_subspan(offset, count, subspan_selector< Extent >() );
    }

    // [span.obs], span observers

    gsl_NODISCARD gsl_api gsl_constexpr size_type
    size() const gsl_noexcept
    {
        return storage_.size();
    }
    gsl_NODISCARD gsl_api gsl_constexpr std::ptrdiff_t
    ssize() const gsl_noexcept
    {
        return gsl_lite::narrow_cast< std::ptrdiff_t >( storage_.size() );
    }

    gsl_NODISCARD gsl_api gsl_constexpr size_type
    size_bytes() const gsl_noexcept
    {
        return size() * sizeof(element_type);
    }

    gsl_NODISCARD gsl_api gsl_constexpr bool
    empty() const gsl_noexcept
    {
        return size() == 0;
    }

    // [span.elem], span element access

    gsl_SUPPRESS_MSGSL_WARNING(bounds.1)
    gsl_NODISCARD gsl_api gsl_constexpr14 reference
    operator[](size_type idx) const
    {
        gsl_Expects( static_cast<std::size_t>( idx ) < static_cast<std::size_t>( size() ) );
        return storage_.data()[ idx ];
    }

    gsl_NODISCARD gsl_api gsl_constexpr14
    reference front() const
    {
        gsl_Expects( size() > 0 );
        return storage_.data()[ 0 ];
    }

    gsl_NODISCARD gsl_api gsl_constexpr14
    reference back() const
    {
        gsl_Expects( size() > 0 );
        return storage_.data()[ size() - 1 ];
    }

    gsl_NODISCARD gsl_api gsl_constexpr pointer
    data() const gsl_noexcept
    {
        return storage_.data();
    }

    // [span.iter], span iterator support

    gsl_NODISCARD gsl_api gsl_constexpr14 iterator
    begin() const gsl_noexcept
    {
        const pointer data = storage_.data();
        gsl_SUPPRESS_MSGSL_WARNING(bounds.1)
        return iterator( data, data + size(), data );
    }
    gsl_NODISCARD gsl_api gsl_constexpr14 iterator
    end() const gsl_noexcept
    {
        const pointer data = storage_.data();
        gsl_SUPPRESS_MSGSL_WARNING(bounds.1)
        const pointer endData = data + storage_.size();
        return iterator( data, endData, endData );
    }
    gsl_NODISCARD gsl_api gsl_constexpr14 const_iterator
    cbegin() const gsl_noexcept
    {
        const pointer data = storage_.data();
        gsl_SUPPRESS_MSGSL_WARNING(bounds.1)
        return const_iterator( data, data + size(), data );
    }
    gsl_NODISCARD gsl_api gsl_constexpr14 const_iterator
    cend() const gsl_noexcept
    {
        const pointer data = storage_.data();
        gsl_SUPPRESS_MSGSL_WARNING(bounds.1)
        const pointer endData = data + storage_.size();
        return const_iterator( data, endData, endData );
    }

    gsl_NODISCARD gsl_api gsl_constexpr14 reverse_iterator
    rbegin() const gsl_noexcept
    {
        return reverse_iterator( end() );
    }
    gsl_NODISCARD gsl_api gsl_constexpr14 reverse_iterator
    rend() const gsl_noexcept
    {
        return reverse_iterator( begin() );
    }

    gsl_NODISCARD gsl_api gsl_constexpr14 const_reverse_iterator
    crbegin() const gsl_noexcept
    {
        return const_reverse_iterator( cend() );
    }
    gsl_NODISCARD gsl_api gsl_constexpr14 const_reverse_iterator
    crend() const gsl_noexcept
    {
        return const_reverse_iterator( cbegin() );
    }

    gsl_constexpr14 void
    swap( span & other ) gsl_noexcept
    {
        std::swap( storage_, other.storage_ );
    }

# if ! gsl_DEPRECATE_TO_LEVEL( 9 )
    template< class U >
    gsl_DEPRECATED_MSG("as_span() member function is unsafe")
    gsl_NODISCARD gsl_api typename detail::calculate_recast_span_type< element_type, U, Extent >::type
    as_span() const
    {
        gsl_Expects( ( this->size_bytes() % sizeof( U ) ) == 0 );
        typedef typename detail::calculate_recast_span_type< element_type, U, Extent >::type type;
        return type( reinterpret_cast<U *>( data() ), size_bytes() / sizeof( U ) ); // NOLINT
    }
# endif // ! gsl_DEPRECATE_TO_LEVEL( 9 )

# ifdef gsl_COMPILER_MS_STL_VERSION
    // Tell MSVC how to unwrap spans in range-based-for
    gsl_api gsl_constexpr pointer _Unchecked_begin() const gsl_noexcept { return data(); }
    gsl_api gsl_constexpr pointer _Unchecked_end() const gsl_noexcept
    {
        gsl_SUPPRESS_MSGSL_WARNING(bounds.1)
        return data() + size();
    }
# endif // gsl_COMPILER_MS_STL_VERSION

private:
    // Needed to remove unnecessary null check in subspans
    struct known_not_null
    {
        pointer p;

        gsl_api gsl_constexpr known_not_null(pointer _p) gsl_noexcept
            : p( _p )
        {
        }
    };

    // this implementation detail class lets us take advantage of the
    // empty base class optimization to pay for only storage of a single
    // pointer in the case of fixed-size spans
    template< class ExtentType >
    class storage_type : public ExtentType
    {
    public:
        gsl_api gsl_constexpr storage_type( detail::extent_type< 0 > ext )
            : ExtentType( ext ), data_( gsl_nullptr )
        {
        }

        // known_not_null parameter is needed to remove unnecessary null check
        // in subspans and constructors from arrays
        template< class OtherExtentType >
        gsl_api gsl_constexpr storage_type( known_not_null data, OtherExtentType ext )
            : ExtentType( ext ), data_( data.p )
        {
        }

        template< class OtherExtentType >
        gsl_api gsl_constexpr14 storage_type( pointer data, OtherExtentType ext )
            : ExtentType( ext ), data_( data )
        {
            gsl_Expects( data || ExtentType::size() == 0 );
        }

        gsl_api gsl_constexpr pointer data() const gsl_noexcept { return data_; }

    private:
        pointer data_;
    };

    storage_type< detail::extent_type< Extent > > storage_;

    // The rest is needed to remove unnecessary null check
    // in subspans and constructors from arrays
    gsl_api gsl_constexpr span( known_not_null ptr, size_type count ) gsl_noexcept
        : storage_(ptr, count)
    {
    }

    template< gsl_CONFIG_SPAN_INDEX_TYPE CallerExtent >
    class subspan_selector
    {
    };

    template< gsl_CONFIG_SPAN_INDEX_TYPE CallerExtent >
    gsl_api gsl_constexpr14 span< element_type, dynamic_extent >
    make_subspan( size_type offset, size_type count, subspan_selector< CallerExtent > ) const
    {
        span< element_type, dynamic_extent > const tmp( *this );
        return tmp.subspan( offset, count );
    }

    gsl_SUPPRESS_MSGSL_WARNING(bounds.1)
    gsl_api gsl_constexpr14 span< element_type, dynamic_extent >
    make_subspan( size_type offset, size_type count, subspan_selector< dynamic_extent > ) const
    {
        gsl_Expects( static_cast<std::size_t>( size() ) >= static_cast<std::size_t>( offset ) );

        if ( count == dynamic_extent )
        {
            return span< element_type, dynamic_extent >( known_not_null( data() + offset ), size() - offset );
        }

        gsl_Expects( static_cast<std::size_t>( size() ) - static_cast<std::size_t>( offset ) >= static_cast<std::size_t>( count ) );
        return span< element_type, dynamic_extent >( known_not_null( data() + offset ), count );
    }
};

// class template argument deduction guides:

# if gsl_HAVE( DEDUCTION_GUIDES )   // gsl_CPP17_OR_GREATER

template<class Type, std::size_t Extent >
span( Type (&)[Extent] ) -> span< Type, Extent >;

template< class Type, std::size_t Size >
span( std::array<Type, Size> & ) -> span< Type, Size >;

template< class Type, std::size_t Size >
span( std::array<Type, Size> const & ) -> span< Type const, Size >;

template< class Container,
          class Element = std::remove_pointer_t<decltype( std::declval< Container & >().data()) > >
span( Container & ) -> span< Element >;

template< class Container,
          class Element = std::remove_pointer_t<decltype( std::declval< Container const & >().data()) > >
span( Container const & ) -> span< Element >;

# endif // gsl_HAVE( DEDUCTION_GUIDES )

# if ! gsl_CPP17_OR_GREATER
#  if defined( __clang__ ) && defined( _MSC_VER )
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wdeprecated" // Bug in clang-cl.exe which raises a C++17 -Wdeprecated warning about this static constexpr workaround in C++14 mode.
#  endif // defined( __clang__ ) && defined( _MSC_VER )
template< class T, gsl_CONFIG_SPAN_INDEX_TYPE Extent >
gsl_constexpr const typename span< T, Extent >::size_type span< T, Extent >::extent;
#  if defined( __clang__ ) && defined( _MSC_VER )
#   pragma clang diagnostic pop
#  endif // defined( __clang__ ) && defined( _MSC_VER )
# endif // ! gsl_CPP17_OR_GREATER

# if gsl_COMPILER_MSVC_VER
#  pragma warning( pop )
# endif // gsl_COMPILER_MSVC_VER

# if defined( __GNUC__ ) && __GNUC__ > 6
#  pragma GCC diagnostic pop
# endif // defined( __GNUC__ ) && __GNUC__ > 6

# if defined( __clang__ )
#  if __has_warning( "-Wunsafe-buffer-usage" )
#   pragma clang diagnostic pop
#  endif // __has_warning( "-Wunsafe-buffer-usage" )
# endif // defined( __clang__ )

// 26.7.3.7 Comparison operators [span.comparison]

# if gsl_CONFIG( ALLOWS_SPAN_COMPARISON )
#  if gsl_CONFIG( ALLOWS_NONSTRICT_SPAN_COMPARISON )

template< class T, class U, gsl_CONFIG_SPAN_INDEX_TYPE LExtent, gsl_CONFIG_SPAN_INDEX_TYPE RExtent >
gsl_SUPPRESS_MSGSL_WARNING(stl.1)
gsl_NODISCARD inline gsl_constexpr bool operator==( span<T, LExtent> const & l, span<U, RExtent> const & r )
{
    gsl_STATIC_ASSERT_( LExtent == RExtent || LExtent == dynamic_extent || RExtent == dynamic_extent, "comparing spans of mismatching sizes" );
    return  l.size()  == r.size()
        && ( l.data() == r.data() || detail::equal( l.begin(), l.end(), r.begin() ) );
}

template< class T, class U, gsl_CONFIG_SPAN_INDEX_TYPE LExtent, gsl_CONFIG_SPAN_INDEX_TYPE RExtent >
gsl_SUPPRESS_MSGSL_WARNING(stl.1)
gsl_NODISCARD inline gsl_constexpr bool operator< ( span<T, LExtent> const & l, span<U, RExtent> const & r )
{
    return detail::lexicographical_compare( l.begin(), l.end(), r.begin(), r.end() );
}

#  else // a.k.a. !gsl_CONFIG( ALLOWS_NONSTRICT_SPAN_COMPARISON )

template< class T, gsl_CONFIG_SPAN_INDEX_TYPE LExtent, gsl_CONFIG_SPAN_INDEX_TYPE RExtent >
gsl_SUPPRESS_MSGSL_WARNING(stl.1)
gsl_NODISCARD inline gsl_constexpr bool operator==( span<T, LExtent> const & l, span<T, RExtent> const & r )
{
    gsl_STATIC_ASSERT_( LExtent == RExtent || LExtent == dynamic_extent || RExtent == dynamic_extent, "comparing spans of mismatching sizes" );
    return  l.size()  == r.size()
        && ( l.data() == r.data() || detail::equal( l.begin(), l.end(), r.begin() ) );
}

template< class T, gsl_CONFIG_SPAN_INDEX_TYPE LExtent, gsl_CONFIG_SPAN_INDEX_TYPE RExtent >
gsl_SUPPRESS_MSGSL_WARNING(stl.1)
gsl_NODISCARD inline gsl_constexpr bool operator< ( span<T, LExtent> const & l, span<T, RExtent> const & r )
{
    return detail::lexicographical_compare( l.begin(), l.end(), r.begin(), r.end() );
}
#  endif // gsl_CONFIG( ALLOWS_NONSTRICT_SPAN_COMPARISON )

template< class T, class U, gsl_CONFIG_SPAN_INDEX_TYPE LExtent, gsl_CONFIG_SPAN_INDEX_TYPE RExtent >
gsl_NODISCARD inline gsl_constexpr bool
operator!=( span<T, LExtent> const & l, span<U, RExtent> const & r )
{
    return !( l == r );
}

template< class T, class U, gsl_CONFIG_SPAN_INDEX_TYPE LExtent, gsl_CONFIG_SPAN_INDEX_TYPE RExtent >
gsl_NODISCARD inline gsl_constexpr bool
operator<=( span<T, LExtent> const & l, span<U, RExtent> const & r )
{
    return !( r < l );
}

template< class T, class U, gsl_CONFIG_SPAN_INDEX_TYPE LExtent, gsl_CONFIG_SPAN_INDEX_TYPE RExtent >
gsl_NODISCARD inline gsl_constexpr bool
operator>( span<T, LExtent> const & l, span<U, RExtent> const & r )
{
    return ( r < l );
}

template< class T, class U, gsl_CONFIG_SPAN_INDEX_TYPE LExtent, gsl_CONFIG_SPAN_INDEX_TYPE RExtent >
gsl_NODISCARD inline gsl_constexpr bool
operator>=( span<T, LExtent> const & l, span<U, RExtent> const & r )
{
    return !( l < r );
}
# endif // gsl_CONFIG( ALLOWS_SPAN_COMPARISON )

// span algorithms

template< class T, gsl_CONFIG_SPAN_INDEX_TYPE Extent >
gsl_NODISCARD gsl_api inline gsl_constexpr std::size_t
size( span<T, Extent> const & spn )
{
    return static_cast<std::size_t>( spn.size() );
}

template< class T, gsl_CONFIG_SPAN_INDEX_TYPE Extent >
gsl_NODISCARD gsl_api inline gsl_constexpr std::ptrdiff_t
ssize( span<T, Extent> const & spn )
{
    return spn.ssize();
}

namespace detail {

template< class II, class N, class OI >
gsl_api gsl_constexpr14 inline OI copy_n( II first, N count, OI result )
{
    if ( count > 0 )
    {
        *result++ = *first;
        for ( N i = 1; i < count; ++i )
        {
            *result++ = *++first;
        }
    }
    return result;
}

} // namespace detail

template< class T, class U, gsl_CONFIG_SPAN_INDEX_TYPE LExtent, gsl_CONFIG_SPAN_INDEX_TYPE RExtent >
gsl_api gsl_constexpr14 inline void copy( span<T, LExtent> src, span<U, RExtent> dest )
{
# if gsl_CPP14_OR_GREATER // gsl_HAVE( TYPE_TRAITS ) (circumvent Travis clang 3.4)
    static_assert( std::is_assignable<U &, T const &>::value, "Cannot assign elements of source span to elements of destination span" );
# endif
    gsl_STATIC_ASSERT_( RExtent >= LExtent || LExtent == dynamic_extent || RExtent == dynamic_extent ,"incompatible span extents" );
    gsl_Expects( dest.size() >= src.size() );
    detail::copy_n( src.data(), src.size(), dest.data() );
}

# if gsl_FEATURE( BYTE )
// span creator functions (see ctors)

template< class T, gsl_CONFIG_SPAN_INDEX_TYPE Extent >
gsl_NODISCARD gsl_api inline span< const byte >
as_bytes( span< T, Extent > spn ) gsl_noexcept
{
    return span< const byte >( reinterpret_cast<const byte *>( spn.data() ), spn.size_bytes() ); // NOLINT
}

template< class T, gsl_CONFIG_SPAN_INDEX_TYPE Extent >
gsl_NODISCARD gsl_api inline span< byte >
as_writable_bytes( span< T, Extent > spn ) gsl_noexcept
{
    return span< byte >( reinterpret_cast<byte *>( spn.data() ), spn.size_bytes() ); // NOLINT
}
# endif // gsl_FEATURE( BYTE )

# if gsl_FEATURE_TO_STD( MAKE_SPAN )

template< class T >
gsl_NODISCARD gsl_api inline gsl_constexpr span<T>
make_span( T * ptr, typename span<T>::index_type count )
{
    return span<T>( ptr, count );
}

template< class T >
gsl_NODISCARD gsl_api inline gsl_constexpr span<T>
make_span( T * first, T * last )
{
    return span<T>( first, last );
}

template< class T, std::size_t N >
gsl_NODISCARD inline gsl_constexpr span<T, static_cast<gsl_CONFIG_SPAN_INDEX_TYPE>( N )>
make_span( T (&arr)[N] )
{
    return span<T, static_cast<gsl_CONFIG_SPAN_INDEX_TYPE>( N )>( arr );
}

#  if gsl_HAVE( ARRAY )

template< class T, std::size_t N >
gsl_NODISCARD inline gsl_constexpr span<T, static_cast<gsl_CONFIG_SPAN_INDEX_TYPE>( N )>
make_span( std::array<T, N> & arr )
{
    return span<T, static_cast<gsl_CONFIG_SPAN_INDEX_TYPE>( N )>( arr );
}

template< class T, std::size_t N >
gsl_NODISCARD inline gsl_constexpr span<const T, static_cast<gsl_CONFIG_SPAN_INDEX_TYPE>( N )>
make_span( std::array<T, N> const & arr )
{
    return span<const T, static_cast<gsl_CONFIG_SPAN_INDEX_TYPE>( N )>( arr );
}
#  endif

# if gsl_HAVE( CONSTRAINED_SPAN_CONTAINER_CTOR ) && gsl_HAVE( AUTO )

template< class Container, class EP = decltype( std17::data(std::declval<Container&>())) >
gsl_NODISCARD inline gsl_constexpr auto
make_span( Container & cont ) -> span< typename std::remove_pointer<EP>::type >
{
    return span< typename std::remove_pointer<EP>::type >( cont );
}

template< class Container, class EP = decltype( std17::data(std::declval<Container&>())) >
gsl_NODISCARD inline gsl_constexpr auto
make_span( Container const & cont ) -> span< const typename std::remove_pointer<EP>::type >
{
    return span< const typename std::remove_pointer<EP>::type >( cont );
}

#  else

template< class T >
inline span<T>
make_span( std::vector<T> & cont )
{
    return span<T>( cont.data(), cont.data() + cont.size() );
}

template< class T >
inline span<const T>
make_span( std::vector<T> const & cont )
{
    return span<const T>( cont.data(), cont.data() + cont.size() );
}

#  endif

#  if gsl_FEATURE_TO_STD( WITH_CONTAINER )

template< class Container >
gsl_NODISCARD inline gsl_constexpr span<typename Container::value_type>
make_span( with_container_t, Container & cont ) gsl_noexcept
{
    return span< typename Container::value_type >( with_container, cont );
}

template< class Container >
gsl_NODISCARD inline gsl_constexpr span<const typename Container::value_type>
make_span( with_container_t, Container const & cont ) gsl_noexcept
{
    return span< const typename Container::value_type >( with_container, cont );
}

#  endif // gsl_FEATURE_TO_STD( WITH_CONTAINER )
# endif // gsl_FEATURE_TO_STD( MAKE_SPAN )

# if gsl_FEATURE( BYTE ) && gsl_FEATURE_TO_STD( BYTE_SPAN )

template< class T >
gsl_NODISCARD gsl_api inline gsl_constexpr span<byte>
byte_span( T & t ) gsl_noexcept
{
    return span<byte>( reinterpret_cast<byte *>( &t ), sizeof(T) );
}

template< class T >
gsl_NODISCARD gsl_api inline gsl_constexpr span<const byte>
byte_span( T const & t ) gsl_noexcept
{
    return span<const byte>( reinterpret_cast<byte const *>( &t ), sizeof(T) );
}

# endif // gsl_FEATURE( BYTE ) && gsl_FEATURE_TO_STD( BYTE_SPAN )
#endif // gsl_FEATURE( SPAN )

#if gsl_FEATURE( STRING_SPAN )
//
// basic_string_span:
//

template< class T >
class basic_string_span;

namespace detail {

template< class T >
struct is_basic_string_span_oracle : std11::false_type {};

template< class T >
struct is_basic_string_span_oracle< basic_string_span<T> > : std11::true_type {};

template< class T >
struct is_basic_string_span : is_basic_string_span_oracle< typename std11::remove_cv<T>::type > {};

template< class T >
gsl_api inline gsl_constexpr14 std::size_t string_length( T * ptr, std::size_t max )
{
    if ( ptr == gsl_nullptr || max <= 0 )
        return 0;

    std::size_t len = 0;
    while ( len < max && ptr[len] ) // NOLINT
        ++len;

    return len;
}

} // namespace detail

//
// basic_string_span<> - A view of contiguous characters, replace (*,len).
//
template< class T >
class basic_string_span
{
public:
    typedef T element_type;
    typedef span<T> span_type;

    typedef typename span_type::size_type size_type;
    typedef typename span_type::index_type index_type;
    typedef typename span_type::difference_type difference_type;

    typedef typename span_type::pointer pointer ;
    typedef typename span_type::reference reference ;

    typedef typename span_type::iterator iterator ;
    typedef typename span_type::const_iterator const_iterator ;
    typedef typename span_type::reverse_iterator reverse_iterator;
    typedef typename span_type::const_reverse_iterator const_reverse_iterator;

    // construction:

# if gsl_HAVE( IS_DEFAULT )
    gsl_constexpr basic_string_span() gsl_noexcept = default;
# else
    gsl_api gsl_constexpr basic_string_span() gsl_noexcept {}
# endif

# if gsl_HAVE( NULLPTR )
    gsl_api gsl_constexpr basic_string_span( std::nullptr_t ) gsl_noexcept
    : span_( nullptr, static_cast<index_type>( 0 ) )
    {}
# endif

# ifdef __CUDACC_RELAXED_CONSTEXPR__
    gsl_api
# endif // __CUDACC_RELAXED_CONSTEXPR__
# if gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_DEPRECATED_MSG("basic_string_span<> is deprecated; use span<> instead")
# endif // gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_constexpr basic_string_span( pointer ptr )
    : span_( remove_z( ptr, (std::numeric_limits<index_type>::max)() ) )
    {}

# if gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_DEPRECATED_MSG("basic_string_span<> is deprecated; use span<> instead")
# endif // gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_api gsl_constexpr basic_string_span( pointer ptr, index_type count )
    : span_( ptr, count )
    {}

# if gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_DEPRECATED_MSG("basic_string_span<> is deprecated; use span<> instead")
# endif // gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_api gsl_constexpr basic_string_span( pointer firstElem, pointer lastElem )
    : span_( firstElem, lastElem )
    {}

    template< std::size_t N >
# if gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_DEPRECATED_MSG("basic_string_span<> is deprecated; use span<> instead")
# endif // gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_constexpr basic_string_span( element_type (&arr)[N] )
    : span_( remove_z( gsl_ADDRESSOF( arr[0] ), N ) )
    {}

# if gsl_HAVE( ARRAY )

    template< std::size_t N >
#  if gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_DEPRECATED_MSG("basic_string_span<> is deprecated; use span<> instead")
#  endif // gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_constexpr basic_string_span( std::array< typename std11::remove_const<element_type>::type, N> & arr )
    : span_( remove_z( arr ) )
    {}

    template< std::size_t N >
#  if gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_DEPRECATED_MSG("basic_string_span<> is deprecated; use span<> instead")
#  endif // gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_constexpr basic_string_span( std::array< typename std11::remove_const<element_type>::type, N> const & arr )
    : span_( remove_z( arr ) )
    {}

# endif // gsl_HAVE( ARRAY )

# if gsl_HAVE( CONSTRAINED_SPAN_CONTAINER_CTOR )

    // Exclude: array, [basic_string,] basic_string_span

    template< class Container
        gsl_ENABLE_IF_((
            ! detail::is_std_array< Container >::value
            && ! detail::is_basic_string_span< Container >::value
            && std::is_convertible< typename Container::pointer, pointer >::value
            && std::is_convertible< typename Container::pointer, decltype(std::declval<Container>().data()) >::value
        ))
    >
#  if gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_DEPRECATED_MSG("basic_string_span<> is deprecated; use span<> instead")
#  endif // gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_constexpr basic_string_span( Container & cont )
    : span_( ( cont ) )
    {}

    // Exclude: array, [basic_string,] basic_string_span

    template< class Container
        gsl_ENABLE_IF_((
            ! detail::is_std_array< Container >::value
            && ! detail::is_basic_string_span< Container >::value
            && std::is_convertible< typename Container::pointer, pointer >::value
            && std::is_convertible< typename Container::pointer, decltype(std::declval<Container const &>().data()) >::value
        ))
    >
#  if gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_DEPRECATED_MSG("basic_string_span<> is deprecated; use span<> instead")
#  endif // gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_constexpr basic_string_span( Container const & cont )
    : span_( ( cont ) )
    {}

# elif gsl_HAVE( UNCONSTRAINED_SPAN_CONTAINER_CTOR )

    template< class Container >
#  if gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_DEPRECATED_MSG("basic_string_span<> is deprecated; use span<> instead")
#  endif // gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_constexpr basic_string_span( Container & cont )
    : span_( cont )
    {}

    template< class Container >
#  if gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_DEPRECATED_MSG("basic_string_span<> is deprecated; use span<> instead")
#  endif // gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_constexpr basic_string_span( Container const & cont )
    : span_( cont )
    {}

# else

    template< class U, gsl_CONFIG_SPAN_INDEX_TYPE Extent >
#  if gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_DEPRECATED_MSG("basic_string_span<> is deprecated; use span<> instead")
#  endif // gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_api gsl_constexpr basic_string_span( span< U, Extent > const & rhs )
    : span_( rhs )
    {}

# endif

# if gsl_FEATURE_TO_STD( WITH_CONTAINER )

    template< class Container >
#  if gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_DEPRECATED_MSG("basic_string_span<> is deprecated; use span<> instead")
#  endif // gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_constexpr basic_string_span( with_container_t, Container & cont )
    : span_( with_container, cont )
    {}
# endif

# if gsl_HAVE( IS_DEFAULT )
#  if gsl_BETWEEN( gsl_COMPILER_GNUC_VERSION, 440, 600 )
    gsl_constexpr basic_string_span( basic_string_span const & ) = default;

    gsl_constexpr basic_string_span( basic_string_span && ) = default;
#  else
    gsl_constexpr basic_string_span( basic_string_span const & ) gsl_noexcept = default;

    gsl_constexpr basic_string_span( basic_string_span && ) gsl_noexcept = default;
#  endif
# endif

    template< class U
        gsl_ENABLE_IF_(( std::is_convertible<typename basic_string_span<U>::pointer, pointer>::value ))
    >
# if gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_DEPRECATED_MSG("basic_string_span<> is deprecated; use span<> instead")
# endif // gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_api gsl_constexpr basic_string_span( basic_string_span<U> const & rhs )
    : span_( reinterpret_cast<pointer>( rhs.data() ), rhs.length() ) // NOLINT
    {}

# if gsl_STDLIB_CPP11_120
    template< class U
        gsl_ENABLE_IF_(( std::is_convertible<typename basic_string_span<U>::pointer, pointer>::value ))
    >
#  if gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_DEPRECATED_MSG("basic_string_span<> is deprecated; use span<> instead")
#  endif // gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_api gsl_constexpr basic_string_span( basic_string_span<U> && rhs )
    : span_( reinterpret_cast<pointer>( rhs.data() ), rhs.length() ) // NOLINT
    {}
# endif // gsl_STDLIB_CPP11_120

    template< class CharTraits, class Allocator >
# if gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_DEPRECATED_MSG("basic_string_span<> is deprecated; use span<> instead")
# endif // gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_constexpr basic_string_span(
        std::basic_string< typename std11::remove_const<element_type>::type, CharTraits, Allocator > & str )
    : span_( gsl_ADDRESSOF( str[0] ), str.length() )
    {}

    template< class CharTraits, class Allocator >
# if gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_DEPRECATED_MSG("basic_string_span<> is deprecated; use span<> instead")
# endif // gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_constexpr basic_string_span(
        std::basic_string< typename std11::remove_const<element_type>::type, CharTraits, Allocator > const & str )
    : span_( gsl_ADDRESSOF( str[0] ), str.length() )
    {}

    // assignment:

# if gsl_HAVE( IS_DEFAULT )
    gsl_constexpr14 basic_string_span & operator=( basic_string_span const & ) gsl_noexcept = default;

    gsl_constexpr14 basic_string_span & operator=( basic_string_span && ) gsl_noexcept = default;
# endif

    // sub span:

    /*gsl_api*/ // currently disabled due to an apparent NVCC bug
    gsl_NODISCARD gsl_constexpr14 basic_string_span
    first( index_type count ) const
    {
        return span_.first( count );
    }

    /*gsl_api*/ // currently disabled due to an apparent NVCC bug
    gsl_NODISCARD gsl_constexpr14 basic_string_span
    last( index_type count ) const
    {
        return span_.last( count );
    }

    /*gsl_api*/ // currently disabled due to an apparent NVCC bug
    gsl_NODISCARD gsl_constexpr14 basic_string_span
    subspan( index_type offset ) const
    {
        return span_.subspan( offset );
    }

    /*gsl_api*/ // currently disabled due to an apparent NVCC bug
    gsl_NODISCARD gsl_constexpr14 basic_string_span
    subspan( index_type offset, index_type count ) const
    {
        return span_.subspan( offset, count );
    }

    // observers:

    gsl_NODISCARD gsl_api gsl_constexpr index_type
    length() const gsl_noexcept
    {
        return span_.size();
    }

    gsl_NODISCARD gsl_api gsl_constexpr index_type
    size() const gsl_noexcept
    {
        return span_.size();
    }

    gsl_NODISCARD gsl_api gsl_constexpr index_type
    length_bytes() const gsl_noexcept
    {
        return span_.size_bytes();
    }

    gsl_NODISCARD gsl_api gsl_constexpr index_type
    size_bytes() const gsl_noexcept
    {
        return span_.size_bytes();
    }

    gsl_NODISCARD gsl_api gsl_constexpr bool
    empty() const gsl_noexcept
    {
        return size() == 0;
    }

    gsl_NODISCARD gsl_api gsl_constexpr14 reference
    operator[]( index_type idx ) const
    {
        return span_[idx];
    }

    gsl_NODISCARD gsl_api gsl_constexpr14 reference
    front() const
    {
        return span_.front();
    }

    gsl_NODISCARD gsl_api gsl_constexpr14 reference
    back() const
    {
        return span_.back();
    }

    gsl_NODISCARD gsl_api gsl_constexpr pointer
    data() const gsl_noexcept
    {
        return span_.data();
    }

    gsl_NODISCARD gsl_api gsl_constexpr iterator
    begin() const gsl_noexcept
    {
        return span_.begin();
    }

    gsl_NODISCARD gsl_api gsl_constexpr iterator
    end() const gsl_noexcept
    {
        return span_.end();
    }

    gsl_NODISCARD gsl_constexpr17 reverse_iterator
    rbegin() const gsl_noexcept
    {
        return span_.rbegin();
    }

    gsl_NODISCARD gsl_constexpr17 reverse_iterator
    rend() const gsl_noexcept
    {
        return span_.rend();
    }

    // const version not in p0123r2:

    gsl_NODISCARD gsl_api gsl_constexpr const_iterator
    cbegin() const gsl_noexcept
    {
        return span_.cbegin();
    }

    gsl_NODISCARD gsl_api gsl_constexpr const_iterator
    cend() const gsl_noexcept
    {
        return span_.cend();
    }

    gsl_NODISCARD gsl_constexpr17 const_reverse_iterator
    crbegin() const gsl_noexcept
    {
        return span_.crbegin();
    }

    gsl_NODISCARD gsl_constexpr17 const_reverse_iterator
    crend() const gsl_noexcept
    {
        return span_.crend();
    }

private:
    gsl_api static gsl_constexpr14 span_type remove_z( pointer sz, std::size_t max )
    {
        return span_type( sz, detail::string_length( sz, max ) );
    }

# if gsl_HAVE( ARRAY )
    template< size_t N >
    gsl_NODISCARD static gsl_constexpr14 span_type
    remove_z( std::array<typename std11::remove_const<element_type>::type, N> & arr )
    {
        return remove_z( gsl_ADDRESSOF( arr[0] ), gsl_lite::narrow_cast< std::size_t >( N ) );
    }

    template< size_t N >
    gsl_NODISCARD static gsl_constexpr14 span_type
    remove_z( std::array<typename std11::remove_const<element_type>::type, N> const & arr )
    {
        return remove_z( gsl_ADDRESSOF( arr[0] ), gsl_lite::narrow_cast< std::size_t >( N ) );
    }
# endif

private:
    span_type span_;
};

// basic_string_span comparison functions:

# if gsl_CONFIG( ALLOWS_NONSTRICT_SPAN_COMPARISON )

template< class T, class U >
gsl_SUPPRESS_MSGSL_WARNING(stl.1)
gsl_NODISCARD inline gsl_constexpr14 bool
operator==( basic_string_span<T> const & l, U const & u ) gsl_noexcept
{
    const basic_string_span< typename std11::add_const<T>::type > r( u );

    return l.size() == r.size()
        && detail::equal( l.begin(), l.end(), r.begin() );
}

template< class T, class U >
gsl_SUPPRESS_MSGSL_WARNING(stl.1)
gsl_NODISCARD inline gsl_constexpr14 bool
operator<( basic_string_span<T> const & l, U const & u ) gsl_noexcept
{
    const basic_string_span< typename std11::add_const<T>::type > r( u );

    return detail::lexicographical_compare( l.begin(), l.end(), r.begin(), r.end() );
}

#  if gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG )

template< class T, class U
    gsl_ENABLE_IF_(( !detail::is_basic_string_span<U>::value ))
>
gsl_SUPPRESS_MSGSL_WARNING(stl.1)
gsl_NODISCARD inline gsl_constexpr14 bool
operator==( U const & u, basic_string_span<T> const & r ) gsl_noexcept
{
    const basic_string_span< typename std11::add_const<T>::type > l( u );

    return l.size() == r.size()
        && detail::equal( l.begin(), l.end(), r.begin() );
}

template< class T, class U
    gsl_ENABLE_IF_(( !detail::is_basic_string_span<U>::value ))
>
gsl_SUPPRESS_MSGSL_WARNING(stl.1)
gsl_NODISCARD inline gsl_constexpr14 bool
operator<( U const & u, basic_string_span<T> const & r ) gsl_noexcept
{
    const basic_string_span< typename std11::add_const<T>::type > l( u );

    return detail::lexicographical_compare( l.begin(), l.end(), r.begin(), r.end() );
}
#  endif

# else // ! gsl_CONFIG( ALLOWS_NONSTRICT_SPAN_COMPARISON )

template< class T >
gsl_SUPPRESS_MSGSL_WARNING(stl.1)
gsl_NODISCARD inline gsl_constexpr14 bool
operator==( basic_string_span<T> const & l, basic_string_span<T> const & r ) gsl_noexcept
{
    return l.size() == r.size()
        && detail::equal( l.begin(), l.end(), r.begin() );
}

template< class T >
gsl_SUPPRESS_MSGSL_WARNING(stl.1)
gsl_NODISCARD inline gsl_constexpr14 bool
operator<( basic_string_span<T> const & l, basic_string_span<T> const & r ) gsl_noexcept
{
    return detail::lexicographical_compare( l.begin(), l.end(), r.begin(), r.end() );
}

# endif // gsl_CONFIG( ALLOWS_NONSTRICT_SPAN_COMPARISON )

template< class T, class U >
gsl_NODISCARD inline gsl_constexpr14 bool
operator!=( basic_string_span<T> const & l, U const & r ) gsl_noexcept
{
    return !( l == r );
}

template< class T, class U >
gsl_NODISCARD inline gsl_constexpr14 bool
operator<=( basic_string_span<T> const & l, U const & r ) gsl_noexcept
{
# if gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG ) || ! gsl_CONFIG( ALLOWS_NONSTRICT_SPAN_COMPARISON )
    return !( r < l );
# else
    basic_string_span< typename std11::add_const<T>::type > rr( r );
    return !( rr < l );
# endif
}

template< class T, class U >
gsl_NODISCARD inline gsl_constexpr14 bool
operator>( basic_string_span<T> const & l, U const & r ) gsl_noexcept
{
# if gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG ) || ! gsl_CONFIG( ALLOWS_NONSTRICT_SPAN_COMPARISON )
    return ( r < l );
# else
    basic_string_span< typename std11::add_const<T>::type > rr( r );
    return ( rr < l );
# endif
}

template< class T, class U >
gsl_NODISCARD inline gsl_constexpr14 bool
operator>=( basic_string_span<T> const & l, U const & r ) gsl_noexcept
{
    return !( l < r );
}

# if gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG )

template< class T, class U
    gsl_ENABLE_IF_(( !detail::is_basic_string_span<U>::value ))
>
gsl_NODISCARD inline gsl_constexpr14 bool
operator!=( U const & l, basic_string_span<T> const & r ) gsl_noexcept
{
    return !( l == r );
}

template< class T, class U
    gsl_ENABLE_IF_(( !detail::is_basic_string_span<U>::value ))
>
gsl_NODISCARD inline gsl_constexpr14 bool
operator<=( U const & l, basic_string_span<T> const & r ) gsl_noexcept
{
    return !( r < l );
}

template< class T, class U
    gsl_ENABLE_IF_(( !detail::is_basic_string_span<U>::value ))
>
gsl_NODISCARD inline gsl_constexpr14 bool
operator>( U const & l, basic_string_span<T> const & r ) gsl_noexcept
{
    return ( r < l );
}

template< class T, class U
    gsl_ENABLE_IF_(( !detail::is_basic_string_span<U>::value ))
>
gsl_NODISCARD inline gsl_constexpr14 bool
operator>=( U const & l, basic_string_span<T> const & r ) gsl_noexcept
{
    return !( l < r );
}

# endif // gsl_HAVE( DEFAULT_FUNCTION_TEMPLATE_ARG )

# if gsl_FEATURE( BYTE )
// convert basic_string_span to byte span:

template< class T >
gsl_NODISCARD gsl_api inline span< const byte >
as_bytes( basic_string_span<T> spn ) gsl_noexcept
{
    return span< const byte >( reinterpret_cast<const byte *>( spn.data() ), spn.size_bytes() ); // NOLINT
}
# endif // gsl_FEATURE( BYTE )
#endif // gsl_FEATURE( STRING_SPAN )

//
// String types:
//

typedef char * zstring;
typedef const char * czstring;

# if gsl_HAVE( WCHAR )
typedef wchar_t * wzstring;
typedef const wchar_t * cwzstring;
# endif

#ifdef __cpp_char8_t  // C++20
typedef char8_t * u8zstring;
typedef const char8_t * cu8zstring;
#endif

#if gsl_CPP11_140
typedef char16_t * u16zstring;
typedef const char16_t * cu16zstring;
typedef char32_t * u32zstring;
typedef const char32_t * cu32zstring;
#endif

#if gsl_FEATURE( STRING_SPAN )

typedef basic_string_span< char > string_span;
typedef basic_string_span< char const > cstring_span;

# if gsl_HAVE( WCHAR )
typedef basic_string_span< wchar_t > wstring_span;
typedef basic_string_span< wchar_t const > cwstring_span;
# endif

// to_string() allow (explicit) conversions from string_span to string

# if 0

template< class T >
inline std::basic_string< typename std::remove_const<T>::type > to_string( basic_string_span<T> spn )
{
     std::string( spn.data(), spn.length() );
}

# else

gsl_NODISCARD inline std::string
to_string( string_span const & spn )
{
    return std::string( spn.data(), static_cast<std::size_t>( spn.length() ) );
}

gsl_NODISCARD inline std::string
to_string( cstring_span const & spn )
{
    return std::string( spn.data(), static_cast<std::size_t>( spn.length() ) );
}

#  if gsl_HAVE( WCHAR )

gsl_NODISCARD inline std::wstring
to_string( wstring_span const & spn )
{
    return std::wstring( spn.data(), static_cast<std::size_t>( spn.length() ) );
}

gsl_NODISCARD inline std::wstring
to_string( cwstring_span const & spn )
{
    return std::wstring( spn.data(), static_cast<std::size_t>( spn.length() ) );
}

#  endif // gsl_HAVE( WCHAR )
# endif // to_string()

//
// Stream output for string_span types
//

namespace detail {

template< class Stream >
void write_padding( Stream & os, std::streamsize n )
{
    for ( std::streamsize i = 0; i < n; ++i )
        os.rdbuf()->sputc( os.fill() );
}

template< class Stream, class Span >
Stream & write_to_stream( Stream & os, Span const & spn )
{
    typename Stream::sentry sentry( os );

    if ( !os )
        return os;

    const std::streamsize length = gsl_lite::narrow_failfast<std::streamsize>( spn.length() );

    // Whether, and how, to pad
    const bool pad = ( length < os.width() );
    const bool left_pad = pad && ( os.flags() & std::ios_base::adjustfield ) == std::ios_base::right;

    if ( left_pad )
        detail::write_padding( os, os.width() - length );

    // Write span characters
    os.rdbuf()->sputn( spn.data(), length );

    if ( pad && !left_pad )
        detail::write_padding( os, os.width() - length );

    // Reset output stream width
    os.width(0);

    return os;
}

} // namespace detail

template< typename Traits >
std::basic_ostream< char, Traits > & operator<<( std::basic_ostream< char, Traits > & os, string_span const & spn )
{
    return detail::write_to_stream( os, spn );
}

template< typename Traits >
std::basic_ostream< char, Traits > & operator<<( std::basic_ostream< char, Traits > & os, cstring_span const & spn )
{
    return detail::write_to_stream( os, spn );
}

# if gsl_HAVE( WCHAR )

template< typename Traits >
std::basic_ostream< wchar_t, Traits > & operator<<( std::basic_ostream< wchar_t, Traits > & os, wstring_span const & spn )
{
    return detail::write_to_stream( os, spn );
}

template< typename Traits >
std::basic_ostream< wchar_t, Traits > & operator<<( std::basic_ostream< wchar_t, Traits > & os, cwstring_span const & spn )
{
    return detail::write_to_stream( os, spn );
}

# endif // gsl_HAVE( WCHAR )

//
// ensure_sentinel()
//
// Provides a way to obtain a span from a contiguous sequence
// that ends with a (non-inclusive) sentinel value.
//
// Will fail-fast if sentinel cannot be found before max elements are examined.
//
namespace detail {

template< class T, class SizeType, const T Sentinel >
gsl_constexpr14 static span<T> ensure_sentinel( T * seq, SizeType max = (std::numeric_limits<SizeType>::max)() )
{
    typedef T * pointer;

    gsl_SUPPRESS_MSVC_WARNING( 26429, "f.23: symbol 'cur' is never tested for nullness, it can be marked as not_null" )
    pointer cur = seq;

    while ( static_cast<SizeType>( cur - seq ) < max && *cur != Sentinel )
        ++cur;

    gsl_Expects( *cur == Sentinel );

    return span<T>( seq, gsl_lite::narrow_cast< typename span<T>::index_type >( cur - seq ) );
}
} // namespace detail

//
// ensure_z - creates a string_span for a czstring or cwzstring.
// Will fail fast if a null-terminator cannot be found before
// the limit of size_type.
//

template< class T >
gsl_NODISCARD inline gsl_constexpr14 span<T>
ensure_z( T * const & sz, size_t max = (std::numeric_limits<size_t>::max)() )
{
    return detail::ensure_sentinel<T, size_t, 0>( sz, max );
}

template< class T, size_t N >
gsl_NODISCARD inline gsl_constexpr14 span<T>
ensure_z( T (&sz)[N] )
{
    return ::gsl_lite::ensure_z( gsl_ADDRESSOF( sz[0] ), N );
}

# if gsl_HAVE( TYPE_TRAITS )

template< class Container >
gsl_NODISCARD inline gsl_constexpr14 span< typename std::remove_pointer<typename Container::pointer>::type >
ensure_z( Container & cont )
{
    return ::gsl_lite::ensure_z( cont.data(), cont.length() );
}
# endif

//
// basic_zstring_span<> - A view of contiguous null-terminated characters, replace (*,len).
//

template< class T >
class basic_zstring_span
{
public:
    typedef T element_type;
    typedef span<T> span_type;

    typedef typename span_type::index_type index_type;
    typedef typename span_type::difference_type difference_type;

    typedef element_type * czstring_type;
    typedef basic_string_span<element_type> string_span_type;

# if gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_DEPRECATED_MSG("basic_zstring_span<> is deprecated")
# endif // gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_api gsl_constexpr14 basic_zstring_span( span_type s )
        : span_( s )
    {
        // expects a zero-terminated span
        gsl_Expects( s.back() == '\0' );
    }

# if gsl_HAVE( IS_DEFAULT )
    gsl_constexpr basic_zstring_span( basic_zstring_span const & ) = default;
    gsl_constexpr basic_zstring_span( basic_zstring_span &&      ) = default;
    gsl_constexpr14 basic_zstring_span & operator=( basic_zstring_span const & ) = default;
    gsl_constexpr14 basic_zstring_span & operator=( basic_zstring_span &&      ) = default;
# else
    gsl_api gsl_constexpr basic_zstring_span( basic_zstring_span const & other) : span_ ( other.span_ ) {}
    gsl_api gsl_constexpr basic_zstring_span & operator=( basic_zstring_span const & other ) { span_ = other.span_; return *this; }
# endif

    gsl_NODISCARD gsl_api gsl_constexpr bool
    empty() const gsl_noexcept
    {
        return false;
    }

# if gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_DEPRECATED_MSG("basic_zstring_span<> is deprecated")
# endif // gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_NODISCARD gsl_api gsl_constexpr string_span_type
    as_string_span() const gsl_noexcept
    {
        return string_span_type( span_.data(), span_.size() - 1 );
    }

# if gsl_DEPRECATE_TO_LEVEL( 7 )
    gsl_DEPRECATED_MSG("basic_zstring_span<> is deprecated")
# endif // gsl_DEPRECATE_TO_LEVEL( 7 )
   /*gsl_api*/ // currently disabled due to an apparent NVCC bug
    gsl_NODISCARD gsl_constexpr string_span_type
    ensure_z() const
    {
        return ::gsl_lite::ensure_z(span_.data(), span_.size());
    }

    gsl_NODISCARD gsl_api gsl_constexpr czstring_type
    assume_z() const gsl_noexcept
    {
        return span_.data();
    }

private:
    span_type span_;
};

//
// zString types:
//

typedef basic_zstring_span< char > zstring_span;
typedef basic_zstring_span< char const > czstring_span;

# if gsl_HAVE( WCHAR )
typedef basic_zstring_span< wchar_t > wzstring_span;
typedef basic_zstring_span< wchar_t const > cwzstring_span;
# endif
#endif // gsl_FEATURE( STRING_SPAN )

} // namespace gsl_lite

#if gsl_HAVE( HASH )

//
// std::hash specializations for GSL types
//

namespace gsl_lite {

namespace detail {

//
// Helper struct for std::hash specializations
//

template<bool Condition>
struct conditionally_enabled_hash
{

};

// disabled as described in [unord.hash]
template<>
struct conditionally_enabled_hash< false >
{
gsl_is_delete_access:
    conditionally_enabled_hash() gsl_is_delete;
    conditionally_enabled_hash( conditionally_enabled_hash const & ) gsl_is_delete;
    conditionally_enabled_hash( conditionally_enabled_hash && ) gsl_is_delete;
    conditionally_enabled_hash & operator=( conditionally_enabled_hash const & ) gsl_is_delete;
    conditionally_enabled_hash & operator=( conditionally_enabled_hash && ) gsl_is_delete;
};

} // namespace detail

} // namespace gsl_lite

namespace std {

template< class T >
struct hash< ::gsl_lite::not_null< T > > : public ::gsl_lite::detail::conditionally_enabled_hash< is_default_constructible< hash< T > >::value >
{
public:
    gsl_NODISCARD std::size_t
    operator()( ::gsl_lite::not_null<T> const & v ) const
    // hash function is not `noexcept` because `as_nullable()` has preconditions
    {
        return hash<T>()( ::gsl_lite::as_nullable( v ) );
    }
};
template< class T >
struct hash< ::gsl_lite::not_null< T* > >
{
public:
    gsl_NODISCARD std::size_t
    operator()( ::gsl_lite::not_null< T* > const & v ) const gsl_noexcept
    {
        return hash<T*>()( ::gsl_lite::as_nullable( v ) );
    }
};

# if gsl_FEATURE( BYTE )
template<>
struct hash< ::gsl_lite::byte >
{
public:
    gsl_NODISCARD std::size_t operator()( ::gsl_lite::byte v ) const gsl_noexcept
    {
#  if gsl_CONFIG_DEFAULTS_VERSION >= 1
        return std::hash<unsigned char>{ }( ::gsl_lite::to_uchar( v ) );
#  else // gsl_CONFIG_DEFAULTS_VERSION < 1
            // Keep the old hashing algorithm if legacy defaults are used.
        return ::gsl_lite::to_integer<std::size_t>( v );
#  endif // gsl_CONFIG_DEFAULTS_VERSION >= 1
    }
};
# endif // gsl_FEATURE( BYTE )

} // namespace std

#endif // gsl_HAVE( HASH )

#if gsl_FEATURE( SPAN ) && gsl_STDLIB_CPP11_OR_GREATER
namespace std {

template< class T >
struct pointer_traits< ::gsl_lite::detail::span_iterator< T > >
{
    typedef ::gsl_lite::detail::span_iterator< T > pointer;
    typedef T element_type;
    typedef ptrdiff_t difference_type;

    static gsl_constexpr element_type * to_address( pointer const i ) gsl_noexcept { return i.current_; }
};

} // namespace std
#endif // gsl_FEATURE( SPAN ) && gsl_STDLIB_CPP11_OR_GREATER

#if gsl_FEATURE( GSL_COMPATIBILITY_MODE )

// Enable GSL compatibility mode by aliasing all symbols in the `gsl` namespace and defining the unprefixed
// macros `Expects()` and `Ensures()`.
//
// gsl-lite can generally coexist with Microsoft GSL. However, if GSL compatibility mode is enabled,
// gsl-lite cannot be used in the same translation unit as Microsoft GSL.
// (Link-time compatibility is unaffected; different translation units that use either Microsoft GSL
// or gsl-lite may be linked together.)

namespace gsl = ::gsl_lite;

#define Expects( x )  gsl_Expects( x )
#define Ensures( x )  gsl_Ensures( x )

#endif // gsl_FEATURE( GSL_COMPATIBILITY_MODE )

gsl_RESTORE_MSVC_WARNINGS()
#if gsl_COMPILER_CLANG_VERSION || gsl_COMPILER_APPLECLANG_VERSION
# pragma clang diagnostic pop
#endif // gsl_COMPILER_CLANG_VERSION || gsl_COMPILER_APPLECLANG_VERSION
#if gsl_COMPILER_GNUC_VERSION
# pragma GCC diagnostic pop
#endif // gsl_COMPILER_GNUC_VERSION

// #undef internal macros
#undef gsl_STATIC_ASSERT_
#undef gsl_ENABLE_IF_R_
#undef gsl_ENABLE_IF_NTTP_
#undef gsl_ENABLE_IF_
#undef gsl_TRAILING_RETURN_TYPE_
#undef gsl_TRAILING_RETURN_TYPE_2_
#undef gsl_RETURN_DECLTYPE_
#undef gsl_BASELINE_CPP20_

#endif // GSL_LITE_GSL_LITE_HPP_INCLUDED

// end of file
