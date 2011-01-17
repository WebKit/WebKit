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

#ifndef WebAccessibilityCache_h
#define WebAccessibilityCache_h

#include "WebCommon.h"

namespace WebKit {

class WebView;
class WebAccessibilityObject;

class WebAccessibilityCache {
public:
    WebAccessibilityCache() {}
    virtual ~WebAccessibilityCache() {}

    WEBKIT_API static WebAccessibilityCache* create();
    WEBKIT_API static void enableAccessibility();
    WEBKIT_API static bool accessibilityEnabled();

    virtual void initialize(WebView* view) = 0;
    virtual bool isInitialized() const = 0;

    virtual WebAccessibilityObject getObjectById(int) = 0;
    virtual int addOrGetId(const WebAccessibilityObject& object) = 0;
    virtual bool isCached(const WebAccessibilityObject&) = 0;

    virtual void remove(int) = 0;
    virtual void clear() = 0;
};

} // namespace WebKit

#endif
