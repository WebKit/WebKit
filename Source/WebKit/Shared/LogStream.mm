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

#import "LogStreamMessages.h"
#import "Logging.h"
#import "StreamConnectionWorkQueue.h"
#import "StreamServerConnection.h"
#import "WebProcessProxy.h"
#import <wtf/OSObjectPtr.h>
#import <wtf/TZoneMallocInlines.h>

#if HAVE(OS_SIGNPOST)
#import <wtf/SystemTracing.h>
#endif

#define MESSAGE_CHECK(assertion, connection) MESSAGE_CHECK_BASE(assertion, connection)

namespace WebKit {

static std::atomic<unsigned> globalLogCountForTesting { 0 };

WTF_MAKE_TZONE_ALLOCATED_IMPL(LogStream);

LogStream::LogStream(WebProcessProxy& process, Ref<ConnectionType>&& connection, LogStreamIdentifier identifier)
    : m_connection(WTF::move(connection))
#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
    , m_process(process)
#endif
    , m_identifier(identifier)
    , m_pid(process.processID())
{
}

LogStream::~LogStream() = default;

void LogStream::stopListeningForIPC()
{
    assertIsMainRunLoop();
#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
    m_connection->stopReceivingMessages(Messages::LogStream::messageReceiverName(), m_identifier.toUInt64());
#endif
}

void LogStream::logOnBehalfOfWebContent(std::span<const uint8_t> logSubsystem, std::span<const uint8_t> logCategory, std::span<const uint8_t> nullTerminatedLogString, uint8_t logType)
{
#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
    ASSERT(!isMainRunLoop());
#endif
    auto isNullTerminated = [](std::span<const uint8_t> view) {
        return view.data() && !view.empty() && view.back() == '\0';
    };

    bool isValidLogType = logType == OS_LOG_TYPE_DEFAULT || logType == OS_LOG_TYPE_INFO || logType == OS_LOG_TYPE_DEBUG || logType == OS_LOG_TYPE_ERROR || logType == OS_LOG_TYPE_FAULT;

    RefPtr connection = m_connection.get();
    MESSAGE_CHECK(isNullTerminated(nullTerminatedLogString) && isValidLogType, connection);
    MESSAGE_CHECK(logSubsystem.size() <= logSubsystemMaxSize, connection);
    MESSAGE_CHECK(logCategory.size() <= logCategoryMaxSize, connection);
    MESSAGE_CHECK(nullTerminatedLogString.size() <= logStringMaxSize, connection);

    // os_log_hook on sender side sends a null category and subsystem when logging to OS_LOG_DEFAULT.
    OSObjectPtr<os_log_t> osLog;
    if (isNullTerminated(logSubsystem) && isNullTerminated(logCategory)) {
        auto subsystem = byteCast<char>(logSubsystem.data());
        auto category = byteCast<char>(logCategory.data());
        if (equalSpans("Testing\0"_span, logCategory))
            globalLogCountForTesting++;
        osLog = adoptOSObject(os_log_create(subsystem, category));
    }
    if (!osLog)
        osLog = OS_LOG_DEFAULT;

#if HAVE(OS_SIGNPOST)
    if (WTFSignpostHandleIndirectLog(osLog.get(), m_pid, byteCast<char>(nullTerminatedLogString)))
        return;
#endif

    // Use '%{public}s' in the format string for the preprocessed string from the WebContent process.
    // This should not reveal any redacted information in the string, since it has already been composed in the WebContent process.
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    SUPPRESS_UNCOUNTED_LOCAL os_log_with_type(osLog.get(), static_cast<os_log_type_t>(logType), "WebContent[%d] %{public}s", m_pid, byteCast<char>(nullTerminatedLogString).data());
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)

RefPtr<LogStream> LogStream::create(WebProcessProxy& process, IPC::StreamServerConnectionHandle&& serverConnection, LogStreamIdentifier identifier, CompletionHandler<void(IPC::Semaphore& streamWakeUpSemaphore, IPC::Semaphore& streamClientWaitSemaphore)>&& completionHandler)
{
    RefPtr connection = IPC::StreamServerConnection::tryCreate(WTF::move(serverConnection), { });
    if (!connection)
        return nullptr;
    static NeverDestroyed<Ref<IPC::StreamConnectionWorkQueue>> logQueue = IPC::StreamConnectionWorkQueue::create("Log work queue"_s);

    Ref instance = adoptRef(*new LogStream(process, connection.releaseNonNull(), identifier));
    instance->m_connection->open(instance.get(), logQueue.get());
    instance->m_connection->startReceivingMessages(instance, Messages::LogStream::messageReceiverName(), identifier.toUInt64());
    completionHandler(logQueue.get()->wakeUpSemaphore(), instance->m_connection->clientWaitSemaphore());
    return instance;
}

void LogStream::didReceiveInvalidMessage(IPC::StreamServerConnection&, IPC::MessageName messageName, const Vector<uint32_t>&)
{
    RELEASE_LOG_FAULT_WITH_PAYLOAD(IPC, makeString("Received an invalid message '"_s, description(messageName), "' from WebContent process, requesting for it to be terminated."_s).utf8().data());
    callOnMainRunLoop([weakProcess = m_process] {
        if (RefPtr process = weakProcess.get())
            process->terminate();
    });
}

#else

Ref<LogStream> LogStream::create(WebProcessProxy& process, Ref<IPC::Connection>&& connection, LogStreamIdentifier identifier)
{
    return adoptRef(*new LogStream(process, WTF::move(connection), identifier));
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
