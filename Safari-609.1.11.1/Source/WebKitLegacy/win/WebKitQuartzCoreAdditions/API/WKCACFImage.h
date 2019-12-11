/*
* Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#include "WebKitQuartzCoreAdditionsBase.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Returns the type identifier of all WKCACFImage instances. */
WKQCA_EXPORT CFTypeID WKCACFImageGetTypeID(void);

/* Return the dimensions of the image. */
WKQCA_EXPORT size_t WKCACFImageGetWidth(WKCACFImageRef);
WKQCA_EXPORT size_t WKCACFImageGetHeight(WKCACFImageRef);

/* Returns a file mapping object which contains the bits of the image. (Note that this does not
 * copy the bits; the returned file mapping object points to the same underlying bits that the
 * image itself uses.) The bits are in the format of a top-down, 32-bit DIB. It is the caller's
 * responsibility to destroy the file mapping via ::CloseHandle. */
WKQCA_EXPORT HANDLE WKCACFImageCopyFileMapping(WKCACFImageRef, size_t* fileMappingSize);

#ifdef __cplusplus
}
#endif
