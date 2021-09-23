/*
 * Copyright (C) 2019 Microsoft Corporation.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RemoteInspectorPipe.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "InspectorPlaywrightAgent.h"
#include <JavaScriptCore/InspectorFrontendChannel.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/UniqueArray.h>
#include <wtf/Vector.h>
#include <wtf/WorkQueue.h>

#if OS(UNIX)
#include <stdio.h>
#include <unistd.h>
#endif

#if PLATFORM(WIN)
#include <io.h>
#endif

namespace WebKit {

namespace {

const int readFD = 3;
const int writeFD = 4;

const size_t kWritePacketSize = 1 << 16;

#if PLATFORM(WIN)
HANDLE readHandle;
HANDLE writeHandle;
#endif

size_t ReadBytes(void* buffer, size_t size, bool exact_size)
{
    size_t bytesRead = 0;
    while (bytesRead < size) {
#if PLATFORM(WIN)
        DWORD sizeRead = 0;
        bool hadError = !ReadFile(readHandle, static_cast<char*>(buffer) + bytesRead,
            size - bytesRead, &sizeRead, nullptr);
#else
        int sizeRead = read(readFD, static_cast<char*>(buffer) + bytesRead,
            size - bytesRead);
        if (sizeRead < 0 && errno == EINTR)
            continue;
        bool hadError = sizeRead <= 0;
#endif
        if (hadError) {
            return 0;
        }
        bytesRead += sizeRead;
        if (!exact_size)
            break;
    }
    return bytesRead;
}

void WriteBytes(const char* bytes, size_t size)
{
    size_t totalWritten = 0;
    while (totalWritten < size) {
        size_t length = size - totalWritten;
        if (length > kWritePacketSize)
            length = kWritePacketSize;
#if PLATFORM(WIN)
        DWORD bytesWritten = 0;
        bool hadError = !WriteFile(writeHandle, bytes + totalWritten, static_cast<DWORD>(length), &bytesWritten, nullptr);
#else
        int bytesWritten = write(writeFD, bytes + totalWritten, length);
        if (bytesWritten < 0 && errno == EINTR)
            continue;
        bool hadError = bytesWritten <= 0;
#endif
        if (hadError)
            return;
        totalWritten += bytesWritten;
    }
}

}  // namespace

class RemoteInspectorPipe::RemoteFrontendChannel : public Inspector::FrontendChannel {
    WTF_MAKE_FAST_ALLOCATED;

public:
    RemoteFrontendChannel()
        : m_senderQueue(WorkQueue::create("Inspector pipe writer"))
    {
    }

    ~RemoteFrontendChannel() override = default;

    ConnectionType connectionType() const override
    {
        return ConnectionType::Remote;
    }

    void sendMessageToFrontend(const String& message) override
    {
        m_senderQueue->dispatch([message = message.isolatedCopy()]() {
            WriteBytes(message.ascii().data(), message.length());
            WriteBytes("\0", 1);
        });
    }

private:
    Ref<WorkQueue> m_senderQueue;
};

RemoteInspectorPipe::RemoteInspectorPipe(InspectorPlaywrightAgent& playwrightAgent)
    : m_playwrightAgent(playwrightAgent)
{
    m_remoteFrontendChannel = makeUnique<RemoteFrontendChannel>();
    start();
}

RemoteInspectorPipe::~RemoteInspectorPipe()
{
    stop();
}

bool RemoteInspectorPipe::start()
{
    if (m_receiverThread)
        return true;

#if PLATFORM(WIN)
    readHandle = reinterpret_cast<HANDLE>(_get_osfhandle(readFD));
    writeHandle = reinterpret_cast<HANDLE>(_get_osfhandle(writeFD));
#endif

    m_playwrightAgent.connectFrontend(*m_remoteFrontendChannel);
    m_terminated = false;
    m_receiverThread = Thread::create("Inspector pipe reader", [this] {
        workerRun();
    });
    return true;
}

void RemoteInspectorPipe::stop()
{
    if (!m_receiverThread)
        return;

    m_playwrightAgent.disconnectFrontend();

    m_terminated = true;
    m_receiverThread->waitForCompletion();
    m_receiverThread = nullptr;
}

void RemoteInspectorPipe::workerRun()
{
    const size_t bufSize = 256 * 1024;
    auto buffer = makeUniqueArray<char>(bufSize);
    Vector<char> line;
    while (!m_terminated) {
        size_t size = ReadBytes(buffer.get(), bufSize, false);
        if (!size) {
            RunLoop::main().dispatch([this] {
                if (!m_terminated)
                    m_playwrightAgent.disconnectFrontend();
            });
            break;
        }
        size_t start = 0;
        size_t end = line.size();
        line.append(buffer.get(), size);
        while (true) {
            for (; end < line.size(); ++end) {
                if (line[end] == '\0')
                    break;
            }
            if (end == line.size())
                break;

            if (end > start) {
                String message = String::fromUTF8(line.data() + start, end - start);
                RunLoop::main().dispatch([this, message = WTFMove(message)] {
                    if (!m_terminated)
                        m_playwrightAgent.dispatchMessageFromFrontend(message);
                });
            }
            ++end;
            start = end;
        }
        if (start != 0 && start < line.size())
            memmove(line.data(), line.data() + start, line.size() - start);
        line.shrink(line.size() - start);
    }
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
