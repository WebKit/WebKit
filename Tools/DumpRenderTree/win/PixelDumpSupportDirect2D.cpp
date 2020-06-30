/*
* Copyright (C) 2018 Apple Inc. All rights reserved.
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
* THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
* PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
* OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"
#include "PixelDumpSupportDirect2D.h"

#include "DumpRenderTree.h"
#include "PixelDumpSupport.h"
#include <algorithm>
#include <ctype.h>
#include <d2d1.h>
#include <wtf/Assertions.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/StringExtras.h>


static const CFStringRef kUTTypePNG = CFSTR("public.png");

ID2D1Factory* pixelDumpSystemFactory()
{
    static ID2D1Factory* direct2DFactory = nullptr;
    if (!direct2DFactory) {
#ifndef NDEBUG
        D2D1_FACTORY_OPTIONS options = { };
        options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
        HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, options, &direct2DFactory);
#else
        HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, &direct2DFactory);
#endif
        RELEASE_ASSERT(SUCCEEDED(hr));
    }

    return direct2DFactory;
}

static void printPNG(ID2D1Bitmap* image, const char* checksum)
{
    UNUSED_PARAM(image);
    UNUSED_PARAM(checksum);
    // Not implemented.
}

void computeSHA1HashStringForBitmapContext(BitmapContext* context, char hashString[33])
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(hashString);
    // Not implemented.
}

void dumpBitmap(BitmapContext* context, const char* checksum)
{
    _com_ptr_t<_com_IIID<ID2D1Bitmap, &__uuidof(ID2D1Bitmap)>> image;
    HRESULT hr = context->platformContext()->GetBitmap(&image);
    if (!SUCCEEDED(hr))
        return;

    printPNG(image.GetInterfacePtr(), checksum);
}
