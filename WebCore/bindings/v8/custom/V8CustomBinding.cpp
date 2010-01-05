/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "V8CustomBinding.h"

#include "CSSHelper.h"
#include "Element.h"
#include "Document.h"
#include "DOMWindow.h"
#include "History.h"
#include "HTMLNames.h"
#include "HTMLFrameElementBase.h"
#include "Location.h"
#include "V8Binding.h"
#include "V8BindingState.h"
#include "V8Proxy.h"

#if ENABLE(SVG)
#include "SVGPathSeg.h"
#endif

namespace WebCore {

// --------------- Security Checks -------------------------
INDEXED_ACCESS_CHECK(History)
{
    ASSERT(V8ClassIndex::FromInt(data->Int32Value()) == V8ClassIndex::HISTORY);
    // Only allow same origin access.
    History* history = V8DOMWrapper::convertToNativeObject<History>(V8ClassIndex::HISTORY, host);
    return V8BindingSecurity::canAccessFrame(V8BindingState::Only(), history->frame(), false);
}

NAMED_ACCESS_CHECK(History)
{
    ASSERT(V8ClassIndex::FromInt(data->Int32Value()) == V8ClassIndex::HISTORY);
    // Only allow same origin access.
    History* history = V8DOMWrapper::convertToNativeObject<History>(V8ClassIndex::HISTORY, host);
    return V8BindingSecurity::canAccessFrame(V8BindingState::Only(), history->frame(), false);
}

#undef INDEXED_ACCESS_CHECK
#undef NAMED_ACCESS_CHECK
#undef NAMED_PROPERTY_GETTER
#undef NAMED_PROPERTY_SETTER

Frame* V8Custom::GetTargetFrame(v8::Local<v8::Object> host, v8::Local<v8::Value> data)
{
    Frame* target = 0;
    switch (V8ClassIndex::FromInt(data->Int32Value())) {
    case V8ClassIndex::DOMWINDOW: {
        v8::Handle<v8::Object> window = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, host);
        if (window.IsEmpty())
            return target;

        DOMWindow* targetWindow = V8DOMWrapper::convertToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, window);
        target = targetWindow->frame();
        break;
    }
    case V8ClassIndex::LOCATION: {
        History* history = V8DOMWrapper::convertToNativeObject<History>(V8ClassIndex::HISTORY, host);
        target = history->frame();
        break;
    }
    case V8ClassIndex::HISTORY: {
        Location* location = V8DOMWrapper::convertToNativeObject<Location>(V8ClassIndex::LOCATION, host);
        target = location->frame();
        break;
    }
    default:
        break;
    }
    return target;
}

#if ENABLE(SVG)
V8ClassIndex::V8WrapperType V8Custom::DowncastSVGPathSeg(void* pathSeg)
{
    WebCore::SVGPathSeg* realPathSeg = reinterpret_cast<WebCore::SVGPathSeg*>(pathSeg);

    switch (realPathSeg->pathSegType()) {
#define MAKE_CASE(svgValue, v8Value) case WebCore::SVGPathSeg::svgValue: return V8ClassIndex::v8Value

    MAKE_CASE(PATHSEG_CLOSEPATH,                    SVGPATHSEGCLOSEPATH);
    MAKE_CASE(PATHSEG_MOVETO_ABS,                   SVGPATHSEGMOVETOABS);
    MAKE_CASE(PATHSEG_MOVETO_REL,                   SVGPATHSEGMOVETOREL);
    MAKE_CASE(PATHSEG_LINETO_ABS,                   SVGPATHSEGLINETOABS);
    MAKE_CASE(PATHSEG_LINETO_REL,                   SVGPATHSEGLINETOREL);
    MAKE_CASE(PATHSEG_CURVETO_CUBIC_ABS,            SVGPATHSEGCURVETOCUBICABS);
    MAKE_CASE(PATHSEG_CURVETO_CUBIC_REL,            SVGPATHSEGCURVETOCUBICREL);
    MAKE_CASE(PATHSEG_CURVETO_QUADRATIC_ABS,        SVGPATHSEGCURVETOQUADRATICABS);
    MAKE_CASE(PATHSEG_CURVETO_QUADRATIC_REL,        SVGPATHSEGCURVETOQUADRATICREL);
    MAKE_CASE(PATHSEG_ARC_ABS,                      SVGPATHSEGARCABS);
    MAKE_CASE(PATHSEG_ARC_REL,                      SVGPATHSEGARCREL);
    MAKE_CASE(PATHSEG_LINETO_HORIZONTAL_ABS,        SVGPATHSEGLINETOHORIZONTALABS);
    MAKE_CASE(PATHSEG_LINETO_HORIZONTAL_REL,        SVGPATHSEGLINETOHORIZONTALREL);
    MAKE_CASE(PATHSEG_LINETO_VERTICAL_ABS,          SVGPATHSEGLINETOVERTICALABS);
    MAKE_CASE(PATHSEG_LINETO_VERTICAL_REL,          SVGPATHSEGLINETOVERTICALREL);
    MAKE_CASE(PATHSEG_CURVETO_CUBIC_SMOOTH_ABS,     SVGPATHSEGCURVETOCUBICSMOOTHABS);
    MAKE_CASE(PATHSEG_CURVETO_CUBIC_SMOOTH_REL,     SVGPATHSEGCURVETOCUBICSMOOTHREL);
    MAKE_CASE(PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS, SVGPATHSEGCURVETOQUADRATICSMOOTHABS);
    MAKE_CASE(PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL, SVGPATHSEGCURVETOQUADRATICSMOOTHREL);

#undef MAKE_CASE

    default:
        return V8ClassIndex::INVALID_CLASS_INDEX;
    }
}

#endif // ENABLE(SVG)

} // namespace WebCore
