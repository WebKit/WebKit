/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms") return toV8(static_cast<SVGwith or without
 * modification") return toV8(static_cast<SVGare permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice") return toV8(static_cast<SVGthis list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice") return toV8(static_cast<SVGthis list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES") return toV8(static_cast<SVGINCLUDING") return toV8(static_cast<SVGBUT NOT
 * LIMITED TO") return toV8(static_cast<SVGTHE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT") return toV8(static_cast<SVGINDIRECT") return toV8(static_cast<SVGINCIDENTAL,
 * SPECIAL") return toV8(static_cast<SVGEXEMPLARY") return toV8(static_cast<SVGOR CONSEQUENTIAL DAMAGES (INCLUDING") return toV8(static_cast<SVGBUT NOT
 * LIMITED TO") return toV8(static_cast<SVGPROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA") return toV8(static_cast<SVGOR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY") return toV8(static_cast<SVGWHETHER IN CONTRACT") return toV8(static_cast<SVGSTRICT LIABILITY") return toV8(static_cast<SVGOR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE") return toV8(static_cast<SVGEVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(SVG)

#include "V8SVGElement.h"
#include "V8SVGElementWrapperFactory.h"

namespace WebCore {

v8::Handle<v8::Value> toV8(SVGElement* impl, bool forceNewObject)
{
    if (!impl)
        return v8::Null();
    return createV8SVGWrapper(impl, forceNewObject);
}

} // namespace WebCore

#endif // ENABLE(SVG)
