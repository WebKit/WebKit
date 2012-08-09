/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
 * Copyright (C) 2012 Company 100 Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef NativeImagePtr_h
#define NativeImagePtr_h

#if PLATFORM(WX)
class wxBitmap;
class wxGraphicsBitmap;
#elif USE(CG)
typedef struct CGImage* CGImageRef;
#elif PLATFORM(QT)
#include <qglobal.h>
QT_BEGIN_NAMESPACE
class QImage;
QT_END_NAMESPACE
#elif USE(CAIRO)
#include "NativeImageCairo.h"
#elif USE(SKIA)
namespace WebCore {
class NativeImageSkia;
}
#elif OS(WINCE)
#include "SharedBitmap.h"
#endif

namespace WebCore {

#if USE(CG)
typedef CGImageRef NativeImagePtr;
#elif PLATFORM(OPENVG)
class TiledImageOpenVG;
typedef TiledImageOpenVG* NativeImagePtr;
#elif PLATFORM(WX)
#if USE(WXGC)
typedef wxGraphicsBitmap* NativeImagePtr;
#else
typedef wxBitmap* NativeImagePtr;
#endif
#elif USE(CAIRO)
typedef WebCore::NativeImageCairo* NativeImagePtr;
#elif USE(SKIA)
typedef WebCore::NativeImageSkia* NativeImagePtr;
#elif OS(WINCE)
typedef RefPtr<SharedBitmap> NativeImagePtr;
#elif PLATFORM(BLACKBERRY)
typedef void* NativeImagePtr;
#elif PLATFORM(QT)
typedef QImage* NativeImagePtr;
#endif

}

#endif
