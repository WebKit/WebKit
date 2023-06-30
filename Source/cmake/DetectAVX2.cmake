#################################
# Check for the presence of AVX2.
#
# Once done, this will define:
# - AVX2_SUPPORT_FOUND - the system supports (at least) AVX2.
#
# Copyright (c) 2023, Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
#   * Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#
#   * Redistributions in binary form must reproduce the above copyright notice,
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.
#
#   * Neither the name of the copyright holders nor the names of its contributors
#     may be used to endorse or promote products derived from this software without
#     specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
# SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
# TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
# WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

set(AVX2_SUPPORT_FOUND FALSE)

macro(CHECK_FOR_AVX2)
    include(CheckCXXSourceRuns)

    set(CMAKE_REQUIRED_FLAGS_SAVE ${CMAKE_REQUIRED_FLAGS})

    if (COMPILER_IS_GCC_OR_CLANG)
        set(CMAKE_REQUIRED_FLAGS -mavx2)
    endif ()

    if (MSVC AND CMAKE_CL_64)
        set(CMAKE_REQUIRED_FLAGS /arch:AVX2)
    endif ()

    check_cxx_source_runs("
        #include <immintrin.h>
        int main()
        {
          volatile __m256i a, b;
          a = _mm256_set1_epi8 (1);
          b = _mm256_add_epi8 (a,a);
          return 0;
        }"
        HAVE_AVX2_EXTENSIONS)

    set(CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS_SAVE})

    if (COMPILER_IS_GCC_OR_CLANG OR (MSVC AND CMAKE_CL_64))
        if (HAVE_AVX2_EXTENSIONS)
            set(AVX2_SUPPORT_FOUND TRUE)
        endif ()
    endif ()

endmacro(CHECK_FOR_AVX2)

CHECK_FOR_AVX2()
