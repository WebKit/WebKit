/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>
#include <wtf/PrintStream.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

namespace JSC {

class ExecState;
class JSArray;
class JSObject;
class LLIntOffsetsExtractor;
class SlotVisitor;
class VM;

typedef ExecState CallFrame;

// ShadowChicken is a log that can be used to produce a shadow stack of CHICKEN-style stack frames.
// This enables the debugger to almost always see the tail-deleted stack frames, so long as we have
// memory inside ShadowChicken to remember them.
//
// The ShadowChicken log comprises packets that have one of two shapes:
//
// Prologue Packet, which has:
//     - Callee object.
//     - Frame pointer.
//     - Caller frame pointer.
//
// Tail Call Packet, which has just:
//     - Frame pointer.
//
// Prologue Packets are placed into the log in any JS function's prologue. Tail Call Packets are
// placed into the log just before making a proper tail call. We never log returns, since that would
// require a lot of infrastructure (unwinding, multiple ways of returning, etc). We don't need to
// see the returns because the prologue packets have a frame pointer. The tail call packets tell us
// when there was a tail call, and record the FP *before* the tail call.
//
// At any time it is possible to construct a shadow stack from the log and the actual machine stack.

class ShadowChicken {
    WTF_MAKE_NONCOPYABLE(ShadowChicken);
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct Packet {
        Packet()
        {
        }
        
        static const unsigned unlikelyValue = 0x7a11;
        
        static JSObject* tailMarker()
        {
            return bitwise_cast<JSObject*>(static_cast<intptr_t>(unlikelyValue));
        }
        
        static JSObject* throwMarker()
        {
            return bitwise_cast<JSObject*>(static_cast<intptr_t>(unlikelyValue + 1));
        }
        
        static Packet prologue(JSObject* callee, CallFrame* frame, CallFrame* callerFrame)
        {
            Packet result;
            result.callee = callee;
            result.frame = frame;
            result.callerFrame = callerFrame;
            return result;
        }
        
        // FIXME: Tail packets should hold currentScope so that the inspector can look at local
        // variables in tail-deleted frames.
        // https://bugs.webkit.org/show_bug.cgi?id=155722
        static Packet tail(CallFrame* frame)
        {
            Packet result;
            result.callee = tailMarker();
            result.frame = frame;
            return result;
        }
        
        static Packet throwPacket()
        {
            Packet result;
            result.callee = throwMarker();
            return result;
        }
        
        explicit operator bool() const { return !!callee; }
        
        bool isPrologue() const { return *this && callee != tailMarker() && callee != throwMarker(); }
        bool isTail() const { return *this && callee == tailMarker(); }
        bool isThrow() const { return *this && callee == throwMarker(); }
        
        void dump(PrintStream&) const;
        
        JSObject* callee { nullptr };
        CallFrame* frame { nullptr };
        CallFrame* callerFrame { nullptr };
    };
    
    struct Frame {
        Frame()
        {
        }
        
        Frame(JSObject* callee, CallFrame* frame, bool isTailDeleted)
            : callee(callee)
            , frame(frame)
            , isTailDeleted(isTailDeleted)
        {
        }
        
        bool operator==(const Frame& other) const
        {
            return callee == other.callee
                && frame == other.frame
                && isTailDeleted == other.isTailDeleted;
        }
        
        bool operator!=(const Frame& other) const
        {
            return !(*this == other);
        }
        
        void dump(PrintStream&) const;
        
        // FIXME: This should be able to hold the moral equivalent of StackVisitor::Frame, so that
        // we can support inlining.
        // https://bugs.webkit.org/show_bug.cgi?id=155686
        JSObject* callee { nullptr };
        CallFrame* frame { nullptr };
        bool isTailDeleted { false };
    };
    
    ShadowChicken();
    ~ShadowChicken();
    
    void log(VM& vm, ExecState* exec, const Packet&);
    
    void update(VM&, ExecState*);
    
    // Expects this signature: (const Frame& frame) -> bool.
    // Note that this only works right with inlining disabled, but that's OK since for now we
    // disable inlining when the inspector is attached. It would be easy to make this work with
    // inlining, and would mostly require that we can request that StackVisitor doesn't skip tail
    // frames.
    template<typename Functor>
    void iterate(VM&, ExecState*, const Functor&);
    
    void visitChildren(SlotVisitor&);
    void reset();
    
    // JIT support.
    Packet* log() const { return m_log; }
    unsigned logSize() const { return m_logSize; }
    Packet** addressOfLogCursor() { return &m_logCursor; }
    Packet* logEnd() { return m_logEnd; }
    
    void dump(PrintStream&) const;
    
    JS_EXPORT_PRIVATE JSArray* functionsOnStack(ExecState*);

private:
    friend class LLIntOffsetsExtractor;
    
    Packet* m_log { nullptr };
    unsigned m_logSize { 0 };
    Packet* m_logCursor { nullptr };
    Packet* m_logEnd { nullptr };
    
    Vector<Frame> m_stack;
};

} // namespace JSC

