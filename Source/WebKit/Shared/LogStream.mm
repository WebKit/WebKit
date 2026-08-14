/* Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "LogStream.h"

#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)

#import "AuxiliaryProcessProxy.h"
#import "LogStreamMessages.h"
#import "Logging.h"
#import "StreamConnectionWorkQueue.h"
#import "StreamServerConnection.h"
#import <wtf/NeverDestroyed.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/TZoneMallocInlines.h>

#if HAVE(OS_SIGNPOST)
#import <wtf/SystemTracing.h>
#endif

#define MESSAGE_CHECK(assertion, connection) MESSAGE_CHECK_BASE(assertion, connection)

namespace WebKit {

static std::atomic<unsigned> globalLogCountForTesting { 0 };

WTF_MAKE_TZONE_ALLOCATED_IMPL(LogStream);

void logWithProcessNamePrefix(os_log_t log, os_log_type_t type, ASCIILiteral processName, int pid, const char* message)
{
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    if (processName == "WebContent"_s)
        os_log_with_type(log, type, "WebContent[%d] %{public}s", pid, message); // NOLINT
    else if (processName == "Model"_s)
        os_log_with_type(log, type, "Model[%d] %{public}s", pid, message); // NOLINT
    else
        os_log_with_type(log, type, "%{public}s[%d] %{public}s", processName.characters(), pid, message); // NOLINT
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
// All LogStreams share a single work queue: the StreamServerConnection for each LogStream is opened on
// (and thus bound to) this queue, so all of its connection work -- including invalidate() -- must run
// on it. See rdar://182244946 and the comment in stopListeningForIPC().
static IPC::StreamConnectionWorkQueue& logWorkQueueSingleton()
{
    static NeverDestroyed<Ref<IPC::StreamConnectionWorkQueue>> queue = IPC::StreamConnectionWorkQueue::create("Log work queue"_s);
    return queue.get();
}
#endif

LogStream::LogStream(AuxiliaryProcessProxy& process, ASCIILiteral processName, Ref<ConnectionType>&& connection, LogStreamIdentifier identifier)
    : m_connection(WTF::move(connection))
#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
    , m_process(process)
#endif
    , m_identifier(identifier)
    , m_pid(process.processID())
    , m_processName(processName)
{
}

LogStream::~LogStream() = default;

void LogStream::stopListeningForIPC()
{
    assertIsMainRunLoop();
#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
    // Fully tear down the stream connection. stopReceivingMessages() clears the StreamServerConnection's
    // receiver map (breaking the m_receivers <-> m_connection retain cycle so the objects can be freed),
    // and invalidate() invalidates the underlying IPC::Connection so its Mach port / kqueue workloop is
    // released. Neither alone is sufficient; without this the connection leaked after the WebContent
    // process was gone, until the UI process was killed for resource exhaustion. See rdar://182244946.
    //
    // The connection was opened on the log work queue, so it is bound to that queue's dispatcher and its
    // teardown must run there too: IPC::Connection::invalidate() asserts it is called on its own
    // dispatcher (a release ASSERT_WITH_SECURITY_IMPLICATION). Dispatch the teardown onto the queue
    // rather than running it synchronously on the main thread. The queue is shared by every LogStream,
    // so we must not stop it (unlike single-owner StreamServerConnection users, which can block in
    // StreamConnectionWorkQueue::stopAndWaitForCompletion()); we only invalidate our own connection.
    //
    // We are typically the last reference holder (ScopedActiveMessageReceiveQueue drops its RefPtr as
    // soon as this returns), so capture Ref to both the LogStream and its connection: invalidate()
    // clears the connection's CheckedPtr back to us (the client), so the LogStream must outlive the
    // dispatched teardown.
    logWorkQueueSingleton().dispatch([protectedThis = Ref { *this }, connection = Ref { m_connection }, identifier = m_identifier] {
        connection->stopReceivingMessages(Messages::LogStream::messageReceiverName(), identifier.toUInt64());
        connection->invalidate();
    });
#endif
}

void LogStream::logOnBehalfOfWebContent(std::span<const uint8_t> subsystemSpan, std::span<const uint8_t> categorySpan, std::span<const uint8_t> stringSpan, uint8_t logType)
{
#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
    ASSERT(!isMainRunLoop());
#endif

    RefPtr connection = m_connection.get();

    bool isValidLogType = logType == OS_LOG_TYPE_DEFAULT || logType == OS_LOG_TYPE_INFO || logType == OS_LOG_TYPE_DEBUG || logType == OS_LOG_TYPE_ERROR || logType == OS_LOG_TYPE_FAULT;
    MESSAGE_CHECK(isValidLogType, connection);

    CString subsystem = subsystemSpan;
    CString category = categorySpan;
    CString string = stringSpan;
    MESSAGE_CHECK(subsystem.length() < logSubsystemMaxSize, connection);
    MESSAGE_CHECK(category.length() < logCategoryMaxSize, connection);
    MESSAGE_CHECK(string.length() < logStringMaxSize, connection);

    // os_log_hook on sender side sends a null category and subsystem when logging to OS_LOG_DEFAULT.
    OSObjectPtr<os_log_t> osLog;
    if (!subsystem.isEmpty() && !category.isEmpty()) {
        if (category == "Testing"_s)
            globalLogCountForTesting++;
        osLog = adoptOSObject(os_log_create(subsystem.data(), category.data()));
    }
    if (!osLog)
        osLog = OS_LOG_DEFAULT;

#if HAVE(OS_SIGNPOST)
    if (WTFSignpostHandleIndirectLog(osLog.get(), m_pid, string.spanIncludingNullTerminator()))
        return;
#endif

    // Use '%{public}s' in the format string for the preprocessed string from the WebContent process.
    // This should not reveal any redacted information in the string, since it has already been composed in the WebContent process.
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    SUPPRESS_UNCOUNTED_LOCAL logWithProcessNamePrefix(osLog.get(), static_cast<os_log_type_t>(logType), m_processName, m_pid, string.data());
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)

RefPtr<LogStream> LogStream::create(AuxiliaryProcessProxy& process, ASCIILiteral processName, IPC::StreamServerConnectionHandle&& serverConnection, LogStreamIdentifier identifier)
{
    RefPtr connection = IPC::StreamServerConnection::tryCreate(WTF::move(serverConnection), { });
    if (!connection)
        return nullptr;

    Ref instance = adoptRef(*new LogStream(process, processName, connection.releaseNonNull(), identifier));
    instance->m_connection->open(instance.get(), logWorkQueueSingleton());
    instance->m_connection->startReceivingMessages(instance, Messages::LogStream::messageReceiverName(), identifier.toUInt64());
    return instance;
}

void LogStream::didReceiveInvalidMessage(IPC::StreamServerConnection&, IPC::MessageName messageName, const Vector<uint32_t>&)
{
    RELEASE_LOG_FAULT_WITH_PAYLOAD(IPC, "Received an invalid message %s from %s process %d, requesting for it to be terminated.", description(messageName), m_processName, m_pid);
    callOnMainRunLoop([weakProcess = m_process] {
        if (RefPtr process = weakProcess.get())
            process->terminate();
    });
}

#else

Ref<LogStream> LogStream::create(AuxiliaryProcessProxy& process, ASCIILiteral processName, Ref<IPC::Connection>&& connection, LogStreamIdentifier identifier)
{
    return adoptRef(*new LogStream(process, processName, WTF::move(connection), identifier));
}

#endif

unsigned LogStream::logCountForTesting()
{
    return globalLogCountForTesting;
}

#if __has_include("LogMessagesImplementations.h")
#import "LogMessagesImplementations.h"
#endif

}

#endif
