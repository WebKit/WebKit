/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(KEYBOARD_LOCK)

#include <wtf/CompletionHandler.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class Page;

class KeyboardLockController : public ThreadSafeRefCounted<KeyboardLockController> {
public:
    static KeyboardLockController& shared();
    WEBCORE_EXPORT static void setSharedController(Ref<KeyboardLockController>&&);

    virtual ~KeyboardLockController() = default;
    virtual void lock(const WeakPtr<Page>&, const Vector<String>&, DOMPromiseDeferred<void>&&) = 0;
    virtual void unlock(const WeakPtr<Page>&) = 0;
protected:
    KeyboardLockController() = default;
};

class DummyKeyboardLockController final : public KeyboardLockController {
public:
    static Ref<DummyKeyboardLockController> create() { return adoptRef(*new DummyKeyboardLockController); }
private:
    DummyKeyboardLockController() = default;
    void lock(const WeakPtr<Page>&, const Vector<String>&, DOMPromiseDeferred<void>&& promise) final { promise.resolve(); }
    void unlock(const WeakPtr<Page>&) final { }
};

} // namespace WebCore

#endif // ENABLE(KEYBOARD_LOCK)
