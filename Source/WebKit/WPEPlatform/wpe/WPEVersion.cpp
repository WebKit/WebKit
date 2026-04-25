/*
 * Copyright (C) 2023 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEVersion.h"

/**
 * WPEVersion:
 * @Short_description: Provides the WPE platform version
 * @Title: WPEVersion
 *
 * Provides convenience functions returning WPE's major, minor and
 * micro versions of the WPE platform library your code is running
 * against. This is not necessarily the same as the
 * #WPE_PLATFORM_MAJOR_VERSION, #WPE_PLATFORM_MINOR_VERSION or
 * #WPE_PLATFORM_MICRO_VERSION, which represent the version of the WPE platform
 * headers included when compiling the code.
 */

/**
 * wpe_platform_get_major_version:
 *
 * Returns the major version number of the WPE platform library.
 * (e.g. in WPEPlatform version 1.8.3 this is 1.)
 *
 * This function is in the library, so it represents the WPE platform library
 * your code is running against. Contrast with the #WPE_PLATFORM_MAJOR_VERSION
 * macro, which represents the major version of the WPE platform headers you
 * have included when compiling your code.
 *
 * Returns: the major version number of the WPE platform library
 */
guint wpe_platform_get_major_version(void)
{
    return WPE_PLATFORM_MAJOR_VERSION;
}

/**
 * wpe_platform_get_minor_version:
 *
 * Returns the minor version number of the WPE platform library.
 * (e.g. in WPEPlatform version 1.8.3 this is 8.)
 *
 * This function is in the library, so it represents the WPE platform library
 * your code is running against. Contrast with the #WPE_PLATFORM_MINOR_VERSION
 * macro, which represents the minor version of the WPE platform headers you
 * have included when compiling your code.
 *
 * Returns: the minor version number of the WPE platform library
 */
guint wpe_platform_get_minor_version(void)
{
    return WPE_PLATFORM_MINOR_VERSION;
}

/**
 * wpe_platform_get_micro_version:
 *
 * Returns the micro version number of the WPE platform library.
 * (e.g. in WPEPlatform version 1.8.3 this is 3.)
 *
 * This function is in the library, so it represents the WPE platform library
 * your code is running against. Contrast with the #WPE_PLATFORM_MICRO_VERSION
 * macro, which represents the micro version of the WPE platform headers you
 * have included when compiling your code.
 *
 * Returns: the micro version number of the WPE platform library
 */
guint wpe_platform_get_micro_version(void)
{
    return WPE_PLATFORM_MICRO_VERSION;
}
