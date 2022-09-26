/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TestsController.h"
#include <dlfcn.h>
#include <process-initialization/nk-testwebkitapi.h>

static void loadLibraryOrExit(const char* name)
{
    if (!dlopen(name, RTLD_NOW)) {
        fprintf(stderr, "Failed to load %s.\n", name);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char** argv)
{
    loadLibraryOrExit(ICU_LOAD_AT);
#if defined(BUILDING_TestWebCore) || defined(BUILDING_TestWebKit)
    loadLibraryOrExit(PNG_LOAD_AT);
#if defined(JPEG_LOAD_AT)
    loadLibraryOrExit(JPEG_LOAD_AT);
#endif 
#if defined(WebP_LOAD_AT)
    loadLibraryOrExit(WebP_LOAD_AT);
#endif
    loadLibraryOrExit(Fontconfig_LOAD_AT);
    loadLibraryOrExit(Freetype_LOAD_AT);
    loadLibraryOrExit(HarfBuzz_LOAD_AT);
    loadLibraryOrExit(Cairo_LOAD_AT);
    loadLibraryOrExit(WebKitRequirements_LOAD_AT);
#endif
#if defined(BUILDING_TestWebCore) || defined(BUILDING_TestWebKit) || defined(BUILDING_TestJavaScriptCore)
#if !ENABLE(STATIC_JSC)
    loadLibraryOrExit("libJavaScriptCore");
#endif
#endif
#if defined(BUILDING_TestWebKit)
    loadLibraryOrExit(OpenSSL_LOAD_AT);
    loadLibraryOrExit(CURL_LOAD_AT);
    loadLibraryOrExit("libWebKit");
#endif
    return TestWebKitAPI::TestsController::singleton().run(argc, argv) ? EXIT_SUCCESS : EXIT_FAILURE;
}
