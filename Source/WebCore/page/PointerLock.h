/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PointerLock_h
#define PointerLock_h

#if ENABLE(POINTER_LOCK)

#include "DOMWindowProperty.h"
#include "VoidCallback.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class Element;
class Frame;
class PointerLockController;

class PointerLock : public RefCounted<PointerLock>, public DOMWindowProperty {
public:
    static PassRefPtr<PointerLock> create(Frame* frame) { return adoptRef(new PointerLock(frame)); }

    ~PointerLock();

    // DOMWindowProperty Interface
    virtual void disconnectFrameForPageCache() OVERRIDE;
    virtual void willDestroyGlobalObjectInFrame() OVERRIDE;

    void lock(Element* target, PassRefPtr<VoidCallback> successCallback, PassRefPtr<VoidCallback> failureCallback);
    void unlock();
    bool isLocked();

private:
    explicit PointerLock(Frame*);

    PointerLockController* m_controller;
};

} // namespace WebCore

#endif // ENABLE(POINTER_LOCK)

#endif // PointerLock_h

