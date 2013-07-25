/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef StackIteratorPrivate_h
#define StackIteratorPrivate_h

namespace JSC {

class ExecState;
class JSFunction;
class VM;

typedef ExecState CallFrame;

class StackIterator {
public:
    class Frame;
    typedef bool (*FrameFilter)(Frame*);

    Frame& operator*() { return *m_frame; }
    ALWAYS_INLINE Frame* operator->() { return m_frame; }

    bool operator==(Frame* frame) { return m_frame == frame; }
    bool operator!=(Frame* frame) { return m_frame != frame; }
    void operator++() { gotoNextFrame(); }
    void find(JSFunction*);

private:
    JS_EXPORT_PRIVATE StackIterator(CallFrame* startFrame, FrameFilter = nullptr);

    static Frame* end() { return nullptr; }
    JS_EXPORT_PRIVATE void gotoNextFrame();

    Frame* m_frame;
    FrameFilter m_filter;

    friend class ExecState;
};

} // namespace JSC

#endif // StackIteratorPrivate_h

