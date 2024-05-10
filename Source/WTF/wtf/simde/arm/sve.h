/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/arm/sve.h :: */
/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_ARM_SVE_H)
#define SIMDE_ARM_SVE_H

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/arm/sve/types.h :: */
/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

/* TODO: SVE2 is going to be a bit awkward with this setup.  We currently
 * either use SVE vectors or assume that the vector length is known at
 * compile-time.  For CPUs which provide SVE but not SVE2 we're going
 * to be getting scalable vectors, so we may need to loop through them.
 *
 * Currently I'm thinking we'll have a separate function for non-SVE
 * types.  We can call that function in a loop from an SVE version,
 * and we can call it once from a resolver.
 *
 * Unfortunately this is going to mean a lot of boilerplate for SVE,
 * which already has several variants of a lot of functions (*_z, *_m,
 * etc.), plus overloaded functions in C++ and generic selectors in C.
 *
 * Anyways, all this means that we're going to need to always define
 * the portable types.
 *
 * The good news is that at least we don't have to deal with
 * to/from_private functions; since the no-SVE versions will only be
 * called with non-SVE params.  */

#if !defined(SIMDE_ARM_SVE_TYPES_H)
#define SIMDE_ARM_SVE_TYPES_H

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/simde-common.h :: */
/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2017-2020 Evan Nemerson <evan@nemerson.com>
 *   2023      Yi-Yen Chung <eric681@andestech.com> (Copyright owned by Andes Technology)
 *   2023      Ju-Hung Li <jhlee@pllab.cs.nthu.edu.tw> (Copyright owned by NTHU pllab)
 */

#if !defined(SIMDE_COMMON_H)
#define SIMDE_COMMON_H

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/hedley.h :: */
/* Hedley - https://nemequ.github.io/hedley
 * Created by Evan Nemerson <evan@nemerson.com>
 *
 * To the extent possible under law, the author(s) have dedicated all
 * copyright and related and neighboring rights to this software to
 * the public domain worldwide. This software is distributed without
 * any warranty.
 *
 * For details, see <http://creativecommons.org/publicdomain/zero/1.0/>.
 * SPDX-License-Identifier: CC0-1.0
 */

#if !defined(HEDLEY_VERSION) || (HEDLEY_VERSION < 16)
#if defined(HEDLEY_VERSION)
#  undef HEDLEY_VERSION
#endif
#define HEDLEY_VERSION 16

#if defined(HEDLEY_STRINGIFY_EX)
#  undef HEDLEY_STRINGIFY_EX
#endif
#define HEDLEY_STRINGIFY_EX(x) #x

#if defined(HEDLEY_STRINGIFY)
#  undef HEDLEY_STRINGIFY
#endif
#define HEDLEY_STRINGIFY(x) HEDLEY_STRINGIFY_EX(x)

#if defined(HEDLEY_CONCAT_EX)
#  undef HEDLEY_CONCAT_EX
#endif
#define HEDLEY_CONCAT_EX(a,b) a##b

#if defined(HEDLEY_CONCAT)
#  undef HEDLEY_CONCAT
#endif
#define HEDLEY_CONCAT(a,b) HEDLEY_CONCAT_EX(a,b)

#if defined(HEDLEY_CONCAT3_EX)
#  undef HEDLEY_CONCAT3_EX
#endif
#define HEDLEY_CONCAT3_EX(a,b,c) a##b##c

#if defined(HEDLEY_CONCAT3)
#  undef HEDLEY_CONCAT3
#endif
#define HEDLEY_CONCAT3(a,b,c) HEDLEY_CONCAT3_EX(a,b,c)

#if defined(HEDLEY_VERSION_ENCODE)
#  undef HEDLEY_VERSION_ENCODE
#endif
#define HEDLEY_VERSION_ENCODE(major,minor,revision) (((major) * 1000000) + ((minor) * 1000) + (revision))

#if defined(HEDLEY_VERSION_DECODE_MAJOR)
#  undef HEDLEY_VERSION_DECODE_MAJOR
#endif
#define HEDLEY_VERSION_DECODE_MAJOR(version) ((version) / 1000000)

#if defined(HEDLEY_VERSION_DECODE_MINOR)
#  undef HEDLEY_VERSION_DECODE_MINOR
#endif
#define HEDLEY_VERSION_DECODE_MINOR(version) (((version) % 1000000) / 1000)

#if defined(HEDLEY_VERSION_DECODE_REVISION)
#  undef HEDLEY_VERSION_DECODE_REVISION
#endif
#define HEDLEY_VERSION_DECODE_REVISION(version) ((version) % 1000)

#if defined(HEDLEY_GNUC_VERSION)
#  undef HEDLEY_GNUC_VERSION
#endif
#if defined(__GNUC__) && defined(__GNUC_PATCHLEVEL__)
#  define HEDLEY_GNUC_VERSION HEDLEY_VERSION_ENCODE(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)
#elif defined(__GNUC__)
#  define HEDLEY_GNUC_VERSION HEDLEY_VERSION_ENCODE(__GNUC__, __GNUC_MINOR__, 0)
#endif

#if defined(HEDLEY_GNUC_VERSION_CHECK)
#  undef HEDLEY_GNUC_VERSION_CHECK
#endif
#if defined(HEDLEY_GNUC_VERSION)
#  define HEDLEY_GNUC_VERSION_CHECK(major,minor,patch) (HEDLEY_GNUC_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#  define HEDLEY_GNUC_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(HEDLEY_MSVC_VERSION)
#  undef HEDLEY_MSVC_VERSION
#endif
#if defined(_MSC_FULL_VER) && (_MSC_FULL_VER >= 140000000) && !defined(__ICL)
#  define HEDLEY_MSVC_VERSION HEDLEY_VERSION_ENCODE(_MSC_FULL_VER / 10000000, (_MSC_FULL_VER % 10000000) / 100000, (_MSC_FULL_VER % 100000) / 100)
#elif defined(_MSC_FULL_VER) && !defined(__ICL)
#  define HEDLEY_MSVC_VERSION HEDLEY_VERSION_ENCODE(_MSC_FULL_VER / 1000000, (_MSC_FULL_VER % 1000000) / 10000, (_MSC_FULL_VER % 10000) / 10)
#elif defined(_MSC_VER) && !defined(__ICL)
#  define HEDLEY_MSVC_VERSION HEDLEY_VERSION_ENCODE(_MSC_VER / 100, _MSC_VER % 100, 0)
#endif

#if defined(HEDLEY_MSVC_VERSION_CHECK)
#  undef HEDLEY_MSVC_VERSION_CHECK
#endif
#if !defined(HEDLEY_MSVC_VERSION)
#  define HEDLEY_MSVC_VERSION_CHECK(major,minor,patch) (0)
#elif defined(_MSC_VER) && (_MSC_VER >= 1400)
#  define HEDLEY_MSVC_VERSION_CHECK(major,minor,patch) (_MSC_FULL_VER >= ((major * 10000000) + (minor * 100000) + (patch)))
#elif defined(_MSC_VER) && (_MSC_VER >= 1200)
#  define HEDLEY_MSVC_VERSION_CHECK(major,minor,patch) (_MSC_FULL_VER >= ((major * 1000000) + (minor * 10000) + (patch)))
#else
#  define HEDLEY_MSVC_VERSION_CHECK(major,minor,patch) (_MSC_VER >= ((major * 100) + (minor)))
#endif

#if defined(HEDLEY_INTEL_VERSION)
#  undef HEDLEY_INTEL_VERSION
#endif
#if defined(__INTEL_COMPILER) && defined(__INTEL_COMPILER_UPDATE) && !defined(__ICL)
#  define HEDLEY_INTEL_VERSION HEDLEY_VERSION_ENCODE(__INTEL_COMPILER / 100, __INTEL_COMPILER % 100, __INTEL_COMPILER_UPDATE)
#elif defined(__INTEL_COMPILER) && !defined(__ICL)
#  define HEDLEY_INTEL_VERSION HEDLEY_VERSION_ENCODE(__INTEL_COMPILER / 100, __INTEL_COMPILER % 100, 0)
#endif

#if defined(HEDLEY_INTEL_VERSION_CHECK)
#  undef HEDLEY_INTEL_VERSION_CHECK
#endif
#if defined(HEDLEY_INTEL_VERSION)
#  define HEDLEY_INTEL_VERSION_CHECK(major,minor,patch) (HEDLEY_INTEL_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#  define HEDLEY_INTEL_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(HEDLEY_INTEL_CL_VERSION)
#  undef HEDLEY_INTEL_CL_VERSION
#endif
#if defined(__INTEL_COMPILER) && defined(__INTEL_COMPILER_UPDATE) && defined(__ICL)
#  define HEDLEY_INTEL_CL_VERSION HEDLEY_VERSION_ENCODE(__INTEL_COMPILER, __INTEL_COMPILER_UPDATE, 0)
#endif

#if defined(HEDLEY_INTEL_CL_VERSION_CHECK)
#  undef HEDLEY_INTEL_CL_VERSION_CHECK
#endif
#if defined(HEDLEY_INTEL_CL_VERSION)
#  define HEDLEY_INTEL_CL_VERSION_CHECK(major,minor,patch) (HEDLEY_INTEL_CL_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#  define HEDLEY_INTEL_CL_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(HEDLEY_PGI_VERSION)
#  undef HEDLEY_PGI_VERSION
#endif
#if defined(__PGI) && defined(__PGIC__) && defined(__PGIC_MINOR__) && defined(__PGIC_PATCHLEVEL__)
#  define HEDLEY_PGI_VERSION HEDLEY_VERSION_ENCODE(__PGIC__, __PGIC_MINOR__, __PGIC_PATCHLEVEL__)
#endif

#if defined(HEDLEY_PGI_VERSION_CHECK)
#  undef HEDLEY_PGI_VERSION_CHECK
#endif
#if defined(HEDLEY_PGI_VERSION)
#  define HEDLEY_PGI_VERSION_CHECK(major,minor,patch) (HEDLEY_PGI_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#  define HEDLEY_PGI_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(HEDLEY_SUNPRO_VERSION)
#  undef HEDLEY_SUNPRO_VERSION
#endif
#if defined(__SUNPRO_C) && (__SUNPRO_C > 0x1000)
#  define HEDLEY_SUNPRO_VERSION HEDLEY_VERSION_ENCODE((((__SUNPRO_C >> 16) & 0xf) * 10) + ((__SUNPRO_C >> 12) & 0xf), (((__SUNPRO_C >> 8) & 0xf) * 10) + ((__SUNPRO_C >> 4) & 0xf), (__SUNPRO_C & 0xf) * 10)
#elif defined(__SUNPRO_C)
#  define HEDLEY_SUNPRO_VERSION HEDLEY_VERSION_ENCODE((__SUNPRO_C >> 8) & 0xf, (__SUNPRO_C >> 4) & 0xf, (__SUNPRO_C) & 0xf)
#elif defined(__SUNPRO_CC) && (__SUNPRO_CC > 0x1000)
#  define HEDLEY_SUNPRO_VERSION HEDLEY_VERSION_ENCODE((((__SUNPRO_CC >> 16) & 0xf) * 10) + ((__SUNPRO_CC >> 12) & 0xf), (((__SUNPRO_CC >> 8) & 0xf) * 10) + ((__SUNPRO_CC >> 4) & 0xf), (__SUNPRO_CC & 0xf) * 10)
#elif defined(__SUNPRO_CC)
#  define HEDLEY_SUNPRO_VERSION HEDLEY_VERSION_ENCODE((__SUNPRO_CC >> 8) & 0xf, (__SUNPRO_CC >> 4) & 0xf, (__SUNPRO_CC) & 0xf)
#endif

#if defined(HEDLEY_SUNPRO_VERSION_CHECK)
#  undef HEDLEY_SUNPRO_VERSION_CHECK
#endif
#if defined(HEDLEY_SUNPRO_VERSION)
#  define HEDLEY_SUNPRO_VERSION_CHECK(major,minor,patch) (HEDLEY_SUNPRO_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#  define HEDLEY_SUNPRO_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(HEDLEY_EMSCRIPTEN_VERSION)
#  undef HEDLEY_EMSCRIPTEN_VERSION
#endif
#if defined(__EMSCRIPTEN__)
#  include <emscripten.h>
#  define HEDLEY_EMSCRIPTEN_VERSION HEDLEY_VERSION_ENCODE(__EMSCRIPTEN_major__, __EMSCRIPTEN_minor__, __EMSCRIPTEN_tiny__)
#endif

#if defined(HEDLEY_EMSCRIPTEN_VERSION_CHECK)
#  undef HEDLEY_EMSCRIPTEN_VERSION_CHECK
#endif
#if defined(HEDLEY_EMSCRIPTEN_VERSION)
#  define HEDLEY_EMSCRIPTEN_VERSION_CHECK(major,minor,patch) (HEDLEY_EMSCRIPTEN_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#  define HEDLEY_EMSCRIPTEN_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(HEDLEY_ARM_VERSION)
#  undef HEDLEY_ARM_VERSION
#endif
#if defined(__CC_ARM) && defined(__ARMCOMPILER_VERSION)
#  define HEDLEY_ARM_VERSION HEDLEY_VERSION_ENCODE(__ARMCOMPILER_VERSION / 1000000, (__ARMCOMPILER_VERSION % 1000000) / 10000, (__ARMCOMPILER_VERSION % 10000) / 100)
#elif defined(__CC_ARM) && defined(__ARMCC_VERSION)
#  define HEDLEY_ARM_VERSION HEDLEY_VERSION_ENCODE(__ARMCC_VERSION / 1000000, (__ARMCC_VERSION % 1000000) / 10000, (__ARMCC_VERSION % 10000) / 100)
#endif

#if defined(HEDLEY_ARM_VERSION_CHECK)
#  undef HEDLEY_ARM_VERSION_CHECK
#endif
#if defined(HEDLEY_ARM_VERSION)
#  define HEDLEY_ARM_VERSION_CHECK(major,minor,patch) (HEDLEY_ARM_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#  define HEDLEY_ARM_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(HEDLEY_IBM_VERSION)
#  undef HEDLEY_IBM_VERSION
#endif
#if defined(__ibmxl__)
#  define HEDLEY_IBM_VERSION HEDLEY_VERSION_ENCODE(__ibmxl_version__, __ibmxl_release__, __ibmxl_modification__)
#elif defined(__xlC__) && defined(__xlC_ver__)
#  define HEDLEY_IBM_VERSION HEDLEY_VERSION_ENCODE(__xlC__ >> 8, __xlC__ & 0xff, (__xlC_ver__ >> 8) & 0xff)
#elif defined(__xlC__)
#  define HEDLEY_IBM_VERSION HEDLEY_VERSION_ENCODE(__xlC__ >> 8, __xlC__ & 0xff, 0)
#endif

#if defined(HEDLEY_IBM_VERSION_CHECK)
#  undef HEDLEY_IBM_VERSION_CHECK
#endif
#if defined(HEDLEY_IBM_VERSION)
#  define HEDLEY_IBM_VERSION_CHECK(major,minor,patch) (HEDLEY_IBM_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#  define HEDLEY_IBM_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(HEDLEY_TI_VERSION)
#  undef HEDLEY_TI_VERSION
#endif
#if \
    defined(__TI_COMPILER_VERSION__) && \
    ( \
      defined(__TMS470__) || defined(__TI_ARM__) || \
      defined(__MSP430__) || \
      defined(__TMS320C2000__) \
    )
#  if (__TI_COMPILER_VERSION__ >= 16000000)
#    define HEDLEY_TI_VERSION HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000, (__TI_COMPILER_VERSION__ % 1000000) / 1000, (__TI_COMPILER_VERSION__ % 1000))
#  endif
#endif

#if defined(HEDLEY_TI_VERSION_CHECK)
#  undef HEDLEY_TI_VERSION_CHECK
#endif
#if defined(HEDLEY_TI_VERSION)
#  define HEDLEY_TI_VERSION_CHECK(major,minor,patch) (HEDLEY_TI_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#  define HEDLEY_TI_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(HEDLEY_TI_CL2000_VERSION)
#  undef HEDLEY_TI_CL2000_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && defined(__TMS320C2000__)
#  define HEDLEY_TI_CL2000_VERSION HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000, (__TI_COMPILER_VERSION__ % 1000000) / 1000, (__TI_COMPILER_VERSION__ % 1000))
#endif

#if defined(HEDLEY_TI_CL2000_VERSION_CHECK)
#  undef HEDLEY_TI_CL2000_VERSION_CHECK
#endif
#if defined(HEDLEY_TI_CL2000_VERSION)
#  define HEDLEY_TI_CL2000_VERSION_CHECK(major,minor,patch) (HEDLEY_TI_CL2000_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#  define HEDLEY_TI_CL2000_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(HEDLEY_TI_CL430_VERSION)
#  undef HEDLEY_TI_CL430_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && defined(__MSP430__)
#  define HEDLEY_TI_CL430_VERSION HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000, (__TI_COMPILER_VERSION__ % 1000000) / 1000, (__TI_COMPILER_VERSION__ % 1000))
#endif

#if defined(HEDLEY_TI_CL430_VERSION_CHECK)
#  undef HEDLEY_TI_CL430_VERSION_CHECK
#endif
#if defined(HEDLEY_TI_CL430_VERSION)
#  define HEDLEY_TI_CL430_VERSION_CHECK(major,minor,patch) (HEDLEY_TI_CL430_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#  define HEDLEY_TI_CL430_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(HEDLEY_TI_ARMCL_VERSION)
#  undef HEDLEY_TI_ARMCL_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && (defined(__TMS470__) || defined(__TI_ARM__))
#  define HEDLEY_TI_ARMCL_VERSION HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000, (__TI_COMPILER_VERSION__ % 1000000) / 1000, (__TI_COMPILER_VERSION__ % 1000))
#endif

#if defined(HEDLEY_TI_ARMCL_VERSION_CHECK)
#  undef HEDLEY_TI_ARMCL_VERSION_CHECK
#endif
#if defined(HEDLEY_TI_ARMCL_VERSION)
#  define HEDLEY_TI_ARMCL_VERSION_CHECK(major,minor,patch) (HEDLEY_TI_ARMCL_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#  define HEDLEY_TI_ARMCL_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(HEDLEY_TI_CL6X_VERSION)
#  undef HEDLEY_TI_CL6X_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && defined(__TMS320C6X__)
#  define HEDLEY_TI_CL6X_VERSION HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000, (__TI_COMPILER_VERSION__ % 1000000) / 1000, (__TI_COMPILER_VERSION__ % 1000))
#endif

#if defined(HEDLEY_TI_CL6X_VERSION_CHECK)
#  undef HEDLEY_TI_CL6X_VERSION_CHECK
#endif
#if defined(HEDLEY_TI_CL6X_VERSION)
#  define HEDLEY_TI_CL6X_VERSION_CHECK(major,minor,patch) (HEDLEY_TI_CL6X_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#  define HEDLEY_TI_CL6X_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(HEDLEY_TI_CL7X_VERSION)
#  undef HEDLEY_TI_CL7X_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && defined(__C7000__)
#  define HEDLEY_TI_CL7X_VERSION HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000, (__TI_COMPILER_VERSION__ % 1000000) / 1000, (__TI_COMPILER_VERSION__ % 1000))
#endif

#if defined(HEDLEY_TI_CL7X_VERSION_CHECK)
#  undef HEDLEY_TI_CL7X_VERSION_CHECK
#endif
#if defined(HEDLEY_TI_CL7X_VERSION)
#  define HEDLEY_TI_CL7X_VERSION_CHECK(major,minor,patch) (HEDLEY_TI_CL7X_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#  define HEDLEY_TI_CL7X_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(HEDLEY_TI_CLPRU_VERSION)
#  undef HEDLEY_TI_CLPRU_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && defined(__PRU__)
#  define HEDLEY_TI_CLPRU_VERSION HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000, (__TI_COMPILER_VERSION__ % 1000000) / 1000, (__TI_COMPILER_VERSION__ % 1000))
#endif

#if defined(HEDLEY_TI_CLPRU_VERSION_CHECK)
#  undef HEDLEY_TI_CLPRU_VERSION_CHECK
#endif
#if defined(HEDLEY_TI_CLPRU_VERSION)
#  define HEDLEY_TI_CLPRU_VERSION_CHECK(major,minor,patch) (HEDLEY_TI_CLPRU_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#  define HEDLEY_TI_CLPRU_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(HEDLEY_CRAY_VERSION)
#  undef HEDLEY_CRAY_VERSION
#endif
#if defined(_CRAYC)
#  if defined(_RELEASE_PATCHLEVEL)
#    define HEDLEY_CRAY_VERSION HEDLEY_VERSION_ENCODE(_RELEASE_MAJOR, _RELEASE_MINOR, _RELEASE_PATCHLEVEL)
#  else
#    define HEDLEY_CRAY_VERSION HEDLEY_VERSION_ENCODE(_RELEASE_MAJOR, _RELEASE_MINOR, 0)
#  endif
#endif

#if defined(HEDLEY_CRAY_VERSION_CHECK)
#  undef HEDLEY_CRAY_VERSION_CHECK
#endif
#if defined(HEDLEY_CRAY_VERSION)
#  define HEDLEY_CRAY_VERSION_CHECK(major,minor,patch) (HEDLEY_CRAY_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#  define HEDLEY_CRAY_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(HEDLEY_IAR_VERSION)
#  undef HEDLEY_IAR_VERSION
#endif
#if defined(__IAR_SYSTEMS_ICC__)
#  if __VER__ > 1000
#    define HEDLEY_IAR_VERSION HEDLEY_VERSION_ENCODE((__VER__ / 1000000), ((__VER__ / 1000) % 1000), (__VER__ % 1000))
#  else
#    define HEDLEY_IAR_VERSION HEDLEY_VERSION_ENCODE(__VER__ / 100, __VER__ % 100, 0)
#  endif
#endif

#if defined(HEDLEY_IAR_VERSION_CHECK)
#  undef HEDLEY_IAR_VERSION_CHECK
#endif
#if defined(HEDLEY_IAR_VERSION)
#  define HEDLEY_IAR_VERSION_CHECK(major,minor,patch) (HEDLEY_IAR_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#  define HEDLEY_IAR_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(HEDLEY_TINYC_VERSION)
#  undef HEDLEY_TINYC_VERSION
#endif
#if defined(__TINYC__)
#  define HEDLEY_TINYC_VERSION HEDLEY_VERSION_ENCODE(__TINYC__ / 1000, (__TINYC__ / 100) % 10, __TINYC__ % 100)
#endif

#if defined(HEDLEY_TINYC_VERSION_CHECK)
#  undef HEDLEY_TINYC_VERSION_CHECK
#endif
#if defined(HEDLEY_TINYC_VERSION)
#  define HEDLEY_TINYC_VERSION_CHECK(major,minor,patch) (HEDLEY_TINYC_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#  define HEDLEY_TINYC_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(HEDLEY_DMC_VERSION)
#  undef HEDLEY_DMC_VERSION
#endif
#if defined(__DMC__)
#  define HEDLEY_DMC_VERSION HEDLEY_VERSION_ENCODE(__DMC__ >> 8, (__DMC__ >> 4) & 0xf, __DMC__ & 0xf)
#endif

#if defined(HEDLEY_DMC_VERSION_CHECK)
#  undef HEDLEY_DMC_VERSION_CHECK
#endif
#if defined(HEDLEY_DMC_VERSION)
#  define HEDLEY_DMC_VERSION_CHECK(major,minor,patch) (HEDLEY_DMC_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#  define HEDLEY_DMC_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(HEDLEY_COMPCERT_VERSION)
#  undef HEDLEY_COMPCERT_VERSION
#endif
#if defined(__COMPCERT_VERSION__)
#  define HEDLEY_COMPCERT_VERSION HEDLEY_VERSION_ENCODE(__COMPCERT_VERSION__ / 10000, (__COMPCERT_VERSION__ / 100) % 100, __COMPCERT_VERSION__ % 100)
#endif

#if defined(HEDLEY_COMPCERT_VERSION_CHECK)
#  undef HEDLEY_COMPCERT_VERSION_CHECK
#endif
#if defined(HEDLEY_COMPCERT_VERSION)
#  define HEDLEY_COMPCERT_VERSION_CHECK(major,minor,patch) (HEDLEY_COMPCERT_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#  define HEDLEY_COMPCERT_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(HEDLEY_PELLES_VERSION)
#  undef HEDLEY_PELLES_VERSION
#endif
#if defined(__POCC__)
#  define HEDLEY_PELLES_VERSION HEDLEY_VERSION_ENCODE(__POCC__ / 100, __POCC__ % 100, 0)
#endif

#if defined(HEDLEY_PELLES_VERSION_CHECK)
#  undef HEDLEY_PELLES_VERSION_CHECK
#endif
#if defined(HEDLEY_PELLES_VERSION)
#  define HEDLEY_PELLES_VERSION_CHECK(major,minor,patch) (HEDLEY_PELLES_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#  define HEDLEY_PELLES_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(HEDLEY_MCST_LCC_VERSION)
#  undef HEDLEY_MCST_LCC_VERSION
#endif
#if defined(__LCC__) && defined(__LCC_MINOR__)
#  define HEDLEY_MCST_LCC_VERSION HEDLEY_VERSION_ENCODE(__LCC__ / 100, __LCC__ % 100, __LCC_MINOR__)
#endif

#if defined(HEDLEY_MCST_LCC_VERSION_CHECK)
#  undef HEDLEY_MCST_LCC_VERSION_CHECK
#endif
#if defined(HEDLEY_MCST_LCC_VERSION)
#  define HEDLEY_MCST_LCC_VERSION_CHECK(major,minor,patch) (HEDLEY_MCST_LCC_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#  define HEDLEY_MCST_LCC_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(HEDLEY_GCC_VERSION)
#  undef HEDLEY_GCC_VERSION
#endif
#if \
  defined(HEDLEY_GNUC_VERSION) && \
  !defined(__clang__) && \
  !defined(HEDLEY_INTEL_VERSION) && \
  !defined(HEDLEY_PGI_VERSION) && \
  !defined(HEDLEY_ARM_VERSION) && \
  !defined(HEDLEY_CRAY_VERSION) && \
  !defined(HEDLEY_TI_VERSION) && \
  !defined(HEDLEY_TI_ARMCL_VERSION) && \
  !defined(HEDLEY_TI_CL430_VERSION) && \
  !defined(HEDLEY_TI_CL2000_VERSION) && \
  !defined(HEDLEY_TI_CL6X_VERSION) && \
  !defined(HEDLEY_TI_CL7X_VERSION) && \
  !defined(HEDLEY_TI_CLPRU_VERSION) && \
  !defined(__COMPCERT__) && \
  !defined(HEDLEY_MCST_LCC_VERSION)
#  define HEDLEY_GCC_VERSION HEDLEY_GNUC_VERSION
#endif

#if defined(HEDLEY_GCC_VERSION_CHECK)
#  undef HEDLEY_GCC_VERSION_CHECK
#endif
#if defined(HEDLEY_GCC_VERSION)
#  define HEDLEY_GCC_VERSION_CHECK(major,minor,patch) (HEDLEY_GCC_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#  define HEDLEY_GCC_VERSION_CHECK(major,minor,patch) (0)
#endif

#if defined(HEDLEY_HAS_ATTRIBUTE)
#  undef HEDLEY_HAS_ATTRIBUTE
#endif
#if \
  defined(__has_attribute) && \
  ( \
    (!defined(HEDLEY_IAR_VERSION) || HEDLEY_IAR_VERSION_CHECK(8,5,9)) \
  )
#  define HEDLEY_HAS_ATTRIBUTE(attribute) __has_attribute(attribute)
#else
#  define HEDLEY_HAS_ATTRIBUTE(attribute) (0)
#endif

#if defined(HEDLEY_GNUC_HAS_ATTRIBUTE)
#  undef HEDLEY_GNUC_HAS_ATTRIBUTE
#endif
#if defined(__has_attribute)
#  define HEDLEY_GNUC_HAS_ATTRIBUTE(attribute,major,minor,patch) HEDLEY_HAS_ATTRIBUTE(attribute)
#else
#  define HEDLEY_GNUC_HAS_ATTRIBUTE(attribute,major,minor,patch) HEDLEY_GNUC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(HEDLEY_GCC_HAS_ATTRIBUTE)
#  undef HEDLEY_GCC_HAS_ATTRIBUTE
#endif
#if defined(__has_attribute)
#  define HEDLEY_GCC_HAS_ATTRIBUTE(attribute,major,minor,patch) HEDLEY_HAS_ATTRIBUTE(attribute)
#else
#  define HEDLEY_GCC_HAS_ATTRIBUTE(attribute,major,minor,patch) HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(HEDLEY_HAS_CPP_ATTRIBUTE)
#  undef HEDLEY_HAS_CPP_ATTRIBUTE
#endif
#if \
  defined(__has_cpp_attribute) && \
  defined(__cplusplus) && \
  (!defined(HEDLEY_SUNPRO_VERSION) || HEDLEY_SUNPRO_VERSION_CHECK(5,15,0))
#  define HEDLEY_HAS_CPP_ATTRIBUTE(attribute) __has_cpp_attribute(attribute)
#else
#  define HEDLEY_HAS_CPP_ATTRIBUTE(attribute) (0)
#endif

#if defined(HEDLEY_HAS_CPP_ATTRIBUTE_NS)
#  undef HEDLEY_HAS_CPP_ATTRIBUTE_NS
#endif
#if !defined(__cplusplus) || !defined(__has_cpp_attribute)
#  define HEDLEY_HAS_CPP_ATTRIBUTE_NS(ns,attribute) (0)
#elif \
  !defined(HEDLEY_PGI_VERSION) && \
  !defined(HEDLEY_IAR_VERSION) && \
  (!defined(HEDLEY_SUNPRO_VERSION) || HEDLEY_SUNPRO_VERSION_CHECK(5,15,0)) && \
  (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
#  define HEDLEY_HAS_CPP_ATTRIBUTE_NS(ns,attribute) HEDLEY_HAS_CPP_ATTRIBUTE(ns::attribute)
#else
#  define HEDLEY_HAS_CPP_ATTRIBUTE_NS(ns,attribute) (0)
#endif

#if defined(HEDLEY_GNUC_HAS_CPP_ATTRIBUTE)
#  undef HEDLEY_GNUC_HAS_CPP_ATTRIBUTE
#endif
#if defined(__has_cpp_attribute) && defined(__cplusplus)
#  define HEDLEY_GNUC_HAS_CPP_ATTRIBUTE(attribute,major,minor,patch) __has_cpp_attribute(attribute)
#else
#  define HEDLEY_GNUC_HAS_CPP_ATTRIBUTE(attribute,major,minor,patch) HEDLEY_GNUC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(HEDLEY_GCC_HAS_CPP_ATTRIBUTE)
#  undef HEDLEY_GCC_HAS_CPP_ATTRIBUTE
#endif
#if defined(__has_cpp_attribute) && defined(__cplusplus)
#  define HEDLEY_GCC_HAS_CPP_ATTRIBUTE(attribute,major,minor,patch) __has_cpp_attribute(attribute)
#else
#  define HEDLEY_GCC_HAS_CPP_ATTRIBUTE(attribute,major,minor,patch) HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(HEDLEY_HAS_BUILTIN)
#  undef HEDLEY_HAS_BUILTIN
#endif
#if defined(__has_builtin)
#  define HEDLEY_HAS_BUILTIN(builtin) __has_builtin(builtin)
#else
#  define HEDLEY_HAS_BUILTIN(builtin) (0)
#endif

#if defined(HEDLEY_GNUC_HAS_BUILTIN)
#  undef HEDLEY_GNUC_HAS_BUILTIN
#endif
#if defined(__has_builtin)
#  define HEDLEY_GNUC_HAS_BUILTIN(builtin,major,minor,patch) __has_builtin(builtin)
#else
#  define HEDLEY_GNUC_HAS_BUILTIN(builtin,major,minor,patch) HEDLEY_GNUC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(HEDLEY_GCC_HAS_BUILTIN)
#  undef HEDLEY_GCC_HAS_BUILTIN
#endif
#if defined(__has_builtin)
#  define HEDLEY_GCC_HAS_BUILTIN(builtin,major,minor,patch) __has_builtin(builtin)
#else
#  define HEDLEY_GCC_HAS_BUILTIN(builtin,major,minor,patch) HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(HEDLEY_HAS_FEATURE)
#  undef HEDLEY_HAS_FEATURE
#endif
#if defined(__has_feature)
#  define HEDLEY_HAS_FEATURE(feature) __has_feature(feature)
#else
#  define HEDLEY_HAS_FEATURE(feature) (0)
#endif

#if defined(HEDLEY_GNUC_HAS_FEATURE)
#  undef HEDLEY_GNUC_HAS_FEATURE
#endif
#if defined(__has_feature)
#  define HEDLEY_GNUC_HAS_FEATURE(feature,major,minor,patch) __has_feature(feature)
#else
#  define HEDLEY_GNUC_HAS_FEATURE(feature,major,minor,patch) HEDLEY_GNUC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(HEDLEY_GCC_HAS_FEATURE)
#  undef HEDLEY_GCC_HAS_FEATURE
#endif
#if defined(__has_feature)
#  define HEDLEY_GCC_HAS_FEATURE(feature,major,minor,patch) __has_feature(feature)
#else
#  define HEDLEY_GCC_HAS_FEATURE(feature,major,minor,patch) HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(HEDLEY_HAS_EXTENSION)
#  undef HEDLEY_HAS_EXTENSION
#endif
#if defined(__has_extension)
#  define HEDLEY_HAS_EXTENSION(extension) __has_extension(extension)
#else
#  define HEDLEY_HAS_EXTENSION(extension) (0)
#endif

#if defined(HEDLEY_GNUC_HAS_EXTENSION)
#  undef HEDLEY_GNUC_HAS_EXTENSION
#endif
#if defined(__has_extension)
#  define HEDLEY_GNUC_HAS_EXTENSION(extension,major,minor,patch) __has_extension(extension)
#else
#  define HEDLEY_GNUC_HAS_EXTENSION(extension,major,minor,patch) HEDLEY_GNUC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(HEDLEY_GCC_HAS_EXTENSION)
#  undef HEDLEY_GCC_HAS_EXTENSION
#endif
#if defined(__has_extension)
#  define HEDLEY_GCC_HAS_EXTENSION(extension,major,minor,patch) __has_extension(extension)
#else
#  define HEDLEY_GCC_HAS_EXTENSION(extension,major,minor,patch) HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(HEDLEY_HAS_DECLSPEC_ATTRIBUTE)
#  undef HEDLEY_HAS_DECLSPEC_ATTRIBUTE
#endif
#if defined(__has_declspec_attribute)
#  define HEDLEY_HAS_DECLSPEC_ATTRIBUTE(attribute) __has_declspec_attribute(attribute)
#else
#  define HEDLEY_HAS_DECLSPEC_ATTRIBUTE(attribute) (0)
#endif

#if defined(HEDLEY_GNUC_HAS_DECLSPEC_ATTRIBUTE)
#  undef HEDLEY_GNUC_HAS_DECLSPEC_ATTRIBUTE
#endif
#if defined(__has_declspec_attribute)
#  define HEDLEY_GNUC_HAS_DECLSPEC_ATTRIBUTE(attribute,major,minor,patch) __has_declspec_attribute(attribute)
#else
#  define HEDLEY_GNUC_HAS_DECLSPEC_ATTRIBUTE(attribute,major,minor,patch) HEDLEY_GNUC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(HEDLEY_GCC_HAS_DECLSPEC_ATTRIBUTE)
#  undef HEDLEY_GCC_HAS_DECLSPEC_ATTRIBUTE
#endif
#if defined(__has_declspec_attribute)
#  define HEDLEY_GCC_HAS_DECLSPEC_ATTRIBUTE(attribute,major,minor,patch) __has_declspec_attribute(attribute)
#else
#  define HEDLEY_GCC_HAS_DECLSPEC_ATTRIBUTE(attribute,major,minor,patch) HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(HEDLEY_HAS_WARNING)
#  undef HEDLEY_HAS_WARNING
#endif
#if defined(__has_warning)
#  define HEDLEY_HAS_WARNING(warning) __has_warning(warning)
#else
#  define HEDLEY_HAS_WARNING(warning) (0)
#endif

#if defined(HEDLEY_GNUC_HAS_WARNING)
#  undef HEDLEY_GNUC_HAS_WARNING
#endif
#if defined(__has_warning)
#  define HEDLEY_GNUC_HAS_WARNING(warning,major,minor,patch) __has_warning(warning)
#else
#  define HEDLEY_GNUC_HAS_WARNING(warning,major,minor,patch) HEDLEY_GNUC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(HEDLEY_GCC_HAS_WARNING)
#  undef HEDLEY_GCC_HAS_WARNING
#endif
#if defined(__has_warning)
#  define HEDLEY_GCC_HAS_WARNING(warning,major,minor,patch) __has_warning(warning)
#else
#  define HEDLEY_GCC_HAS_WARNING(warning,major,minor,patch) HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif

#if \
  (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)) || \
  defined(__clang__) || \
  HEDLEY_GCC_VERSION_CHECK(3,0,0) || \
  HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  HEDLEY_IAR_VERSION_CHECK(8,0,0) || \
  HEDLEY_PGI_VERSION_CHECK(18,4,0) || \
  HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
  HEDLEY_TI_VERSION_CHECK(15,12,0) || \
  HEDLEY_TI_ARMCL_VERSION_CHECK(4,7,0) || \
  HEDLEY_TI_CL430_VERSION_CHECK(2,0,1) || \
  HEDLEY_TI_CL2000_VERSION_CHECK(6,1,0) || \
  HEDLEY_TI_CL6X_VERSION_CHECK(7,0,0) || \
  HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
  HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
  HEDLEY_CRAY_VERSION_CHECK(5,0,0) || \
  HEDLEY_TINYC_VERSION_CHECK(0,9,17) || \
  HEDLEY_SUNPRO_VERSION_CHECK(8,0,0) || \
  (HEDLEY_IBM_VERSION_CHECK(10,1,0) && defined(__C99_PRAGMA_OPERATOR))
#  define HEDLEY_PRAGMA(value) _Pragma(#value)
#elif HEDLEY_MSVC_VERSION_CHECK(15,0,0)
#  define HEDLEY_PRAGMA(value) __pragma(value)
#else
#  define HEDLEY_PRAGMA(value)
#endif

#if defined(HEDLEY_DIAGNOSTIC_PUSH)
#  undef HEDLEY_DIAGNOSTIC_PUSH
#endif
#if defined(HEDLEY_DIAGNOSTIC_POP)
#  undef HEDLEY_DIAGNOSTIC_POP
#endif
#if defined(__clang__)
#  define HEDLEY_DIAGNOSTIC_PUSH _Pragma("clang diagnostic push")
#  define HEDLEY_DIAGNOSTIC_POP _Pragma("clang diagnostic pop")
#elif HEDLEY_INTEL_VERSION_CHECK(13,0,0)
#  define HEDLEY_DIAGNOSTIC_PUSH _Pragma("warning(push)")
#  define HEDLEY_DIAGNOSTIC_POP _Pragma("warning(pop)")
#elif HEDLEY_GCC_VERSION_CHECK(4,6,0)
#  define HEDLEY_DIAGNOSTIC_PUSH _Pragma("GCC diagnostic push")
#  define HEDLEY_DIAGNOSTIC_POP _Pragma("GCC diagnostic pop")
#elif \
  HEDLEY_MSVC_VERSION_CHECK(15,0,0) || \
  HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#  define HEDLEY_DIAGNOSTIC_PUSH __pragma(warning(push))
#  define HEDLEY_DIAGNOSTIC_POP __pragma(warning(pop))
#elif HEDLEY_ARM_VERSION_CHECK(5,6,0)
#  define HEDLEY_DIAGNOSTIC_PUSH _Pragma("push")
#  define HEDLEY_DIAGNOSTIC_POP _Pragma("pop")
#elif \
    HEDLEY_TI_VERSION_CHECK(15,12,0) || \
    HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
    HEDLEY_TI_CL430_VERSION_CHECK(4,4,0) || \
    HEDLEY_TI_CL6X_VERSION_CHECK(8,1,0) || \
    HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0)
#  define HEDLEY_DIAGNOSTIC_PUSH _Pragma("diag_push")
#  define HEDLEY_DIAGNOSTIC_POP _Pragma("diag_pop")
#elif HEDLEY_PELLES_VERSION_CHECK(2,90,0)
#  define HEDLEY_DIAGNOSTIC_PUSH _Pragma("warning(push)")
#  define HEDLEY_DIAGNOSTIC_POP _Pragma("warning(pop)")
#else
#  define HEDLEY_DIAGNOSTIC_PUSH
#  define HEDLEY_DIAGNOSTIC_POP
#endif

/* HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_ is for
   HEDLEY INTERNAL USE ONLY.  API subject to change without notice. */
#if defined(HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_)
#  undef HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_
#endif
#if defined(__cplusplus)
#  if HEDLEY_HAS_WARNING("-Wc++98-compat")
#    if HEDLEY_HAS_WARNING("-Wc++17-extensions")
#      if HEDLEY_HAS_WARNING("-Wc++1z-extensions")
#        define HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(xpr) \
           HEDLEY_DIAGNOSTIC_PUSH \
           _Pragma("clang diagnostic ignored \"-Wc++98-compat\"") \
           _Pragma("clang diagnostic ignored \"-Wc++17-extensions\"") \
           _Pragma("clang diagnostic ignored \"-Wc++1z-extensions\"") \
           xpr \
           HEDLEY_DIAGNOSTIC_POP
#      else
#        define HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(xpr) \
           HEDLEY_DIAGNOSTIC_PUSH \
           _Pragma("clang diagnostic ignored \"-Wc++98-compat\"") \
           _Pragma("clang diagnostic ignored \"-Wc++17-extensions\"") \
           xpr \
           HEDLEY_DIAGNOSTIC_POP
#      endif
#    else
#      define HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(xpr) \
         HEDLEY_DIAGNOSTIC_PUSH \
         _Pragma("clang diagnostic ignored \"-Wc++98-compat\"") \
         xpr \
         HEDLEY_DIAGNOSTIC_POP
#    endif
#  endif
#endif
#if !defined(HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_)
#  define HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(x) x
#endif

#if defined(HEDLEY_CONST_CAST)
#  undef HEDLEY_CONST_CAST
#endif
#if defined(__cplusplus)
#  define HEDLEY_CONST_CAST(T, expr) (const_cast<T>(expr))
#elif \
  HEDLEY_HAS_WARNING("-Wcast-qual") || \
  HEDLEY_GCC_VERSION_CHECK(4,6,0) || \
  HEDLEY_INTEL_VERSION_CHECK(13,0,0)
#  define HEDLEY_CONST_CAST(T, expr) (__extension__ ({ \
      HEDLEY_DIAGNOSTIC_PUSH \
      HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL \
      ((T) (expr)); \
      HEDLEY_DIAGNOSTIC_POP \
    }))
#else
#  define HEDLEY_CONST_CAST(T, expr) ((T) (expr))
#endif

#if defined(HEDLEY_REINTERPRET_CAST)
#  undef HEDLEY_REINTERPRET_CAST
#endif
#if defined(__cplusplus)
#  define HEDLEY_REINTERPRET_CAST(T, expr) (reinterpret_cast<T>(expr))
#else
#  define HEDLEY_REINTERPRET_CAST(T, expr) ((T) (expr))
#endif

#if defined(HEDLEY_STATIC_CAST)
#  undef HEDLEY_STATIC_CAST
#endif
#if defined(__cplusplus)
#  define HEDLEY_STATIC_CAST(T, expr) (static_cast<T>(expr))
#else
#  define HEDLEY_STATIC_CAST(T, expr) ((T) (expr))
#endif

#if defined(HEDLEY_CPP_CAST)
#  undef HEDLEY_CPP_CAST
#endif
#if defined(__cplusplus)
#  if HEDLEY_HAS_WARNING("-Wold-style-cast")
#    define HEDLEY_CPP_CAST(T, expr) \
       HEDLEY_DIAGNOSTIC_PUSH \
       _Pragma("clang diagnostic ignored \"-Wold-style-cast\"") \
       ((T) (expr)) \
       HEDLEY_DIAGNOSTIC_POP
#  elif HEDLEY_IAR_VERSION_CHECK(8,3,0)
#    define HEDLEY_CPP_CAST(T, expr) \
       HEDLEY_DIAGNOSTIC_PUSH \
       _Pragma("diag_suppress=Pe137") \
       HEDLEY_DIAGNOSTIC_POP
#  else
#    define HEDLEY_CPP_CAST(T, expr) ((T) (expr))
#  endif
#else
#  define HEDLEY_CPP_CAST(T, expr) (expr)
#endif

#if defined(HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED)
#  undef HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED
#endif
#if HEDLEY_HAS_WARNING("-Wdeprecated-declarations")
#  define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")
#elif HEDLEY_INTEL_VERSION_CHECK(13,0,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("warning(disable:1478 1786)")
#elif HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED __pragma(warning(disable:1478 1786))
#elif HEDLEY_PGI_VERSION_CHECK(20,7,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("diag_suppress 1215,1216,1444,1445")
#elif HEDLEY_PGI_VERSION_CHECK(17,10,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("diag_suppress 1215,1444")
#elif HEDLEY_GCC_VERSION_CHECK(4,3,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#elif HEDLEY_MSVC_VERSION_CHECK(15,0,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED __pragma(warning(disable:4996))
#elif HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("diag_suppress 1215,1444")
#elif \
    HEDLEY_TI_VERSION_CHECK(15,12,0) || \
    (HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
    (HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
    (HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
    (HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
    HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
    HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("diag_suppress 1291,1718")
#elif HEDLEY_SUNPRO_VERSION_CHECK(5,13,0) && !defined(__cplusplus)
#  define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("error_messages(off,E_DEPRECATED_ATT,E_DEPRECATED_ATT_MESS)")
#elif HEDLEY_SUNPRO_VERSION_CHECK(5,13,0) && defined(__cplusplus)
#  define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("error_messages(off,symdeprecated,symdeprecated2)")
#elif HEDLEY_IAR_VERSION_CHECK(8,0,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("diag_suppress=Pe1444,Pe1215")
#elif HEDLEY_PELLES_VERSION_CHECK(2,90,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("warn(disable:2241)")
#else
#  define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED
#endif

#if defined(HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS)
#  undef HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS
#endif
#if HEDLEY_HAS_WARNING("-Wunknown-pragmas")
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("clang diagnostic ignored \"-Wunknown-pragmas\"")
#elif HEDLEY_INTEL_VERSION_CHECK(13,0,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("warning(disable:161)")
#elif HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS __pragma(warning(disable:161))
#elif HEDLEY_PGI_VERSION_CHECK(17,10,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("diag_suppress 1675")
#elif HEDLEY_GCC_VERSION_CHECK(4,3,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("GCC diagnostic ignored \"-Wunknown-pragmas\"")
#elif HEDLEY_MSVC_VERSION_CHECK(15,0,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS __pragma(warning(disable:4068))
#elif \
    HEDLEY_TI_VERSION_CHECK(16,9,0) || \
    HEDLEY_TI_CL6X_VERSION_CHECK(8,0,0) || \
    HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    HEDLEY_TI_CLPRU_VERSION_CHECK(2,3,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("diag_suppress 163")
#elif HEDLEY_TI_CL6X_VERSION_CHECK(8,0,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("diag_suppress 163")
#elif HEDLEY_IAR_VERSION_CHECK(8,0,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("diag_suppress=Pe161")
#elif HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("diag_suppress 161")
#else
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS
#endif

#if defined(HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES)
#  undef HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES
#endif
#if HEDLEY_HAS_WARNING("-Wunknown-attributes")
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("clang diagnostic ignored \"-Wunknown-attributes\"")
#elif HEDLEY_GCC_VERSION_CHECK(4,6,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#elif HEDLEY_INTEL_VERSION_CHECK(17,0,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("warning(disable:1292)")
#elif HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES __pragma(warning(disable:1292))
#elif HEDLEY_MSVC_VERSION_CHECK(19,0,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES __pragma(warning(disable:5030))
#elif HEDLEY_PGI_VERSION_CHECK(20,7,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("diag_suppress 1097,1098")
#elif HEDLEY_PGI_VERSION_CHECK(17,10,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("diag_suppress 1097")
#elif HEDLEY_SUNPRO_VERSION_CHECK(5,14,0) && defined(__cplusplus)
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("error_messages(off,attrskipunsup)")
#elif \
    HEDLEY_TI_VERSION_CHECK(18,1,0) || \
    HEDLEY_TI_CL6X_VERSION_CHECK(8,3,0) || \
    HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("diag_suppress 1173")
#elif HEDLEY_IAR_VERSION_CHECK(8,0,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("diag_suppress=Pe1097")
#elif HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES _Pragma("diag_suppress 1097")
#else
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES
#endif

#if defined(HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL)
#  undef HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL
#endif
#if HEDLEY_HAS_WARNING("-Wcast-qual")
#  define HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL _Pragma("clang diagnostic ignored \"-Wcast-qual\"")
#elif HEDLEY_INTEL_VERSION_CHECK(13,0,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL _Pragma("warning(disable:2203 2331)")
#elif HEDLEY_GCC_VERSION_CHECK(3,0,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL _Pragma("GCC diagnostic ignored \"-Wcast-qual\"")
#else
#  define HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL
#endif

#if defined(HEDLEY_DIAGNOSTIC_DISABLE_UNUSED_FUNCTION)
#  undef HEDLEY_DIAGNOSTIC_DISABLE_UNUSED_FUNCTION
#endif
#if HEDLEY_HAS_WARNING("-Wunused-function")
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNUSED_FUNCTION _Pragma("clang diagnostic ignored \"-Wunused-function\"")
#elif HEDLEY_GCC_VERSION_CHECK(3,4,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNUSED_FUNCTION _Pragma("GCC diagnostic ignored \"-Wunused-function\"")
#elif HEDLEY_MSVC_VERSION_CHECK(1,0,0)
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNUSED_FUNCTION __pragma(warning(disable:4505))
#elif HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNUSED_FUNCTION _Pragma("diag_suppress 3142")
#else
#  define HEDLEY_DIAGNOSTIC_DISABLE_UNUSED_FUNCTION
#endif

#if defined(HEDLEY_DEPRECATED)
#  undef HEDLEY_DEPRECATED
#endif
#if defined(HEDLEY_DEPRECATED_FOR)
#  undef HEDLEY_DEPRECATED_FOR
#endif
#if \
  HEDLEY_MSVC_VERSION_CHECK(14,0,0) || \
  HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#  define HEDLEY_DEPRECATED(since) __declspec(deprecated("Since " # since))
#  define HEDLEY_DEPRECATED_FOR(since, replacement) __declspec(deprecated("Since " #since "; use " #replacement))
#elif \
  (HEDLEY_HAS_EXTENSION(attribute_deprecated_with_message) && !defined(HEDLEY_IAR_VERSION)) || \
  HEDLEY_GCC_VERSION_CHECK(4,5,0) || \
  HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  HEDLEY_ARM_VERSION_CHECK(5,6,0) || \
  HEDLEY_SUNPRO_VERSION_CHECK(5,13,0) || \
  HEDLEY_PGI_VERSION_CHECK(17,10,0) || \
  HEDLEY_TI_VERSION_CHECK(18,1,0) || \
  HEDLEY_TI_ARMCL_VERSION_CHECK(18,1,0) || \
  HEDLEY_TI_CL6X_VERSION_CHECK(8,3,0) || \
  HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
  HEDLEY_TI_CLPRU_VERSION_CHECK(2,3,0) || \
  HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define HEDLEY_DEPRECATED(since) __attribute__((__deprecated__("Since " #since)))
#  define HEDLEY_DEPRECATED_FOR(since, replacement) __attribute__((__deprecated__("Since " #since "; use " #replacement)))
#elif defined(__cplusplus) && (__cplusplus >= 201402L)
#  define HEDLEY_DEPRECATED(since) HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[deprecated("Since " #since)]])
#  define HEDLEY_DEPRECATED_FOR(since, replacement) HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[deprecated("Since " #since "; use " #replacement)]])
#elif \
  HEDLEY_HAS_ATTRIBUTE(deprecated) || \
  HEDLEY_GCC_VERSION_CHECK(3,1,0) || \
  HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
  HEDLEY_TI_VERSION_CHECK(15,12,0) || \
  (HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
  (HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
  (HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
  (HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
  HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
  HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
  HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10) || \
  HEDLEY_IAR_VERSION_CHECK(8,10,0)
#  define HEDLEY_DEPRECATED(since) __attribute__((__deprecated__))
#  define HEDLEY_DEPRECATED_FOR(since, replacement) __attribute__((__deprecated__))
#elif \
  HEDLEY_MSVC_VERSION_CHECK(13,10,0) || \
  HEDLEY_PELLES_VERSION_CHECK(6,50,0) || \
  HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#  define HEDLEY_DEPRECATED(since) __declspec(deprecated)
#  define HEDLEY_DEPRECATED_FOR(since, replacement) __declspec(deprecated)
#elif HEDLEY_IAR_VERSION_CHECK(8,0,0)
#  define HEDLEY_DEPRECATED(since) _Pragma("deprecated")
#  define HEDLEY_DEPRECATED_FOR(since, replacement) _Pragma("deprecated")
#else
#  define HEDLEY_DEPRECATED(since)
#  define HEDLEY_DEPRECATED_FOR(since, replacement)
#endif

#if defined(HEDLEY_UNAVAILABLE)
#  undef HEDLEY_UNAVAILABLE
#endif
#if \
  HEDLEY_HAS_ATTRIBUTE(warning) || \
  HEDLEY_GCC_VERSION_CHECK(4,3,0) || \
  HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define HEDLEY_UNAVAILABLE(available_since) __attribute__((__warning__("Not available until " #available_since)))
#else
#  define HEDLEY_UNAVAILABLE(available_since)
#endif

#if defined(HEDLEY_WARN_UNUSED_RESULT)
#  undef HEDLEY_WARN_UNUSED_RESULT
#endif
#if defined(HEDLEY_WARN_UNUSED_RESULT_MSG)
#  undef HEDLEY_WARN_UNUSED_RESULT_MSG
#endif
#if \
  HEDLEY_HAS_ATTRIBUTE(warn_unused_result) || \
  HEDLEY_GCC_VERSION_CHECK(3,4,0) || \
  HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  HEDLEY_TI_VERSION_CHECK(15,12,0) || \
  (HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
  (HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
  (HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
  (HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
  HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
  HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
  (HEDLEY_SUNPRO_VERSION_CHECK(5,15,0) && defined(__cplusplus)) || \
  HEDLEY_PGI_VERSION_CHECK(17,10,0) || \
  HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define HEDLEY_WARN_UNUSED_RESULT __attribute__((__warn_unused_result__))
#  define HEDLEY_WARN_UNUSED_RESULT_MSG(msg) __attribute__((__warn_unused_result__))
#elif (HEDLEY_HAS_CPP_ATTRIBUTE(nodiscard) >= 201907L)
#  define HEDLEY_WARN_UNUSED_RESULT HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[nodiscard]])
#  define HEDLEY_WARN_UNUSED_RESULT_MSG(msg) HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[nodiscard(msg)]])
#elif HEDLEY_HAS_CPP_ATTRIBUTE(nodiscard)
#  define HEDLEY_WARN_UNUSED_RESULT HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[nodiscard]])
#  define HEDLEY_WARN_UNUSED_RESULT_MSG(msg) HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[nodiscard]])
#elif defined(_Check_return_) /* SAL */
#  define HEDLEY_WARN_UNUSED_RESULT _Check_return_
#  define HEDLEY_WARN_UNUSED_RESULT_MSG(msg) _Check_return_
#else
#  define HEDLEY_WARN_UNUSED_RESULT
#  define HEDLEY_WARN_UNUSED_RESULT_MSG(msg)
#endif

#if defined(HEDLEY_SENTINEL)
#  undef HEDLEY_SENTINEL
#endif
#if \
  HEDLEY_HAS_ATTRIBUTE(sentinel) || \
  HEDLEY_GCC_VERSION_CHECK(4,0,0) || \
  HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  HEDLEY_ARM_VERSION_CHECK(5,4,0) || \
  HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define HEDLEY_SENTINEL(position) __attribute__((__sentinel__(position)))
#else
#  define HEDLEY_SENTINEL(position)
#endif

#if defined(HEDLEY_NO_RETURN)
#  undef HEDLEY_NO_RETURN
#endif
#if HEDLEY_IAR_VERSION_CHECK(8,0,0)
#  define HEDLEY_NO_RETURN __noreturn
#elif \
  HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define HEDLEY_NO_RETURN __attribute__((__noreturn__))
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#  define HEDLEY_NO_RETURN _Noreturn
#elif defined(__cplusplus) && (__cplusplus >= 201103L)
#  define HEDLEY_NO_RETURN HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[noreturn]])
#elif \
  HEDLEY_HAS_ATTRIBUTE(noreturn) || \
  HEDLEY_GCC_VERSION_CHECK(3,2,0) || \
  HEDLEY_SUNPRO_VERSION_CHECK(5,11,0) || \
  HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
  HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
  HEDLEY_TI_VERSION_CHECK(15,12,0) || \
  (HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
  (HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
  (HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
  (HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
  HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
  HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
  HEDLEY_IAR_VERSION_CHECK(8,10,0)
#  define HEDLEY_NO_RETURN __attribute__((__noreturn__))
#elif HEDLEY_SUNPRO_VERSION_CHECK(5,10,0)
#  define HEDLEY_NO_RETURN _Pragma("does_not_return")
#elif \
  HEDLEY_MSVC_VERSION_CHECK(13,10,0) || \
  HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#  define HEDLEY_NO_RETURN __declspec(noreturn)
#elif HEDLEY_TI_CL6X_VERSION_CHECK(6,0,0) && defined(__cplusplus)
#  define HEDLEY_NO_RETURN _Pragma("FUNC_NEVER_RETURNS;")
#elif HEDLEY_COMPCERT_VERSION_CHECK(3,2,0)
#  define HEDLEY_NO_RETURN __attribute((noreturn))
#elif HEDLEY_PELLES_VERSION_CHECK(9,0,0)
#  define HEDLEY_NO_RETURN __declspec(noreturn)
#else
#  define HEDLEY_NO_RETURN
#endif

#if defined(HEDLEY_NO_ESCAPE)
#  undef HEDLEY_NO_ESCAPE
#endif
#if HEDLEY_HAS_ATTRIBUTE(noescape)
#  define HEDLEY_NO_ESCAPE __attribute__((__noescape__))
#else
#  define HEDLEY_NO_ESCAPE
#endif

#if defined(HEDLEY_UNREACHABLE)
#  undef HEDLEY_UNREACHABLE
#endif
#if defined(HEDLEY_UNREACHABLE_RETURN)
#  undef HEDLEY_UNREACHABLE_RETURN
#endif
#if defined(HEDLEY_ASSUME)
#  undef HEDLEY_ASSUME
#endif
#if \
  HEDLEY_MSVC_VERSION_CHECK(13,10,0) || \
  HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#  define HEDLEY_ASSUME(expr) __assume(expr)
#elif HEDLEY_HAS_BUILTIN(__builtin_assume)
#  define HEDLEY_ASSUME(expr) __builtin_assume(expr)
#elif \
    HEDLEY_TI_CL2000_VERSION_CHECK(6,2,0) || \
    HEDLEY_TI_CL6X_VERSION_CHECK(4,0,0)
#  if defined(__cplusplus)
#    define HEDLEY_ASSUME(expr) std::_nassert(expr)
#  else
#    define HEDLEY_ASSUME(expr) _nassert(expr)
#  endif
#endif
#if \
  (HEDLEY_HAS_BUILTIN(__builtin_unreachable) && (!defined(HEDLEY_ARM_VERSION))) || \
  HEDLEY_GCC_VERSION_CHECK(4,5,0) || \
  HEDLEY_PGI_VERSION_CHECK(18,10,0) || \
  HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  HEDLEY_IBM_VERSION_CHECK(13,1,5) || \
  HEDLEY_CRAY_VERSION_CHECK(10,0,0) || \
  HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define HEDLEY_UNREACHABLE() __builtin_unreachable()
#elif defined(HEDLEY_ASSUME)
#  define HEDLEY_UNREACHABLE() HEDLEY_ASSUME(0)
#endif
#if !defined(HEDLEY_ASSUME)
#  if defined(HEDLEY_UNREACHABLE)
#    define HEDLEY_ASSUME(expr) HEDLEY_STATIC_CAST(void, ((expr) ? 1 : (HEDLEY_UNREACHABLE(), 1)))
#  else
#    define HEDLEY_ASSUME(expr) HEDLEY_STATIC_CAST(void, expr)
#  endif
#endif
#if defined(HEDLEY_UNREACHABLE)
#  if  \
      HEDLEY_TI_CL2000_VERSION_CHECK(6,2,0) || \
      HEDLEY_TI_CL6X_VERSION_CHECK(4,0,0)
#    define HEDLEY_UNREACHABLE_RETURN(value) return (HEDLEY_STATIC_CAST(void, HEDLEY_ASSUME(0)), (value))
#  else
#    define HEDLEY_UNREACHABLE_RETURN(value) HEDLEY_UNREACHABLE()
#  endif
#else
#  define HEDLEY_UNREACHABLE_RETURN(value) return (value)
#endif
#if !defined(HEDLEY_UNREACHABLE)
#  define HEDLEY_UNREACHABLE() HEDLEY_ASSUME(0)
#endif

HEDLEY_DIAGNOSTIC_PUSH
#if HEDLEY_HAS_WARNING("-Wpedantic")
#  pragma clang diagnostic ignored "-Wpedantic"
#endif
#if HEDLEY_HAS_WARNING("-Wc++98-compat-pedantic") && defined(__cplusplus)
#  pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
#endif
#if HEDLEY_GCC_HAS_WARNING("-Wvariadic-macros",4,0,0)
#  if defined(__clang__)
#    pragma clang diagnostic ignored "-Wvariadic-macros"
#  elif defined(HEDLEY_GCC_VERSION)
#    pragma GCC diagnostic ignored "-Wvariadic-macros"
#  endif
#endif
#if defined(HEDLEY_NON_NULL)
#  undef HEDLEY_NON_NULL
#endif
#if \
  HEDLEY_HAS_ATTRIBUTE(nonnull) || \
  HEDLEY_GCC_VERSION_CHECK(3,3,0) || \
  HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  HEDLEY_ARM_VERSION_CHECK(4,1,0)
#  define HEDLEY_NON_NULL(...) __attribute__((__nonnull__(__VA_ARGS__)))
#else
#  define HEDLEY_NON_NULL(...)
#endif
HEDLEY_DIAGNOSTIC_POP

#if defined(HEDLEY_PRINTF_FORMAT)
#  undef HEDLEY_PRINTF_FORMAT
#endif
#if defined(__MINGW32__) && HEDLEY_GCC_HAS_ATTRIBUTE(format,4,4,0) && !defined(__USE_MINGW_ANSI_STDIO)
#  define HEDLEY_PRINTF_FORMAT(string_idx,first_to_check) __attribute__((__format__(ms_printf, string_idx, first_to_check)))
#elif defined(__MINGW32__) && HEDLEY_GCC_HAS_ATTRIBUTE(format,4,4,0) && defined(__USE_MINGW_ANSI_STDIO)
#  define HEDLEY_PRINTF_FORMAT(string_idx,first_to_check) __attribute__((__format__(gnu_printf, string_idx, first_to_check)))
#elif \
  HEDLEY_HAS_ATTRIBUTE(format) || \
  HEDLEY_GCC_VERSION_CHECK(3,1,0) || \
  HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  HEDLEY_ARM_VERSION_CHECK(5,6,0) || \
  HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
  HEDLEY_TI_VERSION_CHECK(15,12,0) || \
  (HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
  (HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
  (HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
  (HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
  HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
  HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
  HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define HEDLEY_PRINTF_FORMAT(string_idx,first_to_check) __attribute__((__format__(__printf__, string_idx, first_to_check)))
#elif HEDLEY_PELLES_VERSION_CHECK(6,0,0)
#  define HEDLEY_PRINTF_FORMAT(string_idx,first_to_check) __declspec(vaformat(printf,string_idx,first_to_check))
#else
#  define HEDLEY_PRINTF_FORMAT(string_idx,first_to_check)
#endif

#if defined(HEDLEY_CONSTEXPR)
#  undef HEDLEY_CONSTEXPR
#endif
#if defined(__cplusplus)
#  if __cplusplus >= 201103L
#    define HEDLEY_CONSTEXPR HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(constexpr)
#  endif
#endif
#if !defined(HEDLEY_CONSTEXPR)
#  define HEDLEY_CONSTEXPR
#endif

#if defined(HEDLEY_PREDICT)
#  undef HEDLEY_PREDICT
#endif
#if defined(HEDLEY_LIKELY)
#  undef HEDLEY_LIKELY
#endif
#if defined(HEDLEY_UNLIKELY)
#  undef HEDLEY_UNLIKELY
#endif
#if defined(HEDLEY_UNPREDICTABLE)
#  undef HEDLEY_UNPREDICTABLE
#endif
#if HEDLEY_HAS_BUILTIN(__builtin_unpredictable)
#  define HEDLEY_UNPREDICTABLE(expr) __builtin_unpredictable((expr))
#endif
#if \
  (HEDLEY_HAS_BUILTIN(__builtin_expect_with_probability) && !defined(HEDLEY_PGI_VERSION) && !defined(HEDLEY_INTEL_VERSION)) || \
  HEDLEY_GCC_VERSION_CHECK(9,0,0) || \
  HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define HEDLEY_PREDICT(expr, value, probability) __builtin_expect_with_probability(  (expr), (value), (probability))
#  define HEDLEY_PREDICT_TRUE(expr, probability)   __builtin_expect_with_probability(!!(expr),    1   , (probability))
#  define HEDLEY_PREDICT_FALSE(expr, probability)  __builtin_expect_with_probability(!!(expr),    0   , (probability))
#  define HEDLEY_LIKELY(expr)                      __builtin_expect                 (!!(expr),    1                  )
#  define HEDLEY_UNLIKELY(expr)                    __builtin_expect                 (!!(expr),    0                  )
#elif \
  (HEDLEY_HAS_BUILTIN(__builtin_expect) && !defined(HEDLEY_INTEL_CL_VERSION)) || \
  HEDLEY_GCC_VERSION_CHECK(3,0,0) || \
  HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  (HEDLEY_SUNPRO_VERSION_CHECK(5,15,0) && defined(__cplusplus)) || \
  HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
  HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
  HEDLEY_TI_VERSION_CHECK(15,12,0) || \
  HEDLEY_TI_ARMCL_VERSION_CHECK(4,7,0) || \
  HEDLEY_TI_CL430_VERSION_CHECK(3,1,0) || \
  HEDLEY_TI_CL2000_VERSION_CHECK(6,1,0) || \
  HEDLEY_TI_CL6X_VERSION_CHECK(6,1,0) || \
  HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
  HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
  HEDLEY_TINYC_VERSION_CHECK(0,9,27) || \
  HEDLEY_CRAY_VERSION_CHECK(8,1,0) || \
  HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define HEDLEY_PREDICT(expr, expected, probability) \
     (((probability) >= 0.9) ? __builtin_expect((expr), (expected)) : (HEDLEY_STATIC_CAST(void, expected), (expr)))
#  define HEDLEY_PREDICT_TRUE(expr, probability) \
     (__extension__ ({ \
       double hedley_probability_ = (probability); \
       ((hedley_probability_ >= 0.9) ? __builtin_expect(!!(expr), 1) : ((hedley_probability_ <= 0.1) ? __builtin_expect(!!(expr), 0) : !!(expr))); \
     }))
#  define HEDLEY_PREDICT_FALSE(expr, probability) \
     (__extension__ ({ \
       double hedley_probability_ = (probability); \
       ((hedley_probability_ >= 0.9) ? __builtin_expect(!!(expr), 0) : ((hedley_probability_ <= 0.1) ? __builtin_expect(!!(expr), 1) : !!(expr))); \
     }))
#  define HEDLEY_LIKELY(expr)   __builtin_expect(!!(expr), 1)
#  define HEDLEY_UNLIKELY(expr) __builtin_expect(!!(expr), 0)
#else
#  define HEDLEY_PREDICT(expr, expected, probability) (HEDLEY_STATIC_CAST(void, expected), (expr))
#  define HEDLEY_PREDICT_TRUE(expr, probability) (!!(expr))
#  define HEDLEY_PREDICT_FALSE(expr, probability) (!!(expr))
#  define HEDLEY_LIKELY(expr) (!!(expr))
#  define HEDLEY_UNLIKELY(expr) (!!(expr))
#endif
#if !defined(HEDLEY_UNPREDICTABLE)
#  define HEDLEY_UNPREDICTABLE(expr) HEDLEY_PREDICT(expr, 1, 0.5)
#endif

#if defined(HEDLEY_MALLOC)
#  undef HEDLEY_MALLOC
#endif
#if \
  HEDLEY_HAS_ATTRIBUTE(malloc) || \
  HEDLEY_GCC_VERSION_CHECK(3,1,0) || \
  HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  HEDLEY_SUNPRO_VERSION_CHECK(5,11,0) || \
  HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
  HEDLEY_IBM_VERSION_CHECK(12,1,0) || \
  HEDLEY_TI_VERSION_CHECK(15,12,0) || \
  (HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
  (HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
  (HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
  (HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
  HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
  HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
  HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define HEDLEY_MALLOC __attribute__((__malloc__))
#elif HEDLEY_SUNPRO_VERSION_CHECK(5,10,0)
#  define HEDLEY_MALLOC _Pragma("returns_new_memory")
#elif \
  HEDLEY_MSVC_VERSION_CHECK(14,0,0) || \
  HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#  define HEDLEY_MALLOC __declspec(restrict)
#else
#  define HEDLEY_MALLOC
#endif

#if defined(HEDLEY_PURE)
#  undef HEDLEY_PURE
#endif
#if \
  HEDLEY_HAS_ATTRIBUTE(pure) || \
  HEDLEY_GCC_VERSION_CHECK(2,96,0) || \
  HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  HEDLEY_SUNPRO_VERSION_CHECK(5,11,0) || \
  HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
  HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
  HEDLEY_TI_VERSION_CHECK(15,12,0) || \
  (HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
  (HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
  (HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
  (HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
  HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
  HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
  HEDLEY_PGI_VERSION_CHECK(17,10,0) || \
  HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define HEDLEY_PURE __attribute__((__pure__))
#elif HEDLEY_SUNPRO_VERSION_CHECK(5,10,0)
#  define HEDLEY_PURE _Pragma("does_not_write_global_data")
#elif defined(__cplusplus) && \
    ( \
      HEDLEY_TI_CL430_VERSION_CHECK(2,0,1) || \
      HEDLEY_TI_CL6X_VERSION_CHECK(4,0,0) || \
      HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) \
    )
#  define HEDLEY_PURE _Pragma("FUNC_IS_PURE;")
#else
#  define HEDLEY_PURE
#endif

#if defined(HEDLEY_CONST)
#  undef HEDLEY_CONST
#endif
#if \
  HEDLEY_HAS_ATTRIBUTE(const) || \
  HEDLEY_GCC_VERSION_CHECK(2,5,0) || \
  HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  HEDLEY_SUNPRO_VERSION_CHECK(5,11,0) || \
  HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
  HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
  HEDLEY_TI_VERSION_CHECK(15,12,0) || \
  (HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
  (HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
  (HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
  (HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
  HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
  HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
  HEDLEY_PGI_VERSION_CHECK(17,10,0) || \
  HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define HEDLEY_CONST __attribute__((__const__))
#elif \
  HEDLEY_SUNPRO_VERSION_CHECK(5,10,0)
#  define HEDLEY_CONST _Pragma("no_side_effect")
#else
#  define HEDLEY_CONST HEDLEY_PURE
#endif

#if defined(HEDLEY_RESTRICT)
#  undef HEDLEY_RESTRICT
#endif
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) && !defined(__cplusplus)
#  define HEDLEY_RESTRICT restrict
#elif \
  HEDLEY_GCC_VERSION_CHECK(3,1,0) || \
  HEDLEY_MSVC_VERSION_CHECK(14,0,0) || \
  HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0) || \
  HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
  HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
  HEDLEY_PGI_VERSION_CHECK(17,10,0) || \
  HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
  HEDLEY_TI_CL2000_VERSION_CHECK(6,2,4) || \
  HEDLEY_TI_CL6X_VERSION_CHECK(8,1,0) || \
  HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
  (HEDLEY_SUNPRO_VERSION_CHECK(5,14,0) && defined(__cplusplus)) || \
  HEDLEY_IAR_VERSION_CHECK(8,0,0) || \
  defined(__clang__) || \
  HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define HEDLEY_RESTRICT __restrict
#elif HEDLEY_SUNPRO_VERSION_CHECK(5,3,0) && !defined(__cplusplus)
#  define HEDLEY_RESTRICT _Restrict
#else
#  define HEDLEY_RESTRICT
#endif

#if defined(HEDLEY_INLINE)
#  undef HEDLEY_INLINE
#endif
#if \
  (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)) || \
  (defined(__cplusplus) && (__cplusplus >= 199711L))
#  define HEDLEY_INLINE inline
#elif \
  defined(HEDLEY_GCC_VERSION) || \
  HEDLEY_ARM_VERSION_CHECK(6,2,0)
#  define HEDLEY_INLINE __inline__
#elif \
  HEDLEY_MSVC_VERSION_CHECK(12,0,0) || \
  HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0) || \
  HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
  HEDLEY_TI_ARMCL_VERSION_CHECK(5,1,0) || \
  HEDLEY_TI_CL430_VERSION_CHECK(3,1,0) || \
  HEDLEY_TI_CL2000_VERSION_CHECK(6,2,0) || \
  HEDLEY_TI_CL6X_VERSION_CHECK(8,0,0) || \
  HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
  HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
  HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define HEDLEY_INLINE __inline
#else
#  define HEDLEY_INLINE
#endif

#if defined(HEDLEY_ALWAYS_INLINE)
#  undef HEDLEY_ALWAYS_INLINE
#endif
#if \
  HEDLEY_HAS_ATTRIBUTE(always_inline) || \
  HEDLEY_GCC_VERSION_CHECK(4,0,0) || \
  HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  HEDLEY_SUNPRO_VERSION_CHECK(5,11,0) || \
  HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
  HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
  HEDLEY_TI_VERSION_CHECK(15,12,0) || \
  (HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
  (HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
  (HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
  (HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
  HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
  HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
  HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10) || \
  HEDLEY_IAR_VERSION_CHECK(8,10,0)
#  define HEDLEY_ALWAYS_INLINE __attribute__((__always_inline__)) HEDLEY_INLINE
#elif \
  HEDLEY_MSVC_VERSION_CHECK(12,0,0) || \
  HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#  define HEDLEY_ALWAYS_INLINE __forceinline
#elif defined(__cplusplus) && \
    ( \
      HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
      HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
      HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
      HEDLEY_TI_CL6X_VERSION_CHECK(6,1,0) || \
      HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
      HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) \
    )
#  define HEDLEY_ALWAYS_INLINE _Pragma("FUNC_ALWAYS_INLINE;")
#elif HEDLEY_IAR_VERSION_CHECK(8,0,0)
#  define HEDLEY_ALWAYS_INLINE _Pragma("inline=forced")
#else
#  define HEDLEY_ALWAYS_INLINE HEDLEY_INLINE
#endif

#if defined(HEDLEY_NEVER_INLINE)
#  undef HEDLEY_NEVER_INLINE
#endif
#if \
  HEDLEY_HAS_ATTRIBUTE(noinline) || \
  HEDLEY_GCC_VERSION_CHECK(4,0,0) || \
  HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  HEDLEY_SUNPRO_VERSION_CHECK(5,11,0) || \
  HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
  HEDLEY_IBM_VERSION_CHECK(10,1,0) || \
  HEDLEY_TI_VERSION_CHECK(15,12,0) || \
  (HEDLEY_TI_ARMCL_VERSION_CHECK(4,8,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_ARMCL_VERSION_CHECK(5,2,0) || \
  (HEDLEY_TI_CL2000_VERSION_CHECK(6,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL2000_VERSION_CHECK(6,4,0) || \
  (HEDLEY_TI_CL430_VERSION_CHECK(4,0,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL430_VERSION_CHECK(4,3,0) || \
  (HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
  HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) || \
  HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
  HEDLEY_TI_CLPRU_VERSION_CHECK(2,1,0) || \
  HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10) || \
  HEDLEY_IAR_VERSION_CHECK(8,10,0)
#  define HEDLEY_NEVER_INLINE __attribute__((__noinline__))
#elif \
  HEDLEY_MSVC_VERSION_CHECK(13,10,0) || \
  HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#  define HEDLEY_NEVER_INLINE __declspec(noinline)
#elif HEDLEY_PGI_VERSION_CHECK(10,2,0)
#  define HEDLEY_NEVER_INLINE _Pragma("noinline")
#elif HEDLEY_TI_CL6X_VERSION_CHECK(6,0,0) && defined(__cplusplus)
#  define HEDLEY_NEVER_INLINE _Pragma("FUNC_CANNOT_INLINE;")
#elif HEDLEY_IAR_VERSION_CHECK(8,0,0)
#  define HEDLEY_NEVER_INLINE _Pragma("inline=never")
#elif HEDLEY_COMPCERT_VERSION_CHECK(3,2,0)
#  define HEDLEY_NEVER_INLINE __attribute((noinline))
#elif HEDLEY_PELLES_VERSION_CHECK(9,0,0)
#  define HEDLEY_NEVER_INLINE __declspec(noinline)
#else
#  define HEDLEY_NEVER_INLINE
#endif

#if defined(HEDLEY_PRIVATE)
#  undef HEDLEY_PRIVATE
#endif
#if defined(HEDLEY_PUBLIC)
#  undef HEDLEY_PUBLIC
#endif
#if defined(HEDLEY_IMPORT)
#  undef HEDLEY_IMPORT
#endif
#if defined(_WIN32) || defined(__CYGWIN__)
#  define HEDLEY_PRIVATE
#  define HEDLEY_PUBLIC   __declspec(dllexport)
#  define HEDLEY_IMPORT   __declspec(dllimport)
#else
#  if \
    HEDLEY_HAS_ATTRIBUTE(visibility) || \
    HEDLEY_GCC_VERSION_CHECK(3,3,0) || \
    HEDLEY_SUNPRO_VERSION_CHECK(5,11,0) || \
    HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    HEDLEY_IBM_VERSION_CHECK(13,1,0) || \
    ( \
      defined(__TI_EABI__) && \
      ( \
        (HEDLEY_TI_CL6X_VERSION_CHECK(7,2,0) && defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
        HEDLEY_TI_CL6X_VERSION_CHECK(7,5,0) \
      ) \
    ) || \
    HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#    define HEDLEY_PRIVATE __attribute__((__visibility__("hidden")))
#    define HEDLEY_PUBLIC  __attribute__((__visibility__("default")))
#  else
#    define HEDLEY_PRIVATE
#    define HEDLEY_PUBLIC
#  endif
#  define HEDLEY_IMPORT    extern
#endif

#if defined(HEDLEY_NO_THROW)
#  undef HEDLEY_NO_THROW
#endif
#if \
  HEDLEY_HAS_ATTRIBUTE(nothrow) || \
  HEDLEY_GCC_VERSION_CHECK(3,3,0) || \
  HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define HEDLEY_NO_THROW __attribute__((__nothrow__))
#elif \
  HEDLEY_MSVC_VERSION_CHECK(13,1,0) || \
  HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0) || \
  HEDLEY_ARM_VERSION_CHECK(4,1,0)
#  define HEDLEY_NO_THROW __declspec(nothrow)
#else
#  define HEDLEY_NO_THROW
#endif

#if defined(HEDLEY_FALL_THROUGH)
# undef HEDLEY_FALL_THROUGH
#endif
#if defined(HEDLEY_INTEL_VERSION)
#  define HEDLEY_FALL_THROUGH
#elif \
  HEDLEY_HAS_ATTRIBUTE(fallthrough) || \
  HEDLEY_GCC_VERSION_CHECK(7,0,0) || \
  HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define HEDLEY_FALL_THROUGH __attribute__((__fallthrough__))
#elif HEDLEY_HAS_CPP_ATTRIBUTE_NS(clang,fallthrough)
#  define HEDLEY_FALL_THROUGH HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[clang::fallthrough]])
#elif HEDLEY_HAS_CPP_ATTRIBUTE(fallthrough)
#  define HEDLEY_FALL_THROUGH HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[fallthrough]])
#elif defined(__fallthrough) /* SAL */
#  define HEDLEY_FALL_THROUGH __fallthrough
#else
#  define HEDLEY_FALL_THROUGH
#endif

#if defined(HEDLEY_RETURNS_NON_NULL)
#  undef HEDLEY_RETURNS_NON_NULL
#endif
#if \
  HEDLEY_HAS_ATTRIBUTE(returns_nonnull) || \
  HEDLEY_GCC_VERSION_CHECK(4,9,0) || \
  HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define HEDLEY_RETURNS_NON_NULL __attribute__((__returns_nonnull__))
#elif defined(_Ret_notnull_) /* SAL */
#  define HEDLEY_RETURNS_NON_NULL _Ret_notnull_
#else
#  define HEDLEY_RETURNS_NON_NULL
#endif

#if defined(HEDLEY_ARRAY_PARAM)
#  undef HEDLEY_ARRAY_PARAM
#endif
#if \
  defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) && \
  !defined(__STDC_NO_VLA__) && \
  !defined(__cplusplus) && \
  !defined(HEDLEY_PGI_VERSION) && \
  !defined(HEDLEY_TINYC_VERSION)
#  define HEDLEY_ARRAY_PARAM(name) (name)
#else
#  define HEDLEY_ARRAY_PARAM(name)
#endif

#if defined(HEDLEY_IS_CONSTANT)
#  undef HEDLEY_IS_CONSTANT
#endif
#if defined(HEDLEY_REQUIRE_CONSTEXPR)
#  undef HEDLEY_REQUIRE_CONSTEXPR
#endif
/* HEDLEY_IS_CONSTEXPR_ is for
   HEDLEY INTERNAL USE ONLY.  API subject to change without notice. */
#if defined(HEDLEY_IS_CONSTEXPR_)
#  undef HEDLEY_IS_CONSTEXPR_
#endif
#if \
  HEDLEY_HAS_BUILTIN(__builtin_constant_p) || \
  HEDLEY_GCC_VERSION_CHECK(3,4,0) || \
  HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  HEDLEY_TINYC_VERSION_CHECK(0,9,19) || \
  HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
  HEDLEY_IBM_VERSION_CHECK(13,1,0) || \
  HEDLEY_TI_CL6X_VERSION_CHECK(6,1,0) || \
  (HEDLEY_SUNPRO_VERSION_CHECK(5,10,0) && !defined(__cplusplus)) || \
  HEDLEY_CRAY_VERSION_CHECK(8,1,0) || \
  HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#  define HEDLEY_IS_CONSTANT(expr) __builtin_constant_p(expr)
#endif
#if !defined(__cplusplus)
#  if \
       HEDLEY_HAS_BUILTIN(__builtin_types_compatible_p) || \
       HEDLEY_GCC_VERSION_CHECK(3,4,0) || \
       HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
       HEDLEY_IBM_VERSION_CHECK(13,1,0) || \
       HEDLEY_CRAY_VERSION_CHECK(8,1,0) || \
       HEDLEY_ARM_VERSION_CHECK(5,4,0) || \
       HEDLEY_TINYC_VERSION_CHECK(0,9,24)
#    if defined(__INTPTR_TYPE__)
#      define HEDLEY_IS_CONSTEXPR_(expr) __builtin_types_compatible_p(__typeof__((1 ? (void*) ((__INTPTR_TYPE__) ((expr) * 0)) : (int*) 0)), int*)
#    else
#      include <stdint.h>
#      define HEDLEY_IS_CONSTEXPR_(expr) __builtin_types_compatible_p(__typeof__((1 ? (void*) ((intptr_t) ((expr) * 0)) : (int*) 0)), int*)
#    endif
#  elif \
       ( \
          defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) && \
          !defined(HEDLEY_SUNPRO_VERSION) && \
          !defined(HEDLEY_PGI_VERSION) && \
          !defined(HEDLEY_IAR_VERSION)) || \
       (HEDLEY_HAS_EXTENSION(c_generic_selections) && !defined(HEDLEY_IAR_VERSION)) || \
       HEDLEY_GCC_VERSION_CHECK(4,9,0) || \
       HEDLEY_INTEL_VERSION_CHECK(17,0,0) || \
       HEDLEY_IBM_VERSION_CHECK(12,1,0) || \
       HEDLEY_ARM_VERSION_CHECK(5,3,0)
#    if defined(__INTPTR_TYPE__)
#      define HEDLEY_IS_CONSTEXPR_(expr) _Generic((1 ? (void*) ((__INTPTR_TYPE__) ((expr) * 0)) : (int*) 0), int*: 1, void*: 0)
#    else
#      include <stdint.h>
#      define HEDLEY_IS_CONSTEXPR_(expr) _Generic((1 ? (void*) ((intptr_t) * 0) : (int*) 0), int*: 1, void*: 0)
#    endif
#  elif \
       defined(HEDLEY_GCC_VERSION) || \
       defined(HEDLEY_INTEL_VERSION) || \
       defined(HEDLEY_TINYC_VERSION) || \
       defined(HEDLEY_TI_ARMCL_VERSION) || \
       HEDLEY_TI_CL430_VERSION_CHECK(18,12,0) || \
       defined(HEDLEY_TI_CL2000_VERSION) || \
       defined(HEDLEY_TI_CL6X_VERSION) || \
       defined(HEDLEY_TI_CL7X_VERSION) || \
       defined(HEDLEY_TI_CLPRU_VERSION) || \
       defined(__clang__)
#    define HEDLEY_IS_CONSTEXPR_(expr) ( \
         sizeof(void) != \
         sizeof(*( \
           1 ? \
             ((void*) ((expr) * 0L) ) : \
             ((struct { char v[sizeof(void) * 2]; } *) 1) \
           ) \
         ) \
       )
#  endif
#endif
#if defined(HEDLEY_IS_CONSTEXPR_)
#  if !defined(HEDLEY_IS_CONSTANT)
#    define HEDLEY_IS_CONSTANT(expr) HEDLEY_IS_CONSTEXPR_(expr)
#  endif
#  define HEDLEY_REQUIRE_CONSTEXPR(expr) (HEDLEY_IS_CONSTEXPR_(expr) ? (expr) : (-1))
#else
#  if !defined(HEDLEY_IS_CONSTANT)
#    define HEDLEY_IS_CONSTANT(expr) (0)
#  endif
#  define HEDLEY_REQUIRE_CONSTEXPR(expr) (expr)
#endif

#if defined(HEDLEY_BEGIN_C_DECLS)
#  undef HEDLEY_BEGIN_C_DECLS
#endif
#if defined(HEDLEY_END_C_DECLS)
#  undef HEDLEY_END_C_DECLS
#endif
#if defined(HEDLEY_C_DECL)
#  undef HEDLEY_C_DECL
#endif
#if defined(__cplusplus)
#  define HEDLEY_BEGIN_C_DECLS extern "C" {
#  define HEDLEY_END_C_DECLS }
#  define HEDLEY_C_DECL extern "C"
#else
#  define HEDLEY_BEGIN_C_DECLS
#  define HEDLEY_END_C_DECLS
#  define HEDLEY_C_DECL
#endif

#if defined(HEDLEY_STATIC_ASSERT)
#  undef HEDLEY_STATIC_ASSERT
#endif
#if \
  !defined(__cplusplus) && ( \
      (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)) || \
      (HEDLEY_HAS_FEATURE(c_static_assert) && !defined(HEDLEY_INTEL_CL_VERSION)) || \
      HEDLEY_GCC_VERSION_CHECK(6,0,0) || \
      HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
      defined(_Static_assert) \
    )
#  define HEDLEY_STATIC_ASSERT(expr, message) _Static_assert(expr, message)
#elif \
  (defined(__cplusplus) && (__cplusplus >= 201103L)) || \
  HEDLEY_MSVC_VERSION_CHECK(16,0,0) || \
  HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#  define HEDLEY_STATIC_ASSERT(expr, message) HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(static_assert(expr, message))
#else
#  define HEDLEY_STATIC_ASSERT(expr, message)
#endif

#if defined(HEDLEY_NULL)
#  undef HEDLEY_NULL
#endif
#if defined(__cplusplus)
#  if __cplusplus >= 201103L
#    define HEDLEY_NULL HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(nullptr)
#  elif defined(NULL)
#    define HEDLEY_NULL NULL
#  else
#    define HEDLEY_NULL HEDLEY_STATIC_CAST(void*, 0)
#  endif
#elif defined(NULL)
#  define HEDLEY_NULL NULL
#else
#  define HEDLEY_NULL ((void*) 0)
#endif

#if defined(HEDLEY_MESSAGE)
#  undef HEDLEY_MESSAGE
#endif
#if HEDLEY_HAS_WARNING("-Wunknown-pragmas")
#  define HEDLEY_MESSAGE(msg) \
  HEDLEY_DIAGNOSTIC_PUSH \
  HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS \
  HEDLEY_PRAGMA(message msg) \
  HEDLEY_DIAGNOSTIC_POP
#elif \
  HEDLEY_GCC_VERSION_CHECK(4,4,0) || \
  HEDLEY_INTEL_VERSION_CHECK(13,0,0)
#  define HEDLEY_MESSAGE(msg) HEDLEY_PRAGMA(message msg)
#elif HEDLEY_CRAY_VERSION_CHECK(5,0,0)
#  define HEDLEY_MESSAGE(msg) HEDLEY_PRAGMA(_CRI message msg)
#elif HEDLEY_IAR_VERSION_CHECK(8,0,0)
#  define HEDLEY_MESSAGE(msg) HEDLEY_PRAGMA(message(msg))
#elif HEDLEY_PELLES_VERSION_CHECK(2,0,0)
#  define HEDLEY_MESSAGE(msg) HEDLEY_PRAGMA(message(msg))
#else
#  define HEDLEY_MESSAGE(msg)
#endif

#if defined(HEDLEY_WARNING)
#  undef HEDLEY_WARNING
#endif
#if HEDLEY_HAS_WARNING("-Wunknown-pragmas")
#  define HEDLEY_WARNING(msg) \
  HEDLEY_DIAGNOSTIC_PUSH \
  HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS \
  HEDLEY_PRAGMA(clang warning msg) \
  HEDLEY_DIAGNOSTIC_POP
#elif \
  HEDLEY_GCC_VERSION_CHECK(4,8,0) || \
  HEDLEY_PGI_VERSION_CHECK(18,4,0) || \
  HEDLEY_INTEL_VERSION_CHECK(13,0,0)
#  define HEDLEY_WARNING(msg) HEDLEY_PRAGMA(GCC warning msg)
#elif \
  HEDLEY_MSVC_VERSION_CHECK(15,0,0) || \
  HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#  define HEDLEY_WARNING(msg) HEDLEY_PRAGMA(message(msg))
#else
#  define HEDLEY_WARNING(msg) HEDLEY_MESSAGE(msg)
#endif

#if defined(HEDLEY_REQUIRE)
#  undef HEDLEY_REQUIRE
#endif
#if defined(HEDLEY_REQUIRE_MSG)
#  undef HEDLEY_REQUIRE_MSG
#endif
#if HEDLEY_HAS_ATTRIBUTE(diagnose_if)
#  if HEDLEY_HAS_WARNING("-Wgcc-compat")
#    define HEDLEY_REQUIRE(expr) \
       HEDLEY_DIAGNOSTIC_PUSH \
       _Pragma("clang diagnostic ignored \"-Wgcc-compat\"") \
       __attribute__((diagnose_if(!(expr), #expr, "error"))) \
       HEDLEY_DIAGNOSTIC_POP
#    define HEDLEY_REQUIRE_MSG(expr,msg) \
       HEDLEY_DIAGNOSTIC_PUSH \
       _Pragma("clang diagnostic ignored \"-Wgcc-compat\"") \
       __attribute__((diagnose_if(!(expr), msg, "error"))) \
       HEDLEY_DIAGNOSTIC_POP
#  else
#    define HEDLEY_REQUIRE(expr) __attribute__((diagnose_if(!(expr), #expr, "error")))
#    define HEDLEY_REQUIRE_MSG(expr,msg) __attribute__((diagnose_if(!(expr), msg, "error")))
#  endif
#else
#  define HEDLEY_REQUIRE(expr)
#  define HEDLEY_REQUIRE_MSG(expr,msg)
#endif

#if defined(HEDLEY_FLAGS)
#  undef HEDLEY_FLAGS
#endif
#if HEDLEY_HAS_ATTRIBUTE(flag_enum) && (!defined(__cplusplus) || HEDLEY_HAS_WARNING("-Wbitfield-enum-conversion"))
#  define HEDLEY_FLAGS __attribute__((__flag_enum__))
#else
#  define HEDLEY_FLAGS
#endif

#if defined(HEDLEY_FLAGS_CAST)
#  undef HEDLEY_FLAGS_CAST
#endif
#if HEDLEY_INTEL_VERSION_CHECK(19,0,0)
#  define HEDLEY_FLAGS_CAST(T, expr) (__extension__ ({ \
  HEDLEY_DIAGNOSTIC_PUSH \
      _Pragma("warning(disable:188)") \
      ((T) (expr)); \
      HEDLEY_DIAGNOSTIC_POP \
    }))
#else
#  define HEDLEY_FLAGS_CAST(T, expr) HEDLEY_STATIC_CAST(T, expr)
#endif

#if defined(HEDLEY_EMPTY_BASES)
#  undef HEDLEY_EMPTY_BASES
#endif
#if \
  (HEDLEY_MSVC_VERSION_CHECK(19,0,23918) && !HEDLEY_MSVC_VERSION_CHECK(20,0,0)) || \
  HEDLEY_INTEL_CL_VERSION_CHECK(2021,1,0)
#  define HEDLEY_EMPTY_BASES __declspec(empty_bases)
#else
#  define HEDLEY_EMPTY_BASES
#endif

/* Remaining macros are deprecated. */

#if defined(HEDLEY_GCC_NOT_CLANG_VERSION_CHECK)
#  undef HEDLEY_GCC_NOT_CLANG_VERSION_CHECK
#endif
#if defined(__clang__)
#  define HEDLEY_GCC_NOT_CLANG_VERSION_CHECK(major,minor,patch) (0)
#else
#  define HEDLEY_GCC_NOT_CLANG_VERSION_CHECK(major,minor,patch) HEDLEY_GCC_VERSION_CHECK(major,minor,patch)
#endif

#if defined(HEDLEY_CLANG_HAS_ATTRIBUTE)
#  undef HEDLEY_CLANG_HAS_ATTRIBUTE
#endif
#define HEDLEY_CLANG_HAS_ATTRIBUTE(attribute) HEDLEY_HAS_ATTRIBUTE(attribute)

#if defined(HEDLEY_CLANG_HAS_CPP_ATTRIBUTE)
#  undef HEDLEY_CLANG_HAS_CPP_ATTRIBUTE
#endif
#define HEDLEY_CLANG_HAS_CPP_ATTRIBUTE(attribute) HEDLEY_HAS_CPP_ATTRIBUTE(attribute)

#if defined(HEDLEY_CLANG_HAS_BUILTIN)
#  undef HEDLEY_CLANG_HAS_BUILTIN
#endif
#define HEDLEY_CLANG_HAS_BUILTIN(builtin) HEDLEY_HAS_BUILTIN(builtin)

#if defined(HEDLEY_CLANG_HAS_FEATURE)
#  undef HEDLEY_CLANG_HAS_FEATURE
#endif
#define HEDLEY_CLANG_HAS_FEATURE(feature) HEDLEY_HAS_FEATURE(feature)

#if defined(HEDLEY_CLANG_HAS_EXTENSION)
#  undef HEDLEY_CLANG_HAS_EXTENSION
#endif
#define HEDLEY_CLANG_HAS_EXTENSION(extension) HEDLEY_HAS_EXTENSION(extension)

#if defined(HEDLEY_CLANG_HAS_DECLSPEC_DECLSPEC_ATTRIBUTE)
#  undef HEDLEY_CLANG_HAS_DECLSPEC_DECLSPEC_ATTRIBUTE
#endif
#define HEDLEY_CLANG_HAS_DECLSPEC_ATTRIBUTE(attribute) HEDLEY_HAS_DECLSPEC_ATTRIBUTE(attribute)

#if defined(HEDLEY_CLANG_HAS_WARNING)
#  undef HEDLEY_CLANG_HAS_WARNING
#endif
#define HEDLEY_CLANG_HAS_WARNING(warning) HEDLEY_HAS_WARNING(warning)

#endif /* !defined(HEDLEY_VERSION) || (HEDLEY_VERSION < X) */
/* :: End simde/hedley.h :: */

#define SIMDE_VERSION_MAJOR 0
#define SIMDE_VERSION_MINOR 8
#define SIMDE_VERSION_MICRO 2
#define SIMDE_VERSION HEDLEY_VERSION_ENCODE(SIMDE_VERSION_MAJOR, SIMDE_VERSION_MINOR, SIMDE_VERSION_MICRO)
// Also update meson.build in the root directory of the repository

#include <stddef.h>
#include <stdint.h>

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/simde-detect-clang.h :: */
/* Detect Clang Version
 * Created by Evan Nemerson <evan@nemerson.com>
 *
 * To the extent possible under law, the author(s) have dedicated all
 * copyright and related and neighboring rights to this software to
 * the public domain worldwide. This software is distributed without
 * any warranty.
 *
 * For details, see <http://creativecommons.org/publicdomain/zero/1.0/>.
 * SPDX-License-Identifier: CC0-1.0
 */

/* This file was originally part of SIMDe
 * (<https://github.com/simd-everywhere/simde>).  You're free to do with it as
 * you please, but I do have a few small requests:
 *
 *  * If you make improvements, please submit them back to SIMDe
 *    (at <https://github.com/simd-everywhere/simde/issues>) so others can
 *    benefit from them.
 *  * Please keep a link to SIMDe intact so people know where to submit
 *    improvements.
 *  * If you expose it publicly, please change the SIMDE_ prefix to
 *    something specific to your project.
 *
 * The version numbers clang exposes (in the ___clang_major__,
 * __clang_minor__, and __clang_patchlevel__ macros) are unreliable.
 * Vendors such as Apple will define these values to their version
 * numbers; for example, "Apple Clang 4.0" is really clang 3.1, but
 * __clang_major__ and __clang_minor__ are defined to 4 and 0
 * respectively, instead of 3 and 1.
 *
 * The solution is *usually* to use clang's feature detection macros
 * (<https://clang.llvm.org/docs/LanguageExtensions.html#feature-checking-macros>)
 * to determine if the feature you're interested in is available.  This
 * generally works well, and it should probably be the first thing you
 * try.  Unfortunately, it's not possible to check for everything.  In
 * particular, compiler bugs.
 *
 * This file just uses the feature checking macros to detect features
 * added in specific versions of clang to identify which version of
 * clang the compiler is based on.
 *
 * Right now it only goes back to 3.6, but I'm happy to accept patches
 * to go back further.  And, of course, newer versions are welcome if
 * they're not already present, and if you find a way to detect a point
 * release that would be great, too!
 */

#if !defined(SIMDE_DETECT_CLANG_H)
#define SIMDE_DETECT_CLANG_H 1

/* Attempt to detect the upstream clang version number.  I usually only
 * worry about major version numbers (at least for 4.0+), but if you
 * need more resolution I'm happy to accept patches that are able to
 * detect minor versions as well.  That said, you'll probably have a
 * hard time with detection since AFAIK most minor releases don't add
 * anything we can detect. Updated based on
 * https://github.com/google/highway/blob/438c705a295176b96a50336527bb3e7ea365ffac/hwy/detect_compiler_arch.h#L73
 * - would welcome patches/updates there as well.
 */

#if defined(__clang__) && !defined(SIMDE_DETECT_CLANG_VERSION)
#  if __has_warning("-Wmissing-designated-field-initializers")
#    define SIMDE_DETECT_CLANG_VERSION 190000
#  elif __has_warning("-Woverriding-option")
#    define SIMDE_DETECT_CLANG_VERSION 180000
#  elif __has_attribute(unsafe_buffer_usage)  // no new warnings in 17.0
#    define SIMDE_DETECT_CLANG_VERSION 170000
#  elif __has_attribute(nouwtable)  // no new warnings in 16.0
#    define SIMDE_DETECT_CLANG_VERSION 160000
#  elif __has_warning("-Warray-parameter")
#    define SIMDE_DETECT_CLANG_VERSION 150000
#  elif __has_warning("-Wbitwise-instead-of-logical")
#    define SIMDE_DETECT_CLANG_VERSION 140000
#  elif __has_warning("-Waix-compat")
#    define SIMDE_DETECT_CLANG_VERSION 130000
#  elif __has_warning("-Wformat-insufficient-args")
#    define SIMDE_DETECT_CLANG_VERSION 120000
#  elif __has_warning("-Wimplicit-const-int-float-conversion")
#    define SIMDE_DETECT_CLANG_VERSION 110000
#  elif __has_warning("-Wmisleading-indentation")
#    define SIMDE_DETECT_CLANG_VERSION 100000
#  elif defined(__FILE_NAME__)
#    define SIMDE_DETECT_CLANG_VERSION 90000
#  elif __has_warning("-Wextra-semi-stmt") || __has_builtin(__builtin_rotateleft32)
#    define SIMDE_DETECT_CLANG_VERSION 80000
// For reasons unknown, Xcode 10.3 (Apple LLVM version 10.0.1) is apparently
// based on Clang 7, but does not support the warning we test.
// See https://en.wikipedia.org/wiki/Xcode#Toolchain_versions and
// https://trac.macports.org/wiki/XcodeVersionInfo.
#  elif __has_warning("-Wc++98-compat-extra-semi") || \
      (defined(__apple_build_version__) && __apple_build_version__ >= 10010000)
#    define SIMDE_DETECT_CLANG_VERSION 70000
#  elif __has_warning("-Wpragma-pack")
#    define SIMDE_DETECT_CLANG_VERSION 60000
#  elif __has_warning("-Wbitfield-enum-conversion")
#    define SIMDE_DETECT_CLANG_VERSION 50000
#  elif __has_attribute(diagnose_if)
#    define SIMDE_DETECT_CLANG_VERSION 40000
#  elif __has_warning("-Wcomma")
#    define SIMDE_DETECT_CLANG_VERSION 39000
#  elif __has_warning("-Wdouble-promotion")
#    define SIMDE_DETECT_CLANG_VERSION 38000
#  elif __has_warning("-Wshift-negative-value")
#    define SIMDE_DETECT_CLANG_VERSION 37000
#  elif __has_warning("-Wambiguous-ellipsis")
#    define SIMDE_DETECT_CLANG_VERSION 36000
#  else
#    define SIMDE_DETECT_CLANG_VERSION 1
#  endif
#endif /* defined(__clang__) && !defined(SIMDE_DETECT_CLANG_VERSION) */

/* The SIMDE_DETECT_CLANG_VERSION_CHECK macro is pretty
 * straightforward; it returns true if the compiler is a derivative
 * of clang >= the specified version.
 *
 * Since this file is often (primarily?) useful for working around bugs
 * it is also helpful to have a macro which returns true if only if the
 * compiler is a version of clang *older* than the specified version to
 * make it a bit easier to ifdef regions to add code for older versions,
 * such as pragmas to disable a specific warning. */

#if defined(SIMDE_DETECT_CLANG_VERSION)
#  define SIMDE_DETECT_CLANG_VERSION_CHECK(major, minor, revision) (SIMDE_DETECT_CLANG_VERSION >= ((major * 10000) + (minor * 1000) + (revision)))
#  define SIMDE_DETECT_CLANG_VERSION_NOT(major, minor, revision) (SIMDE_DETECT_CLANG_VERSION < ((major * 10000) + (minor * 1000) + (revision)))
#else
#  define SIMDE_DETECT_CLANG_VERSION_CHECK(major, minor, revision) (0)
#  define SIMDE_DETECT_CLANG_VERSION_NOT(major, minor, revision) (0)
#endif

#endif /* !defined(SIMDE_DETECT_CLANG_H) */
/* :: End simde/simde-detect-clang.h :: */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/simde-arch.h :: */
/* Architecture detection
 * Created by Evan Nemerson <evan@nemerson.com>
 *
 *   To the extent possible under law, the authors have waived all
 *   copyright and related or neighboring rights to this code.  For
 *   details, see the Creative Commons Zero 1.0 Universal license at
 *   <https://creativecommons.org/publicdomain/zero/1.0/>
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Different compilers define different preprocessor macros for the
 * same architecture.  This is an attempt to provide a single
 * interface which is usable on any compiler.
 *
 * In general, a macro named SIMDE_ARCH_* is defined for each
 * architecture the CPU supports.  When there are multiple possible
 * versions, we try to define the macro to the target version.  For
 * example, if you want to check for i586+, you could do something
 * like:
 *
 *   #if defined(SIMDE_ARCH_X86) && (SIMDE_ARCH_X86 >= 5)
 *   ...
 *   #endif
 *
 * You could also just check that SIMDE_ARCH_X86 >= 5 without checking
 * if it's defined first, but some compilers may emit a warning about
 * an undefined macro being used (e.g., GCC with -Wundef).
 *
 * This was originally created for SIMDe
 * <https://github.com/simd-everywhere/simde> (hence the prefix), but this
 * header has no dependencies and may be used anywhere.  It is
 * originally based on information from
 * <https://sourceforge.net/p/predef/wiki/Architectures/>, though it
 * has been enhanced with additional information.
 *
 * If you improve this file, or find a bug, please file the issue at
 * <https://github.com/simd-everywhere/simde/issues>.  If you copy this into
 * your project, even if you change the prefix, please keep the links
 * to SIMDe intact so others know where to report issues, submit
 * enhancements, and find the latest version. */

#if !defined(SIMDE_ARCH_H)
#define SIMDE_ARCH_H

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */

/* Alpha
   <https://en.wikipedia.org/wiki/DEC_Alpha> */
#if defined(__alpha__) || defined(__alpha) || defined(_M_ALPHA)
#  if defined(__alpha_ev6__)
#    define SIMDE_ARCH_ALPHA 6
#  elif defined(__alpha_ev5__)
#    define SIMDE_ARCH_ALPHA 5
#  elif defined(__alpha_ev4__)
#    define SIMDE_ARCH_ALPHA 4
#  else
#    define SIMDE_ARCH_ALPHA 1
#  endif
#endif
#if defined(SIMDE_ARCH_ALPHA)
#  define SIMDE_ARCH_ALPHA_CHECK(version) ((version) <= SIMDE_ARCH_ALPHA)
#else
#  define SIMDE_ARCH_ALPHA_CHECK(version) (0)
#endif

/* Atmel AVR
   <https://en.wikipedia.org/wiki/Atmel_AVR> */
#if defined(__AVR_ARCH__)
#  define SIMDE_ARCH_AVR __AVR_ARCH__
#endif

/* AMD64 / x86_64
   <https://en.wikipedia.org/wiki/X86-64> */
#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) || defined(_M_X64) || defined(_M_AMD64)
#  if !defined(_M_ARM64EC)
#     define SIMDE_ARCH_AMD64 1000
#  endif
#endif

/* ARM
   <https://en.wikipedia.org/wiki/ARM_architecture> */
#if defined(__ARM_ARCH)
#  if __ARM_ARCH > 100
#    define SIMDE_ARCH_ARM (__ARM_ARCH)
#  else
#    define SIMDE_ARCH_ARM (__ARM_ARCH * 100)
#  endif
#elif defined(_M_ARM)
#  if _M_ARM > 100
#    define SIMDE_ARCH_ARM (_M_ARM)
#  else
#    define SIMDE_ARCH_ARM (_M_ARM * 100)
#  endif
#elif defined(_M_ARM64) || defined(_M_ARM64EC)
#  define SIMDE_ARCH_ARM 800
#elif defined(__arm__) || defined(__thumb__) || defined(__TARGET_ARCH_ARM) || defined(_ARM) || defined(_M_ARM) || defined(_M_ARM)
#  define SIMDE_ARCH_ARM 1
#endif
#if defined(SIMDE_ARCH_ARM)
#  define SIMDE_ARCH_ARM_CHECK(major, minor) (((major * 100) + (minor)) <= SIMDE_ARCH_ARM)
#else
#  define SIMDE_ARCH_ARM_CHECK(major, minor) (0)
#endif

/* AArch64
   <https://en.wikipedia.org/wiki/ARM_architecture> */
#if defined(__aarch64__) || defined(_M_ARM64) || defined(_M_ARM64EC)
#  define SIMDE_ARCH_AARCH64 1000
#endif
#if defined(SIMDE_ARCH_AARCH64)
#  define SIMDE_ARCH_AARCH64_CHECK(version) ((version) <= SIMDE_ARCH_AARCH64)
#else
#  define SIMDE_ARCH_AARCH64_CHECK(version) (0)
#endif

/* ARM SIMD ISA extensions */
#if defined(__ARM_NEON) || defined(SIMDE_ARCH_AARCH64)
#  if defined(SIMDE_ARCH_AARCH64)
#    define SIMDE_ARCH_ARM_NEON SIMDE_ARCH_AARCH64
#  elif defined(SIMDE_ARCH_ARM)
#    define SIMDE_ARCH_ARM_NEON SIMDE_ARCH_ARM
#  endif
#endif
#if defined(__ARM_FEATURE_AES) && __ARM_FEATURE_AES
#  define SIMDE_ARCH_ARM_AES
#endif
#if defined(__ARM_FEATURE_COMPLEX) && __ARM_FEATURE_COMPLEX
#  define SIMDE_ARCH_ARM_COMPLEX
#endif
#if defined(__ARM_FEATURE_CRYPTO) && __ARM_FEATURE_CRYPTO
#  define SIMDE_ARCH_ARM_CRYPTO
#endif
#if defined(__ARM_FEATURE_DOTPROD) && __ARM_FEATURE_DOTPROD
#  define SIMDE_ARCH_ARM_DOTPROD
#endif
#if defined(__ARM_FEATURE_FMA) && __ARM_FEATURE_FMA
#  define SIMDE_ARCH_ARM_FMA
#endif
#if defined(__ARM_FEATURE_FP16_FML) && __ARM_FEATURE_FP16_FML
#  define SIMDE_ARCH_ARM_FP16_FML
#endif
#if defined(__ARM_FEATURE_FRINT) && __ARM_FEATURE_FRINT
#  define SIMDE_ARCH_ARM_FRINT
#endif
#if defined(__ARM_FEATURE_MATMUL_INT8) && __ARM_FEATURE_MATMUL_INT8
#  define SIMDE_ARCH_ARM_MATMUL_INT8
#endif
#if defined(__ARM_FEATURE_SHA2) && __ARM_FEATURE_SHA2 && !defined(__APPLE_CC__)
#  define SIMDE_ARCH_ARM_SHA2
#endif
#if defined(__ARM_FEATURE_SHA3) && __ARM_FEATURE_SHA3
#  define SIMDE_ARCH_ARM_SHA3
#endif
#if defined(__ARM_FEATURE_SHA512) && __ARM_FEATURE_SHA512
#  define SIMDE_ARCH_ARM_SHA512
#endif
#if defined(__ARM_FEATURE_SM3) && __ARM_FEATURE_SM3
#  define SIMDE_ARCH_ARM_SM3
#endif
#if defined(__ARM_FEATURE_SM4) && __ARM_FEATURE_SM4
#  define SIMDE_ARCH_ARM_SM4
#endif
#if defined(__ARM_FEATURE_SVE) && __ARM_FEATURE_SVE
#  define SIMDE_ARCH_ARM_SVE
#endif
#if defined(__ARM_FEATURE_QRDMX) && __ARM_FEATURE_QRDMX
#  define SIMDE_ARCH_ARM_QRDMX
#endif

/* Blackfin
   <https://en.wikipedia.org/wiki/Blackfin> */
#if defined(__bfin) || defined(__BFIN__) || defined(__bfin__)
#  define SIMDE_ARCH_BLACKFIN 1
#endif

/* CRIS
   <https://en.wikipedia.org/wiki/ETRAX_CRIS> */
#if defined(__CRIS_arch_version)
#  define SIMDE_ARCH_CRIS __CRIS_arch_version
#elif defined(__cris__) || defined(__cris) || defined(__CRIS) || defined(__CRIS__)
#  define SIMDE_ARCH_CRIS 1
#endif

/* Convex
   <https://en.wikipedia.org/wiki/Convex_Computer> */
#if defined(__convex_c38__)
#  define SIMDE_ARCH_CONVEX 38
#elif defined(__convex_c34__)
#  define SIMDE_ARCH_CONVEX 34
#elif defined(__convex_c32__)
#  define SIMDE_ARCH_CONVEX 32
#elif defined(__convex_c2__)
#  define SIMDE_ARCH_CONVEX 2
#elif defined(__convex__)
#  define SIMDE_ARCH_CONVEX 1
#endif
#if defined(SIMDE_ARCH_CONVEX)
#  define SIMDE_ARCH_CONVEX_CHECK(version) ((version) <= SIMDE_ARCH_CONVEX)
#else
#  define SIMDE_ARCH_CONVEX_CHECK(version) (0)
#endif

/* Adapteva Epiphany
   <https://en.wikipedia.org/wiki/Adapteva_Epiphany> */
#if defined(__epiphany__)
#  define SIMDE_ARCH_EPIPHANY 1
#endif

/* Fujitsu FR-V
   <https://en.wikipedia.org/wiki/FR-V_(microprocessor)> */
#if defined(__frv__)
#  define SIMDE_ARCH_FRV 1
#endif

/* H8/300
   <https://en.wikipedia.org/wiki/H8_Family> */
#if defined(__H8300__)
#  define SIMDE_ARCH_H8300
#endif

/* Elbrus (8S, 8SV and successors)
   <https://en.wikipedia.org/wiki/Elbrus-8S> */
#if defined(__e2k__)
#  define SIMDE_ARCH_E2K
#endif

/* HP/PA / PA-RISC
   <https://en.wikipedia.org/wiki/PA-RISC> */
#if defined(__PA8000__) || defined(__HPPA20__) || defined(__RISC2_0__) || defined(_PA_RISC2_0)
#  define SIMDE_ARCH_HPPA 20
#elif defined(__PA7100__) || defined(__HPPA11__) || defined(_PA_RISC1_1)
#  define SIMDE_ARCH_HPPA 11
#elif defined(_PA_RISC1_0)
#  define SIMDE_ARCH_HPPA 10
#elif defined(__hppa__) || defined(__HPPA__) || defined(__hppa)
#  define SIMDE_ARCH_HPPA 1
#endif
#if defined(SIMDE_ARCH_HPPA)
#  define SIMDE_ARCH_HPPA_CHECK(version) ((version) <= SIMDE_ARCH_HPPA)
#else
#  define SIMDE_ARCH_HPPA_CHECK(version) (0)
#endif

/* x86
   <https://en.wikipedia.org/wiki/X86> */
#if defined(_M_IX86)
#  define SIMDE_ARCH_X86 (_M_IX86 / 100)
#elif defined(__I86__)
#  define SIMDE_ARCH_X86 __I86__
#elif defined(i686) || defined(__i686) || defined(__i686__)
#  define SIMDE_ARCH_X86 6
#elif defined(i586) || defined(__i586) || defined(__i586__)
#  define SIMDE_ARCH_X86 5
#elif defined(i486) || defined(__i486) || defined(__i486__)
#  define SIMDE_ARCH_X86 4
#elif defined(i386) || defined(__i386) || defined(__i386__)
#  define SIMDE_ARCH_X86 3
#elif defined(_X86_) || defined(__X86__) || defined(__THW_INTEL__)
#  define SIMDE_ARCH_X86 3
#endif
#if defined(SIMDE_ARCH_X86)
#  define SIMDE_ARCH_X86_CHECK(version) ((version) <= SIMDE_ARCH_X86)
#else
#  define SIMDE_ARCH_X86_CHECK(version) (0)
#endif

/* SIMD ISA extensions for x86/x86_64 and Elbrus */
#if defined(SIMDE_ARCH_X86) || defined(SIMDE_ARCH_AMD64) || defined(SIMDE_ARCH_E2K)
#  if defined(_M_IX86_FP)
#    define SIMDE_ARCH_X86_MMX
#    if (_M_IX86_FP >= 1)
#      define SIMDE_ARCH_X86_SSE 1
#    endif
#    if (_M_IX86_FP >= 2)
#      define SIMDE_ARCH_X86_SSE2 1
#    endif
#  elif defined(_M_X64)
#    define SIMDE_ARCH_X86_SSE 1
#    define SIMDE_ARCH_X86_SSE2 1
#  else
#    if defined(__MMX__)
#      define SIMDE_ARCH_X86_MMX 1
#    endif
#    if defined(__SSE__)
#      define SIMDE_ARCH_X86_SSE 1
#    endif
#    if defined(__SSE2__)
#      define SIMDE_ARCH_X86_SSE2 1
#    endif
#  endif
#  if defined(__SSE3__)
#    define SIMDE_ARCH_X86_SSE3 1
#  endif
#  if defined(__SSSE3__)
#    define SIMDE_ARCH_X86_SSSE3 1
#  endif
#  if defined(__SSE4_1__)
#    define SIMDE_ARCH_X86_SSE4_1 1
#  endif
#  if defined(__SSE4_2__)
#    define SIMDE_ARCH_X86_SSE4_2 1
#  endif
#  if defined(__XOP__)
#    define SIMDE_ARCH_X86_XOP 1
#  endif
#  if defined(__AVX__)
#    define SIMDE_ARCH_X86_AVX 1
#    if !defined(SIMDE_ARCH_X86_SSE3)
#      define SIMDE_ARCH_X86_SSE3 1
#    endif
#    if !defined(SIMDE_ARCH_X86_SSE4_1)
#      define SIMDE_ARCH_X86_SSE4_1 1
#    endif
#    if !defined(SIMDE_ARCH_X86_SSE4_2)
#      define SIMDE_ARCH_X86_SSE4_2 1
#    endif
#  endif
#  if defined(__AVX2__)
#    define SIMDE_ARCH_X86_AVX2 1
#    if defined(_MSC_VER)
#      define SIMDE_ARCH_X86_FMA 1
#    endif
#  endif
#  if defined(__FMA__)
#    define SIMDE_ARCH_X86_FMA 1
#    if !defined(SIMDE_ARCH_X86_AVX)
#      define SIMDE_ARCH_X86_AVX 1
#    endif
#  endif
#  if defined(__AVX512VP2INTERSECT__)
#    define SIMDE_ARCH_X86_AVX512VP2INTERSECT 1
#  endif
#  if defined(__AVX512BITALG__)
#    define SIMDE_ARCH_X86_AVX512BITALG 1
#  endif
#  if defined(__AVX512VPOPCNTDQ__)
#    define SIMDE_ARCH_X86_AVX512VPOPCNTDQ 1
#  endif
#  if defined(__AVX512VBMI__)
#    define SIMDE_ARCH_X86_AVX512VBMI 1
#  endif
#  if defined(__AVX512VBMI2__)
#    define SIMDE_ARCH_X86_AVX512VBMI2 1
#  endif
#  if defined(__AVX512VNNI__)
#    define SIMDE_ARCH_X86_AVX512VNNI 1
#  endif
#  if defined(__AVX5124VNNIW__)
#    define SIMDE_ARCH_X86_AVX5124VNNIW 1
#  endif
#  if defined(__AVX512BW__)
#    define SIMDE_ARCH_X86_AVX512BW 1
#  endif
#  if defined(__AVX512BF16__)
#    define SIMDE_ARCH_X86_AVX512BF16 1
#  endif
#  if defined(__AVX512CD__)
#    define SIMDE_ARCH_X86_AVX512CD 1
#  endif
#  if defined(__AVX512DQ__)
#    define SIMDE_ARCH_X86_AVX512DQ 1
#  endif
#  if defined(__AVX512F__)
#    define SIMDE_ARCH_X86_AVX512F 1
#  endif
#  if defined(__AVX512VL__)
#    define SIMDE_ARCH_X86_AVX512VL 1
#  endif
#  if defined(__AVX512FP16__)
#    define SIMDE_ARCH_X86_AVX512FP16 1
#  endif
#  if defined(__GFNI__)
#    define SIMDE_ARCH_X86_GFNI 1
#  endif
#  if defined(__PCLMUL__)
#    define SIMDE_ARCH_X86_PCLMUL 1
#  endif
#  if defined(__VPCLMULQDQ__)
#    define SIMDE_ARCH_X86_VPCLMULQDQ 1
#  endif
#  if defined(__F16C__) || (defined(HEDLEY_MSVC_VERSION) && HEDLEY_MSVC_VERSION_CHECK(19,30,0) && defined(SIMDE_ARCH_X86_AVX2) )
#    define SIMDE_ARCH_X86_F16C 1
#  endif
#  if defined(__AES__)
#    define SIMDE_ARCH_X86_AES 1
#  endif
#endif

/* Itanium
   <https://en.wikipedia.org/wiki/Itanium> */
#if defined(__ia64__) || defined(_IA64) || defined(__IA64__) || defined(__ia64) || defined(_M_IA64) || defined(__itanium__)
#  define SIMDE_ARCH_IA64 1
#endif

/* Renesas M32R
   <https://en.wikipedia.org/wiki/M32R> */
#if defined(__m32r__) || defined(__M32R__)
#  define SIMDE_ARCH_M32R
#endif

/* Motorola 68000
   <https://en.wikipedia.org/wiki/Motorola_68000> */
#if defined(__mc68060__) || defined(__MC68060__)
#  define SIMDE_ARCH_M68K 68060
#elif defined(__mc68040__) || defined(__MC68040__)
#  define SIMDE_ARCH_M68K 68040
#elif defined(__mc68030__) || defined(__MC68030__)
#  define SIMDE_ARCH_M68K 68030
#elif defined(__mc68020__) || defined(__MC68020__)
#  define SIMDE_ARCH_M68K 68020
#elif defined(__mc68010__) || defined(__MC68010__)
#  define SIMDE_ARCH_M68K 68010
#elif defined(__mc68000__) || defined(__MC68000__)
#  define SIMDE_ARCH_M68K 68000
#endif
#if defined(SIMDE_ARCH_M68K)
#  define SIMDE_ARCH_M68K_CHECK(version) ((version) <= SIMDE_ARCH_M68K)
#else
#  define SIMDE_ARCH_M68K_CHECK(version) (0)
#endif

/* Xilinx MicroBlaze
   <https://en.wikipedia.org/wiki/MicroBlaze> */
#if defined(__MICROBLAZE__) || defined(__microblaze__)
#  define SIMDE_ARCH_MICROBLAZE
#endif

/* MIPS
   <https://en.wikipedia.org/wiki/MIPS_architecture> */
#if defined(_MIPS_ISA_MIPS64R2)
#  define SIMDE_ARCH_MIPS 642
#elif defined(_MIPS_ISA_MIPS64)
#  define SIMDE_ARCH_MIPS 640
#elif defined(_MIPS_ISA_MIPS32R2)
#  define SIMDE_ARCH_MIPS 322
#elif defined(_MIPS_ISA_MIPS32)
#  define SIMDE_ARCH_MIPS 320
#elif defined(_MIPS_ISA_MIPS4)
#  define SIMDE_ARCH_MIPS 4
#elif defined(_MIPS_ISA_MIPS3)
#  define SIMDE_ARCH_MIPS 3
#elif defined(_MIPS_ISA_MIPS2)
#  define SIMDE_ARCH_MIPS 2
#elif defined(_MIPS_ISA_MIPS1)
#  define SIMDE_ARCH_MIPS 1
#elif defined(_MIPS_ISA_MIPS) || defined(__mips) || defined(__MIPS__)
#  define SIMDE_ARCH_MIPS 1
#endif
#if defined(SIMDE_ARCH_MIPS)
#  define SIMDE_ARCH_MIPS_CHECK(version) ((version) <= SIMDE_ARCH_MIPS)
#else
#  define SIMDE_ARCH_MIPS_CHECK(version) (0)
#endif

#if defined(__mips_loongson_mmi)
#  define SIMDE_ARCH_MIPS_LOONGSON_MMI 1
#endif

#if defined(__mips_msa)
#  define SIMDE_ARCH_MIPS_MSA 1
#endif

/* Matsushita MN10300
   <https://en.wikipedia.org/wiki/MN103> */
#if defined(__MN10300__) || defined(__mn10300__)
#  define SIMDE_ARCH_MN10300 1
#endif

/* POWER
   <https://en.wikipedia.org/wiki/IBM_POWER_Instruction_Set_Architecture> */
#if defined(_M_PPC)
#  define SIMDE_ARCH_POWER _M_PPC
#elif defined(_ARCH_PWR9)
#  define SIMDE_ARCH_POWER 900
#elif defined(_ARCH_PWR8)
#  define SIMDE_ARCH_POWER 800
#elif defined(_ARCH_PWR7)
#  define SIMDE_ARCH_POWER 700
#elif defined(_ARCH_PWR6)
#  define SIMDE_ARCH_POWER 600
#elif defined(_ARCH_PWR5)
#  define SIMDE_ARCH_POWER 500
#elif defined(_ARCH_PWR4)
#  define SIMDE_ARCH_POWER 400
#elif defined(_ARCH_440) || defined(__ppc440__)
#  define SIMDE_ARCH_POWER 440
#elif defined(_ARCH_450) || defined(__ppc450__)
#  define SIMDE_ARCH_POWER 450
#elif defined(_ARCH_601) || defined(__ppc601__)
#  define SIMDE_ARCH_POWER 601
#elif defined(_ARCH_603) || defined(__ppc603__)
#  define SIMDE_ARCH_POWER 603
#elif defined(_ARCH_604) || defined(__ppc604__)
#  define SIMDE_ARCH_POWER 604
#elif defined(_ARCH_605) || defined(__ppc605__)
#  define SIMDE_ARCH_POWER 605
#elif defined(_ARCH_620) || defined(__ppc620__)
#  define SIMDE_ARCH_POWER 620
#elif defined(__powerpc) || defined(__powerpc__) || defined(__POWERPC__) || defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC) || defined(__ppc)
#  define SIMDE_ARCH_POWER 1
#endif
#if defined(SIMDE_ARCH_POWER)
  #define SIMDE_ARCH_POWER_CHECK(version) ((version) <= SIMDE_ARCH_POWER)
#else
  #define SIMDE_ARCH_POWER_CHECK(version) (0)
#endif

#if defined(__ALTIVEC__)
#  define SIMDE_ARCH_POWER_ALTIVEC SIMDE_ARCH_POWER
  #define SIMDE_ARCH_POWER_ALTIVEC_CHECK(version) ((version) <= SIMDE_ARCH_POWER)
#else
  #define SIMDE_ARCH_POWER_ALTIVEC_CHECK(version) (0)
#endif

/* RISC-V
   <https://en.wikipedia.org/wiki/RISC-V> */
#if defined(__riscv) || defined(__riscv__)
#  if __riscv_xlen == 64
#     define SIMDE_ARCH_RISCV64
#  elif __riscv_xlen == 32
#     define SIMDE_ARCH_RISCV32
#  endif
#endif

/* RISC-V SIMD ISA extensions */
#if defined(__riscv_zve32x)
#  define SIMDE_ARCH_RISCV_ZVE32X 1
#endif
#if defined(__riscv_zve32f)
#  define SIMDE_ARCH_RISCV_ZVE32F 1
#endif
#if defined(__riscv_zve64x)
#  define SIMDE_ARCH_RISCV_ZVE64X 1
#endif
#if defined(__riscv_zve64f)
#  define SIMDE_ARCH_RISCV_ZVE64F 1
#endif
#if defined(__riscv_zve64d)
#  define SIMDE_ARCH_RISCV_ZVE64D 1
#endif
#if defined(__riscv_v)
#  define SIMDE_ARCH_RISCV_V 1
#endif
#if defined(__riscv_zvfh)
#  define SIMDE_ARCH_RISCV_ZVFH 1
#endif
#if defined(__riscv_zvfhmin)
#  define SIMDE_ARCH_RISCV_ZVFHMIN 1
#endif

/* SPARC
   <https://en.wikipedia.org/wiki/SPARC> */
#if defined(__sparc_v9__) || defined(__sparcv9)
#  define SIMDE_ARCH_SPARC 9
#elif defined(__sparc_v8__) || defined(__sparcv8)
#  define SIMDE_ARCH_SPARC 8
#elif defined(__sparc_v7__) || defined(__sparcv7)
#  define SIMDE_ARCH_SPARC 7
#elif defined(__sparc_v6__) || defined(__sparcv6)
#  define SIMDE_ARCH_SPARC 6
#elif defined(__sparc_v5__) || defined(__sparcv5)
#  define SIMDE_ARCH_SPARC 5
#elif defined(__sparc_v4__) || defined(__sparcv4)
#  define SIMDE_ARCH_SPARC 4
#elif defined(__sparc_v3__) || defined(__sparcv3)
#  define SIMDE_ARCH_SPARC 3
#elif defined(__sparc_v2__) || defined(__sparcv2)
#  define SIMDE_ARCH_SPARC 2
#elif defined(__sparc_v1__) || defined(__sparcv1)
#  define SIMDE_ARCH_SPARC 1
#elif defined(__sparc__) || defined(__sparc)
#  define SIMDE_ARCH_SPARC 1
#endif
#if defined(SIMDE_ARCH_SPARC)
  #define SIMDE_ARCH_SPARC_CHECK(version) ((version) <= SIMDE_ARCH_SPARC)
#else
  #define SIMDE_ARCH_SPARC_CHECK(version) (0)
#endif

/* SuperH
   <https://en.wikipedia.org/wiki/SuperH> */
#if defined(__sh5__) || defined(__SH5__)
#  define SIMDE_ARCH_SUPERH 5
#elif defined(__sh4__) || defined(__SH4__)
#  define SIMDE_ARCH_SUPERH 4
#elif defined(__sh3__) || defined(__SH3__)
#  define SIMDE_ARCH_SUPERH 3
#elif defined(__sh2__) || defined(__SH2__)
#  define SIMDE_ARCH_SUPERH 2
#elif defined(__sh1__) || defined(__SH1__)
#  define SIMDE_ARCH_SUPERH 1
#elif defined(__sh__) || defined(__SH__)
#  define SIMDE_ARCH_SUPERH 1
#endif

/* IBM System z
   <https://en.wikipedia.org/wiki/IBM_System_z> */
#if defined(__370__) || defined(__THW_370__) || defined(__s390__) || defined(__s390x__) || defined(__zarch__) || defined(__SYSC_ZARCH__)
#  define SIMDE_ARCH_ZARCH __ARCH__
#endif
#if defined(SIMDE_ARCH_ZARCH)
  #define SIMDE_ARCH_ZARCH_CHECK(version) ((version) <= SIMDE_ARCH_ZARCH)
#else
  #define SIMDE_ARCH_ZARCH_CHECK(version) (0)
#endif

#if defined(SIMDE_ARCH_ZARCH) && defined(__VEC__)
  #define SIMDE_ARCH_ZARCH_ZVECTOR SIMDE_ARCH_ZARCH
#endif

/* TMS320 DSP
   <https://en.wikipedia.org/wiki/Texas_Instruments_TMS320> */
#if defined(_TMS320C6740) || defined(__TMS320C6740__)
#  define SIMDE_ARCH_TMS320 6740
#elif defined(_TMS320C6700_PLUS) || defined(__TMS320C6700_PLUS__)
#  define SIMDE_ARCH_TMS320 6701
#elif defined(_TMS320C6700) || defined(__TMS320C6700__)
#  define SIMDE_ARCH_TMS320 6700
#elif defined(_TMS320C6600) || defined(__TMS320C6600__)
#  define SIMDE_ARCH_TMS320 6600
#elif defined(_TMS320C6400_PLUS) || defined(__TMS320C6400_PLUS__)
#  define SIMDE_ARCH_TMS320 6401
#elif defined(_TMS320C6400) || defined(__TMS320C6400__)
#  define SIMDE_ARCH_TMS320 6400
#elif defined(_TMS320C6200) || defined(__TMS320C6200__)
#  define SIMDE_ARCH_TMS320 6200
#elif defined(_TMS320C55X) || defined(__TMS320C55X__)
#  define SIMDE_ARCH_TMS320 550
#elif defined(_TMS320C54X) || defined(__TMS320C54X__)
#  define SIMDE_ARCH_TMS320 540
#elif defined(_TMS320C28X) || defined(__TMS320C28X__)
#  define SIMDE_ARCH_TMS320 280
#endif
#if defined(SIMDE_ARCH_TMS320)
  #define SIMDE_ARCH_TMS320_CHECK(version) ((version) <= SIMDE_ARCH_TMS320)
#else
  #define SIMDE_ARCH_TMS320_CHECK(version) (0)
#endif

/* WebAssembly */
#if defined(__wasm__)
#  define SIMDE_ARCH_WASM 1
#endif

#if defined(SIMDE_ARCH_WASM) && defined(__wasm_simd128__)
#  define SIMDE_ARCH_WASM_SIMD128
#endif

#if defined(SIMDE_ARCH_WASM) && defined(__wasm_relaxed_simd__)
#  define SIMDE_ARCH_WASM_RELAXED_SIMD
#endif

/* Xtensa
   <https://en.wikipedia.org/wiki/> */
#if defined(__xtensa__) || defined(__XTENSA__)
#  define SIMDE_ARCH_XTENSA 1
#endif

/* Availability of 16-bit floating-point arithmetic intrinsics */
#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
#  define SIMDE_ARCH_ARM_NEON_FP16
#endif

/* Availability of 16-bit brain floating-point arithmetic intrinsics */
#if defined(__ARM_FEATURE_BF16_VECTOR_ARITHMETIC)
#  define SIMDE_ARCH_ARM_NEON_BF16
#endif

/* LoongArch
   <https://en.wikipedia.org/wiki/Loongson#LoongArch> */
#if defined(__loongarch32)
#  define SIMDE_ARCH_LOONGARCH 1
#elif defined(__loongarch64)
#  define SIMDE_ARCH_LOONGARCH 2
#endif

/* LSX: LoongArch 128-bits SIMD extension */
#if defined(__loongarch_sx)
#  define SIMDE_ARCH_LOONGARCH_LSX 1
#endif

/* LASX: LoongArch 256-bits SIMD extension */
#if defined(__loongarch_asx)
#  define SIMDE_ARCH_LOONGARCH_LASX 2
#endif

#endif /* !defined(SIMDE_ARCH_H) */
/* :: End simde/simde-arch.h :: */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/simde-features.h :: */
/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2020      Evan Nemerson <evan@nemerson.com>
 *   2023      Ju-Hung Li <jhlee@pllab.cs.nthu.edu.tw> (Copyright owned by NTHU pllab)
 */

/* simde-arch.h is used to determine which features are available according
   to the compiler.  However, we want to make it possible to forcibly enable
   or disable APIs */

#if !defined(SIMDE_FEATURES_H)
#define SIMDE_FEATURES_H

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/simde-diagnostic.h :: */
/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2017-2020 Evan Nemerson <evan@nemerson.com>
 */

/* SIMDe targets a very wide range of standards and compilers, and our
 * goal is to compile cleanly even with extremely aggressive warnings
 * (i.e., -Weverything in clang, -Wextra in GCC, /W4 for MSVC, etc.)
 * treated as errors.
 *
 * While our preference is to resolve the underlying issue a given
 * diagnostic is warning us about, sometimes that's not possible.
 * Fixing a warning in one compiler may cause problems in another.
 * Sometimes a warning doesn't really apply to us (false positives),
 * and sometimes adhering to a warning would mean dropping a feature
 * we *know* the compiler supports since we have tested specifically
 * for the compiler or feature.
 *
 * When practical, warnings are only disabled for specific code.  For
 * a list of warnings which are enabled by default in all SIMDe code,
 * see SIMDE_DISABLE_UNWANTED_DIAGNOSTICS.  Note that we restore the
 * warning stack when SIMDe is done parsing, so code which includes
 * SIMDe is not deprived of these warnings.
 */

#if !defined(SIMDE_DIAGNOSTIC_H)
#define SIMDE_DIAGNOSTIC_H

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */

/* This is only to help us implement functions like _mm_undefined_ps. */
#if defined(SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_)
  #undef SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_
#endif
#if HEDLEY_HAS_WARNING("-Wuninitialized")
  #define SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_ _Pragma("clang diagnostic ignored \"-Wuninitialized\"")
#elif HEDLEY_GCC_VERSION_CHECK(4,2,0)
  #define SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_ _Pragma("GCC diagnostic ignored \"-Wuninitialized\"")
#elif HEDLEY_PGI_VERSION_CHECK(19,10,0)
  #define SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_ _Pragma("diag_suppress 549")
#elif HEDLEY_SUNPRO_VERSION_CHECK(5,14,0) && defined(__cplusplus)
  #define SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_ _Pragma("error_messages(off,SEC_UNINITIALIZED_MEM_READ,SEC_UNDEFINED_RETURN_VALUE,unassigned)")
#elif HEDLEY_SUNPRO_VERSION_CHECK(5,14,0)
  #define SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_ _Pragma("error_messages(off,SEC_UNINITIALIZED_MEM_READ,SEC_UNDEFINED_RETURN_VALUE)")
#elif HEDLEY_SUNPRO_VERSION_CHECK(5,12,0) && defined(__cplusplus)
  #define SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_ _Pragma("error_messages(off,unassigned)")
#elif \
     HEDLEY_TI_VERSION_CHECK(16,9,9) || \
     HEDLEY_TI_CL6X_VERSION_CHECK(8,0,0) || \
     HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
     HEDLEY_TI_CLPRU_VERSION_CHECK(2,3,2)
  #define SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_ _Pragma("diag_suppress 551")
#elif HEDLEY_INTEL_VERSION_CHECK(13,0,0)
  #define SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_ _Pragma("warning(disable:592)")
#elif HEDLEY_MSVC_VERSION_CHECK(19,0,0) && !defined(__MSVC_RUNTIME_CHECKS)
  #define SIMDE_DIAGNOSTIC_DISABLE_UNINITIALIZED_ __pragma(warning(disable:4700))
#endif

/* GCC emits a lot of "notes" about the ABI being different for things
 * in newer versions of GCC.  We don't really care because all our
 * functions are inlined and don't generate ABI. */
#if HEDLEY_GCC_VERSION_CHECK(7,0,0)
  #define SIMDE_DIAGNOSTIC_DISABLE_PSABI_ _Pragma("GCC diagnostic ignored \"-Wpsabi\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_PSABI_
#endif

/* Since MMX uses x87 FP registers, you're supposed to call _mm_empty()
 * after each MMX function before any floating point instructions.
 * Some compilers warn about functions which use MMX functions but
 * don't call _mm_empty().  However, since SIMDe is implementyng the
 * MMX API we shouldn't be calling _mm_empty(); we leave it to the
 * caller to invoke simde_mm_empty(). */
#if HEDLEY_INTEL_VERSION_CHECK(19,0,0)
  #define SIMDE_DIAGNOSTIC_DISABLE_NO_EMMS_INSTRUCTION_ _Pragma("warning(disable:13200 13203)")
#elif defined(HEDLEY_MSVC_VERSION)
  #define SIMDE_DIAGNOSTIC_DISABLE_NO_EMMS_INSTRUCTION_ __pragma(warning(disable:4799))
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_NO_EMMS_INSTRUCTION_
#endif

/* Intel is pushing people to use OpenMP SIMD instead of Cilk+, so they
 * emit a diagnostic if you use #pragma simd instead of
 * #pragma omp simd.  SIMDe supports OpenMP SIMD, you just need to
 * compile with -qopenmp or -qopenmp-simd and define
 * SIMDE_ENABLE_OPENMP.  Cilk+ is just a fallback. */
#if HEDLEY_INTEL_VERSION_CHECK(18,0,0)
  #define SIMDE_DIAGNOSTIC_DISABLE_SIMD_PRAGMA_DEPRECATED_ _Pragma("warning(disable:3948)")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_SIMD_PRAGMA_DEPRECATED_
#endif

/* MSVC emits a diagnostic when we call a function (like
 * simde_mm_set_epi32) while initializing a struct.  We currently do
 * this a *lot* in the tests. */
#if \
  defined(HEDLEY_MSVC_VERSION)
  #define SIMDE_DIAGNOSTIC_DISABLE_NON_CONSTANT_AGGREGATE_INITIALIZER_ __pragma(warning(disable:4204))
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_NON_CONSTANT_AGGREGATE_INITIALIZER_
#endif

/* This warning needs a lot of work.  It is triggered if all you do is
 * pass the value to memcpy/__builtin_memcpy, or if you initialize a
 * member of the union, even if that member takes up the entire union.
 * Last tested with clang-10, hopefully things will improve in the
 * future; if clang fixes this I'd love to enable it. */
#if \
  HEDLEY_HAS_WARNING("-Wconditional-uninitialized")
  #define SIMDE_DIAGNOSTIC_DISABLE_CONDITIONAL_UNINITIALIZED_ _Pragma("clang diagnostic ignored \"-Wconditional-uninitialized\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_CONDITIONAL_UNINITIALIZED_
#endif

/* This warning is meant to catch things like `0.3 + 0.4 == 0.7`, which
 * will is false.  However, SIMDe uses these operations exclusively
 * for things like _mm_cmpeq_ps, for which we really do want to check
 * for equality (or inequality).
 *
 * If someone wants to put together a SIMDE_FLOAT_EQUAL(a, op, b) macro
 * which just wraps a check in some code do disable this diagnostic I'd
 * be happy to accept it. */
#if \
  HEDLEY_HAS_WARNING("-Wfloat-equal") || \
  HEDLEY_GCC_VERSION_CHECK(3,0,0)
  #define SIMDE_DIAGNOSTIC_DISABLE_FLOAT_EQUAL_ _Pragma("GCC diagnostic ignored \"-Wfloat-equal\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_FLOAT_EQUAL_
#endif

/* This is because we use HEDLEY_STATIC_ASSERT for static assertions.
 * If Hedley can't find an implementation it will preprocess to
 * nothing, which means there will be a trailing semi-colon. */
#if HEDLEY_HAS_WARNING("-Wextra-semi")
  #define SIMDE_DIAGNOSTIC_DISABLE_EXTRA_SEMI_ _Pragma("clang diagnostic ignored \"-Wextra-semi\"")
#elif HEDLEY_GCC_VERSION_CHECK(8,1,0) && defined(__cplusplus)
  #define SIMDE_DIAGNOSTIC_DISABLE_EXTRA_SEMI_ _Pragma("GCC diagnostic ignored \"-Wextra-semi\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_EXTRA_SEMI_
#endif

/* We do use a few variadic macros, which technically aren't available
 * until C99 and C++11, but every compiler I'm aware of has supported
 * them for much longer.  That said, usage is isolated to the test
 * suite and compilers known to support them. */
#if HEDLEY_HAS_WARNING("-Wvariadic-macros") || HEDLEY_GCC_VERSION_CHECK(4,0,0)
  #if HEDLEY_HAS_WARNING("-Wc++98-compat-pedantic")
    #define SIMDE_DIAGNOSTIC_DISABLE_VARIADIC_MACROS_ \
      _Pragma("clang diagnostic ignored \"-Wvariadic-macros\"") \
      _Pragma("clang diagnostic ignored \"-Wc++98-compat-pedantic\"")
  #else
    #define SIMDE_DIAGNOSTIC_DISABLE_VARIADIC_MACROS_ _Pragma("GCC diagnostic ignored \"-Wvariadic-macros\"")
  #endif
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_VARIADIC_MACROS_
#endif

/* emscripten requires us to use a __wasm_unimplemented_simd128__ macro
 * before we can access certain SIMD intrinsics, but this diagnostic
 * warns about it being a reserved name.  It is a reserved name, but
 * it's reserved for the compiler and we are using it to convey
 * information to the compiler.
 *
 * This is also used when enabling native aliases since we don't get to
 * choose the macro names. */
#if HEDLEY_HAS_WARNING("-Wreserved-id-macro")
  #define SIMDE_DIAGNOSTIC_DISABLE_RESERVED_ID_MACRO_ _Pragma("clang diagnostic ignored \"-Wreserved-id-macro\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_RESERVED_ID_MACRO_
#endif

/* Similar to above; types like simde__m128i are reserved due to the
 * double underscore, but we didn't choose them, Intel did. */
#if HEDLEY_HAS_WARNING("-Wreserved-identifier")
  #define SIMDE_DIAGNOSTIC_DISABLE_RESERVED_ID_ _Pragma("clang diagnostic ignored \"-Wreserved-identifier\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_RESERVED_ID_
#endif

/* clang 3.8 warns about the packed attribute being unnecessary when
 * used in the _mm_loadu_* functions.  That *may* be true for version
 * 3.8, but for later versions it is crucial in order to make unaligned
 * access safe. */
#if HEDLEY_HAS_WARNING("-Wpacked")
  #define SIMDE_DIAGNOSTIC_DISABLE_PACKED_ _Pragma("clang diagnostic ignored \"-Wpacked\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_PACKED_
#endif

/* Triggered when assigning a float to a double implicitly.  We use
 * explicit casts in SIMDe, this is only used in the test suite. */
#if HEDLEY_HAS_WARNING("-Wdouble-promotion")
  #define SIMDE_DIAGNOSTIC_DISABLE_DOUBLE_PROMOTION_ _Pragma("clang diagnostic ignored \"-Wdouble-promotion\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_DOUBLE_PROMOTION_
#endif

/* Several compilers treat conformant array parameters as VLAs.  We
 * test to make sure we're in C mode (C++ doesn't support CAPs), and
 * that the version of the standard supports CAPs.  We also reject
 * some buggy compilers like MSVC (the logic is in Hedley if you want
 * to take a look), but with certain warnings enabled some compilers
 * still like to emit a diagnostic. */
#if HEDLEY_HAS_WARNING("-Wvla")
  #define SIMDE_DIAGNOSTIC_DISABLE_VLA_ _Pragma("clang diagnostic ignored \"-Wvla\"")
#elif HEDLEY_GCC_VERSION_CHECK(4,3,0)
  #define SIMDE_DIAGNOSTIC_DISABLE_VLA_ _Pragma("GCC diagnostic ignored \"-Wvla\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_VLA_
#endif

/* If you add an unused attribute to a function and don't use it, clang
 * may emit this. */
#if HEDLEY_HAS_WARNING("-Wused-but-marked-unused")
  #define SIMDE_DIAGNOSTIC_DISABLE_USED_BUT_MARKED_UNUSED_ _Pragma("clang diagnostic ignored \"-Wused-but-marked-unused\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_USED_BUT_MARKED_UNUSED_
#endif

#if HEDLEY_HAS_WARNING("-Wpass-failed")
  #define SIMDE_DIAGNOSTIC_DISABLE_PASS_FAILED_ _Pragma("clang diagnostic ignored \"-Wpass-failed\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_PASS_FAILED_
#endif

#if HEDLEY_HAS_WARNING("-Wpadded")
  #define SIMDE_DIAGNOSTIC_DISABLE_PADDED_ _Pragma("clang diagnostic ignored \"-Wpadded\"")
#elif HEDLEY_MSVC_VERSION_CHECK(19,0,0) /* Likely goes back further */
  #define SIMDE_DIAGNOSTIC_DISABLE_PADDED_ __pragma(warning(disable:4324))
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_PADDED_
#endif

#if HEDLEY_HAS_WARNING("-Wzero-as-null-pointer-constant")
  #define SIMDE_DIAGNOSTIC_DISABLE_ZERO_AS_NULL_POINTER_CONSTANT_ _Pragma("clang diagnostic ignored \"-Wzero-as-null-pointer-constant\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_ZERO_AS_NULL_POINTER_CONSTANT_
#endif

#if HEDLEY_HAS_WARNING("-Wold-style-cast")
  #define SIMDE_DIAGNOSTIC_DISABLE_OLD_STYLE_CAST_ _Pragma("clang diagnostic ignored \"-Wold-style-cast\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_OLD_STYLE_CAST_
#endif

#if HEDLEY_HAS_WARNING("-Wcast-function-type") || HEDLEY_GCC_VERSION_CHECK(8,0,0)
  #define SIMDE_DIAGNOSTIC_DISABLE_CAST_FUNCTION_TYPE_ _Pragma("GCC diagnostic ignored \"-Wcast-function-type\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_CAST_FUNCTION_TYPE_
#endif

/* clang will emit this warning when we use C99 extensions when not in
 * C99 mode, even though it does support this.  In such cases we check
 * the compiler and version first, so we know it's not a problem. */
#if HEDLEY_HAS_WARNING("-Wc99-extensions")
  #define SIMDE_DIAGNOSTIC_DISABLE_C99_EXTENSIONS_ _Pragma("clang diagnostic ignored \"-Wc99-extensions\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_C99_EXTENSIONS_
#endif

/* Similar problm as above; we rely on some basic C99 support, but clang
 * has started warning obut this even in C17 mode with -Weverything. */
#if HEDLEY_HAS_WARNING("-Wdeclaration-after-statement")
  #define SIMDE_DIAGNOSTIC_DISABLE_DECLARATION_AFTER_STATEMENT_ _Pragma("clang diagnostic ignored \"-Wdeclaration-after-statement\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_DECLARATION_AFTER_STATEMENT_
#endif

/* https://github.com/simd-everywhere/simde/issues/277 */
#if defined(HEDLEY_GCC_VERSION) && HEDLEY_GCC_VERSION_CHECK(4,6,0) && !HEDLEY_GCC_VERSION_CHECK(6,4,0) && defined(__cplusplus)
  #define SIMDE_DIAGNOSTIC_DISABLE_BUGGY_UNUSED_BUT_SET_VARIBALE_ _Pragma("GCC diagnostic ignored \"-Wunused-but-set-variable\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_BUGGY_UNUSED_BUT_SET_VARIBALE_
#endif

/* This is the warning that you normally define _CRT_SECURE_NO_WARNINGS
 * to silence, but you have to do that before including anything and
 * that would require reordering includes. */
#if defined(_MSC_VER)
  #define SIMDE_DIAGNOSTIC_DISABLE_ANNEX_K_ __pragma(warning(disable:4996))
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_ANNEX_K_
#endif

/* Some compilers, such as clang, may use `long long` for 64-bit
 * integers, but `long long` triggers a diagnostic with
 * -Wc++98-compat-pedantic which says 'long long' is incompatible with
 * C++98. */
#if HEDLEY_HAS_WARNING("-Wc++98-compat-pedantic")
  #if HEDLEY_HAS_WARNING("-Wc++11-long-long")
    #define SIMDE_DIAGNOSTIC_DISABLE_CPP98_COMPAT_PEDANTIC_ \
      _Pragma("clang diagnostic ignored \"-Wc++98-compat-pedantic\"") \
      _Pragma("clang diagnostic ignored \"-Wc++11-long-long\"")
  #else
    #define SIMDE_DIAGNOSTIC_DISABLE_CPP98_COMPAT_PEDANTIC_ _Pragma("clang diagnostic ignored \"-Wc++98-compat-pedantic\"")
  #endif
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_CPP98_COMPAT_PEDANTIC_
#endif

/* Some problem as above */
#if HEDLEY_HAS_WARNING("-Wc++11-long-long")
  #define SIMDE_DIAGNOSTIC_DISABLE_CPP11_LONG_LONG_ _Pragma("clang diagnostic ignored \"-Wc++11-long-long\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_CPP11_LONG_LONG_
#endif

/* emscripten emits this whenever stdin/stdout/stderr is used in a
 * macro. */
#if HEDLEY_HAS_WARNING("-Wdisabled-macro-expansion")
  #define SIMDE_DIAGNOSTIC_DISABLE_DISABLED_MACRO_EXPANSION_ _Pragma("clang diagnostic ignored \"-Wdisabled-macro-expansion\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_DISABLED_MACRO_EXPANSION_
#endif

/* Clang uses C11 generic selections to implement some AltiVec
 * functions, which triggers this diagnostic when not compiling
 * in C11 mode */
#if HEDLEY_HAS_WARNING("-Wc11-extensions")
  #define SIMDE_DIAGNOSTIC_DISABLE_C11_EXTENSIONS_ _Pragma("clang diagnostic ignored \"-Wc11-extensions\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_C11_EXTENSIONS_
#endif

/* Clang sometimes triggers this warning in macros in the AltiVec and
 * NEON headers, or due to missing functions. */
#if HEDLEY_HAS_WARNING("-Wvector-conversion")
  #define SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_ _Pragma("clang diagnostic ignored \"-Wvector-conversion\"")
  /* For NEON, the situation with -Wvector-conversion in clang < 10 is
   * bad enough that we just disable the warning altogether.  On x86,
   * clang has similar issues on several sse4.2+ intrinsics before 3.8. */
  #if \
      (defined(SIMDE_ARCH_ARM) && SIMDE_DETECT_CLANG_VERSION_NOT(10,0,0)) || \
      SIMDE_DETECT_CLANG_VERSION_NOT(3,8,0)
    #define SIMDE_DIAGNOSTIC_DISABLE_BUGGY_VECTOR_CONVERSION_ SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_
  #endif
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_
#endif
#if !defined(SIMDE_DIAGNOSTIC_DISABLE_BUGGY_VECTOR_CONVERSION_)
  #define SIMDE_DIAGNOSTIC_DISABLE_BUGGY_VECTOR_CONVERSION_
#endif

/* Prior to 5.0, clang didn't support disabling diagnostics in
 * statement exprs.  As a result, some macros we use don't
 * properly silence warnings. */
#if SIMDE_DETECT_CLANG_VERSION_NOT(5,0,0) && HEDLEY_HAS_WARNING("-Wcast-qual") && HEDLEY_HAS_WARNING("-Wcast-align")
  #define SIMDE_DIAGNOSTIC_DISABLE_BUGGY_CASTS_ _Pragma("clang diagnostic ignored \"-Wcast-qual\"") _Pragma("clang diagnostic ignored \"-Wcast-align\"")
#elif SIMDE_DETECT_CLANG_VERSION_NOT(5,0,0) && HEDLEY_HAS_WARNING("-Wcast-qual")
  #define SIMDE_DIAGNOSTIC_DISABLE_BUGGY_CASTS_ _Pragma("clang diagnostic ignored \"-Wcast-qual\"")
#elif SIMDE_DETECT_CLANG_VERSION_NOT(5,0,0) && HEDLEY_HAS_WARNING("-Wcast-align")
  #define SIMDE_DIAGNOSTIC_DISABLE_BUGGY_CASTS_ _Pragma("clang diagnostic ignored \"-Wcast-align\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_BUGGY_CASTS_
#endif

/* SLEEF triggers this a *lot* in their headers */
#if HEDLEY_HAS_WARNING("-Wignored-qualifiers")
  #define SIMDE_DIAGNOSTIC_DISABLE_IGNORED_QUALIFIERS_ _Pragma("clang diagnostic ignored \"-Wignored-qualifiers\"")
#elif HEDLEY_GCC_VERSION_CHECK(4,3,0)
  #define SIMDE_DIAGNOSTIC_DISABLE_IGNORED_QUALIFIERS_ _Pragma("GCC diagnostic ignored \"-Wignored-qualifiers\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_IGNORED_QUALIFIERS_
#endif

/* GCC emits this under some circumstances when using __int128 */
#if HEDLEY_GCC_VERSION_CHECK(4,8,0)
  #define SIMDE_DIAGNOSTIC_DISABLE_PEDANTIC_ _Pragma("GCC diagnostic ignored \"-Wpedantic\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_PEDANTIC_
#endif

/* MSVC doesn't like (__assume(0), code) and will warn about code being
 * unreachable, but we want it there because not all compilers
 * understand the unreachable macro and will complain if it is missing.
 * I'm planning on adding a new macro to Hedley to handle this a bit
 * more elegantly, but until then... */
#if defined(HEDLEY_MSVC_VERSION)
  #define SIMDE_DIAGNOSTIC_DISABLE_UNREACHABLE_ __pragma(warning(disable:4702))
#elif defined(__clang__)
  #define SIMDE_DIAGNOSTIC_DISABLE_UNREACHABLE_ HEDLEY_PRAGMA(clang diagnostic ignored "-Wunreachable-code")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_UNREACHABLE_
#endif

/* This is a false positive from GCC in a few places. */
#if HEDLEY_GCC_VERSION_CHECK(4,7,0)
  #define SIMDE_DIAGNOSTIC_DISABLE_MAYBE_UNINITIAZILED_ _Pragma("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
#else
  #define SIMDE_DIAGNOSTIC_DISABLE_MAYBE_UNINITIAZILED_
#endif

#if defined(SIMDE_ENABLE_NATIVE_ALIASES)
  #define SIMDE_DISABLE_UNWANTED_DIAGNOSTICS_NATIVE_ALIASES_ \
    SIMDE_DIAGNOSTIC_DISABLE_RESERVED_ID_MACRO_
#else
  #define SIMDE_DISABLE_UNWANTED_DIAGNOSTICS_NATIVE_ALIASES_
#endif

/* Some native functions on E2K with instruction set < v6 are declared
 * as deprecated due to inefficiency. Still they are more efficient
 * than SIMDe implementation. So we're using them, and switching off
 * these deprecation warnings. */
#if defined(HEDLEY_MCST_LCC_VERSION)
#  define SIMDE_LCC_DISABLE_DEPRECATED_WARNINGS _Pragma("diag_suppress 1215,1444")
#  define SIMDE_LCC_REVERT_DEPRECATED_WARNINGS _Pragma("diag_default 1215,1444")
#else
#  define SIMDE_LCC_DISABLE_DEPRECATED_WARNINGS
#  define SIMDE_LCC_REVERT_DEPRECATED_WARNINGS
#endif

#define SIMDE_DISABLE_UNWANTED_DIAGNOSTICS \
  HEDLEY_DIAGNOSTIC_DISABLE_UNUSED_FUNCTION \
  SIMDE_DISABLE_UNWANTED_DIAGNOSTICS_NATIVE_ALIASES_ \
  SIMDE_DIAGNOSTIC_DISABLE_PSABI_ \
  SIMDE_DIAGNOSTIC_DISABLE_NO_EMMS_INSTRUCTION_ \
  SIMDE_DIAGNOSTIC_DISABLE_SIMD_PRAGMA_DEPRECATED_ \
  SIMDE_DIAGNOSTIC_DISABLE_CONDITIONAL_UNINITIALIZED_ \
  SIMDE_DIAGNOSTIC_DISABLE_DECLARATION_AFTER_STATEMENT_ \
  SIMDE_DIAGNOSTIC_DISABLE_FLOAT_EQUAL_ \
  SIMDE_DIAGNOSTIC_DISABLE_NON_CONSTANT_AGGREGATE_INITIALIZER_ \
  SIMDE_DIAGNOSTIC_DISABLE_EXTRA_SEMI_ \
  SIMDE_DIAGNOSTIC_DISABLE_VLA_ \
  SIMDE_DIAGNOSTIC_DISABLE_USED_BUT_MARKED_UNUSED_ \
  SIMDE_DIAGNOSTIC_DISABLE_PASS_FAILED_ \
  SIMDE_DIAGNOSTIC_DISABLE_CPP98_COMPAT_PEDANTIC_ \
  SIMDE_DIAGNOSTIC_DISABLE_CPP11_LONG_LONG_ \
  SIMDE_DIAGNOSTIC_DISABLE_BUGGY_UNUSED_BUT_SET_VARIBALE_ \
  SIMDE_DIAGNOSTIC_DISABLE_BUGGY_CASTS_ \
  SIMDE_DIAGNOSTIC_DISABLE_BUGGY_VECTOR_CONVERSION_ \
  SIMDE_DIAGNOSTIC_DISABLE_RESERVED_ID_

#endif /* !defined(SIMDE_DIAGNOSTIC_H) */
/* :: End simde/simde-diagnostic.h :: */

#if !defined(SIMDE_X86_SVML_NATIVE) && !defined(SIMDE_X86_SVML_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_SVML)
    #define SIMDE_X86_SVML_NATIVE
  #endif
#endif

#if !defined(SIMDE_X86_AVX512VP2INTERSECT_NATIVE) && !defined(SIMDE_X86_AVX512VP2INTERSECT_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_AVX512VP2INTERSECT)
    #define SIMDE_X86_AVX512VP2INTERSECT_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_AVX512VP2INTERSECT_NATIVE) && !defined(SIMDE_X86_AVX512F_NATIVE)
  #define SIMDE_X86_AVX512F_NATIVE
#endif

#if !defined(SIMDE_X86_AVX512VPOPCNTDQ_NATIVE) && !defined(SIMDE_X86_AVX512VPOPCNTDQ_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_AVX512VPOPCNTDQ)
    #define SIMDE_X86_AVX512VPOPCNTDQ_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_AVX512VPOPCNTDQ_NATIVE) && !defined(SIMDE_X86_AVX512F_NATIVE)
  #define SIMDE_X86_AVX512F_NATIVE
#endif

#if !defined(SIMDE_X86_AVX512BITALG_NATIVE) && !defined(SIMDE_X86_AVX512BITALG_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_AVX512BITALG)
    #define SIMDE_X86_AVX512BITALG_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_AVX512BITALG_NATIVE) && !defined(SIMDE_X86_AVX512F_NATIVE)
  #define SIMDE_X86_AVX512F_NATIVE
#endif

#if !defined(SIMDE_X86_AVX512VBMI_NATIVE) && !defined(SIMDE_X86_AVX512VBMI_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_AVX512VBMI)
    #define SIMDE_X86_AVX512VBMI_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_AVX512VBMI_NATIVE) && !defined(SIMDE_X86_AVX512F_NATIVE)
  #define SIMDE_X86_AVX512F_NATIVE
#endif

#if !defined(SIMDE_X86_AVX512VBMI2_NATIVE) && !defined(SIMDE_X86_AVX512VBMI2_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_AVX512VBMI2)
    #define SIMDE_X86_AVX512VBMI2_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_AVX512VBMI2_NATIVE) && !defined(SIMDE_X86_AVX512F_NATIVE)
  #define SIMDE_X86_AVX512F_NATIVE
#endif

#if !defined(SIMDE_X86_AVX512VNNI_NATIVE) && !defined(SIMDE_X86_AVX512VNNI_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_AVX512VNNI)
    #define SIMDE_X86_AVX512VNNI_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_AVX512VNNI_NATIVE) && !defined(SIMDE_X86_AVX512F_NATIVE)
  #define SIMDE_X86_AVX512F_NATIVE
#endif

#if !defined(SIMDE_X86_AVX5124VNNIW_NATIVE) && !defined(SIMDE_X86_AVX5124VNNIW_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_AVX5124VNNIW)
    #define SIMDE_X86_AVX5124VNNIW_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_AVX5124VNNIW_NATIVE) && !defined(SIMDE_X86_AVX512F_NATIVE)
  #define SIMDE_X86_AVX512F_NATIVE
#endif

#if !defined(SIMDE_X86_AVX512CD_NATIVE) && !defined(SIMDE_X86_AVX512CD_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_AVX512CD)
    #define SIMDE_X86_AVX512CD_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_AVX512CD_NATIVE) && !defined(SIMDE_X86_AVX512F_NATIVE)
  #define SIMDE_X86_AVX512F_NATIVE
#endif

#if !defined(SIMDE_X86_AVX512DQ_NATIVE) && !defined(SIMDE_X86_AVX512DQ_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_AVX512DQ)
    #define SIMDE_X86_AVX512DQ_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_AVX512DQ_NATIVE) && !defined(SIMDE_X86_AVX512F_NATIVE)
  #define SIMDE_X86_AVX512F_NATIVE
#endif

#if !defined(SIMDE_X86_AVX512VL_NATIVE) && !defined(SIMDE_X86_AVX512VL_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_AVX512VL)
    #define SIMDE_X86_AVX512VL_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_AVX512VL_NATIVE) && !defined(SIMDE_X86_AVX512F_NATIVE)
  #define SIMDE_X86_AVX512F_NATIVE
#endif

#if !defined(SIMDE_X86_AVX512BW_NATIVE) && !defined(SIMDE_X86_AVX512BW_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_AVX512BW)
    #define SIMDE_X86_AVX512BW_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_AVX512BW_NATIVE) && !defined(SIMDE_X86_AVX512F_NATIVE)
  #define SIMDE_X86_AVX512F_NATIVE
#endif

#if !defined(SIMDE_X86_AVX512FP16_NATIVE) && !defined(SIMDE_X86_AVX512FP16_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_AVX512FP16)
    #define SIMDE_X86_AVX512FP16_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_AVX512BW_NATIVE) && !defined(SIMDE_X86_AVX512F_NATIVE)
  #define SIMDE_X86_AVX512F_NATIVE
#endif

#if !defined(SIMDE_X86_AVX512BF16_NATIVE) && !defined(SIMDE_X86_AVX512BF16_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_AVX512BF16)
    #define SIMDE_X86_AVX512BF16_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_AVX512BF16_NATIVE) && !defined(SIMDE_X86_AVX512F_NATIVE)
  #define SIMDE_X86_AVX512F_NATIVE
#endif

#if !defined(SIMDE_X86_AVX512F_NATIVE) && !defined(SIMDE_X86_AVX512F_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_AVX512F)
    #define SIMDE_X86_AVX512F_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_AVX512F_NATIVE) && !defined(SIMDE_X86_AVX2_NATIVE)
  #define SIMDE_X86_AVX2_NATIVE
#endif

#if !defined(SIMDE_X86_FMA_NATIVE) && !defined(SIMDE_X86_FMA_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_FMA)
    #define SIMDE_X86_FMA_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_FMA_NATIVE) && !defined(SIMDE_X86_AVX_NATIVE)
  #define SIMDE_X86_AVX_NATIVE
#endif

#if !defined(SIMDE_X86_AVX2_NATIVE) && !defined(SIMDE_X86_AVX2_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_AVX2)
    #define SIMDE_X86_AVX2_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_AVX2_NATIVE) && !defined(SIMDE_X86_AVX_NATIVE)
  #define SIMDE_X86_AVX_NATIVE
#endif

#if !defined(SIMDE_X86_AVX_NATIVE) && !defined(SIMDE_X86_AVX_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_AVX)
    #define SIMDE_X86_AVX_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_AVX_NATIVE) && !defined(SIMDE_X86_SSE4_2_NATIVE)
  #define SIMDE_X86_SSE4_2_NATIVE
#endif

#if !defined(SIMDE_X86_XOP_NATIVE) && !defined(SIMDE_X86_XOP_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_XOP)
    #define SIMDE_X86_XOP_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_XOP_NATIVE) && !defined(SIMDE_X86_SSE4_2_NATIVE)
  #define SIMDE_X86_SSE4_2_NATIVE
#endif

#if !defined(SIMDE_X86_SSE4_2_NATIVE) && !defined(SIMDE_X86_SSE4_2_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_SSE4_2)
    #define SIMDE_X86_SSE4_2_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_SSE4_2_NATIVE) && !defined(SIMDE_X86_SSE4_1_NATIVE)
  #define SIMDE_X86_SSE4_1_NATIVE
#endif

#if !defined(SIMDE_X86_SSE4_1_NATIVE) && !defined(SIMDE_X86_SSE4_1_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_SSE4_1)
    #define SIMDE_X86_SSE4_1_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_SSE4_1_NATIVE) && !defined(SIMDE_X86_SSSE3_NATIVE)
  #define SIMDE_X86_SSSE3_NATIVE
#endif

#if !defined(SIMDE_X86_SSSE3_NATIVE) && !defined(SIMDE_X86_SSSE3_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_SSSE3)
    #define SIMDE_X86_SSSE3_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_SSSE3_NATIVE) && !defined(SIMDE_X86_SSE3_NATIVE)
  #define SIMDE_X86_SSE3_NATIVE
#endif

#if !defined(SIMDE_X86_SSE3_NATIVE) && !defined(SIMDE_X86_SSE3_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_SSE3)
    #define SIMDE_X86_SSE3_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_SSE3_NATIVE) && !defined(SIMDE_X86_SSE2_NATIVE)
  #define SIMDE_X86_SSE2_NATIVE
#endif

#if !defined(SIMDE_X86_AES_NATIVE) && !defined(SIMDE_X86_AES_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_AES)
    #define SIMDE_X86_AES_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_AES_NATIVE) && !defined(SIMDE_X86_SSE2_NATIVE)
  #define SIMDE_X86_SSE2_NATIVE
#endif

#if !defined(SIMDE_X86_SSE2_NATIVE) && !defined(SIMDE_X86_SSE2_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_SSE2)
    #define SIMDE_X86_SSE2_NATIVE
  #endif
#endif
#if defined(SIMDE_X86_SSE2_NATIVE) && !defined(SIMDE_X86_SSE_NATIVE)
  #define SIMDE_X86_SSE_NATIVE
#endif

#if !defined(SIMDE_X86_SSE_NATIVE) && !defined(SIMDE_X86_SSE_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_SSE)
    #define SIMDE_X86_SSE_NATIVE
  #endif
#endif

#if !defined(SIMDE_X86_MMX_NATIVE) && !defined(SIMDE_X86_MMX_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_MMX)
    #define SIMDE_X86_MMX_NATIVE
  #endif
#endif

#if !defined(SIMDE_X86_GFNI_NATIVE) && !defined(SIMDE_X86_GFNI_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_GFNI)
    #define SIMDE_X86_GFNI_NATIVE
  #endif
#endif

#if !defined(SIMDE_X86_PCLMUL_NATIVE) && !defined(SIMDE_X86_PCLMUL_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_PCLMUL)
    #define SIMDE_X86_PCLMUL_NATIVE
  #endif
#endif

#if !defined(SIMDE_X86_VPCLMULQDQ_NATIVE) && !defined(SIMDE_X86_VPCLMULQDQ_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_VPCLMULQDQ)
    #define SIMDE_X86_VPCLMULQDQ_NATIVE
  #endif
#endif

#if !defined(SIMDE_X86_F16C_NATIVE) && !defined(SIMDE_X86_F16C_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86_F16C)
    #define SIMDE_X86_F16C_NATIVE
  #endif
#endif

#if !defined(SIMDE_X86_SVML_NATIVE) && !defined(SIMDE_X86_SVML_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_X86) && (defined(__INTEL_COMPILER) || (HEDLEY_MSVC_VERSION_CHECK(14, 20, 0) && !defined(__clang__)))
    #define SIMDE_X86_SVML_NATIVE
  #endif
#endif

#if defined(HEDLEY_MSVC_VERSION)
  #pragma warning(push)
  #pragma warning(disable:4799)
#endif

#if \
    defined(SIMDE_X86_AVX_NATIVE) || defined(SIMDE_X86_GFNI_NATIVE) || defined(SIMDE_X86_SVML_NATIVE)
  #include <immintrin.h>
#elif defined(SIMDE_X86_SSE4_2_NATIVE)
  #include <nmmintrin.h>
#elif defined(SIMDE_X86_SSE4_1_NATIVE)
  #include <smmintrin.h>
#elif defined(SIMDE_X86_SSSE3_NATIVE)
  #include <tmmintrin.h>
#elif defined(SIMDE_X86_SSE3_NATIVE)
  #include <pmmintrin.h>
#elif defined(SIMDE_X86_SSE2_NATIVE)
  #include <emmintrin.h>
#elif defined(SIMDE_X86_SSE_NATIVE)
  #include <xmmintrin.h>
#elif defined(SIMDE_X86_MMX_NATIVE)
  #include <mmintrin.h>
#endif

#if defined(SIMDE_X86_XOP_NATIVE)
  #if defined(_MSC_VER)
    #include <intrin.h>
  #else
    #include <x86intrin.h>
  #endif
#endif

#if defined(SIMDE_X86_AES_NATIVE)
  #include <wmmintrin.h>
#endif

#if defined(HEDLEY_MSVC_VERSION)
  #pragma warning(pop)
#endif

#if !defined(SIMDE_ARM_NEON_A64V8_NATIVE) && !defined(SIMDE_ARM_NEON_A64V8_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_ARM_NEON) && defined(SIMDE_ARCH_AARCH64) && SIMDE_ARCH_ARM_CHECK(8,0)
    #define SIMDE_ARM_NEON_A64V8_NATIVE
  #endif
#endif
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE) && !defined(SIMDE_ARM_NEON_A32V8_NATIVE)
  #define SIMDE_ARM_NEON_A32V8_NATIVE
#endif

#if !defined(SIMDE_ARM_NEON_A32V8_NATIVE) && !defined(SIMDE_ARM_NEON_A32V8_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_ARM_NEON) && SIMDE_ARCH_ARM_CHECK(8,0) && (__ARM_NEON_FP & 0x02)
    #define SIMDE_ARM_NEON_A32V8_NATIVE
  #endif
#endif
#if defined(__ARM_ACLE)
  #include <arm_acle.h>
#endif
#if defined(SIMDE_ARM_NEON_A32V8_NATIVE) && !defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define SIMDE_ARM_NEON_A32V7_NATIVE
#endif

#if !defined(SIMDE_ARM_NEON_A32V7_NATIVE) && !defined(SIMDE_ARM_NEON_A32V7_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_ARM_NEON) && SIMDE_ARCH_ARM_CHECK(7,0)
    #define SIMDE_ARM_NEON_A32V7_NATIVE
  #endif
#endif
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #include <arm_neon.h>
  #if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
    #include <arm_fp16.h>
  #endif
#endif

#if !defined(SIMDE_ARM_SVE_NATIVE) && !defined(SIMDE_ARM_SVE_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_ARM_SVE)
    #define SIMDE_ARM_SVE_NATIVE
    #include <arm_sve.h>
  #endif
#endif

#if !defined(SIMDE_RISCV_V_NATIVE) && !defined(SIMDE_RISCV_V_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_RISCV_V)
    #define SIMDE_RISCV_V_NATIVE
  #endif
#endif
#if defined(SIMDE_RISCV_V_NATIVE)
  #include <riscv_vector.h>
#endif

#if !defined(SIMDE_WASM_SIMD128_NATIVE) && !defined(SIMDE_WASM_SIMD128_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_WASM_SIMD128)
    #define SIMDE_WASM_SIMD128_NATIVE
  #endif
#endif

#if !defined(SIMDE_WASM_RELAXED_SIMD_NATIVE) && !defined(SIMDE_WASM_RELAXED_SIMD_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_WASM_RELAXED_SIMD)
    #define SIMDE_WASM_RELAXED_SIMD_NATIVE
  #endif
#endif
#if defined(SIMDE_WASM_SIMD128_NATIVE) || defined(SIMDE_WASM_RELAXED_SIMD_NATIVE)
  #include <wasm_simd128.h>
#endif

#if !defined(SIMDE_POWER_ALTIVEC_P9_NATIVE) && !defined(SIMDE_POWER_ALTIVEC_P9_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if SIMDE_ARCH_POWER_ALTIVEC_CHECK(900)
    #define SIMDE_POWER_ALTIVEC_P9_NATIVE
  #endif
#endif
#if defined(SIMDE_POWER_ALTIVEC_P9_NATIVE) && !defined(SIMDE_POWER_ALTIVEC_P8)
  #define SIMDE_POWER_ALTIVEC_P8_NATIVE
#endif

#if !defined(SIMDE_POWER_ALTIVEC_P8_NATIVE) && !defined(SIMDE_POWER_ALTIVEC_P8_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if SIMDE_ARCH_POWER_ALTIVEC_CHECK(800)
    #define SIMDE_POWER_ALTIVEC_P8_NATIVE
  #endif
#endif
#if defined(SIMDE_POWER_ALTIVEC_P8_NATIVE) && !defined(SIMDE_POWER_ALTIVEC_P7)
  #define SIMDE_POWER_ALTIVEC_P7_NATIVE
#endif

#if !defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) && !defined(SIMDE_POWER_ALTIVEC_P7_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if SIMDE_ARCH_POWER_ALTIVEC_CHECK(700)
    #define SIMDE_POWER_ALTIVEC_P7_NATIVE
  #endif
#endif
#if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) && !defined(SIMDE_POWER_ALTIVEC_P6)
  #define SIMDE_POWER_ALTIVEC_P6_NATIVE
#endif

#if !defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) && !defined(SIMDE_POWER_ALTIVEC_P6_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if SIMDE_ARCH_POWER_ALTIVEC_CHECK(600)
    #define SIMDE_POWER_ALTIVEC_P6_NATIVE
  #endif
#endif
#if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) && !defined(SIMDE_POWER_ALTIVEC_P5)
  #define SIMDE_POWER_ALTIVEC_P5_NATIVE
#endif

#if !defined(SIMDE_POWER_ALTIVEC_P5_NATIVE) && !defined(SIMDE_POWER_ALTIVEC_P5_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if SIMDE_ARCH_POWER_ALTIVEC_CHECK(500)
    #define SIMDE_POWER_ALTIVEC_P5_NATIVE
  #endif
#endif

#if !defined(SIMDE_ZARCH_ZVECTOR_15_NATIVE) && !defined(SIMDE_ZARCH_ZVECTOR_15_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if SIMDE_ARCH_ZARCH_CHECK(13) && defined(SIMDE_ARCH_ZARCH_ZVECTOR)
    #define SIMDE_ZARCH_ZVECTOR_15_NATIVE
  #endif
#endif

#if !defined(SIMDE_ZARCH_ZVECTOR_14_NATIVE) && !defined(SIMDE_ZARCH_ZVECTOR_14_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if SIMDE_ARCH_ZARCH_CHECK(12) && defined(SIMDE_ARCH_ZARCH_ZVECTOR)
    #define SIMDE_ZARCH_ZVECTOR_14_NATIVE
  #endif
#endif

#if !defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE) && !defined(SIMDE_ZARCH_ZVECTOR_13_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if SIMDE_ARCH_ZARCH_CHECK(11) && defined(SIMDE_ARCH_ZARCH_ZVECTOR)
    #define SIMDE_ZARCH_ZVECTOR_13_NATIVE
  #endif
#endif

#if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
  /* AltiVec conflicts with lots of stuff.  The bool keyword conflicts
   * with the bool keyword in C++ and the bool macro in C99+ (defined
   * in stdbool.h).  The vector keyword conflicts with std::vector in
   * C++ if you are `using std;`.
   *
   * Luckily AltiVec allows you to use `__vector`/`__bool`/`__pixel`
   * instead, but altivec.h will unconditionally define
   * `vector`/`bool`/`pixel` so we need to work around that.
   *
   * Unfortunately this means that if your code uses AltiVec directly
   * it may break.  If this is the case you'll want to define
   * `SIMDE_POWER_ALTIVEC_NO_UNDEF` before including SIMDe.  Or, even
   * better, port your code to use the double-underscore versions. */
  #if defined(bool)
    #undef bool
  #endif

  #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    #include <altivec.h>

    #if !defined(SIMDE_POWER_ALTIVEC_NO_UNDEF)
      #if defined(vector)
        #undef vector
      #endif
      #if defined(pixel)
        #undef pixel
      #endif
      #if defined(bool)
        #undef bool
      #endif
    #endif /* !defined(SIMDE_POWER_ALTIVEC_NO_UNDEF) */
  #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    #include <vecintrin.h>
  #endif

  /* Use these intsead of vector/pixel/bool in SIMDe. */
  #define SIMDE_POWER_ALTIVEC_VECTOR(T) __vector T
  #define SIMDE_POWER_ALTIVEC_PIXEL __pixel
  #define SIMDE_POWER_ALTIVEC_BOOL __bool

  /* Re-define bool if we're using stdbool.h */
  #if !defined(__cplusplus) && defined(__bool_true_false_are_defined) && !defined(SIMDE_POWER_ALTIVEC_NO_UNDEF)
    #define bool _Bool
  #endif
#endif

#if !defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE) && !defined(SIMDE_MIPS_LOONGSON_MMI_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_MIPS_LOONGSON_MMI)
    #define SIMDE_MIPS_LOONGSON_MMI_NATIVE  1
  #endif
#endif
#if defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
  #include <loongson-mmiintrin.h>
#endif

#if !defined(SIMDE_MIPS_MSA_NATIVE) && !defined(SIMDE_MIPS_MSA_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_MIPS_MSA)
    #define SIMDE_MIPS_MSA_NATIVE  1
  #endif
#endif
#if defined(SIMDE_MIPS_MSA_NATIVE)
  #include <msa.h>
#endif

/* This is used to determine whether or not to fall back on a vector
 * function in an earlier ISA extensions, as well as whether
 * we expected any attempts at vectorization to be fruitful or if we
 * expect to always be running serial code.
 *
 * Note that, for some architectures (okay, *one* architecture) there
 * can be a split where some types are supported for one vector length
 * but others only for a shorter length.  Therefore, it is possible to
 * provide separate values for float/int/double types. */

#if !defined(SIMDE_NATURAL_VECTOR_SIZE)
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    #define SIMDE_NATURAL_VECTOR_SIZE (512)
  #elif defined(SIMDE_X86_AVX2_NATIVE)
    #define SIMDE_NATURAL_VECTOR_SIZE (256)
  #elif defined(SIMDE_X86_AVX_NATIVE)
    #define SIMDE_NATURAL_FLOAT_VECTOR_SIZE (256)
    #define SIMDE_NATURAL_INT_VECTOR_SIZE (128)
    #define SIMDE_NATURAL_DOUBLE_VECTOR_SIZE (128)
  #elif \
      defined(SIMDE_X86_SSE2_NATIVE) || \
      defined(SIMDE_ARM_NEON_A32V7_NATIVE) || \
      defined(SIMDE_WASM_SIMD128_NATIVE) || \
      defined(SIMDE_POWER_ALTIVEC_P5_NATIVE) || \
      defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE) || \
      defined(SIMDE_MIPS_MSA_NATIVE)
    #define SIMDE_NATURAL_VECTOR_SIZE (128)
  #elif defined(SIMDE_X86_SSE_NATIVE)
    #define SIMDE_NATURAL_FLOAT_VECTOR_SIZE (128)
    #define SIMDE_NATURAL_INT_VECTOR_SIZE (64)
    #define SIMDE_NATURAL_DOUBLE_VECTOR_SIZE (0)
  #elif defined(SIMDE_RISCV_V_NATIVE) && defined(__riscv_v_fixed_vlen)
        //FIXME : SIMDE_NATURAL_VECTOR_SIZE == __riscv_v_fixed_vlen
        #define SIMDE_NATURAL_VECTOR_SIZE (128)
  #endif

  #if !defined(SIMDE_NATURAL_VECTOR_SIZE)
    #if defined(SIMDE_NATURAL_FLOAT_VECTOR_SIZE)
      #define SIMDE_NATURAL_VECTOR_SIZE SIMDE_NATURAL_FLOAT_VECTOR_SIZE
    #elif defined(SIMDE_NATURAL_INT_VECTOR_SIZE)
      #define SIMDE_NATURAL_VECTOR_SIZE SIMDE_NATURAL_INT_VECTOR_SIZE
    #elif defined(SIMDE_NATURAL_DOUBLE_VECTOR_SIZE)
      #define SIMDE_NATURAL_VECTOR_SIZE SIMDE_NATURAL_DOUBLE_VECTOR_SIZE
    #else
      #define SIMDE_NATURAL_VECTOR_SIZE (0)
    #endif
  #endif

  #if !defined(SIMDE_NATURAL_FLOAT_VECTOR_SIZE)
    #define SIMDE_NATURAL_FLOAT_VECTOR_SIZE SIMDE_NATURAL_VECTOR_SIZE
  #endif
  #if !defined(SIMDE_NATURAL_INT_VECTOR_SIZE)
    #define SIMDE_NATURAL_INT_VECTOR_SIZE SIMDE_NATURAL_VECTOR_SIZE
  #endif
  #if !defined(SIMDE_NATURAL_DOUBLE_VECTOR_SIZE)
    #define SIMDE_NATURAL_DOUBLE_VECTOR_SIZE SIMDE_NATURAL_VECTOR_SIZE
  #endif
#endif

#define SIMDE_NATURAL_VECTOR_SIZE_LE(x) ((SIMDE_NATURAL_VECTOR_SIZE > 0) && (SIMDE_NATURAL_VECTOR_SIZE <= (x)))
#define SIMDE_NATURAL_VECTOR_SIZE_GE(x) ((SIMDE_NATURAL_VECTOR_SIZE > 0) && (SIMDE_NATURAL_VECTOR_SIZE >= (x)))
#define SIMDE_NATURAL_FLOAT_VECTOR_SIZE_LE(x) ((SIMDE_NATURAL_FLOAT_VECTOR_SIZE > 0) && (SIMDE_NATURAL_FLOAT_VECTOR_SIZE <= (x)))
#define SIMDE_NATURAL_FLOAT_VECTOR_SIZE_GE(x) ((SIMDE_NATURAL_FLOAT_VECTOR_SIZE > 0) && (SIMDE_NATURAL_FLOAT_VECTOR_SIZE >= (x)))
#define SIMDE_NATURAL_INT_VECTOR_SIZE_LE(x) ((SIMDE_NATURAL_INT_VECTOR_SIZE > 0) && (SIMDE_NATURAL_INT_VECTOR_SIZE <= (x)))
#define SIMDE_NATURAL_INT_VECTOR_SIZE_GE(x) ((SIMDE_NATURAL_INT_VECTOR_SIZE > 0) && (SIMDE_NATURAL_INT_VECTOR_SIZE >= (x)))
#define SIMDE_NATURAL_DOUBLE_VECTOR_SIZE_LE(x) ((SIMDE_NATURAL_DOUBLE_VECTOR_SIZE > 0) && (SIMDE_NATURAL_DOUBLE_VECTOR_SIZE <= (x)))
#define SIMDE_NATURAL_DOUBLE_VECTOR_SIZE_GE(x) ((SIMDE_NATURAL_DOUBLE_VECTOR_SIZE > 0) && (SIMDE_NATURAL_DOUBLE_VECTOR_SIZE >= (x)))

/* Native aliases */
#if defined(SIMDE_ENABLE_NATIVE_ALIASES)
  #if !defined(SIMDE_X86_MMX_NATIVE)
    #define SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_SSE_NATIVE)
    #define SIMDE_X86_SSE_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_SSE2_NATIVE)
    #define SIMDE_X86_SSE2_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_SSE3_NATIVE)
    #define SIMDE_X86_SSE3_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_SSSE3_NATIVE)
    #define SIMDE_X86_SSSE3_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_SSE4_1_NATIVE)
    #define SIMDE_X86_SSE4_1_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_SSE4_2_NATIVE)
    #define SIMDE_X86_SSE4_2_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_AVX_NATIVE)
    #define SIMDE_X86_AVX_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_AVX2_NATIVE)
    #define SIMDE_X86_AVX2_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_FMA_NATIVE)
    #define SIMDE_X86_FMA_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_AVX512F_NATIVE)
    #define SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_AVX512VL_NATIVE)
    #define SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_AVX512VBMI_NATIVE)
    #define SIMDE_X86_AVX512VBMI_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_AVX512VBMI2_NATIVE)
    #define SIMDE_X86_AVX512VBMI2_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_AVX512BW_NATIVE)
    #define SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_AVX512VNNI_NATIVE)
    #define SIMDE_X86_AVX512VNNI_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_AVX5124VNNIW_NATIVE)
    #define SIMDE_X86_AVX5124VNNIW_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_AVX512BF16_NATIVE)
    #define SIMDE_X86_AVX512BF16_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_AVX512BITALG_NATIVE)
    #define SIMDE_X86_AVX512BITALG_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_AVX512VPOPCNTDQ_NATIVE)
    #define SIMDE_X86_AVX512VPOPCNTDQ_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_AVX512VP2INTERSECT_NATIVE)
    #define SIMDE_X86_AVX512VP2INTERSECT_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_AVX512DQ_NATIVE)
    #define SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_AVX512CD_NATIVE)
    #define SIMDE_X86_AVX512CD_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_AVX512FP16_NATIVE)
    #define SIMDE_X86_AVX512FP16_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_GFNI_NATIVE)
    #define SIMDE_X86_GFNI_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_PCLMUL_NATIVE)
    #define SIMDE_X86_PCLMUL_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_VPCLMULQDQ_NATIVE)
    #define SIMDE_X86_VPCLMULQDQ_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_F16C_NATIVE)
    #define SIMDE_X86_F16C_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_AES_NATIVE)
    #define SIMDE_X86_AES_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_X86_SVML_NATIVE)
    #define SIMDE_X86_SVML_ENABLE_NATIVE_ALIASES
  #endif

  #if !defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    #define SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_ARM_NEON_A32V8_NATIVE)
    #define SIMDE_ARM_NEON_A32V8_ENABLE_NATIVE_ALIASES
  #endif
  #if !defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    #define SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES
  #endif

  #if !defined(SIMDE_ARM_SVE_NATIVE)
    #define SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES
  #endif

  #if !defined(SIMDE_RISCV_V_NATIVE)
    #define SIMDE_RISCV_V_ENABLE_NATIVE_ALIASES
  #endif

  #if !defined(SIMDE_MIPS_MSA_NATIVE)
    #define SIMDE_MIPS_MSA_ENABLE_NATIVE_ALIASES
  #endif

  #if !defined(SIMDE_WASM_SIMD128_NATIVE)
    #define SIMDE_WASM_SIMD128_ENABLE_NATIVE_ALIASES
  #endif
#endif

/* Are floating point values stored using IEEE 754?  Knowing
 * this at during preprocessing is a bit tricky, mostly because what
 * we're curious about is how values are stored and not whether the
 * implementation is fully conformant in terms of rounding, NaN
 * handling, etc.
 *
 * For example, if you use -ffast-math or -Ofast on
 * GCC or clang IEEE 754 isn't strictly followed, therefore IEE 754
 * support is not advertised (by defining __STDC_IEC_559__).
 *
 * However, what we care about is whether it is safe to assume that
 * floating point values are stored in IEEE 754 format, in which case
 * we can provide faster implementations of some functions.
 *
 * Luckily every vaugely modern architecture I'm aware of uses IEEE 754-
 * so we just assume IEEE 754 for now.  There is a test which verifies
 * this, if that test fails sowewhere please let us know and we'll add
 * an exception for that platform.  Meanwhile, you can define
 * SIMDE_NO_IEEE754_STORAGE. */
#if !defined(SIMDE_IEEE754_STORAGE) && !defined(SIMDE_NO_IEE754_STORAGE)
  #define SIMDE_IEEE754_STORAGE
#endif

#if defined(SIMDE_ARCH_ARM_NEON_FP16)
  #define SIMDE_ARM_NEON_FP16
#endif

#if defined(SIMDE_ARCH_ARM_NEON_BF16)
  #define SIMDE_ARM_NEON_BF16
#endif

#if !defined(SIMDE_LOONGARCH_LASX_NATIVE) && !defined(SIMDE_LOONGARCH_LASX_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_LOONGARCH_LASX)
    #define SIMDE_LOONGARCH_LASX_NATIVE
  #endif
#endif

#if !defined(SIMDE_LOONGARCH_LSX_NATIVE) && !defined(SIMDE_LOONGARCH_LSX_NO_NATIVE) && !defined(SIMDE_NO_NATIVE)
  #if defined(SIMDE_ARCH_LOONGARCH_LSX)
    #define SIMDE_LOONGARCH_LSX_NATIVE
  #endif
#endif

#if defined(SIMDE_LOONGARCH_LASX_NATIVE)
  #include <lasxintrin.h>
#endif
#if defined(SIMDE_LOONGARCH_LSX_NATIVE)
  #include <lsxintrin.h>
#endif

#endif /* !defined(SIMDE_FEATURES_H) */
/* :: End simde/simde-features.h :: */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/simde-math.h :: */
/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2017-2020 Evan Nemerson <evan@nemerson.com>
 *   2023      Yi-Yen Chung <eric681@andestech.com> (Copyright owned by Andes Technology)
 */

/* Attempt to find math functions.  Functions may be in <cmath>,
 * <math.h>, compiler built-ins/intrinsics, or platform/architecture
 * specific headers.  In some cases, especially those not built in to
 * libm, we may need to define our own implementations. */

#if !defined(SIMDE_MATH_H)
#define SIMDE_MATH_H 1

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */

#include <stdint.h>
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #include <arm_neon.h>
#endif

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

/* SLEEF support
 * https://sleef.org/
 *
 * If you include <sleef.h> prior to including SIMDe, SIMDe will use
 * SLEEF.  You can also define SIMDE_MATH_SLEEF_ENABLE prior to
 * including SIMDe to force the issue.
 *
 * Note that SLEEF does requires linking to libsleef.
 *
 * By default, SIMDe will use the 1 ULP functions, but if you use
 * SIMDE_ACCURACY_PREFERENCE of 0 we will use up to 4 ULP.  This is
 * only the case for the simde_math_* functions; for code in other
 * SIMDe headers which calls SLEEF directly we may use functions with
 * greater error if the API we're implementing is less precise (for
 * example, SVML guarantees 4 ULP, so we will generally use the 3.5
 * ULP functions from SLEEF). */
#if !defined(SIMDE_MATH_SLEEF_DISABLE)
  #if defined(__SLEEF_H__)
    #define SIMDE_MATH_SLEEF_ENABLE
  #endif
#endif

#if defined(SIMDE_MATH_SLEEF_ENABLE) && !defined(__SLEEF_H__)
  HEDLEY_DIAGNOSTIC_PUSH
  SIMDE_DIAGNOSTIC_DISABLE_IGNORED_QUALIFIERS_
  #include <sleef.h>
  HEDLEY_DIAGNOSTIC_POP
#endif

#if defined(SIMDE_MATH_SLEEF_ENABLE) && defined(__SLEEF_H__)
  #if defined(SLEEF_VERSION_MAJOR)
    #define SIMDE_MATH_SLEEF_VERSION_CHECK(major, minor, patch) (HEDLEY_VERSION_ENCODE(SLEEF_VERSION_MAJOR, SLEEF_VERSION_MINOR, SLEEF_VERSION_PATCHLEVEL) >= HEDLEY_VERSION_ENCODE(major, minor, patch))
  #else
    #define SIMDE_MATH_SLEEF_VERSION_CHECK(major, minor, patch) (HEDLEY_VERSION_ENCODE(3,0,0) >= HEDLEY_VERSION_ENCODE(major, minor, patch))
  #endif
#else
  #define SIMDE_MATH_SLEEF_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(__has_builtin)
  #define SIMDE_MATH_BUILTIN_LIBM(func) __has_builtin(__builtin_##func)
#elif \
    HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    HEDLEY_GCC_VERSION_CHECK(4,4,0)
  #define SIMDE_MATH_BUILTIN_LIBM(func) (1)
#else
  #define SIMDE_MATH_BUILTIN_LIBM(func) (0)
#endif

#if defined(HUGE_VAL)
  /* Looks like <math.h> or <cmath> has already been included. */

  /* The math.h from libc++ (yes, the C header from the C++ standard
   * library) will define an isnan function, but not an isnan macro
   * like the C standard requires.  So we detect the header guards
   * macro libc++ uses. */
  #if defined(isnan) || (defined(_LIBCPP_MATH_H) && !defined(_LIBCPP_CMATH))
    #define SIMDE_MATH_HAVE_MATH_H
  #elif defined(__cplusplus)
    #define SIMDE_MATH_HAVE_CMATH
  #endif
#elif defined(__has_include)
  #if defined(__cplusplus) && (__cplusplus >= 201103L) && __has_include(<cmath>)
    #define SIMDE_MATH_HAVE_CMATH
    #include <cmath>
  #elif __has_include(<math.h>)
    #define SIMDE_MATH_HAVE_MATH_H
    #include <math.h>
  #elif !defined(SIMDE_MATH_NO_LIBM)
    #define SIMDE_MATH_NO_LIBM
  #endif
#elif !defined(SIMDE_MATH_NO_LIBM)
  #if defined(__cplusplus) && (__cplusplus >= 201103L)
    #define SIMDE_MATH_HAVE_CMATH
    HEDLEY_DIAGNOSTIC_PUSH
    #if defined(HEDLEY_MSVC_VERSION)
      /* VS 14 emits this diagnostic about noexcept being used on a
       * <cmath> function, which we can't do anything about. */
      #pragma warning(disable:4996)
    #endif
    #include <cmath>
    HEDLEY_DIAGNOSTIC_POP
  #else
    #define SIMDE_MATH_HAVE_MATH_H
    #include <math.h>
  #endif
#endif

#if !defined(SIMDE_MATH_INFINITY)
  #if \
      HEDLEY_HAS_BUILTIN(__builtin_inf) || \
      HEDLEY_GCC_VERSION_CHECK(3,3,0) || \
      HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
      HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
      HEDLEY_CRAY_VERSION_CHECK(8,1,0)
    #define SIMDE_MATH_INFINITY (__builtin_inf())
  #elif defined(INFINITY)
    #define SIMDE_MATH_INFINITY INFINITY
  #endif
#endif

#if !defined(SIMDE_INFINITYF)
  #if \
      HEDLEY_HAS_BUILTIN(__builtin_inff) || \
      HEDLEY_GCC_VERSION_CHECK(3,3,0) || \
      HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
      HEDLEY_CRAY_VERSION_CHECK(8,1,0) || \
      HEDLEY_IBM_VERSION_CHECK(13,1,0)
    #define SIMDE_MATH_INFINITYF (__builtin_inff())
  #elif defined(INFINITYF)
    #define SIMDE_MATH_INFINITYF INFINITYF
  #elif defined(SIMDE_MATH_INFINITY)
    #define SIMDE_MATH_INFINITYF HEDLEY_STATIC_CAST(float, SIMDE_MATH_INFINITY)
  #endif
#endif

#if !defined(SIMDE_MATH_NAN)
  #if \
      HEDLEY_HAS_BUILTIN(__builtin_nan) || \
      HEDLEY_GCC_VERSION_CHECK(3,3,0) || \
      HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
      HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
      HEDLEY_CRAY_VERSION_CHECK(8,1,0) || \
      HEDLEY_IBM_VERSION_CHECK(13,1,0)
    #define SIMDE_MATH_NAN (__builtin_nan(""))
  #elif defined(NAN)
    #define SIMDE_MATH_NAN NAN
  #endif
#endif

#if !defined(SIMDE_NANF)
  #if \
      HEDLEY_HAS_BUILTIN(__builtin_nanf) || \
      HEDLEY_GCC_VERSION_CHECK(3,3,0) || \
      HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
      HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
      HEDLEY_CRAY_VERSION_CHECK(8,1,0)
    #define SIMDE_MATH_NANF (__builtin_nanf(""))
  #elif defined(NANF)
    #define SIMDE_MATH_NANF NANF
  #elif defined(SIMDE_MATH_NAN)
    #define SIMDE_MATH_NANF HEDLEY_STATIC_CAST(float, SIMDE_MATH_NAN)
  #endif
#endif

#if !defined(SIMDE_MATH_PI)
  #if defined(M_PI)
    #define SIMDE_MATH_PI M_PI
  #else
    #define SIMDE_MATH_PI 3.14159265358979323846
  #endif
#endif

#if !defined(SIMDE_MATH_PIF)
  #if defined(M_PI)
    #define SIMDE_MATH_PIF HEDLEY_STATIC_CAST(float, M_PI)
  #else
    #define SIMDE_MATH_PIF 3.14159265358979323846f
  #endif
#endif

#if !defined(SIMDE_MATH_PI_OVER_180)
  #define SIMDE_MATH_PI_OVER_180 0.0174532925199432957692369076848861271344287188854172545609719144
#endif

#if !defined(SIMDE_MATH_PI_OVER_180F)
  #define SIMDE_MATH_PI_OVER_180F 0.0174532925199432957692369076848861271344287188854172545609719144f
#endif

#if !defined(SIMDE_MATH_180_OVER_PI)
  #define SIMDE_MATH_180_OVER_PI 57.295779513082320876798154814105170332405472466564321549160243861
#endif

#if !defined(SIMDE_MATH_180_OVER_PIF)
  #define SIMDE_MATH_180_OVER_PIF 57.295779513082320876798154814105170332405472466564321549160243861f
#endif

#if !defined(SIMDE_MATH_FLT_MIN)
  #if defined(__FLT_MIN__)
    #define SIMDE_MATH_FLT_MIN __FLT_MIN__
  #else
    #if !defined(FLT_MIN)
      #if defined(__cplusplus)
        #include <cfloat>
      #else
        #include <float.h>
      #endif
    #endif
    #define SIMDE_MATH_FLT_MIN FLT_MIN
  #endif
#endif

#if !defined(SIMDE_MATH_FLT_MAX)
  #if defined(__FLT_MAX__)
    #define SIMDE_MATH_FLT_MAX __FLT_MAX__
  #else
    #if !defined(FLT_MAX)
      #if defined(__cplusplus)
        #include <cfloat>
      #else
        #include <float.h>
      #endif
    #endif
    #define SIMDE_MATH_FLT_MAX FLT_MAX
  #endif
#endif

#if !defined(SIMDE_MATH_DBL_MIN)
  #if defined(__DBL_MIN__)
    #define SIMDE_MATH_DBL_MIN __DBL_MIN__
  #else
    #if !defined(DBL_MIN)
      #if defined(__cplusplus)
        #include <cfloat>
      #else
        #include <float.h>
      #endif
    #endif
    #define SIMDE_MATH_DBL_MIN DBL_MIN
  #endif
#endif

#if !defined(SIMDE_MATH_DBL_MAX)
  #if defined(__DBL_MAX__)
    #define SIMDE_MATH_DBL_MAX __DBL_MAX__
  #else
    #if !defined(DBL_MAX)
      #if defined(__cplusplus)
        #include <cfloat>
      #else
        #include <float.h>
      #endif
    #endif
    #define SIMDE_MATH_DBL_MAX DBL_MAX
  #endif
#endif

/*** Classification macros from C99 ***/

#if !defined(simde_math_isinf)
  #if SIMDE_MATH_BUILTIN_LIBM(isinf)
    #define simde_math_isinf(v) __builtin_isinf(v)
  #elif defined(isinf) || defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_isinf(v) isinf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_isinf(v) std::isinf(v)
  #endif
#endif

#if !defined(simde_math_isinff)
  #if HEDLEY_HAS_BUILTIN(__builtin_isinff) || \
      HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
      HEDLEY_ARM_VERSION_CHECK(4,1,0)
    #define simde_math_isinff(v) __builtin_isinff(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_isinff(v) std::isinf(v)
  #elif defined(simde_math_isinf)
    #define simde_math_isinff(v) simde_math_isinf(HEDLEY_STATIC_CAST(double, v))
  #endif
#endif

#if !defined(simde_math_isnan)
  #if SIMDE_MATH_BUILTIN_LIBM(isnan)
    #define simde_math_isnan(v) __builtin_isnan(v)
  #elif defined(isnan) || defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_isnan(v) isnan(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_isnan(v) std::isnan(v)
  #endif
#endif

#if !defined(simde_math_isnanf)
  #if HEDLEY_HAS_BUILTIN(__builtin_isnanf) || \
      HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
      HEDLEY_ARM_VERSION_CHECK(4,1,0)
    /* XL C/C++ has __builtin_isnan but not __builtin_isnanf */
    #define simde_math_isnanf(v) __builtin_isnanf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_isnanf(v) std::isnan(v)
  #elif defined(simde_math_isnan)
    #define simde_math_isnanf(v) simde_math_isnan(HEDLEY_STATIC_CAST(double, v))
  #endif
#endif

#if !defined(simde_math_isnormal)
  #if SIMDE_MATH_BUILTIN_LIBM(isnormal)
    #define simde_math_isnormal(v) __builtin_isnormal(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_isnormal(v) isnormal(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_isnormal(v) std::isnormal(v)
  #endif
#endif

#if !defined(simde_math_isnormalf)
  #if HEDLEY_HAS_BUILTIN(__builtin_isnormalf)
    #define simde_math_isnormalf(v) __builtin_isnormalf(v)
  #elif SIMDE_MATH_BUILTIN_LIBM(isnormal)
    #define simde_math_isnormalf(v) __builtin_isnormal(v)
  #elif defined(isnormalf)
    #define simde_math_isnormalf(v) isnormalf(v)
  #elif defined(isnormal) || defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_isnormalf(v) isnormal(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_isnormalf(v) std::isnormal(v)
  #elif defined(simde_math_isnormal)
    #define simde_math_isnormalf(v) simde_math_isnormal(v)
  #endif
#endif

#if !defined(simde_math_issubnormalf)
  #if SIMDE_MATH_BUILTIN_LIBM(fpclassify)
    #define simde_math_issubnormalf(v) __builtin_fpclassify(0, 0, 0, 1, 0, v)
  #elif defined(fpclassify)
    #define simde_math_issubnormalf(v) (fpclassify(v) == FP_SUBNORMAL)
  #elif defined(SIMDE_IEEE754_STORAGE)
    #define simde_math_issubnormalf(v) (((simde_float32_as_uint32(v) & UINT32_C(0x7F800000)) == UINT32_C(0)) && ((simde_float32_as_uint32(v) & UINT32_C(0x007FFFFF)) != UINT32_C(0)))
  #endif
#endif

#if !defined(simde_math_issubnormal)
  #if SIMDE_MATH_BUILTIN_LIBM(fpclassify)
    #define simde_math_issubnormal(v) __builtin_fpclassify(0, 0, 0, 1, 0, v)
  #elif defined(fpclassify)
    #define simde_math_issubnormal(v) (fpclassify(v) == FP_SUBNORMAL)
  #elif defined(SIMDE_IEEE754_STORAGE)
    #define simde_math_issubnormal(v) (((simde_float64_as_uint64(v) & UINT64_C(0x7FF0000000000000)) == UINT64_C(0)) && ((simde_float64_as_uint64(v) & UINT64_C(0x00FFFFFFFFFFFFF)) != UINT64_C(0)))
  #endif
#endif

#if defined(FP_NAN)
  #define SIMDE_MATH_FP_NAN FP_NAN
#else
  #define SIMDE_MATH_FP_NAN 0
#endif
#if defined(FP_INFINITE)
  #define SIMDE_MATH_FP_INFINITE FP_INFINITE
#else
  #define SIMDE_MATH_FP_INFINITE 1
#endif
#if defined(FP_ZERO)
  #define SIMDE_MATH_FP_ZERO FP_ZERO
#else
  #define SIMDE_MATH_FP_ZERO 2
#endif
#if defined(FP_SUBNORMAL)
  #define SIMDE_MATH_FP_SUBNORMAL FP_SUBNORMAL
#else
  #define SIMDE_MATH_FP_SUBNORMAL 3
#endif
#if defined(FP_NORMAL)
  #define SIMDE_MATH_FP_NORMAL FP_NORMAL
#else
  #define SIMDE_MATH_FP_NORMAL 4
#endif

static HEDLEY_INLINE
int
simde_math_fpclassifyf(float v) {
  #if SIMDE_MATH_BUILTIN_LIBM(fpclassify)
    return __builtin_fpclassify(SIMDE_MATH_FP_NAN, SIMDE_MATH_FP_INFINITE, SIMDE_MATH_FP_NORMAL, SIMDE_MATH_FP_SUBNORMAL, SIMDE_MATH_FP_ZERO, v);
  #elif defined(fpclassify)
    return fpclassify(v);
  #else
    return
      simde_math_isnormalf(v) ? SIMDE_MATH_FP_NORMAL    :
      (v == 0.0f)             ? SIMDE_MATH_FP_ZERO      :
      simde_math_isnanf(v)    ? SIMDE_MATH_FP_NAN       :
      simde_math_isinff(v)    ? SIMDE_MATH_FP_INFINITE  :
                                SIMDE_MATH_FP_SUBNORMAL;
  #endif
}

static HEDLEY_INLINE
int
simde_math_fpclassify(double v) {
  #if SIMDE_MATH_BUILTIN_LIBM(fpclassify)
    return __builtin_fpclassify(SIMDE_MATH_FP_NAN, SIMDE_MATH_FP_INFINITE, SIMDE_MATH_FP_NORMAL, SIMDE_MATH_FP_SUBNORMAL, SIMDE_MATH_FP_ZERO, v);
  #elif defined(fpclassify)
    return fpclassify(v);
  #else
    return
      simde_math_isnormal(v) ? SIMDE_MATH_FP_NORMAL    :
      (v == 0.0)             ? SIMDE_MATH_FP_ZERO      :
      simde_math_isnan(v)    ? SIMDE_MATH_FP_NAN       :
      simde_math_isinf(v)    ? SIMDE_MATH_FP_INFINITE  :
                               SIMDE_MATH_FP_SUBNORMAL;
  #endif
}

#define SIMDE_MATH_FP_QNAN      0x01
#define SIMDE_MATH_FP_PZERO     0x02
#define SIMDE_MATH_FP_NZERO     0x04
#define SIMDE_MATH_FP_PINF      0x08
#define SIMDE_MATH_FP_NINF      0x10
#define SIMDE_MATH_FP_DENORMAL  0x20
#define SIMDE_MATH_FP_NEGATIVE  0x40
#define SIMDE_MATH_FP_SNAN      0x80

static HEDLEY_INLINE
uint8_t
simde_math_fpclassf(float v, const int imm8) {
  union {
    float f;
    uint32_t u;
  } fu;
  fu.f = v;
  uint32_t bits = fu.u;
  uint8_t NegNum = (bits >> 31) & 1;
  uint32_t const ExpMask = 0x3F800000; // [30:23]
  uint32_t const MantMask = 0x007FFFFF; // [22:0]
  uint8_t ExpAllOnes = ((bits & ExpMask) == ExpMask);
  uint8_t ExpAllZeros = ((bits & ExpMask) == 0);
  uint8_t MantAllZeros = ((bits & MantMask) == 0);
  uint8_t ZeroNumber = ExpAllZeros & MantAllZeros;
  uint8_t SignalingBit = (bits >> 22) & 1;

  uint8_t result = 0;
  uint8_t qNaN_res = ExpAllOnes & (!MantAllZeros) & SignalingBit;
  uint8_t Pzero_res = (!NegNum) & ExpAllZeros & MantAllZeros;
  uint8_t Nzero_res = NegNum & ExpAllZeros & MantAllZeros;
  uint8_t Pinf_res = (!NegNum) & ExpAllOnes & MantAllZeros;
  uint8_t Ninf_res = NegNum & ExpAllOnes & MantAllZeros;
  uint8_t Denorm_res = ExpAllZeros & (!MantAllZeros);
  uint8_t FinNeg_res = NegNum & (!ExpAllOnes) & (!ZeroNumber);
  uint8_t sNaN_res = ExpAllOnes & (!MantAllZeros) & (!SignalingBit);
  result = (((imm8 >> 0) & qNaN_res)   | \
            ((imm8 >> 1) & Pzero_res)  | \
            ((imm8 >> 2) & Nzero_res)  | \
            ((imm8 >> 3) & Pinf_res)   | \
            ((imm8 >> 4) & Ninf_res)   | \
            ((imm8 >> 5) & Denorm_res) | \
            ((imm8 >> 6) & FinNeg_res) | \
            ((imm8 >> 7) & sNaN_res));
  return result;
}

static HEDLEY_INLINE
uint8_t
simde_math_fpclass(double v, const int imm8) {
  union {
    double d;
    uint64_t u;
  } du;
  du.d = v;
  uint64_t bits = du.u;
  uint8_t NegNum = (bits >> 63) & 1;
  uint64_t const ExpMask =  0x3FF0000000000000; // [62:52]
  uint64_t const MantMask = 0x000FFFFFFFFFFFFF; // [51:0]
  uint8_t ExpAllOnes = ((bits & ExpMask) == ExpMask);
  uint8_t ExpAllZeros = ((bits & ExpMask) == 0);
  uint8_t MantAllZeros = ((bits & MantMask) == 0);
  uint8_t ZeroNumber = ExpAllZeros & MantAllZeros;
  uint8_t SignalingBit = (bits >> 51) & 1;

  uint8_t result = 0;
  uint8_t qNaN_res = ExpAllOnes & (!MantAllZeros) & SignalingBit;
  uint8_t Pzero_res = (!NegNum) & ExpAllZeros & MantAllZeros;
  uint8_t Nzero_res = NegNum & ExpAllZeros & MantAllZeros;
  uint8_t Pinf_res = (!NegNum) & ExpAllOnes & MantAllZeros;
  uint8_t Ninf_res = NegNum & ExpAllOnes & MantAllZeros;
  uint8_t Denorm_res = ExpAllZeros & (!MantAllZeros);
  uint8_t FinNeg_res = NegNum & (!ExpAllOnes) & (!ZeroNumber);
  uint8_t sNaN_res = ExpAllOnes & (!MantAllZeros) & (!SignalingBit);
  result = (((imm8 >> 0) & qNaN_res)   | \
            ((imm8 >> 1) & Pzero_res)  | \
            ((imm8 >> 2) & Nzero_res)  | \
            ((imm8 >> 3) & Pinf_res)   | \
            ((imm8 >> 4) & Ninf_res)   | \
            ((imm8 >> 5) & Denorm_res) | \
            ((imm8 >> 6) & FinNeg_res) | \
            ((imm8 >> 7) & sNaN_res));
  return result;
}

/*** Manipulation functions ***/

#if !defined(simde_math_nextafter)
  #if \
      (HEDLEY_HAS_BUILTIN(__builtin_nextafter) && !defined(HEDLEY_IBM_VERSION)) || \
      HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
      HEDLEY_GCC_VERSION_CHECK(3,4,0) || \
      HEDLEY_INTEL_VERSION_CHECK(13,0,0)
    #define simde_math_nextafter(x, y) __builtin_nextafter(x, y)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_nextafter(x, y) std::nextafter(x, y)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_nextafter(x, y) nextafter(x, y)
  #endif
#endif

#if !defined(simde_math_nextafterf)
  #if \
      (HEDLEY_HAS_BUILTIN(__builtin_nextafterf) && !defined(HEDLEY_IBM_VERSION)) || \
      HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
      HEDLEY_GCC_VERSION_CHECK(3,4,0) || \
      HEDLEY_INTEL_VERSION_CHECK(13,0,0)
    #define simde_math_nextafterf(x, y) __builtin_nextafterf(x, y)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_nextafterf(x, y) std::nextafter(x, y)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_nextafterf(x, y) nextafterf(x, y)
  #endif
#endif

/*** Functions from C99 ***/

#if !defined(simde_math_abs)
  #if SIMDE_MATH_BUILTIN_LIBM(abs)
    #define simde_math_abs(v) __builtin_abs(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_abs(v) std::abs(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_abs(v) abs(v)
  #endif
#endif

#if !defined(simde_math_labs)
  #if SIMDE_MATH_BUILTIN_LIBM(labs)
    #define simde_math_labs(v) __builtin_labs(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_labs(v) std::labs(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_labs(v) labs(v)
  #endif
#endif

#if !defined(simde_math_llabs)
  #if SIMDE_MATH_BUILTIN_LIBM(llabs)
    #define simde_math_llabs(v) __builtin_llabs(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_llabs(v) std::llabs(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_llabs(v) llabs(v)
  #endif
#endif

#if !defined(simde_math_fabsf)
  #if SIMDE_MATH_BUILTIN_LIBM(fabsf)
    #define simde_math_fabsf(v) __builtin_fabsf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_fabsf(v) std::abs(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_fabsf(v) fabsf(v)
  #endif
#endif

#if !defined(simde_math_acos)
  #if SIMDE_MATH_BUILTIN_LIBM(acos)
    #define simde_math_acos(v) __builtin_acos(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_acos(v) std::acos(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_acos(v) acos(v)
  #endif
#endif

#if !defined(simde_math_acosf)
  #if SIMDE_MATH_BUILTIN_LIBM(acosf)
    #define simde_math_acosf(v) __builtin_acosf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_acosf(v) std::acos(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_acosf(v) acosf(v)
  #endif
#endif

#if !defined(simde_math_acosh)
  #if SIMDE_MATH_BUILTIN_LIBM(acosh)
    #define simde_math_acosh(v) __builtin_acosh(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_acosh(v) std::acosh(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_acosh(v) acosh(v)
  #endif
#endif

#if !defined(simde_math_acoshf)
  #if SIMDE_MATH_BUILTIN_LIBM(acoshf)
    #define simde_math_acoshf(v) __builtin_acoshf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_acoshf(v) std::acosh(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_acoshf(v) acoshf(v)
  #endif
#endif

#if !defined(simde_math_asin)
  #if SIMDE_MATH_BUILTIN_LIBM(asin)
    #define simde_math_asin(v) __builtin_asin(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_asin(v) std::asin(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_asin(v) asin(v)
  #endif
#endif

#if !defined(simde_math_asinf)
  #if SIMDE_MATH_BUILTIN_LIBM(asinf)
    #define simde_math_asinf(v) __builtin_asinf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_asinf(v) std::asin(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_asinf(v) asinf(v)
  #endif
#endif

#if !defined(simde_math_asinh)
  #if SIMDE_MATH_BUILTIN_LIBM(asinh)
    #define simde_math_asinh(v) __builtin_asinh(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_asinh(v) std::asinh(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_asinh(v) asinh(v)
  #endif
#endif

#if !defined(simde_math_asinhf)
  #if SIMDE_MATH_BUILTIN_LIBM(asinhf)
    #define simde_math_asinhf(v) __builtin_asinhf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_asinhf(v) std::asinh(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_asinhf(v) asinhf(v)
  #endif
#endif

#if !defined(simde_math_atan)
  #if SIMDE_MATH_BUILTIN_LIBM(atan)
    #define simde_math_atan(v) __builtin_atan(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_atan(v) std::atan(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_atan(v) atan(v)
  #endif
#endif

#if !defined(simde_math_atan2)
  #if SIMDE_MATH_BUILTIN_LIBM(atan2)
    #define simde_math_atan2(y, x) __builtin_atan2(y, x)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_atan2(y, x) std::atan2(y, x)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_atan2(y, x) atan2(y, x)
  #endif
#endif

#if !defined(simde_math_atan2f)
  #if SIMDE_MATH_BUILTIN_LIBM(atan2f)
    #define simde_math_atan2f(y, x) __builtin_atan2f(y, x)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_atan2f(y, x) std::atan2(y, x)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_atan2f(y, x) atan2f(y, x)
  #endif
#endif

#if !defined(simde_math_atanf)
  #if SIMDE_MATH_BUILTIN_LIBM(atanf)
    #define simde_math_atanf(v) __builtin_atanf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_atanf(v) std::atan(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_atanf(v) atanf(v)
  #endif
#endif

#if !defined(simde_math_atanh)
  #if SIMDE_MATH_BUILTIN_LIBM(atanh)
    #define simde_math_atanh(v) __builtin_atanh(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_atanh(v) std::atanh(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_atanh(v) atanh(v)
  #endif
#endif

#if !defined(simde_math_atanhf)
  #if SIMDE_MATH_BUILTIN_LIBM(atanhf)
    #define simde_math_atanhf(v) __builtin_atanhf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_atanhf(v) std::atanh(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_atanhf(v) atanhf(v)
  #endif
#endif

#if !defined(simde_math_cbrt)
  #if SIMDE_MATH_BUILTIN_LIBM(cbrt)
    #define simde_math_cbrt(v) __builtin_cbrt(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_cbrt(v) std::cbrt(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_cbrt(v) cbrt(v)
  #endif
#endif

#if !defined(simde_math_cbrtf)
  #if SIMDE_MATH_BUILTIN_LIBM(cbrtf)
    #define simde_math_cbrtf(v) __builtin_cbrtf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_cbrtf(v) std::cbrt(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_cbrtf(v) cbrtf(v)
  #endif
#endif

#if !defined(simde_math_ceil)
  #if SIMDE_MATH_BUILTIN_LIBM(ceil)
    #define simde_math_ceil(v) __builtin_ceil(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_ceil(v) std::ceil(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_ceil(v) ceil(v)
  #endif
#endif

#if !defined(simde_math_ceilf)
  #if SIMDE_MATH_BUILTIN_LIBM(ceilf)
    #define simde_math_ceilf(v) __builtin_ceilf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_ceilf(v) std::ceil(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_ceilf(v) ceilf(v)
  #endif
#endif

#if !defined(simde_math_copysign)
  #if SIMDE_MATH_BUILTIN_LIBM(copysign)
    #define simde_math_copysign(x, y) __builtin_copysign(x, y)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_copysign(x, y) std::copysign(x, y)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_copysign(x, y) copysign(x, y)
  #endif
#endif

#if !defined(simde_math_copysignf)
  #if SIMDE_MATH_BUILTIN_LIBM(copysignf)
    #define simde_math_copysignf(x, y) __builtin_copysignf(x, y)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_copysignf(x, y) std::copysignf(x, y)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_copysignf(x, y) copysignf(x, y)
  #endif
#endif

#if !defined(simde_math_signbit)
  #if SIMDE_MATH_BUILTIN_LIBM(signbit)
    #if (!defined(__clang__) || SIMDE_DETECT_CLANG_VERSION_CHECK(7,0,0))
      #define simde_math_signbit(x) __builtin_signbit(x)
    #else
      #define simde_math_signbit(x) __builtin_signbit(HEDLEY_STATIC_CAST(double, (x)))
    #endif
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_signbit(x) std::signbit(x)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_signbit(x) signbit(x)
  #endif
#endif

#if !defined(simde_math_cos)
  #if SIMDE_MATH_BUILTIN_LIBM(cos)
    #define simde_math_cos(v) __builtin_cos(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_cos(v) std::cos(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_cos(v) cos(v)
  #endif
#endif

#if !defined(simde_math_cosf)
  #if defined(SIMDE_MATH_SLEEF_ENABLE)
    #if SIMDE_ACCURACY_PREFERENCE < 1
      #define simde_math_cosf(v) Sleef_cosf_u35(v)
    #else
      #define simde_math_cosf(v) Sleef_cosf_u10(v)
    #endif
  #elif SIMDE_MATH_BUILTIN_LIBM(cosf)
    #define simde_math_cosf(v) __builtin_cosf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_cosf(v) std::cos(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_cosf(v) cosf(v)
  #endif
#endif

#if !defined(simde_math_cosh)
  #if SIMDE_MATH_BUILTIN_LIBM(cosh)
    #define simde_math_cosh(v) __builtin_cosh(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_cosh(v) std::cosh(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_cosh(v) cosh(v)
  #endif
#endif

#if !defined(simde_math_coshf)
  #if SIMDE_MATH_BUILTIN_LIBM(coshf)
    #define simde_math_coshf(v) __builtin_coshf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_coshf(v) std::cosh(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_coshf(v) coshf(v)
  #endif
#endif

#if !defined(simde_math_erf)
  #if SIMDE_MATH_BUILTIN_LIBM(erf)
    #define simde_math_erf(v) __builtin_erf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_erf(v) std::erf(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_erf(v) erf(v)
  #endif
#endif

#if !defined(simde_math_erff)
  #if SIMDE_MATH_BUILTIN_LIBM(erff)
    #define simde_math_erff(v) __builtin_erff(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_erff(v) std::erf(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_erff(v) erff(v)
  #endif
#endif

#if !defined(simde_math_erfc)
  #if SIMDE_MATH_BUILTIN_LIBM(erfc)
    #define simde_math_erfc(v) __builtin_erfc(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_erfc(v) std::erfc(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_erfc(v) erfc(v)
  #endif
#endif

#if !defined(simde_math_erfcf)
  #if SIMDE_MATH_BUILTIN_LIBM(erfcf)
    #define simde_math_erfcf(v) __builtin_erfcf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_erfcf(v) std::erfc(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_erfcf(v) erfcf(v)
  #endif
#endif

#if !defined(simde_math_exp)
  #if SIMDE_MATH_BUILTIN_LIBM(exp)
    #define simde_math_exp(v) __builtin_exp(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_exp(v) std::exp(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_exp(v) exp(v)
  #endif
#endif

#if !defined(simde_math_expf)
  #if SIMDE_MATH_BUILTIN_LIBM(expf)
    #define simde_math_expf(v) __builtin_expf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_expf(v) std::exp(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_expf(v) expf(v)
  #endif
#endif

#if !defined(simde_math_expm1)
  #if SIMDE_MATH_BUILTIN_LIBM(expm1)
    #define simde_math_expm1(v) __builtin_expm1(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_expm1(v) std::expm1(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_expm1(v) expm1(v)
  #endif
#endif

#if !defined(simde_math_expm1f)
  #if SIMDE_MATH_BUILTIN_LIBM(expm1f)
    #define simde_math_expm1f(v) __builtin_expm1f(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_expm1f(v) std::expm1(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_expm1f(v) expm1f(v)
  #endif
#endif

#if !defined(simde_math_exp2)
  #if SIMDE_MATH_BUILTIN_LIBM(exp2)
    #define simde_math_exp2(v) __builtin_exp2(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_exp2(v) std::exp2(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_exp2(v) exp2(v)
  #endif
#endif

#if !defined(simde_math_exp2f)
  #if SIMDE_MATH_BUILTIN_LIBM(exp2f)
    #define simde_math_exp2f(v) __builtin_exp2f(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_exp2f(v) std::exp2(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_exp2f(v) exp2f(v)
  #endif
#endif

#if HEDLEY_HAS_BUILTIN(__builtin_exp10) ||  HEDLEY_GCC_VERSION_CHECK(3,4,0)
  #  define simde_math_exp10(v) __builtin_exp10(v)
#else
#  define simde_math_exp10(v) pow(10.0, (v))
#endif

#if HEDLEY_HAS_BUILTIN(__builtin_exp10f) ||  HEDLEY_GCC_VERSION_CHECK(3,4,0)
  #  define simde_math_exp10f(v) __builtin_exp10f(v)
#else
#  define simde_math_exp10f(v) powf(10.0f, (v))
#endif

#if !defined(simde_math_fabs)
  #if SIMDE_MATH_BUILTIN_LIBM(fabs)
    #define simde_math_fabs(v) __builtin_fabs(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_fabs(v) std::fabs(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_fabs(v) fabs(v)
  #endif
#endif

#if !defined(simde_math_fabsf)
  #if SIMDE_MATH_BUILTIN_LIBM(fabsf)
    #define simde_math_fabsf(v) __builtin_fabsf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_fabsf(v) std::fabs(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_fabsf(v) fabsf(v)
  #endif
#endif

#if !defined(simde_math_floor)
  #if SIMDE_MATH_BUILTIN_LIBM(floor)
    #define simde_math_floor(v) __builtin_floor(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_floor(v) std::floor(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_floor(v) floor(v)
  #endif
#endif

#if !defined(simde_math_floorf)
  #if SIMDE_MATH_BUILTIN_LIBM(floorf)
    #define simde_math_floorf(v) __builtin_floorf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_floorf(v) std::floor(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_floorf(v) floorf(v)
  #endif
#endif

#if !defined(simde_math_fma)
  #if SIMDE_MATH_BUILTIN_LIBM(fma)
    #define simde_math_fma(x, y, z) __builtin_fma(x, y, z)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_fma(x, y, z) std::fma(x, y, z)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_fma(x, y, z) fma(x, y, z)
  #endif
#endif

#if !defined(simde_math_fmaf)
  #if SIMDE_MATH_BUILTIN_LIBM(fmaf)
    #define simde_math_fmaf(x, y, z) __builtin_fmaf(x, y, z)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_fmaf(x, y, z) std::fma(x, y, z)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_fmaf(x, y, z) fmaf(x, y, z)
  #endif
#endif

#if !defined(simde_math_fmax)
  #if SIMDE_MATH_BUILTIN_LIBM(fmax)
    #define simde_math_fmax(x, y) __builtin_fmax(x, y)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_fmax(x, y) std::fmax(x, y)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_fmax(x, y) fmax(x, y)
  #endif
#endif

#if !defined(simde_math_fmaxf)
  #if SIMDE_MATH_BUILTIN_LIBM(fmaxf)
    #define simde_math_fmaxf(x, y) __builtin_fmaxf(x, y)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_fmaxf(x, y) std::fmax(x, y)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_fmaxf(x, y) fmaxf(x, y)
  #endif
#endif

#if !defined(simde_math_hypot)
  #if SIMDE_MATH_BUILTIN_LIBM(hypot)
    #define simde_math_hypot(y, x) __builtin_hypot(y, x)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_hypot(y, x) std::hypot(y, x)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_hypot(y, x) hypot(y, x)
  #endif
#endif

#if !defined(simde_math_hypotf)
  #if SIMDE_MATH_BUILTIN_LIBM(hypotf)
    #define simde_math_hypotf(y, x) __builtin_hypotf(y, x)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_hypotf(y, x) std::hypot(y, x)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_hypotf(y, x) hypotf(y, x)
  #endif
#endif

#if !defined(simde_math_log)
  #if SIMDE_MATH_BUILTIN_LIBM(log)
    #define simde_math_log(v) __builtin_log(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_log(v) std::log(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_log(v) log(v)
  #endif
#endif

#if !defined(simde_math_logf)
  #if SIMDE_MATH_BUILTIN_LIBM(logf)
    #define simde_math_logf(v) __builtin_logf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_logf(v) std::log(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_logf(v) logf(v)
  #endif
#endif

#if !defined(simde_math_logb)
  #if SIMDE_MATH_BUILTIN_LIBM(logb)
    #define simde_math_logb(v) __builtin_logb(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_logb(v) std::logb(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_logb(v) logb(v)
  #endif
#endif

#if !defined(simde_math_logbf)
  #if SIMDE_MATH_BUILTIN_LIBM(logbf)
    #define simde_math_logbf(v) __builtin_logbf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_logbf(v) std::logb(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_logbf(v) logbf(v)
  #endif
#endif

#if !defined(simde_math_log1p)
  #if SIMDE_MATH_BUILTIN_LIBM(log1p)
    #define simde_math_log1p(v) __builtin_log1p(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_log1p(v) std::log1p(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_log1p(v) log1p(v)
  #endif
#endif

#if !defined(simde_math_log1pf)
  #if SIMDE_MATH_BUILTIN_LIBM(log1pf)
    #define simde_math_log1pf(v) __builtin_log1pf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_log1pf(v) std::log1p(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_log1pf(v) log1pf(v)
  #endif
#endif

#if !defined(simde_math_log2)
  #if SIMDE_MATH_BUILTIN_LIBM(log2)
    #define simde_math_log2(v) __builtin_log2(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_log2(v) std::log2(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_log2(v) log2(v)
  #endif
#endif

#if !defined(simde_math_log2f)
  #if SIMDE_MATH_BUILTIN_LIBM(log2f)
    #define simde_math_log2f(v) __builtin_log2f(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_log2f(v) std::log2(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_log2f(v) log2f(v)
  #endif
#endif

#if !defined(simde_math_log10)
  #if SIMDE_MATH_BUILTIN_LIBM(log10)
    #define simde_math_log10(v) __builtin_log10(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_log10(v) std::log10(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_log10(v) log10(v)
  #endif
#endif

#if !defined(simde_math_log10f)
  #if SIMDE_MATH_BUILTIN_LIBM(log10f)
    #define simde_math_log10f(v) __builtin_log10f(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_log10f(v) std::log10(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_log10f(v) log10f(v)
  #endif
#endif

#if !defined(simde_math_modf)
  #if SIMDE_MATH_BUILTIN_LIBM(modf)
    #define simde_math_modf(x, iptr) __builtin_modf(x, iptr)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_modf(x, iptr) std::modf(x, iptr)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_modf(x, iptr) modf(x, iptr)
  #endif
#endif

#if !defined(simde_math_modff)
  #if SIMDE_MATH_BUILTIN_LIBM(modff)
    #define simde_math_modff(x, iptr) __builtin_modff(x, iptr)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_modff(x, iptr) std::modf(x, iptr)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_modff(x, iptr) modff(x, iptr)
  #endif
#endif

#if !defined(simde_math_nearbyint)
  #if SIMDE_MATH_BUILTIN_LIBM(nearbyint)
    #define simde_math_nearbyint(v) __builtin_nearbyint(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_nearbyint(v) std::nearbyint(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_nearbyint(v) nearbyint(v)
  #endif
#endif

#if !defined(simde_math_nearbyintf)
  #if SIMDE_MATH_BUILTIN_LIBM(nearbyintf)
    #define simde_math_nearbyintf(v) __builtin_nearbyintf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_nearbyintf(v) std::nearbyint(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_nearbyintf(v) nearbyintf(v)
  #endif
#endif

#if !defined(simde_math_pow)
  #if SIMDE_MATH_BUILTIN_LIBM(pow)
    #define simde_math_pow(y, x) __builtin_pow(y, x)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_pow(y, x) std::pow(y, x)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_pow(y, x) pow(y, x)
  #endif
#endif

#if !defined(simde_math_powf)
  #if SIMDE_MATH_BUILTIN_LIBM(powf)
    #define simde_math_powf(y, x) __builtin_powf(y, x)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_powf(y, x) std::pow(y, x)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_powf(y, x) powf(y, x)
  #endif
#endif

#if !defined(simde_math_rint)
  #if SIMDE_MATH_BUILTIN_LIBM(rint)
    #define simde_math_rint(v) __builtin_rint(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_rint(v) std::rint(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_rint(v) rint(v)
  #endif
#endif

#if !defined(simde_math_rintf)
  #if SIMDE_MATH_BUILTIN_LIBM(rintf)
    #define simde_math_rintf(v) __builtin_rintf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_rintf(v) std::rint(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_rintf(v) rintf(v)
  #endif
#endif

#if !defined(simde_math_round)
  #if SIMDE_MATH_BUILTIN_LIBM(round)
    #define simde_math_round(v) __builtin_round(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_round(v) std::round(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_round(v) round(v)
  #endif
#endif

#if !defined(simde_math_roundf)
  #if SIMDE_MATH_BUILTIN_LIBM(roundf)
    #define simde_math_roundf(v) __builtin_roundf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_roundf(v) std::round(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_roundf(v) roundf(v)
  #endif
#endif

#if !defined(simde_math_roundeven)
  #if \
     ((!defined(HEDLEY_EMSCRIPTEN_VERSION) || HEDLEY_EMSCRIPTEN_VERSION_CHECK(3, 1, 43)) && HEDLEY_HAS_BUILTIN(__builtin_roundeven)) || \
      HEDLEY_GCC_VERSION_CHECK(10,0,0)
    #define simde_math_roundeven(v) __builtin_roundeven(v)
  #elif defined(simde_math_round) && defined(simde_math_fabs)
    static HEDLEY_INLINE
    double
    simde_math_roundeven(double v) {
      double rounded = simde_math_round(v);
      double diff = rounded - v;
      if (HEDLEY_UNLIKELY(simde_math_fabs(diff) == 0.5) && (HEDLEY_STATIC_CAST(int64_t, rounded) & 1)) {
        rounded = v - diff;
      }
      return rounded;
    }
    #define simde_math_roundeven simde_math_roundeven
  #endif
#endif

#if !defined(simde_math_roundevenf)
  #if \
     ((!defined(HEDLEY_EMSCRIPTEN_VERSION) || HEDLEY_EMSCRIPTEN_VERSION_CHECK(3, 1, 43)) && HEDLEY_HAS_BUILTIN(__builtin_roundevenf)) || \
      HEDLEY_GCC_VERSION_CHECK(10,0,0)
    #define simde_math_roundevenf(v) __builtin_roundevenf(v)
  #elif defined(simde_math_roundf) && defined(simde_math_fabsf)
    static HEDLEY_INLINE
    float
    simde_math_roundevenf(float v) {
      float rounded = simde_math_roundf(v);
      float diff = rounded - v;
      if (HEDLEY_UNLIKELY(simde_math_fabsf(diff) == 0.5f) && (HEDLEY_STATIC_CAST(int32_t, rounded) & 1)) {
        rounded = v - diff;
      }
      return rounded;
    }
    #define simde_math_roundevenf simde_math_roundevenf
  #endif
#endif

#if !defined(simde_math_sin)
  #if SIMDE_MATH_BUILTIN_LIBM(sin)
    #define simde_math_sin(v) __builtin_sin(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_sin(v) std::sin(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_sin(v) sin(v)
  #endif
#endif

#if !defined(simde_math_sinf)
  #if SIMDE_MATH_BUILTIN_LIBM(sinf)
    #define simde_math_sinf(v) __builtin_sinf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_sinf(v) std::sin(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_sinf(v) sinf(v)
  #endif
#endif

#if !defined(simde_math_sinh)
  #if SIMDE_MATH_BUILTIN_LIBM(sinh)
    #define simde_math_sinh(v) __builtin_sinh(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_sinh(v) std::sinh(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_sinh(v) sinh(v)
  #endif
#endif

#if !defined(simde_math_sinhf)
  #if SIMDE_MATH_BUILTIN_LIBM(sinhf)
    #define simde_math_sinhf(v) __builtin_sinhf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_sinhf(v) std::sinh(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_sinhf(v) sinhf(v)
  #endif
#endif

#if !defined(simde_math_sqrt)
  #if SIMDE_MATH_BUILTIN_LIBM(sqrt)
    #define simde_math_sqrt(v) __builtin_sqrt(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_sqrt(v) std::sqrt(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_sqrt(v) sqrt(v)
  #endif
#endif

#if !defined(simde_math_sqrtf)
  #if SIMDE_MATH_BUILTIN_LIBM(sqrtf)
    #define simde_math_sqrtf(v) __builtin_sqrtf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_sqrtf(v) std::sqrt(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_sqrtf(v) sqrtf(v)
  #endif
#endif

#if !defined(simde_math_sqrtl)
  #if SIMDE_MATH_BUILTIN_LIBM(sqrtl)
    #define simde_math_sqrtl(v) __builtin_sqrtl(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_sqrtl(v) std::sqrt(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_sqrtl(v) sqrtl(v)
  #endif
#endif

#if !defined(simde_math_tan)
  #if SIMDE_MATH_BUILTIN_LIBM(tan)
    #define simde_math_tan(v) __builtin_tan(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_tan(v) std::tan(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_tan(v) tan(v)
  #endif
#endif

#if !defined(simde_math_tanf)
  #if SIMDE_MATH_BUILTIN_LIBM(tanf)
    #define simde_math_tanf(v) __builtin_tanf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_tanf(v) std::tan(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_tanf(v) tanf(v)
  #endif
#endif

#if !defined(simde_math_tanh)
  #if SIMDE_MATH_BUILTIN_LIBM(tanh)
    #define simde_math_tanh(v) __builtin_tanh(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_tanh(v) std::tanh(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_tanh(v) tanh(v)
  #endif
#endif

#if !defined(simde_math_tanhf)
  #if SIMDE_MATH_BUILTIN_LIBM(tanhf)
    #define simde_math_tanhf(v) __builtin_tanhf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_tanhf(v) std::tanh(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_tanhf(v) tanhf(v)
  #endif
#endif

#if !defined(simde_math_trunc)
  #if SIMDE_MATH_BUILTIN_LIBM(trunc)
    #define simde_math_trunc(v) __builtin_trunc(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_trunc(v) std::trunc(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_trunc(v) trunc(v)
  #endif
#endif

#if !defined(simde_math_truncf)
  #if SIMDE_MATH_BUILTIN_LIBM(truncf)
    #define simde_math_truncf(v) __builtin_truncf(v)
  #elif defined(SIMDE_MATH_HAVE_CMATH)
    #define simde_math_truncf(v) std::trunc(v)
  #elif defined(SIMDE_MATH_HAVE_MATH_H)
    #define simde_math_truncf(v) truncf(v)
  #endif
#endif

/*** Comparison macros (which don't raise invalid errors) ***/

#if defined(isunordered)
  #define simde_math_isunordered(x, y) isunordered(x, y)
#elif HEDLEY_HAS_BUILTIN(__builtin_isunordered)
  #define simde_math_isunordered(x, y) __builtin_isunordered(x, y)
#else
  static HEDLEY_INLINE
  int simde_math_isunordered(double x, double y) {
    return (x != y) && (x != x || y != y);
  }
  #define simde_math_isunordered simde_math_isunordered

  static HEDLEY_INLINE
  int simde_math_isunorderedf(float x, float y) {
    return (x != y) && (x != x || y != y);
  }
  #define simde_math_isunorderedf simde_math_isunorderedf
#endif
#if !defined(simde_math_isunorderedf)
  #define simde_math_isunorderedf simde_math_isunordered
#endif

/*** Additional functions not in libm ***/

#if defined(simde_math_fabs) && defined(simde_math_sqrt) && defined(simde_math_exp)
  static HEDLEY_INLINE
  double
  simde_math_cdfnorm(double x) {
    /* https://www.johndcook.com/blog/cpp_phi/
    * Public Domain */
    static const double a1 =  0.254829592;
    static const double a2 = -0.284496736;
    static const double a3 =  1.421413741;
    static const double a4 = -1.453152027;
    static const double a5 =  1.061405429;
    static const double p  =  0.3275911;

    const int sign = x < 0;
    x = simde_math_fabs(x) / simde_math_sqrt(2.0);

    /* A&S formula 7.1.26 */
    double t = 1.0 / (1.0 + p * x);
    double y = 1.0 - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1) * t * simde_math_exp(-x * x);

    return 0.5 * (1.0 + (sign ? -y : y));
  }
  #define simde_math_cdfnorm simde_math_cdfnorm
#endif

#if defined(simde_math_fabsf) && defined(simde_math_sqrtf) && defined(simde_math_expf)
  static HEDLEY_INLINE
  float
  simde_math_cdfnormf(float x) {
    /* https://www.johndcook.com/blog/cpp_phi/
    * Public Domain */
    static const float a1 =  0.254829592f;
    static const float a2 = -0.284496736f;
    static const float a3 =  1.421413741f;
    static const float a4 = -1.453152027f;
    static const float a5 =  1.061405429f;
    static const float p  =  0.3275911f;

    const int sign = x < 0;
    x = simde_math_fabsf(x) / simde_math_sqrtf(2.0f);

    /* A&S formula 7.1.26 */
    float t = 1.0f / (1.0f + p * x);
    float y = 1.0f - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1) * t * simde_math_expf(-x * x);

    return 0.5f * (1.0f + (sign ? -y : y));
  }
  #define simde_math_cdfnormf simde_math_cdfnormf
#endif

#if !defined(simde_math_cdfnorminv) && defined(simde_math_log) && defined(simde_math_sqrt)
  /*https://web.archive.org/web/20150910081113/http://home.online.no/~pjacklam/notes/invnorm/impl/sprouse/ltqnorm.c*/
  static HEDLEY_INLINE
  double
  simde_math_cdfnorminv(double p) {
    static const double a[6] = {
      -3.969683028665376e+01,
       2.209460984245205e+02,
      -2.759285104469687e+02,
       1.383577518672690e+02,
      -3.066479806614716e+01,
       2.506628277459239e+00
    };

    static const double b[5] = {
      -5.447609879822406e+01,
       1.615858368580409e+02,
      -1.556989798598866e+02,
       6.680131188771972e+01,
      -1.328068155288572e+01
    };

    static const double c[6] = {
      -7.784894002430293e-03,
      -3.223964580411365e-01,
      -2.400758277161838e+00,
      -2.549732539343734e+00,
       4.374664141464968e+00,
       2.938163982698783e+00
    };

    static const double d[4] = {
      7.784695709041462e-03,
      3.224671290700398e-01,
      2.445134137142996e+00,
      3.754408661907416e+00
    };

    static const double low  = 0.02425;
    static const double high = 0.97575;
    double q, r;

    if (p < 0 || p > 1) {
      return 0.0;
    } else if (p == 0) {
      return -SIMDE_MATH_INFINITY;
    } else if (p == 1) {
      return SIMDE_MATH_INFINITY;
    } else if (p < low) {
      q = simde_math_sqrt(-2.0 * simde_math_log(p));
      return
        (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
        (((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1));
    } else if (p > high) {
      q = simde_math_sqrt(-2.0 * simde_math_log(1.0 - p));
      return
        -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
         (((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1));
    } else {
      q = p - 0.5;
      r = q * q;
      return (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) *
        q / (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1);
    }
}
#define simde_math_cdfnorminv simde_math_cdfnorminv
#endif

#if !defined(simde_math_cdfnorminvf) && defined(simde_math_logf) && defined(simde_math_sqrtf)
  static HEDLEY_INLINE
  float
  simde_math_cdfnorminvf(float p) {
    static const float a[6] = {
      -3.969683028665376e+01f,
       2.209460984245205e+02f,
      -2.759285104469687e+02f,
       1.383577518672690e+02f,
      -3.066479806614716e+01f,
       2.506628277459239e+00f
    };
    static const float b[5] = {
      -5.447609879822406e+01f,
       1.615858368580409e+02f,
      -1.556989798598866e+02f,
       6.680131188771972e+01f,
      -1.328068155288572e+01f
    };
    static const float c[6] = {
      -7.784894002430293e-03f,
      -3.223964580411365e-01f,
      -2.400758277161838e+00f,
      -2.549732539343734e+00f,
       4.374664141464968e+00f,
       2.938163982698783e+00f
    };
    static const float d[4] = {
      7.784695709041462e-03f,
      3.224671290700398e-01f,
      2.445134137142996e+00f,
      3.754408661907416e+00f
    };
    static const float low  = 0.02425f;
    static const float high = 0.97575f;
    float q, r;

    if (p < 0 || p > 1) {
      return 0.0f;
    } else if (p == 0) {
      return -SIMDE_MATH_INFINITYF;
    } else if (p == 1) {
      return SIMDE_MATH_INFINITYF;
    } else if (p < low) {
      q = simde_math_sqrtf(-2.0f * simde_math_logf(p));
      return
        (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
        (((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1));
    } else if (p > high) {
      q = simde_math_sqrtf(-2.0f * simde_math_logf(1.0f - p));
      return
        -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
         (((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1));
    } else {
      q = p - 0.5f;
      r = q * q;
      return (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) *
         q / (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1);
    }
  }
  #define simde_math_cdfnorminvf simde_math_cdfnorminvf
#endif

#if !defined(simde_math_erfinv) && defined(simde_math_log) && defined(simde_math_copysign) && defined(simde_math_sqrt)
  static HEDLEY_INLINE
  double
  simde_math_erfinv(double x) {
    /* https://stackoverflow.com/questions/27229371/inverse-error-function-in-c
     *
     * The original answer on SO uses a constant of 0.147, but in my
     * testing 0.14829094707965850830078125 gives a lower average absolute error
     * (0.0001410958211636170744895935 vs. 0.0001465479290345683693885803).
     * That said, if your goal is to minimize the *maximum* absolute
     * error, 0.15449436008930206298828125 provides significantly better
     * results; 0.0009250640869140625000000000 vs ~ 0.005. */
    double tt1, tt2, lnx;
    double sgn = simde_math_copysign(1.0, x);

    x = (1.0 - x) * (1.0 + x);
    lnx = simde_math_log(x);

    tt1 = 2.0 / (SIMDE_MATH_PI * 0.14829094707965850830078125) + 0.5 * lnx;
    tt2 = (1.0 / 0.14829094707965850830078125) * lnx;

    return sgn * simde_math_sqrt(-tt1 + simde_math_sqrt(tt1 * tt1 - tt2));
  }
  #define simde_math_erfinv simde_math_erfinv
#endif

#if !defined(simde_math_erfinvf) && defined(simde_math_logf) && defined(simde_math_copysignf) && defined(simde_math_sqrtf)
  static HEDLEY_INLINE
  float
  simde_math_erfinvf(float x) {
    float tt1, tt2, lnx;
    float sgn = simde_math_copysignf(1.0f, x);

    x = (1.0f - x) * (1.0f + x);
    lnx = simde_math_logf(x);

    tt1 = 2.0f / (SIMDE_MATH_PIF * 0.14829094707965850830078125f) + 0.5f * lnx;
    tt2 = (1.0f / 0.14829094707965850830078125f) * lnx;

    return sgn * simde_math_sqrtf(-tt1 + simde_math_sqrtf(tt1 * tt1 - tt2));
  }
  #define simde_math_erfinvf simde_math_erfinvf
#endif

#if !defined(simde_math_erfcinv) && defined(simde_math_erfinv) && defined(simde_math_log) && defined(simde_math_sqrt)
  static HEDLEY_INLINE
  double
  simde_math_erfcinv(double x) {
    if(x >= 0.0625 && x < 2.0) {
      return simde_math_erfinv(1.0 - x);
    } else if (x < 0.0625 && x >= 1.0e-100) {
      static const double p[6] = {
        0.1550470003116,
        1.382719649631,
        0.690969348887,
        -1.128081391617,
        0.680544246825,
        -0.16444156791
      };
      static const double q[3] = {
        0.155024849822,
        1.385228141995,
        1.000000000000
      };

      const double t = 1.0 / simde_math_sqrt(-simde_math_log(x));
      return (p[0] / t + p[1] + t * (p[2] + t * (p[3] + t * (p[4] + t * p[5])))) /
            (q[0] + t * (q[1] + t * (q[2])));
    } else if (x < 1.0e-100 && x >= SIMDE_MATH_DBL_MIN) {
      static const double p[4] = {
        0.00980456202915,
        0.363667889171,
        0.97302949837,
        -0.5374947401
      };
      static const double q[3] = {
        0.00980451277802,
        0.363699971544,
        1.000000000000
      };

      const double t = 1.0 / simde_math_sqrt(-simde_math_log(x));
      return (p[0] / t + p[1] + t * (p[2] + t * p[3])) /
             (q[0] + t * (q[1] + t * (q[2])));
    } else if (!simde_math_isnormal(x)) {
      return SIMDE_MATH_INFINITY;
    } else {
      return -SIMDE_MATH_INFINITY;
    }
  }

  #define simde_math_erfcinv simde_math_erfcinv
#endif

#if !defined(simde_math_erfcinvf) && defined(simde_math_erfinvf) && defined(simde_math_logf) && defined(simde_math_sqrtf)
  static HEDLEY_INLINE
  float
  simde_math_erfcinvf(float x) {
    if(x >= 0.0625f && x < 2.0f) {
      return simde_math_erfinvf(1.0f - x);
    } else if (x < 0.0625f && x >= SIMDE_MATH_FLT_MIN) {
      static const float p[6] = {
         0.1550470003116f,
         1.382719649631f,
         0.690969348887f,
        -1.128081391617f,
         0.680544246825f
        -0.164441567910f
      };
      static const float q[3] = {
        0.155024849822f,
        1.385228141995f,
        1.000000000000f
      };

      const float t = 1.0f / simde_math_sqrtf(-simde_math_logf(x));
      return (p[0] / t + p[1] + t * (p[2] + t * (p[3] + t * (p[4] + t * p[5])))) /
             (q[0] + t * (q[1] + t * (q[2])));
    } else if (x < SIMDE_MATH_FLT_MIN && simde_math_isnormalf(x)) {
      static const float p[4] = {
        0.00980456202915f,
        0.36366788917100f,
        0.97302949837000f,
        -0.5374947401000f
      };
      static const float q[3] = {
        0.00980451277802f,
        0.36369997154400f,
        1.00000000000000f
      };

      const float t = 1.0f / simde_math_sqrtf(-simde_math_logf(x));
      return (p[0] / t + p[1] + t * (p[2] + t * p[3])) /
             (q[0] + t * (q[1] + t * (q[2])));
    } else {
      return simde_math_isnormalf(x) ? -SIMDE_MATH_INFINITYF : SIMDE_MATH_INFINITYF;
    }
  }

  #define simde_math_erfcinvf simde_math_erfcinvf
#endif

static HEDLEY_INLINE
double
simde_math_rad2deg(double radians) {
 return radians * SIMDE_MATH_180_OVER_PI;
}

static HEDLEY_INLINE
float
simde_math_rad2degf(float radians) {
    return radians * SIMDE_MATH_180_OVER_PIF;
}

static HEDLEY_INLINE
double
simde_math_deg2rad(double degrees) {
  return degrees * SIMDE_MATH_PI_OVER_180;
}

static HEDLEY_INLINE
float
simde_math_deg2radf(float degrees) {
    return degrees * (SIMDE_MATH_PI_OVER_180F);
}

/***  Saturated arithmetic ***/

static HEDLEY_INLINE
int8_t
simde_math_adds_i8(int8_t a, int8_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqaddb_s8(a, b);
  #else
    uint8_t a_ = HEDLEY_STATIC_CAST(uint8_t, a);
    uint8_t b_ = HEDLEY_STATIC_CAST(uint8_t, b);
    uint8_t r_ = a_ + b_;

    a_ = (a_ >> ((8 * sizeof(r_)) - 1)) + INT8_MAX;
    if (HEDLEY_STATIC_CAST(int8_t, ((a_ ^ b_) | ~(b_ ^ r_))) >= 0) {
      r_ = a_;
    }

    return HEDLEY_STATIC_CAST(int8_t, r_);
  #endif
}

static HEDLEY_INLINE
int16_t
simde_math_adds_i16(int16_t a, int16_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqaddh_s16(a, b);
  #else
    uint16_t a_ = HEDLEY_STATIC_CAST(uint16_t, a);
    uint16_t b_ = HEDLEY_STATIC_CAST(uint16_t, b);
    uint16_t r_ = a_ + b_;

    a_ = (a_ >> ((8 * sizeof(r_)) - 1)) + INT16_MAX;
    if (HEDLEY_STATIC_CAST(int16_t, ((a_ ^ b_) | ~(b_ ^ r_))) >= 0) {
      r_ = a_;
    }

    return HEDLEY_STATIC_CAST(int16_t, r_);
  #endif
}

static HEDLEY_INLINE
int32_t
simde_math_adds_i32(int32_t a, int32_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqadds_s32(a, b);
  #else
    uint32_t a_ = HEDLEY_STATIC_CAST(uint32_t, a);
    uint32_t b_ = HEDLEY_STATIC_CAST(uint32_t, b);
    uint32_t r_ = a_ + b_;

    a_ = (a_ >> ((8 * sizeof(r_)) - 1)) + INT32_MAX;
    if (HEDLEY_STATIC_CAST(int32_t, ((a_ ^ b_) | ~(b_ ^ r_))) >= 0) {
      r_ = a_;
    }

    return HEDLEY_STATIC_CAST(int32_t, r_);
  #endif
}

static HEDLEY_INLINE
int64_t
simde_math_adds_i64(int64_t a, int64_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqaddd_s64(a, b);
  #else
    uint64_t a_ = HEDLEY_STATIC_CAST(uint64_t, a);
    uint64_t b_ = HEDLEY_STATIC_CAST(uint64_t, b);
    uint64_t r_ = a_ + b_;

    a_ = (a_ >> ((8 * sizeof(r_)) - 1)) + INT64_MAX;
    if (HEDLEY_STATIC_CAST(int64_t, ((a_ ^ b_) | ~(b_ ^ r_))) >= 0) {
      r_ = a_;
    }

    return HEDLEY_STATIC_CAST(int64_t, r_);
  #endif
}

static HEDLEY_INLINE
uint8_t
simde_math_adds_u8(uint8_t a, uint8_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqaddb_u8(a, b);
  #else
    uint8_t r = a + b;
    r |= -(r < a);
    return r;
  #endif
}

static HEDLEY_INLINE
uint16_t
simde_math_adds_u16(uint16_t a, uint16_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqaddh_u16(a, b);
  #else
    uint16_t r = a + b;
    r |= -(r < a);
    return r;
  #endif
}

static HEDLEY_INLINE
uint32_t
simde_math_adds_u32(uint32_t a, uint32_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqadds_u32(a, b);
  #else
    uint32_t r = a + b;
    r |= -(r < a);
    return r;
  #endif
}

static HEDLEY_INLINE
uint64_t
simde_math_adds_u64(uint64_t a, uint64_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqaddd_u64(a, b);
  #else
    uint64_t r = a + b;
    r |= -(r < a);
    return r;
  #endif
}

static HEDLEY_INLINE
int8_t
simde_math_subs_i8(int8_t a, int8_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqsubb_s8(a, b);
  #else
    uint8_t a_ = HEDLEY_STATIC_CAST(uint8_t, a);
    uint8_t b_ = HEDLEY_STATIC_CAST(uint8_t, b);
    uint8_t r_ = a_ - b_;

    a_ = (a_ >> 7) + INT8_MAX;

    if (HEDLEY_STATIC_CAST(int8_t, (a_ ^ b_) & (a_ ^ r_)) < 0) {
      r_ = a_;
    }

    return HEDLEY_STATIC_CAST(int8_t, r_);
  #endif
}

static HEDLEY_INLINE
int16_t
simde_math_subs_i16(int16_t a, int16_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqsubh_s16(a, b);
  #else
    uint16_t a_ = HEDLEY_STATIC_CAST(uint16_t, a);
    uint16_t b_ = HEDLEY_STATIC_CAST(uint16_t, b);
    uint16_t r_ = a_ - b_;

    a_ = (a_ >> 15) + INT16_MAX;

    if (HEDLEY_STATIC_CAST(int16_t, (a_ ^ b_) & (a_ ^ r_)) < 0) {
      r_ = a_;
    }

    return HEDLEY_STATIC_CAST(int16_t, r_);
  #endif
}

static HEDLEY_INLINE
int32_t
simde_math_subs_i32(int32_t a, int32_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqsubs_s32(a, b);
  #else
    uint32_t a_ = HEDLEY_STATIC_CAST(uint32_t, a);
    uint32_t b_ = HEDLEY_STATIC_CAST(uint32_t, b);
    uint32_t r_ = a_ - b_;

    a_ = (a_ >> 31) + INT32_MAX;

    if (HEDLEY_STATIC_CAST(int32_t, (a_ ^ b_) & (a_ ^ r_)) < 0) {
      r_ = a_;
    }

    return HEDLEY_STATIC_CAST(int32_t, r_);
  #endif
}

static HEDLEY_INLINE
int64_t
simde_math_subs_i64(int64_t a, int64_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqsubd_s64(a, b);
  #else
    uint64_t a_ = HEDLEY_STATIC_CAST(uint64_t, a);
    uint64_t b_ = HEDLEY_STATIC_CAST(uint64_t, b);
    uint64_t r_ = a_ - b_;

    a_ = (a_ >> 63) + INT64_MAX;

    if (HEDLEY_STATIC_CAST(int64_t, (a_ ^ b_) & (a_ ^ r_)) < 0) {
      r_ = a_;
    }

    return HEDLEY_STATIC_CAST(int64_t, r_);
  #endif
}

static HEDLEY_INLINE
uint8_t
simde_math_subs_u8(uint8_t a, uint8_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqsubb_u8(a, b);
  #else
    uint8_t res = a - b;
    res &= -(res <= a);
    return res;
  #endif
}

static HEDLEY_INLINE
uint16_t
simde_math_subs_u16(uint16_t a, uint16_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqsubh_u16(a, b);
  #else
    uint16_t res = a - b;
    res &= -(res <= a);
    return res;
  #endif
}

static HEDLEY_INLINE
uint32_t
simde_math_subs_u32(uint32_t a, uint32_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqsubs_u32(a, b);
  #else
    uint32_t res = a - b;
    res &= -(res <= a);
    return res;
  #endif
}

static HEDLEY_INLINE
uint64_t
simde_math_subs_u64(uint64_t a, uint64_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqsubd_u64(a, b);
  #else
    uint64_t res = a - b;
    res &= -(res <= a);
    return res;
  #endif
}

HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_MATH_H) */
/* :: End simde/simde-math.h :: */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/simde-constify.h :: */
/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2020      Evan Nemerson <evan@nemerson.com>
 */

/* Constify macros.  For internal use only.
 *
 * These are used to make it possible to call a function which takes
 * an Integer Constant Expression (ICE) using a compile time constant.
 * Technically it would also be possible to use a value not trivially
 * known by the compiler, but there would be a siginficant performance
 * hit (a switch switch is used).
 *
 * The basic idea is pretty simple; we just emit a do while loop which
 * contains a switch with a case for every possible value of the
 * constant.
 *
 * As long as the value you pass to the function in constant, pretty
 * much any copmiler shouldn't have a problem generating exactly the
 * same code as if you had used an ICE.
 *
 * This is intended to be used in the SIMDe implementations of
 * functions the compilers require to be an ICE, but the other benefit
 * is that if we also disable the warnings from
 * SIMDE_REQUIRE_CONSTANT_RANGE we can actually just allow the tests
 * to use non-ICE parameters
 */

#if !defined(SIMDE_CONSTIFY_H)
#define SIMDE_CONSTIFY_H

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DIAGNOSTIC_DISABLE_VARIADIC_MACROS_
SIMDE_DIAGNOSTIC_DISABLE_CPP98_COMPAT_PEDANTIC_

#define SIMDE_CONSTIFY_2_(func_name, result, default_case, imm, ...) \
  do { \
    switch(imm) { \
      case 0: result = func_name(__VA_ARGS__, 0); break; \
      case 1: result = func_name(__VA_ARGS__, 1); break; \
      default: result = default_case; break; \
    } \
  } while (0)

#define SIMDE_CONSTIFY_4_(func_name, result, default_case, imm, ...) \
  do { \
    switch(imm) { \
      case 0: result = func_name(__VA_ARGS__, 0); break; \
      case 1: result = func_name(__VA_ARGS__, 1); break; \
      case 2: result = func_name(__VA_ARGS__, 2); break; \
      case 3: result = func_name(__VA_ARGS__, 3); break; \
      default: result = default_case; break; \
    } \
  } while (0)

#define SIMDE_CONSTIFY_8_(func_name, result, default_case, imm, ...) \
  do { \
    switch(imm) { \
      case 0: result = func_name(__VA_ARGS__, 0); break; \
      case 1: result = func_name(__VA_ARGS__, 1); break; \
      case 2: result = func_name(__VA_ARGS__, 2); break; \
      case 3: result = func_name(__VA_ARGS__, 3); break; \
      case 4: result = func_name(__VA_ARGS__, 4); break; \
      case 5: result = func_name(__VA_ARGS__, 5); break; \
      case 6: result = func_name(__VA_ARGS__, 6); break; \
      case 7: result = func_name(__VA_ARGS__, 7); break; \
      default: result = default_case; break; \
    } \
  } while (0)

#define SIMDE_CONSTIFY_16_(func_name, result, default_case, imm, ...) \
  do { \
    switch(imm) { \
      case  0: result = func_name(__VA_ARGS__,  0); break; \
      case  1: result = func_name(__VA_ARGS__,  1); break; \
      case  2: result = func_name(__VA_ARGS__,  2); break; \
      case  3: result = func_name(__VA_ARGS__,  3); break; \
      case  4: result = func_name(__VA_ARGS__,  4); break; \
      case  5: result = func_name(__VA_ARGS__,  5); break; \
      case  6: result = func_name(__VA_ARGS__,  6); break; \
      case  7: result = func_name(__VA_ARGS__,  7); break; \
      case  8: result = func_name(__VA_ARGS__,  8); break; \
      case  9: result = func_name(__VA_ARGS__,  9); break; \
      case 10: result = func_name(__VA_ARGS__, 10); break; \
      case 11: result = func_name(__VA_ARGS__, 11); break; \
      case 12: result = func_name(__VA_ARGS__, 12); break; \
      case 13: result = func_name(__VA_ARGS__, 13); break; \
      case 14: result = func_name(__VA_ARGS__, 14); break; \
      case 15: result = func_name(__VA_ARGS__, 15); break; \
      default: result = default_case; break; \
    } \
  } while (0)

#define SIMDE_CONSTIFY_32_(func_name, result, default_case, imm, ...) \
  do { \
    switch(imm) { \
      case  0: result = func_name(__VA_ARGS__,  0); break; \
      case  1: result = func_name(__VA_ARGS__,  1); break; \
      case  2: result = func_name(__VA_ARGS__,  2); break; \
      case  3: result = func_name(__VA_ARGS__,  3); break; \
      case  4: result = func_name(__VA_ARGS__,  4); break; \
      case  5: result = func_name(__VA_ARGS__,  5); break; \
      case  6: result = func_name(__VA_ARGS__,  6); break; \
      case  7: result = func_name(__VA_ARGS__,  7); break; \
      case  8: result = func_name(__VA_ARGS__,  8); break; \
      case  9: result = func_name(__VA_ARGS__,  9); break; \
      case 10: result = func_name(__VA_ARGS__, 10); break; \
      case 11: result = func_name(__VA_ARGS__, 11); break; \
      case 12: result = func_name(__VA_ARGS__, 12); break; \
      case 13: result = func_name(__VA_ARGS__, 13); break; \
      case 14: result = func_name(__VA_ARGS__, 14); break; \
      case 15: result = func_name(__VA_ARGS__, 15); break; \
      case 16: result = func_name(__VA_ARGS__, 16); break; \
      case 17: result = func_name(__VA_ARGS__, 17); break; \
      case 18: result = func_name(__VA_ARGS__, 18); break; \
      case 19: result = func_name(__VA_ARGS__, 19); break; \
      case 20: result = func_name(__VA_ARGS__, 20); break; \
      case 21: result = func_name(__VA_ARGS__, 21); break; \
      case 22: result = func_name(__VA_ARGS__, 22); break; \
      case 23: result = func_name(__VA_ARGS__, 23); break; \
      case 24: result = func_name(__VA_ARGS__, 24); break; \
      case 25: result = func_name(__VA_ARGS__, 25); break; \
      case 26: result = func_name(__VA_ARGS__, 26); break; \
      case 27: result = func_name(__VA_ARGS__, 27); break; \
      case 28: result = func_name(__VA_ARGS__, 28); break; \
      case 29: result = func_name(__VA_ARGS__, 29); break; \
      case 30: result = func_name(__VA_ARGS__, 30); break; \
      case 31: result = func_name(__VA_ARGS__, 31); break; \
      default: result = default_case; break; \
    } \
  } while (0)

#define SIMDE_CONSTIFY_64_(func_name, result, default_case, imm, ...) \
  do { \
    switch(imm) { \
      case  0: result = func_name(__VA_ARGS__,  0); break; \
      case  1: result = func_name(__VA_ARGS__,  1); break; \
      case  2: result = func_name(__VA_ARGS__,  2); break; \
      case  3: result = func_name(__VA_ARGS__,  3); break; \
      case  4: result = func_name(__VA_ARGS__,  4); break; \
      case  5: result = func_name(__VA_ARGS__,  5); break; \
      case  6: result = func_name(__VA_ARGS__,  6); break; \
      case  7: result = func_name(__VA_ARGS__,  7); break; \
      case  8: result = func_name(__VA_ARGS__,  8); break; \
      case  9: result = func_name(__VA_ARGS__,  9); break; \
      case 10: result = func_name(__VA_ARGS__, 10); break; \
      case 11: result = func_name(__VA_ARGS__, 11); break; \
      case 12: result = func_name(__VA_ARGS__, 12); break; \
      case 13: result = func_name(__VA_ARGS__, 13); break; \
      case 14: result = func_name(__VA_ARGS__, 14); break; \
      case 15: result = func_name(__VA_ARGS__, 15); break; \
      case 16: result = func_name(__VA_ARGS__, 16); break; \
      case 17: result = func_name(__VA_ARGS__, 17); break; \
      case 18: result = func_name(__VA_ARGS__, 18); break; \
      case 19: result = func_name(__VA_ARGS__, 19); break; \
      case 20: result = func_name(__VA_ARGS__, 20); break; \
      case 21: result = func_name(__VA_ARGS__, 21); break; \
      case 22: result = func_name(__VA_ARGS__, 22); break; \
      case 23: result = func_name(__VA_ARGS__, 23); break; \
      case 24: result = func_name(__VA_ARGS__, 24); break; \
      case 25: result = func_name(__VA_ARGS__, 25); break; \
      case 26: result = func_name(__VA_ARGS__, 26); break; \
      case 27: result = func_name(__VA_ARGS__, 27); break; \
      case 28: result = func_name(__VA_ARGS__, 28); break; \
      case 29: result = func_name(__VA_ARGS__, 29); break; \
      case 30: result = func_name(__VA_ARGS__, 30); break; \
      case 31: result = func_name(__VA_ARGS__, 31); break; \
      case 32: result = func_name(__VA_ARGS__, 32); break; \
      case 33: result = func_name(__VA_ARGS__, 33); break; \
      case 34: result = func_name(__VA_ARGS__, 34); break; \
      case 35: result = func_name(__VA_ARGS__, 35); break; \
      case 36: result = func_name(__VA_ARGS__, 36); break; \
      case 37: result = func_name(__VA_ARGS__, 37); break; \
      case 38: result = func_name(__VA_ARGS__, 38); break; \
      case 39: result = func_name(__VA_ARGS__, 39); break; \
      case 40: result = func_name(__VA_ARGS__, 40); break; \
      case 41: result = func_name(__VA_ARGS__, 41); break; \
      case 42: result = func_name(__VA_ARGS__, 42); break; \
      case 43: result = func_name(__VA_ARGS__, 43); break; \
      case 44: result = func_name(__VA_ARGS__, 44); break; \
      case 45: result = func_name(__VA_ARGS__, 45); break; \
      case 46: result = func_name(__VA_ARGS__, 46); break; \
      case 47: result = func_name(__VA_ARGS__, 47); break; \
      case 48: result = func_name(__VA_ARGS__, 48); break; \
      case 49: result = func_name(__VA_ARGS__, 49); break; \
      case 50: result = func_name(__VA_ARGS__, 50); break; \
      case 51: result = func_name(__VA_ARGS__, 51); break; \
      case 52: result = func_name(__VA_ARGS__, 52); break; \
      case 53: result = func_name(__VA_ARGS__, 53); break; \
      case 54: result = func_name(__VA_ARGS__, 54); break; \
      case 55: result = func_name(__VA_ARGS__, 55); break; \
      case 56: result = func_name(__VA_ARGS__, 56); break; \
      case 57: result = func_name(__VA_ARGS__, 57); break; \
      case 58: result = func_name(__VA_ARGS__, 58); break; \
      case 59: result = func_name(__VA_ARGS__, 59); break; \
      case 60: result = func_name(__VA_ARGS__, 60); break; \
      case 61: result = func_name(__VA_ARGS__, 61); break; \
      case 62: result = func_name(__VA_ARGS__, 62); break; \
      case 63: result = func_name(__VA_ARGS__, 63); break; \
      default: result = default_case; break; \
    } \
  } while (0)

#define SIMDE_CONSTIFY_2_NO_RESULT_(func_name, default_case, imm, ...) \
  do { \
    switch(imm) { \
      case 0: func_name(__VA_ARGS__, 0); break; \
      case 1: func_name(__VA_ARGS__, 1); break; \
      default: default_case; break; \
    } \
  } while (0)

#define SIMDE_CONSTIFY_4_NO_RESULT_(func_name, default_case, imm, ...) \
  do { \
    switch(imm) { \
      case 0: func_name(__VA_ARGS__, 0); break; \
      case 1: func_name(__VA_ARGS__, 1); break; \
      case 2: func_name(__VA_ARGS__, 2); break; \
      case 3: func_name(__VA_ARGS__, 3); break; \
      default: default_case; break; \
    } \
  } while (0)

#define SIMDE_CONSTIFY_8_NO_RESULT_(func_name, default_case, imm, ...) \
  do { \
    switch(imm) { \
      case 0: func_name(__VA_ARGS__, 0); break; \
      case 1: func_name(__VA_ARGS__, 1); break; \
      case 2: func_name(__VA_ARGS__, 2); break; \
      case 3: func_name(__VA_ARGS__, 3); break; \
      case 4: func_name(__VA_ARGS__, 4); break; \
      case 5: func_name(__VA_ARGS__, 5); break; \
      case 6: func_name(__VA_ARGS__, 6); break; \
      case 7: func_name(__VA_ARGS__, 7); break; \
      default: default_case; break; \
    } \
  } while (0)

#define SIMDE_CONSTIFY_16_NO_RESULT_(func_name, default_case, imm, ...) \
  do { \
    switch(imm) { \
      case  0: func_name(__VA_ARGS__,  0); break; \
      case  1: func_name(__VA_ARGS__,  1); break; \
      case  2: func_name(__VA_ARGS__,  2); break; \
      case  3: func_name(__VA_ARGS__,  3); break; \
      case  4: func_name(__VA_ARGS__,  4); break; \
      case  5: func_name(__VA_ARGS__,  5); break; \
      case  6: func_name(__VA_ARGS__,  6); break; \
      case  7: func_name(__VA_ARGS__,  7); break; \
      case  8: func_name(__VA_ARGS__,  8); break; \
      case  9: func_name(__VA_ARGS__,  9); break; \
      case 10: func_name(__VA_ARGS__, 10); break; \
      case 11: func_name(__VA_ARGS__, 11); break; \
      case 12: func_name(__VA_ARGS__, 12); break; \
      case 13: func_name(__VA_ARGS__, 13); break; \
      case 14: func_name(__VA_ARGS__, 14); break; \
      case 15: func_name(__VA_ARGS__, 15); break; \
      default: default_case; break; \
    } \
  } while (0)

#define SIMDE_CONSTIFY_32_NO_RESULT_(func_name, default_case, imm, ...) \
  do { \
    switch(imm) { \
      case  0: func_name(__VA_ARGS__,  0); break; \
      case  1: func_name(__VA_ARGS__,  1); break; \
      case  2: func_name(__VA_ARGS__,  2); break; \
      case  3: func_name(__VA_ARGS__,  3); break; \
      case  4: func_name(__VA_ARGS__,  4); break; \
      case  5: func_name(__VA_ARGS__,  5); break; \
      case  6: func_name(__VA_ARGS__,  6); break; \
      case  7: func_name(__VA_ARGS__,  7); break; \
      case  8: func_name(__VA_ARGS__,  8); break; \
      case  9: func_name(__VA_ARGS__,  9); break; \
      case 10: func_name(__VA_ARGS__, 10); break; \
      case 11: func_name(__VA_ARGS__, 11); break; \
      case 12: func_name(__VA_ARGS__, 12); break; \
      case 13: func_name(__VA_ARGS__, 13); break; \
      case 14: func_name(__VA_ARGS__, 14); break; \
      case 15: func_name(__VA_ARGS__, 15); break; \
      case 16: func_name(__VA_ARGS__, 16); break; \
      case 17: func_name(__VA_ARGS__, 17); break; \
      case 18: func_name(__VA_ARGS__, 18); break; \
      case 19: func_name(__VA_ARGS__, 19); break; \
      case 20: func_name(__VA_ARGS__, 20); break; \
      case 21: func_name(__VA_ARGS__, 21); break; \
      case 22: func_name(__VA_ARGS__, 22); break; \
      case 23: func_name(__VA_ARGS__, 23); break; \
      case 24: func_name(__VA_ARGS__, 24); break; \
      case 25: func_name(__VA_ARGS__, 25); break; \
      case 26: func_name(__VA_ARGS__, 26); break; \
      case 27: func_name(__VA_ARGS__, 27); break; \
      case 28: func_name(__VA_ARGS__, 28); break; \
      case 29: func_name(__VA_ARGS__, 29); break; \
      case 30: func_name(__VA_ARGS__, 30); break; \
      case 31: func_name(__VA_ARGS__, 31); break; \
      default: default_case; break; \
    } \
  } while (0)

#define SIMDE_CONSTIFY_64_NO_RESULT_(func_name, default_case, imm, ...) \
  do { \
    switch(imm) { \
      case  0: func_name(__VA_ARGS__,  0); break; \
      case  1: func_name(__VA_ARGS__,  1); break; \
      case  2: func_name(__VA_ARGS__,  2); break; \
      case  3: func_name(__VA_ARGS__,  3); break; \
      case  4: func_name(__VA_ARGS__,  4); break; \
      case  5: func_name(__VA_ARGS__,  5); break; \
      case  6: func_name(__VA_ARGS__,  6); break; \
      case  7: func_name(__VA_ARGS__,  7); break; \
      case  8: func_name(__VA_ARGS__,  8); break; \
      case  9: func_name(__VA_ARGS__,  9); break; \
      case 10: func_name(__VA_ARGS__, 10); break; \
      case 11: func_name(__VA_ARGS__, 11); break; \
      case 12: func_name(__VA_ARGS__, 12); break; \
      case 13: func_name(__VA_ARGS__, 13); break; \
      case 14: func_name(__VA_ARGS__, 14); break; \
      case 15: func_name(__VA_ARGS__, 15); break; \
      case 16: func_name(__VA_ARGS__, 16); break; \
      case 17: func_name(__VA_ARGS__, 17); break; \
      case 18: func_name(__VA_ARGS__, 18); break; \
      case 19: func_name(__VA_ARGS__, 19); break; \
      case 20: func_name(__VA_ARGS__, 20); break; \
      case 21: func_name(__VA_ARGS__, 21); break; \
      case 22: func_name(__VA_ARGS__, 22); break; \
      case 23: func_name(__VA_ARGS__, 23); break; \
      case 24: func_name(__VA_ARGS__, 24); break; \
      case 25: func_name(__VA_ARGS__, 25); break; \
      case 26: func_name(__VA_ARGS__, 26); break; \
      case 27: func_name(__VA_ARGS__, 27); break; \
      case 28: func_name(__VA_ARGS__, 28); break; \
      case 29: func_name(__VA_ARGS__, 29); break; \
      case 30: func_name(__VA_ARGS__, 30); break; \
      case 31: func_name(__VA_ARGS__, 31); break; \
      case 32: func_name(__VA_ARGS__, 32); break; \
      case 33: func_name(__VA_ARGS__, 33); break; \
      case 34: func_name(__VA_ARGS__, 34); break; \
      case 35: func_name(__VA_ARGS__, 35); break; \
      case 36: func_name(__VA_ARGS__, 36); break; \
      case 37: func_name(__VA_ARGS__, 37); break; \
      case 38: func_name(__VA_ARGS__, 38); break; \
      case 39: func_name(__VA_ARGS__, 39); break; \
      case 40: func_name(__VA_ARGS__, 40); break; \
      case 41: func_name(__VA_ARGS__, 41); break; \
      case 42: func_name(__VA_ARGS__, 42); break; \
      case 43: func_name(__VA_ARGS__, 43); break; \
      case 44: func_name(__VA_ARGS__, 44); break; \
      case 45: func_name(__VA_ARGS__, 45); break; \
      case 46: func_name(__VA_ARGS__, 46); break; \
      case 47: func_name(__VA_ARGS__, 47); break; \
      case 48: func_name(__VA_ARGS__, 48); break; \
      case 49: func_name(__VA_ARGS__, 49); break; \
      case 50: func_name(__VA_ARGS__, 50); break; \
      case 51: func_name(__VA_ARGS__, 51); break; \
      case 52: func_name(__VA_ARGS__, 52); break; \
      case 53: func_name(__VA_ARGS__, 53); break; \
      case 54: func_name(__VA_ARGS__, 54); break; \
      case 55: func_name(__VA_ARGS__, 55); break; \
      case 56: func_name(__VA_ARGS__, 56); break; \
      case 57: func_name(__VA_ARGS__, 57); break; \
      case 58: func_name(__VA_ARGS__, 58); break; \
      case 59: func_name(__VA_ARGS__, 59); break; \
      case 60: func_name(__VA_ARGS__, 60); break; \
      case 61: func_name(__VA_ARGS__, 61); break; \
      case 62: func_name(__VA_ARGS__, 62); break; \
      case 63: func_name(__VA_ARGS__, 63); break; \
      default: default_case; break; \
    } \
  } while (0)

HEDLEY_DIAGNOSTIC_POP

#endif
/* :: End simde/simde-constify.h :: */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/simde-align.h :: */
/* Alignment
 * Created by Evan Nemerson <evan@nemerson.com>
 *
 *   To the extent possible under law, the authors have waived all
 *   copyright and related or neighboring rights to this code.  For
 *   details, see the Creative Commons Zero 1.0 Universal license at
 *   <https://creativecommons.org/publicdomain/zero/1.0/>
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 **********************************************************************
 *
 * This is portability layer which should help iron out some
 * differences across various compilers, as well as various versions of
 * C and C++.
 *
 * It was originally developed for SIMD Everywhere
 * (<https://github.com/simd-everywhere/simde>), but since its only
 * dependency is Hedley (<https://nemequ.github.io/hedley>, also CC0)
 * it can easily be used in other projects, so please feel free to do
 * so.
 *
 * If you do use this in your project, please keep a link to SIMDe in
 * your code to remind you where to report any bugs and/or check for
 * updated versions.
 *
 * # API Overview
 *
 * The API has several parts, and most macros have a few variations.
 * There are APIs for declaring aligned fields/variables, optimization
 * hints, and run-time alignment checks.
 *
 * Briefly, macros ending with "_TO" take numeric values and are great
 * when you know the value you would like to use.  Macros ending with
 * "_LIKE", on the other hand, accept a type and are used when you want
 * to use the alignment of a type instead of hardcoding a value.
 *
 * Documentation for each section of the API is inline.
 *
 * True to form, MSVC is the main problem and imposes several
 * limitations on the effectiveness of the APIs.  Detailed descriptions
 * of the limitations of each macro are inline, but in general:
 *
 *  * On C11+ or C++11+ code written using this API will work.  The
 *    ASSUME macros may or may not generate a hint to the compiler, but
 *    that is only an optimization issue and will not actually cause
 *    failures.
 *  * If you're using pretty much any compiler other than MSVC,
 *    everything should basically work as well as in C11/C++11.
 */

#if !defined(SIMDE_ALIGN_H)
#define SIMDE_ALIGN_H

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */

/* I know this seems a little silly, but some non-hosted compilers
 * don't have stddef.h, so we try to accommodate them. */
#if !defined(SIMDE_ALIGN_SIZE_T_)
  #if defined(__SIZE_TYPE__)
    #define SIMDE_ALIGN_SIZE_T_ __SIZE_TYPE__
  #elif defined(__SIZE_T_TYPE__)
    #define SIMDE_ALIGN_SIZE_T_ __SIZE_TYPE__
  #elif defined(__cplusplus)
    #include <cstddef>
    #define SIMDE_ALIGN_SIZE_T_ size_t
  #else
    #include <stddef.h>
    #define SIMDE_ALIGN_SIZE_T_ size_t
  #endif
#endif

#if !defined(SIMDE_ALIGN_INTPTR_T_)
  #if defined(__INTPTR_TYPE__)
    #define SIMDE_ALIGN_INTPTR_T_ __INTPTR_TYPE__
  #elif defined(__PTRDIFF_TYPE__)
    #define SIMDE_ALIGN_INTPTR_T_ __PTRDIFF_TYPE__
  #elif defined(__PTRDIFF_T_TYPE__)
    #define SIMDE_ALIGN_INTPTR_T_ __PTRDIFF_T_TYPE__
  #elif defined(__cplusplus)
    #include <cstddef>
    #define SIMDE_ALIGN_INTPTR_T_ ptrdiff_t
  #else
    #include <stddef.h>
    #define SIMDE_ALIGN_INTPTR_T_ ptrdiff_t
  #endif
#endif

#if defined(SIMDE_ALIGN_DEBUG)
  #if defined(__cplusplus)
    #include <cstdio>
  #else
    #include <stdio.h>
  #endif
#endif

/* SIMDE_ALIGN_OF(Type)
 *
 * The SIMDE_ALIGN_OF macro works like alignof, or _Alignof, or
 * __alignof, or __alignof__, or __ALIGNOF__, depending on the compiler.
 * It isn't defined everywhere (only when the compiler has some alignof-
 * like feature we can use to implement it), but it should work in most
 * modern compilers, as well as C11 and C++11.
 *
 * If we can't find an implementation for SIMDE_ALIGN_OF then the macro
 * will not be defined, so if you can handle that situation sensibly
 * you may need to sprinkle some ifdefs into your code.
 */
#if \
    (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)) || \
    (0 && HEDLEY_HAS_FEATURE(c_alignof))
  #define SIMDE_ALIGN_OF(Type) _Alignof(Type)
#elif \
    (defined(__cplusplus) && (__cplusplus >= 201103L)) || \
    (0 && HEDLEY_HAS_FEATURE(cxx_alignof))
  #define SIMDE_ALIGN_OF(Type) alignof(Type)
#elif \
    HEDLEY_GCC_VERSION_CHECK(2,95,0) || \
    HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    HEDLEY_SUNPRO_VERSION_CHECK(5,13,0) || \
    HEDLEY_TINYC_VERSION_CHECK(0,9,24) || \
    HEDLEY_PGI_VERSION_CHECK(19,10,0) || \
    HEDLEY_CRAY_VERSION_CHECK(10,0,0) || \
    HEDLEY_TI_ARMCL_VERSION_CHECK(16,9,0) || \
    HEDLEY_TI_CL2000_VERSION_CHECK(16,9,0) || \
    HEDLEY_TI_CL6X_VERSION_CHECK(8,0,0) || \
    HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    HEDLEY_TI_CL430_VERSION_CHECK(16,9,0) || \
    HEDLEY_TI_CLPRU_VERSION_CHECK(2,3,2) || \
    HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10) || \
    defined(__IBM__ALIGNOF__) || \
    defined(__clang__)
  #define SIMDE_ALIGN_OF(Type) __alignof__(Type)
#elif \
  HEDLEY_IAR_VERSION_CHECK(8,40,0)
  #define SIMDE_ALIGN_OF(Type) __ALIGNOF__(Type)
#elif \
  HEDLEY_MSVC_VERSION_CHECK(19,0,0)
  /* Probably goes back much further, but MS takes down their old docs.
   * If you can verify that this works in earlier versions please let
   * me know! */
  #define SIMDE_ALIGN_OF(Type) __alignof(Type)
#endif

/* SIMDE_ALIGN_MAXIMUM:
 *
 * This is the maximum alignment that the compiler supports.  You can
 * define the value prior to including SIMDe if necessary, but in that
 * case *please* submit an issue so we can add the platform to the
 * detection code.
 *
 * Most compilers are okay with types which are aligned beyond what
 * they think is the maximum, as long as the alignment is a power
 * of two.  Older versions of MSVC is the exception, so we need to cap
 * the alignment requests at values that the implementation supports.
 *
 * XL C/C++ will accept values larger than 16 (which is the alignment
 * of an AltiVec vector), but will not reliably align to the larger
 * value, so so we cap the value at 16 there.
 *
 * If the compiler accepts any power-of-two value within reason then
 * this macro should be left undefined, and the SIMDE_ALIGN_CAP
 * macro will just return the value passed to it. */
#if !defined(SIMDE_ALIGN_MAXIMUM)
  #if defined(HEDLEY_MSVC_VERSION)
    #if HEDLEY_MSVC_VERSION_CHECK(19, 16, 0)
      // Visual studio 2017 and newer does not need a max
    #else
      #if defined(_M_IX86) || defined(_M_AMD64)
        #if HEDLEY_MSVC_VERSION_CHECK(19,14,0)
          #define SIMDE_ALIGN_PLATFORM_MAXIMUM 64
        #elif HEDLEY_MSVC_VERSION_CHECK(16,0,0)
          /* VS 2010 is really a guess based on Wikipedia; if anyone can
           * test with old VS versions I'd really appreciate it. */
          #define SIMDE_ALIGN_PLATFORM_MAXIMUM 32
        #else
          #define SIMDE_ALIGN_PLATFORM_MAXIMUM 16
        #endif
      #elif defined(_M_ARM) || defined(_M_ARM64)
        #define SIMDE_ALIGN_PLATFORM_MAXIMUM 8
      #endif
    #endif
  #elif defined(HEDLEY_IBM_VERSION)
    #define SIMDE_ALIGN_PLATFORM_MAXIMUM 16
  #endif
#endif

/* You can mostly ignore these; they're intended for internal use.
 * If you do need to use them please let me know; if they fulfill
 * a common use case I'll probably drop the trailing underscore
 * and make them part of the public API. */
#if defined(SIMDE_ALIGN_PLATFORM_MAXIMUM)
  #if SIMDE_ALIGN_PLATFORM_MAXIMUM >= 64
    #define SIMDE_ALIGN_64_ 64
    #define SIMDE_ALIGN_32_ 32
    #define SIMDE_ALIGN_16_ 16
    #define SIMDE_ALIGN_8_ 8
  #elif SIMDE_ALIGN_PLATFORM_MAXIMUM >= 32
    #define SIMDE_ALIGN_64_ 32
    #define SIMDE_ALIGN_32_ 32
    #define SIMDE_ALIGN_16_ 16
    #define SIMDE_ALIGN_8_ 8
  #elif SIMDE_ALIGN_PLATFORM_MAXIMUM >= 16
    #define SIMDE_ALIGN_64_ 16
    #define SIMDE_ALIGN_32_ 16
    #define SIMDE_ALIGN_16_ 16
    #define SIMDE_ALIGN_8_ 8
  #elif SIMDE_ALIGN_PLATFORM_MAXIMUM >= 8
    #define SIMDE_ALIGN_64_ 8
    #define SIMDE_ALIGN_32_ 8
    #define SIMDE_ALIGN_16_ 8
    #define SIMDE_ALIGN_8_ 8
  #else
    #error Max alignment expected to be >= 8
  #endif
#else
  #define SIMDE_ALIGN_64_ 64
  #define SIMDE_ALIGN_32_ 32
  #define SIMDE_ALIGN_16_ 16
  #define SIMDE_ALIGN_8_ 8
#endif

/**
 * SIMDE_ALIGN_CAP(Alignment)
 *
 * Returns the minimum of Alignment or SIMDE_ALIGN_MAXIMUM.
 */
#if defined(SIMDE_ALIGN_MAXIMUM)
  #define SIMDE_ALIGN_CAP(Alignment) (((Alignment) < (SIMDE_ALIGN_PLATFORM_MAXIMUM)) ? (Alignment) : (SIMDE_ALIGN_PLATFORM_MAXIMUM))
#else
  #define SIMDE_ALIGN_CAP(Alignment) (Alignment)
#endif

/* SIMDE_ALIGN_TO(Alignment)
 *
 * SIMDE_ALIGN_TO is used to declare types or variables.  It basically
 * maps to the align attribute in most compilers, the align declspec
 * in MSVC, or _Alignas/alignas in C11/C++11.
 *
 * Example:
 *
 *   struct i32x4 {
 *     SIMDE_ALIGN_TO(16) int32_t values[4];
 *   }
 *
 * Limitations:
 *
 * MSVC requires that the Alignment parameter be numeric; you can't do
 * something like `SIMDE_ALIGN_TO(SIMDE_ALIGN_OF(int))`.  This is
 * unfortunate because that's really how the LIKE macros are
 * implemented, and I am not aware of a way to get anything like this
 * to work without using the C11/C++11 keywords.
 *
 * It also means that we can't use SIMDE_ALIGN_CAP to limit the
 * alignment to the value specified, which MSVC also requires, so on
 * MSVC you should use the `SIMDE_ALIGN_TO_8/16/32/64` macros instead.
 * They work like `SIMDE_ALIGN_TO(SIMDE_ALIGN_CAP(Alignment))` would,
 * but should be safe to use on MSVC.
 *
 * All this is to say that, if you want your code to work on MSVC, you
 * should use the SIMDE_ALIGN_TO_8/16/32/64 macros below instead of
 * SIMDE_ALIGN_TO(8/16/32/64).
 */
#if \
    HEDLEY_HAS_ATTRIBUTE(aligned) || \
    HEDLEY_GCC_VERSION_CHECK(2,95,0) || \
    HEDLEY_CRAY_VERSION_CHECK(8,4,0) || \
    HEDLEY_IBM_VERSION_CHECK(11,1,0) || \
    HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    HEDLEY_PGI_VERSION_CHECK(19,4,0) || \
    HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    HEDLEY_TINYC_VERSION_CHECK(0,9,24) || \
    HEDLEY_TI_ARMCL_VERSION_CHECK(16,9,0) || \
    HEDLEY_TI_CL2000_VERSION_CHECK(16,9,0) || \
    HEDLEY_TI_CL6X_VERSION_CHECK(8,0,0) || \
    HEDLEY_TI_CL7X_VERSION_CHECK(1,2,0) || \
    HEDLEY_TI_CL430_VERSION_CHECK(16,9,0) || \
    HEDLEY_TI_CLPRU_VERSION_CHECK(2,3,2)
  #define SIMDE_ALIGN_TO(Alignment) __attribute__((__aligned__(SIMDE_ALIGN_CAP(Alignment))))
#elif \
    (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L))
  #define SIMDE_ALIGN_TO(Alignment) _Alignas(SIMDE_ALIGN_CAP(Alignment))
#elif \
    (defined(__cplusplus) && (__cplusplus >= 201103L))
  #define SIMDE_ALIGN_TO(Alignment) alignas(SIMDE_ALIGN_CAP(Alignment))
#elif \
    defined(HEDLEY_MSVC_VERSION)
  #define SIMDE_ALIGN_TO(Alignment) __declspec(align(Alignment))
  /* Unfortunately MSVC can't handle __declspec(align(__alignof(Type)));
   * the alignment passed to the declspec has to be an integer. */
  #define SIMDE_ALIGN_OF_UNUSABLE_FOR_LIKE
#endif
#define SIMDE_ALIGN_TO_64 SIMDE_ALIGN_TO(SIMDE_ALIGN_64_)
#define SIMDE_ALIGN_TO_32 SIMDE_ALIGN_TO(SIMDE_ALIGN_32_)
#define SIMDE_ALIGN_TO_16 SIMDE_ALIGN_TO(SIMDE_ALIGN_16_)
#define SIMDE_ALIGN_TO_8 SIMDE_ALIGN_TO(SIMDE_ALIGN_8_)

/* SIMDE_ALIGN_ASSUME_TO(Pointer, Alignment)
 *
 * SIMDE_ALIGN_ASSUME_TO is semantically similar to C++20's
 * std::assume_aligned, or __builtin_assume_aligned.  It tells the
 * compiler to assume that the provided pointer is aligned to an
 * `Alignment`-byte boundary.
 *
 * If you define SIMDE_ALIGN_DEBUG prior to including this header then
 * SIMDE_ALIGN_ASSUME_TO will turn into a runtime check.   We don't
 * integrate with NDEBUG in this header, but it may be a good idea to
 * put something like this in your code:
 *
 *   #if !defined(NDEBUG)
 *     #define SIMDE_ALIGN_DEBUG
 *   #endif
 *   #include <.../simde-align.h>
 */
#if \
    HEDLEY_HAS_BUILTIN(__builtin_assume_aligned) || \
    HEDLEY_GCC_VERSION_CHECK(4,7,0)
  #define SIMDE_ALIGN_ASSUME_TO_UNCHECKED(Pointer, Alignment) \
    HEDLEY_REINTERPRET_CAST(__typeof__(Pointer), __builtin_assume_aligned(HEDLEY_CONST_CAST(void*, HEDLEY_REINTERPRET_CAST(const void*, Pointer)), Alignment))
#elif HEDLEY_INTEL_VERSION_CHECK(13,0,0)
  #define SIMDE_ALIGN_ASSUME_TO_UNCHECKED(Pointer, Alignment) (__extension__ ({ \
      __typeof__(v) simde_assume_aligned_t_ = (Pointer); \
      __assume_aligned(simde_assume_aligned_t_, Alignment); \
      simde_assume_aligned_t_; \
    }))
#elif defined(__cplusplus) && (__cplusplus > 201703L)
  #include <memory>
  #define SIMDE_ALIGN_ASSUME_TO_UNCHECKED(Pointer, Alignment) std::assume_aligned<Alignment>(Pointer)
#else
  #if defined(__cplusplus)
    template<typename T> HEDLEY_ALWAYS_INLINE static T* simde_align_assume_to_unchecked(T* ptr, const size_t alignment)
  #else
    HEDLEY_ALWAYS_INLINE static void* simde_align_assume_to_unchecked(void* ptr, const size_t alignment)
  #endif
  {
    HEDLEY_ASSUME((HEDLEY_REINTERPRET_CAST(size_t, (ptr)) % SIMDE_ALIGN_CAP(alignment)) == 0);
    return ptr;
  }
  #if defined(__cplusplus)
    #define SIMDE_ALIGN_ASSUME_TO_UNCHECKED(Pointer, Alignment) simde_align_assume_to_unchecked((Pointer), (Alignment))
  #else
    #define SIMDE_ALIGN_ASSUME_TO_UNCHECKED(Pointer, Alignment) simde_align_assume_to_unchecked(HEDLEY_CONST_CAST(void*, HEDLEY_REINTERPRET_CAST(const void*, Pointer)), (Alignment))
  #endif
#endif

#if !defined(SIMDE_ALIGN_DEBUG)
  #define SIMDE_ALIGN_ASSUME_TO(Pointer, Alignment) SIMDE_ALIGN_ASSUME_TO_UNCHECKED(Pointer, Alignment)
#else
  #include <stdio.h>
  #if defined(__cplusplus)
    template<typename T>
    static HEDLEY_ALWAYS_INLINE
    T*
    simde_align_assume_to_checked_uncapped(T* ptr, const size_t alignment, const char* file, int line, const char* ptrname)
  #else
    static HEDLEY_ALWAYS_INLINE
    void*
    simde_align_assume_to_checked_uncapped(void* ptr, const size_t alignment, const char* file, int line, const char* ptrname)
  #endif
  {
    if (HEDLEY_UNLIKELY((HEDLEY_REINTERPRET_CAST(SIMDE_ALIGN_INTPTR_T_, (ptr)) % HEDLEY_STATIC_CAST(SIMDE_ALIGN_INTPTR_T_, SIMDE_ALIGN_CAP(alignment))) != 0)) {
      fprintf(stderr, "%s:%d: alignment check failed for `%s' (%p %% %u == %u)\n",
        file, line, ptrname, HEDLEY_REINTERPRET_CAST(const void*, ptr),
        HEDLEY_STATIC_CAST(unsigned int, SIMDE_ALIGN_CAP(alignment)),
        HEDLEY_STATIC_CAST(unsigned int, HEDLEY_REINTERPRET_CAST(SIMDE_ALIGN_INTPTR_T_, (ptr)) % HEDLEY_STATIC_CAST(SIMDE_ALIGN_INTPTR_T_, SIMDE_ALIGN_CAP(alignment))));
    }

    return ptr;
  }

  #if defined(__cplusplus)
    #define SIMDE_ALIGN_ASSUME_TO(Pointer, Alignment) simde_align_assume_to_checked_uncapped((Pointer), (Alignment), __FILE__, __LINE__, #Pointer)
  #else
    #define SIMDE_ALIGN_ASSUME_TO(Pointer, Alignment) simde_align_assume_to_checked_uncapped(HEDLEY_CONST_CAST(void*, HEDLEY_REINTERPRET_CAST(const void*, Pointer)), (Alignment), __FILE__, __LINE__, #Pointer)
  #endif
#endif

/* SIMDE_ALIGN_LIKE(Type)
 * SIMDE_ALIGN_LIKE_#(Type)
 *
 * The SIMDE_ALIGN_LIKE macros are similar to the SIMDE_ALIGN_TO macros
 * except instead of an integer they take a type; basically, it's just
 * a more convenient way to do something like:
 *
 *   SIMDE_ALIGN_TO(SIMDE_ALIGN_OF(Type))
 *
 * The versions with a numeric suffix will fall back on using a numeric
 * value in the event we can't use SIMDE_ALIGN_OF(Type).  This is
 * mainly for MSVC, where __declspec(align()) can't handle anything
 * other than hard-coded numeric values.
 */
#if defined(SIMDE_ALIGN_OF) && defined(SIMDE_ALIGN_TO) && !defined(SIMDE_ALIGN_OF_UNUSABLE_FOR_LIKE)
  #define SIMDE_ALIGN_LIKE(Type) SIMDE_ALIGN_TO(SIMDE_ALIGN_OF(Type))
  #define SIMDE_ALIGN_LIKE_64(Type) SIMDE_ALIGN_LIKE(Type)
  #define SIMDE_ALIGN_LIKE_32(Type) SIMDE_ALIGN_LIKE(Type)
  #define SIMDE_ALIGN_LIKE_16(Type) SIMDE_ALIGN_LIKE(Type)
  #define SIMDE_ALIGN_LIKE_8(Type) SIMDE_ALIGN_LIKE(Type)
#else
  #define SIMDE_ALIGN_LIKE_64(Type) SIMDE_ALIGN_TO_64
  #define SIMDE_ALIGN_LIKE_32(Type) SIMDE_ALIGN_TO_32
  #define SIMDE_ALIGN_LIKE_16(Type) SIMDE_ALIGN_TO_16
  #define SIMDE_ALIGN_LIKE_8(Type) SIMDE_ALIGN_TO_8
#endif

/* SIMDE_ALIGN_ASSUME_LIKE(Pointer, Type)
 *
 * This is similar to SIMDE_ALIGN_ASSUME_TO, except that it takes a
 * type instead of a numeric value. */
#if defined(SIMDE_ALIGN_OF) && defined(SIMDE_ALIGN_ASSUME_TO)
  #define SIMDE_ALIGN_ASSUME_LIKE(Pointer, Type) SIMDE_ALIGN_ASSUME_TO(Pointer, SIMDE_ALIGN_OF(Type))
#endif

/* SIMDE_ALIGN_CAST(Type, Pointer)
 *
 * SIMDE_ALIGN_CAST is like C++'s reinterpret_cast, but it will try
 * to silence warnings that some compilers may produce if you try
 * to assign to a type with increased alignment requirements.
 *
 * Note that it does *not* actually attempt to tell the compiler that
 * the pointer is aligned like the destination should be; that's the
 * job of the next macro.  This macro is necessary for stupid APIs
 * like _mm_loadu_si128 where the input is a __m128i* but the function
 * is specifically for data which isn't necessarily aligned to
 * _Alignof(__m128i).
 */
#if HEDLEY_HAS_WARNING("-Wcast-align") || defined(__clang__) || HEDLEY_GCC_VERSION_CHECK(3,4,0)
  #define SIMDE_ALIGN_CAST(Type, Pointer) (__extension__({ \
      HEDLEY_DIAGNOSTIC_PUSH \
      _Pragma("GCC diagnostic ignored \"-Wcast-align\"") \
      Type simde_r_ = HEDLEY_REINTERPRET_CAST(Type, Pointer); \
      HEDLEY_DIAGNOSTIC_POP \
      simde_r_; \
    }))
#else
  #define SIMDE_ALIGN_CAST(Type, Pointer) HEDLEY_REINTERPRET_CAST(Type, Pointer)
#endif

/* SIMDE_ALIGN_ASSUME_CAST(Type, Pointer)
 *
 * This is sort of like a combination of a reinterpret_cast and a
 * SIMDE_ALIGN_ASSUME_LIKE.  It uses SIMDE_ALIGN_ASSUME_LIKE to tell
 * the compiler that the pointer is aligned like the specified type
 * and casts the pointer to the specified type while suppressing any
 * warnings from the compiler about casting to a type with greater
 * alignment requirements.
 */
#define SIMDE_ALIGN_ASSUME_CAST(Type, Pointer) SIMDE_ALIGN_ASSUME_LIKE(SIMDE_ALIGN_CAST(Type, Pointer), Type)

#endif /* !defined(SIMDE_ALIGN_H) */
/* :: End simde/simde-align.h :: */

/* In some situations, SIMDe has to make large performance sacrifices
 * for small increases in how faithfully it reproduces an API, but
 * only a relatively small number of users will actually need the API
 * to be completely accurate.  The SIMDE_FAST_* options can be used to
 * disable these trade-offs.
 *
 * They can be enabled by passing -DSIMDE_FAST_MATH to the compiler, or
 * the individual defines (e.g., -DSIMDE_FAST_NANS) if you only want to
 * enable some optimizations.  Using -ffast-math and/or
 * -ffinite-math-only will also enable the relevant options.  If you
 * don't want that you can pass -DSIMDE_NO_FAST_* to disable them. */

/* Most programs avoid NaNs by never passing values which can result in
 * a NaN; for example, if you only pass non-negative values to the sqrt
 * functions, it won't generate a NaN.  On some platforms, similar
 * functions handle NaNs differently; for example, the _mm_min_ps SSE
 * function will return 0.0 if you pass it (0.0, NaN), but the NEON
 * vminq_f32 function will return NaN.  Making them behave like one
 * another is expensive; it requires generating a mask of all lanes
 * with NaNs, then performing the operation (e.g., vminq_f32), then
 * blending together the result with another vector using the mask.
 *
 * If you don't want SIMDe to worry about the differences between how
 * NaNs are handled on the two platforms, define this (or pass
 * -ffinite-math-only) */
#if !defined(SIMDE_FAST_MATH) && !defined(SIMDE_NO_FAST_MATH) && defined(__FAST_MATH__)
  #define SIMDE_FAST_MATH
#endif

#if !defined(SIMDE_FAST_NANS) && !defined(SIMDE_NO_FAST_NANS)
  #if defined(SIMDE_FAST_MATH)
    #define SIMDE_FAST_NANS
  #elif defined(__FINITE_MATH_ONLY__)
    #if __FINITE_MATH_ONLY__
      #define SIMDE_FAST_NANS
    #endif
  #endif
#endif

/* Many functions are defined as using the current rounding mode
 * (i.e., the SIMD version of fegetround()) when converting to
 * an integer.  For example, _mm_cvtpd_epi32.  Unfortunately,
 * on some platforms (such as ARMv8+ where round-to-nearest is
 * always used, regardless of the FPSCR register) this means we
 * have to first query the current rounding mode, then choose
 * the proper function (rounnd
 , ceil, floor, etc.) */
#if !defined(SIMDE_FAST_ROUND_MODE) && !defined(SIMDE_NO_FAST_ROUND_MODE) && defined(SIMDE_FAST_MATH)
  #define SIMDE_FAST_ROUND_MODE
#endif

/* This controls how ties are rounded.  For example, does 10.5 round to
 * 10 or 11?  IEEE 754 specifies round-towards-even, but ARMv7 (for
 * example) doesn't support it and it must be emulated (which is rather
 * slow).  If you're okay with just using the default for whatever arch
 * you're on, you should definitely define this.
 *
 * Note that we don't use this macro to avoid correct implementations
 * in functions which are explicitly about rounding (such as vrnd* on
 * NEON, _mm_round_* on x86, etc.); it is only used for code where
 * rounding is a component in another function, and even then it isn't
 * usually a problem since such functions will use the current rounding
 * mode. */
#if !defined(SIMDE_FAST_ROUND_TIES) && !defined(SIMDE_NO_FAST_ROUND_TIES) && defined(SIMDE_FAST_MATH)
  #define SIMDE_FAST_ROUND_TIES
#endif

/* For functions which convert from one type to another (mostly from
 * floating point to integer types), sometimes we need to do a range
 * check and potentially return a different result if the value
 * falls outside that range.  Skipping this check can provide a
 * performance boost, at the expense of faithfulness to the API we're
 * emulating. */
#if !defined(SIMDE_FAST_CONVERSION_RANGE) && !defined(SIMDE_NO_FAST_CONVERSION_RANGE) && defined(SIMDE_FAST_MATH)
  #define SIMDE_FAST_CONVERSION_RANGE
#endif

/* Due to differences across platforms, sometimes it can be much
 * faster for us to allow spurious floating point exceptions,
 * or to no generate them when we should. */
#if !defined(SIMDE_FAST_EXCEPTIONS) && !defined(SIMDE_NO_FAST_EXCEPTIONS) && defined(SIMDE_FAST_MATH)
  #define SIMDE_FAST_EXCEPTIONS
#endif

#if \
    HEDLEY_HAS_BUILTIN(__builtin_constant_p) || \
    HEDLEY_GCC_VERSION_CHECK(3,4,0) || \
    HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    HEDLEY_TINYC_VERSION_CHECK(0,9,19) || \
    HEDLEY_ARM_VERSION_CHECK(4,1,0) || \
    HEDLEY_IBM_VERSION_CHECK(13,1,0) || \
    HEDLEY_TI_CL6X_VERSION_CHECK(6,1,0) || \
    (HEDLEY_SUNPRO_VERSION_CHECK(5,10,0) && !defined(__cplusplus)) || \
    HEDLEY_CRAY_VERSION_CHECK(8,1,0) || \
    HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
  #define SIMDE_CHECK_CONSTANT_(expr) (__builtin_constant_p(expr))
#elif defined(__cplusplus) && (__cplusplus > 201703L)
  #include <type_traits>
  #define SIMDE_CHECK_CONSTANT_(expr) (std::is_constant_evaluated())
#endif

#if !defined(SIMDE_NO_CHECK_IMMEDIATE_CONSTANT)
  #if defined(SIMDE_CHECK_CONSTANT_) && \
      SIMDE_DETECT_CLANG_VERSION_CHECK(9,0,0) && \
      (!defined(__apple_build_version__) || ((__apple_build_version__ < 11000000) || (__apple_build_version__ >= 12000000)))
    #define SIMDE_REQUIRE_CONSTANT(arg) HEDLEY_REQUIRE_MSG(SIMDE_CHECK_CONSTANT_(arg), "`" #arg "' must be constant")
  #else
    #define SIMDE_REQUIRE_CONSTANT(arg)
  #endif
#else
  #define SIMDE_REQUIRE_CONSTANT(arg)
#endif

#define SIMDE_REQUIRE_RANGE(arg, min, max) \
  HEDLEY_REQUIRE_MSG((((arg) >= (min)) && ((arg) <= (max))), "'" #arg "' must be in [" #min ", " #max "]")

#define SIMDE_REQUIRE_CONSTANT_RANGE(arg, min, max) \
  SIMDE_REQUIRE_CONSTANT(arg) \
  SIMDE_REQUIRE_RANGE(arg, min, max)

/* A copy of HEDLEY_STATIC_ASSERT, except we don't define an empty
 * fallback if we can't find an implementation; instead we have to
 * check if SIMDE_STATIC_ASSERT is defined before using it. */
#if \
  !defined(__cplusplus) && ( \
      (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)) || \
      HEDLEY_HAS_FEATURE(c_static_assert) || \
      HEDLEY_GCC_VERSION_CHECK(6,0,0) || \
      HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
      defined(_Static_assert) \
    )
  /* Sometimes _Static_assert is defined (in cdefs.h) using a symbol which
   * starts with a double-underscore. This is a system header so we have no
   * control over it, but since it's a macro it will emit a diagnostic which
   * prevents compilation with -Werror. */
  #if HEDLEY_HAS_WARNING("-Wreserved-identifier")
    #define SIMDE_STATIC_ASSERT(expr, message) (__extension__({ \
      HEDLEY_DIAGNOSTIC_PUSH \
      _Pragma("clang diagnostic ignored \"-Wreserved-identifier\"") \
      _Static_assert(expr, message); \
      HEDLEY_DIAGNOSTIC_POP \
    }))
  #else
    #define SIMDE_STATIC_ASSERT(expr, message) _Static_assert(expr, message)
  #endif
#elif \
  (defined(__cplusplus) && (__cplusplus >= 201103L)) || \
  HEDLEY_MSVC_VERSION_CHECK(16,0,0)
  #define SIMDE_STATIC_ASSERT(expr, message) HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(static_assert(expr, message))
#endif

/* Statement exprs */
#if \
    HEDLEY_GNUC_VERSION_CHECK(2,95,0) || \
    HEDLEY_TINYC_VERSION_CHECK(0,9,26) || \
    HEDLEY_INTEL_VERSION_CHECK(9,0,0) || \
    HEDLEY_PGI_VERSION_CHECK(18,10,0) || \
    HEDLEY_SUNPRO_VERSION_CHECK(5,12,0) || \
    HEDLEY_IBM_VERSION_CHECK(11,1,0) || \
    HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
  #define SIMDE_STATEMENT_EXPR_(expr) (__extension__ expr)
#endif

/* This is just a convenience macro to make it easy to call a single
 * function with a specific diagnostic disabled. */
#if defined(SIMDE_STATEMENT_EXPR_)
  #define SIMDE_DISABLE_DIAGNOSTIC_EXPR_(diagnostic, expr) \
    SIMDE_STATEMENT_EXPR_(({ \
      HEDLEY_DIAGNOSTIC_PUSH \
      diagnostic \
      (expr); \
      HEDLEY_DIAGNOSTIC_POP \
    }))
#endif

#if defined(SIMDE_CHECK_CONSTANT_) && defined(SIMDE_STATIC_ASSERT)
  #define SIMDE_ASSERT_CONSTANT_(v) SIMDE_STATIC_ASSERT(SIMDE_CHECK_CONSTANT_(v), #v " must be constant.")
#endif

#if \
  (HEDLEY_HAS_ATTRIBUTE(may_alias) && !defined(HEDLEY_SUNPRO_VERSION)) || \
  HEDLEY_GCC_VERSION_CHECK(3,3,0) || \
  HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
  HEDLEY_IBM_VERSION_CHECK(13,1,0)
#  define SIMDE_MAY_ALIAS __attribute__((__may_alias__))
#else
#  define SIMDE_MAY_ALIAS
#endif

/*  Lots of compilers support GCC-style vector extensions, but many
    don't support all the features.  Define different macros depending
    on support for

    * SIMDE_VECTOR - Declaring a vector.
    * SIMDE_VECTOR_OPS - basic operations (binary and unary).
    * SIMDE_VECTOR_NEGATE - negating a vector
    * SIMDE_VECTOR_SCALAR - For binary operators, the second argument
        can be a scalar, in which case the result is as if that scalar
        had been broadcast to all lanes of a vector.
    * SIMDE_VECTOR_SUBSCRIPT - Supports array subscript notation for
        extracting/inserting a single element.=

    SIMDE_VECTOR can be assumed if any others are defined, the
    others are independent. */
#if !defined(SIMDE_NO_VECTOR)
#  if \
    HEDLEY_GCC_VERSION_CHECK(4,8,0)
#    define SIMDE_VECTOR(size) __attribute__((__vector_size__(size)))
#    define SIMDE_VECTOR_OPS
#    define SIMDE_VECTOR_NEGATE
#    define SIMDE_VECTOR_SCALAR
#    define SIMDE_VECTOR_SUBSCRIPT
#  elif HEDLEY_INTEL_VERSION_CHECK(16,0,0)
#    define SIMDE_VECTOR(size) __attribute__((__vector_size__(size)))
#    define SIMDE_VECTOR_OPS
#    define SIMDE_VECTOR_NEGATE
/* ICC only supports SIMDE_VECTOR_SCALAR for constants */
#    define SIMDE_VECTOR_SUBSCRIPT
#  elif \
    HEDLEY_GCC_VERSION_CHECK(4,1,0) || \
    HEDLEY_INTEL_VERSION_CHECK(13,0,0) || \
    HEDLEY_MCST_LCC_VERSION_CHECK(1,25,10)
#    define SIMDE_VECTOR(size) __attribute__((__vector_size__(size)))
#    define SIMDE_VECTOR_OPS
#  elif HEDLEY_SUNPRO_VERSION_CHECK(5,12,0)
#    define SIMDE_VECTOR(size) __attribute__((__vector_size__(size)))
#  elif HEDLEY_HAS_ATTRIBUTE(vector_size)
#    define SIMDE_VECTOR(size) __attribute__((__vector_size__(size)))
#    define SIMDE_VECTOR_OPS
#    define SIMDE_VECTOR_NEGATE
#    define SIMDE_VECTOR_SUBSCRIPT
#    if SIMDE_DETECT_CLANG_VERSION_CHECK(5,0,0)
#      define SIMDE_VECTOR_SCALAR
#    endif
#  endif

/* GCC and clang have built-in functions to handle shuffling and
   converting of vectors, but the implementations are slightly
   different.  This macro is just an abstraction over them.  Note that
   elem_size is in bits but vec_size is in bytes. */
#  if !defined(SIMDE_NO_SHUFFLE_VECTOR) && defined(SIMDE_VECTOR_SUBSCRIPT)
     HEDLEY_DIAGNOSTIC_PUSH
     /* We don't care about -Wvariadic-macros; all compilers that support
      * shufflevector/shuffle support them. */
#    if HEDLEY_HAS_WARNING("-Wc++98-compat-pedantic")
#      pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
#    endif
#    if HEDLEY_HAS_WARNING("-Wvariadic-macros") || HEDLEY_GCC_VERSION_CHECK(4,0,0)
#      pragma GCC diagnostic ignored "-Wvariadic-macros"
#    endif

#    if HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
#      define SIMDE_SHUFFLE_VECTOR_(elem_size, vec_size, a, b, ...) __builtin_shufflevector(a, b, __VA_ARGS__)
#    elif HEDLEY_GCC_HAS_BUILTIN(__builtin_shuffle,4,7,0) && !defined(__INTEL_COMPILER)
#      define SIMDE_SHUFFLE_VECTOR_(elem_size, vec_size, a, b, ...) (__extension__ ({ \
         int##elem_size##_t SIMDE_VECTOR(vec_size) simde_shuffle_ = { __VA_ARGS__ }; \
           __builtin_shuffle(a, b, simde_shuffle_); \
         }))
#    endif
     HEDLEY_DIAGNOSTIC_POP
#  endif

/* TODO: this actually works on XL C/C++ without SIMDE_VECTOR_SUBSCRIPT
   but the code needs to be refactored a bit to take advantage. */
#  if !defined(SIMDE_NO_CONVERT_VECTOR) && defined(SIMDE_VECTOR_SUBSCRIPT)
#    if HEDLEY_HAS_BUILTIN(__builtin_convertvector) || HEDLEY_GCC_VERSION_CHECK(9,0,0)
#      if HEDLEY_GCC_VERSION_CHECK(9,0,0) && !HEDLEY_GCC_VERSION_CHECK(9,3,0)
         /* https://gcc.gnu.org/bugzilla/show_bug.cgi?id=93557 */
#        define SIMDE_CONVERT_VECTOR_(to, from) ((to) = (__extension__({ \
             __typeof__(from) from_ = (from); \
             ((void) from_); \
             __builtin_convertvector(from_, __typeof__(to)); \
           })))
#      else
#        define SIMDE_CONVERT_VECTOR_(to, from) ((to) = __builtin_convertvector((from), __typeof__(to)))
#      endif
#    endif
#  endif
#endif

/* Since we currently require SUBSCRIPT before using a vector in a
   union, we define these as dependencies of SUBSCRIPT.  They are
   likely to disappear in the future, once SIMDe learns how to make
   use of vectors without using the union members.  Do not use them
   in your code unless you're okay with it breaking when SIMDe
   changes. */
#if defined(SIMDE_VECTOR_SUBSCRIPT)
#  if defined(SIMDE_VECTOR_OPS)
#    define SIMDE_VECTOR_SUBSCRIPT_OPS
#  endif
#  if defined(SIMDE_VECTOR_SCALAR)
#    define SIMDE_VECTOR_SUBSCRIPT_SCALAR
#  endif
#endif

#if !defined(SIMDE_DISABLE_OPENMP)
  #if !defined(SIMDE_ENABLE_OPENMP) && ((defined(_OPENMP) && (_OPENMP >= 201307L)) || (defined(_OPENMP_SIMD) && (_OPENMP_SIMD >= 201307L))) || defined(HEDLEY_MCST_LCC_VERSION)
    #define SIMDE_ENABLE_OPENMP
  #endif
#endif

#if !defined(SIMDE_ENABLE_CILKPLUS) && (defined(__cilk) || defined(HEDLEY_INTEL_VERSION))
#  define SIMDE_ENABLE_CILKPLUS
#endif

#if defined(SIMDE_ENABLE_OPENMP)
#  define SIMDE_VECTORIZE HEDLEY_PRAGMA(omp simd)
#  define SIMDE_VECTORIZE_SAFELEN(l) HEDLEY_PRAGMA(omp simd safelen(l))
#  if defined(__clang__)
#    define SIMDE_VECTORIZE_REDUCTION(r) \
        HEDLEY_DIAGNOSTIC_PUSH \
        _Pragma("clang diagnostic ignored \"-Wsign-conversion\"") \
        HEDLEY_PRAGMA(omp simd reduction(r)) \
        HEDLEY_DIAGNOSTIC_POP
#  else
#    define SIMDE_VECTORIZE_REDUCTION(r) HEDLEY_PRAGMA(omp simd reduction(r))
#  endif
#  if !defined(HEDLEY_MCST_LCC_VERSION)
#    define SIMDE_VECTORIZE_ALIGNED(a) HEDLEY_PRAGMA(omp simd aligned(a))
#  else
#    define SIMDE_VECTORIZE_ALIGNED(a) HEDLEY_PRAGMA(omp simd)
#  endif
#elif defined(SIMDE_ENABLE_CILKPLUS)
#  define SIMDE_VECTORIZE HEDLEY_PRAGMA(simd)
#  define SIMDE_VECTORIZE_SAFELEN(l) HEDLEY_PRAGMA(simd vectorlength(l))
#  define SIMDE_VECTORIZE_REDUCTION(r) HEDLEY_PRAGMA(simd reduction(r))
#  define SIMDE_VECTORIZE_ALIGNED(a) HEDLEY_PRAGMA(simd aligned(a))
#elif defined(__clang__) && !defined(HEDLEY_IBM_VERSION)
#  define SIMDE_VECTORIZE HEDLEY_PRAGMA(clang loop vectorize(enable))
#  define SIMDE_VECTORIZE_SAFELEN(l) HEDLEY_PRAGMA(clang loop vectorize_width(l))
#  define SIMDE_VECTORIZE_REDUCTION(r) SIMDE_VECTORIZE
#  define SIMDE_VECTORIZE_ALIGNED(a)
#elif HEDLEY_GCC_VERSION_CHECK(4,9,0)
#  define SIMDE_VECTORIZE HEDLEY_PRAGMA(GCC ivdep)
#  define SIMDE_VECTORIZE_SAFELEN(l) SIMDE_VECTORIZE
#  define SIMDE_VECTORIZE_REDUCTION(r) SIMDE_VECTORIZE
#  define SIMDE_VECTORIZE_ALIGNED(a)
#elif HEDLEY_CRAY_VERSION_CHECK(5,0,0)
#  define SIMDE_VECTORIZE HEDLEY_PRAGMA(_CRI ivdep)
#  define SIMDE_VECTORIZE_SAFELEN(l) SIMDE_VECTORIZE
#  define SIMDE_VECTORIZE_REDUCTION(r) SIMDE_VECTORIZE
#  define SIMDE_VECTORIZE_ALIGNED(a)
#else
#  define SIMDE_VECTORIZE
#  define SIMDE_VECTORIZE_SAFELEN(l)
#  define SIMDE_VECTORIZE_REDUCTION(r)
#  define SIMDE_VECTORIZE_ALIGNED(a)
#endif

#define SIMDE_MASK_NZ_(v, mask) (((v) & (mask)) | !((v) & (mask)))

/* Intended for checking coverage, you should never use this in
   production. */
#if defined(SIMDE_NO_INLINE)
#  define SIMDE_FUNCTION_ATTRIBUTES HEDLEY_NEVER_INLINE static
#else
#  define SIMDE_FUNCTION_ATTRIBUTES HEDLEY_ALWAYS_INLINE static
#endif

#if defined(SIMDE_NO_INLINE)
#  define SIMDE_HUGE_FUNCTION_ATTRIBUTES HEDLEY_NEVER_INLINE static
#elif defined(SIMDE_CONSTRAINED_COMPILATION)
#  define SIMDE_HUGE_FUNCTION_ATTRIBUTES static
#else
#  define SIMDE_HUGE_FUNCTION_ATTRIBUTES HEDLEY_ALWAYS_INLINE static
#endif

#if \
    HEDLEY_HAS_ATTRIBUTE(unused) || \
    HEDLEY_GCC_VERSION_CHECK(2,95,0)
#  define SIMDE_FUNCTION_POSSIBLY_UNUSED_ __attribute__((__unused__))
#else
#  define SIMDE_FUNCTION_POSSIBLY_UNUSED_
#endif

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DIAGNOSTIC_DISABLE_USED_BUT_MARKED_UNUSED_

#if defined(_MSC_VER)
#  define SIMDE_BEGIN_DECLS_ HEDLEY_DIAGNOSTIC_PUSH __pragma(warning(disable:4996 4204)) HEDLEY_BEGIN_C_DECLS
#  define SIMDE_END_DECLS_ HEDLEY_DIAGNOSTIC_POP HEDLEY_END_C_DECLS
#else
#  define SIMDE_BEGIN_DECLS_ \
     HEDLEY_DIAGNOSTIC_PUSH \
     SIMDE_DIAGNOSTIC_DISABLE_USED_BUT_MARKED_UNUSED_ \
     HEDLEY_BEGIN_C_DECLS
#  define SIMDE_END_DECLS_ \
     HEDLEY_END_C_DECLS \
     HEDLEY_DIAGNOSTIC_POP
#endif

#if defined(__SIZEOF_INT128__)
#  define SIMDE_HAVE_INT128_
HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DIAGNOSTIC_DISABLE_PEDANTIC_
typedef __int128 simde_int128;
typedef unsigned __int128 simde_uint128;
HEDLEY_DIAGNOSTIC_POP
#endif

#if !defined(SIMDE_ENDIAN_LITTLE)
#  define SIMDE_ENDIAN_LITTLE 1234
#endif
#if !defined(SIMDE_ENDIAN_BIG)
#  define SIMDE_ENDIAN_BIG 4321
#endif

#if !defined(SIMDE_ENDIAN_ORDER)
/* GCC (and compilers masquerading as GCC) define  __BYTE_ORDER__. */
#  if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#    define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_LITTLE
#  elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#    define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_BIG
/* TI defines _BIG_ENDIAN or _LITTLE_ENDIAN */
#  elif defined(_BIG_ENDIAN)
#    define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_BIG
#  elif defined(_LITTLE_ENDIAN)
#    define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_LITTLE
/* We know the endianness of some common architectures.  Common
 * architectures not listed (ARM, POWER, MIPS, etc.) here are
 * bi-endian. */
#  elif defined(__amd64) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
#    define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_LITTLE
#  elif defined(__s390x__) || defined(__zarch__)
#    define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_BIG
/* Looks like we'll have to rely on the platform.  If we're missing a
 * platform, please let us know. */
#  elif defined(_WIN32)
#    define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_LITTLE
#  elif defined(sun) || defined(__sun) /* Solaris */
#    include <sys/byteorder.h>
#    if defined(_LITTLE_ENDIAN)
#      define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_LITTLE
#    elif defined(_BIG_ENDIAN)
#      define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_BIG
#    endif
#  elif defined(__APPLE__)
#    include <libkern/OSByteOrder.h>
#    if defined(__LITTLE_ENDIAN__)
#      define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_LITTLE
#    elif defined(__BIG_ENDIAN__)
#      define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_BIG
#    endif
#  elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__) || defined(__DragonFly__) || defined(BSD)
#    include <machine/endian.h>
#    if defined(__BYTE_ORDER) && (__BYTE_ORDER == __LITTLE_ENDIAN)
#      define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_LITTLE
#    elif defined(__BYTE_ORDER) && (__BYTE_ORDER == __BIG_ENDIAN)
#      define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_BIG
#    endif
#  elif defined(__linux__) || defined(__linux) || defined(__gnu_linux__)
#    include <endian.h>
#    if defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && (__BYTE_ORDER == __LITTLE_ENDIAN)
#      define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_LITTLE
#    elif defined(__BYTE_ORDER) && defined(__BIG_ENDIAN) && (__BYTE_ORDER == __BIG_ENDIAN)
#      define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_BIG
#    endif
#  endif
#endif

#if \
    HEDLEY_HAS_BUILTIN(__builtin_bswap64) || \
    HEDLEY_GCC_VERSION_CHECK(4,3,0) || \
    HEDLEY_IBM_VERSION_CHECK(13,1,0) || \
    HEDLEY_INTEL_VERSION_CHECK(13,0,0)
  #define simde_bswap64(v) __builtin_bswap64(v)
#elif HEDLEY_MSVC_VERSION_CHECK(13,10,0)
  #define simde_bswap64(v) _byteswap_uint64(v)
#else
  SIMDE_FUNCTION_ATTRIBUTES
  uint64_t
  simde_bswap64(uint64_t v) {
    return
      ((v & (((uint64_t) 0xff) << 56)) >> 56) |
      ((v & (((uint64_t) 0xff) << 48)) >> 40) |
      ((v & (((uint64_t) 0xff) << 40)) >> 24) |
      ((v & (((uint64_t) 0xff) << 32)) >>  8) |
      ((v & (((uint64_t) 0xff) << 24)) <<  8) |
      ((v & (((uint64_t) 0xff) << 16)) << 24) |
      ((v & (((uint64_t) 0xff) <<  8)) << 40) |
      ((v & (((uint64_t) 0xff)      )) << 56);
  }
#endif

#if !defined(SIMDE_ENDIAN_ORDER)
#  error Unknown byte order; please file a bug
#else
#  if SIMDE_ENDIAN_ORDER == SIMDE_ENDIAN_LITTLE
#    define simde_endian_bswap64_be(value) simde_bswap64(value)
#    define simde_endian_bswap64_le(value) (value)
#  elif SIMDE_ENDIAN_ORDER == SIMDE_ENDIAN_BIG
#    define simde_endian_bswap64_be(value) (value)
#    define simde_endian_bswap64_le(value) simde_bswap64(value)
#  endif
#endif

/* TODO: we should at least make an attempt to detect the correct
   types for simde_float32/float64 instead of just assuming float and
   double. */

#if !defined(SIMDE_FLOAT32_TYPE)
#  define SIMDE_FLOAT32_TYPE float
#  define SIMDE_FLOAT32_C(value) value##f
#else
#  define SIMDE_FLOAT32_C(value) ((SIMDE_FLOAT32_TYPE) value)
#endif
typedef SIMDE_FLOAT32_TYPE simde_float32;

#if !defined(SIMDE_FLOAT64_TYPE)
#  define SIMDE_FLOAT64_TYPE double
#  define SIMDE_FLOAT64_C(value) value
#else
#  define SIMDE_FLOAT64_C(value) ((SIMDE_FLOAT64_TYPE) value)
#endif
typedef SIMDE_FLOAT64_TYPE simde_float64;

#if defined(SIMDE_POLY8_TYPE)
#  undef SIMDE_POLY8_TYPE
#endif
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
#  define SIMDE_POLY8_TYPE poly8_t
#  define SIMDE_POLY8_C(value) (HEDLEY_STATIC_CAST(poly8_t, value))
#else
#  define SIMDE_POLY8_TYPE uint8_t
#  define SIMDE_POLY8_C(value) (HEDLEY_STATIC_CAST(uint8_t, value))
#endif
typedef SIMDE_POLY8_TYPE simde_poly8;

#if defined(SIMDE_POLY16_TYPE)
#  undef SIMDE_POLY16_TYPE
#endif
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
#  define SIMDE_POLY16_TYPE poly16_t
#  define SIMDE_POLY16_C(value) (HEDLEY_STATIC_CAST(poly16_t, value))
#else
#  define SIMDE_POLY16_TYPE uint16_t
#  define SIMDE_POLY16_C(value) (HEDLEY_STATIC_CAST(uint16_t, value))
#endif
typedef SIMDE_POLY16_TYPE simde_poly16;

#if defined(SIMDE_POLY64_TYPE)
#  undef SIMDE_POLY64_TYPE
#endif
#if defined(SIMDE_ARM_NEON_A32V8_NATIVE)
#  define SIMDE_POLY64_TYPE poly64_t
#  define SIMDE_POLY64_C(value) (HEDLEY_STATIC_CAST(poly64_t, value ## ull))
#else
#  define SIMDE_POLY64_TYPE uint64_t
#  define SIMDE_POLY64_C(value) value ## ull
#endif
typedef SIMDE_POLY64_TYPE simde_poly64;

#if defined(SIMDE_POLY128_TYPE)
#  undef SIMDE_POLY128_TYPE
#endif
#if defined(SIMDE_ARM_NEON_A32V8_NATIVE) && defined(SIMDE_ARCH_ARM_CRYPTO)
#  define SIMDE_POLY128_TYPE poly128_t
#  define SIMDE_POLY128_C(value) value
#elif defined(__SIZEOF_INT128__)
#  define SIMDE_POLY128_TYPE __int128
#  define SIMDE_POLY128_C(value) (HEDLEY_STATIC_CAST(__int128, value))
#else
#  define SIMDE_POLY128_TYPE uint64_t
#  define SIMDE_TARGET_NOT_SUPPORT_INT128_TYPE 1
#endif
typedef SIMDE_POLY128_TYPE simde_poly128;

#if defined(__cplusplus)
  typedef bool simde_bool;
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
  typedef _Bool simde_bool;
#elif defined(bool)
  typedef bool simde_bool;
#else
  #include <stdbool.h>
  typedef bool simde_bool;
#endif

#if HEDLEY_HAS_WARNING("-Wbad-function-cast")
#  define SIMDE_CONVERT_FTOI(T,v) \
    HEDLEY_DIAGNOSTIC_PUSH \
    _Pragma("clang diagnostic ignored \"-Wbad-function-cast\"") \
    HEDLEY_STATIC_CAST(T, (v)) \
    HEDLEY_DIAGNOSTIC_POP
#else
#  define SIMDE_CONVERT_FTOI(T,v) ((T) (v))
#endif

/* TODO: detect compilers which support this outside of C11 mode */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
  #define SIMDE_CHECKED_REINTERPRET_CAST(to, from, value) _Generic((value), to: (value), default: (_Generic((value), from: ((to) (value)))))
  #define SIMDE_CHECKED_STATIC_CAST(to, from, value) _Generic((value), to: (value), default: (_Generic((value), from: ((to) (value)))))
#else
  #define SIMDE_CHECKED_REINTERPRET_CAST(to, from, value) HEDLEY_REINTERPRET_CAST(to, value)
  #define SIMDE_CHECKED_STATIC_CAST(to, from, value) HEDLEY_STATIC_CAST(to, value)
#endif

#if HEDLEY_HAS_WARNING("-Wfloat-equal")
#  define SIMDE_DIAGNOSTIC_DISABLE_FLOAT_EQUAL _Pragma("clang diagnostic ignored \"-Wfloat-equal\"")
#elif HEDLEY_GCC_VERSION_CHECK(3,0,0)
#  define SIMDE_DIAGNOSTIC_DISABLE_FLOAT_EQUAL _Pragma("GCC diagnostic ignored \"-Wfloat-equal\"")
#else
#  define SIMDE_DIAGNOSTIC_DISABLE_FLOAT_EQUAL
#endif

/* Some functions can trade accuracy for speed.  For those functions
   you can control the trade-off using this macro.  Possible values:

   0: prefer speed
   1: reasonable trade-offs
   2: prefer accuracy */
#if !defined(SIMDE_ACCURACY_PREFERENCE)
#  define SIMDE_ACCURACY_PREFERENCE 1
#endif

#if defined(__STDC_HOSTED__)
#  define SIMDE_STDC_HOSTED __STDC_HOSTED__
#else
#  if \
     defined(HEDLEY_PGI_VERSION) || \
     defined(HEDLEY_MSVC_VERSION)
#    define SIMDE_STDC_HOSTED 1
#  else
#    define SIMDE_STDC_HOSTED 0
#  endif
#endif

/* Try to deal with environments without a standard library. */
#if !defined(simde_memcpy)
  #if HEDLEY_HAS_BUILTIN(__builtin_memcpy)
    #define simde_memcpy(dest, src, n) __builtin_memcpy(dest, src, n)
  #endif
#endif
#if !defined(simde_memset)
  #if HEDLEY_HAS_BUILTIN(__builtin_memset)
    #define simde_memset(s, c, n) __builtin_memset(s, c, n)
  #endif
#endif
#if !defined(simde_memcmp)
  #if HEDLEY_HAS_BUILTIN(__builtin_memcmp)
    #define simde_memcmp(s1, s2, n) __builtin_memcmp(s1, s2, n)
  #endif
#endif

#if !defined(simde_memcpy) || !defined(simde_memset) || !defined(simde_memcmp)
  #if !defined(SIMDE_NO_STRING_H)
    #if defined(__has_include)
      #if !__has_include(<string.h>)
        #define SIMDE_NO_STRING_H
      #endif
    #elif (SIMDE_STDC_HOSTED == 0)
      #define SIMDE_NO_STRING_H
    #endif
  #endif

  #if !defined(SIMDE_NO_STRING_H)
    #include <string.h>
    #if !defined(simde_memcpy)
      #define simde_memcpy(dest, src, n) memcpy(dest, src, n)
    #endif
    #if !defined(simde_memset)
      #define simde_memset(s, c, n) memset(s, c, n)
    #endif
    #if !defined(simde_memcmp)
      #define simde_memcmp(s1, s2, n) memcmp(s1, s2, n)
    #endif
  #else
    /* These are meant to be portable, not fast.  If you're hitting them you
     * should think about providing your own (by defining the simde_memcpy
     * macro prior to including any SIMDe files) or submitting a patch to
     * SIMDe so we can detect your system-provided memcpy/memset, like by
     * adding your compiler to the checks for __builtin_memcpy and/or
     * __builtin_memset. */
    #if !defined(simde_memcpy)
      SIMDE_FUNCTION_ATTRIBUTES
      void
      simde_memcpy_(void* dest, const void* src, size_t len) {
        char* dest_ = HEDLEY_STATIC_CAST(char*, dest);
        char* src_ = HEDLEY_STATIC_CAST(const char*, src);
        for (size_t i = 0 ; i < len ; i++) {
          dest_[i] = src_[i];
        }
      }
      #define simde_memcpy(dest, src, n) simde_memcpy_(dest, src, n)
    #endif

    #if !defined(simde_memset)
      SIMDE_FUNCTION_ATTRIBUTES
      void
      simde_memset_(void* s, int c, size_t len) {
        char* s_ = HEDLEY_STATIC_CAST(char*, s);
        char c_ = HEDLEY_STATIC_CAST(char, c);
        for (size_t i = 0 ; i < len ; i++) {
          s_[i] = c_[i];
        }
      }
      #define simde_memset(s, c, n) simde_memset_(s, c, n)
    #endif

    #if !defined(simde_memcmp)
      SIMDE_FUCTION_ATTRIBUTES
      int
      simde_memcmp_(const void *s1, const void *s2, size_t n) {
        unsigned char* s1_ = HEDLEY_STATIC_CAST(unsigned char*, s1);
        unsigned char* s2_ = HEDLEY_STATIC_CAST(unsigned char*, s2);
        for (size_t i = 0 ; i < len ; i++) {
          if (s1_[i] != s2_[i]) {
            return (int) (s1_[i] - s2_[i]);
          }
        }
        return 0;
      }
    #define simde_memcmp(s1, s2, n) simde_memcmp_(s1, s2, n)
    #endif
  #endif
#endif

/*** Functions that quiet a signaling NaN ***/

static HEDLEY_INLINE
double
simde_math_quiet(double x) {
  uint64_t tmp, mask;
  if (!simde_math_isnan(x)) {
    return x;
  }
  simde_memcpy(&tmp, &x, 8);
  mask = 0x7ff80000;
  mask <<= 32;
  tmp |= mask;
  simde_memcpy(&x, &tmp, 8);
  return x;
}

static HEDLEY_INLINE
float
simde_math_quietf(float x) {
  uint32_t tmp;
  if (!simde_math_isnanf(x)) {
    return x;
  }
  simde_memcpy(&tmp, &x, 4);
  tmp |= 0x7fc00000lu;
  simde_memcpy(&x, &tmp, 4);
  return x;
}

#if defined(FE_ALL_EXCEPT)
  #define SIMDE_HAVE_FENV_H
#elif defined(__has_include)
  #if __has_include(<fenv.h>)
    #include <fenv.h>
    #define SIMDE_HAVE_FENV_H
  #endif
#elif SIMDE_STDC_HOSTED == 1
  #include <fenv.h>
  #define SIMDE_HAVE_FENV_H
#endif

#if defined(EXIT_FAILURE)
  #define SIMDE_HAVE_STDLIB_H
#elif defined(__has_include)
  #if __has_include(<stdlib.h>)
    #include <stdlib.h>
    #define SIMDE_HAVE_STDLIB_H
  #endif
#elif SIMDE_STDC_HOSTED == 1
  #include <stdlib.h>
  #define SIMDE_HAVE_STDLIB_H
#endif

#if defined(__has_include)
#  if defined(__cplusplus) && (__cplusplus >= 201103L) && __has_include(<cfenv>)
#    include <cfenv>
#  elif __has_include(<fenv.h>)
#    include <fenv.h>
#  endif
#  if __has_include(<stdlib.h>)
#    include <stdlib.h>
#  endif
#elif SIMDE_STDC_HOSTED == 1
#  include <stdlib.h>
#  include <fenv.h>
#endif

#define SIMDE_DEFINE_CONVERSION_FUNCTION_(Name, T_To, T_From) \
  static HEDLEY_ALWAYS_INLINE HEDLEY_CONST SIMDE_FUNCTION_POSSIBLY_UNUSED_ \
  T_To \
  Name (T_From value) { \
    T_To r; \
    simde_memcpy(&r, &value, sizeof(r)); \
    return r; \
  }

SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_float32_as_uint32,      uint32_t, simde_float32)
SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint32_as_float32, simde_float32, uint32_t)
SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_float64_as_uint64,      uint64_t, simde_float64)
SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint64_as_float64, simde_float64, uint64_t)

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/check.h :: */
/* Check (assertions)
 * Portable Snippets - https://github.com/nemequ/portable-snippets
 * Created by Evan Nemerson <evan@nemerson.com>
 *
 *   To the extent possible under law, the authors have waived all
 *   copyright and related or neighboring rights to this code.  For
 *   details, see the Creative Commons Zero 1.0 Universal license at
 *   https://creativecommons.org/publicdomain/zero/1.0/
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#if !defined(SIMDE_CHECK_H)
#define SIMDE_CHECK_H

#if !defined(SIMDE_NDEBUG) && !defined(SIMDE_DEBUG)
#  define SIMDE_NDEBUG 1
#endif

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
#include <stdint.h>

#if !defined(_WIN32)
#  define SIMDE_SIZE_MODIFIER "z"
#  define SIMDE_CHAR_MODIFIER "hh"
#  define SIMDE_SHORT_MODIFIER "h"
#else
#  if defined(_M_X64) || defined(__amd64__)
#    define SIMDE_SIZE_MODIFIER "I64"
#  else
#    define SIMDE_SIZE_MODIFIER ""
#  endif
#  define SIMDE_CHAR_MODIFIER ""
#  define SIMDE_SHORT_MODIFIER ""
#endif

#if defined(_MSC_VER) &&  (_MSC_VER >= 1500)
#  define SIMDE_PUSH_DISABLE_MSVC_C4127_ __pragma(warning(push)) __pragma(warning(disable:4127))
#  define SIMDE_POP_DISABLE_MSVC_C4127_ __pragma(warning(pop))
#else
#  define SIMDE_PUSH_DISABLE_MSVC_C4127_
#  define SIMDE_POP_DISABLE_MSVC_C4127_
#endif

#if !defined(simde_errorf)
#  if defined(__has_include)
#    if __has_include(<stdio.h>)
#      include <stdio.h>
#    endif
#  elif defined(SIMDE_STDC_HOSTED)
#    if SIMDE_STDC_HOSTED == 1
#      include <stdio.h>
#    endif
#  elif defined(__STDC_HOSTED__)
#    if __STDC_HOSTETD__ == 1
#      include <stdio.h>
#    endif
#  endif

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/debug-trap.h :: */
/* Debugging assertions and traps
 * Portable Snippets - https://github.com/nemequ/portable-snippets
 * Created by Evan Nemerson <evan@nemerson.com>
 *
 *   To the extent possible under law, the authors have waived all
 *   copyright and related or neighboring rights to this code.  For
 *   details, see the Creative Commons Zero 1.0 Universal license at
 *   https://creativecommons.org/publicdomain/zero/1.0/
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#if !defined(SIMDE_DEBUG_TRAP_H)
#define SIMDE_DEBUG_TRAP_H

#if !defined(SIMDE_NDEBUG) && defined(NDEBUG) && !defined(SIMDE_DEBUG)
#  define SIMDE_NDEBUG 1
#endif

#if defined(__has_builtin) && !defined(__ibmxl__)
#  if __has_builtin(__builtin_debugtrap)
#    define simde_trap() __builtin_debugtrap()
#  elif __has_builtin(__debugbreak)
#    define simde_trap() __debugbreak()
#  endif
#endif
#if !defined(simde_trap)
#  if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#    define simde_trap() __debugbreak()
#  elif defined(__ARMCC_VERSION)
#    define simde_trap() __breakpoint(42)
#  elif defined(__ibmxl__) || defined(__xlC__)
#    include <builtins.h>
#    define simde_trap() __trap(42)
#  elif defined(__DMC__) && defined(_M_IX86)
     static inline void simde_trap(void) { __asm int 3h; }
#  elif defined(__i386__) || defined(__x86_64__)
     static inline void simde_trap(void) { __asm__ __volatile__("int $03"); }
#  elif defined(__thumb__)
     static inline void simde_trap(void) { __asm__ __volatile__(".inst 0xde01"); }
#  elif defined(__aarch64__)
     static inline void simde_trap(void) { __asm__ __volatile__(".inst 0xd4200000"); }
#  elif defined(__arm__)
     static inline void simde_trap(void) { __asm__ __volatile__(".inst 0xe7f001f0"); }
#  elif defined (__alpha__) && !defined(__osf__)
     static inline void simde_trap(void) { __asm__ __volatile__("bpt"); }
#  elif defined(_54_)
     static inline void simde_trap(void) { __asm__ __volatile__("ESTOP"); }
#  elif defined(_55_)
     static inline void simde_trap(void) { __asm__ __volatile__(";\n .if (.MNEMONIC)\n ESTOP_1\n .else\n ESTOP_1()\n .endif\n NOP"); }
#  elif defined(_64P_)
     static inline void simde_trap(void) { __asm__ __volatile__("SWBP 0"); }
#  elif defined(_6x_)
     static inline void simde_trap(void) { __asm__ __volatile__("NOP\n .word 0x10000000"); }
#  elif defined(__STDC_HOSTED__) && (__STDC_HOSTED__ == 0) && defined(__GNUC__)
#    define simde_trap() __builtin_trap()
#  else
#    include <signal.h>
#    if defined(SIGTRAP)
#      define simde_trap() raise(SIGTRAP)
#    else
#      define simde_trap() raise(SIGABRT)
#    endif
#  endif
#endif

#if defined(HEDLEY_LIKELY)
#  define SIMDE_DBG_LIKELY(expr) HEDLEY_LIKELY(expr)
#elif defined(__GNUC__) && (__GNUC__ >= 3)
#  define SIMDE_DBG_LIKELY(expr) __builtin_expect(!!(expr), 1)
#else
#  define SIMDE_DBG_LIKELY(expr) (!!(expr))
#endif

#if !defined(SIMDE_NDEBUG) || (SIMDE_NDEBUG == 0)
#  define simde_dbg_assert(expr) do { \
    if (!SIMDE_DBG_LIKELY(expr)) { \
      simde_trap(); \
    } \
  } while (0)
#else
#  define simde_dbg_assert(expr)
#endif

#endif /* !defined(SIMDE_DEBUG_TRAP_H) */
/* :: End simde/debug-trap.h :: */

   HEDLEY_DIAGNOSTIC_PUSH
   SIMDE_DIAGNOSTIC_DISABLE_VARIADIC_MACROS_
#  if defined(EOF)
#    define simde_errorf(format, ...) (fprintf(stderr, format, __VA_ARGS__), abort())
#  else
#    define simde_errorf(format, ...) (simde_trap())
#  endif
   HEDLEY_DIAGNOSTIC_POP
#endif

#define simde_error(msg) simde_errorf("%s", msg)

#if defined(SIMDE_NDEBUG) || \
    (defined(__cplusplus) && (__cplusplus < 201103L)) || \
    (defined(__STDC__) && (__STDC__ < 199901L))
#  if defined(SIMDE_CHECK_FAIL_DEFINED)
#    define simde_assert(expr)
#  else
#    if defined(HEDLEY_ASSUME)
#      define simde_assert(expr) HEDLEY_ASSUME(expr)
#    elif HEDLEY_GCC_VERSION_CHECK(4,5,0)
#      define simde_assert(expr) ((void) (!!(expr) ? 1 : (__builtin_unreachable(), 1)))
#    elif HEDLEY_MSVC_VERSION_CHECK(13,10,0)
#      define simde_assert(expr) __assume(expr)
#    else
#      define simde_assert(expr)
#    endif
#  endif
#  define simde_assert_true(expr) simde_assert(expr)
#  define simde_assert_false(expr) simde_assert(!(expr))
#  define simde_assert_type_full(prefix, suffix, T, fmt, a, op, b) simde_assert(((a) op (b)))
#  define simde_assert_double_equal(a, b, precision)
#  define simde_assert_string_equal(a, b)
#  define simde_assert_string_not_equal(a, b)
#  define simde_assert_memory_equal(size, a, b)
#  define simde_assert_memory_not_equal(size, a, b)
#else
#  define simde_assert(expr) \
    do { \
      if (!HEDLEY_LIKELY(expr)) { \
        simde_error("assertion failed: " #expr "\n"); \
      } \
      SIMDE_PUSH_DISABLE_MSVC_C4127_ \
    } while (0) \
    SIMDE_POP_DISABLE_MSVC_C4127_

#  define simde_assert_true(expr) \
    do { \
      if (!HEDLEY_LIKELY(expr)) { \
        simde_error("assertion failed: " #expr " is not true\n"); \
      } \
      SIMDE_PUSH_DISABLE_MSVC_C4127_ \
    } while (0) \
    SIMDE_POP_DISABLE_MSVC_C4127_

#  define simde_assert_false(expr) \
    do { \
      if (!HEDLEY_LIKELY(!(expr))) { \
        simde_error("assertion failed: " #expr " is not false\n"); \
      } \
      SIMDE_PUSH_DISABLE_MSVC_C4127_ \
    } while (0) \
    SIMDE_POP_DISABLE_MSVC_C4127_

#  define simde_assert_type_full(prefix, suffix, T, fmt, a, op, b)   \
    do { \
      T simde_tmp_a_ = (a); \
      T simde_tmp_b_ = (b); \
      if (!(simde_tmp_a_ op simde_tmp_b_)) { \
        simde_errorf("assertion failed: %s %s %s (" prefix "%" fmt suffix " %s " prefix "%" fmt suffix ")\n", \
                     #a, #op, #b, simde_tmp_a_, #op, simde_tmp_b_); \
      } \
      SIMDE_PUSH_DISABLE_MSVC_C4127_ \
    } while (0) \
    SIMDE_POP_DISABLE_MSVC_C4127_

#  define simde_assert_double_equal(a, b, precision) \
    do { \
      const double simde_tmp_a_ = (a); \
      const double simde_tmp_b_ = (b); \
      const double simde_tmp_diff_ = ((simde_tmp_a_ - simde_tmp_b_) < 0) ? \
        -(simde_tmp_a_ - simde_tmp_b_) : \
        (simde_tmp_a_ - simde_tmp_b_); \
      if (HEDLEY_UNLIKELY(simde_tmp_diff_ > 1e-##precision)) { \
        simde_errorf("assertion failed: %s == %s (%0." #precision "g == %0." #precision "g)\n", \
                     #a, #b, simde_tmp_a_, simde_tmp_b_); \
      } \
      SIMDE_PUSH_DISABLE_MSVC_C4127_ \
    } while (0) \
    SIMDE_POP_DISABLE_MSVC_C4127_

#  include <string.h>
#  define simde_assert_string_equal(a, b) \
    do { \
      const char* simde_tmp_a_ = a; \
      const char* simde_tmp_b_ = b; \
      if (HEDLEY_UNLIKELY(strcmp(simde_tmp_a_, simde_tmp_b_) != 0)) { \
        simde_errorf("assertion failed: string %s == %s (\"%s\" == \"%s\")\n", \
                     #a, #b, simde_tmp_a_, simde_tmp_b_); \
      } \
      SIMDE_PUSH_DISABLE_MSVC_C4127_ \
    } while (0) \
    SIMDE_POP_DISABLE_MSVC_C4127_

#  define simde_assert_string_not_equal(a, b) \
    do { \
      const char* simde_tmp_a_ = a; \
      const char* simde_tmp_b_ = b; \
      if (HEDLEY_UNLIKELY(strcmp(simde_tmp_a_, simde_tmp_b_) == 0)) { \
        simde_errorf("assertion failed: string %s != %s (\"%s\" == \"%s\")\n", \
                     #a, #b, simde_tmp_a_, simde_tmp_b_); \
      } \
      SIMDE_PUSH_DISABLE_MSVC_C4127_ \
    } while (0) \
    SIMDE_POP_DISABLE_MSVC_C4127_

#  define simde_assert_memory_equal(size, a, b) \
    do { \
      const unsigned char* simde_tmp_a_ = (const unsigned char*) (a); \
      const unsigned char* simde_tmp_b_ = (const unsigned char*) (b); \
      const size_t simde_tmp_size_ = (size); \
      if (HEDLEY_UNLIKELY(memcmp(simde_tmp_a_, simde_tmp_b_, simde_tmp_size_)) != 0) { \
        size_t simde_tmp_pos_; \
        for (simde_tmp_pos_ = 0 ; simde_tmp_pos_ < simde_tmp_size_ ; simde_tmp_pos_++) { \
          if (simde_tmp_a_[simde_tmp_pos_] != simde_tmp_b_[simde_tmp_pos_]) { \
            simde_errorf("assertion failed: memory %s == %s, at offset %" SIMDE_SIZE_MODIFIER "u\n", \
                         #a, #b, simde_tmp_pos_); \
            break; \
          } \
        } \
      } \
      SIMDE_PUSH_DISABLE_MSVC_C4127_ \
    } while (0) \
    SIMDE_POP_DISABLE_MSVC_C4127_

#  define simde_assert_memory_not_equal(size, a, b) \
    do { \
      const unsigned char* simde_tmp_a_ = (const unsigned char*) (a); \
      const unsigned char* simde_tmp_b_ = (const unsigned char*) (b); \
      const size_t simde_tmp_size_ = (size); \
      if (HEDLEY_UNLIKELY(memcmp(simde_tmp_a_, simde_tmp_b_, simde_tmp_size_)) == 0) { \
        simde_errorf("assertion failed: memory %s != %s (%" SIMDE_SIZE_MODIFIER "u bytes)\n", \
                     #a, #b, simde_tmp_size_); \
      } \
      SIMDE_PUSH_DISABLE_MSVC_C4127_ \
    } while (0) \
    SIMDE_POP_DISABLE_MSVC_C4127_
#endif

#define simde_assert_type(T, fmt, a, op, b) \
  simde_assert_type_full("", "", T, fmt, a, op, b)

#define simde_assert_char(a, op, b) \
  simde_assert_type_full("'\\x", "'", char, "02" SIMDE_CHAR_MODIFIER "x", a, op, b)
#define simde_assert_uchar(a, op, b) \
  simde_assert_type_full("'\\x", "'", unsigned char, "02" SIMDE_CHAR_MODIFIER "x", a, op, b)
#define simde_assert_short(a, op, b) \
  simde_assert_type(short, SIMDE_SHORT_MODIFIER "d", a, op, b)
#define simde_assert_ushort(a, op, b) \
  simde_assert_type(unsigned short, SIMDE_SHORT_MODIFIER "u", a, op, b)
#define simde_assert_int(a, op, b) \
  simde_assert_type(int, "d", a, op, b)
#define simde_assert_uint(a, op, b) \
  simde_assert_type(unsigned int, "u", a, op, b)
#define simde_assert_long(a, op, b) \
  simde_assert_type(long int, "ld", a, op, b)
#define simde_assert_ulong(a, op, b) \
  simde_assert_type(unsigned long int, "lu", a, op, b)
#define simde_assert_llong(a, op, b) \
  simde_assert_type(long long int, "lld", a, op, b)
#define simde_assert_ullong(a, op, b) \
  simde_assert_type(unsigned long long int, "llu", a, op, b)

#define simde_assert_size(a, op, b) \
  simde_assert_type(size_t, SIMDE_SIZE_MODIFIER "u", a, op, b)

#define simde_assert_float(a, op, b) \
  simde_assert_type(float, "f", a, op, b)
#define simde_assert_double(a, op, b) \
  simde_assert_type(double, "g", a, op, b)
#define simde_assert_ptr(a, op, b) \
  simde_assert_type(const void*, "p", a, op, b)

#define simde_assert_int8(a, op, b) \
  simde_assert_type(int8_t, PRIi8, a, op, b)
#define simde_assert_uint8(a, op, b) \
  simde_assert_type(uint8_t, PRIu8, a, op, b)
#define simde_assert_int16(a, op, b) \
  simde_assert_type(int16_t, PRIi16, a, op, b)
#define simde_assert_uint16(a, op, b) \
  simde_assert_type(uint16_t, PRIu16, a, op, b)
#define simde_assert_int32(a, op, b) \
  simde_assert_type(int32_t, PRIi32, a, op, b)
#define simde_assert_uint32(a, op, b) \
  simde_assert_type(uint32_t, PRIu32, a, op, b)
#define simde_assert_int64(a, op, b) \
  simde_assert_type(int64_t, PRIi64, a, op, b)
#define simde_assert_uint64(a, op, b) \
  simde_assert_type(uint64_t, PRIu64, a, op, b)

#define simde_assert_ptr_equal(a, b) \
  simde_assert_ptr(a, ==, b)
#define simde_assert_ptr_not_equal(a, b) \
  simde_assert_ptr(a, !=, b)
#define simde_assert_null(ptr) \
  simde_assert_ptr(ptr, ==, NULL)
#define simde_assert_not_null(ptr) \
  simde_assert_ptr(ptr, !=, NULL)
#define simde_assert_ptr_null(ptr) \
  simde_assert_ptr(ptr, ==, NULL)
#define simde_assert_ptr_not_null(ptr) \
  simde_assert_ptr(ptr, !=, NULL)

#endif /* !defined(SIMDE_CHECK_H) */
/* :: End simde/check.h :: */

/* GCC/clang have a bunch of functionality in builtins which we would
 * like to access, but the suffixes indicate whether the operate on
 * int, long, or long long, not fixed width types (e.g., int32_t).
 * we use these macros to attempt to map from fixed-width to the
 * names GCC uses.  Note that you should still cast the input(s) and
 * return values (to/from SIMDE_BUILTIN_TYPE_*_) since often even if
 * types are the same size they may not be compatible according to the
 * compiler.  For example, on x86 long and long lonsg are generally
 * both 64 bits, but platforms vary on whether an int64_t is mapped
 * to a long or long long. */

#include <limits.h>

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DIAGNOSTIC_DISABLE_CPP98_COMPAT_PEDANTIC_

#if (INT8_MAX == INT_MAX) && (INT8_MIN == INT_MIN)
  #define SIMDE_BUILTIN_SUFFIX_8_
  #define SIMDE_BUILTIN_TYPE_8_ int
#elif (INT8_MAX == LONG_MAX) && (INT8_MIN == LONG_MIN)
  #define SIMDE_BUILTIN_SUFFIX_8_ l
  #define SIMDE_BUILTIN_TYPE_8_ long
#elif (INT8_MAX == LLONG_MAX) && (INT8_MIN == LLONG_MIN)
  #define SIMDE_BUILTIN_SUFFIX_8_ ll
  #define SIMDE_BUILTIN_TYPE_8_ long long
#endif

#if (INT16_MAX == INT_MAX) && (INT16_MIN == INT_MIN)
  #define SIMDE_BUILTIN_SUFFIX_16_
  #define SIMDE_BUILTIN_TYPE_16_ int
#elif (INT16_MAX == LONG_MAX) && (INT16_MIN == LONG_MIN)
  #define SIMDE_BUILTIN_SUFFIX_16_ l
  #define SIMDE_BUILTIN_TYPE_16_ long
#elif (INT16_MAX == LLONG_MAX) && (INT16_MIN == LLONG_MIN)
  #define SIMDE_BUILTIN_SUFFIX_16_ ll
  #define SIMDE_BUILTIN_TYPE_16_ long long
#endif

#if (INT32_MAX == INT_MAX) && (INT32_MIN == INT_MIN)
  #define SIMDE_BUILTIN_SUFFIX_32_
  #define SIMDE_BUILTIN_TYPE_32_ int
#elif (INT32_MAX == LONG_MAX) && (INT32_MIN == LONG_MIN)
  #define SIMDE_BUILTIN_SUFFIX_32_ l
  #define SIMDE_BUILTIN_TYPE_32_ long
#elif (INT32_MAX == LLONG_MAX) && (INT32_MIN == LLONG_MIN)
  #define SIMDE_BUILTIN_SUFFIX_32_ ll
  #define SIMDE_BUILTIN_TYPE_32_ long long
#endif

#if (INT64_MAX == INT_MAX) && (INT64_MIN == INT_MIN)
  #define SIMDE_BUILTIN_SUFFIX_64_
  #define SIMDE_BUILTIN_TYPE_64_ int
#elif (INT64_MAX == LONG_MAX) && (INT64_MIN == LONG_MIN)
  #define SIMDE_BUILTIN_SUFFIX_64_ l
  #define SIMDE_BUILTIN_TYPE_64_ long
#elif (INT64_MAX == LLONG_MAX) && (INT64_MIN == LLONG_MIN)
  #define SIMDE_BUILTIN_SUFFIX_64_ ll
  #define SIMDE_BUILTIN_TYPE_64_ long long
#endif

/* SIMDE_DIAGNOSTIC_DISABLE_CPP98_COMPAT_PEDANTIC_ */
HEDLEY_DIAGNOSTIC_POP

#if defined(SIMDE_BUILTIN_SUFFIX_8_)
  #define SIMDE_BUILTIN_8_(name) HEDLEY_CONCAT3(__builtin_, name, SIMDE_BUILTIN_SUFFIX_8_)
  #define SIMDE_BUILTIN_HAS_8_(name) HEDLEY_HAS_BUILTIN(HEDLEY_CONCAT3(__builtin_, name, SIMDE_BUILTIN_SUFFIX_8_))
#else
  #define SIMDE_BUILTIN_HAS_8_(name) 0
#endif
#if defined(SIMDE_BUILTIN_SUFFIX_16_)
  #define SIMDE_BUILTIN_16_(name) HEDLEY_CONCAT3(__builtin_, name, SIMDE_BUILTIN_SUFFIX_16_)
  #define SIMDE_BUILTIN_HAS_16_(name) HEDLEY_HAS_BUILTIN(HEDLEY_CONCAT3(__builtin_, name, SIMDE_BUILTIN_SUFFIX_16_))
#else
  #define SIMDE_BUILTIN_HAS_16_(name) 0
#endif
#if defined(SIMDE_BUILTIN_SUFFIX_32_)
  #define SIMDE_BUILTIN_32_(name) HEDLEY_CONCAT3(__builtin_, name, SIMDE_BUILTIN_SUFFIX_32_)
  #define SIMDE_BUILTIN_HAS_32_(name) HEDLEY_HAS_BUILTIN(HEDLEY_CONCAT3(__builtin_, name, SIMDE_BUILTIN_SUFFIX_32_))
#else
  #define SIMDE_BUILTIN_HAS_32_(name) 0
#endif
#if defined(SIMDE_BUILTIN_SUFFIX_64_)
  #define SIMDE_BUILTIN_64_(name) HEDLEY_CONCAT3(__builtin_, name, SIMDE_BUILTIN_SUFFIX_64_)
  #define SIMDE_BUILTIN_HAS_64_(name) HEDLEY_HAS_BUILTIN(HEDLEY_CONCAT3(__builtin_, name, SIMDE_BUILTIN_SUFFIX_64_))
#else
  #define SIMDE_BUILTIN_HAS_64_(name) 0
#endif

#if !defined(__cplusplus)
  #if defined(__clang__)
    #if HEDLEY_HAS_WARNING("-Wc11-extensions")
      #define SIMDE_GENERIC_(...) (__extension__ ({ \
          HEDLEY_DIAGNOSTIC_PUSH \
          _Pragma("clang diagnostic ignored \"-Wc11-extensions\"") \
          _Generic(__VA_ARGS__); \
          HEDLEY_DIAGNOSTIC_POP \
        }))
    #elif HEDLEY_HAS_WARNING("-Wc1x-extensions")
      #define SIMDE_GENERIC_(...) (__extension__ ({ \
          HEDLEY_DIAGNOSTIC_PUSH \
          _Pragma("clang diagnostic ignored \"-Wc1x-extensions\"") \
          _Generic(__VA_ARGS__); \
          HEDLEY_DIAGNOSTIC_POP \
        }))
    #endif
  #elif \
      defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) || \
      HEDLEY_HAS_EXTENSION(c_generic_selections) || \
      HEDLEY_GCC_VERSION_CHECK(4,9,0) || \
      HEDLEY_INTEL_VERSION_CHECK(17,0,0) || \
      HEDLEY_IBM_VERSION_CHECK(12,1,0) || \
      HEDLEY_ARM_VERSION_CHECK(5,3,0)
    #define SIMDE_GENERIC_(...) _Generic(__VA_ARGS__)
  #endif
#endif

/* Sometimes we run into problems with specific versions of compilers
   which make the native versions unusable for us.  Often this is due
   to missing functions, sometimes buggy implementations, etc.  These
   macros are how we check for specific bugs.  As they are fixed we'll
   start only defining them for problematic compiler versions. */

#if !defined(SIMDE_IGNORE_COMPILER_BUGS)
#  if defined(HEDLEY_GCC_VERSION)
#    if !HEDLEY_GCC_VERSION_CHECK(4,9,0)
#      define SIMDE_BUG_GCC_REV_208793
#    endif
#    if !HEDLEY_GCC_VERSION_CHECK(5,0,0)
#      define SIMDE_BUG_GCC_BAD_MM_SRA_EPI32 /* TODO: find relevant bug or commit */
#    endif
#    if !HEDLEY_GCC_VERSION_CHECK(6,0,0)
#      define SIMDE_BUG_GCC_SIZEOF_IMMEDIATE
#    endif
#    if !HEDLEY_GCC_VERSION_CHECK(4,6,0)
#      define SIMDE_BUG_GCC_BAD_MM_EXTRACT_EPI8 /* TODO: find relevant bug or commit */
#    endif
#    if !HEDLEY_GCC_VERSION_CHECK(7,4,0) || (HEDLEY_GCC_VERSION_CHECK(8,0,0) && !HEDLEY_GCC_VERSION_CHECK(8,3,0))
#      define SIMDE_BUG_GCC_87467
#    endif
#    if !HEDLEY_GCC_VERSION_CHECK(8,0,0)
#      define SIMDE_BUG_GCC_REV_247851
#    endif
#    if !HEDLEY_GCC_VERSION_CHECK(10,0,0)
#      define SIMDE_BUG_GCC_REV_274313
#      define SIMDE_BUG_GCC_91341
#      define SIMDE_BUG_GCC_92035
#    endif
#    if !HEDLEY_GCC_VERSION_CHECK(9,0,0) && defined(SIMDE_ARCH_AARCH64)
#      define SIMDE_BUG_GCC_ARM_SHIFT_SCALAR
#    endif
#    if !HEDLEY_GCC_VERSION_CHECK(9,0,0) && defined(SIMDE_ARCH_AARCH64)
#      define SIMDE_BUG_GCC_BAD_VEXT_REV32
#    endif
#    if !(HEDLEY_GCC_VERSION_CHECK(9,4,0) \
          || (HEDLEY_GCC_VERSION_CHECK(8,5,0) && !HEDLEY_GCC_VERSION_CHECK(9,0,0)) \
         ) && defined(SIMDE_ARCH_X86) && !defined(SIMDE_ARCH_AMD64)
#      define SIMDE_BUG_GCC_94482
#    endif
#    if (defined(SIMDE_ARCH_X86) && !defined(SIMDE_ARCH_AMD64)) || defined(SIMDE_ARCH_ZARCH)
#      define SIMDE_BUG_GCC_53784
#    endif
#    if defined(SIMDE_ARCH_X86) || defined(SIMDE_ARCH_AMD64)
#      if HEDLEY_GCC_VERSION_CHECK(4,3,0) /* -Wsign-conversion */
#        define SIMDE_BUG_GCC_95144
#      endif
#      if !HEDLEY_GCC_VERSION_CHECK(11,2,0)
#        define SIMDE_BUG_GCC_95483
#      endif
#      if defined(__OPTIMIZE__)
#        define SIMDE_BUG_GCC_100927
#      endif
#      if !(HEDLEY_GCC_VERSION_CHECK(10,3,0))
#        define SIMDE_BUG_GCC_98521
#      endif
#    endif
#    if !HEDLEY_GCC_VERSION_CHECK(9,4,0) && defined(SIMDE_ARCH_AARCH64)
#      define SIMDE_BUG_GCC_94488
#    endif
#    if !HEDLEY_GCC_VERSION_CHECK(9,1,0) && defined(SIMDE_ARCH_AARCH64)
#      define SIMDE_BUG_GCC_REV_264019
#    endif
#    if (!HEDLEY_GCC_VERSION_CHECK(9,0,0) && !defined(SIMDE_ARCH_AARCH64)) || (!defined(SIMDE_ARCH_AARCH64) && defined(SIMDE_ARCH_ARM))
#      define SIMDE_BUG_GCC_REV_260989
#    endif
#    if defined(SIMDE_ARCH_ARM) && !defined(SIMDE_ARCH_AARCH64)
#      define SIMDE_BUG_GCC_95399
#      define SIMDE_BUG_GCC_95471
#      define SIMDE_BUG_GCC_111609
#      if SIMDE_ARCH_ARM_CHECK(8,0)
#        define SIMDE_BUG_GCC_113065
#      endif
#    endif
#    if defined(SIMDE_ARCH_POWER)
#      define SIMDE_BUG_GCC_95227
#      define SIMDE_BUG_GCC_95782
#      if !HEDLEY_GCC_VERSION_CHECK(12,0,0)
#        define SIMDE_BUG_VEC_CPSGN_REVERSED_ARGS
#      endif
#    endif
#    if defined(SIMDE_ARCH_X86) || defined(SIMDE_ARCH_AMD64)
#      if !HEDLEY_GCC_VERSION_CHECK(10,2,0) && !defined(__OPTIMIZE__)
#        define SIMDE_BUG_GCC_96174
#      endif
#    endif
#    if defined(SIMDE_ARCH_ZARCH)
#      define SIMDE_BUG_GCC_95782
#      if HEDLEY_GCC_VERSION_CHECK(10,0,0)
#        define SIMDE_BUG_GCC_101614
#      endif
#    endif
#    if defined(SIMDE_ARCH_MIPS_MSA)
#      define SIMDE_BUG_GCC_97248
#      if !HEDLEY_GCC_VERSION_CHECK(12,1,0)
#        define SIMDE_BUG_GCC_100760
#        define SIMDE_BUG_GCC_100761
#        define SIMDE_BUG_GCC_100762
#      endif
#    endif
#    if !defined(__OPTIMIZE__) && !(\
       HEDLEY_GCC_VERSION_CHECK(11,4,0) \
       || (HEDLEY_GCC_VERSION_CHECK(10,4,0) && !(HEDLEY_GCC_VERSION_CHECK(11,0,0))) \
       || (HEDLEY_GCC_VERSION_CHECK(9,5,0) && !(HEDLEY_GCC_VERSION_CHECK(10,0,0))))
#      define SIMDE_BUG_GCC_105339
#    endif
#  elif defined(__clang__)
#    if defined(SIMDE_ARCH_AARCH64)
#      define SIMDE_BUG_CLANG_48257  // https://github.com/llvm/llvm-project/issues/47601
#      define SIMDE_BUG_CLANG_71362  // https://github.com/llvm/llvm-project/issues/71362
#      define SIMDE_BUG_CLANG_71365  // https://github.com/llvm/llvm-project/issues/71365
#      define SIMDE_BUG_CLANG_71751  // https://github.com/llvm/llvm-project/issues/71751
#      if !SIMDE_DETECT_CLANG_VERSION_CHECK(15,0,0)
#        define SIMDE_BUG_CLANG_45541
#      endif
#      if !SIMDE_DETECT_CLANG_VERSION_CHECK(12,0,0)
#        define SIMDE_BUG_CLANG_46840
#        define SIMDE_BUG_CLANG_46844
#      endif
#      if SIMDE_DETECT_CLANG_VERSION_CHECK(10,0,0) && SIMDE_DETECT_CLANG_VERSION_NOT(11,0,0)
#        define SIMDE_BUG_CLANG_BAD_VI64_OPS
#      endif
#      if SIMDE_DETECT_CLANG_VERSION_NOT(9,0,0)
#        define SIMDE_BUG_CLANG_GIT_4EC445B8
#        define SIMDE_BUG_CLANG_REV_365298 /* 0464e07c8f6e3310c28eb210a4513bc2243c2a7e */
#      endif
#    endif
#    if defined(SIMDE_ARCH_ARM)
#      if !SIMDE_DETECT_CLANG_VERSION_CHECK(11,0,0)
#        define SIMDE_BUG_CLANG_BAD_VGET_SET_LANE_TYPES
#      endif
#      if defined(SIMDE_ARM_NEON_A32V7_NATIVE) && !defined(SIMDE_ARM_NEON_A32V8_NATIVE)
#        define SIMDE_BUG_CLANG_71763  // https://github.com/llvm/llvm-project/issues/71763
#      endif
#    endif
#    if defined(SIMDE_ARCH_POWER) && !SIMDE_DETECT_CLANG_VERSION_CHECK(12,0,0)
#      define SIMDE_BUG_CLANG_46770
#    endif
#    if defined(SIMDE_ARCH_POWER) && (SIMDE_ARCH_POWER == 700) && (SIMDE_DETECT_CLANG_VERSION_CHECK(11,0,0))
#      if !SIMDE_DETECT_CLANG_VERSION_CHECK(13,0,0)
#        define SIMDE_BUG_CLANG_50893
#        define SIMDE_BUG_CLANG_50901
#      endif
#    endif
#    if defined(_ARCH_PWR9) && !SIMDE_DETECT_CLANG_VERSION_CHECK(12,0,0) && !defined(__OPTIMIZE__)
#      define SIMDE_BUG_CLANG_POWER9_16x4_BAD_SHIFT
#    endif
#    if defined(SIMDE_ARCH_POWER)
#      if !SIMDE_DETECT_CLANG_VERSION_CHECK(14,0,0)
#        define SIMDE_BUG_CLANG_50932
#      endif
#      if !SIMDE_DETECT_CLANG_VERSION_CHECK(12,0,0)
#        define SIMDE_BUG_VEC_CPSGN_REVERSED_ARGS
#      endif
#    endif
#    if defined(SIMDE_ARCH_X86) || defined(SIMDE_ARCH_AMD64)
#      if SIMDE_DETECT_CLANG_VERSION_NOT(5,0,0)
#        define SIMDE_BUG_CLANG_REV_298042 /* 6afc436a7817a52e78ae7bcdc3faafd460124cac */
#      endif
#      if SIMDE_DETECT_CLANG_VERSION_NOT(3,7,0)
#        define SIMDE_BUG_CLANG_REV_234560 /* b929ad7b1726a32650a8051f69a747fb6836c540 */
#      endif
#      if SIMDE_DETECT_CLANG_VERSION_CHECK(3,8,0) && SIMDE_DETECT_CLANG_VERSION_NOT(5,0,0)
#        define SIMDE_BUG_CLANG_BAD_MADD
#      endif
#      if SIMDE_DETECT_CLANG_VERSION_CHECK(4,0,0) && SIMDE_DETECT_CLANG_VERSION_NOT(5,0,0)
#        define SIMDE_BUG_CLANG_REV_299346 /* ac9959eb533a58482ea4da6c4db1e635a98de384 */
#      endif
#      if SIMDE_DETECT_CLANG_VERSION_NOT(8,0,0)
#        define SIMDE_BUG_CLANG_REV_344862 /* eae26bf73715994c2bd145f9b6dc3836aa4ffd4f */
#      endif
#      if HEDLEY_HAS_WARNING("-Wsign-conversion") && SIMDE_DETECT_CLANG_VERSION_NOT(11,0,0)
#        define SIMDE_BUG_CLANG_45931
#      endif
#      if HEDLEY_HAS_WARNING("-Wvector-conversion") && SIMDE_DETECT_CLANG_VERSION_NOT(11,0,0)
#        define SIMDE_BUG_CLANG_44589
#      endif
#      define SIMDE_BUG_CLANG_48673  // https://github.com/llvm/llvm-project/issues/48017
#    endif
#    define SIMDE_BUG_CLANG_45959  // https://github.com/llvm/llvm-project/issues/45304
#    if defined(SIMDE_ARCH_WASM_SIMD128) && !SIMDE_DETECT_CLANG_VERSION_CHECK(17,0,0)
#      define SIMDE_BUG_CLANG_60655
#    endif
#  elif defined(HEDLEY_MSVC_VERSION)
#    if defined(SIMDE_ARCH_X86)
#      define SIMDE_BUG_MSVC_ROUND_EXTRACT
#    endif
#  elif defined(HEDLEY_INTEL_VERSION)
#    define SIMDE_BUG_INTEL_857088
#  elif defined(HEDLEY_MCST_LCC_VERSION)
#    define SIMDE_BUG_MCST_LCC_MISSING_AVX_LOAD_STORE_M128_FUNCS
#    define SIMDE_BUG_MCST_LCC_MISSING_CMOV_M256
#    define SIMDE_BUG_MCST_LCC_FMA_WRONG_RESULT
#  elif defined(HEDLEY_PGI_VERSION)
#    define SIMDE_BUG_PGI_30104
#    define SIMDE_BUG_PGI_30107
#    define SIMDE_BUG_PGI_30106
#  endif
#endif

/* GCC and Clang both have the same issue:
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=95144
 * https://bugs.llvm.org/show_bug.cgi?id=45931
 * This is just an easy way to work around it.
 */
#if \
    (HEDLEY_HAS_WARNING("-Wsign-conversion") && SIMDE_DETECT_CLANG_VERSION_NOT(11,0,0)) || \
    HEDLEY_GCC_VERSION_CHECK(4,3,0)
#  define SIMDE_BUG_IGNORE_SIGN_CONVERSION(expr) (__extension__ ({ \
       HEDLEY_DIAGNOSTIC_PUSH  \
       _Pragma("GCC diagnostic ignored \"-Wsign-conversion\"") \
       __typeof__(expr) simde_bug_ignore_sign_conversion_v_= (expr); \
       HEDLEY_DIAGNOSTIC_POP  \
       simde_bug_ignore_sign_conversion_v_; \
     }))
#else
#  define SIMDE_BUG_IGNORE_SIGN_CONVERSION(expr) (expr)
#endif

/* Usually the shift count is signed (for example, NEON or SSE).
 * OTOH, unsigned is good for PPC (vec_srl uses unsigned), and the only option for E2K.
 * Further info: https://github.com/simd-everywhere/simde/pull/700
 */
#if defined(SIMDE_ARCH_E2K) || defined(SIMDE_ARCH_POWER)
  #define SIMDE_CAST_VECTOR_SHIFT_COUNT(width, value) HEDLEY_STATIC_CAST(uint##width##_t, (value))
#else
  #define SIMDE_CAST_VECTOR_SHIFT_COUNT(width, value) HEDLEY_STATIC_CAST(int##width##_t, (value))
#endif

/* Initial support for RISCV V extensions based on ZVE64D. */
#if defined(SIMDE_ARCH_RISCV_ZVE64D) && SIMDE_NATURAL_VECTOR_SIZE >= 64
  #define RVV_FIXED_TYPE_DEF(name, lmul) \
    typedef vint8##name##_t  fixed_vint8##name##_t __attribute__((riscv_rvv_vector_bits(__riscv_v_fixed_vlen * lmul))); \
    typedef vint16##name##_t fixed_vint16##name##_t __attribute__((riscv_rvv_vector_bits(__riscv_v_fixed_vlen * lmul))); \
    typedef vint32##name##_t fixed_vint32##name##_t __attribute__((riscv_rvv_vector_bits(__riscv_v_fixed_vlen * lmul))); \
    typedef vuint8##name##_t fixed_vuint8##name##_t __attribute__((riscv_rvv_vector_bits(__riscv_v_fixed_vlen * lmul))); \
    typedef vuint16##name##_t fixed_vuint16##name##_t __attribute__((riscv_rvv_vector_bits(__riscv_v_fixed_vlen * lmul))); \
    typedef vuint32##name##_t fixed_vuint32##name##_t __attribute__((riscv_rvv_vector_bits(__riscv_v_fixed_vlen * lmul))); \
    typedef vfloat32##name##_t fixed_vfloat32##name##_t __attribute__((riscv_rvv_vector_bits(__riscv_v_fixed_vlen * lmul)));
    RVV_FIXED_TYPE_DEF(mf2, 1/2);
    RVV_FIXED_TYPE_DEF(m1, 1);
    RVV_FIXED_TYPE_DEF(m2, 2);
  #define RVV_FIXED_TYPE_DEF_64B(name, lmul) \
    typedef vint64##name##_t fixed_vint64##name##_t __attribute__((riscv_rvv_vector_bits(__riscv_v_fixed_vlen * lmul))); \
    typedef vuint64##name##_t fixed_vuint64##name##_t __attribute__((riscv_rvv_vector_bits(__riscv_v_fixed_vlen * lmul))); \
    typedef vfloat64##name##_t fixed_vfloat64##name##_t __attribute__((riscv_rvv_vector_bits(__riscv_v_fixed_vlen * lmul)));
    RVV_FIXED_TYPE_DEF_64B(m1, 1);
    RVV_FIXED_TYPE_DEF_64B(m2, 2);
  #if defined(SIMDE_ARCH_RISCV_ZVFH)
    #define RVV_FIXED_TYPE_DEF_16F(name, lmul) \
      typedef vfloat16##name##_t fixed_vfloat16##name##_t __attribute__((riscv_rvv_vector_bits(__riscv_v_fixed_vlen * lmul)));
    RVV_FIXED_TYPE_DEF_16F(mf2, 1/2);
    RVV_FIXED_TYPE_DEF_16F(m1, 1);
    RVV_FIXED_TYPE_DEF_16F(m2, 2);
  #endif
#endif

/* SIMDE_DIAGNOSTIC_DISABLE_USED_BUT_MARKED_UNUSED_ */
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_COMMON_H) */
/* :: End simde/simde-common.h :: */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/simde-f16.h :: */
/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2021      Evan Nemerson <evan@nemerson.com>
 *   2023      Ju-Hung Li <jhlee@pllab.cs.nthu.edu.tw> (Copyright owned by NTHU pllab)
 */

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */

#if !defined(SIMDE_FLOAT16_H)
#define SIMDE_FLOAT16_H

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

/* Portable version which should work on pretty much any compiler.
 * Obviously you can't rely on compiler support for things like
 * conversion to/from 32-bit floats, so make sure you always use the
 * functions and macros in this file!
 *
 * The portable implementations are (heavily) based on CC0 code by
 * Fabian Giesen: <https://gist.github.com/rygorous/2156668> (see also
 * <https://fgiesen.wordpress.com/2012/03/28/half-to-float-done-quic/>).
 * I have basically just modified it to get rid of some UB (lots of
 * aliasing, right shifting a negative value), use fixed-width types,
 * and work in C. */
#define SIMDE_FLOAT16_API_PORTABLE 1
/* _Float16, per C standard (TS 18661-3;
 * <http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1945.pdf>). */
#define SIMDE_FLOAT16_API_FLOAT16 2
/* clang >= 6.0 supports __fp16 as an interchange format on all
 * targets, but only allows you to use them for arguments and return
 * values on targets which have defined an ABI.  We get around the
 * restriction by wrapping the __fp16 in a struct, but we can't do
 * that on Arm since it would break compatibility with the NEON F16
 * functions. */
#define SIMDE_FLOAT16_API_FP16_NO_ABI 3
/* This is basically __fp16 as specified by Arm, where arguments and
 * return values are raw __fp16 values not structs. */
#define SIMDE_FLOAT16_API_FP16 4

/* Choosing an implementation.  This is a bit rough, but I don't have
 * any ideas on how to improve it.  If you do, patches are definitely
 * welcome. */
#if !defined(SIMDE_FLOAT16_API)
  #if defined(__ARM_FP16_FORMAT_IEEE) && (defined(SIMDE_ARM_NEON_FP16) || defined(__ARM_FP16_ARGS))
    #define SIMDE_FLOAT16_API SIMDE_FLOAT16_API_FP16
  #elif !defined(__EMSCRIPTEN__) && !(defined(__clang__) && defined(SIMDE_ARCH_POWER)) && \
    !(defined(HEDLEY_MSVC_VERSION) && defined(__clang__)) && \
    !(defined(SIMDE_ARCH_MIPS) && defined(__clang__)) && \
    !(defined(__clang__) && defined(SIMDE_ARCH_RISCV64)) && ( \
      defined(SIMDE_X86_AVX512FP16_NATIVE) || \
      (defined(SIMDE_ARCH_X86_SSE2) && HEDLEY_GCC_VERSION_CHECK(12,0,0)) || \
      (defined(SIMDE_ARCH_AARCH64) && HEDLEY_GCC_VERSION_CHECK(7,0,0) && !defined(__cplusplus)) || \
      ((defined(SIMDE_ARCH_X86) || defined(SIMDE_ARCH_AMD64)) && SIMDE_DETECT_CLANG_VERSION_CHECK(15,0,0)) || \
      (!(defined(SIMDE_ARCH_X86) || defined(SIMDE_ARCH_AMD64)) && SIMDE_DETECT_CLANG_VERSION_CHECK(6,0,0))) || \
      defined(SIMDE_ARCH_RISCV_ZVFH)
    /* We haven't found a better way to detect this.  It seems like defining
    * __STDC_WANT_IEC_60559_TYPES_EXT__, then including float.h, then
    * checking for defined(FLT16_MAX) should work, but both gcc and
    * clang will define the constants even if _Float16 is not
    * supported.  Ideas welcome. */
    #define SIMDE_FLOAT16_API SIMDE_FLOAT16_API_FLOAT16
  #elif defined(__FLT16_MIN__) && \
      (defined(__clang__) && \
      (!defined(SIMDE_ARCH_AARCH64) || SIMDE_DETECT_CLANG_VERSION_CHECK(7,0,0)) \
      && !defined(SIMDE_ARCH_RISCV64))
    #define SIMDE_FLOAT16_API SIMDE_FLOAT16_API_FP16_NO_ABI
  #else
    #define SIMDE_FLOAT16_API SIMDE_FLOAT16_API_PORTABLE
  #endif
#endif

#if SIMDE_FLOAT16_API == SIMDE_FLOAT16_API_FLOAT16
  typedef _Float16 simde_float16;
  #define SIMDE_FLOAT16_IS_SCALAR 1
  #if !defined(__cplusplus)
    #define SIMDE_FLOAT16_C(value) value##f16
  #else
    #define SIMDE_FLOAT16_C(value) HEDLEY_STATIC_CAST(_Float16, (value))
  #endif
#elif SIMDE_FLOAT16_API == SIMDE_FLOAT16_API_FP16_NO_ABI
  typedef struct { __fp16 value; } simde_float16;
  #if defined(SIMDE_STATEMENT_EXPR_) && !defined(SIMDE_TESTS_H)
    #define SIMDE_FLOAT16_C(value) (__extension__({ ((simde_float16) { HEDLEY_DIAGNOSTIC_PUSH SIMDE_DIAGNOSTIC_DISABLE_C99_EXTENSIONS_ HEDLEY_STATIC_CAST(__fp16, (value)) }); HEDLEY_DIAGNOSTIC_POP }))
  #else
    #define SIMDE_FLOAT16_C(value) ((simde_float16) { HEDLEY_STATIC_CAST(__fp16, (value)) })
    #define SIMDE_FLOAT16_IS_SCALAR 1
  #endif
#elif SIMDE_FLOAT16_API == SIMDE_FLOAT16_API_FP16
  typedef __fp16 simde_float16;
  #define SIMDE_FLOAT16_IS_SCALAR 1
  #define SIMDE_FLOAT16_C(value) HEDLEY_STATIC_CAST(__fp16, (value))
#elif SIMDE_FLOAT16_API == SIMDE_FLOAT16_API_PORTABLE
  typedef struct { uint16_t value; } simde_float16;
#else
  #error No 16-bit floating point API.
#endif

#if \
    defined(SIMDE_VECTOR_OPS) && \
    (SIMDE_FLOAT16_API != SIMDE_FLOAT16_API_PORTABLE) && \
    (SIMDE_FLOAT16_API != SIMDE_FLOAT16_API_FP16_NO_ABI)
  #define SIMDE_FLOAT16_VECTOR
#endif

/* Reinterpret -- you *generally* shouldn't need these, they're really
 * intended for internal use.  However, on x86 half-precision floats
 * get stuffed into a __m128i/__m256i, so it may be useful. */

SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_float16_as_uint16,      uint16_t, simde_float16)
SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint16_as_float16, simde_float16,      uint16_t)

#if SIMDE_FLOAT16_API == SIMDE_FLOAT16_API_PORTABLE
  #define SIMDE_NANHF simde_uint16_as_float16(0x7E00) // a quiet Not-a-Number
  #define SIMDE_INFINITYHF simde_uint16_as_float16(0x7C00)
  #define SIMDE_NINFINITYHF simde_uint16_as_float16(0xFC00)
#else
  #if SIMDE_FLOAT16_API == SIMDE_FLOAT16_API_FP16_NO_ABI
    #if SIMDE_MATH_BUILTIN_LIBM(nanf16)
      #define SIMDE_NANHF SIMDE_FLOAT16_C(__builtin_nanf16(""))
    #elif defined(SIMDE_MATH_NAN)
      #define SIMDE_NANHF SIMDE_FLOAT16_C(SIMDE_MATH_NAN)
    #endif
    #if SIMDE_MATH_BUILTIN_LIBM(inf16)
      #define SIMDE_INFINITYHF SIMDE_FLOAT16_C(__builtin_inf16())
      #define SIMDE_NINFINITYHF SIMDE_FLOAT16_C(-__builtin_inf16())
    #else
      #define SIMDE_INFINITYHF SIMDE_FLOAT16_C(SIMDE_MATH_INFINITY)
      #define SIMDE_NINFINITYHF SIMDE_FLOAT16_C(-SIMDE_MATH_INFINITY)
    #endif
  #else
    #if SIMDE_MATH_BUILTIN_LIBM(nanf16)
      #define SIMDE_NANHF  __builtin_nanf16("")
    #elif defined(SIMDE_MATH_NAN)
      #define SIMDE_NANHF SIMDE_MATH_NAN
    #endif
    #if SIMDE_MATH_BUILTIN_LIBM(inf16)
      #define SIMDE_INFINITYHF __builtin_inf16()
      #define SIMDE_NINFINITYHF -(__builtin_inf16())
    #else
      #define SIMDE_INFINITYHF HEDLEY_STATIC_CAST(simde_float16, SIMDE_MATH_INFINITY)
      #define SIMDE_NINFINITYHF HEDLEY_STATIC_CAST(simde_float16, -SIMDE_MATH_INFINITY)
    #endif
  #endif
#endif

/* Conversion -- convert between single-precision and half-precision
 * floats. */
static HEDLEY_ALWAYS_INLINE HEDLEY_CONST
simde_float16
simde_float16_from_float32 (simde_float32 value) {
  simde_float16 res;

  #if \
      (SIMDE_FLOAT16_API == SIMDE_FLOAT16_API_FLOAT16) || \
      (SIMDE_FLOAT16_API == SIMDE_FLOAT16_API_FP16)
    res = HEDLEY_STATIC_CAST(simde_float16, value);
  #elif (SIMDE_FLOAT16_API == SIMDE_FLOAT16_API_FP16_NO_ABI)
    res.value = HEDLEY_STATIC_CAST(__fp16, value);
  #else
    /* This code is CC0, based heavily on code by Fabian Giesen. */
    uint32_t f32u = simde_float32_as_uint32(value);
    static const uint32_t f32u_infty = UINT32_C(255) << 23;
    static const uint32_t f16u_max = (UINT32_C(127) + UINT32_C(16)) << 23;
    static const uint32_t denorm_magic =
      ((UINT32_C(127) - UINT32_C(15)) + (UINT32_C(23) - UINT32_C(10)) + UINT32_C(1)) << 23;
    uint16_t f16u;

    uint32_t sign = f32u & (UINT32_C(1) << 31);
    f32u ^= sign;

   /* NOTE all the integer compares in this function cast the operands
    * to signed values to help compilers vectorize to SSE2, which lacks
    * unsigned comparison instructions.  This is fine since all
    * operands are below 0x80000000 (we clear the sign bit). */

    if (f32u > f16u_max) { /* result is Inf or NaN (all exponent bits set) */
      f16u = (f32u > f32u_infty) ?  UINT32_C(0x7e00) : UINT32_C(0x7c00); /* NaN->qNaN and Inf->Inf */
    } else { /* (De)normalized number or zero */
      if (f32u < (UINT32_C(113) << 23)) { /* resulting FP16 is subnormal or zero */
        /* use a magic value to align our 10 mantissa bits at the bottom of
        * the float. as long as FP addition is round-to-nearest-even this
        * just works. */
        f32u = simde_float32_as_uint32(simde_uint32_as_float32(f32u) + simde_uint32_as_float32(denorm_magic));

        /* and one integer subtract of the bias later, we have our final float! */
        f16u = HEDLEY_STATIC_CAST(uint16_t, f32u - denorm_magic);
      } else {
        uint32_t mant_odd = (f32u >> 13) & 1;

        /* update exponent, rounding bias part 1 */
        f32u += (HEDLEY_STATIC_CAST(uint32_t, 15 - 127) << 23) + UINT32_C(0xfff);
        /* rounding bias part 2 */
        f32u += mant_odd;
        /* take the bits! */
        f16u = HEDLEY_STATIC_CAST(uint16_t, f32u >> 13);
      }
    }

    f16u |= sign >> 16;
    res = simde_uint16_as_float16(f16u);
  #endif

  return res;
}

static HEDLEY_ALWAYS_INLINE HEDLEY_CONST
simde_float32
simde_float16_to_float32 (simde_float16 value) {
  simde_float32 res;

  #if defined(SIMDE_FLOAT16_FLOAT16) || defined(SIMDE_FLOAT16_FP16)
    res = HEDLEY_STATIC_CAST(simde_float32, value);
  #else
    /* This code is CC0, based heavily on code by Fabian Giesen. */
    uint16_t half = simde_float16_as_uint16(value);
    const simde_float32 denorm_magic = simde_uint32_as_float32((UINT32_C(113) << 23));
    const uint32_t shifted_exp = UINT32_C(0x7c00) << 13; /* exponent mask after shift */
    uint32_t f32u;

    f32u = (half & UINT32_C(0x7fff)) << 13; /* exponent/mantissa bits */
    uint32_t exp = shifted_exp & f32u; /* just the exponent */
    f32u += (UINT32_C(127) - UINT32_C(15)) << 23; /* exponent adjust */

    /* handle exponent special cases */
    if (exp == shifted_exp) /* Inf/NaN? */
      f32u += (UINT32_C(128) - UINT32_C(16)) << 23; /* extra exp adjust */
    else if (exp == 0) { /* Zero/Denormal? */
      f32u += (1) << 23; /* extra exp adjust */
      f32u = simde_float32_as_uint32(simde_uint32_as_float32(f32u) - denorm_magic); /* renormalize */
    }

    f32u |= (half & UINT32_C(0x8000)) << 16; /* sign bit */
    res = simde_uint32_as_float32(f32u);
  #endif

  return res;
}

#ifdef SIMDE_FLOAT16_C
  #define SIMDE_FLOAT16_VALUE(value) SIMDE_FLOAT16_C(value)
#else
  #define SIMDE_FLOAT16_VALUE(value) simde_float16_from_float32(SIMDE_FLOAT32_C(value))
#endif

#if !defined(simde_isinfhf) && defined(simde_math_isinff)
  #define simde_isinfhf(a) simde_math_isinff(simde_float16_to_float32(a))
#endif
#if !defined(simde_isnanhf) && defined(simde_math_isnanf)
  #define simde_isnanhf(a) simde_math_isnanf(simde_float16_to_float32(a))
#endif
#if !defined(simde_isnormalhf) && defined(simde_math_isnormalf)
  #define simde_isnormalhf(a) simde_math_isnormalf(simde_float16_to_float32(a))
#endif
#if !defined(simde_issubnormalhf) && defined(simde_math_issubnormalf)
  #define simde_issubnormalhf(a) simde_math_issubnormalf(simde_float16_to_float32(a))
#endif

#define simde_fpclassifyhf(a) simde_math_fpclassifyf(simde_float16_to_float32(a))

static HEDLEY_INLINE
uint8_t
simde_fpclasshf(simde_float16 v, const int imm8) {
  uint16_t bits = simde_float16_as_uint16(v);
  uint8_t negative = (bits >> 15) & 1;
  uint16_t const ExpMask = 0x7C00; // [14:10]
  uint16_t const MantMask = 0x03FF; // [9:0]
  uint8_t exponent_all_ones = ((bits & ExpMask) == ExpMask);
  uint8_t exponent_all_zeros = ((bits & ExpMask) == 0);
  uint8_t mantissa_all_zeros = ((bits & MantMask) == 0);
  uint8_t zero = exponent_all_zeros & mantissa_all_zeros;
  uint8_t signaling_bit = (bits >> 9) & 1;

  uint8_t result = 0;
  uint8_t snan = exponent_all_ones & (!mantissa_all_zeros) & (!signaling_bit);
  uint8_t qnan = exponent_all_ones & (!mantissa_all_zeros) & signaling_bit;
  uint8_t positive_zero = (!negative) & zero;
  uint8_t negative_zero = negative & zero;
  uint8_t positive_infinity = (!negative) & exponent_all_ones & mantissa_all_zeros;
  uint8_t negative_infinity = negative & exponent_all_ones & mantissa_all_zeros;
  uint8_t denormal = exponent_all_zeros & (!mantissa_all_zeros);
  uint8_t finite_negative = negative & (!exponent_all_ones) & (!zero);
  result = (((imm8 >> 0) & qnan)              | \
            ((imm8 >> 1) & positive_zero)     | \
            ((imm8 >> 2) & negative_zero)     | \
            ((imm8 >> 3) & positive_infinity) | \
            ((imm8 >> 4) & negative_infinity) | \
            ((imm8 >> 5) & denormal)          | \
            ((imm8 >> 6) & finite_negative)   | \
            ((imm8 >> 7) & snan));
  return result;
}

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_FLOAT16_H) */
/* :: End simde/simde-f16.h :: */

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

#if defined(SIMDE_VECTOR_SUBSCRIPT)
  #define SIMDE_ARM_SVE_DECLARE_VECTOR(Element_Type, Name, Vector_Size) Element_Type Name SIMDE_VECTOR(Vector_Size)
#else
  #define SIMDE_ARM_SVE_DECLARE_VECTOR(Element_Type, Name, Vector_Size) Element_Type Name[(Vector_Size) / sizeof(Element_Type)]
#endif

#if defined(SIMDE_ARM_SVE_NATIVE)
  typedef     svbool_t      simde_svbool_t;
  typedef     svint8_t      simde_svint8_t;
  typedef    svint16_t     simde_svint16_t;
  typedef    svint32_t     simde_svint32_t;
  typedef    svint64_t     simde_svint64_t;
  typedef    svuint8_t     simde_svuint8_t;
  typedef   svuint16_t    simde_svuint16_t;
  typedef   svuint32_t    simde_svuint32_t;
  typedef   svuint64_t    simde_svuint64_t;
  #if defined(__ARM_FEATURE_SVE_BF16)
    typedef svbfloat16_t simde_svbfloat16_t;
  #endif
  typedef  svfloat16_t  simde_svfloat16_t;
  typedef  svfloat32_t  simde_svfloat32_t;
  typedef  svfloat64_t  simde_svfloat64_t;
  typedef    float32_t    simde_float32_t;
  typedef    float64_t    simde_float64_t;
#else
  #if SIMDE_NATURAL_VECTOR_SIZE > 0
    #define SIMDE_ARM_SVE_VECTOR_SIZE SIMDE_NATURAL_VECTOR_SIZE
  #else
    #define SIMDE_ARM_SVE_VECTOR_SIZE (128)
  #endif

  typedef simde_float32 simde_float32_t;
  typedef simde_float64 simde_float64_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(int8_t, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512i m512i;
    #endif
    #if defined(SIMDE_X86_AVX2_NATIVE)
      __m256i m256i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256i)];
    #endif
    #if defined(SIMDE_X86_SSE2_NATIVE)
      __m128i m128i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128i)];
    #endif

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      int8x16_t neon;
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(signed char) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svint8_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(int16_t, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512i m512i;
    #endif
    #if defined(SIMDE_X86_AVX2_NATIVE)
      __m256i m256i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256i)];
    #endif
    #if defined(SIMDE_X86_SSE2_NATIVE)
      __m128i m128i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128i)];
    #endif

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      int16x8_t neon;
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(signed short) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svint16_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(int32_t, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512i m512i;
    #endif
    #if defined(SIMDE_X86_AVX2_NATIVE)
      __m256i m256i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256i)];
    #endif
    #if defined(SIMDE_X86_SSE2_NATIVE)
      __m128i m128i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128i)];
    #endif

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      int32x4_t neon;
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(signed int) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svint32_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(int64_t, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512i m512i;
    #endif
    #if defined(SIMDE_X86_AVX2_NATIVE)
      __m256i m256i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256i)];
    #endif
    #if defined(SIMDE_X86_SSE2_NATIVE)
      __m128i m128i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128i)];
    #endif

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      int64x2_t neon;
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(signed long long int) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svint64_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(uint8_t, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512i m512i;
    #endif
    #if defined(SIMDE_X86_AVX2_NATIVE)
      __m256i m256i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256i)];
    #endif
    #if defined(SIMDE_X86_SSE2_NATIVE)
      __m128i m128i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128i)];
    #endif

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      uint8x16_t neon;
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(unsigned char) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svuint8_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(uint16_t, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512i m512i;
    #endif
    #if defined(SIMDE_X86_AVX2_NATIVE)
      __m256i m256i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256i)];
    #endif
    #if defined(SIMDE_X86_SSE2_NATIVE)
      __m128i m128i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128i)];
    #endif

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      uint16x8_t neon;
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(unsigned short) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svuint16_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(uint32_t, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512i m512i;
    #endif
    #if defined(SIMDE_X86_AVX2_NATIVE)
      __m256i m256i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256i)];
    #endif
    #if defined(SIMDE_X86_SSE2_NATIVE)
      __m128i m128i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128i)];
    #endif

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      uint32x4_t neon;
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(unsigned int) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svuint32_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(uint64_t, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512i m512i;
    #endif
    #if defined(SIMDE_X86_AVX2_NATIVE)
      __m256i m256i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256i)];
    #endif
    #if defined(SIMDE_X86_SSE2_NATIVE)
      __m128i m128i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128i)];
    #endif

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      uint64x2_t neon;
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long int) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svuint64_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(uint16_t, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512i m512i;
    #endif
    #if defined(SIMDE_X86_AVX2_NATIVE)
      __m256i m256i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256i)];
    #endif
    #if defined(SIMDE_X86_SSE2_NATIVE)
      __m128i m128i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128i)];
    #endif

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE) && defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
      float16x8_t neon;
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(unsigned short) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svfloat16_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(uint16_t, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512i m512i;
    #endif
    #if defined(SIMDE_X86_AVX2_NATIVE)
      __m256i m256i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256i)];
    #endif
    #if defined(SIMDE_X86_SSE2_NATIVE)
      __m128i m128i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128i)];
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(unsigned short) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svbfloat16_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(simde_float32, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512 m512;
    #endif
    #if defined(SIMDE_X86_AVX_NATIVE)
      __m256 m256[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256)];
    #endif
    #if defined(SIMDE_X86_SSE_NATIVE)
      __m128 m128[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128)];
    #endif

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      float32x4_t neon;
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(float) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svfloat32_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(simde_float64, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512d m512d;
    #endif
    #if defined(SIMDE_X86_AVX2_NATIVE)
      __m256d m256d[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256d)];
    #endif
    #if defined(SIMDE_X86_SSE2_NATIVE)
      __m128d m128d[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128d)];
    #endif

    #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      float64x2_t neon;
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(double) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svfloat64_t;

  #if defined(SIMDE_X86_AVX512BW_NATIVE) && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    typedef struct {
      __mmask64 value;
      int       type;
    } simde_svbool_t;

    #if defined(__BMI2__)
      static const uint64_t simde_arm_sve_mask_bp_lo_ = UINT64_C(0x5555555555555555);
      static const uint64_t simde_arm_sve_mask_bp_hi_ = UINT64_C(0xaaaaaaaaaaaaaaaa);

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask64
      simde_arm_sve_mmask32_to_mmask64(__mmask32 m) {
        return HEDLEY_STATIC_CAST(__mmask64,
          _pdep_u64(HEDLEY_STATIC_CAST(uint64_t, m), simde_arm_sve_mask_bp_lo_) |
          _pdep_u64(HEDLEY_STATIC_CAST(uint64_t, m), simde_arm_sve_mask_bp_hi_));
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask32
      simde_arm_sve_mmask16_to_mmask32(__mmask16 m) {
        return HEDLEY_STATIC_CAST(__mmask32,
          _pdep_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_lo_)) |
          _pdep_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_hi_)));
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask16
      simde_arm_sve_mmask8_to_mmask16(__mmask8 m) {
        return HEDLEY_STATIC_CAST(__mmask16,
          _pdep_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_lo_)) |
          _pdep_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_hi_)));
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask8
      simde_arm_sve_mmask4_to_mmask8(__mmask8 m) {
        return HEDLEY_STATIC_CAST(__mmask8,
          _pdep_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_lo_)) |
          _pdep_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_hi_)));
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask32
      simde_arm_sve_mmask64_to_mmask32(__mmask64 m) {
        return HEDLEY_STATIC_CAST(__mmask32,
          _pext_u64(HEDLEY_STATIC_CAST(uint64_t, m), HEDLEY_STATIC_CAST(uint64_t, simde_arm_sve_mask_bp_lo_)) &
          _pext_u64(HEDLEY_STATIC_CAST(uint64_t, m), HEDLEY_STATIC_CAST(uint64_t, simde_arm_sve_mask_bp_hi_)));
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask16
      simde_arm_sve_mmask32_to_mmask16(__mmask32 m) {
        return HEDLEY_STATIC_CAST(__mmask16,
          _pext_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_lo_)) &
          _pext_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_hi_)));
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask8
      simde_arm_sve_mmask16_to_mmask8(__mmask16 m) {
        return HEDLEY_STATIC_CAST(__mmask8,
          _pext_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_lo_)) &
          _pext_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_hi_)));
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask8
      simde_arm_sve_mmask8_to_mmask4(__mmask8 m) {
        return HEDLEY_STATIC_CAST(__mmask8,
          _pext_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_lo_)) &
          _pext_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_hi_)));
      }
    #else
      SIMDE_FUNCTION_ATTRIBUTES
      __mmask64
      simde_arm_sve_mmask32_to_mmask64(__mmask32 m) {
        uint64_t e = HEDLEY_STATIC_CAST(uint64_t, m);
        uint64_t o = HEDLEY_STATIC_CAST(uint64_t, m);

        e = (e | (e << 16)) & UINT64_C(0x0000ffff0000ffff);
        e = (e | (e <<  8)) & UINT64_C(0x00ff00ff00ff00ff);
        e = (e | (e <<  4)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
        e = (e | (e <<  2)) & UINT64_C(0x3333333333333333);
        e = (e | (e <<  1)) & UINT64_C(0x5555555555555555);

        o = (o | (o << 16)) & UINT64_C(0x0000ffff0000ffff);
        o = (o | (o <<  8)) & UINT64_C(0x00ff00ff00ff00ff);
        o = (o | (o <<  4)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
        o = (o | (o <<  2)) & UINT64_C(0x3333333333333333);
        o = (o | (o <<  1)) & UINT64_C(0x5555555555555555);

        return HEDLEY_STATIC_CAST(__mmask64, e | (o << 1));
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask32
      simde_arm_sve_mmask16_to_mmask32(__mmask16 m) {
        uint32_t e = HEDLEY_STATIC_CAST(uint32_t, m);
        uint32_t o = HEDLEY_STATIC_CAST(uint32_t, m);

        e = (e | (e << 8)) & UINT32_C(0x00FF00FF);
        e = (e | (e << 4)) & UINT32_C(0x0F0F0F0F);
        e = (e | (e << 2)) & UINT32_C(0x33333333);
        e = (e | (e << 1)) & UINT32_C(0x55555555);

        o = (o | (o << 8)) & UINT32_C(0x00FF00FF);
        o = (o | (o << 4)) & UINT32_C(0x0F0F0F0F);
        o = (o | (o << 2)) & UINT32_C(0x33333333);
        o = (o | (o << 1)) & UINT32_C(0x55555555);

        return HEDLEY_STATIC_CAST(__mmask32, e | (o << 1));
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask16
      simde_arm_sve_mmask8_to_mmask16(__mmask8 m) {
        uint16_t e = HEDLEY_STATIC_CAST(uint16_t, m);
        uint16_t o = HEDLEY_STATIC_CAST(uint16_t, m);

        e = (e | (e <<  4)) & UINT16_C(0x0f0f);
        e = (e | (e <<  2)) & UINT16_C(0x3333);
        e = (e | (e <<  1)) & UINT16_C(0x5555);

        o = (o | (o <<  4)) & UINT16_C(0x0f0f);
        o = (o | (o <<  2)) & UINT16_C(0x3333);
        o = (o | (o <<  1)) & UINT16_C(0x5555);

        return HEDLEY_STATIC_CAST(uint16_t, e | (o << 1));
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask8
      simde_arm_sve_mmask4_to_mmask8(__mmask8 m) {
        uint8_t e = HEDLEY_STATIC_CAST(uint8_t, m);
        uint8_t o = HEDLEY_STATIC_CAST(uint8_t, m);

        e = (e | (e <<  2)) & UINT8_C(0x33);
        e = (e | (e <<  1)) & UINT8_C(0x55);

        o = (o | (o <<  2)) & UINT8_C(0x33);
        o = (o | (o <<  1)) & UINT8_C(0x55);

        return HEDLEY_STATIC_CAST(uint8_t, e | (o << 1));
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask32
      simde_arm_sve_mmask64_to_mmask32(__mmask64 m) {
        uint64_t l = (HEDLEY_STATIC_CAST(uint64_t, m)     ) & UINT64_C(0x5555555555555555);
        l = (l | (l >> 1)) & UINT64_C(0x3333333333333333);
        l = (l | (l >> 2)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
        l = (l | (l >> 4)) & UINT64_C(0x00ff00ff00ff00ff);
        l = (l | (l >> 8)) & UINT64_C(0x0000ffff0000ffff);

        uint64_t h = (HEDLEY_STATIC_CAST(uint64_t, m) >> 1) & UINT64_C(0x5555555555555555);
        h = (h | (h >> 1)) & UINT64_C(0x3333333333333333);
        h = (h | (h >> 2)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
        h = (h | (h >> 4)) & UINT64_C(0x00ff00ff00ff00ff);
        h = (h | (h >> 8)) & UINT64_C(0x0000ffff0000ffff);

        return HEDLEY_STATIC_CAST(uint32_t, l & h);
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask16
      simde_arm_sve_mmask32_to_mmask16(__mmask32 m) {
        uint32_t l = (HEDLEY_STATIC_CAST(uint32_t, m)     ) & UINT32_C(0x55555555);
        l = (l | (l >> 1)) & UINT32_C(0x33333333);
        l = (l | (l >> 2)) & UINT32_C(0x0f0f0f0f);
        l = (l | (l >> 4)) & UINT32_C(0x00ff00ff);
        l = (l | (l >> 8)) & UINT32_C(0x0000ffff);

        uint32_t h = (HEDLEY_STATIC_CAST(uint32_t, m) >> 1) & UINT32_C(0x55555555);
        h = (h | (h >> 1)) & UINT32_C(0x33333333);
        h = (h | (h >> 2)) & UINT32_C(0x0f0f0f0f);
        h = (h | (h >> 4)) & UINT32_C(0x00ff00ff);
        h = (h | (h >> 8)) & UINT32_C(0x0000ffff);

        return HEDLEY_STATIC_CAST(uint16_t, l & h);
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask8
      simde_arm_sve_mmask16_to_mmask8(__mmask16 m) {
        uint16_t l = (HEDLEY_STATIC_CAST(uint16_t, m)     ) & UINT16_C(0x5555);
        l = (l | (l >> 1)) & UINT16_C(0x3333);
        l = (l | (l >> 2)) & UINT16_C(0x0f0f);
        l = (l | (l >> 4)) & UINT16_C(0x00ff);

        uint16_t h = (HEDLEY_STATIC_CAST(uint16_t, m) >> 1) & UINT16_C(0x5555);
        h = (h | (h >> 1)) & UINT16_C(0x3333);
        h = (h | (h >> 2)) & UINT16_C(0x0f0f);
        h = (h | (h >> 4)) & UINT16_C(0x00ff);

        return HEDLEY_STATIC_CAST(uint8_t, l & h);
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask8
      simde_arm_sve_mmask8_to_mmask4(__mmask8 m) {
        uint8_t l = (HEDLEY_STATIC_CAST(uint8_t, m)     ) & UINT8_C(0x55);
        l = (l | (l >> 1)) & UINT8_C(0x33);
        l = (l | (l >> 2)) & UINT8_C(0x0f);
        l = (l | (l >> 4)) & UINT8_C(0xff);

        uint8_t h = (HEDLEY_STATIC_CAST(uint8_t, m) >> 1) & UINT8_C(0x55);
        h = (h | (h >> 1)) & UINT8_C(0x33);
        h = (h | (h >> 2)) & UINT8_C(0x0f);
        h = (h | (h >> 4)) & UINT8_C(0xff);

        return HEDLEY_STATIC_CAST(uint8_t, l & h);
      }
    #endif

    typedef enum {
      SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK64,
      SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK32,
      SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK16,
      SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK8,
      #if SIMDE_ARM_SVE_VECTOR_SIZE < 512
        SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK4,
      #endif
    } simde_svbool_mmask_type;

    HEDLEY_CONST HEDLEY_ALWAYS_INLINE
    simde_svbool_t
    simde_svbool_from_mmask64(__mmask64 mi) {
      simde_svbool_t b;

      b.value = HEDLEY_STATIC_CAST(__mmask64, mi);
      b.type  = SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK64;

      return b;
    }

    SIMDE_FUNCTION_ATTRIBUTES HEDLEY_CONST
    simde_svbool_t
    simde_svbool_from_mmask32(__mmask32 mi) {
      simde_svbool_t b;

      b.value = HEDLEY_STATIC_CAST(__mmask64, mi);
      b.type  = SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK32;

      return b;
    }

    SIMDE_FUNCTION_ATTRIBUTES HEDLEY_CONST
    simde_svbool_t
    simde_svbool_from_mmask16(__mmask16 mi) {
      simde_svbool_t b;

      b.value = HEDLEY_STATIC_CAST(__mmask64, mi);
      b.type  = SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK16;

      return b;
    }

    SIMDE_FUNCTION_ATTRIBUTES HEDLEY_CONST
    simde_svbool_t
    simde_svbool_from_mmask8(__mmask8 mi) {
      simde_svbool_t b;

      b.value = HEDLEY_STATIC_CAST(__mmask64, mi);
      b.type  = SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK8;

      return b;
    }

    #if SIMDE_ARM_SVE_VECTOR_SIZE < 512
      SIMDE_FUNCTION_ATTRIBUTES HEDLEY_CONST
      simde_svbool_t
      simde_svbool_from_mmask4(__mmask8 mi) {
        simde_svbool_t b;

        b.value = HEDLEY_STATIC_CAST(__mmask64, mi);
        b.type  = SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK4;

        return b;
      }

      SIMDE_FUNCTION_ATTRIBUTES HEDLEY_CONST
      __mmask8
      simde_svbool_to_mmask4(simde_svbool_t b) {
        __mmask64 tmp = b.value;

        switch (b.type) {
          case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK64:
            tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask64_to_mmask32(HEDLEY_STATIC_CAST(__mmask64, tmp)));
            HEDLEY_FALL_THROUGH;
          case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK32:
            tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask32_to_mmask16(HEDLEY_STATIC_CAST(__mmask32, tmp)));
            HEDLEY_FALL_THROUGH;
          case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK16:
            tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask16_to_mmask8(HEDLEY_STATIC_CAST(__mmask16, tmp)));
            HEDLEY_FALL_THROUGH;
          case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK8:
            tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask8_to_mmask4(HEDLEY_STATIC_CAST(__mmask8, tmp)));
        }

        return HEDLEY_STATIC_CAST(__mmask8, tmp);
      }
    #endif

    SIMDE_FUNCTION_ATTRIBUTES HEDLEY_CONST
    __mmask8
    simde_svbool_to_mmask8(simde_svbool_t b) {
      __mmask64 tmp = b.value;

      switch (b.type) {
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK64:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask64_to_mmask32(HEDLEY_STATIC_CAST(__mmask64, tmp)));
          HEDLEY_FALL_THROUGH;
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK32:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask32_to_mmask16(HEDLEY_STATIC_CAST(__mmask32, tmp)));
          HEDLEY_FALL_THROUGH;
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK16:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask16_to_mmask8(HEDLEY_STATIC_CAST(__mmask16, tmp)));
          HEDLEY_FALL_THROUGH;
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK8:
          break;

        #if SIMDE_ARM_SVE_VECTOR_SIZE < 512
          case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK4:
            tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask4_to_mmask8(HEDLEY_STATIC_CAST(__mmask8, tmp)));
        #endif
      }

      return HEDLEY_STATIC_CAST(__mmask8, tmp);
    }

    SIMDE_FUNCTION_ATTRIBUTES HEDLEY_CONST
    __mmask16
    simde_svbool_to_mmask16(simde_svbool_t b) {
      __mmask64 tmp = b.value;

      switch (b.type) {
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK64:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask64_to_mmask32(HEDLEY_STATIC_CAST(__mmask64, tmp)));
          HEDLEY_FALL_THROUGH;
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK32:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask32_to_mmask16(HEDLEY_STATIC_CAST(__mmask32, tmp)));
          HEDLEY_FALL_THROUGH;
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK16:
          break;

        #if SIMDE_ARM_SVE_VECTOR_SIZE < 512
          case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK4:
            tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask4_to_mmask8(HEDLEY_STATIC_CAST(__mmask8, tmp)));
            HEDLEY_FALL_THROUGH;
        #endif
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK8:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask8_to_mmask16(HEDLEY_STATIC_CAST(__mmask8, tmp)));
      }

      return HEDLEY_STATIC_CAST(__mmask16, tmp);
    }

    SIMDE_FUNCTION_ATTRIBUTES HEDLEY_CONST
    __mmask32
    simde_svbool_to_mmask32(simde_svbool_t b) {
      __mmask64 tmp = b.value;

      switch (b.type) {
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK64:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask64_to_mmask32(HEDLEY_STATIC_CAST(__mmask64, tmp)));
          HEDLEY_FALL_THROUGH;
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK32:
          break;

        #if SIMDE_ARM_SVE_VECTOR_SIZE < 512
          case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK4:
            tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask4_to_mmask8(HEDLEY_STATIC_CAST(__mmask8, tmp)));
            HEDLEY_FALL_THROUGH;
        #endif
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK8:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask8_to_mmask16(HEDLEY_STATIC_CAST(__mmask8, tmp)));
          HEDLEY_FALL_THROUGH;
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK16:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask16_to_mmask32(HEDLEY_STATIC_CAST(__mmask16, tmp)));
      }

      return HEDLEY_STATIC_CAST(__mmask32, tmp);
    }

    SIMDE_FUNCTION_ATTRIBUTES HEDLEY_CONST
    __mmask64
    simde_svbool_to_mmask64(simde_svbool_t b) {
      __mmask64 tmp = b.value;

      switch (b.type) {
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK64:
          break;

        #if SIMDE_ARM_SVE_VECTOR_SIZE < 512
          case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK4:
            tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask4_to_mmask8(HEDLEY_STATIC_CAST(__mmask8, tmp)));
            HEDLEY_FALL_THROUGH;
        #endif
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK8:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask8_to_mmask16(HEDLEY_STATIC_CAST(__mmask8, tmp)));
          HEDLEY_FALL_THROUGH;
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK16:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask16_to_mmask32(HEDLEY_STATIC_CAST(__mmask16, tmp)));
          HEDLEY_FALL_THROUGH;
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK32:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask32_to_mmask64(HEDLEY_STATIC_CAST(__mmask32, tmp)));
      }

      return HEDLEY_STATIC_CAST(__mmask64, tmp);
    }

    /* TODO: we're going to need need svbool_to/from_svint* functions
     * for when we can't implement a function using AVX-512. */
  #else
    typedef union {
      SIMDE_ARM_SVE_DECLARE_VECTOR(  int8_t,  values_i8, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));
      SIMDE_ARM_SVE_DECLARE_VECTOR( int16_t, values_i16, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));
      SIMDE_ARM_SVE_DECLARE_VECTOR( int32_t, values_i32, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));
      SIMDE_ARM_SVE_DECLARE_VECTOR( int64_t, values_i64, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));
      SIMDE_ARM_SVE_DECLARE_VECTOR( uint8_t,  values_u8, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));
      SIMDE_ARM_SVE_DECLARE_VECTOR(uint16_t, values_u16, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));
      SIMDE_ARM_SVE_DECLARE_VECTOR(uint32_t, values_u32, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));
      SIMDE_ARM_SVE_DECLARE_VECTOR(uint64_t, values_u64, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

      #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
        __m512i m512i;
      #endif
      #if defined(SIMDE_X86_AVX2_NATIVE)
        __m256i m256i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256i)];
      #endif
      #if defined(SIMDE_X86_SSE2_NATIVE)
        __m128i m128i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128i)];
      #endif

      #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
         int8x16_t neon_i8;
         int16x8_t neon_i16;
         int32x4_t neon_i32;
         int64x2_t neon_i64;
        uint8x16_t neon_u8;
        uint16x8_t neon_u16;
        uint32x4_t neon_u32;
        uint64x2_t neon_u64;
      #endif

      #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
        SIMDE_POWER_ALTIVEC_VECTOR(SIMDE_POWER_ALTIVEC_BOOL  char) altivec_b8;
        SIMDE_POWER_ALTIVEC_VECTOR(SIMDE_POWER_ALTIVEC_BOOL short) altivec_b16;
        SIMDE_POWER_ALTIVEC_VECTOR(SIMDE_POWER_ALTIVEC_BOOL   int) altivec_b32;
      #endif
      #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
        SIMDE_POWER_ALTIVEC_VECTOR(SIMDE_POWER_ALTIVEC_BOOL  long long) altivec_b64;
      #endif

      #if defined(SIMDE_WASM_SIMD128_NATIVE)
        v128_t v128;
      #endif
    } simde_svbool_t;

    SIMDE_DEFINE_CONVERSION_FUNCTION_(   simde_svbool_to_svint8,    simde_svint8_t,   simde_svbool_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svbool_from_svint8,    simde_svbool_t,   simde_svint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svbool_to_svint16,   simde_svint16_t,   simde_svbool_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_svbool_from_svint16,    simde_svbool_t,  simde_svint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svbool_to_svint32,   simde_svint32_t,   simde_svbool_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_svbool_from_svint32,    simde_svbool_t,  simde_svint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svbool_to_svint64,   simde_svint64_t,   simde_svbool_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_svbool_from_svint64,    simde_svbool_t,  simde_svint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svbool_to_svuint8,   simde_svuint8_t,   simde_svbool_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_svbool_from_svuint8,    simde_svbool_t,  simde_svuint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svbool_to_svuint16, simde_svuint16_t,   simde_svbool_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_svbool_from_svuint16,   simde_svbool_t, simde_svuint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svbool_to_svuint32, simde_svuint32_t,   simde_svbool_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_svbool_from_svuint32,   simde_svbool_t, simde_svuint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svbool_to_svuint64, simde_svuint64_t,   simde_svbool_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_svbool_from_svuint64,   simde_svbool_t, simde_svuint64_t)
  #endif

  #if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
    typedef     simde_svbool_t     svbool_t;
    typedef     simde_svint8_t     svint8_t;
    typedef    simde_svint16_t    svint16_t;
    typedef    simde_svint32_t    svint32_t;
    typedef    simde_svint64_t    svint64_t;
    typedef    simde_svuint8_t    svuint8_t;
    typedef   simde_svuint16_t   svuint16_t;
    typedef   simde_svuint32_t   svuint32_t;
    typedef   simde_svuint64_t   svuint64_t;
    typedef  simde_svfloat16_t  svfloat16_t;
    typedef simde_svbfloat16_t svbfloat16_t;
    typedef  simde_svfloat32_t  svfloat32_t;
    typedef  simde_svfloat64_t  svfloat64_t;
  #endif
#endif

#if !defined(SIMDE_ARM_SVE_DEFAULT_UNDEFINED_SUFFIX)
  #define SIMDE_ARM_SVE_DEFAULT_UNDEFINED_SUFFIX z
#endif
#define SIMDE_ARM_SVE_UNDEFINED_SYMBOL(name) HEDLEY_CONCAT3(name, _, SIMDE_ARM_SVE_DEFAULT_UNDEFINED_SUFFIX)

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

/* These are going to be used pretty much everywhere since they are
 * used to create the loops SVE requires.  Since we want to support
 * only including the files you need instead of just using sve.h,
 * it's helpful to pull these in here.  While this file is called
 * arm/sve/types.h, it might be better to think of it more as
 * arm/sve/common.h. */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/arm/sve/cnt.h :: */
/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_ARM_SVE_CNT_H)
#define SIMDE_ARM_SVE_CNT_H

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_svcntb(void) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svcntb();
  #else
    return sizeof(simde_svint8_t) / sizeof(int8_t);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svcntb
  #define svcntb() simde_svcntb()
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_svcnth(void) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svcnth();
  #else
    return sizeof(simde_svint16_t) / sizeof(int16_t);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svcnth
  #define svcnth() simde_svcnth()
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_svcntw(void) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svcntw();
  #else
    return sizeof(simde_svint32_t) / sizeof(int32_t);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svcntw
  #define svcntw() simde_svcntw()
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_svcntd(void) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svcntd();
  #else
    return sizeof(simde_svint64_t) / sizeof(int64_t);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svcntd
  #define svcntd() simde_svcntd()
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_CNT_H */
/* :: End simde/arm/sve/cnt.h :: */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/arm/sve/ld1.h :: */
/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

/* Note: we don't have vector implementations for most of these because
 * we can't just load everything and mask out the uninteresting bits;
 * that might cause a fault, for example if the end of the buffer buts
 * up against a protected page.
 *
 * One thing we might be able to do would be to check if the predicate
 * is all ones and, if so, use an unpredicated load instruction.  This
 * would probably we worthwhile for smaller types, though perhaps not
 * for larger types since it would mean branching for every load plus
 * the overhead of checking whether all bits are 1. */

#if !defined(SIMDE_ARM_SVE_LD1_H)
#define SIMDE_ARM_SVE_LD1_H

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svld1_s8(simde_svbool_t pg, const int8_t * base) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svld1_s8(pg, base);
  #else
    simde_svint8_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_maskz_loadu_epi8(simde_svbool_to_mmask64(pg), base);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_maskz_loadu_epi8(simde_svbool_to_mmask32(pg), base);
    #else
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntb()) ; i++) {
        r.values[i] = pg.values_i8[i] ? base[i] : INT8_C(0);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svld1_s8
  #define svld1_s8(pg, base) simde_svld1_s8((pg), (base))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svld1_s16(simde_svbool_t pg, const int16_t * base) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svld1_s16(pg, base);
  #else
    simde_svint16_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_maskz_loadu_epi16(simde_svbool_to_mmask32(pg), base);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_maskz_loadu_epi16(simde_svbool_to_mmask16(pg), base);
    #else
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcnth()) ; i++) {
        r.values[i] = pg.values_i16[i] ? base[i] : INT16_C(0);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svld1_s16
  #define svld1_s16(pg, base) simde_svld1_s16((pg), (base))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svld1_s32(simde_svbool_t pg, const int32_t * base) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svld1_s32(pg, base);
  #else
    simde_svint32_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_maskz_loadu_epi32(simde_svbool_to_mmask16(pg), base);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_maskz_loadu_epi32(simde_svbool_to_mmask8(pg), base);
    #else
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntw()) ; i++) {
        r.values[i] = pg.values_i32[i] ? base[i] : INT32_C(0);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svld1_s32
  #define svld1_s32(pg, base) simde_svld1_s32((pg), (base))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svld1_s64(simde_svbool_t pg, const int64_t * base) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svld1_s64(pg, base);
  #else
    simde_svint64_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_maskz_loadu_epi64(simde_svbool_to_mmask8(pg), base);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_maskz_loadu_epi64(simde_svbool_to_mmask4(pg), base);
    #else
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntd()) ; i++) {
        r.values[i] = pg.values_i64[i] ? base[i] : INT64_C(0);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svld1_s64
  #define svld1_s64(pg, base) simde_svld1_s64((pg), (base))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svld1_u8(simde_svbool_t pg, const uint8_t * base) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svld1_u8(pg, base);
  #else
    simde_svuint8_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_maskz_loadu_epi8(simde_svbool_to_mmask64(pg), base);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_maskz_loadu_epi8(simde_svbool_to_mmask32(pg), base);
    #else
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntb()) ; i++) {
        r.values[i] = pg.values_i8[i] ? base[i] : UINT8_C(0);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svld1_u8
  #define svld1_u8(pg, base) simde_svld1_u8((pg), (base))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svld1_u16(simde_svbool_t pg, const uint16_t * base) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svld1_u16(pg, base);
  #else
    simde_svuint16_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_maskz_loadu_epi16(simde_svbool_to_mmask32(pg), base);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_maskz_loadu_epi16(simde_svbool_to_mmask16(pg), base);
    #else
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcnth()) ; i++) {
        r.values[i] = pg.values_i16[i] ? base[i] : UINT16_C(0);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svld1_u16
  #define svld1_u16(pg, base) simde_svld1_u16((pg), (base))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svld1_u32(simde_svbool_t pg, const uint32_t * base) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svld1_u32(pg, base);
  #else
    simde_svuint32_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_maskz_loadu_epi32(simde_svbool_to_mmask16(pg), base);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_maskz_loadu_epi32(simde_svbool_to_mmask8(pg), base);
    #else
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntw()) ; i++) {
        r.values[i] = pg.values_i32[i] ? base[i] : UINT32_C(0);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svld1_u32
  #define svld1_u32(pg, base) simde_svld1_u32((pg), (base))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svld1_u64(simde_svbool_t pg, const uint64_t * base) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svld1_u64(pg, base);
  #else
    simde_svuint64_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_maskz_loadu_epi64(simde_svbool_to_mmask8(pg), base);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_maskz_loadu_epi64(simde_svbool_to_mmask4(pg), base);
    #else
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntd()) ; i++) {
        r.values[i] = pg.values_i64[i] ? base[i] : UINT64_C(0);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svld1_u64
  #define svld1_u64(pg, base) simde_svld1_u64((pg), (base))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svld1_f32(simde_svbool_t pg, const simde_float32 * base) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svld1_f32(pg, base);
  #else
    simde_svfloat32_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512 = _mm512_maskz_loadu_ps(simde_svbool_to_mmask16(pg), base);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256[0] = _mm256_maskz_loadu_ps(simde_svbool_to_mmask8(pg), base);
    #else
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntw()) ; i++) {
        r.values[i] = pg.values_i32[i] ? base[i] : SIMDE_FLOAT32_C(0.0);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svld1_f32
  #define svld1_f32(pg, base) simde_svld1_f32((pg), (base))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svld1_f64(simde_svbool_t pg, const simde_float64 * base) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svld1_f64(pg, base);
  #else
    simde_svfloat64_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512d = _mm512_maskz_loadu_pd(simde_svbool_to_mmask8(pg), base);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256d[0] = _mm256_maskz_loadu_pd(simde_svbool_to_mmask4(pg), base);
    #else
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntd()) ; i++) {
        r.values[i] = pg.values_i64[i] ? base[i] : SIMDE_FLOAT64_C(0.0);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svld1_f64
  #define svld1_f64(pg, base) simde_svld1_f64((pg), (base))
#endif

#if defined(__cplusplus)
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svld1(simde_svbool_t pg, const        int8_t * base) { return simde_svld1_s8 (pg, base); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svld1(simde_svbool_t pg, const       int16_t * base) { return simde_svld1_s16(pg, base); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svld1(simde_svbool_t pg, const       int32_t * base) { return simde_svld1_s32(pg, base); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svld1(simde_svbool_t pg, const       int64_t * base) { return simde_svld1_s64(pg, base); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svld1(simde_svbool_t pg, const       uint8_t * base) { return simde_svld1_u8 (pg, base); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svld1(simde_svbool_t pg, const      uint16_t * base) { return simde_svld1_u16(pg, base); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svld1(simde_svbool_t pg, const      uint32_t * base) { return simde_svld1_u32(pg, base); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svld1(simde_svbool_t pg, const      uint64_t * base) { return simde_svld1_u64(pg, base); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svld1(simde_svbool_t pg, const simde_float32 * base) { return simde_svld1_f32(pg, base); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svld1(simde_svbool_t pg, const simde_float64 * base) { return simde_svld1_f64(pg, base); }
#elif defined(SIMDE_GENERIC_)
  #define simde_svld1(pg, base) \
    (SIMDE_GENERIC_((base), \
      const        int8_t *: simde_svld1_s8 , \
      const       int16_t *: simde_svld1_s16, \
      const       int32_t *: simde_svld1_s32, \
      const       int64_t *: simde_svld1_s64, \
      const       uint8_t *: simde_svld1_u8 , \
      const      uint16_t *: simde_svld1_u16, \
      const      uint32_t *: simde_svld1_u32, \
      const      uint64_t *: simde_svld1_u64, \
      const simde_float32 *: simde_svld1_f32, \
      const simde_float64 *: simde_svld1_f64)(pg, base))
#endif
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef svld1
  #define svld1(pg, base) simde_svld1((pg), (base))
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_LD1_H */
/* :: End simde/arm/sve/ld1.h :: */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/arm/sve/ptest.h :: */
/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_ARM_SVE_PTEST_H)
#define SIMDE_ARM_SVE_PTEST_H

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

SIMDE_FUNCTION_ATTRIBUTES
simde_bool
simde_svptest_first(simde_svbool_t pg, simde_svbool_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svptest_first(pg, op);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_LIKELY(pg.value & 1))
      return op.value & 1;

    if (pg.value == 0 || op.value == 0)
      return 0;

    #if defined(_MSC_VER)
      unsigned long r = 0;
      _BitScanForward64(&r, HEDLEY_STATIC_CAST(uint64_t, pg.value));
      return (op.value >> r) & 1;
    #else
      return (op.value >> __builtin_ctzll(HEDLEY_STATIC_CAST(unsigned long long, pg.value))) & 1;
    #endif
  #else
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntb()) ; i++) {
      if (pg.values_i8[i]) {
        return !!op.values_i8[i];
      }
    }

    return 0;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svptest_first
  #define svptest_first(pg, op) simde_svptest_first(pg, op)
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_PTEST_H */
/* :: End simde/arm/sve/ptest.h :: */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/arm/sve/ptrue.h :: */
/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_ARM_SVE_PTRUE_H)
#define SIMDE_ARM_SVE_PTRUE_H

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svptrue_b8(void) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svptrue_b8();
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    simde_svbool_t r;

    #if SIMDE_ARM_SVE_VECTOR_SIZE >= 512
      r = simde_svbool_from_mmask64(HEDLEY_STATIC_CAST(__mmask64, ~UINT64_C(0)));
    #else
      r = simde_svbool_from_mmask32(HEDLEY_STATIC_CAST(__mmask32, ~UINT32_C(0)));
    #endif

    return r;
  #else
    simde_svint8_t r;

    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntb()) ; i++) {
      r.values[i] = ~INT8_C(0);
    }

    return simde_svbool_from_svint8(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svptrue_b8
  #define svptrue_b8() simde_svptrue_b8()
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svptrue_b16(void) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svptrue_b16();
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    simde_svbool_t r;

    #if SIMDE_ARM_SVE_VECTOR_SIZE >= 512
      r = simde_svbool_from_mmask32(HEDLEY_STATIC_CAST(__mmask32, ~UINT32_C(0)));
    #else
      r = simde_svbool_from_mmask16(HEDLEY_STATIC_CAST(__mmask16, ~UINT16_C(0)));
    #endif

    return r;
  #else
    simde_svint16_t r;

    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcnth()) ; i++) {
      r.values[i] = ~INT16_C(0);
    }

    return simde_svbool_from_svint16(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svptrue_b16
  #define svptrue_b16() simde_svptrue_b16()
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svptrue_b32(void) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svptrue_b32();
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    simde_svbool_t r;

    #if SIMDE_ARM_SVE_VECTOR_SIZE >= 512
      r = simde_svbool_from_mmask32(HEDLEY_STATIC_CAST(__mmask16, ~UINT16_C(0)));
    #else
      r = simde_svbool_from_mmask8(HEDLEY_STATIC_CAST(__mmask8, ~UINT8_C(0)));
    #endif

    return r;
  #else
    simde_svint32_t r;

    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntw()) ; i++) {
      r.values[i] = ~INT32_C(0);
    }

    return simde_svbool_from_svint32(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svptrue_b32
  #define svptrue_b32() simde_svptrue_b32()
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svptrue_b64(void) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svptrue_b64();
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    simde_svbool_t r;

    #if SIMDE_ARM_SVE_VECTOR_SIZE >= 512
      r = simde_svbool_from_mmask8(HEDLEY_STATIC_CAST(__mmask8, ~UINT8_C(0)));
    #else
      r = simde_svbool_from_mmask4(HEDLEY_STATIC_CAST(__mmask8, ~UINT8_C(0)));
    #endif

    return r;
  #else
    simde_svint64_t r;

    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntd()) ; i++) {
      r.values[i] = ~INT64_C(0);
    }

    return simde_svbool_from_svint64(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svptrue_b64
  #define svptrue_b64() simde_svptrue_b64()
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_PTRUE_H */
/* :: End simde/arm/sve/ptrue.h :: */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/arm/sve/st1.h :: */
/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_ARM_SVE_ST1_H)
#define SIMDE_ARM_SVE_ST1_H

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

SIMDE_FUNCTION_ATTRIBUTES
void
simde_svst1_s8(simde_svbool_t pg, int8_t * base, simde_svint8_t data) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    svst1_s8(pg, base, data);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    _mm512_mask_storeu_epi8(base, simde_svbool_to_mmask64(pg), data.m512i);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    _mm256_mask_storeu_epi8(base, simde_svbool_to_mmask32(pg), data.m256i[0]);
  #else
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntb()) ; i++) {
      if (pg.values_i8[i]) {
        base[i] = data.values[i];
      }
    }
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svst1_s8
  #define svst1_s8(pg, base, data) simde_svst1_s8((pg), (base), (data))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_svst1_s16(simde_svbool_t pg, int16_t * base, simde_svint16_t data) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    svst1_s16(pg, base, data);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    _mm512_mask_storeu_epi16(base, simde_svbool_to_mmask32(pg), data.m512i);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    _mm256_mask_storeu_epi16(base, simde_svbool_to_mmask16(pg), data.m256i[0]);
  #else
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcnth()) ; i++) {
      if (pg.values_i16[i]) {
        base[i] = data.values[i];
      }
    }
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svst1_s16
  #define svst1_s16(pg, base, data) simde_svst1_s16((pg), (base), (data))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_svst1_s32(simde_svbool_t pg, int32_t * base, simde_svint32_t data) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    svst1_s32(pg, base, data);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    _mm512_mask_storeu_epi32(base, simde_svbool_to_mmask16(pg), data.m512i);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    _mm256_mask_storeu_epi32(base, simde_svbool_to_mmask8(pg), data.m256i[0]);
  #else
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntw()) ; i++) {
      if (pg.values_i32[i]) {
        base[i] = data.values[i];
      }
    }
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svst1_s32
  #define svst1_s32(pg, base, data) simde_svst1_s32((pg), (base), (data))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_svst1_s64(simde_svbool_t pg, int64_t * base, simde_svint64_t data) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    svst1_s64(pg, base, data);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    _mm512_mask_storeu_epi64(base, simde_svbool_to_mmask8(pg), data.m512i);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    _mm256_mask_storeu_epi64(base, simde_svbool_to_mmask4(pg), data.m256i[0]);
  #else
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntd()) ; i++) {
      if (pg.values_i64[i]) {
        base[i] = data.values[i];
      }
    }
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svst1_s64
  #define svst1_s64(pg, base, data) simde_svst1_s64((pg), (base), (data))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_svst1_u8(simde_svbool_t pg, uint8_t * base, simde_svuint8_t data) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    svst1_u8(pg, base, data);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    _mm512_mask_storeu_epi8(base, simde_svbool_to_mmask64(pg), data.m512i);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    _mm256_mask_storeu_epi8(base, simde_svbool_to_mmask32(pg), data.m256i[0]);
  #else
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntb()) ; i++) {
      if (pg.values_u8[i]) {
        base[i] = data.values[i];
      }
    }
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svst1_u8
  #define svst1_u8(pg, base, data) simde_svst1_u8((pg), (base), (data))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_svst1_u16(simde_svbool_t pg, uint16_t * base, simde_svuint16_t data) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    svst1_u16(pg, base, data);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    _mm512_mask_storeu_epi16(base, simde_svbool_to_mmask32(pg), data.m512i);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    _mm256_mask_storeu_epi16(base, simde_svbool_to_mmask16(pg), data.m256i[0]);
  #else
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcnth()) ; i++) {
      if (pg.values_u16[i]) {
        base[i] = data.values[i];
      }
    }
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svst1_u16
  #define svst1_u16(pg, base, data) simde_svst1_u16((pg), (base), (data))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_svst1_u32(simde_svbool_t pg, uint32_t * base, simde_svuint32_t data) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    svst1_u32(pg, base, data);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    _mm512_mask_storeu_epi32(base, simde_svbool_to_mmask16(pg), data.m512i);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    _mm256_mask_storeu_epi32(base, simde_svbool_to_mmask8(pg), data.m256i[0]);
  #else
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntw()) ; i++) {
      if (pg.values_u32[i]) {
        base[i] = data.values[i];
      }
    }
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svst1_u32
  #define svst1_u32(pg, base, data) simde_svst1_u32((pg), (base), (data))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_svst1_u64(simde_svbool_t pg, uint64_t * base, simde_svuint64_t data) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    svst1_u64(pg, base, data);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    _mm512_mask_storeu_epi64(base, simde_svbool_to_mmask8(pg), data.m512i);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    _mm256_mask_storeu_epi64(base, simde_svbool_to_mmask4(pg), data.m256i[0]);
  #else
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntd()) ; i++) {
      if (pg.values_u64[i]) {
        base[i] = data.values[i];
      }
    }
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svst1_u64
  #define svst1_u64(pg, base, data) simde_svst1_u64((pg), (base), (data))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_svst1_f32(simde_svbool_t pg, simde_float32 * base, simde_svfloat32_t data) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    svst1_f32(pg, base, data);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    _mm512_mask_storeu_ps(base, simde_svbool_to_mmask16(pg), data.m512);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    _mm256_mask_storeu_ps(base, simde_svbool_to_mmask8(pg), data.m256[0]);
  #else
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntw()) ; i++) {
      if (pg.values_i32[i]) {
        base[i] = data.values[i];
      }
    }
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svst1_f32
  #define svst1_f32(pg, base, data) simde_svst1_f32((pg), (base), (data))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_svst1_f64(simde_svbool_t pg, simde_float64 * base, simde_svfloat64_t data) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    svst1_f64(pg, base, data);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    _mm512_mask_storeu_pd(base, simde_svbool_to_mmask8(pg), data.m512d);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    _mm256_mask_storeu_pd(base, simde_svbool_to_mmask4(pg), data.m256d[0]);
  #else
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntd()) ; i++) {
      if (pg.values_i64[i]) {
        base[i] = data.values[i];
      }
    }
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svst1_f64
  #define svst1_f64(pg, base, data) simde_svst1_f64((pg), (base), (data))
#endif

#if defined(__cplusplus)
  SIMDE_FUNCTION_ATTRIBUTES void simde_svst1(simde_svbool_t pg,        int8_t * base,    simde_svint8_t data) { simde_svst1_s8 (pg, base, data); }
  SIMDE_FUNCTION_ATTRIBUTES void simde_svst1(simde_svbool_t pg,       int16_t * base,   simde_svint16_t data) { simde_svst1_s16(pg, base, data); }
  SIMDE_FUNCTION_ATTRIBUTES void simde_svst1(simde_svbool_t pg,       int32_t * base,   simde_svint32_t data) { simde_svst1_s32(pg, base, data); }
  SIMDE_FUNCTION_ATTRIBUTES void simde_svst1(simde_svbool_t pg,       int64_t * base,   simde_svint64_t data) { simde_svst1_s64(pg, base, data); }
  SIMDE_FUNCTION_ATTRIBUTES void simde_svst1(simde_svbool_t pg,       uint8_t * base,   simde_svuint8_t data) { simde_svst1_u8 (pg, base, data); }
  SIMDE_FUNCTION_ATTRIBUTES void simde_svst1(simde_svbool_t pg,      uint16_t * base,  simde_svuint16_t data) { simde_svst1_u16(pg, base, data); }
  SIMDE_FUNCTION_ATTRIBUTES void simde_svst1(simde_svbool_t pg,      uint32_t * base,  simde_svuint32_t data) { simde_svst1_u32(pg, base, data); }
  SIMDE_FUNCTION_ATTRIBUTES void simde_svst1(simde_svbool_t pg,      uint64_t * base,  simde_svuint64_t data) { simde_svst1_u64(pg, base, data); }
  SIMDE_FUNCTION_ATTRIBUTES void simde_svst1(simde_svbool_t pg, simde_float32 * base, simde_svfloat32_t data) { simde_svst1_f32(pg, base, data); }
  SIMDE_FUNCTION_ATTRIBUTES void simde_svst1(simde_svbool_t pg, simde_float64 * base, simde_svfloat64_t data) { simde_svst1_f64(pg, base, data); }
#elif defined(SIMDE_GENERIC_)
  #define simde_svst1(pg, base, data) \
    (SIMDE_GENERIC_((data), \
         simde_svint8_t: simde_svst1_s8 , \
        simde_svint16_t: simde_svst1_s16, \
        simde_svint32_t: simde_svst1_s32, \
        simde_svint64_t: simde_svst1_s64, \
        simde_svuint8_t: simde_svst1_u8 , \
       simde_svuint16_t: simde_svst1_u16, \
       simde_svuint32_t: simde_svst1_u32, \
       simde_svuint64_t: simde_svst1_u64, \
      simde_svfloat32_t: simde_svst1_f32, \
      simde_svfloat64_t: simde_svst1_f64)((pg), (base), (data)))
#endif
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef svst1
  #define svst1(pg, base, data) simde_svst1((pg), (base), (data))
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_ST1_H */
/* :: End simde/arm/sve/st1.h :: */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/arm/sve/whilelt.h :: */
/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_ARM_SVE_WHILELT_H)
#define SIMDE_ARM_SVE_WHILELT_H

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svwhilelt_b8_s32(int32_t op1, int32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svwhilelt_b8_s32(op1, op2);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask64(HEDLEY_STATIC_CAST(__mmask64, 0));

    int_fast32_t remaining = (HEDLEY_STATIC_CAST(int_fast32_t, op2) - HEDLEY_STATIC_CAST(int_fast32_t, op1));
    __mmask64 r = ~HEDLEY_STATIC_CAST(__mmask64, 0);
    if (HEDLEY_UNLIKELY(remaining < 64)) {
      r >>= 64 - remaining;
    }

    return simde_svbool_from_mmask64(r);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask32(HEDLEY_STATIC_CAST(__mmask32, 0));

    int_fast32_t remaining = (HEDLEY_STATIC_CAST(int_fast32_t, op2) - HEDLEY_STATIC_CAST(int_fast32_t, op1));
    __mmask32 r = HEDLEY_STATIC_CAST(__mmask32, ~UINT32_C(0));
    if (HEDLEY_UNLIKELY(remaining < 32)) {
      r >>= 32 - remaining;
    }

    return simde_svbool_from_mmask32(r);
  #else
    simde_svint8_t r;

    int_fast32_t remaining = (op1 >= op2) ? 0 : (HEDLEY_STATIC_CAST(int_fast32_t, op2) - HEDLEY_STATIC_CAST(int_fast32_t, op1));

    SIMDE_VECTORIZE
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntb()) ; i++) {
      r.values[i] = (remaining-- > 0) ? ~UINT8_C(0) : UINT8_C(0);
    }

    return simde_svbool_from_svint8(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svwhilelt_b8_s32
  #define svwhilelt_b8_s32(op1, op2) simde_svwhilelt_b8_s32(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svwhilelt_b16_s32(int32_t op1, int32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svwhilelt_b16_s32(op1, op2);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask32(HEDLEY_STATIC_CAST(__mmask32, 0));

    int_fast32_t remaining = (HEDLEY_STATIC_CAST(int_fast32_t, op2) - HEDLEY_STATIC_CAST(int_fast32_t, op1));
    __mmask32 r = HEDLEY_STATIC_CAST(__mmask32, ~UINT32_C(0));
    if (HEDLEY_UNLIKELY(remaining < 32)) {
      r >>= 32 - remaining;
    }

    return simde_svbool_from_mmask32(r);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask16(HEDLEY_STATIC_CAST(__mmask16, 0));

    int_fast32_t remaining = (HEDLEY_STATIC_CAST(int_fast32_t, op2) - HEDLEY_STATIC_CAST(int_fast32_t, op1));
    __mmask16 r = HEDLEY_STATIC_CAST(__mmask16, ~UINT16_C(0));
    if (HEDLEY_UNLIKELY(remaining < 16)) {
      r >>= 16 - remaining;
    }

    return simde_svbool_from_mmask16(r);
  #else
    simde_svint16_t r;

    int_fast32_t remaining = (op1 >= op2) ? 0 : (HEDLEY_STATIC_CAST(int_fast32_t, op2) - HEDLEY_STATIC_CAST(int_fast32_t, op1));

    SIMDE_VECTORIZE
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcnth()) ; i++) {
      r.values[i] = (remaining-- > 0) ? ~UINT16_C(0) : UINT16_C(0);
    }

    return simde_svbool_from_svint16(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svwhilelt_b16_s32
  #define svwhilelt_b16_s32(op1, op2) simde_svwhilelt_b16_s32(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svwhilelt_b32_s32(int32_t op1, int32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svwhilelt_b32_s32(op1, op2);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask16(HEDLEY_STATIC_CAST(__mmask16, 0));

    int_fast32_t remaining = (HEDLEY_STATIC_CAST(int_fast32_t, op2) - HEDLEY_STATIC_CAST(int_fast32_t, op1));
    __mmask16 r = HEDLEY_STATIC_CAST(__mmask16, ~UINT16_C(0));
    if (HEDLEY_UNLIKELY(remaining < 16)) {
      r >>= 16 - remaining;
    }

    return simde_svbool_from_mmask16(r);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask8(HEDLEY_STATIC_CAST(__mmask8, 0));

    int_fast32_t remaining = (HEDLEY_STATIC_CAST(int_fast32_t, op2) - HEDLEY_STATIC_CAST(int_fast32_t, op1));
    __mmask8 r = HEDLEY_STATIC_CAST(__mmask8, ~UINT8_C(0));
    if (HEDLEY_UNLIKELY(remaining < 8)) {
      r >>= 8 - remaining;
    }

    return simde_svbool_from_mmask8(r);
  #else
    simde_svint32_t r;

    int_fast32_t remaining = (op1 >= op2) ? 0 : (HEDLEY_STATIC_CAST(int_fast32_t, op2) - HEDLEY_STATIC_CAST(int_fast32_t, op1));

    SIMDE_VECTORIZE
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntw()) ; i++) {
      r.values[i] = (remaining-- > 0) ? ~INT32_C(0) : INT32_C(0);
    }

    return simde_svbool_from_svint32(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svwhilelt_b32_s32
  #define svwhilelt_b32_s32(op1, op2) simde_svwhilelt_b32_s32(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svwhilelt_b64_s32(int32_t op1, int32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svwhilelt_b64_s32(op1, op2);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask8(HEDLEY_STATIC_CAST(__mmask8, 0));

    int_fast32_t remaining = (HEDLEY_STATIC_CAST(int_fast32_t, op2) - HEDLEY_STATIC_CAST(int_fast32_t, op1));
    __mmask8 r = HEDLEY_STATIC_CAST(__mmask8, ~UINT8_C(0));
    if (HEDLEY_UNLIKELY(remaining < 8)) {
      r >>= 8 - remaining;
    }

    return simde_svbool_from_mmask8(r);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask4(HEDLEY_STATIC_CAST(__mmask8, 0));

    int_fast32_t remaining = (HEDLEY_STATIC_CAST(int_fast32_t, op2) - HEDLEY_STATIC_CAST(int_fast32_t, op1));
    __mmask8 r = HEDLEY_STATIC_CAST(__mmask8, 0x0f);
    if (HEDLEY_UNLIKELY(remaining < 4)) {
      r >>= 4 - remaining;
    }

    return simde_svbool_from_mmask4(r);
  #else
    simde_svint64_t r;

    int_fast32_t remaining = (op1 >= op2) ? 0 : (HEDLEY_STATIC_CAST(int_fast32_t, op2) - HEDLEY_STATIC_CAST(int_fast32_t, op1));

    SIMDE_VECTORIZE
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntd()) ; i++) {
      r.values[i] = (remaining-- > 0) ? ~INT64_C(0) : INT64_C(0);
    }

    return simde_svbool_from_svint64(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svwhilelt_b64_s32
  #define svwhilelt_b64_s32(op1, op2) simde_svwhilelt_b64_s32(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svwhilelt_b8_s64(int64_t op1, int64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svwhilelt_b8_s64(op1, op2);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask64(HEDLEY_STATIC_CAST(__mmask64, 0));

    int_fast64_t remaining = (HEDLEY_STATIC_CAST(int_fast64_t, op2) - HEDLEY_STATIC_CAST(int_fast64_t, op1));
    __mmask64 r = ~HEDLEY_STATIC_CAST(__mmask64, 0);
    if (HEDLEY_UNLIKELY(remaining < 64)) {
      r >>= 64 - remaining;
    }

    return simde_svbool_from_mmask64(r);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask32(HEDLEY_STATIC_CAST(__mmask32, 0));

    int_fast64_t remaining = (HEDLEY_STATIC_CAST(int_fast64_t, op2) - HEDLEY_STATIC_CAST(int_fast64_t, op1));
    __mmask32 r = HEDLEY_STATIC_CAST(__mmask32, ~UINT32_C(0));
    if (HEDLEY_UNLIKELY(remaining < 32)) {
      r >>= 32 - remaining;
    }

    return simde_svbool_from_mmask32(r);
  #else
    simde_svint8_t r;

    int_fast64_t remaining = (op1 >= op2) ? 0 : (HEDLEY_STATIC_CAST(int_fast64_t, op2) - HEDLEY_STATIC_CAST(int_fast64_t, op1));

    SIMDE_VECTORIZE
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntb()) ; i++) {
      r.values[i] = (remaining-- > 0) ? ~UINT8_C(0) : UINT8_C(0);
    }

    return simde_svbool_from_svint8(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svwhilelt_b8_s64
  #define svwhilelt_b8_s64(op1, op2) simde_svwhilelt_b8_s64(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svwhilelt_b16_s64(int64_t op1, int64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svwhilelt_b16_s64(op1, op2);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask32(HEDLEY_STATIC_CAST(__mmask32, 0));

    int_fast64_t remaining = (HEDLEY_STATIC_CAST(int_fast64_t, op2) - HEDLEY_STATIC_CAST(int_fast64_t, op1));
    __mmask32 r = HEDLEY_STATIC_CAST(__mmask32, ~UINT32_C(0));
    if (HEDLEY_UNLIKELY(remaining < 32)) {
      r >>= 32 - remaining;
    }

    return simde_svbool_from_mmask32(r);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask16(HEDLEY_STATIC_CAST(__mmask16, 0));

    int_fast64_t remaining = (HEDLEY_STATIC_CAST(int_fast64_t, op2) - HEDLEY_STATIC_CAST(int_fast64_t, op1));
    __mmask16 r = HEDLEY_STATIC_CAST(__mmask16, ~UINT16_C(0));
    if (HEDLEY_UNLIKELY(remaining < 16)) {
      r >>= 16 - remaining;
    }

    return simde_svbool_from_mmask16(r);
  #else
    simde_svint16_t r;

    int_fast64_t remaining = (op1 >= op2) ? 0 : (HEDLEY_STATIC_CAST(int_fast64_t, op2) - HEDLEY_STATIC_CAST(int_fast64_t, op1));

    SIMDE_VECTORIZE
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcnth()) ; i++) {
      r.values[i] = (remaining-- > 0) ? ~UINT16_C(0) : UINT16_C(0);
    }

    return simde_svbool_from_svint16(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svwhilelt_b16_s64
  #define svwhilelt_b16_s64(op1, op2) simde_svwhilelt_b16_s64(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svwhilelt_b32_s64(int64_t op1, int64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svwhilelt_b32_s64(op1, op2);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask16(HEDLEY_STATIC_CAST(__mmask16, 0));

    int_fast64_t remaining = (HEDLEY_STATIC_CAST(int_fast64_t, op2) - HEDLEY_STATIC_CAST(int_fast64_t, op1));
    __mmask16 r = HEDLEY_STATIC_CAST(__mmask16, ~UINT16_C(0));
    if (HEDLEY_UNLIKELY(remaining < 16)) {
      r >>= 16 - remaining;
    }

    return simde_svbool_from_mmask16(r);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask8(HEDLEY_STATIC_CAST(__mmask8, 0));

    int_fast64_t remaining = (HEDLEY_STATIC_CAST(int_fast64_t, op2) - HEDLEY_STATIC_CAST(int_fast64_t, op1));
    __mmask8 r = HEDLEY_STATIC_CAST(__mmask8, ~UINT8_C(0));
    if (HEDLEY_UNLIKELY(remaining < 8)) {
      r >>= 8 - remaining;
    }

    return simde_svbool_from_mmask8(r);
  #else
    simde_svint64_t r;

    int_fast64_t remaining = (op1 >= op2) ? 0 : (HEDLEY_STATIC_CAST(int_fast64_t, op2) - HEDLEY_STATIC_CAST(int_fast64_t, op1));

    SIMDE_VECTORIZE
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntw()) ; i++) {
      r.values[i] = (remaining-- > 0) ? ~INT64_C(0) : INT64_C(0);
    }

    return simde_svbool_from_svint64(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svwhilelt_b32_s64
  #define svwhilelt_b32_s64(op1, op2) simde_svwhilelt_b32_s64(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svwhilelt_b64_s64(int64_t op1, int64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svwhilelt_b64_s64(op1, op2);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask8(HEDLEY_STATIC_CAST(__mmask8, 0));

    int_fast64_t remaining = (HEDLEY_STATIC_CAST(int_fast64_t, op2) - HEDLEY_STATIC_CAST(int_fast64_t, op1));
    __mmask8 r = HEDLEY_STATIC_CAST(__mmask8, ~UINT8_C(0));
    if (HEDLEY_UNLIKELY(remaining < 8)) {
      r >>= 8 - remaining;
    }

    return simde_svbool_from_mmask8(r);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask4(HEDLEY_STATIC_CAST(__mmask8, 0));

    int_fast64_t remaining = (HEDLEY_STATIC_CAST(int_fast64_t, op2) - HEDLEY_STATIC_CAST(int_fast64_t, op1));
    __mmask8 r = HEDLEY_STATIC_CAST(__mmask8, 0x0f);
    if (HEDLEY_UNLIKELY(remaining < 4)) {
      r >>= 4 - remaining;
    }

    return simde_svbool_from_mmask4(r);
  #else
    simde_svint64_t r;

    int_fast64_t remaining = (op1 >= op2) ? 0 : (HEDLEY_STATIC_CAST(int_fast64_t, op2) - HEDLEY_STATIC_CAST(int_fast64_t, op1));

    SIMDE_VECTORIZE
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntd()) ; i++) {
      r.values[i] = (remaining-- > 0) ? ~INT64_C(0) : INT64_C(0);
    }

    return simde_svbool_from_svint64(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svwhilelt_b64_s64
  #define svwhilelt_b64_s64(op1, op2) simde_svwhilelt_b64_s64(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svwhilelt_b8_u32(uint32_t op1, uint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svwhilelt_b8_u32(op1, op2);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask64(HEDLEY_STATIC_CAST(__mmask64, 0));

    uint_fast32_t remaining = (HEDLEY_STATIC_CAST(uint_fast32_t, op2) - HEDLEY_STATIC_CAST(uint_fast32_t, op1));
    __mmask64 r = ~HEDLEY_STATIC_CAST(__mmask64, 0);
    if (HEDLEY_UNLIKELY(remaining < 64)) {
      r >>= 64 - remaining;
    }

    return simde_svbool_from_mmask64(r);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask32(HEDLEY_STATIC_CAST(__mmask32, 0));

    uint_fast32_t remaining = (HEDLEY_STATIC_CAST(uint_fast32_t, op2) - HEDLEY_STATIC_CAST(uint_fast32_t, op1));
    __mmask32 r = HEDLEY_STATIC_CAST(__mmask32, ~UINT32_C(0));
    if (HEDLEY_UNLIKELY(remaining < 32)) {
      r >>= 32 - remaining;
    }

    return simde_svbool_from_mmask32(r);
  #else
    simde_svint8_t r;

    uint_fast32_t remaining = (op1 >= op2) ? 0 : (HEDLEY_STATIC_CAST(uint_fast32_t, op2) - HEDLEY_STATIC_CAST(uint_fast32_t, op1));

    SIMDE_VECTORIZE
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntb()) ; i++) {
      r.values[i] = (remaining-- > 0) ? ~UINT8_C(0) : UINT8_C(0);
    }

    return simde_svbool_from_svint8(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svwhilelt_b8_u32
  #define svwhilelt_b8_u32(op1, op2) simde_svwhilelt_b8_u32(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svwhilelt_b16_u32(uint32_t op1, uint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svwhilelt_b16_u32(op1, op2);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask32(HEDLEY_STATIC_CAST(__mmask32, 0));

    uint_fast32_t remaining = (HEDLEY_STATIC_CAST(uint_fast32_t, op2) - HEDLEY_STATIC_CAST(uint_fast32_t, op1));
    __mmask32 r = HEDLEY_STATIC_CAST(__mmask32, ~UINT32_C(0));
    if (HEDLEY_UNLIKELY(remaining < 32)) {
      r >>= 32 - remaining;
    }

    return simde_svbool_from_mmask32(r);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask16(HEDLEY_STATIC_CAST(__mmask16, 0));

    uint_fast32_t remaining = (HEDLEY_STATIC_CAST(uint_fast32_t, op2) - HEDLEY_STATIC_CAST(uint_fast32_t, op1));
    __mmask16 r = HEDLEY_STATIC_CAST(__mmask16, ~UINT16_C(0));
    if (HEDLEY_UNLIKELY(remaining < 16)) {
      r >>= 16 - remaining;
    }

    return simde_svbool_from_mmask16(r);
  #else
    simde_svint16_t r;

    uint_fast32_t remaining = (op1 >= op2) ? 0 : (HEDLEY_STATIC_CAST(uint_fast32_t, op2) - HEDLEY_STATIC_CAST(uint_fast32_t, op1));

    SIMDE_VECTORIZE
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcnth()) ; i++) {
      r.values[i] = (remaining-- > 0) ? ~UINT16_C(0) : UINT16_C(0);
    }

    return simde_svbool_from_svint16(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svwhilelt_b16_u32
  #define svwhilelt_b16_u32(op1, op2) simde_svwhilelt_b16_u32(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svwhilelt_b32_u32(uint32_t op1, uint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svwhilelt_b32_u32(op1, op2);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask16(HEDLEY_STATIC_CAST(__mmask16, 0));

    uint_fast32_t remaining = (HEDLEY_STATIC_CAST(uint_fast32_t, op2) - HEDLEY_STATIC_CAST(uint_fast32_t, op1));
    __mmask16 r = HEDLEY_STATIC_CAST(__mmask16, ~UINT16_C(0));
    if (HEDLEY_UNLIKELY(remaining < 16)) {
      r >>= 16 - remaining;
    }

    return simde_svbool_from_mmask16(r);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask8(HEDLEY_STATIC_CAST(__mmask8, 0));

    uint_fast32_t remaining = (HEDLEY_STATIC_CAST(uint_fast32_t, op2) - HEDLEY_STATIC_CAST(uint_fast32_t, op1));
    __mmask8 r = HEDLEY_STATIC_CAST(__mmask8, ~UINT8_C(0));
    if (HEDLEY_UNLIKELY(remaining < 8)) {
      r >>= 8 - remaining;
    }

    return simde_svbool_from_mmask8(r);
  #else
    simde_svuint32_t r;

    uint_fast32_t remaining = (op1 >= op2) ? 0 : (HEDLEY_STATIC_CAST(uint_fast32_t, op2) - HEDLEY_STATIC_CAST(uint_fast32_t, op1));

    SIMDE_VECTORIZE
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntw()) ; i++) {
      r.values[i] = (remaining-- > 0) ? ~UINT32_C(0) : UINT32_C(0);
    }

    return simde_svbool_from_svuint32(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svwhilelt_b32_u32
  #define svwhilelt_b32_u32(op1, op2) simde_svwhilelt_b32_u32(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svwhilelt_b64_u32(uint32_t op1, uint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svwhilelt_b64_u32(op1, op2);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask8(HEDLEY_STATIC_CAST(__mmask8, 0));

    uint_fast32_t remaining = (HEDLEY_STATIC_CAST(uint_fast32_t, op2) - HEDLEY_STATIC_CAST(uint_fast32_t, op1));
    __mmask8 r = HEDLEY_STATIC_CAST(__mmask8, ~UINT8_C(0));
    if (HEDLEY_UNLIKELY(remaining < 8)) {
      r >>= 8 - remaining;
    }

    return simde_svbool_from_mmask8(r);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask4(HEDLEY_STATIC_CAST(__mmask8, 0));

    uint_fast32_t remaining = (HEDLEY_STATIC_CAST(uint_fast32_t, op2) - HEDLEY_STATIC_CAST(uint_fast32_t, op1));
    __mmask8 r = HEDLEY_STATIC_CAST(__mmask8, 0x0f);
    if (HEDLEY_UNLIKELY(remaining < 4)) {
      r >>= 4 - remaining;
    }

    return simde_svbool_from_mmask4(r);
  #else
    simde_svint64_t r;

    uint_fast32_t remaining = (op1 >= op2) ? 0 : (HEDLEY_STATIC_CAST(uint_fast32_t, op2) - HEDLEY_STATIC_CAST(uint_fast32_t, op1));

    SIMDE_VECTORIZE
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntd()) ; i++) {
      r.values[i] = (remaining-- > 0) ? ~INT64_C(0) : INT64_C(0);
    }

    return simde_svbool_from_svint64(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svwhilelt_b64_u32
  #define svwhilelt_b64_u32(op1, op2) simde_svwhilelt_b64_u32(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svwhilelt_b8_u64(uint64_t op1, uint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svwhilelt_b8_u64(op1, op2);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask64(HEDLEY_STATIC_CAST(__mmask64, 0));

    uint_fast64_t remaining = (HEDLEY_STATIC_CAST(uint_fast64_t, op2) - HEDLEY_STATIC_CAST(uint_fast64_t, op1));
    __mmask64 r = ~HEDLEY_STATIC_CAST(__mmask64, 0);
    if (HEDLEY_UNLIKELY(remaining < 64)) {
      r >>= 64 - remaining;
    }

    return simde_svbool_from_mmask64(r);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask32(HEDLEY_STATIC_CAST(__mmask32, 0));

    uint_fast64_t remaining = (HEDLEY_STATIC_CAST(uint_fast64_t, op2) - HEDLEY_STATIC_CAST(uint_fast64_t, op1));
    __mmask32 r = HEDLEY_STATIC_CAST(__mmask32, ~UINT64_C(0));
    if (HEDLEY_UNLIKELY(remaining < 32)) {
      r >>= 32 - remaining;
    }

    return simde_svbool_from_mmask32(r);
  #else
    simde_svint8_t r;

    uint_fast64_t remaining = (op1 >= op2) ? 0 : (HEDLEY_STATIC_CAST(uint_fast64_t, op2) - HEDLEY_STATIC_CAST(uint_fast64_t, op1));

    SIMDE_VECTORIZE
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntb()) ; i++) {
      r.values[i] = (remaining-- > 0) ? ~UINT8_C(0) : UINT8_C(0);
    }

    return simde_svbool_from_svint8(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svwhilelt_b8_u64
  #define svwhilelt_b8_u64(op1, op2) simde_svwhilelt_b8_u64(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svwhilelt_b16_u64(uint64_t op1, uint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svwhilelt_b16_u64(op1, op2);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask32(HEDLEY_STATIC_CAST(__mmask32, 0));

    uint_fast64_t remaining = (HEDLEY_STATIC_CAST(uint_fast64_t, op2) - HEDLEY_STATIC_CAST(uint_fast64_t, op1));
    __mmask32 r = HEDLEY_STATIC_CAST(__mmask32, ~UINT32_C(0));
    if (HEDLEY_UNLIKELY(remaining < 32)) {
      r >>= 32 - remaining;
    }

    return simde_svbool_from_mmask32(r);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask16(HEDLEY_STATIC_CAST(__mmask16, 0));

    uint_fast64_t remaining = (HEDLEY_STATIC_CAST(uint_fast64_t, op2) - HEDLEY_STATIC_CAST(uint_fast64_t, op1));
    __mmask16 r = HEDLEY_STATIC_CAST(__mmask16, ~UINT16_C(0));
    if (HEDLEY_UNLIKELY(remaining < 16)) {
      r >>= 16 - remaining;
    }

    return simde_svbool_from_mmask16(r);
  #else
    simde_svint16_t r;

    uint_fast64_t remaining = (op1 >= op2) ? 0 : (HEDLEY_STATIC_CAST(uint_fast64_t, op2) - HEDLEY_STATIC_CAST(uint_fast64_t, op1));

    SIMDE_VECTORIZE
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcnth()) ; i++) {
      r.values[i] = (remaining-- > 0) ? ~UINT16_C(0) : UINT16_C(0);
    }

    return simde_svbool_from_svint16(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svwhilelt_b16_u64
  #define svwhilelt_b16_u64(op1, op2) simde_svwhilelt_b16_u64(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svwhilelt_b32_u64(uint64_t op1, uint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svwhilelt_b32_u64(op1, op2);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask16(HEDLEY_STATIC_CAST(__mmask16, 0));

    uint_fast64_t remaining = (HEDLEY_STATIC_CAST(uint_fast64_t, op2) - HEDLEY_STATIC_CAST(uint_fast64_t, op1));
    __mmask16 r = HEDLEY_STATIC_CAST(__mmask16, ~UINT16_C(0));
    if (HEDLEY_UNLIKELY(remaining < 16)) {
      r >>= 16 - remaining;
    }

    return simde_svbool_from_mmask16(r);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask8(HEDLEY_STATIC_CAST(__mmask8, 0));

    uint_fast64_t remaining = (HEDLEY_STATIC_CAST(uint_fast64_t, op2) - HEDLEY_STATIC_CAST(uint_fast64_t, op1));
    __mmask8 r = HEDLEY_STATIC_CAST(__mmask8, ~UINT8_C(0));
    if (HEDLEY_UNLIKELY(remaining < 8)) {
      r >>= 8 - remaining;
    }

    return simde_svbool_from_mmask8(r);
  #else
    simde_svuint64_t r;

    uint_fast64_t remaining = (op1 >= op2) ? 0 : (HEDLEY_STATIC_CAST(uint_fast64_t, op2) - HEDLEY_STATIC_CAST(uint_fast64_t, op1));

    SIMDE_VECTORIZE
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntw()) ; i++) {
      r.values[i] = (remaining-- > 0) ? ~UINT64_C(0) : UINT64_C(0);
    }

    return simde_svbool_from_svuint64(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svwhilelt_b32_u64
  #define svwhilelt_b32_u64(op1, op2) simde_svwhilelt_b32_u64(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svwhilelt_b64_u64(uint64_t op1, uint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svwhilelt_b64_u64(op1, op2);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask8(HEDLEY_STATIC_CAST(__mmask8, 0));

    uint_fast64_t remaining = (HEDLEY_STATIC_CAST(uint_fast64_t, op2) - HEDLEY_STATIC_CAST(uint_fast64_t, op1));
    __mmask8 r = HEDLEY_STATIC_CAST(__mmask8, ~UINT8_C(0));
    if (HEDLEY_UNLIKELY(remaining < 8)) {
      r >>= 8 - remaining;
    }

    return simde_svbool_from_mmask8(r);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    if (HEDLEY_UNLIKELY(op1 >= op2))
      return simde_svbool_from_mmask4(HEDLEY_STATIC_CAST(__mmask8, 0));

    uint_fast64_t remaining = (HEDLEY_STATIC_CAST(uint_fast64_t, op2) - HEDLEY_STATIC_CAST(uint_fast64_t, op1));
    __mmask8 r = HEDLEY_STATIC_CAST(__mmask8, 0x0f);
    if (HEDLEY_UNLIKELY(remaining < 4)) {
      r >>= 4 - remaining;
    }

    return simde_svbool_from_mmask4(r);
  #else
    simde_svint64_t r;

    uint_fast64_t remaining = (op1 >= op2) ? 0 : (HEDLEY_STATIC_CAST(uint_fast64_t, op2) - HEDLEY_STATIC_CAST(uint_fast64_t, op1));

    SIMDE_VECTORIZE
    for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, simde_svcntd()) ; i++) {
      r.values[i] = (remaining-- > 0) ? ~INT64_C(0) : INT64_C(0);
    }

    return simde_svbool_from_svint64(r);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svwhilelt_b64_u64
  #define svwhilelt_b64_u64(op1, op2) simde_svwhilelt_b64_u64(op1, op2)
#endif

#if defined(__cplusplus)
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svwhilelt_b8 ( int32_t op1,  int32_t op2) { return  simde_svwhilelt_b8_s32(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svwhilelt_b8 ( int64_t op1,  int64_t op2) { return  simde_svwhilelt_b8_s64(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svwhilelt_b8 (uint32_t op1, uint32_t op2) { return  simde_svwhilelt_b8_u32(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svwhilelt_b8 (uint64_t op1, uint64_t op2) { return  simde_svwhilelt_b8_u64(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svwhilelt_b16( int32_t op1,  int32_t op2) { return simde_svwhilelt_b16_s32(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svwhilelt_b16( int64_t op1,  int64_t op2) { return simde_svwhilelt_b16_s64(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svwhilelt_b16(uint32_t op1, uint32_t op2) { return simde_svwhilelt_b16_u32(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svwhilelt_b16(uint64_t op1, uint64_t op2) { return simde_svwhilelt_b16_u64(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svwhilelt_b32( int32_t op1,  int32_t op2) { return simde_svwhilelt_b32_s32(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svwhilelt_b32( int64_t op1,  int64_t op2) { return simde_svwhilelt_b32_s64(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svwhilelt_b32(uint32_t op1, uint32_t op2) { return simde_svwhilelt_b32_u32(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svwhilelt_b32(uint64_t op1, uint64_t op2) { return simde_svwhilelt_b32_u64(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svwhilelt_b64( int32_t op1,  int32_t op2) { return simde_svwhilelt_b64_s32(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svwhilelt_b64( int64_t op1,  int64_t op2) { return simde_svwhilelt_b64_s64(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svwhilelt_b64(uint32_t op1, uint32_t op2) { return simde_svwhilelt_b64_u32(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svwhilelt_b64(uint64_t op1, uint64_t op2) { return simde_svwhilelt_b64_u64(op1, op2); }
#elif defined(SIMDE_GENERIC_)
  #define simde_svwhilelt_b8(op1, op2) \
    (SIMDE_GENERIC_((op1), \
       int32_t: simde_svwhilelt_b8_s32, \
      uint32_t: simde_svwhilelt_b8_u32, \
       int64_t: simde_svwhilelt_b8_s64, \
      uint64_t: simde_svwhilelt_b8_u64)((op1), (op2)))
  #define simde_svwhilelt_b16(op1, op2) \
    (SIMDE_GENERIC_((op1), \
       int32_t: simde_svwhilelt_b16_s32, \
      uint32_t: simde_svwhilelt_b16_u32, \
       int64_t: simde_svwhilelt_b16_s64, \
      uint64_t: simde_svwhilelt_b16_u64)((op1), (op2)))
  #define simde_svwhilelt_b32(op1, op2) \
    (SIMDE_GENERIC_((op1), \
       int32_t: simde_svwhilelt_b32_s32, \
      uint32_t: simde_svwhilelt_b32_u32, \
       int64_t: simde_svwhilelt_b32_s64, \
      uint64_t: simde_svwhilelt_b32_u64)((op1), (op2)))
  #define simde_svwhilelt_b64(op1, op2) \
    (SIMDE_GENERIC_((op1), \
       int32_t: simde_svwhilelt_b64_s32, \
      uint32_t: simde_svwhilelt_b64_u32, \
       int64_t: simde_svwhilelt_b64_s64, \
      uint64_t: simde_svwhilelt_b64_u64)((op1), (op2)))
#endif
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef svwhilelt_b8
  #undef svwhilelt_b16
  #undef svwhilelt_b32
  #undef svwhilelt_b64
  #define svwhilelt_b8(op1, op2) simde_svwhilelt_b8((op1), (op2))
  #define svwhilelt_b16(op1, op2) simde_svwhilelt_b16((op1), (op2))
  #define svwhilelt_b32(op1, op2) simde_svwhilelt_b32((op1), (op2))
  #define svwhilelt_b64(op1, op2) simde_svwhilelt_b64((op1), (op2))
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_WHILELT_H */
/* :: End simde/arm/sve/whilelt.h :: */

#endif /* SIMDE_ARM_SVE_TYPES_H */
/* :: End simde/arm/sve/types.h :: */

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/arm/sve/add.h :: */
/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_ARM_SVE_ADD_H)
#define SIMDE_ARM_SVE_ADD_H

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/arm/sve/sel.h :: */
/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_ARM_SVE_SEL_H)
#define SIMDE_ARM_SVE_SEL_H

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/arm/sve/reinterpret.h :: */
/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_ARM_SVE_REINTERPRET_H)
#define SIMDE_ARM_SVE_REINTERPRET_H

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

#if defined(SIMDE_ARM_SVE_NATIVE)
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8_s16(   simde_svint16_t op) { return   svreinterpret_s8_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8_s32(   simde_svint32_t op) { return   svreinterpret_s8_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8_s64(   simde_svint64_t op) { return   svreinterpret_s8_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t    simde_svreinterpret_s8_u8(   simde_svuint8_t op) { return    svreinterpret_s8_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8_u16(  simde_svuint16_t op) { return   svreinterpret_s8_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8_u32(  simde_svuint32_t op) { return   svreinterpret_s8_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8_u64(  simde_svuint64_t op) { return   svreinterpret_s8_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8_f16( simde_svfloat16_t op) { return   svreinterpret_s8_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8_f32( simde_svfloat32_t op) { return   svreinterpret_s8_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8_f64( simde_svfloat64_t op) { return   svreinterpret_s8_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t   simde_svreinterpret_s16_s8(    simde_svint8_t op) { return   svreinterpret_s16_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16_s32(   simde_svint32_t op) { return  svreinterpret_s16_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16_s64(   simde_svint64_t op) { return  svreinterpret_s16_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t   simde_svreinterpret_s16_u8(   simde_svuint8_t op) { return   svreinterpret_s16_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16_u16(  simde_svuint16_t op) { return  svreinterpret_s16_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16_u32(  simde_svuint32_t op) { return  svreinterpret_s16_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16_u64(  simde_svuint64_t op) { return  svreinterpret_s16_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16_f16( simde_svfloat16_t op) { return  svreinterpret_s16_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16_f32( simde_svfloat32_t op) { return  svreinterpret_s16_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16_f64( simde_svfloat64_t op) { return  svreinterpret_s16_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t   simde_svreinterpret_s32_s8(    simde_svint8_t op) { return   svreinterpret_s32_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32_s16(   simde_svint16_t op) { return  svreinterpret_s32_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32_s64(   simde_svint64_t op) { return  svreinterpret_s32_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t   simde_svreinterpret_s32_u8(   simde_svuint8_t op) { return   svreinterpret_s32_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32_u16(  simde_svuint16_t op) { return  svreinterpret_s32_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32_u32(  simde_svuint32_t op) { return  svreinterpret_s32_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32_u64(  simde_svuint64_t op) { return  svreinterpret_s32_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32_f16( simde_svfloat16_t op) { return  svreinterpret_s32_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32_f32( simde_svfloat32_t op) { return  svreinterpret_s32_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32_f64( simde_svfloat64_t op) { return  svreinterpret_s32_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t   simde_svreinterpret_s64_s8(    simde_svint8_t op) { return   svreinterpret_s64_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64_s16(   simde_svint16_t op) { return  svreinterpret_s64_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64_s32(   simde_svint32_t op) { return  svreinterpret_s64_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t   simde_svreinterpret_s64_u8(   simde_svuint8_t op) { return   svreinterpret_s64_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64_u16(  simde_svuint16_t op) { return  svreinterpret_s64_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64_u32(  simde_svuint32_t op) { return  svreinterpret_s64_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64_u64(  simde_svuint64_t op) { return  svreinterpret_s64_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64_f16( simde_svfloat16_t op) { return  svreinterpret_s64_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64_f32( simde_svfloat32_t op) { return  svreinterpret_s64_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64_f64( simde_svfloat64_t op) { return  svreinterpret_s64_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t    simde_svreinterpret_u8_s8(    simde_svint8_t op) { return    svreinterpret_u8_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8_s16(   simde_svint16_t op) { return   svreinterpret_u8_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8_s32(   simde_svint32_t op) { return   svreinterpret_u8_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8_s64(   simde_svint64_t op) { return   svreinterpret_u8_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8_u16(  simde_svuint16_t op) { return   svreinterpret_u8_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8_u32(  simde_svuint32_t op) { return   svreinterpret_u8_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8_u64(  simde_svuint64_t op) { return   svreinterpret_u8_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8_f16( simde_svfloat16_t op) { return   svreinterpret_u8_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8_f32( simde_svfloat32_t op) { return   svreinterpret_u8_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8_f64( simde_svfloat64_t op) { return   svreinterpret_u8_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t   simde_svreinterpret_u16_s8(    simde_svint8_t op) { return   svreinterpret_u16_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16_s16(   simde_svint16_t op) { return  svreinterpret_u16_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16_s32(   simde_svint32_t op) { return  svreinterpret_u16_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16_s64(   simde_svint64_t op) { return  svreinterpret_u16_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t   simde_svreinterpret_u16_u8(   simde_svuint8_t op) { return   svreinterpret_u16_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16_u32(  simde_svuint32_t op) { return  svreinterpret_u16_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16_u64(  simde_svuint64_t op) { return  svreinterpret_u16_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16_f16( simde_svfloat16_t op) { return  svreinterpret_u16_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16_f32( simde_svfloat32_t op) { return  svreinterpret_u16_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16_f64( simde_svfloat64_t op) { return  svreinterpret_u16_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t   simde_svreinterpret_u32_s8(    simde_svint8_t op) { return   svreinterpret_u32_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32_s16(   simde_svint16_t op) { return  svreinterpret_u32_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32_s32(   simde_svint32_t op) { return  svreinterpret_u32_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32_s64(   simde_svint64_t op) { return  svreinterpret_u32_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t   simde_svreinterpret_u32_u8(   simde_svuint8_t op) { return   svreinterpret_u32_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32_u16(  simde_svuint16_t op) { return  svreinterpret_u32_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32_u64(  simde_svuint64_t op) { return  svreinterpret_u32_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32_f16( simde_svfloat16_t op) { return  svreinterpret_u32_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32_f32( simde_svfloat32_t op) { return  svreinterpret_u32_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32_f64( simde_svfloat64_t op) { return  svreinterpret_u32_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t   simde_svreinterpret_u64_s8(    simde_svint8_t op) { return   svreinterpret_u64_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64_s16(   simde_svint16_t op) { return  svreinterpret_u64_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64_s32(   simde_svint32_t op) { return  svreinterpret_u64_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64_s64(   simde_svint64_t op) { return  svreinterpret_u64_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t   simde_svreinterpret_u64_u8(   simde_svuint8_t op) { return   svreinterpret_u64_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64_u16(  simde_svuint16_t op) { return  svreinterpret_u64_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64_u32(  simde_svuint32_t op) { return  svreinterpret_u64_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64_f16( simde_svfloat16_t op) { return  svreinterpret_u64_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64_f32( simde_svfloat32_t op) { return  svreinterpret_u64_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64_f64( simde_svfloat64_t op) { return  svreinterpret_u64_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t   simde_svreinterpret_f16_s8(    simde_svint8_t op) { return   svreinterpret_f16_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16_s16(   simde_svint16_t op) { return  svreinterpret_f16_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16_s32(   simde_svint32_t op) { return  svreinterpret_f16_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16_s64(   simde_svint64_t op) { return  svreinterpret_f16_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t   simde_svreinterpret_f16_u8(   simde_svuint8_t op) { return   svreinterpret_f16_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16_u16(  simde_svuint16_t op) { return  svreinterpret_f16_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16_u32(  simde_svuint32_t op) { return  svreinterpret_f16_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16_u64(  simde_svuint64_t op) { return  svreinterpret_f16_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16_f32( simde_svfloat32_t op) { return  svreinterpret_f16_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16_f64( simde_svfloat64_t op) { return  svreinterpret_f16_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t   simde_svreinterpret_f32_s8(    simde_svint8_t op) { return   svreinterpret_f32_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32_s16(   simde_svint16_t op) { return  svreinterpret_f32_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32_s32(   simde_svint32_t op) { return  svreinterpret_f32_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32_s64(   simde_svint64_t op) { return  svreinterpret_f32_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t   simde_svreinterpret_f32_u8(   simde_svuint8_t op) { return   svreinterpret_f32_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32_u16(  simde_svuint16_t op) { return  svreinterpret_f32_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32_u32(  simde_svuint32_t op) { return  svreinterpret_f32_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32_u64(  simde_svuint64_t op) { return  svreinterpret_f32_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32_f16( simde_svfloat16_t op) { return  svreinterpret_f32_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32_f64( simde_svfloat64_t op) { return  svreinterpret_f32_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t   simde_svreinterpret_f64_s8(    simde_svint8_t op) { return   svreinterpret_f64_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64_s16(   simde_svint16_t op) { return  svreinterpret_f64_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64_s32(   simde_svint32_t op) { return  svreinterpret_f64_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64_s64(   simde_svint64_t op) { return  svreinterpret_f64_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t   simde_svreinterpret_f64_u8(   simde_svuint8_t op) { return   svreinterpret_f64_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64_u16(  simde_svuint16_t op) { return  svreinterpret_f64_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64_u32(  simde_svuint32_t op) { return  svreinterpret_f64_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64_u64(  simde_svuint64_t op) { return  svreinterpret_f64_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64_f16( simde_svfloat16_t op) { return  svreinterpret_f64_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64_f32( simde_svfloat32_t op) { return  svreinterpret_f64_f32(op); }
#else
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s8_s16,     simde_svint8_t,    simde_svint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s8_s32,     simde_svint8_t,    simde_svint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s8_s64,     simde_svint8_t,    simde_svint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(   simde_svreinterpret_s8_u8,     simde_svint8_t,    simde_svuint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s8_u16,     simde_svint8_t,   simde_svuint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s8_u32,     simde_svint8_t,   simde_svuint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s8_u64,     simde_svint8_t,   simde_svuint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s8_f16,     simde_svint8_t,  simde_svfloat16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s8_f32,     simde_svint8_t,  simde_svfloat32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s8_f64,     simde_svint8_t,  simde_svfloat64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s16_s8,    simde_svint16_t,     simde_svint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s16_s32,    simde_svint16_t,    simde_svint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s16_s64,    simde_svint16_t,    simde_svint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s16_u8,    simde_svint16_t,    simde_svuint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s16_u16,    simde_svint16_t,   simde_svuint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s16_u32,    simde_svint16_t,   simde_svuint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s16_u64,    simde_svint16_t,   simde_svuint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s16_f16,    simde_svint16_t,  simde_svfloat16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s16_f32,    simde_svint16_t,  simde_svfloat32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s16_f64,    simde_svint16_t,  simde_svfloat64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s32_s8,    simde_svint32_t,     simde_svint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s32_s16,    simde_svint32_t,    simde_svint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s32_s64,    simde_svint32_t,    simde_svint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s32_u8,    simde_svint32_t,    simde_svuint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s32_u16,    simde_svint32_t,   simde_svuint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s32_u32,    simde_svint32_t,   simde_svuint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s32_u64,    simde_svint32_t,   simde_svuint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s32_f16,    simde_svint32_t,  simde_svfloat16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s32_f32,    simde_svint32_t,  simde_svfloat32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s32_f64,    simde_svint32_t,  simde_svfloat64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s64_s8,    simde_svint64_t,     simde_svint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s64_s16,    simde_svint64_t,    simde_svint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s64_s32,    simde_svint64_t,    simde_svint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s64_u8,    simde_svint64_t,    simde_svuint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s64_u16,    simde_svint64_t,   simde_svuint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s64_u32,    simde_svint64_t,   simde_svuint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s64_u64,    simde_svint64_t,   simde_svuint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s64_f16,    simde_svint64_t,  simde_svfloat16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s64_f32,    simde_svint64_t,  simde_svfloat32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s64_f64,    simde_svint64_t,  simde_svfloat64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(   simde_svreinterpret_u8_s8,    simde_svuint8_t,     simde_svint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u8_s16,    simde_svuint8_t,    simde_svint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u8_s32,    simde_svuint8_t,    simde_svint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u8_s64,    simde_svuint8_t,    simde_svint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u8_u16,    simde_svuint8_t,   simde_svuint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u8_u32,    simde_svuint8_t,   simde_svuint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u8_u64,    simde_svuint8_t,   simde_svuint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u8_f16,    simde_svuint8_t,  simde_svfloat16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u8_f32,    simde_svuint8_t,  simde_svfloat32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u8_f64,    simde_svuint8_t,  simde_svfloat64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u16_s8,   simde_svuint16_t,     simde_svint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u16_s16,   simde_svuint16_t,    simde_svint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u16_s32,   simde_svuint16_t,    simde_svint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u16_s64,   simde_svuint16_t,    simde_svint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u16_u8,   simde_svuint16_t,    simde_svuint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u16_u32,   simde_svuint16_t,   simde_svuint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u16_u64,   simde_svuint16_t,   simde_svuint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u16_f16,   simde_svuint16_t,  simde_svfloat16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u16_f32,   simde_svuint16_t,  simde_svfloat32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u16_f64,   simde_svuint16_t,  simde_svfloat64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u32_s8,   simde_svuint32_t,     simde_svint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u32_s16,   simde_svuint32_t,    simde_svint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u32_s32,   simde_svuint32_t,    simde_svint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u32_s64,   simde_svuint32_t,    simde_svint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u32_u8,   simde_svuint32_t,    simde_svuint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u32_u16,   simde_svuint32_t,   simde_svuint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u32_u64,   simde_svuint32_t,   simde_svuint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u32_f16,   simde_svuint32_t,  simde_svfloat16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u32_f32,   simde_svuint32_t,  simde_svfloat32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u32_f64,   simde_svuint32_t,  simde_svfloat64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u64_s8,   simde_svuint64_t,     simde_svint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u64_s16,   simde_svuint64_t,    simde_svint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u64_s32,   simde_svuint64_t,    simde_svint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u64_s64,   simde_svuint64_t,    simde_svint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u64_u8,   simde_svuint64_t,    simde_svuint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u64_u16,   simde_svuint64_t,   simde_svuint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u64_u32,   simde_svuint64_t,   simde_svuint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u64_f16,   simde_svuint64_t,  simde_svfloat16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u64_f32,   simde_svuint64_t,  simde_svfloat32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u64_f64,   simde_svuint64_t,  simde_svfloat64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_f16_s8,  simde_svfloat16_t,     simde_svint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f16_s16,  simde_svfloat16_t,    simde_svint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f16_s32,  simde_svfloat16_t,    simde_svint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f16_s64,  simde_svfloat16_t,    simde_svint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_f16_u8,  simde_svfloat16_t,    simde_svuint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f16_u16,  simde_svfloat16_t,   simde_svuint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f16_u32,  simde_svfloat16_t,   simde_svuint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f16_u64,  simde_svfloat16_t,   simde_svuint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f16_f32,  simde_svfloat16_t,  simde_svfloat32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f16_f64,  simde_svfloat16_t,  simde_svfloat64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_f32_s8,  simde_svfloat32_t,     simde_svint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f32_s16,  simde_svfloat32_t,    simde_svint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f32_s32,  simde_svfloat32_t,    simde_svint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f32_s64,  simde_svfloat32_t,    simde_svint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_f32_u8,  simde_svfloat32_t,    simde_svuint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f32_u16,  simde_svfloat32_t,   simde_svuint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f32_u32,  simde_svfloat32_t,   simde_svuint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f32_u64,  simde_svfloat32_t,   simde_svuint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f32_f16,  simde_svfloat32_t,  simde_svfloat16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f32_f64,  simde_svfloat32_t,  simde_svfloat64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_f64_s8,  simde_svfloat64_t,     simde_svint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f64_s16,  simde_svfloat64_t,    simde_svint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f64_s32,  simde_svfloat64_t,    simde_svint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f64_s64,  simde_svfloat64_t,    simde_svint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_f64_u8,  simde_svfloat64_t,    simde_svuint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f64_u16,  simde_svfloat64_t,   simde_svuint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f64_u32,  simde_svfloat64_t,   simde_svuint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f64_u64,  simde_svfloat64_t,   simde_svuint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f64_f16,  simde_svfloat64_t,  simde_svfloat16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f64_f32,  simde_svfloat64_t,  simde_svfloat32_t)
#endif

#if defined(__cplusplus)
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8(   simde_svint16_t op) { return   simde_svreinterpret_s8_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8(   simde_svint32_t op) { return   simde_svreinterpret_s8_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8(   simde_svint64_t op) { return   simde_svreinterpret_s8_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8(   simde_svuint8_t op) { return    simde_svreinterpret_s8_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8(  simde_svuint16_t op) { return   simde_svreinterpret_s8_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8(  simde_svuint32_t op) { return   simde_svreinterpret_s8_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8(  simde_svuint64_t op) { return   simde_svreinterpret_s8_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8( simde_svfloat16_t op) { return   simde_svreinterpret_s8_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8( simde_svfloat32_t op) { return   simde_svreinterpret_s8_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8( simde_svfloat64_t op) { return   simde_svreinterpret_s8_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16(    simde_svint8_t op) { return   simde_svreinterpret_s16_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16(   simde_svint32_t op) { return  simde_svreinterpret_s16_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16(   simde_svint64_t op) { return  simde_svreinterpret_s16_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16(   simde_svuint8_t op) { return   simde_svreinterpret_s16_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16(  simde_svuint16_t op) { return  simde_svreinterpret_s16_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16(  simde_svuint32_t op) { return  simde_svreinterpret_s16_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16(  simde_svuint64_t op) { return  simde_svreinterpret_s16_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16( simde_svfloat16_t op) { return  simde_svreinterpret_s16_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16( simde_svfloat32_t op) { return  simde_svreinterpret_s16_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16( simde_svfloat64_t op) { return  simde_svreinterpret_s16_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32(    simde_svint8_t op) { return   simde_svreinterpret_s32_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32(   simde_svint16_t op) { return  simde_svreinterpret_s32_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32(   simde_svint64_t op) { return  simde_svreinterpret_s32_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32(   simde_svuint8_t op) { return   simde_svreinterpret_s32_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32(  simde_svuint16_t op) { return  simde_svreinterpret_s32_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32(  simde_svuint32_t op) { return  simde_svreinterpret_s32_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32(  simde_svuint64_t op) { return  simde_svreinterpret_s32_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32( simde_svfloat16_t op) { return  simde_svreinterpret_s32_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32( simde_svfloat32_t op) { return  simde_svreinterpret_s32_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32( simde_svfloat64_t op) { return  simde_svreinterpret_s32_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64(    simde_svint8_t op) { return   simde_svreinterpret_s64_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64(   simde_svint16_t op) { return  simde_svreinterpret_s64_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64(   simde_svint32_t op) { return  simde_svreinterpret_s64_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64(   simde_svuint8_t op) { return   simde_svreinterpret_s64_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64(  simde_svuint16_t op) { return  simde_svreinterpret_s64_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64(  simde_svuint32_t op) { return  simde_svreinterpret_s64_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64(  simde_svuint64_t op) { return  simde_svreinterpret_s64_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64( simde_svfloat16_t op) { return  simde_svreinterpret_s64_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64( simde_svfloat32_t op) { return  simde_svreinterpret_s64_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64( simde_svfloat64_t op) { return  simde_svreinterpret_s64_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8(    simde_svint8_t op) { return    simde_svreinterpret_u8_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8(   simde_svint16_t op) { return   simde_svreinterpret_u8_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8(   simde_svint32_t op) { return   simde_svreinterpret_u8_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8(   simde_svint64_t op) { return   simde_svreinterpret_u8_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8(  simde_svuint16_t op) { return   simde_svreinterpret_u8_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8(  simde_svuint32_t op) { return   simde_svreinterpret_u8_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8(  simde_svuint64_t op) { return   simde_svreinterpret_u8_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8( simde_svfloat16_t op) { return   simde_svreinterpret_u8_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8( simde_svfloat32_t op) { return   simde_svreinterpret_u8_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8( simde_svfloat64_t op) { return   simde_svreinterpret_u8_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16(    simde_svint8_t op) { return   simde_svreinterpret_u16_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16(   simde_svint16_t op) { return  simde_svreinterpret_u16_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16(   simde_svint32_t op) { return  simde_svreinterpret_u16_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16(   simde_svint64_t op) { return  simde_svreinterpret_u16_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16(   simde_svuint8_t op) { return   simde_svreinterpret_u16_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16(  simde_svuint32_t op) { return  simde_svreinterpret_u16_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16(  simde_svuint64_t op) { return  simde_svreinterpret_u16_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16( simde_svfloat16_t op) { return  simde_svreinterpret_u16_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16( simde_svfloat32_t op) { return  simde_svreinterpret_u16_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16( simde_svfloat64_t op) { return  simde_svreinterpret_u16_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32(    simde_svint8_t op) { return   simde_svreinterpret_u32_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32(   simde_svint16_t op) { return  simde_svreinterpret_u32_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32(   simde_svint32_t op) { return  simde_svreinterpret_u32_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32(   simde_svint64_t op) { return  simde_svreinterpret_u32_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32(   simde_svuint8_t op) { return   simde_svreinterpret_u32_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32(  simde_svuint16_t op) { return  simde_svreinterpret_u32_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32(  simde_svuint64_t op) { return  simde_svreinterpret_u32_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32( simde_svfloat16_t op) { return  simde_svreinterpret_u32_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32( simde_svfloat32_t op) { return  simde_svreinterpret_u32_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32( simde_svfloat64_t op) { return  simde_svreinterpret_u32_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64(    simde_svint8_t op) { return   simde_svreinterpret_u64_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64(   simde_svint16_t op) { return  simde_svreinterpret_u64_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64(   simde_svint32_t op) { return  simde_svreinterpret_u64_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64(   simde_svint64_t op) { return  simde_svreinterpret_u64_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64(   simde_svuint8_t op) { return   simde_svreinterpret_u64_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64(  simde_svuint16_t op) { return  simde_svreinterpret_u64_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64(  simde_svuint32_t op) { return  simde_svreinterpret_u64_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64( simde_svfloat16_t op) { return  simde_svreinterpret_u64_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64( simde_svfloat32_t op) { return  simde_svreinterpret_u64_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64( simde_svfloat64_t op) { return  simde_svreinterpret_u64_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16(    simde_svint8_t op) { return   simde_svreinterpret_f16_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16(   simde_svint16_t op) { return  simde_svreinterpret_f16_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16(   simde_svint32_t op) { return  simde_svreinterpret_f16_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16(   simde_svint64_t op) { return  simde_svreinterpret_f16_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16(   simde_svuint8_t op) { return   simde_svreinterpret_f16_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16(  simde_svuint16_t op) { return  simde_svreinterpret_f16_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16(  simde_svuint32_t op) { return  simde_svreinterpret_f16_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16(  simde_svuint64_t op) { return  simde_svreinterpret_f16_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16( simde_svfloat32_t op) { return  simde_svreinterpret_f16_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16( simde_svfloat64_t op) { return  simde_svreinterpret_f16_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32(    simde_svint8_t op) { return   simde_svreinterpret_f32_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32(   simde_svint16_t op) { return  simde_svreinterpret_f32_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32(   simde_svint32_t op) { return  simde_svreinterpret_f32_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32(   simde_svint64_t op) { return  simde_svreinterpret_f32_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32(   simde_svuint8_t op) { return   simde_svreinterpret_f32_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32(  simde_svuint16_t op) { return  simde_svreinterpret_f32_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32(  simde_svuint32_t op) { return  simde_svreinterpret_f32_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32(  simde_svuint64_t op) { return  simde_svreinterpret_f32_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32( simde_svfloat16_t op) { return  simde_svreinterpret_f32_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32( simde_svfloat64_t op) { return  simde_svreinterpret_f32_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64(    simde_svint8_t op) { return   simde_svreinterpret_f64_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64(   simde_svint16_t op) { return  simde_svreinterpret_f64_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64(   simde_svint32_t op) { return  simde_svreinterpret_f64_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64(   simde_svint64_t op) { return  simde_svreinterpret_f64_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64(   simde_svuint8_t op) { return   simde_svreinterpret_f64_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64(  simde_svuint16_t op) { return  simde_svreinterpret_f64_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64(  simde_svuint32_t op) { return  simde_svreinterpret_f64_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64(  simde_svuint64_t op) { return  simde_svreinterpret_f64_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64( simde_svfloat16_t op) { return  simde_svreinterpret_f64_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64( simde_svfloat32_t op) { return  simde_svreinterpret_f64_f32(op); }

  #if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_s8,     svint8_t,    svint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_s8,     svint8_t,    svint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_s8,     svint8_t,    svint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_s8,     svint8_t,    svuint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_s8,     svint8_t,   svuint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_s8,     svint8_t,   svuint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_s8,     svint8_t,   svuint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_s8,     svint8_t,  svfloat16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_s8,     svint8_t,  svfloat32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_s8,     svint8_t,  svfloat64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s16,    svint16_t,     svint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s16,    svint16_t,    svint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s16,    svint16_t,    svint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s16,    svint16_t,    svuint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s16,    svint16_t,   svuint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s16,    svint16_t,   svuint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s16,    svint16_t,   svuint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s16,    svint16_t,  svfloat16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s16,    svint16_t,  svfloat32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s16,    svint16_t,  svfloat64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s32,    svint32_t,     svint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s32,    svint32_t,    svint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s32,    svint32_t,    svint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s32,    svint32_t,    svuint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s32,    svint32_t,   svuint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s32,    svint32_t,   svuint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s32,    svint32_t,   svuint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s32,    svint32_t,  svfloat16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s32,    svint32_t,  svfloat32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s32,    svint32_t,  svfloat64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s64,    svint64_t,     svint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s64,    svint64_t,    svint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s64,    svint64_t,    svint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s64,    svint64_t,    svuint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s64,    svint64_t,   svuint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s64,    svint64_t,   svuint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s64,    svint64_t,   svuint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s64,    svint64_t,  svfloat16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s64,    svint64_t,  svfloat32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s64,    svint64_t,  svfloat64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_u8,    svuint8_t,     svint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_u8,    svuint8_t,    svint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_u8,    svuint8_t,    svint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_u8,    svuint8_t,    svint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_u8,    svuint8_t,   svuint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_u8,    svuint8_t,   svuint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_u8,    svuint8_t,   svuint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_u8,    svuint8_t,  svfloat16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_u8,    svuint8_t,  svfloat32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_u8,    svuint8_t,  svfloat64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u16,   svuint16_t,     svint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u16,   svuint16_t,    svint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u16,   svuint16_t,    svint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u16,   svuint16_t,    svint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u16,   svuint16_t,    svuint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u16,   svuint16_t,   svuint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u16,   svuint16_t,   svuint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u16,   svuint16_t,  svfloat16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u16,   svuint16_t,  svfloat32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u16,   svuint16_t,  svfloat64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u32,   svuint32_t,     svint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u32,   svuint32_t,    svint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u32,   svuint32_t,    svint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u32,   svuint32_t,    svint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u32,   svuint32_t,    svuint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u32,   svuint32_t,   svuint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u32,   svuint32_t,   svuint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u32,   svuint32_t,  svfloat16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u32,   svuint32_t,  svfloat32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u32,   svuint32_t,  svfloat64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u64,   svuint64_t,     svint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u64,   svuint64_t,    svint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u64,   svuint64_t,    svint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u64,   svuint64_t,    svint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u64,   svuint64_t,    svuint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u64,   svuint64_t,   svuint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u64,   svuint64_t,   svuint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u64,   svuint64_t,  svfloat16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u64,   svuint64_t,  svfloat32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u64,   svuint64_t,  svfloat64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f16,  svfloat16_t,     svint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f16,  svfloat16_t,    svint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f16,  svfloat16_t,    svint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f16,  svfloat16_t,    svint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f16,  svfloat16_t,    svuint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f16,  svfloat16_t,   svuint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f16,  svfloat16_t,   svuint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f16,  svfloat16_t,   svuint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f16,  svfloat16_t,  svfloat32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f16,  svfloat16_t,  svfloat64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f32,  svfloat32_t,     svint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f32,  svfloat32_t,    svint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f32,  svfloat32_t,    svint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f32,  svfloat32_t,    svint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f32,  svfloat32_t,    svuint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f32,  svfloat32_t,   svuint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f32,  svfloat32_t,   svuint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f32,  svfloat32_t,   svuint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f32,  svfloat32_t,  svfloat16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f32,  svfloat32_t,  svfloat64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f64,  svfloat64_t,     svint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f64,  svfloat64_t,    svint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f64,  svfloat64_t,    svint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f64,  svfloat64_t,    svint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f64,  svfloat64_t,    svuint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f64,  svfloat64_t,   svuint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f64,  svfloat64_t,   svuint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f64,  svfloat64_t,   svuint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f64,  svfloat64_t,  svfloat16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f64,  svfloat64_t,  svfloat32_t)
  #endif /* defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES) */
#elif defined(SIMDE_GENERIC_)
  #define simde_svreinterpret_f64(op) \
    (_Generic((op), \
        simde_svint16_t: simde_svreinterpret_s8_s16, \
        simde_svint32_t: simde_svreinterpret_s8_s32, \
        simde_svint64_t: simde_svreinterpret_s8_s64, \
        simde_svuint8_t: simde_svreinterpret_s8_u8, \
       simde_svuint16_t: simde_svreinterpret_s8_u16, \
       simde_svuint32_t: simde_svreinterpret_s8_u32, \
       simde_svuint64_t: simde_svreinterpret_s8_u64, \
      simde_svfloat16_t: simde_svreinterpret_s8_f16, \
      simde_svfloat32_t: simde_svreinterpret_s8_f32, \
      simde_svfloat64_t: simde_svreinterpret_s8_f64)(op))
  #define simde_svreinterpret_s8(op) \
    (_Generic((op), \
         simde_svint8_t: simde_svreinterpret_s16_s8, \
        simde_svint32_t: simde_svreinterpret_s16_s32, \
        simde_svint64_t: simde_svreinterpret_s16_s64, \
        simde_svuint8_t: simde_svreinterpret_s16_u8, \
       simde_svuint16_t: simde_svreinterpret_s16_u16, \
       simde_svuint32_t: simde_svreinterpret_s16_u32, \
       simde_svuint64_t: simde_svreinterpret_s16_u64, \
      simde_svfloat16_t: simde_svreinterpret_s16_f16, \
      simde_svfloat32_t: simde_svreinterpret_s16_f32, \
      simde_svfloat64_t: simde_svreinterpret_s16_f64)(op))
  #define simde_svreinterpret_s16(op) \
    (_Generic((op), \
         simde_svint8_t: simde_svreinterpret_s32_s8, \
        simde_svint16_t: simde_svreinterpret_s32_s16, \
        simde_svint64_t: simde_svreinterpret_s32_s64, \
        simde_svuint8_t: simde_svreinterpret_s32_u8, \
       simde_svuint16_t: simde_svreinterpret_s32_u16, \
       simde_svuint32_t: simde_svreinterpret_s32_u32, \
       simde_svuint64_t: simde_svreinterpret_s32_u64, \
      simde_svfloat16_t: simde_svreinterpret_s32_f16, \
      simde_svfloat32_t: simde_svreinterpret_s32_f32, \
      simde_svfloat64_t: simde_svreinterpret_s32_f64)(op))
  #define simde_svreinterpret_s32(op) \
    (_Generic((op), \
         simde_svint8_t: simde_svreinterpret_s64_s8, \
        simde_svint16_t: simde_svreinterpret_s64_s16, \
        simde_svint32_t: simde_svreinterpret_s64_s32, \
        simde_svuint8_t: simde_svreinterpret_s64_u8, \
       simde_svuint16_t: simde_svreinterpret_s64_u16, \
       simde_svuint32_t: simde_svreinterpret_s64_u32, \
       simde_svuint64_t: simde_svreinterpret_s64_u64, \
      simde_svfloat16_t: simde_svreinterpret_s64_f16, \
      simde_svfloat32_t: simde_svreinterpret_s64_f32, \
      simde_svfloat64_t: simde_svreinterpret_s64_f64)(op))
  #define simde_svreinterpret_s64(op) \
    (_Generic((op), \
         simde_svint8_t: simde_svreinterpret_u8_s8, \
        simde_svint16_t: simde_svreinterpret_u8_s16, \
        simde_svint32_t: simde_svreinterpret_u8_s32, \
        simde_svint64_t: simde_svreinterpret_u8_s64, \
       simde_svuint16_t: simde_svreinterpret_u8_u16, \
       simde_svuint32_t: simde_svreinterpret_u8_u32, \
       simde_svuint64_t: simde_svreinterpret_u8_u64, \
      simde_svfloat16_t: simde_svreinterpret_u8_f16, \
      simde_svfloat32_t: simde_svreinterpret_u8_f32, \
      simde_svfloat64_t: simde_svreinterpret_u8_f64)(op))
  #define simde_svreinterpret_u8(op) \
    (_Generic((op), \
         simde_svint8_t: simde_svreinterpret_u16_s8, \
        simde_svint16_t: simde_svreinterpret_u16_s16, \
        simde_svint32_t: simde_svreinterpret_u16_s32, \
        simde_svint64_t: simde_svreinterpret_u16_s64, \
        simde_svuint8_t: simde_svreinterpret_u16_u8, \
       simde_svuint32_t: simde_svreinterpret_u16_u32, \
       simde_svuint64_t: simde_svreinterpret_u16_u64, \
      simde_svfloat16_t: simde_svreinterpret_u16_f16, \
      simde_svfloat32_t: simde_svreinterpret_u16_f32, \
      simde_svfloat64_t: simde_svreinterpret_u16_f64)(op))
  #define simde_svreinterpret_u16(op) \
    (_Generic((op), \
         simde_svint8_t: simde_svreinterpret_u32_s8, \
        simde_svint16_t: simde_svreinterpret_u32_s16, \
        simde_svint32_t: simde_svreinterpret_u32_s32, \
        simde_svint64_t: simde_svreinterpret_u32_s64, \
        simde_svuint8_t: simde_svreinterpret_u32_u8, \
       simde_svuint16_t: simde_svreinterpret_u32_u16, \
       simde_svuint64_t: simde_svreinterpret_u32_u64, \
      simde_svfloat16_t: simde_svreinterpret_u32_f16, \
      simde_svfloat32_t: simde_svreinterpret_u32_f32, \
      simde_svfloat64_t: simde_svreinterpret_u32_f64)(op))
  #define simde_svreinterpret_u32(op) \
    (_Generic((op), \
         simde_svint8_t: simde_svreinterpret_u64_s8, \
        simde_svint16_t: simde_svreinterpret_u64_s16, \
        simde_svint32_t: simde_svreinterpret_u64_s32, \
        simde_svint64_t: simde_svreinterpret_u64_s64, \
        simde_svuint8_t: simde_svreinterpret_u64_u8, \
       simde_svuint16_t: simde_svreinterpret_u64_u16, \
       simde_svuint32_t: simde_svreinterpret_u64_u32, \
      simde_svfloat16_t: simde_svreinterpret_u64_f16, \
      simde_svfloat32_t: simde_svreinterpret_u64_f32, \
      simde_svfloat64_t: simde_svreinterpret_u64_f64)(op))
  #define simde_svreinterpret_u64(op) \
    (_Generic((op), \
         simde_svint8_t: simde_svreinterpret_f16_s8, \
        simde_svint16_t: simde_svreinterpret_f16_s16, \
        simde_svint32_t: simde_svreinterpret_f16_s32, \
        simde_svint64_t: simde_svreinterpret_f16_s64, \
        simde_svuint8_t: simde_svreinterpret_f16_u8, \
       simde_svuint16_t: simde_svreinterpret_f16_u16, \
       simde_svuint32_t: simde_svreinterpret_f16_u32, \
       simde_svuint64_t: simde_svreinterpret_f16_u64, \
      simde_svfloat32_t: simde_svreinterpret_f16_f32, \
      simde_svfloat64_t: simde_svreinterpret_f16_f64)(op))
  #define simde_svreinterpret_f16(op) \
    (_Generic((op), \
         simde_svint8_t: simde_svreinterpret_f32_s8, \
        simde_svint16_t: simde_svreinterpret_f32_s16, \
        simde_svint32_t: simde_svreinterpret_f32_s32, \
        simde_svint64_t: simde_svreinterpret_f32_s64, \
        simde_svuint8_t: simde_svreinterpret_f32_u8, \
       simde_svuint16_t: simde_svreinterpret_f32_u16, \
       simde_svuint32_t: simde_svreinterpret_f32_u32, \
       simde_svuint64_t: simde_svreinterpret_f32_u64, \
      simde_svfloat16_t: simde_svreinterpret_f32_f16, \
      simde_svfloat64_t: simde_svreinterpret_f32_f64)(op))
  #define simde_svreinterpret_f32(op) \
    (_Generic((op), \
         simde_svint8_t: simde_svreinterpret_f64_s8, \
        simde_svint16_t: simde_svreinterpret_f64_s16, \
        simde_svint32_t: simde_svreinterpret_f64_s32, \
        simde_svint64_t: simde_svreinterpret_f64_s64, \
        simde_svuint8_t: simde_svreinterpret_f64_u8, \
       simde_svuint16_t: simde_svreinterpret_f64_u16, \
       simde_svuint32_t: simde_svreinterpret_f64_u32, \
       simde_svuint64_t: simde_svreinterpret_f64_u64, \
      simde_svfloat16_t: simde_svreinterpret_f64_f16, \
      simde_svfloat32_t: simde_svreinterpret_f64_f32)(op))
  #if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #define svreinterpret_f64(op) \
    (_Generic((op), \
              svint16_t: svreinterpret_s8_s16, \
              svint32_t: svreinterpret_s8_s32, \
              svint64_t: svreinterpret_s8_s64, \
              svuint8_t: svreinterpret_s8_u8, \
             svuint16_t: svreinterpret_s8_u16, \
             svuint32_t: svreinterpret_s8_u32, \
             svuint64_t: svreinterpret_s8_u64, \
            svfloat16_t: svreinterpret_s8_f16, \
            svfloat32_t: svreinterpret_s8_f32, \
            svfloat64_t: svreinterpret_s8_f64)(op))
  #define svreinterpret_s8(op) \
    (_Generic((op), \
               svint8_t: svreinterpret_s16_s8, \
              svint32_t: svreinterpret_s16_s32, \
              svint64_t: svreinterpret_s16_s64, \
              svuint8_t: svreinterpret_s16_u8, \
             svuint16_t: svreinterpret_s16_u16, \
             svuint32_t: svreinterpret_s16_u32, \
             svuint64_t: svreinterpret_s16_u64, \
            svfloat16_t: svreinterpret_s16_f16, \
            svfloat32_t: svreinterpret_s16_f32, \
            svfloat64_t: svreinterpret_s16_f64)(op))
  #define svreinterpret_s16(op) \
    (_Generic((op), \
               svint8_t: svreinterpret_s32_s8, \
              svint16_t: svreinterpret_s32_s16, \
              svint64_t: svreinterpret_s32_s64, \
              svuint8_t: svreinterpret_s32_u8, \
             svuint16_t: svreinterpret_s32_u16, \
             svuint32_t: svreinterpret_s32_u32, \
             svuint64_t: svreinterpret_s32_u64, \
            svfloat16_t: svreinterpret_s32_f16, \
            svfloat32_t: svreinterpret_s32_f32, \
            svfloat64_t: svreinterpret_s32_f64)(op))
  #define svreinterpret_s32(op) \
    (_Generic((op), \
               svint8_t: svreinterpret_s64_s8, \
              svint16_t: svreinterpret_s64_s16, \
              svint32_t: svreinterpret_s64_s32, \
              svuint8_t: svreinterpret_s64_u8, \
             svuint16_t: svreinterpret_s64_u16, \
             svuint32_t: svreinterpret_s64_u32, \
             svuint64_t: svreinterpret_s64_u64, \
            svfloat16_t: svreinterpret_s64_f16, \
            svfloat32_t: svreinterpret_s64_f32, \
            svfloat64_t: svreinterpret_s64_f64)(op))
  #define svreinterpret_s64(op) \
    (_Generic((op), \
               svint8_t: svreinterpret_u8_s8, \
              svint16_t: svreinterpret_u8_s16, \
              svint32_t: svreinterpret_u8_s32, \
              svint64_t: svreinterpret_u8_s64, \
             svuint16_t: svreinterpret_u8_u16, \
             svuint32_t: svreinterpret_u8_u32, \
             svuint64_t: svreinterpret_u8_u64, \
            svfloat16_t: svreinterpret_u8_f16, \
            svfloat32_t: svreinterpret_u8_f32, \
            svfloat64_t: svreinterpret_u8_f64)(op))
  #define svreinterpret_u8(op) \
    (_Generic((op), \
               svint8_t: svreinterpret_u16_s8, \
              svint16_t: svreinterpret_u16_s16, \
              svint32_t: svreinterpret_u16_s32, \
              svint64_t: svreinterpret_u16_s64, \
              svuint8_t: svreinterpret_u16_u8, \
             svuint32_t: svreinterpret_u16_u32, \
             svuint64_t: svreinterpret_u16_u64, \
            svfloat16_t: svreinterpret_u16_f16, \
            svfloat32_t: svreinterpret_u16_f32, \
            svfloat64_t: svreinterpret_u16_f64)(op))
  #define svreinterpret_u16(op) \
    (_Generic((op), \
               svint8_t: svreinterpret_u32_s8, \
              svint16_t: svreinterpret_u32_s16, \
              svint32_t: svreinterpret_u32_s32, \
              svint64_t: svreinterpret_u32_s64, \
              svuint8_t: svreinterpret_u32_u8, \
             svuint16_t: svreinterpret_u32_u16, \
             svuint64_t: svreinterpret_u32_u64, \
            svfloat16_t: svreinterpret_u32_f16, \
            svfloat32_t: svreinterpret_u32_f32, \
            svfloat64_t: svreinterpret_u32_f64)(op))
  #define svreinterpret_u32(op) \
    (_Generic((op), \
               svint8_t: svreinterpret_u64_s8, \
              svint16_t: svreinterpret_u64_s16, \
              svint32_t: svreinterpret_u64_s32, \
              svint64_t: svreinterpret_u64_s64, \
              svuint8_t: svreinterpret_u64_u8, \
             svuint16_t: svreinterpret_u64_u16, \
             svuint32_t: svreinterpret_u64_u32, \
            svfloat16_t: svreinterpret_u64_f16, \
            svfloat32_t: svreinterpret_u64_f32, \
            svfloat64_t: svreinterpret_u64_f64)(op))
  #define svreinterpret_u64(op) \
    (_Generic((op), \
               svint8_t: svreinterpret_f16_s8, \
              svint16_t: svreinterpret_f16_s16, \
              svint32_t: svreinterpret_f16_s32, \
              svint64_t: svreinterpret_f16_s64, \
              svuint8_t: svreinterpret_f16_u8, \
             svuint16_t: svreinterpret_f16_u16, \
             svuint32_t: svreinterpret_f16_u32, \
             svuint64_t: svreinterpret_f16_u64, \
            svfloat32_t: svreinterpret_f16_f32, \
            svfloat64_t: svreinterpret_f16_f64)(op))
  #define svreinterpret_f16(op) \
    (_Generic((op), \
               svint8_t: svreinterpret_f32_s8, \
              svint16_t: svreinterpret_f32_s16, \
              svint32_t: svreinterpret_f32_s32, \
              svint64_t: svreinterpret_f32_s64, \
              svuint8_t: svreinterpret_f32_u8, \
             svuint16_t: svreinterpret_f32_u16, \
             svuint32_t: svreinterpret_f32_u32, \
             svuint64_t: svreinterpret_f32_u64, \
            svfloat16_t: svreinterpret_f32_f16, \
            svfloat64_t: svreinterpret_f32_f64)(op))
  #define svreinterpret_f32(op) \
    (_Generic((op), \
               svint8_t: svreinterpret_f64_s8, \
              svint16_t: svreinterpret_f64_s16, \
              svint32_t: svreinterpret_f64_s32, \
              svint64_t: svreinterpret_f64_s64, \
              svuint8_t: svreinterpret_f64_u8, \
             svuint16_t: svreinterpret_f64_u16, \
             svuint32_t: svreinterpret_f64_u32, \
             svuint64_t: svreinterpret_f64_u64, \
            svfloat16_t: svreinterpret_f64_f16, \
            svfloat32_t: svreinterpret_f64_f32)(op))
  #endif /* defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES) */
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_REINTERPRET_H */
/* :: End simde/arm/sve/reinterpret.h :: */

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_x_svsel_s8_z(simde_svbool_t pg, simde_svint8_t op1) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_s8_z(pg, op1, op1);
  #else
    simde_svint8_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vandq_s8(pg.neon_i8, op1.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_maskz_mov_epi8(simde_svbool_to_mmask64(pg), op1.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_maskz_mov_epi8(simde_svbool_to_mmask32(pg), op1.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_and_si256(pg.m256i[i], op1.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_and_si128(pg.m128i[i], op1.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_and(pg.altivec_b8, op1.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = pg.values_i8 & op1.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_and(pg.v128, op1.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = pg.values_i8 & op1.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = pg.values_i8[i] & op1.values[i];
      }
    #endif

    return r;
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svsel_s8(simde_svbool_t pg, simde_svint8_t op1, simde_svint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsel_s8(pg, op1, op2);
  #else
    simde_svint8_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vbslq_s8(pg.neon_u8, op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_mask_mov_epi8(op2.m512i, simde_svbool_to_mmask64(pg), op1.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_mask_mov_epi8(op2.m256i[0], simde_svbool_to_mmask32(pg), op1.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_blendv_epi8(op2.m256i[i], op1.m256i[i], pg.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE4_1_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_blendv_epi8(op2.m128i[i], op1.m128i[i], pg.m128i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_or_si128(_mm_and_si128(pg.m128i[i], op1.m128i[i]), _mm_andnot_si128(pg.m128i[i], op2.m128i[i]));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_sel(op2.altivec, op1.altivec, pg.altivec_b8);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_bitselect(op1.v128, op2.v128, pg.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = (pg.values_i8 & op1.values) | (~pg.values_i8 & op2.values);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = (pg.values_i8[i] & op1.values[i]) | (~pg.values_i8[i] & op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsel_s8
  #define svsel_s8(pg, op1, op2) simde_svsel_s8(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_x_svsel_s16_z(simde_svbool_t pg, simde_svint16_t op1) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_s16_z(pg, op1, op1);
  #else
    simde_svint16_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vandq_s16(pg.neon_i16, op1.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_maskz_mov_epi16(simde_svbool_to_mmask32(pg), op1.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_maskz_mov_epi16(simde_svbool_to_mmask16(pg), op1.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_and_si256(pg.m256i[i], op1.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_and_si128(pg.m128i[i], op1.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_and(pg.altivec_b16, op1.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = pg.values_i16 & op1.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_and(pg.v128, op1.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = pg.values_i16 & op1.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = pg.values_i16[i] & op1.values[i];
      }
    #endif

    return r;
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svsel_s16(simde_svbool_t pg, simde_svint16_t op1, simde_svint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsel_s16(pg, op1, op2);
  #else
    simde_svint16_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vbslq_s16(pg.neon_u16, op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_mask_mov_epi16(op2.m512i, simde_svbool_to_mmask32(pg), op1.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_mask_mov_epi16(op2.m256i[0], simde_svbool_to_mmask16(pg), op1.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_blendv_epi8(op2.m256i[i], op1.m256i[i], pg.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE4_1_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_blendv_epi8(op2.m128i[i], op1.m128i[i], pg.m128i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_or_si128(_mm_and_si128(pg.m128i[i], op1.m128i[i]), _mm_andnot_si128(pg.m128i[i], op2.m128i[i]));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_sel(op2.altivec, op1.altivec, pg.altivec_b16);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_bitselect(op1.v128, op2.v128, pg.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = (pg.values_i16 & op1.values) | (~pg.values_i16 & op2.values);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = (pg.values_i16[i] & op1.values[i]) | (~pg.values_i16[i] & op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsel_s16
  #define svsel_s16(pg, op1, op2) simde_svsel_s16(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_x_svsel_s32_z(simde_svbool_t pg, simde_svint32_t op1) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_s32_z(pg, op1, op1);
  #else
    simde_svint32_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vandq_s32(pg.neon_i32, op1.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_maskz_mov_epi32(simde_svbool_to_mmask16(pg), op1.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_maskz_mov_epi32(simde_svbool_to_mmask8(pg), op1.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_and_si256(pg.m256i[i], op1.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_and_si128(pg.m128i[i], op1.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_and(pg.altivec_b32, op1.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = pg.values_i32 & op1.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_and(pg.v128, op1.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = pg.values_i32 & op1.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = pg.values_i32[i] & op1.values[i];
      }
    #endif

    return r;
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svsel_s32(simde_svbool_t pg, simde_svint32_t op1, simde_svint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsel_s32(pg, op1, op2);
  #else
    simde_svint32_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vbslq_s32(pg.neon_u32, op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_mask_mov_epi32(op2.m512i, simde_svbool_to_mmask16(pg), op1.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_mask_mov_epi32(op2.m256i[0], simde_svbool_to_mmask8(pg), op1.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_blendv_epi8(op2.m256i[i], op1.m256i[i], pg.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE4_1_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_blendv_epi8(op2.m128i[i], op1.m128i[i], pg.m128i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_or_si128(_mm_and_si128(pg.m128i[i], op1.m128i[i]), _mm_andnot_si128(pg.m128i[i], op2.m128i[i]));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_sel(op2.altivec, op1.altivec, pg.altivec_b32);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_bitselect(op1.v128, op2.v128, pg.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = (pg.values_i32 & op1.values) | (~pg.values_i32 & op2.values);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = (pg.values_i32[i] & op1.values[i]) | (~pg.values_i32[i] & op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsel_s32
  #define svsel_s32(pg, op1, op2) simde_svsel_s32(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_x_svsel_s64_z(simde_svbool_t pg, simde_svint64_t op1) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_s64_z(pg, op1, op1);
  #else
    simde_svint64_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vandq_s64(pg.neon_i64, op1.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_maskz_mov_epi64(simde_svbool_to_mmask8(pg), op1.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_maskz_mov_epi64(simde_svbool_to_mmask4(pg), op1.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_and_si256(pg.m256i[i], op1.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_and_si128(pg.m128i[i], op1.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
      r.altivec = vec_and(pg.altivec_b64, op1.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = HEDLEY_REINTERPRET_CAST(__typeof__(op1.altivec), pg.values_i64) & op1.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_and(pg.v128, op1.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = pg.values_i64 & op1.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = pg.values_i64[i] & op1.values[i];
      }
    #endif

    return r;
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svsel_s64(simde_svbool_t pg, simde_svint64_t op1, simde_svint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsel_s64(pg, op1, op2);
  #else
    simde_svint64_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vbslq_s64(pg.neon_u64, op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m512i = _mm512_mask_mov_epi64(op2.m512i, simde_svbool_to_mmask8(pg), op1.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r.m256i[0] = _mm256_mask_mov_epi64(op2.m256i[0], simde_svbool_to_mmask4(pg), op1.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_blendv_epi8(op2.m256i[i], op1.m256i[i], pg.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE4_1_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_blendv_epi8(op2.m128i[i], op1.m128i[i], pg.m128i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_or_si128(_mm_and_si128(pg.m128i[i], op1.m128i[i]), _mm_andnot_si128(pg.m128i[i], op2.m128i[i]));
      }
    #elif (defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)) && !defined(SIMDE_BUG_CLANG_46770)
      r.altivec = vec_sel(op2.altivec, op1.altivec, pg.altivec_b64);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_bitselect(op1.v128, op2.v128, pg.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = (pg.values_i64 & op1.values) | (~pg.values_i64 & op2.values);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = (pg.values_i64[i] & op1.values[i]) | (~pg.values_i64[i] & op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsel_s64
  #define svsel_s64(pg, op1, op2) simde_svsel_s64(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_x_svsel_u8_z(simde_svbool_t pg, simde_svuint8_t op1) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_u8_z(pg, op1, op1);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && ((SIMDE_ARM_SVE_VECTOR_SIZE >= 512) || defined(SIMDE_X86_AVX512VL_NATIVE)) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    simde_svuint8_t r;

    #if SIMDE_ARM_SVE_VECTOR_SIZE >= 512
      r.m512i = _mm512_maskz_mov_epi8(simde_svbool_to_mmask64(pg), op1.m512i);
    #else
      r.m256i[0] = _mm256_maskz_mov_epi8(simde_svbool_to_mmask32(pg), op1.m256i[0]);
    #endif

    return r;
  #else
    return simde_svreinterpret_u8_s8(simde_x_svsel_s8_z(pg, simde_svreinterpret_s8_u8(op1)));
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svsel_u8(simde_svbool_t pg, simde_svuint8_t op1, simde_svuint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsel_u8(pg, op1, op2);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && ((SIMDE_ARM_SVE_VECTOR_SIZE >= 512) || defined(SIMDE_X86_AVX512VL_NATIVE)) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    simde_svuint8_t r;

    #if SIMDE_ARM_SVE_VECTOR_SIZE >= 512
      r.m512i = _mm512_mask_mov_epi8(op2.m512i, simde_svbool_to_mmask64(pg), op1.m512i);
    #else
      r.m256i[0] = _mm256_mask_mov_epi8(op2.m256i[0], simde_svbool_to_mmask32(pg), op1.m256i[0]);
    #endif

    return r;
  #else
    return simde_svreinterpret_u8_s8(simde_svsel_s8(pg, simde_svreinterpret_s8_u8(op1), simde_svreinterpret_s8_u8(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsel_u8
  #define svsel_u8(pg, op1, op2) simde_svsel_u8(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_x_svsel_u16_z(simde_svbool_t pg, simde_svuint16_t op1) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_u16_z(pg, op1, op1);
  #else
    return simde_svreinterpret_u16_s16(simde_x_svsel_s16_z(pg, simde_svreinterpret_s16_u16(op1)));
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svsel_u16(simde_svbool_t pg, simde_svuint16_t op1, simde_svuint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsel_u16(pg, op1, op2);
  #else
    return simde_svreinterpret_u16_s16(simde_svsel_s16(pg, simde_svreinterpret_s16_u16(op1), simde_svreinterpret_s16_u16(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsel_u16
  #define svsel_u16(pg, op1, op2) simde_svsel_u16(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_x_svsel_u32_z(simde_svbool_t pg, simde_svuint32_t op1) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_u32_z(pg, op1, op1);
  #else
    return simde_svreinterpret_u32_s32(simde_x_svsel_s32_z(pg, simde_svreinterpret_s32_u32(op1)));
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svsel_u32(simde_svbool_t pg, simde_svuint32_t op1, simde_svuint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsel_u32(pg, op1, op2);
  #else
    return simde_svreinterpret_u32_s32(simde_svsel_s32(pg, simde_svreinterpret_s32_u32(op1), simde_svreinterpret_s32_u32(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsel_u32
  #define svsel_u32(pg, op1, op2) simde_svsel_u32(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_x_svsel_u64_z(simde_svbool_t pg, simde_svuint64_t op1) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_u64_z(pg, op1, op1);
  #else
    return simde_svreinterpret_u64_s64(simde_x_svsel_s64_z(pg, simde_svreinterpret_s64_u64(op1)));
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svsel_u64(simde_svbool_t pg, simde_svuint64_t op1, simde_svuint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsel_u64(pg, op1, op2);
  #else
    return simde_svreinterpret_u64_s64(simde_svsel_s64(pg, simde_svreinterpret_s64_u64(op1), simde_svreinterpret_s64_u64(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsel_u64
  #define svsel_u64(pg, op1, op2) simde_svsel_u64(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_x_svsel_f32_z(simde_svbool_t pg, simde_svfloat32_t op1) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return simde_svreinterpret_f32_s32(svand_s32_z(pg, simde_svreinterpret_s32_f32(op1), simde_svreinterpret_s32_f32(op1)));
  #else
    return simde_svreinterpret_f32_s32(simde_x_svsel_s32_z(pg, simde_svreinterpret_s32_f32(op1)));
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svsel_f32(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsel_f32(pg, op1, op2);
  #else
    return simde_svreinterpret_f32_s32(simde_svsel_s32(pg, simde_svreinterpret_s32_f32(op1), simde_svreinterpret_s32_f32(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsel_f32
  #define svsel_f32(pg, op1, op2) simde_svsel_f32(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_x_svsel_f64_z(simde_svbool_t pg, simde_svfloat64_t op1) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return simde_svreinterpret_f64_s64(svand_s64_z(pg, simde_svreinterpret_s64_f64(op1), simde_svreinterpret_s64_f64(op1)));
  #else
    return simde_svreinterpret_f64_s64(simde_x_svsel_s64_z(pg, simde_svreinterpret_s64_f64(op1)));
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svsel_f64(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsel_f64(pg, op1, op2);
  #else
    return simde_svreinterpret_f64_s64(simde_svsel_s64(pg, simde_svreinterpret_s64_f64(op1), simde_svreinterpret_s64_f64(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsel_f64
  #define svsel_f64(pg, op1, op2) simde_svsel_f64(pg, op1, op2)
#endif

#if defined(__cplusplus)
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_x_svsel_z(simde_svbool_t pg,    simde_svint8_t op1) { return simde_x_svsel_s8_z (pg, op1); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_x_svsel_z(simde_svbool_t pg,   simde_svint16_t op1) { return simde_x_svsel_s16_z(pg, op1); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_x_svsel_z(simde_svbool_t pg,   simde_svint32_t op1) { return simde_x_svsel_s32_z(pg, op1); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_x_svsel_z(simde_svbool_t pg,   simde_svint64_t op1) { return simde_x_svsel_s64_z(pg, op1); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_x_svsel_z(simde_svbool_t pg,   simde_svuint8_t op1) { return simde_x_svsel_u8_z (pg, op1); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_x_svsel_z(simde_svbool_t pg,  simde_svuint16_t op1) { return simde_x_svsel_u16_z(pg, op1); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_x_svsel_z(simde_svbool_t pg,  simde_svuint32_t op1) { return simde_x_svsel_u32_z(pg, op1); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_x_svsel_z(simde_svbool_t pg,  simde_svuint64_t op1) { return simde_x_svsel_u64_z(pg, op1); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_x_svsel_z(simde_svbool_t pg, simde_svfloat32_t op1) { return simde_x_svsel_f32_z(pg, op1); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_x_svsel_z(simde_svbool_t pg, simde_svfloat64_t op1) { return simde_x_svsel_f64_z(pg, op1); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svsel(simde_svbool_t pg,    simde_svint8_t op1,    simde_svint8_t op2) { return simde_svsel_s8 (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svsel(simde_svbool_t pg,   simde_svint16_t op1,   simde_svint16_t op2) { return simde_svsel_s16(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svsel(simde_svbool_t pg,   simde_svint32_t op1,   simde_svint32_t op2) { return simde_svsel_s32(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svsel(simde_svbool_t pg,   simde_svint64_t op1,   simde_svint64_t op2) { return simde_svsel_s64(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svsel(simde_svbool_t pg,   simde_svuint8_t op1,   simde_svuint8_t op2) { return simde_svsel_u8 (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svsel(simde_svbool_t pg,  simde_svuint16_t op1,  simde_svuint16_t op2) { return simde_svsel_u16(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svsel(simde_svbool_t pg,  simde_svuint32_t op1,  simde_svuint32_t op2) { return simde_svsel_u32(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svsel(simde_svbool_t pg,  simde_svuint64_t op1,  simde_svuint64_t op2) { return simde_svsel_u64(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svsel(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) { return simde_svsel_f32(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svsel(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) { return simde_svsel_f64(pg, op1, op2); }
#elif defined(SIMDE_GENERIC_)
  #define simde_x_svsel_z(pg, op1) \
    (SIMDE_GENERIC_((op1), \
         simde_svint8_t: simde_x_svsel_s8_z, \
        simde_svint16_t: simde_x_svsel_s16_z, \
        simde_svint32_t: simde_x_svsel_s32_z, \
        simde_svint64_t: simde_x_svsel_s64_z, \
        simde_svuint8_t: simde_x_svsel_u8_z, \
       simde_svuint16_t: simde_x_svsel_u16_z, \
       simde_svuint32_t: simde_x_svsel_u32_z, \
       simde_svuint64_t: simde_x_svsel_u64_z, \
      simde_svfloat32_t: simde_x_svsel_f32_z, \
      simde_svfloat64_t: simde_x_svsel_f64_z)((pg), (op1)))

  #define simde_svsel(pg, op1, op2) \
    (SIMDE_GENERIC_((op1), \
         simde_svint8_t: simde_svsel_s8, \
        simde_svint16_t: simde_svsel_s16, \
        simde_svint32_t: simde_svsel_s32, \
        simde_svint64_t: simde_svsel_s64, \
        simde_svuint8_t: simde_svsel_u8, \
       simde_svuint16_t: simde_svsel_u16, \
       simde_svuint32_t: simde_svsel_u32, \
       simde_svuint64_t: simde_svsel_u64, \
      simde_svfloat32_t: simde_svsel_f32, \
      simde_svfloat64_t: simde_svsel_f64)((pg), (op1), (op2)))
#endif
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef svsel
  #define svsel(pg, op1) simde_svsel((pg), (op1))
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_SEL_H */
/* :: End simde/arm/sve/sel.h :: */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/arm/sve/dup.h :: */
/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_ARM_SVE_DUP_H)
#define SIMDE_ARM_SVE_DUP_H

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svdup_n_s8(int8_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s8(op);
  #else
    simde_svint8_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vdupq_n_s8(op);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_set1_epi8(op);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_set1_epi8(op);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_set1_epi8(op);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_splats(op);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i8x16_splat(op);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op;
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s8
  #define svdup_n_s8(op) simde_svdup_n_s8((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svdup_s8(int8_t op) {
  return simde_svdup_n_s8(op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s8
  #define svdup_s8(op) simde_svdup_n_s8((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svdup_n_s8_z(simde_svbool_t pg, int8_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s8_z(pg, op);
  #else
    return simde_x_svsel_s8_z(pg, simde_svdup_n_s8(op));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s8_z
  #define svdup_n_s8_z(pg, op) simde_svdup_n_s8_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svdup_s8_z(simde_svbool_t pg, int8_t op) {
  return simde_svdup_n_s8_z(pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s8_z
  #define svdup_s8_z(pg, op) simde_svdup_n_s8_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svdup_n_s8_m(simde_svint8_t inactive, simde_svbool_t pg, int8_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s8_m(inactive, pg, op);
  #else
    return simde_svsel_s8(pg, simde_svdup_n_s8(op), inactive);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s8_m
  #define svdup_n_s8_m(inactive, pg, op) simde_svdup_n_s8_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svdup_s8_m(simde_svint8_t inactive, simde_svbool_t pg, int8_t op) {
  return simde_svdup_n_s8_m(inactive, pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s8_m
  #define svdup_s8_m(inactive, pg, op) simde_svdup_n_s8_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svdup_n_s16(int16_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s16(op);
  #else
    simde_svint16_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vdupq_n_s16(op);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_set1_epi16(op);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_set1_epi16(op);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_set1_epi16(op);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_splats(op);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i16x8_splat(op);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op;
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s16
  #define svdup_n_s16(op) simde_svdup_n_s16((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svdup_s16(int16_t op) {
  return simde_svdup_n_s16(op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s16
  #define svdup_s16(op) simde_svdup_n_s16((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svdup_n_s16_z(simde_svbool_t pg, int16_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s16_z(pg, op);
  #else
    return simde_x_svsel_s16_z(pg, simde_svdup_n_s16(op));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s16_z
  #define svdup_n_s16_z(pg, op) simde_svdup_n_s16_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svdup_s16_z(simde_svbool_t pg, int8_t op) {
  return simde_svdup_n_s16_z(pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s16_z
  #define svdup_s16_z(pg, op) simde_svdup_n_s16_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svdup_n_s16_m(simde_svint16_t inactive, simde_svbool_t pg, int16_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s16_m(inactive, pg, op);
  #else
    return simde_svsel_s16(pg, simde_svdup_n_s16(op), inactive);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s16_m
  #define svdup_n_s16_m(inactive, pg, op) simde_svdup_n_s16_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svdup_s16_m(simde_svint16_t inactive, simde_svbool_t pg, int16_t op) {
  return simde_svdup_n_s16_m(inactive, pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s16_m
  #define svdup_s16_m(inactive, pg, op) simde_svdup_n_s16_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svdup_n_s32(int32_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s32(op);
  #else
    simde_svint32_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vdupq_n_s32(op);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_set1_epi32(op);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_set1_epi32(op);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_set1_epi32(op);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_splats(op);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i32x4_splat(op);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op;
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s32
  #define svdup_n_s32(op) simde_svdup_n_s32((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svdup_s32(int8_t op) {
  return simde_svdup_n_s32(op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s32
  #define svdup_s32(op) simde_svdup_n_s32((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svdup_n_s32_z(simde_svbool_t pg, int32_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s32_z(pg, op);
  #else
    return simde_x_svsel_s32_z(pg, simde_svdup_n_s32(op));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s32_z
  #define svdup_n_s32_z(pg, op) simde_svdup_n_s32_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svdup_s32_z(simde_svbool_t pg, int32_t op) {
  return simde_svdup_n_s32_z(pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s32_z
  #define svdup_s32_z(pg, op) simde_svdup_n_s32_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svdup_n_s32_m(simde_svint32_t inactive, simde_svbool_t pg, int32_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s32_m(inactive, pg, op);
  #else
    return simde_svsel_s32(pg, simde_svdup_n_s32(op), inactive);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s32_m
  #define svdup_n_s32_m(inactive, pg, op) simde_svdup_n_s32_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svdup_s32_m(simde_svint32_t inactive, simde_svbool_t pg, int32_t op) {
  return simde_svdup_n_s32_m(inactive, pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s32_m
  #define svdup_s32_m(inactive, pg, op) simde_svdup_n_s32_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svdup_n_s64(int64_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s64(op);
  #else
    simde_svint64_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vdupq_n_s64(op);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_set1_epi64(op);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_set1_epi64x(op);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_set1_epi64x(op);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_splats(HEDLEY_STATIC_CAST(signed long long int, op));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i64x2_splat(op);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op;
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s64
  #define svdup_n_s64(op) simde_svdup_n_s64((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svdup_s64(int64_t op) {
  return simde_svdup_n_s64(op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s64
  #define svdup_s64(op) simde_svdup_n_s64((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svdup_n_s64_z(simde_svbool_t pg, int64_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s64_z(pg, op);
  #else
    return simde_x_svsel_s64_z(pg, simde_svdup_n_s64(op));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s64_z
  #define svdup_n_s64_z(pg, op) simde_svdup_n_s64_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svdup_s64_z(simde_svbool_t pg, int64_t op) {
  return simde_svdup_n_s64_z(pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s64_z
  #define svdup_s64_z(pg, op) simde_svdup_n_f64_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svdup_n_s64_m(simde_svint64_t inactive, simde_svbool_t pg, int64_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_s64_m(inactive, pg, op);
  #else
    return simde_svsel_s64(pg, simde_svdup_n_s64(op), inactive);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_s64_m
  #define svdup_n_s64_m(inactive, pg, op) simde_svdup_n_s64_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svdup_s64_m(simde_svint64_t inactive, simde_svbool_t pg, int64_t op) {
  return simde_svdup_n_s64_m(inactive, pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_s64_m
  #define svdup_s64_m(inactive, pg, op) simde_svdup_n_s64_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svdup_n_u8(uint8_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u8(op);
  #else
    simde_svuint8_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vdupq_n_u8(op);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_set1_epi8(HEDLEY_STATIC_CAST(int8_t, op));
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_set1_epi8(HEDLEY_STATIC_CAST(int8_t, op));
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_set1_epi8(HEDLEY_STATIC_CAST(int8_t, op));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_splats(op);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i8x16_splat(HEDLEY_STATIC_CAST(int8_t, op));
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op;
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u8
  #define svdup_n_u8(op) simde_svdup_n_u8((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svdup_u8(uint8_t op) {
  return simde_svdup_n_u8(op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u8
  #define svdup_u8(op) simde_svdup_n_u8((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svdup_n_u8_z(simde_svbool_t pg, uint8_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u8_z(pg, op);
  #else
    return simde_x_svsel_u8_z(pg, simde_svdup_n_u8(op));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u8_z
  #define svdup_n_u8_z(pg, op) simde_svdup_n_u8_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svdup_u8_z(simde_svbool_t pg, uint8_t op) {
  return simde_svdup_n_u8_z(pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u8_z
  #define svdup_u8_z(pg, op) simde_svdup_n_u8_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svdup_n_u8_m(simde_svuint8_t inactive, simde_svbool_t pg, uint8_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u8_m(inactive, pg, op);
  #else
    return simde_svsel_u8(pg, simde_svdup_n_u8(op), inactive);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u8_m
  #define svdup_n_u8_m(inactive, pg, op) simde_svdup_n_u8_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svdup_u8_m(simde_svuint8_t inactive, simde_svbool_t pg, uint8_t op) {
  return simde_svdup_n_u8_m(inactive, pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u8_m
  #define svdup_u8_m(inactive, pg, op) simde_svdup_n_u8_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svdup_n_u16(uint16_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u16(op);
  #else
    simde_svuint16_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vdupq_n_u16(op);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_set1_epi16(HEDLEY_STATIC_CAST(int16_t, op));
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_set1_epi16(HEDLEY_STATIC_CAST(int16_t, op));
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_set1_epi16(HEDLEY_STATIC_CAST(int16_t, op));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_splats(op);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i16x8_splat(HEDLEY_STATIC_CAST(int16_t, op));
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op;
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u16
  #define svdup_n_u16(op) simde_svdup_n_u16((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svdup_u16(uint16_t op) {
  return simde_svdup_n_u16(op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u16
  #define svdup_u16(op) simde_svdup_n_u16((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svdup_n_u16_z(simde_svbool_t pg, uint16_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u16_z(pg, op);
  #else
    return simde_x_svsel_u16_z(pg, simde_svdup_n_u16(op));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u16_z
  #define svdup_n_u16_z(pg, op) simde_svdup_n_u16_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svdup_u16_z(simde_svbool_t pg, uint8_t op) {
  return simde_svdup_n_u16_z(pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u16_z
  #define svdup_u16_z(pg, op) simde_svdup_n_u16_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svdup_n_u16_m(simde_svuint16_t inactive, simde_svbool_t pg, uint16_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u16_m(inactive, pg, op);
  #else
    return simde_svsel_u16(pg, simde_svdup_n_u16(op), inactive);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u16_m
  #define svdup_n_u16_m(inactive, pg, op) simde_svdup_n_u16_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svdup_u16_m(simde_svuint16_t inactive, simde_svbool_t pg, uint16_t op) {
  return simde_svdup_n_u16_m(inactive, pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u16_m
  #define svdup_u16_m(inactive, pg, op) simde_svdup_n_u16_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svdup_n_u32(uint32_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u32(op);
  #else
    simde_svuint32_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vdupq_n_u32(op);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_set1_epi32(HEDLEY_STATIC_CAST(int32_t, op));
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_set1_epi32(HEDLEY_STATIC_CAST(int32_t, op));
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_set1_epi32(HEDLEY_STATIC_CAST(int32_t, op));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_splats(op);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i32x4_splat(HEDLEY_STATIC_CAST(int32_t, op));
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op;
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u32
  #define svdup_n_u32(op) simde_svdup_n_u32((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svdup_u32(uint8_t op) {
  return simde_svdup_n_u32(op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u32
  #define svdup_u32(op) simde_svdup_n_u32((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svdup_n_u32_z(simde_svbool_t pg, uint32_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u32_z(pg, op);
  #else
    return simde_x_svsel_u32_z(pg, simde_svdup_n_u32(op));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u32_z
  #define svdup_n_u32_z(pg, op) simde_svdup_n_u32_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svdup_u32_z(simde_svbool_t pg, uint32_t op) {
  return simde_svdup_n_u32_z(pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u32_z
  #define svdup_u32_z(pg, op) simde_svdup_n_u32_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svdup_n_u32_m(simde_svuint32_t inactive, simde_svbool_t pg, uint32_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u32_m(inactive, pg, op);
  #else
    return simde_svsel_u32(pg, simde_svdup_n_u32(op), inactive);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u32_m
  #define svdup_n_u32_m(inactive, pg, op) simde_svdup_n_u32_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svdup_u32_m(simde_svuint32_t inactive, simde_svbool_t pg, uint32_t op) {
  return simde_svdup_n_u32_m(inactive, pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u32_m
  #define svdup_u32_m(inactive, pg, op) simde_svdup_n_u32_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svdup_n_u64(uint64_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u64(op);
  #else
    simde_svuint64_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vdupq_n_u64(op);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_set1_epi64(HEDLEY_STATIC_CAST(int64_t, op));
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_set1_epi64x(HEDLEY_STATIC_CAST(int64_t, op));
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_set1_epi64x(HEDLEY_STATIC_CAST(int64_t, op));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_splats(HEDLEY_STATIC_CAST(unsigned long long int, op));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i64x2_splat(HEDLEY_STATIC_CAST(int64_t, op));
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op;
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u64
  #define svdup_n_u64(op) simde_svdup_n_u64((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svdup_u64(uint64_t op) {
  return simde_svdup_n_u64(op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u64
  #define svdup_u64(op) simde_svdup_n_u64((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svdup_n_u64_z(simde_svbool_t pg, uint64_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u64_z(pg, op);
  #else
    return simde_x_svsel_u64_z(pg, simde_svdup_n_u64(op));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u64_z
  #define svdup_n_u64_z(pg, op) simde_svdup_n_u64_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svdup_u64_z(simde_svbool_t pg, uint64_t op) {
  return simde_svdup_n_u64_z(pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u64_z
  #define svdup_u64_z(pg, op) simde_svdup_n_f64_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svdup_n_u64_m(simde_svuint64_t inactive, simde_svbool_t pg, uint64_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_u64_m(inactive, pg, op);
  #else
    return simde_svsel_u64(pg, simde_svdup_n_u64(op), inactive);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_u64_m
  #define svdup_n_u64_m(inactive, pg, op) simde_svdup_n_u64_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svdup_u64_m(simde_svuint64_t inactive, simde_svbool_t pg, uint64_t op) {
  return simde_svdup_n_u64_m(inactive, pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_u64_m
  #define svdup_u64_m(inactive, pg, op) simde_svdup_n_u64_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svdup_n_f32(simde_float32 op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_f32(op);
  #else
    simde_svfloat32_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vdupq_n_f32(op);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512 = _mm512_set1_ps(op);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256) / sizeof(r.m256[0])) ; i++) {
        r.m256[i] = _mm256_set1_ps(op);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128) / sizeof(r.m128[0])) ; i++) {
        r.m128[i] = _mm_set1_ps(op);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_14_NATIVE)
      r.altivec = vec_splats(op);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_f32x4_splat(op);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op;
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_f32
  #define svdup_n_f32(op) simde_svdup_n_f32((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svdup_f32(int8_t op) {
  return simde_svdup_n_f32(op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_f32
  #define svdup_f32(op) simde_svdup_n_f32((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svdup_n_f32_z(simde_svbool_t pg, simde_float32 op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_f32_z(pg, op);
  #else
    return simde_x_svsel_f32_z(pg, simde_svdup_n_f32(op));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_f32_z
  #define svdup_n_f32_z(pg, op) simde_svdup_n_f32_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svdup_f32_z(simde_svbool_t pg, simde_float32 op) {
  return simde_svdup_n_f32_z(pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_f32_z
  #define svdup_f32_z(pg, op) simde_svdup_n_f32_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svdup_n_f32_m(simde_svfloat32_t inactive, simde_svbool_t pg, simde_float32_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_f32_m(inactive, pg, op);
  #else
    return simde_svsel_f32(pg, simde_svdup_n_f32(op), inactive);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_f32_m
  #define svdup_n_f32_m(inactive, pg, op) simde_svdup_n_f32_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svdup_f32_m(simde_svfloat32_t inactive, simde_svbool_t pg, simde_float32_t op) {
  return simde_svdup_n_f32_m(inactive, pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_f32_m
  #define svdup_f32_m(inactive, pg, op) simde_svdup_n_f32_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svdup_n_f64(simde_float64 op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_f64(op);
  #else
    simde_svfloat64_t r;

    #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      r.neon = vdupq_n_f64(op);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512d = _mm512_set1_pd(op);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256d) / sizeof(r.m256d[0])) ; i++) {
        r.m256d[i] = _mm256_set1_pd(op);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128d) / sizeof(r.m128d[0])) ; i++) {
        r.m128d[i] = _mm_set1_pd(op);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = vec_splats(op);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_f64x2_splat(op);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op;
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_f64
  #define svdup_n_f64(op) simde_svdup_n_f64((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svdup_f64(simde_float64 op) {
  return simde_svdup_n_f64(op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_f64
  #define svdup_f64(op) simde_svdup_n_f64((op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svdup_n_f64_z(simde_svbool_t pg, simde_float64 op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_f64_z(pg, op);
  #else
    return simde_x_svsel_f64_z(pg, simde_svdup_n_f64(op));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_f64_z
  #define svdup_n_f64_z(pg, op) simde_svdup_n_f64_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svdup_f64_z(simde_svbool_t pg, simde_float64 op) {
  return simde_svdup_n_f64_z(pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_f64_z
  #define svdup_f64_z(pg, op) simde_svdup_n_f64_z((pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svdup_n_f64_m(simde_svfloat64_t inactive, simde_svbool_t pg, simde_float64_t op) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svdup_n_f64_m(inactive, pg, op);
  #else
    return simde_svsel_f64(pg, simde_svdup_n_f64(op), inactive);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_n_f64_m
  #define svdup_n_f64_m(inactive, pg, op) simde_svdup_n_f64_m((inactive), (pg), (op))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svdup_f64_m(simde_svfloat64_t inactive, simde_svbool_t pg, simde_float64_t op) {
  return simde_svdup_n_f64_m(inactive, pg, op);
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svdup_f64_m
  #define svdup_f64_m(inactive, pg, op) simde_svdup_n_f64_m((inactive), (pg), (op))
#endif

#if defined(__cplusplus)
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svdup_n  (                          int8_t  op) { return simde_svdup_n_s8    (    op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svdup    (                          int8_t  op) { return simde_svdup_n_s8    (    op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svdup_n_z(simde_svbool_t pg,        int8_t  op) { return simde_svdup_n_s8_z  (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svdup_z  (simde_svbool_t pg,        int8_t  op) { return simde_svdup_n_s8_z  (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svdup_n  (                         int16_t  op) { return simde_svdup_n_s16   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svdup    (                         int16_t  op) { return simde_svdup_n_s16   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svdup_n_z(simde_svbool_t pg,       int16_t  op) { return simde_svdup_n_s16_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svdup_z  (simde_svbool_t pg,       int16_t  op) { return simde_svdup_n_s16_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svdup_n  (                         int32_t  op) { return simde_svdup_n_s32   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svdup    (                         int32_t  op) { return simde_svdup_n_s32   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svdup_n_z(simde_svbool_t pg,       int32_t  op) { return simde_svdup_n_s32_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svdup_z  (simde_svbool_t pg,       int32_t  op) { return simde_svdup_n_s32_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svdup_n  (                         int64_t  op) { return simde_svdup_n_s64   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svdup    (                         int64_t  op) { return simde_svdup_n_s64   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svdup_n_z(simde_svbool_t pg,       int64_t  op) { return simde_svdup_n_s64_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svdup_z  (simde_svbool_t pg,       int64_t  op) { return simde_svdup_n_s64_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svdup_n  (                         uint8_t  op) { return simde_svdup_n_u8    (    op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svdup    (                         uint8_t  op) { return simde_svdup_n_u8    (    op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svdup_n_z(simde_svbool_t pg,       uint8_t  op) { return simde_svdup_n_u8_z  (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svdup_z  (simde_svbool_t pg,       uint8_t  op) { return simde_svdup_n_u8_z  (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svdup_n  (                        uint16_t  op) { return simde_svdup_n_u16   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svdup    (                        uint16_t  op) { return simde_svdup_n_u16   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svdup_n_z(simde_svbool_t pg,      uint16_t  op) { return simde_svdup_n_u16_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svdup_z  (simde_svbool_t pg,      uint16_t  op) { return simde_svdup_n_u16_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svdup_n  (                        uint32_t  op) { return simde_svdup_n_u32   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svdup    (                        uint32_t  op) { return simde_svdup_n_u32   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svdup_n_z(simde_svbool_t pg,      uint32_t  op) { return simde_svdup_n_u32_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svdup_z  (simde_svbool_t pg,      uint32_t  op) { return simde_svdup_n_u32_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svdup_n  (                        uint64_t  op) { return simde_svdup_n_u64   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svdup    (                        uint64_t  op) { return simde_svdup_n_u64   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svdup_n_z(simde_svbool_t pg,      uint64_t  op) { return simde_svdup_n_u64_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svdup_z  (simde_svbool_t pg,      uint64_t  op) { return simde_svdup_n_u64_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svdup_n  (                   simde_float32  op) { return simde_svdup_n_f32   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svdup    (                   simde_float32  op) { return simde_svdup_n_f32   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svdup_n_z(simde_svbool_t pg, simde_float32  op) { return simde_svdup_n_f32_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svdup_z  (simde_svbool_t pg, simde_float32  op) { return simde_svdup_n_f32_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svdup_n  (                   simde_float64  op) { return simde_svdup_n_f64   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svdup    (                   simde_float64  op) { return simde_svdup_n_f64   (    op); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svdup_n_z(simde_svbool_t pg, simde_float64  op) { return simde_svdup_n_f64_z (pg, op); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svdup_z  (simde_svbool_t pg, simde_float64  op) { return simde_svdup_n_f64_z (pg, op); }

  #if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
    SIMDE_FUNCTION_ATTRIBUTES    svint8_t svdup_n  (                    int8_t  op) { return svdup_n_s8    (    op); }
    SIMDE_FUNCTION_ATTRIBUTES    svint8_t svdup    (                    int8_t  op) { return svdup_n_s8    (    op); }
    SIMDE_FUNCTION_ATTRIBUTES    svint8_t svdup_n_z(svbool_t pg,        int8_t  op) { return svdup_n_s8_z  (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES    svint8_t svdup_z  (svbool_t pg,        int8_t  op) { return svdup_n_s8_z  (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint16_t svdup_n  (                   int16_t  op) { return svdup_n_s16   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint16_t svdup    (                   int16_t  op) { return svdup_n_s16   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint16_t svdup_n_z(svbool_t pg,       int16_t  op) { return svdup_n_s16_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint16_t svdup_z  (svbool_t pg,       int16_t  op) { return svdup_n_s16_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint32_t svdup_n  (                   int32_t  op) { return svdup_n_s32   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint32_t svdup    (                   int32_t  op) { return svdup_n_s32   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint32_t svdup_n_z(svbool_t pg,       int32_t  op) { return svdup_n_s32_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint32_t svdup_z  (svbool_t pg,       int32_t  op) { return svdup_n_s32_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint64_t svdup_n  (                   int64_t  op) { return svdup_n_s64   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint64_t svdup    (                   int64_t  op) { return svdup_n_s64   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint64_t svdup_n_z(svbool_t pg,       int64_t  op) { return svdup_n_s64_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES   svint64_t svdup_z  (svbool_t pg,       int64_t  op) { return svdup_n_s64_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES   svuint8_t svdup_n  (                   uint8_t  op) { return svdup_n_u8    (    op); }
    SIMDE_FUNCTION_ATTRIBUTES   svuint8_t svdup    (                   uint8_t  op) { return svdup_n_u8    (    op); }
    SIMDE_FUNCTION_ATTRIBUTES   svuint8_t svdup_n_z(svbool_t pg,       uint8_t  op) { return svdup_n_u8_z  (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES   svuint8_t svdup_z  (svbool_t pg,       uint8_t  op) { return svdup_n_u8_z  (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint16_t svdup_n  (                  uint16_t  op) { return svdup_n_u16   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint16_t svdup    (                  uint16_t  op) { return svdup_n_u16   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint16_t svdup_n_z(svbool_t pg,      uint16_t  op) { return svdup_n_u16_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint16_t svdup_z  (svbool_t pg,      uint16_t  op) { return svdup_n_u16_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint32_t svdup_n  (                  uint32_t  op) { return svdup_n_u32   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint32_t svdup    (                  uint32_t  op) { return svdup_n_u32   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint32_t svdup_n_z(svbool_t pg,      uint32_t  op) { return svdup_n_u32_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint32_t svdup_z  (svbool_t pg,      uint32_t  op) { return svdup_n_u32_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint64_t svdup_n  (                  uint64_t  op) { return svdup_n_u64   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint64_t svdup    (                  uint64_t  op) { return svdup_n_u64   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint64_t svdup_n_z(svbool_t pg,      uint64_t  op) { return svdup_n_u64_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES  svuint64_t svdup_z  (svbool_t pg,      uint64_t  op) { return svdup_n_u64_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES svfloat32_t svdup_n  (             simde_float32  op) { return svdup_n_f32   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES svfloat32_t svdup    (             simde_float32  op) { return svdup_n_f32   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES svfloat32_t svdup_n_z(svbool_t pg, simde_float32  op) { return svdup_n_f32_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES svfloat32_t svdup_z  (svbool_t pg, simde_float32  op) { return svdup_n_f32_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES svfloat64_t svdup_n  (             simde_float64  op) { return svdup_n_f64   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES svfloat64_t svdup    (             simde_float64  op) { return svdup_n_f64   (    op); }
    SIMDE_FUNCTION_ATTRIBUTES svfloat64_t svdup_n_z(svbool_t pg, simde_float64  op) { return svdup_n_f64_z (pg, op); }
    SIMDE_FUNCTION_ATTRIBUTES svfloat64_t svdup_z  (svbool_t pg, simde_float64  op) { return svdup_n_f64_z (pg, op); }
  #endif
#elif defined(SIMDE_GENERIC_)
  #define simde_svdup_n(op) \
    (SIMDE_GENERIC_((op), \
         int8_t: simde_svdup_n_s8, \
        int16_t: simde_svdup_n_s16, \
        int32_t: simde_svdup_n_s32, \
        int64_t: simde_svdup_n_s64, \
        uint8_t: simde_svdup_n_u8, \
       uint16_t: simde_svdup_n_u16, \
       uint32_t: simde_svdup_n_u32, \
       uint64_t: simde_svdup_n_u64, \
      float32_t: simde_svdup_n_f32, \
      float64_t: simde_svdup_n_f64)((op)))
  #define simde_svdup(op) simde_svdup_n((op))

  #define simde_svdup_n_z(pg, op) \
    (SIMDE_GENERIC_((op), \
         int8_t: simde_svdup_n_s8_z, \
        int16_t: simde_svdup_n_s16_z, \
        int32_t: simde_svdup_n_s32_z, \
        int64_t: simde_svdup_n_s64_z, \
        uint8_t: simde_svdup_n_s8_z, \
       uint16_t: simde_svdup_n_u16_z, \
       uint32_t: simde_svdup_n_u32_z, \
       uint64_t: simde_svdup_n_u64_z, \
      float32_t: simde_svdup_n_u32_z, \
      float64_t: simde_svdup_n_f64_z)((pg), (op)))
  #define simde_svdup_z(pg, op) simde_svdup_n_z((pg), (op))
#endif
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef svdup
  #undef svdup_z
  #undef svdup_n
  #undef svdup_n_z
  #define svdup_n(op) simde_svdup_n((op))
  #define svdup_n_z(pg, op) simde_svdup_n_z((pg), (op))
  #define svdup(op) simde_svdup((op))
  #define svdup_z(pg, op) simde_svdup_z((pg), (op))
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_DUP_H */
/* :: End simde/arm/sve/dup.h :: */

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svadd_s8_x(simde_svbool_t pg, simde_svint8_t op1, simde_svint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_s8_x(pg, op1, op2);
  #else
    simde_svint8_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vaddq_s8(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_add_epi8(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_add_epi8(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_add_epi8(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_add_epi8(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_add(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec + op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i8x16_add(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values + op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] + op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_s8_x
  #define svadd_s8_x(pg, op1, op2) simde_svadd_s8_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svadd_s8_z(simde_svbool_t pg, simde_svint8_t op1, simde_svint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_s8_z(pg, op1, op2);
  #else
    return simde_x_svsel_s8_z(pg, simde_svadd_s8_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_s8_z
  #define svadd_s8_z(pg, op1, op2) simde_svadd_s8_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svadd_s8_m(simde_svbool_t pg, simde_svint8_t op1, simde_svint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_s8_m(pg, op1, op2);
  #else
    return simde_svsel_s8(pg, simde_svadd_s8_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_s8_m
  #define svadd_s8_m(pg, op1, op2) simde_svadd_s8_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svadd_n_s8_x(simde_svbool_t pg, simde_svint8_t op1, int8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_s8_x(pg, op1, op2);
  #else
    return simde_svadd_s8_x(pg, op1, simde_svdup_n_s8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_s8_x
  #define svadd_n_s8_x(pg, op1, op2) simde_svadd_n_s8_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svadd_n_s8_z(simde_svbool_t pg, simde_svint8_t op1, int8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_s8_z(pg, op1, op2);
  #else
    return simde_svadd_s8_z(pg, op1, simde_svdup_n_s8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_s8_z
  #define svadd_n_s8_z(pg, op1, op2) simde_svadd_n_s8_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svadd_n_s8_m(simde_svbool_t pg, simde_svint8_t op1, int8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_s8_m(pg, op1, op2);
  #else
    return simde_svadd_s8_m(pg, op1, simde_svdup_n_s8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_s8_m
  #define svadd_n_s8_m(pg, op1, op2) simde_svadd_n_s8_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svadd_s16_x(simde_svbool_t pg, simde_svint16_t op1, simde_svint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_s16_x(pg, op1, op2);
  #else
    simde_svint16_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vaddq_s16(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_add_epi16(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_add_epi16(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_add_epi16(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_add_epi16(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_add(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec + op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i16x8_add(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values + op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] + op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_s16_x
  #define svadd_s16_x(pg, op1, op2) simde_svadd_s16_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svadd_s16_z(simde_svbool_t pg, simde_svint16_t op1, simde_svint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_s16_z(pg, op1, op2);
  #else
    return simde_x_svsel_s16_z(pg, simde_svadd_s16_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_s16_z
  #define svadd_s16_z(pg, op1, op2) simde_svadd_s16_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svadd_s16_m(simde_svbool_t pg, simde_svint16_t op1, simde_svint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_s16_m(pg, op1, op2);
  #else
    return simde_svsel_s16(pg, simde_svadd_s16_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_s16_m
  #define svadd_s16_m(pg, op1, op2) simde_svadd_s16_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svadd_n_s16_x(simde_svbool_t pg, simde_svint16_t op1, int16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_s16_x(pg, op1, op2);
  #else
    return simde_svadd_s16_x(pg, op1, simde_svdup_n_s16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_s16_x
  #define svadd_n_s16_x(pg, op1, op2) simde_svadd_n_s16_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svadd_n_s16_z(simde_svbool_t pg, simde_svint16_t op1, int16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_s16_z(pg, op1, op2);
  #else
    return simde_svadd_s16_z(pg, op1, simde_svdup_n_s16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_s16_z
  #define svadd_n_s16_z(pg, op1, op2) simde_svadd_n_s16_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svadd_n_s16_m(simde_svbool_t pg, simde_svint16_t op1, int16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_s16_m(pg, op1, op2);
  #else
    return simde_svadd_s16_m(pg, op1, simde_svdup_n_s16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_s16_m
  #define svadd_n_s16_m(pg, op1, op2) simde_svadd_n_s16_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svadd_s32_x(simde_svbool_t pg, simde_svint32_t op1, simde_svint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_s32_x(pg, op1, op2);
  #else
    simde_svint32_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vaddq_s32(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_add_epi32(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_add_epi32(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_add_epi32(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_add_epi32(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_add(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec + op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i32x4_add(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values + op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] + op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_s32_x
  #define svadd_s32_x(pg, op1, op2) simde_svadd_s32_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svadd_s32_z(simde_svbool_t pg, simde_svint32_t op1, simde_svint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_s32_z(pg, op1, op2);
  #else
    return simde_x_svsel_s32_z(pg, simde_svadd_s32_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_s32_z
  #define svadd_s32_z(pg, op1, op2) simde_svadd_s32_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svadd_s32_m(simde_svbool_t pg, simde_svint32_t op1, simde_svint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_s32_m(pg, op1, op2);
  #else
    return simde_svsel_s32(pg, simde_svadd_s32_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_s32_m
  #define svadd_s32_m(pg, op1, op2) simde_svadd_s32_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svadd_n_s32_x(simde_svbool_t pg, simde_svint32_t op1, int32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_s32_x(pg, op1, op2);
  #else
    return simde_svadd_s32_x(pg, op1, simde_svdup_n_s32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_s32_x
  #define svadd_n_s32_x(pg, op1, op2) simde_svadd_n_s32_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svadd_n_s32_z(simde_svbool_t pg, simde_svint32_t op1, int32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_s32_z(pg, op1, op2);
  #else
    return simde_svadd_s32_z(pg, op1, simde_svdup_n_s32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_s32_z
  #define svadd_n_s32_z(pg, op1, op2) simde_svadd_n_s32_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svadd_n_s32_m(simde_svbool_t pg, simde_svint32_t op1, int32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_s32_m(pg, op1, op2);
  #else
    return simde_svadd_s32_m(pg, op1, simde_svdup_n_s32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_s32_m
  #define svadd_n_s32_m(pg, op1, op2) simde_svadd_n_s32_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svadd_s64_x(simde_svbool_t pg, simde_svint64_t op1, simde_svint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_s64_x(pg, op1, op2);
  #else
    simde_svint64_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vaddq_s64(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_add_epi64(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_add_epi64(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_add_epi64(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_add_epi64(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
      r.altivec = vec_add(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec + op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i64x2_add(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values + op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] + op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_s64_x
  #define svadd_s64_x(pg, op1, op2) simde_svadd_s64_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svadd_s64_z(simde_svbool_t pg, simde_svint64_t op1, simde_svint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_s64_z(pg, op1, op2);
  #else
    return simde_x_svsel_s64_z(pg, simde_svadd_s64_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_s64_z
  #define svadd_s64_z(pg, op1, op2) simde_svadd_s64_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svadd_s64_m(simde_svbool_t pg, simde_svint64_t op1, simde_svint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_s64_m(pg, op1, op2);
  #else
    return simde_svsel_s64(pg, simde_svadd_s64_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_s64_m
  #define svadd_s64_m(pg, op1, op2) simde_svadd_s64_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svadd_n_s64_x(simde_svbool_t pg, simde_svint64_t op1, int64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_s64_x(pg, op1, op2);
  #else
    return simde_svadd_s64_x(pg, op1, simde_svdup_n_s64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_s64_x
  #define svadd_n_s64_x(pg, op1, op2) simde_svadd_n_s64_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svadd_n_s64_z(simde_svbool_t pg, simde_svint64_t op1, int64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_s64_z(pg, op1, op2);
  #else
    return simde_svadd_s64_z(pg, op1, simde_svdup_n_s64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_s64_z
  #define svadd_n_s64_z(pg, op1, op2) simde_svadd_n_s64_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svadd_n_s64_m(simde_svbool_t pg, simde_svint64_t op1, int64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_s64_m(pg, op1, op2);
  #else
    return simde_svadd_s64_m(pg, op1, simde_svdup_n_s64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_s64_m
  #define svadd_n_s64_m(pg, op1, op2) simde_svadd_n_s64_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svadd_u8_x(simde_svbool_t pg, simde_svuint8_t op1, simde_svuint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_u8_x(pg, op1, op2);
  #else
    simde_svuint8_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vaddq_u8(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_add_epi8(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_add_epi8(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_add_epi8(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_add_epi8(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_add(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec + op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i8x16_add(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values + op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] + op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_u8_x
  #define svadd_u8_x(pg, op1, op2) simde_svadd_u8_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svadd_u8_z(simde_svbool_t pg, simde_svuint8_t op1, simde_svuint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_u8_z(pg, op1, op2);
  #else
    return simde_x_svsel_u8_z(pg, simde_svadd_u8_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_u8_z
  #define svadd_u8_z(pg, op1, op2) simde_svadd_u8_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svadd_u8_m(simde_svbool_t pg, simde_svuint8_t op1, simde_svuint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_u8_m(pg, op1, op2);
  #else
    return simde_svsel_u8(pg, simde_svadd_u8_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_u8_m
  #define svadd_u8_m(pg, op1, op2) simde_svadd_u8_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svadd_n_u8_x(simde_svbool_t pg, simde_svuint8_t op1, uint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_u8_x(pg, op1, op2);
  #else
    return simde_svadd_u8_x(pg, op1, simde_svdup_n_u8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_u8_x
  #define svadd_n_u8_x(pg, op1, op2) simde_svadd_n_u8_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svadd_n_u8_z(simde_svbool_t pg, simde_svuint8_t op1, uint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_u8_z(pg, op1, op2);
  #else
    return simde_svadd_u8_z(pg, op1, simde_svdup_n_u8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_u8_z
  #define svadd_n_u8_z(pg, op1, op2) simde_svadd_n_u8_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svadd_n_u8_m(simde_svbool_t pg, simde_svuint8_t op1, uint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_u8_m(pg, op1, op2);
  #else
    return simde_svadd_u8_m(pg, op1, simde_svdup_n_u8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_u8_m
  #define svadd_n_u8_m(pg, op1, op2) simde_svadd_n_u8_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svadd_u16_x(simde_svbool_t pg, simde_svuint16_t op1, simde_svuint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_u16_x(pg, op1, op2);
  #else
    simde_svuint16_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vaddq_u16(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_add_epi16(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_add_epi16(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_add_epi16(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_add_epi16(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_add(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec + op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i16x8_add(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values + op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] + op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_u16_x
  #define svadd_u16_x(pg, op1, op2) simde_svadd_u16_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svadd_u16_z(simde_svbool_t pg, simde_svuint16_t op1, simde_svuint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_u16_z(pg, op1, op2);
  #else
    return simde_x_svsel_u16_z(pg, simde_svadd_u16_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_u16_z
  #define svadd_u16_z(pg, op1, op2) simde_svadd_u16_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svadd_u16_m(simde_svbool_t pg, simde_svuint16_t op1, simde_svuint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_u16_m(pg, op1, op2);
  #else
    return simde_svsel_u16(pg, simde_svadd_u16_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_u16_m
  #define svadd_u16_m(pg, op1, op2) simde_svadd_u16_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svadd_n_u16_x(simde_svbool_t pg, simde_svuint16_t op1, uint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_u16_x(pg, op1, op2);
  #else
    return simde_svadd_u16_x(pg, op1, simde_svdup_n_u16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_u16_x
  #define svadd_n_u16_x(pg, op1, op2) simde_svadd_n_u16_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svadd_n_u16_z(simde_svbool_t pg, simde_svuint16_t op1, uint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_u16_z(pg, op1, op2);
  #else
    return simde_svadd_u16_z(pg, op1, simde_svdup_n_u16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_u16_z
  #define svadd_n_u16_z(pg, op1, op2) simde_svadd_n_u16_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svadd_n_u16_m(simde_svbool_t pg, simde_svuint16_t op1, uint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_u16_m(pg, op1, op2);
  #else
    return simde_svadd_u16_m(pg, op1, simde_svdup_n_u16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_u16_m
  #define svadd_n_u16_m(pg, op1, op2) simde_svadd_n_u16_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svadd_u32_x(simde_svbool_t pg, simde_svuint32_t op1, simde_svuint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_u32_x(pg, op1, op2);
  #else
    simde_svuint32_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vaddq_u32(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_add_epi32(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_add_epi32(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_add_epi32(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_add_epi32(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_add(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec + op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i32x4_add(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values + op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] + op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_u32_x
  #define svadd_u32_x(pg, op1, op2) simde_svadd_u32_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svadd_u32_z(simde_svbool_t pg, simde_svuint32_t op1, simde_svuint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_u32_z(pg, op1, op2);
  #else
    return simde_x_svsel_u32_z(pg, simde_svadd_u32_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_u32_z
  #define svadd_u32_z(pg, op1, op2) simde_svadd_u32_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svadd_u32_m(simde_svbool_t pg, simde_svuint32_t op1, simde_svuint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_u32_m(pg, op1, op2);
  #else
    return simde_svsel_u32(pg, simde_svadd_u32_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_u32_m
  #define svadd_u32_m(pg, op1, op2) simde_svadd_u32_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svadd_n_u32_x(simde_svbool_t pg, simde_svuint32_t op1, uint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_u32_x(pg, op1, op2);
  #else
    return simde_svadd_u32_x(pg, op1, simde_svdup_n_u32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_u32_x
  #define svadd_n_u32_x(pg, op1, op2) simde_svadd_n_u32_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svadd_n_u32_z(simde_svbool_t pg, simde_svuint32_t op1, uint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_u32_z(pg, op1, op2);
  #else
    return simde_svadd_u32_z(pg, op1, simde_svdup_n_u32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_u32_z
  #define svadd_n_u32_z(pg, op1, op2) simde_svadd_n_u32_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svadd_n_u32_m(simde_svbool_t pg, simde_svuint32_t op1, uint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_u32_m(pg, op1, op2);
  #else
    return simde_svadd_u32_m(pg, op1, simde_svdup_n_u32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_u32_m
  #define svadd_n_u32_m(pg, op1, op2) simde_svadd_n_u32_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svadd_u64_x(simde_svbool_t pg, simde_svuint64_t op1, simde_svuint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_u64_x(pg, op1, op2);
  #else
    simde_svuint64_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vaddq_u64(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_add_epi64(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_add_epi64(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_add_epi64(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_add_epi64(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
      r.altivec = vec_add(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec + op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i64x2_add(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values + op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] + op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_u64_x
  #define svadd_u64_x(pg, op1, op2) simde_svadd_u64_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svadd_u64_z(simde_svbool_t pg, simde_svuint64_t op1, simde_svuint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_u64_z(pg, op1, op2);
  #else
    return simde_x_svsel_u64_z(pg, simde_svadd_u64_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_u64_z
  #define svadd_u64_z(pg, op1, op2) simde_svadd_u64_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svadd_u64_m(simde_svbool_t pg, simde_svuint64_t op1, simde_svuint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_u64_m(pg, op1, op2);
  #else
    return simde_svsel_u64(pg, simde_svadd_u64_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_u64_m
  #define svadd_u64_m(pg, op1, op2) simde_svadd_u64_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svadd_n_u64_x(simde_svbool_t pg, simde_svuint64_t op1, uint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_u64_x(pg, op1, op2);
  #else
    return simde_svadd_u64_x(pg, op1, simde_svdup_n_u64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_u64_x
  #define svadd_n_u64_x(pg, op1, op2) simde_svadd_n_u64_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svadd_n_u64_z(simde_svbool_t pg, simde_svuint64_t op1, uint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_u64_z(pg, op1, op2);
  #else
    return simde_svadd_u64_z(pg, op1, simde_svdup_n_u64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_u64_z
  #define svadd_n_u64_z(pg, op1, op2) simde_svadd_n_u64_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svadd_n_u64_m(simde_svbool_t pg, simde_svuint64_t op1, uint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_u64_m(pg, op1, op2);
  #else
    return simde_svadd_u64_m(pg, op1, simde_svdup_n_u64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_u64_m
  #define svadd_n_u64_m(pg, op1, op2) simde_svadd_n_u64_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svadd_f32_x(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_f32_x(pg, op1, op2);
  #else
    simde_svfloat32_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vaddq_f32(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512 = _mm512_add_ps(op1.m512, op2.m512);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256[0] = _mm256_add_ps(op1.m256[0], op2.m256[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256) / sizeof(r.m256[0])) ; i++) {
        r.m256[i] = _mm256_add_ps(op1.m256[i], op2.m256[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128) / sizeof(r.m128[0])) ; i++) {
        r.m128[i] = _mm_add_ps(op1.m128[i], op2.m128[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_add(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec + op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_f32x4_add(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values + op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] + op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_f32_x
  #define svadd_f32_x(pg, op1, op2) simde_svadd_f32_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svadd_f32_z(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_f32_z(pg, op1, op2);
  #else
    return simde_x_svsel_f32_z(pg, simde_svadd_f32_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_f32_z
  #define svadd_f32_z(pg, op1, op2) simde_svadd_f32_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svadd_f32_m(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_f32_m(pg, op1, op2);
  #else
    return simde_svsel_f32(pg, simde_svadd_f32_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_f32_m
  #define svadd_f32_m(pg, op1, op2) simde_svadd_f32_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svadd_n_f32_x(simde_svbool_t pg, simde_svfloat32_t op1, simde_float32 op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_f32_x(pg, op1, op2);
  #else
    return simde_svadd_f32_x(pg, op1, simde_svdup_n_f32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_f32_x
  #define svadd_n_f32_x(pg, op1, op2) simde_svadd_n_f32_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svadd_n_f32_z(simde_svbool_t pg, simde_svfloat32_t op1, simde_float32 op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_f32_z(pg, op1, op2);
  #else
    return simde_svadd_f32_z(pg, op1, simde_svdup_n_f32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_f32_z
  #define svadd_n_f32_z(pg, op1, op2) simde_svadd_n_f32_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svadd_n_f32_m(simde_svbool_t pg, simde_svfloat32_t op1, simde_float32 op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_f32_m(pg, op1, op2);
  #else
    return simde_svadd_f32_m(pg, op1, simde_svdup_n_f32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_f32_m
  #define svadd_n_f32_m(pg, op1, op2) simde_svadd_n_f32_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svadd_f64_x(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_f64_x(pg, op1, op2);
  #else
    simde_svfloat64_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      r.neon = vaddq_f64(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512d = _mm512_add_pd(op1.m512d, op2.m512d);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256d[0] = _mm256_add_pd(op1.m256d[0], op2.m256d[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256d) / sizeof(r.m256d[0])) ; i++) {
        r.m256d[i] = _mm256_add_pd(op1.m256d[i], op2.m256d[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128d) / sizeof(r.m128d[0])) ; i++) {
        r.m128d[i] = _mm_add_pd(op1.m128d[i], op2.m128d[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
      r.altivec = vec_add(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec + op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_f64x2_add(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values + op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] + op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_f64_x
  #define svadd_f64_x(pg, op1, op2) simde_svadd_f64_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svadd_f64_z(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_f64_z(pg, op1, op2);
  #else
    return simde_x_svsel_f64_z(pg, simde_svadd_f64_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_f64_z
  #define svadd_f64_z(pg, op1, op2) simde_svadd_f64_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svadd_f64_m(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_f64_m(pg, op1, op2);
  #else
    return simde_svsel_f64(pg, simde_svadd_f64_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_f64_m
  #define svadd_f64_m(pg, op1, op2) simde_svadd_f64_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svadd_n_f64_x(simde_svbool_t pg, simde_svfloat64_t op1, simde_float64 op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_f64_x(pg, op1, op2);
  #else
    return simde_svadd_f64_x(pg, op1, simde_svdup_n_f64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_f64_x
  #define svadd_n_f64_x(pg, op1, op2) simde_svadd_n_f64_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svadd_n_f64_z(simde_svbool_t pg, simde_svfloat64_t op1, simde_float64 op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_f64_z(pg, op1, op2);
  #else
    return simde_svadd_f64_z(pg, op1, simde_svdup_n_f64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_f64_z
  #define svadd_n_f64_z(pg, op1, op2) simde_svadd_n_f64_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svadd_n_f64_m(simde_svbool_t pg, simde_svfloat64_t op1, simde_float64 op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svadd_n_f64_m(pg, op1, op2);
  #else
    return simde_svadd_f64_m(pg, op1, simde_svdup_n_f64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svadd_n_f64_m
  #define svadd_n_f64_m(pg, op1, op2) simde_svadd_n_f64_m(pg, op1, op2)
#endif

#if defined(__cplusplus)
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svadd_x(simde_svbool_t pg,    simde_svint8_t op1,    simde_svint8_t op2) { return simde_svadd_s8_x   (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svadd_x(simde_svbool_t pg,   simde_svint16_t op1,   simde_svint16_t op2) { return simde_svadd_s16_x  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svadd_x(simde_svbool_t pg,   simde_svint32_t op1,   simde_svint32_t op2) { return simde_svadd_s32_x  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svadd_x(simde_svbool_t pg,   simde_svint64_t op1,   simde_svint64_t op2) { return simde_svadd_s64_x  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svadd_x(simde_svbool_t pg,   simde_svuint8_t op1,   simde_svuint8_t op2) { return simde_svadd_u8_x   (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svadd_x(simde_svbool_t pg,  simde_svuint16_t op1,  simde_svuint16_t op2) { return simde_svadd_u16_x  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svadd_x(simde_svbool_t pg,  simde_svuint32_t op1,  simde_svuint32_t op2) { return simde_svadd_u32_x  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svadd_x(simde_svbool_t pg,  simde_svuint64_t op1,  simde_svuint64_t op2) { return simde_svadd_u64_x  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svadd_x(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) { return simde_svadd_f32_x  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svadd_x(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) { return simde_svadd_f64_x  (pg, op1, op2); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svadd_z(simde_svbool_t pg,    simde_svint8_t op1,    simde_svint8_t op2) { return simde_svadd_s8_z   (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svadd_z(simde_svbool_t pg,   simde_svint16_t op1,   simde_svint16_t op2) { return simde_svadd_s16_z  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svadd_z(simde_svbool_t pg,   simde_svint32_t op1,   simde_svint32_t op2) { return simde_svadd_s32_z  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svadd_z(simde_svbool_t pg,   simde_svint64_t op1,   simde_svint64_t op2) { return simde_svadd_s64_z  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svadd_z(simde_svbool_t pg,   simde_svuint8_t op1,   simde_svuint8_t op2) { return simde_svadd_u8_z   (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svadd_z(simde_svbool_t pg,  simde_svuint16_t op1,  simde_svuint16_t op2) { return simde_svadd_u16_z  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svadd_z(simde_svbool_t pg,  simde_svuint32_t op1,  simde_svuint32_t op2) { return simde_svadd_u32_z  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svadd_z(simde_svbool_t pg,  simde_svuint64_t op1,  simde_svuint64_t op2) { return simde_svadd_u64_z  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svadd_z(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) { return simde_svadd_f32_z  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svadd_z(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) { return simde_svadd_f64_z  (pg, op1, op2); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svadd_m(simde_svbool_t pg,    simde_svint8_t op1,    simde_svint8_t op2) { return simde_svadd_s8_m   (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svadd_m(simde_svbool_t pg,   simde_svint16_t op1,   simde_svint16_t op2) { return simde_svadd_s16_m  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svadd_m(simde_svbool_t pg,   simde_svint32_t op1,   simde_svint32_t op2) { return simde_svadd_s32_m  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svadd_m(simde_svbool_t pg,   simde_svint64_t op1,   simde_svint64_t op2) { return simde_svadd_s64_m  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svadd_m(simde_svbool_t pg,   simde_svuint8_t op1,   simde_svuint8_t op2) { return simde_svadd_u8_m   (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svadd_m(simde_svbool_t pg,  simde_svuint16_t op1,  simde_svuint16_t op2) { return simde_svadd_u16_m  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svadd_m(simde_svbool_t pg,  simde_svuint32_t op1,  simde_svuint32_t op2) { return simde_svadd_u32_m  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svadd_m(simde_svbool_t pg,  simde_svuint64_t op1,  simde_svuint64_t op2) { return simde_svadd_u64_m  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svadd_m(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) { return simde_svadd_f32_m  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svadd_m(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) { return simde_svadd_f64_m  (pg, op1, op2); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svadd_x(simde_svbool_t pg,    simde_svint8_t op1,            int8_t op2) { return simde_svadd_n_s8_x (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svadd_x(simde_svbool_t pg,   simde_svint16_t op1,           int16_t op2) { return simde_svadd_n_s16_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svadd_x(simde_svbool_t pg,   simde_svint32_t op1,           int32_t op2) { return simde_svadd_n_s32_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svadd_x(simde_svbool_t pg,   simde_svint64_t op1,           int64_t op2) { return simde_svadd_n_s64_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svadd_x(simde_svbool_t pg,   simde_svuint8_t op1,           uint8_t op2) { return simde_svadd_n_u8_x (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svadd_x(simde_svbool_t pg,  simde_svuint16_t op1,          uint16_t op2) { return simde_svadd_n_u16_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svadd_x(simde_svbool_t pg,  simde_svuint32_t op1,          uint32_t op2) { return simde_svadd_n_u32_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svadd_x(simde_svbool_t pg,  simde_svuint64_t op1,          uint64_t op2) { return simde_svadd_n_u64_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svadd_x(simde_svbool_t pg, simde_svfloat32_t op1,     simde_float32 op2) { return simde_svadd_n_f32_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svadd_x(simde_svbool_t pg, simde_svfloat64_t op1,     simde_float64 op2) { return simde_svadd_n_f64_x(pg, op1, op2); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svadd_z(simde_svbool_t pg,    simde_svint8_t op1,            int8_t op2) { return simde_svadd_n_s8_z (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svadd_z(simde_svbool_t pg,   simde_svint16_t op1,           int16_t op2) { return simde_svadd_n_s16_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svadd_z(simde_svbool_t pg,   simde_svint32_t op1,           int32_t op2) { return simde_svadd_n_s32_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svadd_z(simde_svbool_t pg,   simde_svint64_t op1,           int64_t op2) { return simde_svadd_n_s64_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svadd_z(simde_svbool_t pg,   simde_svuint8_t op1,           uint8_t op2) { return simde_svadd_n_u8_z (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svadd_z(simde_svbool_t pg,  simde_svuint16_t op1,          uint16_t op2) { return simde_svadd_n_u16_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svadd_z(simde_svbool_t pg,  simde_svuint32_t op1,          uint32_t op2) { return simde_svadd_n_u32_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svadd_z(simde_svbool_t pg,  simde_svuint64_t op1,          uint64_t op2) { return simde_svadd_n_u64_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svadd_z(simde_svbool_t pg, simde_svfloat32_t op1,     simde_float32 op2) { return simde_svadd_n_f32_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svadd_z(simde_svbool_t pg, simde_svfloat64_t op1,     simde_float64 op2) { return simde_svadd_n_f64_z(pg, op1, op2); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svadd_m(simde_svbool_t pg,    simde_svint8_t op1,            int8_t op2) { return simde_svadd_n_s8_m (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svadd_m(simde_svbool_t pg,   simde_svint16_t op1,           int16_t op2) { return simde_svadd_n_s16_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svadd_m(simde_svbool_t pg,   simde_svint32_t op1,           int32_t op2) { return simde_svadd_n_s32_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svadd_m(simde_svbool_t pg,   simde_svint64_t op1,           int64_t op2) { return simde_svadd_n_s64_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svadd_m(simde_svbool_t pg,   simde_svuint8_t op1,           uint8_t op2) { return simde_svadd_n_u8_m (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svadd_m(simde_svbool_t pg,  simde_svuint16_t op1,          uint16_t op2) { return simde_svadd_n_u16_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svadd_m(simde_svbool_t pg,  simde_svuint32_t op1,          uint32_t op2) { return simde_svadd_n_u32_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svadd_m(simde_svbool_t pg,  simde_svuint64_t op1,          uint64_t op2) { return simde_svadd_n_u64_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svadd_m(simde_svbool_t pg, simde_svfloat32_t op1,     simde_float32 op2) { return simde_svadd_n_f32_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svadd_m(simde_svbool_t pg, simde_svfloat64_t op1,     simde_float64 op2) { return simde_svadd_n_f64_m(pg, op1, op2); }
#elif defined(SIMDE_GENERIC_)
  #define simde_svadd_x(pg, op1, op2) \
    (SIMDE_GENERIC_((op2), \
         simde_svint8_t: simde_svadd_s8_x, \
        simde_svint16_t: simde_svadd_s16_x, \
        simde_svint32_t: simde_svadd_s32_x, \
        simde_svint64_t: simde_svadd_s64_x, \
        simde_svuint8_t: simde_svadd_u8_x, \
       simde_svuint16_t: simde_svadd_u16_x, \
       simde_svuint32_t: simde_svadd_u32_x, \
       simde_svuint64_t: simde_svadd_u64_x, \
      simde_svfloat32_t: simde_svadd_f32_x, \
      simde_svfloat64_t: simde_svadd_f64_x, \
                 int8_t: simde_svadd_n_s8_x, \
                int16_t: simde_svadd_n_s16_x, \
                int32_t: simde_svadd_n_s32_x, \
                int64_t: simde_svadd_n_s64_x, \
                uint8_t: simde_svadd_n_u8_x, \
               uint16_t: simde_svadd_n_u16_x, \
               uint32_t: simde_svadd_n_u32_x, \
               uint64_t: simde_svadd_n_u64_x, \
          simde_float32: simde_svadd_n_f32_x, \
          simde_float64: simde_svadd_n_f64_x)((pg), (op1), (op2)))

  #define simde_svadd_z(pg, op1, op2) \
    (SIMDE_GENERIC_((op2), \
         simde_svint8_t: simde_svadd_s8_z, \
        simde_svint16_t: simde_svadd_s16_z, \
        simde_svint32_t: simde_svadd_s32_z, \
        simde_svint64_t: simde_svadd_s64_z, \
        simde_svuint8_t: simde_svadd_u8_z, \
       simde_svuint16_t: simde_svadd_u16_z, \
       simde_svuint32_t: simde_svadd_u32_z, \
       simde_svuint64_t: simde_svadd_u64_z, \
      simde_svfloat32_t: simde_svadd_f32_z, \
      simde_svfloat64_t: simde_svadd_f64_z, \
                 int8_t: simde_svadd_n_s8_z, \
                int16_t: simde_svadd_n_s16_z, \
                int32_t: simde_svadd_n_s32_z, \
                int64_t: simde_svadd_n_s64_z, \
                uint8_t: simde_svadd_n_u8_z, \
               uint16_t: simde_svadd_n_u16_z, \
               uint32_t: simde_svadd_n_u32_z, \
               uint64_t: simde_svadd_n_u64_z, \
          simde_float32: simde_svadd_n_f32_z, \
          simde_float64: simde_svadd_n_f64_z)((pg), (op1), (op2)))

  #define simde_svadd_m(pg, op1, op2) \
    (SIMDE_GENERIC_((op2), \
         simde_svint8_t: simde_svadd_s8_m, \
        simde_svint16_t: simde_svadd_s16_m, \
        simde_svint32_t: simde_svadd_s32_m, \
        simde_svint64_t: simde_svadd_s64_m, \
        simde_svuint8_t: simde_svadd_u8_m, \
       simde_svuint16_t: simde_svadd_u16_m, \
       simde_svuint32_t: simde_svadd_u32_m, \
       simde_svuint64_t: simde_svadd_u64_m, \
      simde_svfloat32_t: simde_svadd_f32_m, \
      simde_svfloat64_t: simde_svadd_f64_m, \
                 int8_t: simde_svadd_n_s8_m, \
                int16_t: simde_svadd_n_s16_m, \
                int32_t: simde_svadd_n_s32_m, \
                int64_t: simde_svadd_n_s64_m, \
                uint8_t: simde_svadd_n_u8_m, \
               uint16_t: simde_svadd_n_u16_m, \
               uint32_t: simde_svadd_n_u32_m, \
               uint64_t: simde_svadd_n_u64_m, \
          simde_float32: simde_svadd_n_f32_m, \
          simde_float64: simde_svadd_n_f64_m)((pg), (op1), (op2)))
#endif
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef svadd_x
  #undef svadd_z
  #undef svadd_m
  #undef svadd_n_x
  #undef svadd_n_z
  #undef svadd_n_m
  #define svadd_x(pg, op1, op2) simde_svadd_x((pg), (op1), (op2))
  #define svadd_z(pg, op1, op2) simde_svadd_z((pg), (op1), (op2))
  #define svadd_m(pg, op1, op2) simde_svadd_m((pg), (op1), (op2))
  #define svadd_n_x(pg, op1, op2) simde_svadd_n_x((pg), (op1), (op2))
  #define svadd_n_z(pg, op1, op2) simde_svadd_n_z((pg), (op1), (op2))
  #define svadd_n_m(pg, op1, op2) simde_svadd_n_m((pg), (op1), (op2))
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_ADD_H */
/* :: End simde/arm/sve/add.h :: */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/arm/sve/and.h :: */
/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_ARM_SVE_AND_H)
#define SIMDE_ARM_SVE_AND_H

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svand_s8_x(simde_svbool_t pg, simde_svint8_t op1, simde_svint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_s8_x(pg, op1, op2);
  #else
    simde_svint8_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vandq_s8(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_and_si512(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_and_si256(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_and_si256(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_and_si128(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_and(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec & op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_and(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values & op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] & op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_s8_x
  #define svand_s8_x(pg, op1, op2) simde_svand_s8_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svand_s8_z(simde_svbool_t pg, simde_svint8_t op1, simde_svint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_s8_z(pg, op1, op2);
  #else
    return simde_x_svsel_s8_z(pg, simde_svand_s8_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_s8_z
  #define svand_s8_z(pg, op1, op2) simde_svand_s8_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svand_s8_m(simde_svbool_t pg, simde_svint8_t op1, simde_svint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_s8_m(pg, op1, op2);
  #else
    return simde_svsel_s8(pg, simde_svand_s8_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_s8_m
  #define svand_s8_m(pg, op1, op2) simde_svand_s8_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svand_n_s8_z(simde_svbool_t pg, simde_svint8_t op1, int8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_s8_z(pg, op1, op2);
  #else
    return simde_svand_s8_z(pg, op1, simde_svdup_n_s8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_s8_z
  #define svand_n_s8_z(pg, op1, op2) simde_svand_n_s8_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svand_n_s8_m(simde_svbool_t pg, simde_svint8_t op1, int8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_s8_m(pg, op1, op2);
  #else
    return simde_svand_s8_m(pg, op1, simde_svdup_n_s8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_s8_m
  #define svand_n_s8_m(pg, op1, op2) simde_svand_n_s8_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svand_n_s8_x(simde_svbool_t pg, simde_svint8_t op1, int8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_s8_x(pg, op1, op2);
  #else
    return simde_svand_s8_x(pg, op1, simde_svdup_n_s8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_s8_x
  #define svand_n_s8_x(pg, op1, op2) simde_svand_n_s8_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svand_s16_x(simde_svbool_t pg, simde_svint16_t op1, simde_svint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_s16_x(pg, op1, op2);
  #else
    simde_svint16_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vandq_s16(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_and_si512(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_and_si256(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_and_si256(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_and_si128(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_and(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec & op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_and(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values & op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] & op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_s16_x
  #define svand_s16_x(pg, op1, op2) simde_svand_s16_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svand_s16_z(simde_svbool_t pg, simde_svint16_t op1, simde_svint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_s16_z(pg, op1, op2);
  #else
    return simde_x_svsel_s16_z(pg, simde_svand_s16_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_s16_z
  #define svand_s16_z(pg, op1, op2) simde_svand_s16_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svand_s16_m(simde_svbool_t pg, simde_svint16_t op1, simde_svint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_s16_m(pg, op1, op2);
  #else
    return simde_svsel_s16(pg, simde_svand_s16_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_s16_m
  #define svand_s16_m(pg, op1, op2) simde_svand_s16_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svand_n_s16_z(simde_svbool_t pg, simde_svint16_t op1, int16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_s16_z(pg, op1, op2);
  #else
    return simde_svand_s16_z(pg, op1, simde_svdup_n_s16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_s16_z
  #define svand_n_s16_z(pg, op1, op2) simde_svand_n_s16_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svand_n_s16_m(simde_svbool_t pg, simde_svint16_t op1, int16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_s16_m(pg, op1, op2);
  #else
    return simde_svand_s16_m(pg, op1, simde_svdup_n_s16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_s16_m
  #define svand_n_s16_m(pg, op1, op2) simde_svand_n_s16_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svand_n_s16_x(simde_svbool_t pg, simde_svint16_t op1, int16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_s16_x(pg, op1, op2);
  #else
    return simde_svand_s16_x(pg, op1, simde_svdup_n_s16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_s16_x
  #define svand_n_s16_x(pg, op1, op2) simde_svand_n_s16_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svand_s32_x(simde_svbool_t pg, simde_svint32_t op1, simde_svint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_s32_x(pg, op1, op2);
  #else
    simde_svint32_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vandq_s32(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_and_si512(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_and_si256(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_and_si256(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_and_si128(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_and(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec & op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_and(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values & op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] & op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_s32_x
  #define svand_s32_x(pg, op1, op2) simde_svand_s32_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svand_s32_z(simde_svbool_t pg, simde_svint32_t op1, simde_svint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_s32_z(pg, op1, op2);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && ((SIMDE_ARM_SVE_VECTOR_SIZE >= 512) || defined(SIMDE_X86_AVX512VL_NATIVE)) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    simde_svint32_t r;

    #if SIMDE_ARM_SVE_VECTOR_SIZE >= 512
      r.m512i = _mm512_maskz_and_epi32(simde_svbool_to_mmask16(pg), op1.m512i, op2.m512i);
    #else
      r.m256i[0] = _mm256_maskz_and_epi32(simde_svbool_to_mmask8(pg), op1.m256i[0], op2.m256i[0]);
    #endif

    return r;
  #else
    return simde_x_svsel_s32_z(pg, simde_svand_s32_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_s32_z
  #define svand_s32_z(pg, op1, op2) simde_svand_s32_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svand_s32_m(simde_svbool_t pg, simde_svint32_t op1, simde_svint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_s32_m(pg, op1, op2);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && ((SIMDE_ARM_SVE_VECTOR_SIZE >= 512) || defined(SIMDE_X86_AVX512VL_NATIVE)) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    simde_svint32_t r;

    #if SIMDE_ARM_SVE_VECTOR_SIZE >= 512
      r.m512i = _mm512_mask_and_epi32(op1.m512i, simde_svbool_to_mmask16(pg), op1.m512i, op2.m512i);
    #else
      r.m256i[0] = _mm256_mask_and_epi32(op1.m256i[0], simde_svbool_to_mmask8(pg), op1.m256i[0], op2.m256i[0]);
    #endif

    return r;
  #else
    return simde_svsel_s32(pg, simde_svand_s32_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_s32_m
  #define svand_s32_m(pg, op1, op2) simde_svand_s32_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svand_n_s32_z(simde_svbool_t pg, simde_svint32_t op1, int32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_s32_z(pg, op1, op2);
  #else
    return simde_svand_s32_z(pg, op1, simde_svdup_n_s32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_s32_z
  #define svand_n_s32_z(pg, op1, op2) simde_svand_n_s32_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svand_n_s32_m(simde_svbool_t pg, simde_svint32_t op1, int32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_s32_m(pg, op1, op2);
  #else
    return simde_svand_s32_m(pg, op1, simde_svdup_n_s32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_s32_m
  #define svand_n_s32_m(pg, op1, op2) simde_svand_n_s32_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svand_n_s32_x(simde_svbool_t pg, simde_svint32_t op1, int32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_s32_x(pg, op1, op2);
  #else
    return simde_svand_s32_x(pg, op1, simde_svdup_n_s32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_s32_x
  #define svand_n_s32_x(pg, op1, op2) simde_svand_n_s32_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svand_s64_x(simde_svbool_t pg, simde_svint64_t op1, simde_svint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_s64_x(pg, op1, op2);
  #else
    simde_svint64_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vandq_s64(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_and_si512(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_and_si256(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_and_si256(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_and_si128(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
      r.altivec = vec_and(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec & op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_and(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values & op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] & op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_s64_x
  #define svand_s64_x(pg, op1, op2) simde_svand_s64_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svand_s64_z(simde_svbool_t pg, simde_svint64_t op1, simde_svint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_s64_z(pg, op1, op2);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && ((SIMDE_ARM_SVE_VECTOR_SIZE >= 512) || defined(SIMDE_X86_AVX512VL_NATIVE)) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    simde_svint64_t r;

    #if SIMDE_ARM_SVE_VECTOR_SIZE >= 512
      r.m512i = _mm512_maskz_and_epi64(simde_svbool_to_mmask8(pg), op1.m512i, op2.m512i);
    #else
      r.m256i[0] = _mm256_maskz_and_epi64(simde_svbool_to_mmask4(pg), op1.m256i[0], op2.m256i[0]);
    #endif

    return r;
  #else
    return simde_x_svsel_s64_z(pg, simde_svand_s64_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_s64_z
  #define svand_s64_z(pg, op1, op2) simde_svand_s64_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svand_s64_m(simde_svbool_t pg, simde_svint64_t op1, simde_svint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_s64_m(pg, op1, op2);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && ((SIMDE_ARM_SVE_VECTOR_SIZE >= 512) || defined(SIMDE_X86_AVX512VL_NATIVE)) \
      && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    simde_svint64_t r;

    #if SIMDE_ARM_SVE_VECTOR_SIZE >= 512
      r.m512i = _mm512_mask_and_epi64(op1.m512i, simde_svbool_to_mmask8(pg), op1.m512i, op2.m512i);
    #else
      r.m256i[0] = _mm256_mask_and_epi64(op1.m256i[0], simde_svbool_to_mmask4(pg), op1.m256i[0], op2.m256i[0]);
    #endif

    return r;
  #else
    return simde_svsel_s64(pg, simde_svand_s64_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_s64_m
  #define svand_s64_m(pg, op1, op2) simde_svand_s64_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svand_n_s64_z(simde_svbool_t pg, simde_svint64_t op1, int64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_s64_z(pg, op1, op2);
  #else
    return simde_svand_s64_z(pg, op1, simde_svdup_n_s64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_s64_z
  #define svand_n_s64_z(pg, op1, op2) simde_svand_n_s64_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svand_n_s64_m(simde_svbool_t pg, simde_svint64_t op1, int64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_s64_m(pg, op1, op2);
  #else
    return simde_svand_s64_m(pg, op1, simde_svdup_n_s64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_s64_m
  #define svand_n_s64_m(pg, op1, op2) simde_svand_n_s64_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svand_n_s64_x(simde_svbool_t pg, simde_svint64_t op1, int64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_s64_x(pg, op1, op2);
  #else
    return simde_svand_s64_x(pg, op1, simde_svdup_n_s64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_s64_x
  #define svand_n_s64_x(pg, op1, op2) simde_svand_n_s64_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svand_u8_z(simde_svbool_t pg, simde_svuint8_t op1, simde_svuint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_u8_z(pg, op1, op2);
  #else
    return simde_svreinterpret_u8_s8(simde_svand_s8_z(pg, simde_svreinterpret_s8_u8(op1), simde_svreinterpret_s8_u8(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_u8_z
  #define svand_u8_z(pg, op1, op2) simde_svand_u8_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svand_u8_m(simde_svbool_t pg, simde_svuint8_t op1, simde_svuint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_u8_m(pg, op1, op2);
  #else
    return simde_svreinterpret_u8_s8(simde_svand_s8_m(pg, simde_svreinterpret_s8_u8(op1), simde_svreinterpret_s8_u8(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_u8_m
  #define svand_u8_m(pg, op1, op2) simde_svand_u8_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svand_u8_x(simde_svbool_t pg, simde_svuint8_t op1, simde_svuint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_u8_x(pg, op1, op2);
  #else
    return simde_svreinterpret_u8_s8(simde_svand_s8_x(pg, simde_svreinterpret_s8_u8(op1), simde_svreinterpret_s8_u8(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_u8_x
  #define svand_u8_x(pg, op1, op2) simde_svand_u8_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svand_n_u8_z(simde_svbool_t pg, simde_svuint8_t op1, uint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_u8_z(pg, op1, op2);
  #else
    return simde_svand_u8_z(pg, op1, simde_svdup_n_u8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_u8_z
  #define svand_n_u8_z(pg, op1, op2) simde_svand_n_u8_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svand_n_u8_m(simde_svbool_t pg, simde_svuint8_t op1, uint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_u8_m(pg, op1, op2);
  #else
    return simde_svand_u8_m(pg, op1, simde_svdup_n_u8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_u8_m
  #define svand_n_u8_m(pg, op1, op2) simde_svand_n_u8_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svand_n_u8_x(simde_svbool_t pg, simde_svuint8_t op1, uint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_u8_x(pg, op1, op2);
  #else
    return simde_svand_u8_x(pg, op1, simde_svdup_n_u8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_u8_x
  #define svand_n_u8_x(pg, op1, op2) simde_svand_n_u8_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svand_u16_z(simde_svbool_t pg, simde_svuint16_t op1, simde_svuint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_u16_z(pg, op1, op2);
  #else
    return simde_svreinterpret_u16_s16(simde_svand_s16_z(pg, simde_svreinterpret_s16_u16(op1), simde_svreinterpret_s16_u16(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_u16_z
  #define svand_u16_z(pg, op1, op2) simde_svand_u16_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svand_u16_m(simde_svbool_t pg, simde_svuint16_t op1, simde_svuint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_u16_m(pg, op1, op2);
  #else
    return simde_svreinterpret_u16_s16(simde_svand_s16_m(pg, simde_svreinterpret_s16_u16(op1), simde_svreinterpret_s16_u16(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_u16_m
  #define svand_u16_m(pg, op1, op2) simde_svand_u16_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svand_u16_x(simde_svbool_t pg, simde_svuint16_t op1, simde_svuint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_u16_x(pg, op1, op2);
  #else
    return simde_svreinterpret_u16_s16(simde_svand_s16_x(pg, simde_svreinterpret_s16_u16(op1), simde_svreinterpret_s16_u16(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_u16_x
  #define svand_u16_x(pg, op1, op2) simde_svand_u16_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svand_n_u16_z(simde_svbool_t pg, simde_svuint16_t op1, uint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_u16_z(pg, op1, op2);
  #else
    return simde_svand_u16_z(pg, op1, simde_svdup_n_u16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_u16_z
  #define svand_n_u16_z(pg, op1, op2) simde_svand_n_u16_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svand_n_u16_m(simde_svbool_t pg, simde_svuint16_t op1, uint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_u16_m(pg, op1, op2);
  #else
    return simde_svand_u16_m(pg, op1, simde_svdup_n_u16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_u16_m
  #define svand_n_u16_m(pg, op1, op2) simde_svand_n_u16_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svand_n_u16_x(simde_svbool_t pg, simde_svuint16_t op1, uint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_u16_x(pg, op1, op2);
  #else
    return simde_svand_u16_x(pg, op1, simde_svdup_n_u16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_u16_x
  #define svand_n_u16_x(pg, op1, op2) simde_svand_n_u16_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svand_u32_z(simde_svbool_t pg, simde_svuint32_t op1, simde_svuint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_u32_z(pg, op1, op2);
  #else
    return simde_svreinterpret_u32_s32(simde_svand_s32_z(pg, simde_svreinterpret_s32_u32(op1), simde_svreinterpret_s32_u32(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_u32_z
  #define svand_u32_z(pg, op1, op2) simde_svand_u32_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svand_u32_m(simde_svbool_t pg, simde_svuint32_t op1, simde_svuint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_u32_m(pg, op1, op2);
  #else
    return simde_svreinterpret_u32_s32(simde_svand_s32_m(pg, simde_svreinterpret_s32_u32(op1), simde_svreinterpret_s32_u32(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_u32_m
  #define svand_u32_m(pg, op1, op2) simde_svand_u32_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svand_u32_x(simde_svbool_t pg, simde_svuint32_t op1, simde_svuint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_u32_x(pg, op1, op2);
  #else
    return simde_svreinterpret_u32_s32(simde_svand_s32_x(pg, simde_svreinterpret_s32_u32(op1), simde_svreinterpret_s32_u32(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_u32_x
  #define svand_u32_x(pg, op1, op2) simde_svand_u32_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svand_n_u32_z(simde_svbool_t pg, simde_svuint32_t op1, uint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_u32_z(pg, op1, op2);
  #else
    return simde_svand_u32_z(pg, op1, simde_svdup_n_u32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_u32_z
  #define svand_n_u32_z(pg, op1, op2) simde_svand_n_u32_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svand_n_u32_m(simde_svbool_t pg, simde_svuint32_t op1, uint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_u32_m(pg, op1, op2);
  #else
    return simde_svand_u32_m(pg, op1, simde_svdup_n_u32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_u32_m
  #define svand_n_u32_m(pg, op1, op2) simde_svand_n_u32_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svand_n_u32_x(simde_svbool_t pg, simde_svuint32_t op1, uint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_u32_x(pg, op1, op2);
  #else
    return simde_svand_u32_x(pg, op1, simde_svdup_n_u32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_u32_x
  #define svand_n_u32_x(pg, op1, op2) simde_svand_n_u32_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svand_u64_z(simde_svbool_t pg, simde_svuint64_t op1, simde_svuint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_u64_z(pg, op1, op2);
  #else
    return simde_svreinterpret_u64_s64(simde_svand_s64_z(pg, simde_svreinterpret_s64_u64(op1), simde_svreinterpret_s64_u64(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_u64_z
  #define svand_u64_z(pg, op1, op2) simde_svand_u64_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svand_u64_m(simde_svbool_t pg, simde_svuint64_t op1, simde_svuint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_u64_m(pg, op1, op2);
  #else
    return simde_svreinterpret_u64_s64(simde_svand_s64_m(pg, simde_svreinterpret_s64_u64(op1), simde_svreinterpret_s64_u64(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_u64_m
  #define svand_u64_m(pg, op1, op2) simde_svand_u64_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svand_u64_x(simde_svbool_t pg, simde_svuint64_t op1, simde_svuint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_u64_x(pg, op1, op2);
  #else
    return simde_svreinterpret_u64_s64(simde_svand_s64_x(pg, simde_svreinterpret_s64_u64(op1), simde_svreinterpret_s64_u64(op2)));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_u64_x
  #define svand_u64_x(pg, op1, op2) simde_svand_u64_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svand_n_u64_z(simde_svbool_t pg, simde_svuint64_t op1, uint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_u64_z(pg, op1, op2);
  #else
    return simde_svand_u64_z(pg, op1, simde_svdup_n_u64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_u64_z
  #define svand_n_u64_x(pg, op1, op2) simde_svand_n_u64_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svand_n_u64_m(simde_svbool_t pg, simde_svuint64_t op1, uint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_u64_m(pg, op1, op2);
  #else
    return simde_svand_u64_m(pg, op1, simde_svdup_n_u64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_u64_m
  #define svand_n_u64_x(pg, op1, op2) simde_svand_n_u64_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svand_n_u64_x(simde_svbool_t pg, simde_svuint64_t op1, uint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svand_n_u64_x(pg, op1, op2);
  #else
    return simde_svand_u64_x(pg, op1, simde_svdup_n_u64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svand_n_u64_x
  #define svand_n_u64_x(pg, op1, op2) simde_svand_n_u64_x(pg, op1, op2)
#endif

#if defined(__cplusplus)
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svand_z(simde_svbool_t pg,    simde_svint8_t op1,    simde_svint8_t op2) { return simde_svand_s8_z (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svand_z(simde_svbool_t pg,   simde_svint16_t op1,   simde_svint16_t op2) { return simde_svand_s16_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svand_z(simde_svbool_t pg,   simde_svint32_t op1,   simde_svint32_t op2) { return simde_svand_s32_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svand_z(simde_svbool_t pg,   simde_svint64_t op1,   simde_svint64_t op2) { return simde_svand_s64_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svand_z(simde_svbool_t pg,   simde_svuint8_t op1,   simde_svuint8_t op2) { return simde_svand_u8_z (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svand_z(simde_svbool_t pg,  simde_svuint16_t op1,  simde_svuint16_t op2) { return simde_svand_u16_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svand_z(simde_svbool_t pg,  simde_svuint32_t op1,  simde_svuint32_t op2) { return simde_svand_u32_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svand_z(simde_svbool_t pg,  simde_svuint64_t op1,  simde_svuint64_t op2) { return simde_svand_u64_z(pg, op1, op2); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svand_m(simde_svbool_t pg,    simde_svint8_t op1,    simde_svint8_t op2) { return simde_svand_s8_m (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svand_m(simde_svbool_t pg,   simde_svint16_t op1,   simde_svint16_t op2) { return simde_svand_s16_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svand_m(simde_svbool_t pg,   simde_svint32_t op1,   simde_svint32_t op2) { return simde_svand_s32_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svand_m(simde_svbool_t pg,   simde_svint64_t op1,   simde_svint64_t op2) { return simde_svand_s64_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svand_m(simde_svbool_t pg,   simde_svuint8_t op1,   simde_svuint8_t op2) { return simde_svand_u8_m (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svand_m(simde_svbool_t pg,  simde_svuint16_t op1,  simde_svuint16_t op2) { return simde_svand_u16_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svand_m(simde_svbool_t pg,  simde_svuint32_t op1,  simde_svuint32_t op2) { return simde_svand_u32_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svand_m(simde_svbool_t pg,  simde_svuint64_t op1,  simde_svuint64_t op2) { return simde_svand_u64_m(pg, op1, op2); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svand_x(simde_svbool_t pg,    simde_svint8_t op1,    simde_svint8_t op2) { return simde_svand_s8_x (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svand_x(simde_svbool_t pg,   simde_svint16_t op1,   simde_svint16_t op2) { return simde_svand_s16_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svand_x(simde_svbool_t pg,   simde_svint32_t op1,   simde_svint32_t op2) { return simde_svand_s32_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svand_x(simde_svbool_t pg,   simde_svint64_t op1,   simde_svint64_t op2) { return simde_svand_s64_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svand_x(simde_svbool_t pg,   simde_svuint8_t op1,   simde_svuint8_t op2) { return simde_svand_u8_x (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svand_x(simde_svbool_t pg,  simde_svuint16_t op1,  simde_svuint16_t op2) { return simde_svand_u16_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svand_x(simde_svbool_t pg,  simde_svuint32_t op1,  simde_svuint32_t op2) { return simde_svand_u32_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svand_x(simde_svbool_t pg,  simde_svuint64_t op1,  simde_svuint64_t op2) { return simde_svand_u64_x(pg, op1, op2); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svand_z(simde_svbool_t pg,    simde_svint8_t op1,    int8_t op2) { return  simde_svand_n_s8_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svand_z(simde_svbool_t pg,   simde_svint16_t op1,   int16_t op2) { return simde_svand_n_s16_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svand_z(simde_svbool_t pg,   simde_svint32_t op1,   int32_t op2) { return simde_svand_n_s32_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svand_z(simde_svbool_t pg,   simde_svint64_t op1,   int64_t op2) { return simde_svand_n_s64_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svand_z(simde_svbool_t pg,   simde_svuint8_t op1,   uint8_t op2) { return  simde_svand_n_u8_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svand_z(simde_svbool_t pg,  simde_svuint16_t op1,  uint16_t op2) { return simde_svand_n_u16_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svand_z(simde_svbool_t pg,  simde_svuint32_t op1,  uint32_t op2) { return simde_svand_n_u32_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svand_z(simde_svbool_t pg,  simde_svuint64_t op1,  uint64_t op2) { return simde_svand_n_u64_z(pg, op1, op2); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svand_m(simde_svbool_t pg,    simde_svint8_t op1,    int8_t op2) { return  simde_svand_n_s8_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svand_m(simde_svbool_t pg,   simde_svint16_t op1,   int16_t op2) { return simde_svand_n_s16_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svand_m(simde_svbool_t pg,   simde_svint32_t op1,   int32_t op2) { return simde_svand_n_s32_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svand_m(simde_svbool_t pg,   simde_svint64_t op1,   int64_t op2) { return simde_svand_n_s64_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svand_m(simde_svbool_t pg,   simde_svuint8_t op1,   uint8_t op2) { return  simde_svand_n_u8_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svand_m(simde_svbool_t pg,  simde_svuint16_t op1,  uint16_t op2) { return simde_svand_n_u16_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svand_m(simde_svbool_t pg,  simde_svuint32_t op1,  uint32_t op2) { return simde_svand_n_u32_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svand_m(simde_svbool_t pg,  simde_svuint64_t op1,  uint64_t op2) { return simde_svand_n_u64_m(pg, op1, op2); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svand_x(simde_svbool_t pg,    simde_svint8_t op1,    int8_t op2) { return  simde_svand_n_s8_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svand_x(simde_svbool_t pg,   simde_svint16_t op1,   int16_t op2) { return simde_svand_n_s16_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svand_x(simde_svbool_t pg,   simde_svint32_t op1,   int32_t op2) { return simde_svand_n_s32_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svand_x(simde_svbool_t pg,   simde_svint64_t op1,   int64_t op2) { return simde_svand_n_s64_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svand_x(simde_svbool_t pg,   simde_svuint8_t op1,   uint8_t op2) { return  simde_svand_n_u8_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svand_x(simde_svbool_t pg,  simde_svuint16_t op1,  uint16_t op2) { return simde_svand_n_u16_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svand_x(simde_svbool_t pg,  simde_svuint32_t op1,  uint32_t op2) { return simde_svand_n_u32_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svand_x(simde_svbool_t pg,  simde_svuint64_t op1,  uint64_t op2) { return simde_svand_n_u64_x(pg, op1, op2); }
#elif defined(SIMDE_GENERIC_)
  #define simde_svand_z(pg, op1, op2) \
    (SIMDE_GENERIC_((op2), \
         simde_svint8_t: simde_svand_s8_z, \
        simde_svint16_t: simde_svand_s16_z, \
        simde_svint32_t: simde_svand_s32_z, \
        simde_svint64_t: simde_svand_s64_z, \
        simde_svuint8_t: simde_svand_u8_z, \
       simde_svuint16_t: simde_svand_u16_z, \
       simde_svuint32_t: simde_svand_u32_z, \
       simde_svuint64_t: simde_svand_u64_z, \
                 int8_t: simde_svand_n_s8_z, \
                int16_t: simde_svand_n_s16_z, \
                int32_t: simde_svand_n_s32_z, \
                int64_t: simde_svand_n_s64_z, \
                uint8_t: simde_svand_n_u8_z, \
               uint16_t: simde_svand_n_u16_z, \
               uint32_t: simde_svand_n_u32_z, \
               uint64_t: simde_svand_n_u64_z)((pg), (op1), (op2)))

  #define simde_svand_m(pg, op1, op2) \
    (SIMDE_GENERIC_((op2), \
         simde_svint8_t: simde_svand_s8_m, \
        simde_svint16_t: simde_svand_s16_m, \
        simde_svint32_t: simde_svand_s32_m, \
        simde_svint64_t: simde_svand_s64_m, \
        simde_svuint8_t: simde_svand_u8_m, \
       simde_svuint16_t: simde_svand_u16_m, \
       simde_svuint32_t: simde_svand_u32_m, \
       simde_svuint64_t: simde_svand_u64_m, \
                 int8_t: simde_svand_n_s8_m, \
                int16_t: simde_svand_n_s16_m, \
                int32_t: simde_svand_n_s32_m, \
                int64_t: simde_svand_n_s64_m, \
                uint8_t: simde_svand_n_u8_m, \
               uint16_t: simde_svand_n_u16_m, \
               uint32_t: simde_svand_n_u32_m, \
               uint64_t: simde_svand_n_u64_m)((pg), (op1), (op2)))

  #define simde_svand_x(pg, op1, op2) \
    (SIMDE_GENERIC_((op2), \
         simde_svint8_t: simde_svand_s8_x, \
        simde_svint16_t: simde_svand_s16_x, \
        simde_svint32_t: simde_svand_s32_x, \
        simde_svint64_t: simde_svand_s64_x, \
        simde_svuint8_t: simde_svand_u8_x, \
       simde_svuint16_t: simde_svand_u16_x, \
       simde_svuint32_t: simde_svand_u32_x, \
       simde_svuint64_t: simde_svand_u64_x, \
                 int8_t: simde_svand_n_s8_x, \
                int16_t: simde_svand_n_s16_x, \
                int32_t: simde_svand_n_s32_x, \
                int64_t: simde_svand_n_s64_x, \
                uint8_t: simde_svand_n_u8_x, \
               uint16_t: simde_svand_n_u16_x, \
               uint32_t: simde_svand_n_u32_x, \
               uint64_t: simde_svand_n_u64_x)((pg), (op1), (op2)))
#endif
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef svand_x
  #undef svand_z
  #undef svand_m
  #define svand_x(pg, op1, op2) simde_svand_x((pg), (op1), (op2))
  #define svand_z(pg, op1, op2) simde_svand_z((pg), (op1), (op2))
  #define svand_m(pg, op1, op2) simde_svand_m((pg), (op1), (op2))
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_AND_H */
/* :: End simde/arm/sve/and.h :: */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/arm/sve/cmplt.h :: */
/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_ARM_SVE_CMPLT_H)
#define SIMDE_ARM_SVE_CMPLT_H

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svcmplt_s8(simde_svbool_t pg, simde_svint8_t op1, simde_svint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svcmplt_s8(pg, op1, op2);
  #else
    simde_svbool_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r = simde_svbool_from_mmask64(_mm512_mask_cmplt_epi8_mask(simde_svbool_to_mmask64(pg), op1.m512i, op2.m512i));
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r = simde_svbool_from_mmask32(_mm256_mask_cmplt_epi8_mask(simde_svbool_to_mmask32(pg), op1.m256i[0], op2.m256i[0]));
    #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon_i8 = vandq_s8(pg.neon_i8, vreinterpretq_s8_u8(vcltq_s8(op1.neon, op2.neon)));
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_and_si128(pg.m128i[i], _mm_cmplt_epi8(op1.m128i[i], op2.m128i[i]));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec_b8 = vec_and(pg.altivec_b8, vec_cmplt(op1.altivec, op2.altivec));
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec_b8 = pg.altivec_b8 & vec_cmplt(op1.altivec, op2.altivec);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_and(pg.v128, wasm_i8x16_lt(op1.v128, op2.v128));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values_i8 = pg.values_i8 & HEDLEY_REINTERPRET_CAST(__typeof__(r.values_i8), op1.values < op2.values);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values_i8) / sizeof(r.values_i8[0])) ; i++) {
        r.values_i8[i] = pg.values_i8[i] & ((op1.values[i] < op2.values[i]) ? ~INT8_C(0) : INT8_C(0));
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svcmplt_s8
  #define svcmplt_s8(pg, op1, op2) simde_svcmplt_s8(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svcmplt_s16(simde_svbool_t pg, simde_svint16_t op1, simde_svint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svcmplt_s16(pg, op1, op2);
  #else
    simde_svbool_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r = simde_svbool_from_mmask32(_mm512_mask_cmplt_epi16_mask(simde_svbool_to_mmask32(pg), op1.m512i, op2.m512i));
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r = simde_svbool_from_mmask16(_mm256_mask_cmplt_epi16_mask(simde_svbool_to_mmask16(pg), op1.m256i[0], op2.m256i[0]));
    #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon_i16 = vandq_s16(pg.neon_i16, vreinterpretq_s16_u16(vcltq_s16(op1.neon, op2.neon)));
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_and_si128(pg.m128i[i], _mm_cmplt_epi16(op1.m128i[i], op2.m128i[i]));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec_b16 = vec_and(pg.altivec_b16, vec_cmplt(op1.altivec, op2.altivec));
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec_b16 = pg.altivec_b16 & vec_cmplt(op1.altivec, op2.altivec);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_and(pg.v128, wasm_i16x8_lt(op1.v128, op2.v128));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values_i16 = pg.values_i16 & HEDLEY_REINTERPRET_CAST(__typeof__(r.values_i16), op1.values < op2.values);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values_i16) / sizeof(r.values_i16[0])) ; i++) {
        r.values_i16[i] = pg.values_i16[i] & ((op1.values[i] < op2.values[i]) ? ~INT16_C(0) : INT16_C(0));
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svcmplt_s16
  #define svcmplt_s16(pg, op1, op2) simde_svcmplt_s16(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svcmplt_s32(simde_svbool_t pg, simde_svint32_t op1, simde_svint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svcmplt_s32(pg, op1, op2);
  #else
    simde_svbool_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r = simde_svbool_from_mmask16(_mm512_mask_cmplt_epi32_mask(simde_svbool_to_mmask16(pg), op1.m512i, op2.m512i));
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r = simde_svbool_from_mmask8(_mm256_mask_cmplt_epi32_mask(simde_svbool_to_mmask8(pg), op1.m256i[0], op2.m256i[0]));
    #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon_i32 = vandq_s32(pg.neon_i32, vreinterpretq_s32_u32(vcltq_s32(op1.neon, op2.neon)));
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_and_si128(pg.m128i[i], _mm_cmplt_epi32(op1.m128i[i], op2.m128i[i]));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec_b32 = vec_and(pg.altivec_b32, vec_cmplt(op1.altivec, op2.altivec));
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec_b32 = pg.altivec_b32 & vec_cmplt(op1.altivec, op2.altivec);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_and(pg.v128, wasm_i32x4_lt(op1.v128, op2.v128));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values_i32 = pg.values_i32 & HEDLEY_REINTERPRET_CAST(__typeof__(r.values_i32), op1.values < op2.values);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values_i32) / sizeof(r.values_i32[0])) ; i++) {
        r.values_i32[i] = pg.values_i32[i] & ((op1.values[i] < op2.values[i]) ? ~INT32_C(0) : INT32_C(0));
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svcmplt_s32
  #define svcmplt_s32(pg, op1, op2) simde_svcmplt_s32(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svcmplt_s64(simde_svbool_t pg, simde_svint64_t op1, simde_svint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svcmplt_s64(pg, op1, op2);
  #else
    simde_svbool_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r = simde_svbool_from_mmask8(_mm512_mask_cmplt_epi64_mask(simde_svbool_to_mmask8(pg), op1.m512i, op2.m512i));
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r = simde_svbool_from_mmask4(_mm256_mask_cmplt_epi64_mask(simde_svbool_to_mmask4(pg), op1.m256i[0], op2.m256i[0]));
    #elif defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      r.neon_i64 = vandq_s64(pg.neon_i64, vreinterpretq_s64_u64(vcltq_s64(op1.neon, op2.neon)));
    #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
      r.altivec_b64 = vec_and(pg.altivec_b64, vec_cmplt(op1.altivec, op2.altivec));
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec_b64 = pg.altivec_b64 & vec_cmplt(op1.altivec, op2.altivec);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE) && defined(SIMDE_WASM_TODO)
      r.v128 = wasm_v128_and(pg.v128, wasm_i64x2_lt(op1.v128, op2.v128));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values_i64 = pg.values_i64 & HEDLEY_REINTERPRET_CAST(__typeof__(r.values_i64), op1.values < op2.values);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values_i64) / sizeof(r.values_i64[0])) ; i++) {
        r.values_i64[i] = pg.values_i64[i] & ((op1.values[i] < op2.values[i]) ? ~INT64_C(0) : INT64_C(0));
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svcmplt_s64
  #define svcmplt_s64(pg, op1, op2) simde_svcmplt_s64(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svcmplt_u8(simde_svbool_t pg, simde_svuint8_t op1, simde_svuint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svcmplt_u8(pg, op1, op2);
  #else
    simde_svbool_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r = simde_svbool_from_mmask64(_mm512_mask_cmplt_epu8_mask(simde_svbool_to_mmask64(pg), op1.m512i, op2.m512i));
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r = simde_svbool_from_mmask32(_mm256_mask_cmplt_epu8_mask(simde_svbool_to_mmask32(pg), op1.m256i[0], op2.m256i[0]));
    #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon_u8 = vandq_u8(pg.neon_u8, vcltq_u8(op1.neon, op2.neon));
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec_b8 = vec_and(pg.altivec_b8, vec_cmplt(op1.altivec, op2.altivec));
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec_b8 = pg.altivec_b8 & vec_cmplt(op1.altivec, op2.altivec);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_and(pg.v128, wasm_u8x16_lt(op1.v128, op2.v128));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values_u8 = pg.values_u8 & HEDLEY_REINTERPRET_CAST(__typeof__(r.values_u8), op1.values < op2.values);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values_u8) / sizeof(r.values_u8[0])) ; i++) {
        r.values_u8[i] = pg.values_u8[i] & ((op1.values[i] < op2.values[i]) ? ~UINT8_C(0) : UINT8_C(0));
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svcmplt_u8
  #define svcmplt_u8(pg, op1, op2) simde_svcmplt_u8(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svcmplt_u16(simde_svbool_t pg, simde_svuint16_t op1, simde_svuint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svcmplt_u16(pg, op1, op2);
  #else
    simde_svbool_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r = simde_svbool_from_mmask32(_mm512_mask_cmplt_epu16_mask(simde_svbool_to_mmask32(pg), op1.m512i, op2.m512i));
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r = simde_svbool_from_mmask16(_mm256_mask_cmplt_epu16_mask(simde_svbool_to_mmask16(pg), op1.m256i[0], op2.m256i[0]));
    #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon_u16 = vandq_u16(pg.neon_u16, vcltq_u16(op1.neon, op2.neon));
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec_b16 = vec_and(pg.altivec_b16, vec_cmplt(op1.altivec, op2.altivec));
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec_b16 = pg.altivec_b16 & vec_cmplt(op1.altivec, op2.altivec);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_and(pg.v128, wasm_u16x8_lt(op1.v128, op2.v128));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values_u16 = pg.values_u16 & HEDLEY_REINTERPRET_CAST(__typeof__(r.values_u16), op1.values < op2.values);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values_u16) / sizeof(r.values_u16[0])) ; i++) {
        r.values_u16[i] = pg.values_u16[i] & ((op1.values[i] < op2.values[i]) ? ~UINT16_C(0) : UINT16_C(0));
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svcmplt_u16
  #define svcmplt_u16(pg, op1, op2) simde_svcmplt_u16(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svcmplt_u32(simde_svbool_t pg, simde_svuint32_t op1, simde_svuint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svcmplt_u32(pg, op1, op2);
  #else
    simde_svbool_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r = simde_svbool_from_mmask16(_mm512_mask_cmplt_epu32_mask(simde_svbool_to_mmask16(pg), op1.m512i, op2.m512i));
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r = simde_svbool_from_mmask8(_mm256_mask_cmplt_epu32_mask(simde_svbool_to_mmask8(pg), op1.m256i[0], op2.m256i[0]));
    #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon_u32 = vandq_u32(pg.neon_u32, vcltq_u32(op1.neon, op2.neon));
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec_b32 = vec_and(pg.altivec_b32, vec_cmplt(op1.altivec, op2.altivec));
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec_b32 = pg.altivec_b32 & vec_cmplt(op1.altivec, op2.altivec);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_and(pg.v128, wasm_u32x4_lt(op1.v128, op2.v128));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values_u32 = pg.values_u32 & HEDLEY_REINTERPRET_CAST(__typeof__(r.values_u32), op1.values < op2.values);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values_u32) / sizeof(r.values_u32[0])) ; i++) {
        r.values_u32[i] = pg.values_u32[i] & ((op1.values[i] < op2.values[i]) ? ~UINT32_C(0) : UINT32_C(0));
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svcmplt_u32
  #define svcmplt_u32(pg, op1, op2) simde_svcmplt_u32(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svcmplt_u64(simde_svbool_t pg, simde_svuint64_t op1, simde_svuint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svcmplt_u64(pg, op1, op2);
  #else
    simde_svbool_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r = simde_svbool_from_mmask8(_mm512_mask_cmplt_epu64_mask(simde_svbool_to_mmask8(pg), op1.m512i, op2.m512i));
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r = simde_svbool_from_mmask4(_mm256_mask_cmplt_epu64_mask(simde_svbool_to_mmask4(pg), op1.m256i[0], op2.m256i[0]));
    #elif defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      r.neon_u64 = vandq_u64(pg.neon_u64, vcltq_u64(op1.neon, op2.neon));
    #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
      r.altivec_b64 = vec_and(pg.altivec_b64, vec_cmplt(op1.altivec, op2.altivec));
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec_b64 = pg.altivec_b64 & vec_cmplt(op1.altivec, op2.altivec);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE) && defined(SIMDE_WASM_TODO)
      r.v128 = wasm_v128_and(pg.v128, wasm_u64x2_lt(op1.v128, op2.v128));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values_u64 = pg.values_u64 & HEDLEY_REINTERPRET_CAST(__typeof__(r.values_u64), op1.values < op2.values);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values_u64) / sizeof(r.values_u64[0])) ; i++) {
        r.values_u64[i] = pg.values_u64[i] & ((op1.values[i] < op2.values[i]) ? ~UINT64_C(0) : UINT64_C(0));
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svcmplt_u64
  #define svcmplt_u64(pg, op1, op2) simde_svcmplt_u64(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svcmplt_f32(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svcmplt_f32(pg, op1, op2);
  #else
    simde_svbool_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r = simde_svbool_from_mmask16(_mm512_mask_cmp_ps_mask(simde_svbool_to_mmask16(pg), op1.m512, op2.m512, _CMP_LT_OQ));
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r = simde_svbool_from_mmask8(_mm256_mask_cmp_ps_mask(simde_svbool_to_mmask8(pg), op1.m256[0], op2.m256[0], _CMP_LT_OQ));
    #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon_u32 = vandq_u32(pg.neon_u32, vcltq_f32(op1.neon, op2.neon));
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_castps_si128(_mm_and_ps(_mm_castsi128_ps(pg.m128i[i]), _mm_cmplt_ps(op1.m128[i], op2.m128[i])));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec_b32 = vec_and(pg.altivec_b32, vec_cmplt(op1.altivec, op2.altivec));
    #elif defined(SIMDE_ZARCH_ZVECTOR_14_NATIVE)
      r.altivec_b32 = pg.altivec_b32 & vec_cmplt(op1.altivec, op2.altivec);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_v128_and(pg.v128, wasm_f32x4_lt(op1.v128, op2.v128));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values_i32 = pg.values_i32 & HEDLEY_REINTERPRET_CAST(__typeof__(r.values_i32), op1.values < op2.values);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values_i32) / sizeof(r.values_i32[0])) ; i++) {
        r.values_i32[i] = pg.values_i32[i] & ((op1.values[i] < op2.values[i]) ? ~INT32_C(0) : INT32_C(0));
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svcmplt_f32
  #define svcmplt_f32(pg, op1, op2) simde_svcmplt_f32(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svbool_t
simde_svcmplt_f64(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svcmplt_f64(pg, op1, op2);
  #else
    simde_svbool_t r;

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r = simde_svbool_from_mmask8(_mm512_mask_cmp_pd_mask(simde_svbool_to_mmask8(pg), op1.m512d, op2.m512d, _CMP_LT_OQ));
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) \
        && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
      r = simde_svbool_from_mmask4(_mm256_mask_cmp_pd_mask(simde_svbool_to_mmask4(pg), op1.m256d[0], op2.m256d[0], _CMP_LT_OQ));
    #elif defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      r.neon_u64 = vandq_u64(pg.neon_u64, vcltq_f64(op1.neon, op2.neon));
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_castpd_si128(_mm_and_pd(_mm_castsi128_pd(pg.m128i[i]), _mm_cmplt_pd(op1.m128d[i], op2.m128d[i])));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec_b64 = pg.altivec_b64 & vec_cmplt(op1.altivec, op2.altivec);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE) && defined(SIMDE_WASM_TODO)
      r.v128 = wasm_v128_and(pg.v128, wasm_f64x2_lt(op1.v128, op2.v128));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values_i64 = pg.values_i64 & HEDLEY_REINTERPRET_CAST(__typeof__(r.values_i64), op1.values < op2.values);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values_i64) / sizeof(r.values_i64[0])) ; i++) {
        r.values_i64[i] = pg.values_i64[i] & ((op1.values[i] < op2.values[i]) ? ~INT64_C(0) : INT64_C(0));
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svcmplt_f64
  #define svcmplt_f64(pg, op1, op2) simde_svcmplt_f64(pg, op1, op2)
#endif

#if defined(__cplusplus)
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svcmplt(simde_svbool_t pg,    simde_svint8_t op1,    simde_svint8_t op2) { return  simde_svcmplt_s8(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svcmplt(simde_svbool_t pg,   simde_svint16_t op1,   simde_svint16_t op2) { return simde_svcmplt_s16(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svcmplt(simde_svbool_t pg,   simde_svint32_t op1,   simde_svint32_t op2) { return simde_svcmplt_s32(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svcmplt(simde_svbool_t pg,   simde_svint64_t op1,   simde_svint64_t op2) { return simde_svcmplt_s64(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svcmplt(simde_svbool_t pg,   simde_svuint8_t op1,   simde_svuint8_t op2) { return  simde_svcmplt_u8(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svcmplt(simde_svbool_t pg,  simde_svuint16_t op1,  simde_svuint16_t op2) { return simde_svcmplt_u16(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svcmplt(simde_svbool_t pg,  simde_svuint32_t op1,  simde_svuint32_t op2) { return simde_svcmplt_u32(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svcmplt(simde_svbool_t pg,  simde_svuint64_t op1,  simde_svuint64_t op2) { return simde_svcmplt_u64(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svcmplt(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) { return simde_svcmplt_f32(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svbool_t simde_svcmplt(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) { return simde_svcmplt_f64(pg, op1, op2); }

  #if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
    SIMDE_FUNCTION_ATTRIBUTES svbool_t svcmplt(svbool_t pg,    svint8_t op1,    svint8_t op2) { return  svcmplt_s8(pg, op1, op2); }
    SIMDE_FUNCTION_ATTRIBUTES svbool_t svcmplt(svbool_t pg,   svint16_t op1,   svint16_t op2) { return svcmplt_s16(pg, op1, op2); }
    SIMDE_FUNCTION_ATTRIBUTES svbool_t svcmplt(svbool_t pg,   svint32_t op1,   svint32_t op2) { return svcmplt_s32(pg, op1, op2); }
    SIMDE_FUNCTION_ATTRIBUTES svbool_t svcmplt(svbool_t pg,   svint64_t op1,   svint64_t op2) { return svcmplt_s64(pg, op1, op2); }
    SIMDE_FUNCTION_ATTRIBUTES svbool_t svcmplt(svbool_t pg,   svuint8_t op1,   svuint8_t op2) { return  svcmplt_u8(pg, op1, op2); }
    SIMDE_FUNCTION_ATTRIBUTES svbool_t svcmplt(svbool_t pg,  svuint16_t op1,  svuint16_t op2) { return svcmplt_u16(pg, op1, op2); }
    SIMDE_FUNCTION_ATTRIBUTES svbool_t svcmplt(svbool_t pg,  svuint32_t op1,  svuint32_t op2) { return svcmplt_u32(pg, op1, op2); }
    SIMDE_FUNCTION_ATTRIBUTES svbool_t svcmplt(svbool_t pg,  svuint64_t op1,  svuint64_t op2) { return svcmplt_u64(pg, op1, op2); }
    SIMDE_FUNCTION_ATTRIBUTES svbool_t svcmplt(svbool_t pg, svfloat32_t op1, svfloat32_t op2) { return svcmplt_f32(pg, op1, op2); }
    SIMDE_FUNCTION_ATTRIBUTES svbool_t svcmplt(svbool_t pg, svfloat64_t op1, svfloat64_t op2) { return svcmplt_f64(pg, op1, op2); }
  #endif
#elif defined(SIMDE_GENERIC_)
  #define simde_svcmplt(pg, op1, op2) \
    (SIMDE_GENERIC_((op1), \
        simde_svint8_t:  simde_svcmplt_s8)(pg, op1, op2), \
       simde_svint16_t: simde_svcmplt_s16)(pg, op1, op2), \
       simde_svint32_t: simde_svcmplt_s32)(pg, op1, op2), \
       simde_svint64_t: simde_svcmplt_s64)(pg, op1, op2), \
       simde_svuint8_t:  simde_svcmplt_u8)(pg, op1, op2), \
      simde_svuint16_t: simde_svcmplt_u16)(pg, op1, op2), \
      simde_svuint32_t: simde_svcmplt_u32)(pg, op1, op2), \
      simde_svuint64_t: simde_svcmplt_u64)(pg, op1, op2), \
       simde_svint32_t: simde_svcmplt_f32)(pg, op1, op2), \
       simde_svint64_t: simde_svcmplt_f64)(pg, op1, op2))

  #if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
    #define svcmplt(pg, op1, op2) \
      (SIMDE_GENERIC_((op1), \
          svint8_t:  svcmplt_s8)(pg, op1, op2), \
         svint16_t: svcmplt_s16)(pg, op1, op2), \
         svint32_t: svcmplt_s32)(pg, op1, op2), \
         svint64_t: svcmplt_s64)(pg, op1, op2), \
         svuint8_t:  svcmplt_u8)(pg, op1, op2), \
        svuint16_t: svcmplt_u16)(pg, op1, op2), \
        svuint32_t: svcmplt_u32)(pg, op1, op2), \
        svuint64_t: svcmplt_u64)(pg, op1, op2), \
         svint32_t: svcmplt_f32)(pg, op1, op2), \
         svint64_t: svcmplt_f64)(pg, op1, op2))
  #endif
#endif
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef svcmplt
  #define svcmplt(pg, op1, op2) simde_svcmplt((pg), (op1), (op2))
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_CMPLT_H */
/* :: End simde/arm/sve/cmplt.h :: */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/arm/sve/qadd.h :: */
/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_ARM_SVE_QADD_H)
#define SIMDE_ARM_SVE_QADD_H

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svqadd_s8(simde_svint8_t op1, simde_svint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_s8(op1, op2);
  #else
    simde_svint8_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vqaddq_s8(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_adds_epi8(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_adds_epi8(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_adds_epi8(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_adds(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec =
        vec_packs(
          vec_unpackh(op1.altivec) + vec_unpackh(op2.altivec),
          vec_unpackl(op1.altivec) + vec_unpackl(op2.altivec)
        );
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i8x16_add_sat(op1.v128, op2.v128);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = simde_math_adds_i8(op1.values[i], op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_s8
  #define svqadd_s8(op1, op2) simde_svqadd_s8(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svqadd_n_s8(simde_svint8_t op1, int8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_n_s8(op1, op2);
  #else
    return simde_svqadd_s8(op1, simde_svdup_n_s8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_n_s8
  #define svqadd_n_s8(op1, op2) simde_svqadd_n_s8(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svqadd_s16(simde_svint16_t op1, simde_svint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_s16(op1, op2);
  #else
    simde_svint16_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vqaddq_s16(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_adds_epi16(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_adds_epi16(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_adds_epi16(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_adds(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec =
        vec_packs(
          vec_unpackh(op1.altivec) + vec_unpackh(op2.altivec),
          vec_unpackl(op1.altivec) + vec_unpackl(op2.altivec)
        );
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i16x8_add_sat(op1.v128, op2.v128);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = simde_math_adds_i16(op1.values[i], op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_s16
  #define svqadd_s16(op1, op2) simde_svqadd_s16(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svqadd_n_s16(simde_svint16_t op1, int16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_n_s16(op1, op2);
  #else
    return simde_svqadd_s16(op1, simde_svdup_n_s16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_n_s16
  #define svqadd_n_s16(op1, op2) simde_svqadd_n_s16(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svqadd_s32(simde_svint32_t op1, simde_svint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_s32(op1, op2);
  #else
    simde_svint32_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vqaddq_s32(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512VL_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm512_cvtsepi64_epi32(_mm512_add_epi64(_mm512_cvtepi32_epi64(op1.m256i[i]), _mm512_cvtepi32_epi64(op2.m256i[i])));
      }
    #elif defined(SIMDE_X86_AVX512VL_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm256_cvtsepi64_epi32(_mm256_add_epi64(_mm256_cvtepi32_epi64(op1.m128i[i]), _mm256_cvtepi32_epi64(op2.m128i[i])));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_adds(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec =
        vec_packs(
          vec_unpackh(op1.altivec) + vec_unpackh(op2.altivec),
          vec_unpackl(op1.altivec) + vec_unpackl(op2.altivec)
        );
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = simde_math_adds_i32(op1.values[i], op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_s32
  #define svqadd_s32(op1, op2) simde_svqadd_s32(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svqadd_n_s32(simde_svint32_t op1, int32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_n_s32(op1, op2);
  #else
    return simde_svqadd_s32(op1, simde_svdup_n_s32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_n_s32
  #define svqadd_n_s32(op1, op2) simde_svqadd_n_s32(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svqadd_s64(simde_svint64_t op1, simde_svint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_s64(op1, op2);
  #else
    simde_svint64_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vqaddq_s64(op1.neon, op2.neon);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = simde_math_adds_i64(op1.values[i], op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_s64
  #define svqadd_s64(op1, op2) simde_svqadd_s64(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svqadd_n_s64(simde_svint64_t op1, int64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_n_s64(op1, op2);
  #else
    return simde_svqadd_s64(op1, simde_svdup_n_s64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_n_s64
  #define svqadd_n_s64(op1, op2) simde_svqadd_n_s64(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svqadd_u8(simde_svuint8_t op1, simde_svuint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_u8(op1, op2);
  #else
    simde_svuint8_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vqaddq_u8(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_adds_epu8(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_adds_epu8(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_adds_epu8(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_adds(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec =
        vec_packs(
          vec_unpackh(op1.altivec) + vec_unpackh(op2.altivec),
          vec_unpackl(op1.altivec) + vec_unpackl(op2.altivec)
        );
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_u8x16_add_sat(op1.v128, op2.v128);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = simde_math_adds_u8(op1.values[i], op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_u8
  #define svqadd_u8(op1, op2) simde_svqadd_u8(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svqadd_n_u8(simde_svuint8_t op1, uint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_n_u8(op1, op2);
  #else
    return simde_svqadd_u8(op1, simde_svdup_n_u8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_n_u8
  #define svqadd_n_u8(op1, op2) simde_svqadd_n_u8(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svqadd_u16(simde_svuint16_t op1, simde_svuint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_u16(op1, op2);
  #else
    simde_svuint16_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vqaddq_u16(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_adds_epu16(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_adds_epu16(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_adds_epu16(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_adds(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec =
        vec_packs(
          vec_unpackh(op1.altivec) + vec_unpackh(op2.altivec),
          vec_unpackl(op1.altivec) + vec_unpackl(op2.altivec)
        );
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_u16x8_add_sat(op1.v128, op2.v128);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = simde_math_adds_u16(op1.values[i], op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_u16
  #define svqadd_u16(op1, op2) simde_svqadd_u16(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svqadd_n_u16(simde_svuint16_t op1, uint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_n_u16(op1, op2);
  #else
    return simde_svqadd_u16(op1, simde_svdup_n_u16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_n_u16
  #define svqadd_n_u16(op1, op2) simde_svqadd_n_u16(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svqadd_u32(simde_svuint32_t op1, simde_svuint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_u32(op1, op2);
  #else
    simde_svuint32_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vqaddq_u32(op1.neon, op2.neon);
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_adds(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec =
        vec_packs(
          vec_unpackh(op1.altivec) + vec_unpackh(op2.altivec),
          vec_unpackl(op1.altivec) + vec_unpackl(op2.altivec)
        );
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = simde_math_adds_u32(op1.values[i], op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_u32
  #define svqadd_u32(op1, op2) simde_svqadd_u32(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svqadd_n_u32(simde_svuint32_t op1, uint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_n_u32(op1, op2);
  #else
    return simde_svqadd_u32(op1, simde_svdup_n_u32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_n_u32
  #define svqadd_n_u32(op1, op2) simde_svqadd_n_u32(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svqadd_u64(simde_svuint64_t op1, simde_svuint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_u64(op1, op2);
  #else
    simde_svuint64_t r;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vqaddq_u64(op1.neon, op2.neon);
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = simde_math_adds_u64(op1.values[i], op2.values[i]);
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_u64
  #define svqadd_u64(op1, op2) simde_svqadd_u64(op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svqadd_n_u64(simde_svuint64_t op1, uint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svqadd_n_u64(op1, op2);
  #else
    return simde_svqadd_u64(op1, simde_svdup_n_u64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svqadd_n_u64
  #define svqadd_n_u64(op1, op2) simde_svqadd_n_u64(op1, op2)
#endif

#if defined(__cplusplus)
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svqadd(   simde_svint8_t op1,    simde_svint8_t op2) { return simde_svqadd_s8   (op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svqadd(  simde_svint16_t op1,   simde_svint16_t op2) { return simde_svqadd_s16  (op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svqadd(  simde_svint32_t op1,   simde_svint32_t op2) { return simde_svqadd_s32  (op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svqadd(  simde_svint64_t op1,   simde_svint64_t op2) { return simde_svqadd_s64  (op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svqadd(  simde_svuint8_t op1,   simde_svuint8_t op2) { return simde_svqadd_u8   (op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svqadd( simde_svuint16_t op1,  simde_svuint16_t op2) { return simde_svqadd_u16  (op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svqadd( simde_svuint32_t op1,  simde_svuint32_t op2) { return simde_svqadd_u32  (op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svqadd( simde_svuint64_t op1,  simde_svuint64_t op2) { return simde_svqadd_u64  (op1, op2); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svqadd(   simde_svint8_t op1,            int8_t op2) { return simde_svqadd_n_s8 (op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svqadd(  simde_svint16_t op1,           int16_t op2) { return simde_svqadd_n_s16(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svqadd(  simde_svint32_t op1,           int32_t op2) { return simde_svqadd_n_s32(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svqadd(  simde_svint64_t op1,           int64_t op2) { return simde_svqadd_n_s64(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svqadd(  simde_svuint8_t op1,           uint8_t op2) { return simde_svqadd_n_u8 (op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svqadd( simde_svuint16_t op1,          uint16_t op2) { return simde_svqadd_n_u16(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svqadd( simde_svuint32_t op1,          uint32_t op2) { return simde_svqadd_n_u32(op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svqadd( simde_svuint64_t op1,          uint64_t op2) { return simde_svqadd_n_u64(op1, op2); }
#elif defined(SIMDE_GENERIC_)
  #define simde_svqadd_x(op1, op2) \
    (SIMDE_GENERIC_((op2), \
         simde_svint8_t: simde_svqadd_s8, \
        simde_svint16_t: simde_svqadd_s16, \
        simde_svint32_t: simde_svqadd_s32, \
        simde_svint64_t: simde_svqadd_s64, \
        simde_svuint8_t: simde_svqadd_u8, \
       simde_svuint16_t: simde_svqadd_u16, \
       simde_svuint32_t: simde_svqadd_u32, \
       simde_svuint64_t: simde_svqadd_u64, \
                 int8_t: simde_svqadd_n_s8, \
                int16_t: simde_svqadd_n_s16, \
                int32_t: simde_svqadd_n_s32, \
                int64_t: simde_svqadd_n_s64, \
                uint8_t: simde_svqadd_n_u8, \
               uint16_t: simde_svqadd_n_u16, \
               uint32_t: simde_svqadd_n_u32, \
               uint64_t: simde_svqadd_n_u64)((pg), (op1), (op2)))
#endif
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef svqadd
  #define svqadd(op1, op2) simde_svqadd((pg), (op1), (op2))
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_QADD_H */
/* :: End simde/arm/sve/qadd.h :: */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* :: Begin simde/arm/sve/sub.h :: */
/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_ARM_SVE_SUB_H)
#define SIMDE_ARM_SVE_SUB_H

/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svsub_s8_x(simde_svbool_t pg, simde_svint8_t op1, simde_svint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s8_x(pg, op1, op2);
  #else
    simde_svint8_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vsubq_s8(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_sub_epi8(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_sub_epi8(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_sub_epi8(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_sub_epi8(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_sub(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec - op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i8x16_sub(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values - op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] - op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s8_x
  #define svsub_s8_x(pg, op1, op2) simde_svsub_s8_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svsub_s8_z(simde_svbool_t pg, simde_svint8_t op1, simde_svint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s8_z(pg, op1, op2);
  #else
    return simde_x_svsel_s8_z(pg, simde_svsub_s8_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s8_z
  #define svsub_s8_z(pg, op1, op2) simde_svsub_s8_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svsub_s8_m(simde_svbool_t pg, simde_svint8_t op1, simde_svint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s8_m(pg, op1, op2);
  #else
    return simde_svsel_s8(pg, simde_svsub_s8_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s8_m
  #define svsub_s8_m(pg, op1, op2) simde_svsub_s8_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svsub_n_s8_x(simde_svbool_t pg, simde_svint8_t op1, int8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s8_x(pg, op1, op2);
  #else
    return simde_svsub_s8_x(pg, op1, simde_svdup_n_s8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s8_x
  #define svsub_n_s8_x(pg, op1, op2) simde_svsub_n_s8_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svsub_n_s8_z(simde_svbool_t pg, simde_svint8_t op1, int8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s8_z(pg, op1, op2);
  #else
    return simde_svsub_s8_z(pg, op1, simde_svdup_n_s8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s8_z
  #define svsub_n_s8_z(pg, op1, op2) simde_svsub_n_s8_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint8_t
simde_svsub_n_s8_m(simde_svbool_t pg, simde_svint8_t op1, int8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s8_m(pg, op1, op2);
  #else
    return simde_svsub_s8_m(pg, op1, simde_svdup_n_s8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s8_m
  #define svsub_n_s8_m(pg, op1, op2) simde_svsub_n_s8_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svsub_s16_x(simde_svbool_t pg, simde_svint16_t op1, simde_svint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s16_x(pg, op1, op2);
  #else
    simde_svint16_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vsubq_s16(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_sub_epi16(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_sub_epi16(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_sub_epi16(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_sub_epi16(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_sub(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec - op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i16x8_sub(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values - op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] - op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s16_x
  #define svsub_s16_x(pg, op1, op2) simde_svsub_s16_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svsub_s16_z(simde_svbool_t pg, simde_svint16_t op1, simde_svint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s16_z(pg, op1, op2);
  #else
    return simde_x_svsel_s16_z(pg, simde_svsub_s16_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s16_z
  #define svsub_s16_z(pg, op1, op2) simde_svsub_s16_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svsub_s16_m(simde_svbool_t pg, simde_svint16_t op1, simde_svint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s16_m(pg, op1, op2);
  #else
    return simde_svsel_s16(pg, simde_svsub_s16_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s16_m
  #define svsub_s16_m(pg, op1, op2) simde_svsub_s16_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svsub_n_s16_x(simde_svbool_t pg, simde_svint16_t op1, int16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s16_x(pg, op1, op2);
  #else
    return simde_svsub_s16_x(pg, op1, simde_svdup_n_s16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s16_x
  #define svsub_n_s16_x(pg, op1, op2) simde_svsub_n_s16_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svsub_n_s16_z(simde_svbool_t pg, simde_svint16_t op1, int16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s16_z(pg, op1, op2);
  #else
    return simde_svsub_s16_z(pg, op1, simde_svdup_n_s16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s16_z
  #define svsub_n_s16_z(pg, op1, op2) simde_svsub_n_s16_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint16_t
simde_svsub_n_s16_m(simde_svbool_t pg, simde_svint16_t op1, int16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s16_m(pg, op1, op2);
  #else
    return simde_svsub_s16_m(pg, op1, simde_svdup_n_s16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s16_m
  #define svsub_n_s16_m(pg, op1, op2) simde_svsub_n_s16_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svsub_s32_x(simde_svbool_t pg, simde_svint32_t op1, simde_svint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s32_x(pg, op1, op2);
  #else
    simde_svint32_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vsubq_s32(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_sub_epi32(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_sub_epi32(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_sub_epi32(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_sub_epi32(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_sub(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec - op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i32x4_sub(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values - op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] - op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s32_x
  #define svsub_s32_x(pg, op1, op2) simde_svsub_s32_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svsub_s32_z(simde_svbool_t pg, simde_svint32_t op1, simde_svint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s32_z(pg, op1, op2);
  #else
    return simde_x_svsel_s32_z(pg, simde_svsub_s32_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s32_z
  #define svsub_s32_z(pg, op1, op2) simde_svsub_s32_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svsub_s32_m(simde_svbool_t pg, simde_svint32_t op1, simde_svint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s32_m(pg, op1, op2);
  #else
    return simde_svsel_s32(pg, simde_svsub_s32_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s32_m
  #define svsub_s32_m(pg, op1, op2) simde_svsub_s32_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svsub_n_s32_x(simde_svbool_t pg, simde_svint32_t op1, int32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s32_x(pg, op1, op2);
  #else
    return simde_svsub_s32_x(pg, op1, simde_svdup_n_s32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s32_x
  #define svsub_n_s32_x(pg, op1, op2) simde_svsub_n_s32_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svsub_n_s32_z(simde_svbool_t pg, simde_svint32_t op1, int32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s32_z(pg, op1, op2);
  #else
    return simde_svsub_s32_z(pg, op1, simde_svdup_n_s32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s32_z
  #define svsub_n_s32_z(pg, op1, op2) simde_svsub_n_s32_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint32_t
simde_svsub_n_s32_m(simde_svbool_t pg, simde_svint32_t op1, int32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s32_m(pg, op1, op2);
  #else
    return simde_svsub_s32_m(pg, op1, simde_svdup_n_s32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s32_m
  #define svsub_n_s32_m(pg, op1, op2) simde_svsub_n_s32_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svsub_s64_x(simde_svbool_t pg, simde_svint64_t op1, simde_svint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s64_x(pg, op1, op2);
  #else
    simde_svint64_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vsubq_s64(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_sub_epi64(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_sub_epi64(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_sub_epi64(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_sub_epi64(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
      r.altivec = vec_sub(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec - op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i64x2_sub(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values - op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] - op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s64_x
  #define svsub_s64_x(pg, op1, op2) simde_svsub_s64_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svsub_s64_z(simde_svbool_t pg, simde_svint64_t op1, simde_svint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s64_z(pg, op1, op2);
  #else
    return simde_x_svsel_s64_z(pg, simde_svsub_s64_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s64_z
  #define svsub_s64_z(pg, op1, op2) simde_svsub_s64_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svsub_s64_m(simde_svbool_t pg, simde_svint64_t op1, simde_svint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_s64_m(pg, op1, op2);
  #else
    return simde_svsel_s64(pg, simde_svsub_s64_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_s64_m
  #define svsub_s64_m(pg, op1, op2) simde_svsub_s64_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svsub_n_s64_x(simde_svbool_t pg, simde_svint64_t op1, int64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s64_x(pg, op1, op2);
  #else
    return simde_svsub_s64_x(pg, op1, simde_svdup_n_s64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s64_x
  #define svsub_n_s64_x(pg, op1, op2) simde_svsub_n_s64_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svsub_n_s64_z(simde_svbool_t pg, simde_svint64_t op1, int64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s64_z(pg, op1, op2);
  #else
    return simde_svsub_s64_z(pg, op1, simde_svdup_n_s64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s64_z
  #define svsub_n_s64_z(pg, op1, op2) simde_svsub_n_s64_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svint64_t
simde_svsub_n_s64_m(simde_svbool_t pg, simde_svint64_t op1, int64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_s64_m(pg, op1, op2);
  #else
    return simde_svsub_s64_m(pg, op1, simde_svdup_n_s64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_s64_m
  #define svsub_n_s64_m(pg, op1, op2) simde_svsub_n_s64_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svsub_u8_x(simde_svbool_t pg, simde_svuint8_t op1, simde_svuint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u8_x(pg, op1, op2);
  #else
    simde_svuint8_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vsubq_u8(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_sub_epi8(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_sub_epi8(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_sub_epi8(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_sub_epi8(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_sub(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec - op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i8x16_sub(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values - op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] - op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u8_x
  #define svsub_u8_x(pg, op1, op2) simde_svsub_u8_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svsub_u8_z(simde_svbool_t pg, simde_svuint8_t op1, simde_svuint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u8_z(pg, op1, op2);
  #else
    return simde_x_svsel_u8_z(pg, simde_svsub_u8_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u8_z
  #define svsub_u8_z(pg, op1, op2) simde_svsub_u8_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svsub_u8_m(simde_svbool_t pg, simde_svuint8_t op1, simde_svuint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u8_m(pg, op1, op2);
  #else
    return simde_svsel_u8(pg, simde_svsub_u8_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u8_m
  #define svsub_u8_m(pg, op1, op2) simde_svsub_u8_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svsub_n_u8_x(simde_svbool_t pg, simde_svuint8_t op1, uint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u8_x(pg, op1, op2);
  #else
    return simde_svsub_u8_x(pg, op1, simde_svdup_n_u8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u8_x
  #define svsub_n_u8_x(pg, op1, op2) simde_svsub_n_u8_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svsub_n_u8_z(simde_svbool_t pg, simde_svuint8_t op1, uint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u8_z(pg, op1, op2);
  #else
    return simde_svsub_u8_z(pg, op1, simde_svdup_n_u8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u8_z
  #define svsub_n_u8_z(pg, op1, op2) simde_svsub_n_u8_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint8_t
simde_svsub_n_u8_m(simde_svbool_t pg, simde_svuint8_t op1, uint8_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u8_m(pg, op1, op2);
  #else
    return simde_svsub_u8_m(pg, op1, simde_svdup_n_u8(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u8_m
  #define svsub_n_u8_m(pg, op1, op2) simde_svsub_n_u8_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svsub_u16_x(simde_svbool_t pg, simde_svuint16_t op1, simde_svuint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u16_x(pg, op1, op2);
  #else
    simde_svuint16_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vsubq_u16(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_sub_epi16(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_sub_epi16(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_sub_epi16(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_sub_epi16(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_sub(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec - op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i16x8_sub(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values - op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] - op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u16_x
  #define svsub_u16_x(pg, op1, op2) simde_svsub_u16_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svsub_u16_z(simde_svbool_t pg, simde_svuint16_t op1, simde_svuint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u16_z(pg, op1, op2);
  #else
    return simde_x_svsel_u16_z(pg, simde_svsub_u16_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u16_z
  #define svsub_u16_z(pg, op1, op2) simde_svsub_u16_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svsub_u16_m(simde_svbool_t pg, simde_svuint16_t op1, simde_svuint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u16_m(pg, op1, op2);
  #else
    return simde_svsel_u16(pg, simde_svsub_u16_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u16_m
  #define svsub_u16_m(pg, op1, op2) simde_svsub_u16_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svsub_n_u16_x(simde_svbool_t pg, simde_svuint16_t op1, uint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u16_x(pg, op1, op2);
  #else
    return simde_svsub_u16_x(pg, op1, simde_svdup_n_u16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u16_x
  #define svsub_n_u16_x(pg, op1, op2) simde_svsub_n_u16_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svsub_n_u16_z(simde_svbool_t pg, simde_svuint16_t op1, uint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u16_z(pg, op1, op2);
  #else
    return simde_svsub_u16_z(pg, op1, simde_svdup_n_u16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u16_z
  #define svsub_n_u16_z(pg, op1, op2) simde_svsub_n_u16_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint16_t
simde_svsub_n_u16_m(simde_svbool_t pg, simde_svuint16_t op1, uint16_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u16_m(pg, op1, op2);
  #else
    return simde_svsub_u16_m(pg, op1, simde_svdup_n_u16(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u16_m
  #define svsub_n_u16_m(pg, op1, op2) simde_svsub_n_u16_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svsub_u32_x(simde_svbool_t pg, simde_svuint32_t op1, simde_svuint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u32_x(pg, op1, op2);
  #else
    simde_svuint32_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vsubq_u32(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_sub_epi32(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_sub_epi32(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_sub_epi32(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_sub_epi32(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_sub(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec - op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i32x4_sub(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values - op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] - op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u32_x
  #define svsub_u32_x(pg, op1, op2) simde_svsub_u32_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svsub_u32_z(simde_svbool_t pg, simde_svuint32_t op1, simde_svuint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u32_z(pg, op1, op2);
  #else
    return simde_x_svsel_u32_z(pg, simde_svsub_u32_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u32_z
  #define svsub_u32_z(pg, op1, op2) simde_svsub_u32_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svsub_u32_m(simde_svbool_t pg, simde_svuint32_t op1, simde_svuint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u32_m(pg, op1, op2);
  #else
    return simde_svsel_u32(pg, simde_svsub_u32_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u32_m
  #define svsub_u32_m(pg, op1, op2) simde_svsub_u32_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svsub_n_u32_x(simde_svbool_t pg, simde_svuint32_t op1, uint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u32_x(pg, op1, op2);
  #else
    return simde_svsub_u32_x(pg, op1, simde_svdup_n_u32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u32_x
  #define svsub_n_u32_x(pg, op1, op2) simde_svsub_n_u32_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svsub_n_u32_z(simde_svbool_t pg, simde_svuint32_t op1, uint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u32_z(pg, op1, op2);
  #else
    return simde_svsub_u32_z(pg, op1, simde_svdup_n_u32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u32_z
  #define svsub_n_u32_z(pg, op1, op2) simde_svsub_n_u32_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint32_t
simde_svsub_n_u32_m(simde_svbool_t pg, simde_svuint32_t op1, uint32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u32_m(pg, op1, op2);
  #else
    return simde_svsub_u32_m(pg, op1, simde_svdup_n_u32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u32_m
  #define svsub_n_u32_m(pg, op1, op2) simde_svsub_n_u32_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svsub_u64_x(simde_svbool_t pg, simde_svuint64_t op1, simde_svuint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u64_x(pg, op1, op2);
  #else
    simde_svuint64_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vsubq_u64(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512i = _mm512_sub_epi64(op1.m512i, op2.m512i);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256i[0] = _mm256_sub_epi64(op1.m256i[0], op2.m256i[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256i) / sizeof(r.m256i[0])) ; i++) {
        r.m256i[i] = _mm256_sub_epi64(op1.m256i[i], op2.m256i[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128i) / sizeof(r.m128i[0])) ; i++) {
        r.m128i[i] = _mm_sub_epi64(op1.m128i[i], op2.m128i[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
      r.altivec = vec_sub(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec - op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_i64x2_sub(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values - op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] - op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u64_x
  #define svsub_u64_x(pg, op1, op2) simde_svsub_u64_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svsub_u64_z(simde_svbool_t pg, simde_svuint64_t op1, simde_svuint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u64_z(pg, op1, op2);
  #else
    return simde_x_svsel_u64_z(pg, simde_svsub_u64_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u64_z
  #define svsub_u64_z(pg, op1, op2) simde_svsub_u64_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svsub_u64_m(simde_svbool_t pg, simde_svuint64_t op1, simde_svuint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_u64_m(pg, op1, op2);
  #else
    return simde_svsel_u64(pg, simde_svsub_u64_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_u64_m
  #define svsub_u64_m(pg, op1, op2) simde_svsub_u64_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svsub_n_u64_x(simde_svbool_t pg, simde_svuint64_t op1, uint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u64_x(pg, op1, op2);
  #else
    return simde_svsub_u64_x(pg, op1, simde_svdup_n_u64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u64_x
  #define svsub_n_u64_x(pg, op1, op2) simde_svsub_n_u64_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svsub_n_u64_z(simde_svbool_t pg, simde_svuint64_t op1, uint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u64_z(pg, op1, op2);
  #else
    return simde_svsub_u64_z(pg, op1, simde_svdup_n_u64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u64_z
  #define svsub_n_u64_z(pg, op1, op2) simde_svsub_n_u64_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svuint64_t
simde_svsub_n_u64_m(simde_svbool_t pg, simde_svuint64_t op1, uint64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_u64_m(pg, op1, op2);
  #else
    return simde_svsub_u64_m(pg, op1, simde_svdup_n_u64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_u64_m
  #define svsub_n_u64_m(pg, op1, op2) simde_svsub_n_u64_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svsub_f32_x(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_f32_x(pg, op1, op2);
  #else
    simde_svfloat32_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r.neon = vsubq_f32(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512 = _mm512_sub_ps(op1.m512, op2.m512);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256[0] = _mm256_sub_ps(op1.m256[0], op2.m256[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256) / sizeof(r.m256[0])) ; i++) {
        r.m256[i] = _mm256_sub_ps(op1.m256[i], op2.m256[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128) / sizeof(r.m128[0])) ; i++) {
        r.m128[i] = _mm_sub_ps(op1.m128[i], op2.m128[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r.altivec = vec_sub(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec - op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_f32x4_sub(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values - op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] - op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_f32_x
  #define svsub_f32_x(pg, op1, op2) simde_svsub_f32_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svsub_f32_z(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_f32_z(pg, op1, op2);
  #else
    return simde_x_svsel_f32_z(pg, simde_svsub_f32_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_f32_z
  #define svsub_f32_z(pg, op1, op2) simde_svsub_f32_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svsub_f32_m(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_f32_m(pg, op1, op2);
  #else
    return simde_svsel_f32(pg, simde_svsub_f32_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_f32_m
  #define svsub_f32_m(pg, op1, op2) simde_svsub_f32_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svsub_n_f32_x(simde_svbool_t pg, simde_svfloat32_t op1, simde_float32 op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_f32_x(pg, op1, op2);
  #else
    return simde_svsub_f32_x(pg, op1, simde_svdup_n_f32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_f32_x
  #define svsub_n_f32_x(pg, op1, op2) simde_svsub_n_f32_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svsub_n_f32_z(simde_svbool_t pg, simde_svfloat32_t op1, simde_float32 op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_f32_z(pg, op1, op2);
  #else
    return simde_svsub_f32_z(pg, op1, simde_svdup_n_f32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_f32_z
  #define svsub_n_f32_z(pg, op1, op2) simde_svsub_n_f32_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat32_t
simde_svsub_n_f32_m(simde_svbool_t pg, simde_svfloat32_t op1, simde_float32 op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_f32_m(pg, op1, op2);
  #else
    return simde_svsub_f32_m(pg, op1, simde_svdup_n_f32(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_f32_m
  #define svsub_n_f32_m(pg, op1, op2) simde_svsub_n_f32_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svsub_f64_x(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_f64_x(pg, op1, op2);
  #else
    simde_svfloat64_t r;
    HEDLEY_STATIC_CAST(void, pg);

    #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      r.neon = vsubq_f64(op1.neon, op2.neon);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      r.m512d = _mm512_sub_pd(op1.m512d, op2.m512d);
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      r.m256d[0] = _mm256_sub_pd(op1.m256d[0], op2.m256d[0]);
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m256d) / sizeof(r.m256d[0])) ; i++) {
        r.m256d[i] = _mm256_sub_pd(op1.m256d[i], op2.m256d[i]);
      }
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.m128d) / sizeof(r.m128d[0])) ; i++) {
        r.m128d[i] = _mm_sub_pd(op1.m128d[i], op2.m128d[i]);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
      r.altivec = vec_sub(op1.altivec, op2.altivec);
    #elif defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r.altivec = op1.altivec - op2.altivec;
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r.v128 = wasm_f64x2_sub(op1.v128, op2.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      r.values = op1.values - op2.values;
    #else
      SIMDE_VECTORIZE
      for (int i = 0 ; i < HEDLEY_STATIC_CAST(int, sizeof(r.values) / sizeof(r.values[0])) ; i++) {
        r.values[i] = op1.values[i] - op2.values[i];
      }
    #endif

    return r;
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_f64_x
  #define svsub_f64_x(pg, op1, op2) simde_svsub_f64_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svsub_f64_z(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_f64_z(pg, op1, op2);
  #else
    return simde_x_svsel_f64_z(pg, simde_svsub_f64_x(pg, op1, op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_f64_z
  #define svsub_f64_z(pg, op1, op2) simde_svsub_f64_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svsub_f64_m(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_f64_m(pg, op1, op2);
  #else
    return simde_svsel_f64(pg, simde_svsub_f64_x(pg, op1, op2), op1);
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_f64_m
  #define svsub_f64_m(pg, op1, op2) simde_svsub_f64_m(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svsub_n_f64_x(simde_svbool_t pg, simde_svfloat64_t op1, simde_float64 op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_f64_x(pg, op1, op2);
  #else
    return simde_svsub_f64_x(pg, op1, simde_svdup_n_f64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_f64_x
  #define svsub_n_f64_x(pg, op1, op2) simde_svsub_n_f64_x(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svsub_n_f64_z(simde_svbool_t pg, simde_svfloat64_t op1, simde_float64 op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_f64_z(pg, op1, op2);
  #else
    return simde_svsub_f64_z(pg, op1, simde_svdup_n_f64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_f64_z
  #define svsub_n_f64_z(pg, op1, op2) simde_svsub_n_f64_z(pg, op1, op2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_svfloat64_t
simde_svsub_n_f64_m(simde_svbool_t pg, simde_svfloat64_t op1, simde_float64 op2) {
  #if defined(SIMDE_ARM_SVE_NATIVE)
    return svsub_n_f64_m(pg, op1, op2);
  #else
    return simde_svsub_f64_m(pg, op1, simde_svdup_n_f64(op2));
  #endif
}
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef simde_svsub_n_f64_m
  #define svsub_n_f64_m(pg, op1, op2) simde_svsub_n_f64_m(pg, op1, op2)
#endif

#if defined(__cplusplus)
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svsub_x(simde_svbool_t pg,    simde_svint8_t op1,    simde_svint8_t op2) { return simde_svsub_s8_x   (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svsub_x(simde_svbool_t pg,   simde_svint16_t op1,   simde_svint16_t op2) { return simde_svsub_s16_x  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svsub_x(simde_svbool_t pg,   simde_svint32_t op1,   simde_svint32_t op2) { return simde_svsub_s32_x  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svsub_x(simde_svbool_t pg,   simde_svint64_t op1,   simde_svint64_t op2) { return simde_svsub_s64_x  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svsub_x(simde_svbool_t pg,   simde_svuint8_t op1,   simde_svuint8_t op2) { return simde_svsub_u8_x   (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svsub_x(simde_svbool_t pg,  simde_svuint16_t op1,  simde_svuint16_t op2) { return simde_svsub_u16_x  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svsub_x(simde_svbool_t pg,  simde_svuint32_t op1,  simde_svuint32_t op2) { return simde_svsub_u32_x  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svsub_x(simde_svbool_t pg,  simde_svuint64_t op1,  simde_svuint64_t op2) { return simde_svsub_u64_x  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svsub_x(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) { return simde_svsub_f32_x  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svsub_x(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) { return simde_svsub_f64_x  (pg, op1, op2); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svsub_z(simde_svbool_t pg,    simde_svint8_t op1,    simde_svint8_t op2) { return simde_svsub_s8_z   (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svsub_z(simde_svbool_t pg,   simde_svint16_t op1,   simde_svint16_t op2) { return simde_svsub_s16_z  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svsub_z(simde_svbool_t pg,   simde_svint32_t op1,   simde_svint32_t op2) { return simde_svsub_s32_z  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svsub_z(simde_svbool_t pg,   simde_svint64_t op1,   simde_svint64_t op2) { return simde_svsub_s64_z  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svsub_z(simde_svbool_t pg,   simde_svuint8_t op1,   simde_svuint8_t op2) { return simde_svsub_u8_z   (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svsub_z(simde_svbool_t pg,  simde_svuint16_t op1,  simde_svuint16_t op2) { return simde_svsub_u16_z  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svsub_z(simde_svbool_t pg,  simde_svuint32_t op1,  simde_svuint32_t op2) { return simde_svsub_u32_z  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svsub_z(simde_svbool_t pg,  simde_svuint64_t op1,  simde_svuint64_t op2) { return simde_svsub_u64_z  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svsub_z(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) { return simde_svsub_f32_z  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svsub_z(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) { return simde_svsub_f64_z  (pg, op1, op2); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svsub_m(simde_svbool_t pg,    simde_svint8_t op1,    simde_svint8_t op2) { return simde_svsub_s8_m   (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svsub_m(simde_svbool_t pg,   simde_svint16_t op1,   simde_svint16_t op2) { return simde_svsub_s16_m  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svsub_m(simde_svbool_t pg,   simde_svint32_t op1,   simde_svint32_t op2) { return simde_svsub_s32_m  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svsub_m(simde_svbool_t pg,   simde_svint64_t op1,   simde_svint64_t op2) { return simde_svsub_s64_m  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svsub_m(simde_svbool_t pg,   simde_svuint8_t op1,   simde_svuint8_t op2) { return simde_svsub_u8_m   (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svsub_m(simde_svbool_t pg,  simde_svuint16_t op1,  simde_svuint16_t op2) { return simde_svsub_u16_m  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svsub_m(simde_svbool_t pg,  simde_svuint32_t op1,  simde_svuint32_t op2) { return simde_svsub_u32_m  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svsub_m(simde_svbool_t pg,  simde_svuint64_t op1,  simde_svuint64_t op2) { return simde_svsub_u64_m  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svsub_m(simde_svbool_t pg, simde_svfloat32_t op1, simde_svfloat32_t op2) { return simde_svsub_f32_m  (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svsub_m(simde_svbool_t pg, simde_svfloat64_t op1, simde_svfloat64_t op2) { return simde_svsub_f64_m  (pg, op1, op2); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svsub_x(simde_svbool_t pg,    simde_svint8_t op1,            int8_t op2) { return simde_svsub_n_s8_x (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svsub_x(simde_svbool_t pg,   simde_svint16_t op1,           int16_t op2) { return simde_svsub_n_s16_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svsub_x(simde_svbool_t pg,   simde_svint32_t op1,           int32_t op2) { return simde_svsub_n_s32_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svsub_x(simde_svbool_t pg,   simde_svint64_t op1,           int64_t op2) { return simde_svsub_n_s64_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svsub_x(simde_svbool_t pg,   simde_svuint8_t op1,           uint8_t op2) { return simde_svsub_n_u8_x (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svsub_x(simde_svbool_t pg,  simde_svuint16_t op1,          uint16_t op2) { return simde_svsub_n_u16_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svsub_x(simde_svbool_t pg,  simde_svuint32_t op1,          uint32_t op2) { return simde_svsub_n_u32_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svsub_x(simde_svbool_t pg,  simde_svuint64_t op1,          uint64_t op2) { return simde_svsub_n_u64_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svsub_x(simde_svbool_t pg, simde_svfloat32_t op1,     simde_float32 op2) { return simde_svsub_n_f32_x(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svsub_x(simde_svbool_t pg, simde_svfloat64_t op1,     simde_float64 op2) { return simde_svsub_n_f64_x(pg, op1, op2); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svsub_z(simde_svbool_t pg,    simde_svint8_t op1,            int8_t op2) { return simde_svsub_n_s8_z (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svsub_z(simde_svbool_t pg,   simde_svint16_t op1,           int16_t op2) { return simde_svsub_n_s16_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svsub_z(simde_svbool_t pg,   simde_svint32_t op1,           int32_t op2) { return simde_svsub_n_s32_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svsub_z(simde_svbool_t pg,   simde_svint64_t op1,           int64_t op2) { return simde_svsub_n_s64_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svsub_z(simde_svbool_t pg,   simde_svuint8_t op1,           uint8_t op2) { return simde_svsub_n_u8_z (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svsub_z(simde_svbool_t pg,  simde_svuint16_t op1,          uint16_t op2) { return simde_svsub_n_u16_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svsub_z(simde_svbool_t pg,  simde_svuint32_t op1,          uint32_t op2) { return simde_svsub_n_u32_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svsub_z(simde_svbool_t pg,  simde_svuint64_t op1,          uint64_t op2) { return simde_svsub_n_u64_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svsub_z(simde_svbool_t pg, simde_svfloat32_t op1,     simde_float32 op2) { return simde_svsub_n_f32_z(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svsub_z(simde_svbool_t pg, simde_svfloat64_t op1,     simde_float64 op2) { return simde_svsub_n_f64_z(pg, op1, op2); }

  SIMDE_FUNCTION_ATTRIBUTES    simde_svint8_t simde_svsub_m(simde_svbool_t pg,    simde_svint8_t op1,            int8_t op2) { return simde_svsub_n_s8_m (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint16_t simde_svsub_m(simde_svbool_t pg,   simde_svint16_t op1,           int16_t op2) { return simde_svsub_n_s16_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint32_t simde_svsub_m(simde_svbool_t pg,   simde_svint32_t op1,           int32_t op2) { return simde_svsub_n_s32_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svint64_t simde_svsub_m(simde_svbool_t pg,   simde_svint64_t op1,           int64_t op2) { return simde_svsub_n_s64_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint8_t simde_svsub_m(simde_svbool_t pg,   simde_svuint8_t op1,           uint8_t op2) { return simde_svsub_n_u8_m (pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint16_t simde_svsub_m(simde_svbool_t pg,  simde_svuint16_t op1,          uint16_t op2) { return simde_svsub_n_u16_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint32_t simde_svsub_m(simde_svbool_t pg,  simde_svuint32_t op1,          uint32_t op2) { return simde_svsub_n_u32_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svuint64_t simde_svsub_m(simde_svbool_t pg,  simde_svuint64_t op1,          uint64_t op2) { return simde_svsub_n_u64_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat32_t simde_svsub_m(simde_svbool_t pg, simde_svfloat32_t op1,     simde_float32 op2) { return simde_svsub_n_f32_m(pg, op1, op2); }
  SIMDE_FUNCTION_ATTRIBUTES simde_svfloat64_t simde_svsub_m(simde_svbool_t pg, simde_svfloat64_t op1,     simde_float64 op2) { return simde_svsub_n_f64_m(pg, op1, op2); }
#elif defined(SIMDE_GENERIC_)
  #define simde_svsub_x(pg, op1, op2) \
    (SIMDE_GENERIC_((op2), \
         simde_svint8_t: simde_svsub_s8_x, \
        simde_svint16_t: simde_svsub_s16_x, \
        simde_svint32_t: simde_svsub_s32_x, \
        simde_svint64_t: simde_svsub_s64_x, \
        simde_svuint8_t: simde_svsub_u8_x, \
       simde_svuint16_t: simde_svsub_u16_x, \
       simde_svuint32_t: simde_svsub_u32_x, \
       simde_svuint64_t: simde_svsub_u64_x, \
      simde_svfloat32_t: simde_svsub_f32_x, \
      simde_svfloat64_t: simde_svsub_f64_x, \
                 int8_t: simde_svsub_n_s8_x, \
                int16_t: simde_svsub_n_s16_x, \
                int32_t: simde_svsub_n_s32_x, \
                int64_t: simde_svsub_n_s64_x, \
                uint8_t: simde_svsub_n_u8_x, \
               uint16_t: simde_svsub_n_u16_x, \
               uint32_t: simde_svsub_n_u32_x, \
               uint64_t: simde_svsub_n_u64_x, \
          simde_float32: simde_svsub_n_f32_x, \
          simde_float64: simde_svsub_n_f64_x)((pg), (op1), (op2)))

  #define simde_svsub_z(pg, op1, op2) \
    (SIMDE_GENERIC_((op2), \
         simde_svint8_t: simde_svsub_s8_z, \
        simde_svint16_t: simde_svsub_s16_z, \
        simde_svint32_t: simde_svsub_s32_z, \
        simde_svint64_t: simde_svsub_s64_z, \
        simde_svuint8_t: simde_svsub_u8_z, \
       simde_svuint16_t: simde_svsub_u16_z, \
       simde_svuint32_t: simde_svsub_u32_z, \
       simde_svuint64_t: simde_svsub_u64_z, \
      simde_svfloat32_t: simde_svsub_f32_z, \
      simde_svfloat64_t: simde_svsub_f64_z, \
                 int8_t: simde_svsub_n_s8_z, \
                int16_t: simde_svsub_n_s16_z, \
                int32_t: simde_svsub_n_s32_z, \
                int64_t: simde_svsub_n_s64_z, \
                uint8_t: simde_svsub_n_u8_z, \
               uint16_t: simde_svsub_n_u16_z, \
               uint32_t: simde_svsub_n_u32_z, \
               uint64_t: simde_svsub_n_u64_z, \
          simde_float32: simde_svsub_n_f32_z, \
          simde_float64: simde_svsub_n_f64_z)((pg), (op1), (op2)))

  #define simde_svsub_m(pg, op1, op2) \
    (SIMDE_GENERIC_((op2), \
         simde_svint8_t: simde_svsub_s8_m, \
        simde_svint16_t: simde_svsub_s16_m, \
        simde_svint32_t: simde_svsub_s32_m, \
        simde_svint64_t: simde_svsub_s64_m, \
        simde_svuint8_t: simde_svsub_u8_m, \
       simde_svuint16_t: simde_svsub_u16_m, \
       simde_svuint32_t: simde_svsub_u32_m, \
       simde_svuint64_t: simde_svsub_u64_m, \
      simde_svfloat32_t: simde_svsub_f32_m, \
      simde_svfloat64_t: simde_svsub_f64_m, \
                 int8_t: simde_svsub_n_s8_m, \
                int16_t: simde_svsub_n_s16_m, \
                int32_t: simde_svsub_n_s32_m, \
                int64_t: simde_svsub_n_s64_m, \
                uint8_t: simde_svsub_n_u8_m, \
               uint16_t: simde_svsub_n_u16_m, \
               uint32_t: simde_svsub_n_u32_m, \
               uint64_t: simde_svsub_n_u64_m, \
          simde_float32: simde_svsub_n_f32_m, \
          simde_float64: simde_svsub_n_f64_m)((pg), (op1), (op2)))
#endif
#if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #undef svsub_x
  #undef svsub_z
  #undef svsub_m
  #undef svsub_n_x
  #undef svsub_n_z
  #undef svsub_n_m
  #define svsub_x(pg, op1, op2) simde_svsub_x((pg), (op1), (op2))
  #define svsub_z(pg, op1, op2) simde_svsub_z((pg), (op1), (op2))
  #define svsub_m(pg, op1, op2) simde_svsub_m((pg), (op1), (op2))
  #define svsub_n_x(pg, op1, op2) simde_svsub_n_x((pg), (op1), (op2))
  #define svsub_n_z(pg, op1, op2) simde_svsub_n_z((pg), (op1), (op2))
  #define svsub_n_m(pg, op1, op2) simde_svsub_n_m((pg), (op1), (op2))
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_SUB_H */
/* :: End simde/arm/sve/sub.h :: */
/* AUTOMATICALLY GENERATED FILE, DO NOT MODIFY */
/* 71fd833d9666141edcd1d3c109a80e228303d8d7 */

#endif /* SIMDE_ARM_SVE_H */
/* :: End simde/arm/sve.h :: */
