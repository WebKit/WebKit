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

#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/Microtask.h>
#include <JavaScriptCore/SlotVisitorMacros.h>
#include <wtf/CompactRefPtrTuple.h>
#include <wtf/Compiler.h>
#include <wtf/Deque.h>
#include <wtf/SentinelLinkedList.h>
#include <wtf/TZoneMalloc.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

class JSGlobalObject;
class MarkedMicrotaskDeque;
class MicrotaskDispatcher;
class MicrotaskQueue;
class QueuedTask;
class VM;

enum class QueuedTaskResult : uint8_t {
    Executed,
    Discard,
    Suspended,
};

class MicrotaskDispatcher : public RefCounted<MicrotaskDispatcher> {
    WTF_MAKE_COMPACT_TZONE_ALLOCATED(MicrotaskDispatcher);
public:
    enum class Type : uint8_t {
        None,
        JSCDebuggable,
        // WebCoreMicrotaskDispatcher starts from here.
        WebCoreJS,
        WebCoreJSDebuggable,
        WebCoreUserGestureIndicator,
        WebCoreFunction,
    };

    explicit MicrotaskDispatcher(Type type)
        : m_type(type)
    { }

    virtual ~MicrotaskDispatcher() = default;
    virtual QueuedTaskResult run(QueuedTask&) = 0;
    virtual bool isRunnable() const = 0;
    Type type() const { return m_type; }
    bool isWebCoreMicrotaskDispatcher() const { return static_cast<uint8_t>(m_type) >= static_cast<uint8_t>(Type::WebCoreJS); }

private:
    Type m_type { Type::None };
};

class DebuggableMicrotaskDispatcher final : public MicrotaskDispatcher {
    WTF_MAKE_COMPACT_TZONE_ALLOCATED(DebuggableMicrotaskDispatcher);
public:
    explicit DebuggableMicrotaskDispatcher()
        : MicrotaskDispatcher(Type::JSCDebuggable)
    { }

    static Ref<DebuggableMicrotaskDispatcher> create()
    {
        return adoptRef(*new DebuggableMicrotaskDispatcher());
    }

    QueuedTaskResult run(QueuedTask&) final;
    bool isRunnable() const final;
};

class QueuedTask {
    WTF_MAKE_TZONE_ALLOCATED(QueuedTask);
    friend class MicrotaskQueue;
    friend class MarkedMicrotaskDeque;
public:
    static constexpr unsigned maxArguments = maxMicrotaskArguments;
    using Result = QueuedTaskResult;

    QueuedTask(Ref<MicrotaskDispatcher>&& dispatcher)
        : m_dispatcher(WTF::move(dispatcher), static_cast<uint16_t>(InternalMicrotask::Opaque))
        , m_globalObject(nullptr)
    {
    }

    template<typename... Args>
    requires (sizeof...(Args) <= maxArguments) && (std::is_convertible_v<Args, JSValue> && ...)
    QueuedTask(RefPtr<MicrotaskDispatcher>&& dispatcher, InternalMicrotask job, uint8_t payload, JSGlobalObject* globalObject, Args&&...args)
        : m_dispatcher(WTF::move(dispatcher), static_cast<uint16_t>(job) | (static_cast<uint16_t>(payload) << 8))
        , m_globalObject(globalObject)
        , m_arguments { std::forward<Args>(args)... }
    {
    }

    void setDispatcher(RefPtr<MicrotaskDispatcher>&& dispatcher)
    {
        m_dispatcher.setPointer(WTF::move(dispatcher));
    }

    bool isRunnable() const;

    MicrotaskDispatcher* dispatcher() const { return m_dispatcher.pointer(); }
    std::optional<MicrotaskIdentifier> identifier() const
    {
        auto* pointer = m_dispatcher.pointer();
        if (!pointer)
            return std::nullopt;
        return MicrotaskIdentifier { std::bit_cast<uintptr_t>(pointer) };
    }
    JSGlobalObject* globalObject() const { return m_globalObject; }
    InternalMicrotask job() const { return static_cast<InternalMicrotask>(static_cast<uint8_t>(m_dispatcher.type())); }
    // Task-specific metadata stored in upper 8 bits of m_dispatcher's type field.
    // Typically holds JSPromise::Status or a nested InternalMicrotask value.
    uint8_t payload() const { return static_cast<uint8_t>(m_dispatcher.type() >> 8); }
    std::span<const JSValue, maxArguments> arguments() const { return std::span<const JSValue, maxArguments> { m_arguments, maxArguments }; }

private:
    CompactRefPtrTuple<MicrotaskDispatcher, uint16_t> m_dispatcher;
    JSGlobalObject* m_globalObject;
    JSValue m_arguments[maxArguments] { };
};
static_assert(sizeof(QueuedTask) <= 40, "Size of QueuedTask is critical for performance");

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
        m_queue.append(WTF::move(task));
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
    bool tryMergePromiseReactionChain(VM&, QueuedTask& newTask);

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

    template<bool useCallOnEachMicrotask>
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

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
