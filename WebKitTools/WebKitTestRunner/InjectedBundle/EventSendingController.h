/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef EventSendingController_h
#define EventSendingController_h

#include "JSWrappable.h"
#include <wtf/PassRefPtr.h>

namespace WTR {

class EventSendingController : public JSWrappable {
public:
    static PassRefPtr<EventSendingController> create();
    virtual ~EventSendingController();

    void makeWindowObject(JSContextRef, JSObjectRef windowObject, JSValueRef* exception);

    // JSWrappable
    virtual JSClassRef wrapperClass();

    void mouseDown(JSContextRef context, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    void mouseUp(JSContextRef context, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    void mouseMoveTo(JSContextRef context, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    void keyDown(JSContextRef context, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    void contextClick(JSContextRef context, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    void leapForward(JSContextRef context, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

    // Zoom functions.
    void textZoomIn();
    void textZoomOut();
    void zoomPageIn();
    void zoomPageOut();

private:
    EventSendingController();
};

} // namespace WTR

#endif // EventSendingController_h
