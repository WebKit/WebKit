/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef V8DOMMap_h
#define V8DOMMap_h

#include "dom_wrapper_map.h"

#include <v8.h>

namespace WebCore {
    class Node;
#if ENABLE(SVG)
    class SVGElementInstance;
#endif

    // Callback when JS wrapper of active DOM object is dead.
    void weakActiveDOMObjectCallback(v8::Persistent<v8::Value> v8Object, void* domObject);

    // A map from DOM node to its JS wrapper.
    DOMWrapperMap<Node>& getDOMNodeMap();

    // A map from a DOM object (non-node) to its JS wrapper. This map does not contain the DOM objects which can have pending activity (active dom objects).
    DOMWrapperMap<void>& getDOMObjectMap();

    // A map from a DOM object to its JS wrapper for DOM objects which can have pending activity.
    DOMWrapperMap<void>& getActiveDOMObjectMap();

#if ENABLE(SVG)
    // A map for SVGElementInstances to its JS wrapper.
    DOMWrapperMap<SVGElementInstance>& getDOMSVGElementInstanceMap();

    // Map of SVG objects with contexts to V8 objects.
    DOMWrapperMap<void>& getDOMSVGObjectWithContextMap();
#endif
} // namespace WebCore

#endif // V8DOMMap_h
