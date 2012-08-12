/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef FindInPageCoordinates_h
#define FindInPageCoordinates_h

#include "FloatRect.h"

namespace WebCore {
class Range;
class RenderObject;
}

namespace WebKit {

// Find-in-page coordinate conversion methods.
//
// This coordinate system is designed to give consistent tickmarks in cases where find matches
// are in scrollable areas but might not be visible (e.g. child frames, scroll:overflow).
// In these cases, using absolute positions might lead to tickmarks pointing outside the visible
// area of its container, which is counter-intuitive for users.
//
// Find-in-page coordinates are represented as normalized fractions of the main frame document with
// the property that they are built by composing the relative position of each renderer to the maximum
// effective layout size of its container all the way up the render tree. The resulting coordinates
// are scroll-independent, representing any contents scaled to the visible area of their container.
// The provided methods support scroll:overflow and are CSS position and transform-friendly.

WebCore::FloatRect findInPageRectFromAbsoluteRect(const WebCore::FloatRect&, const WebCore::RenderObject*);
WebCore::FloatRect findInPageRectFromRange(WebCore::Range*);

} // namespace WebKit

#endif
