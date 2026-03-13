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

#pragma once

#include <wtf/CheckedPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class LocalFrameView;

class LocalFrameViewDestructionObserver : public CanMakeSingleThreadWeakPtr<LocalFrameViewDestructionObserver>, public CanMakeCheckedPtr<LocalFrameViewDestructionObserver> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(LocalFrameViewDestructionObserver);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(LocalFrameViewDestructionObserver);
public:
    LocalFrameViewDestructionObserver(LocalFrameView&);

    virtual ~LocalFrameViewDestructionObserver();
    virtual void frameViewIsBeingDestroyed() = 0;

    LocalFrameView* localFrameView() { return m_frameView.get(); }
    LocalFrameView* localFrameView() const { return m_frameView.get(); }

    void observeFrameView(LocalFrameView&);
    void unobserveFrameView();

private:
    CheckedPtr<LocalFrameView> m_frameView;
};

} // namespace WebCore
