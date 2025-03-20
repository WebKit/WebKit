/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "JSCJSValue.h"
#include "Microtask.h"
#include "SlotVisitorMacros.h"
#include <wtf/Deque.h>
#include <wtf/SentinelLinkedList.h>

namespace JSC {

class JSGlobalObject;
class MarkedMicrotaskDeque;
class MicrotaskDispatcher;
class MicrotaskQueue;
class VM;

class QueuedTask {
    WTF_MAKE_TZONE_ALLOCATED(QueuedTask);
    friend class MicrotaskQueue;
    friend class MarkedMicrotaskDeque;
public:
    static constexpr unsigned maxArguments = 4;

    enum class Result : uint8_t {
        Executed,
        Discard,
        Suspended,
    };

    QueuedTask(RefPtr<MicrotaskDispatcher>&& dispatcher)
        : m_dispatcher(WTFMove(dispatcher))
        , m_identifier(MicrotaskIdentifier::generate())
    {
    }

    QueuedTask(RefPtr<MicrotaskDispatcher>&& dispatcher, JSGlobalObject* globalObject, JSValue job, JSValue argument0, JSValue argument1, JSValue argument2, JSValue argument3)
        : m_dispatcher(WTFMove(dispatcher))
        , m_identifier(MicrotaskIdentifier::generate())
        , m_globalObject(globalObject)
        , m_job(job)
        , m_arguments { argument0, argument1, argument2, argument3 }
    {
    }

    void setDispatcher(RefPtr<MicrotaskDispatcher>&& dispatcher)
    {
        m_dispatcher = WTFMove(dispatcher);
    }

    bool isRunnable() const;

    MicrotaskDispatcher* dispatcher() const { return m_dispatcher.get(); }
    MicrotaskIdentifier identifier() const { return m_identifier; }
    JSGlobalObject* globalObject() const { return m_globalObject; }
    JSValue job() const { return m_job; }
    std::span<const JSValue> arguments() const { return std::span { m_arguments }; }

private:
    RefPtr<MicrotaskDispatcher> m_dispatcher;
    MicrotaskIdentifier m_identifier;
    JSGlobalObject* m_globalObject { nullptr };
    JSValue m_job { };
    JSValue m_arguments[maxArguments] { };
};

class MicrotaskDispatcher : public RefCounted<MicrotaskDispatcher> {
public:
    enum class Type : uint8_t {
        None,
        // WebCoreMicrotaskDispatcher starts from here.
        JavaScript,
        UserGestureIndicator,
        Function,
    };

    explicit MicrotaskDispatcher(Type type)
        : m_type(type)
    { }

    virtual ~MicrotaskDispatcher() = default;
    virtual QueuedTask::Result run(QueuedTask&) = 0;
    virtual bool isRunnable() const = 0;
    Type type() const { return m_type; }
    bool isWebCoreMicrotaskDispatcher() const { return static_cast<uint8_t>(m_type) >= static_cast<uint8_t>(Type::JavaScript); }

private:
    Type m_type { Type::None };
};

class MarkedMicrotaskDeque {
public:
    friend class MicrotaskQueue;

    MarkedMicrotaskDeque() = default;

    QueuedTask dequeue()
    {
        if (m_markedBefore)
            --m_markedBefore;
        return m_queue.takeFirst();
    }

    void enqueue(QueuedTask&& task)
    {
        m_queue.append(WTFMove(task));
    }

    bool isEmpty() const
    {
        return m_queue.isEmpty();
    }

    size_t size() const { return m_queue.size(); }

    void clear()
    {
        m_queue.clear();
        m_markedBefore = 0;
    }

    void beginMarking()
    {
        m_markedBefore = 0;
    }

    void swap(MarkedMicrotaskDeque& other)
    {
        m_queue.swap(other.m_queue);
        std::swap(m_markedBefore, other.m_markedBefore);
    }

    JS_EXPORT_PRIVATE bool hasMicrotasksForFullyActiveDocument() const;

    DECLARE_VISIT_AGGREGATE;

private:
    Deque<QueuedTask> m_queue;
    size_t m_markedBefore { 0 };
};

class MicrotaskQueue final : public BasicRawSentinelNode<MicrotaskQueue> {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(MicrotaskQueue, JS_EXPORT_PRIVATE);
    WTF_MAKE_NONCOPYABLE(MicrotaskQueue);
public:
    JS_EXPORT_PRIVATE MicrotaskQueue(VM&);
    JS_EXPORT_PRIVATE ~MicrotaskQueue();

    JS_EXPORT_PRIVATE void enqueue(QueuedTask&&);

    bool isEmpty() const
    {
        return m_queue.isEmpty();
    }

    size_t size() const { return m_queue.size(); }

    void clear()
    {
        m_queue.clear();
        m_toKeep.clear();
    }

    void beginMarking()
    {
        m_queue.beginMarking();
        m_toKeep.beginMarking();
    }

    DECLARE_VISIT_AGGREGATE;

    inline void performMicrotaskCheckpoint(VM&, NOESCAPE const Invocable<QueuedTask::Result(QueuedTask&)> auto& functor);

    bool hasMicrotasksForFullyActiveDocument() const
    {
        return m_queue.hasMicrotasksForFullyActiveDocument();
    }

private:
    MarkedMicrotaskDeque m_queue;
    MarkedMicrotaskDeque m_toKeep;
};

} // namespace JSC
