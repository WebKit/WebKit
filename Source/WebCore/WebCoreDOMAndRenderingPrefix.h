/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

/* On Apple, chained on top of WebCorePrefix.h (base PCH supplies Platform.h).
   On non-Apple, compiled as a standalone PCH — include Platform.h directly so
   macros like CPU() are available to all dom/html/page/rendering TUs. */

#if defined(HAVE_CONFIG_H) && HAVE_CONFIG_H && defined(BUILDING_WITH_CMAKE)
#include "cmakeconfig.h"
#endif

#include <wtf/Platform.h>

#if !OS(DARWIN)
// On non-Apple, PCH chaining is not used; include the base prefix directly so
// this PCH is self-contained and all required types and macros are available.
#include "WebCorePrefix.h"
#endif

#ifdef __cplusplus
#undef new
#undef delete

#include <JavaScriptCore/CallFrame.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/Strong.h>
#include <JavaScriptCore/VM.h>

#include <pal/SessionID.h>
#include <pal/text/TextEncoding.h>
#include <vector>
#include <wtf/BitVector.h>
#include <wtf/JSONValues.h>
#include <wtf/SegmentedVector.h>
#include <wtf/WeakHashCountedSet.h>
#include <wtf/WeakListHashSet.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/text/AdaptiveStringSearcher.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>
#if PLATFORM(COCOA)
#include <wtf/spi/cocoa/IOSurfaceSPI.h>
#endif

#include "AffineTransform.h"
#include "CSSNumericValue.h"
#include "CSSPropertyNames.h"
#include "CSSValue.h"
#include "CSSValueKeywords.h"
#include "CachedResource.h"
#include "Document.h"
#include "Event.h"
#include "HTMLElement.h"
#include "HTMLElementTypeHelpers.h"
#include "HTMLNames.h"
#include "Image.h"
#include "LocalFrame.h"
#include "NodeInlines.h"
#include "Path.h"
#include "Region.h"
#include "RenderBox.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SimpleRange.h"
#include "TextFlags.h"
#include "WebAnimationTime.h"

#define new ("if you use new/delete make sure to include config.h at the top of the file"())
#define delete ("if you use new/delete make sure to include config.h at the top of the file"())
#endif
