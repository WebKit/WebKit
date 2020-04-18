/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebPasteboardProxy.h"

#import "Connection.h"
#import "SandboxExtension.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import "WebProcessProxy.h"
#import <WebCore/Color.h>
#import <WebCore/Pasteboard.h>
#import <WebCore/PasteboardItemInfo.h>
#import <WebCore/PlatformPasteboard.h>
#import <WebCore/SharedBuffer.h>
#import <wtf/URL.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, (&connection))
#define MESSAGE_CHECK_WITH_RETURN_VALUE(assertion, returnValue) MESSAGE_CHECK_WITH_RETURN_VALUE_BASE(assertion, (&connection), returnValue)
#define MESSAGE_CHECK_COMPLETION(assertion, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, (&connection), completion)

namespace WebKit {
using namespace WebCore;

void WebPasteboardProxy::grantAccessToCurrentData(WebProcessProxy& process, const String& pasteboardName)
{
    if (!m_webProcessProxyList.contains(&process))
        return;

    if (pasteboardName.isEmpty()) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto changeCount = PlatformPasteboard(pasteboardName).changeCount();
    auto changeCountsAndProcesses = m_pasteboardNameToChangeCountAndProcessesMap.find(pasteboardName);
    if (changeCountsAndProcesses != m_pasteboardNameToChangeCountAndProcessesMap.end() && changeCountsAndProcesses->value.first == changeCount) {
        changeCountsAndProcesses->value.second.add(process);
        return;
    }

    WeakHashSet<WebProcessProxy> processes;
    processes.add(process);
    m_pasteboardNameToChangeCountAndProcessesMap.set(pasteboardName, std::make_pair(changeCount, WTFMove(processes)));
}

void WebPasteboardProxy::revokeAccessToAllData(WebProcessProxy& process)
{
    for (auto& changeCountAndProcesses : m_pasteboardNameToChangeCountAndProcessesMap.values())
        changeCountAndProcesses.second.remove(process);
}

bool WebPasteboardProxy::canAccessPasteboardData(IPC::Connection& connection, const String& pasteboardName) const
{
    MESSAGE_CHECK_WITH_RETURN_VALUE(!pasteboardName.isEmpty(), false);

    auto* process = webProcessProxyForConnection(connection);
    MESSAGE_CHECK_WITH_RETURN_VALUE(process, false);

    for (auto* page : process->pages()) {
        auto& preferences = page->preferences();
        if (!preferences.domPasteAllowed() || !preferences.javaScriptCanAccessClipboard())
            continue;

        // If a web page already allows JavaScript to programmatically read pasteboard data,
        // then it is possible for any other web page residing in the same web process to send
        // IPC messages that can read pasteboard data by pretending to be the page that has
        // allowed unmitigated pasteboard access from script. As such, there is no security
        // benefit in limiting the scope of pasteboard data access to only the web page that
        // enables programmatic pasteboard access.
        return true;
    }

    auto changeCountAndProcesses = m_pasteboardNameToChangeCountAndProcessesMap.find(pasteboardName);
    if (changeCountAndProcesses == m_pasteboardNameToChangeCountAndProcessesMap.end())
        return false;

    auto& [changeCount, processes] = changeCountAndProcesses->value;
    return changeCount == PlatformPasteboard(pasteboardName).changeCount() && processes.contains(*process);
}

void WebPasteboardProxy::didModifyContentsOfPasteboard(IPC::Connection& connection, const String& pasteboardName, int64_t previousChangeCount, int64_t newChangeCount)
{
    MESSAGE_CHECK(!pasteboardName.isEmpty());

    auto* process = webProcessProxyForConnection(connection);
    MESSAGE_CHECK(process);

    auto changeCountAndProcesses = m_pasteboardNameToChangeCountAndProcessesMap.find(pasteboardName);
    if (changeCountAndProcesses != m_pasteboardNameToChangeCountAndProcessesMap.end() && previousChangeCount == changeCountAndProcesses->value.first) {
        WeakHashSet<WebProcessProxy> processes;
        processes.add(process);
        changeCountAndProcesses->value = std::make_pair(newChangeCount, WTFMove(processes));
    }
}

void WebPasteboardProxy::getPasteboardTypes(const String& pasteboardName, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    Vector<String> pasteboardTypes;
    PlatformPasteboard(pasteboardName).getTypes(pasteboardTypes);
    completionHandler(WTFMove(pasteboardTypes));
}

void WebPasteboardProxy::getPasteboardPathnamesForType(IPC::Connection& connection, const String& pasteboardName, const String& pasteboardType,
    CompletionHandler<void(Vector<String>&& pathnames, SandboxExtension::HandleArray&& sandboxExtensions)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!pasteboardType.isNull(), completionHandler({ }, { }));

    // FIXME: This should consult canAccessPasteboardData() as well, and avoid responding with file paths if it returns false.
    Vector<String> pathnames;
    SandboxExtension::HandleArray sandboxExtensions;
    if (webProcessProxyForConnection(connection)) {
        PlatformPasteboard(pasteboardName).getPathnamesForType(pathnames, pasteboardType);

#if PLATFORM(MAC)
        // On iOS, files are copied into app's container upon paste.
        sandboxExtensions.allocate(pathnames.size());
        for (size_t i = 0; i < pathnames.size(); i++) {
            auto& filename = pathnames[i];
            if (![[NSFileManager defaultManager] fileExistsAtPath:filename])
                continue;
            SandboxExtension::createHandle(filename, SandboxExtension::Type::ReadOnly, sandboxExtensions[i]);
        }
#endif
    }
    completionHandler(WTFMove(pathnames), WTFMove(sandboxExtensions));
}

void WebPasteboardProxy::getPasteboardStringForType(IPC::Connection& connection, const String& pasteboardName, const String& pasteboardType, CompletionHandler<void(String&&)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!pasteboardType.isNull(), completionHandler({ }));

    if (!canAccessPasteboardData(connection, pasteboardName))
        return completionHandler({ });

    completionHandler(PlatformPasteboard(pasteboardName).stringForType(pasteboardType));
}

void WebPasteboardProxy::getPasteboardStringsForType(IPC::Connection& connection, const String& pasteboardName, const String& pasteboardType, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!pasteboardType.isNull(), completionHandler({ }));

    if (!canAccessPasteboardData(connection, pasteboardName))
        return completionHandler({ });

    completionHandler(PlatformPasteboard(pasteboardName).allStringsForType(pasteboardType));
}

void WebPasteboardProxy::getPasteboardBufferForType(IPC::Connection& connection, const String& pasteboardName, const String& pasteboardType, CompletionHandler<void(SharedMemory::Handle&&, uint64_t)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!pasteboardType.isNull(), completionHandler({ }, 0));

    if (!canAccessPasteboardData(connection, pasteboardName))
        return completionHandler({ }, 0);

    RefPtr<SharedBuffer> buffer = PlatformPasteboard(pasteboardName).bufferForType(pasteboardType);
    if (!buffer)
        return completionHandler({ }, 0);
    uint64_t size = buffer->size();
    if (!size)
        return completionHandler({ }, 0);
    RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::allocate(size);
    if (!sharedMemoryBuffer)
        return completionHandler({ }, 0);
    memcpy(sharedMemoryBuffer->data(), buffer->data(), size);
    SharedMemory::Handle handle;
    sharedMemoryBuffer->createHandle(handle, SharedMemory::Protection::ReadOnly);
    completionHandler(WTFMove(handle), size);
}

void WebPasteboardProxy::getPasteboardChangeCount(const String& pasteboardName, CompletionHandler<void(int64_t)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).changeCount());
}

void WebPasteboardProxy::getPasteboardColor(IPC::Connection& connection, const String& pasteboardName, CompletionHandler<void(WebCore::Color&&)>&& completionHandler)
{
    if (!canAccessPasteboardData(connection, pasteboardName))
        return completionHandler({ });

    completionHandler(PlatformPasteboard(pasteboardName).color());
}

void WebPasteboardProxy::getPasteboardURL(IPC::Connection& connection, const String& pasteboardName, CompletionHandler<void(const String&)>&& completionHandler)
{
    if (!canAccessPasteboardData(connection, pasteboardName))
        return completionHandler({ });

    completionHandler(PlatformPasteboard(pasteboardName).url().string());
}

void WebPasteboardProxy::addPasteboardTypes(IPC::Connection& connection, const String& pasteboardName, const Vector<String>& pasteboardTypes, CompletionHandler<void(int64_t)>&& completionHandler)
{
    auto previousChangeCount = PlatformPasteboard(pasteboardName).changeCount();
    auto newChangeCount = PlatformPasteboard(pasteboardName).addTypes(pasteboardTypes);
    didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, previousChangeCount);
    completionHandler(newChangeCount);
}

void WebPasteboardProxy::setPasteboardTypes(IPC::Connection& connection, const String& pasteboardName, const Vector<String>& pasteboardTypes, CompletionHandler<void(int64_t)>&& completionHandler)
{
    auto previousChangeCount = PlatformPasteboard(pasteboardName).changeCount();
    auto newChangeCount = PlatformPasteboard(pasteboardName).setTypes(pasteboardTypes);
    didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, newChangeCount);
    completionHandler(newChangeCount);
}

void WebPasteboardProxy::setPasteboardURL(IPC::Connection& connection, const PasteboardURL& pasteboardURL, const String& pasteboardName, CompletionHandler<void(int64_t)>&& completionHandler)
{
    if (auto* webProcessProxy = webProcessProxyForConnection(connection)) {
        if (!webProcessProxy->checkURLReceivedFromWebProcess(pasteboardURL.url.string()))
            return completionHandler(0);

        auto previousChangeCount = PlatformPasteboard(pasteboardName).changeCount();
        auto newChangeCount = PlatformPasteboard(pasteboardName).setURL(pasteboardURL);
        didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, newChangeCount);
        return completionHandler(newChangeCount);
    }
    completionHandler(0);
}

void WebPasteboardProxy::setPasteboardColor(IPC::Connection& connection, const String& pasteboardName, const WebCore::Color& color, CompletionHandler<void(int64_t)>&& completionHandler)
{
    auto previousChangeCount = PlatformPasteboard(pasteboardName).changeCount();
    auto newChangeCount = PlatformPasteboard(pasteboardName).setColor(color);
    didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, newChangeCount);
    completionHandler(newChangeCount);
}

void WebPasteboardProxy::setPasteboardStringForType(IPC::Connection& connection, const String& pasteboardName, const String& pasteboardType, const String& string, CompletionHandler<void(int64_t)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!pasteboardType.isNull(), completionHandler(0));

    auto previousChangeCount = PlatformPasteboard(pasteboardName).changeCount();
    auto newChangeCount = PlatformPasteboard(pasteboardName).setStringForType(string, pasteboardType);
    didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, newChangeCount);
    completionHandler(newChangeCount);
}

void WebPasteboardProxy::containsURLStringSuitableForLoading(const String& pasteboardName, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).containsURLStringSuitableForLoading());
}

void WebPasteboardProxy::urlStringSuitableForLoading(IPC::Connection& connection, const String& pasteboardName, CompletionHandler<void(String&& url, String&& title)>&& completionHandler)
{
    if (!canAccessPasteboardData(connection, pasteboardName))
        return completionHandler({ }, { });

    String title;
    auto urlString = PlatformPasteboard(pasteboardName).urlStringSuitableForLoading(title);
    completionHandler(WTFMove(urlString), WTFMove(title));
}

void WebPasteboardProxy::setPasteboardBufferForType(IPC::Connection& connection, const String& pasteboardName, const String& pasteboardType, const SharedMemory::Handle& handle, uint64_t size, CompletionHandler<void(int64_t)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!pasteboardType.isNull(), completionHandler(0));

    auto previousChangeCount = PlatformPasteboard(pasteboardName).changeCount();
    if (handle.isNull()) {
        auto newChangeCount = PlatformPasteboard(pasteboardName).setBufferForType(nullptr, pasteboardType);
        didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, newChangeCount);
        return completionHandler(newChangeCount);
    }

    // SharedMemory::Handle::size() is rounded up to the nearest page.
    MESSAGE_CHECK_COMPLETION(size <= handle.size(), completionHandler(0));

    RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::map(handle, SharedMemory::Protection::ReadOnly);
    if (!sharedMemoryBuffer)
        return completionHandler(0);
    auto buffer = SharedBuffer::create(static_cast<unsigned char *>(sharedMemoryBuffer->data()), static_cast<size_t>(size));
    auto newChangeCount = PlatformPasteboard(pasteboardName).setBufferForType(buffer.ptr(), pasteboardType);
    didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, newChangeCount);
    completionHandler(newChangeCount);
}

void WebPasteboardProxy::getNumberOfFiles(const String& pasteboardName, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).numberOfFiles());
}

void WebPasteboardProxy::typesSafeForDOMToReadAndWrite(IPC::Connection& connection, const String& pasteboardName, const String& origin, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!origin.isNull(), completionHandler({ }));

    completionHandler(PlatformPasteboard(pasteboardName).typesSafeForDOMToReadAndWrite(origin));
}

void WebPasteboardProxy::writeCustomData(IPC::Connection& connection, const Vector<PasteboardCustomData>& data, const String& pasteboardName, CompletionHandler<void(int64_t)>&& completionHandler)
{
    auto previousChangeCount = PlatformPasteboard(pasteboardName).changeCount();
    auto newChangeCount = PlatformPasteboard(pasteboardName).write(data);
    didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, newChangeCount);
    completionHandler(newChangeCount);
}

void WebPasteboardProxy::allPasteboardItemInfo(const String& pasteboardName, int64_t changeCount, CompletionHandler<void(Optional<Vector<PasteboardItemInfo>>&&)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).allPasteboardItemInfo(changeCount));
}

void WebPasteboardProxy::informationForItemAtIndex(size_t index, const String& pasteboardName, int64_t changeCount, CompletionHandler<void(Optional<PasteboardItemInfo>&&)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).informationForItemAtIndex(index, changeCount));
}

void WebPasteboardProxy::getPasteboardItemsCount(const String& pasteboardName, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).count());
}

void WebPasteboardProxy::readStringFromPasteboard(IPC::Connection& connection, size_t index, const String& pasteboardType, const String& pasteboardName, CompletionHandler<void(String&&)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!pasteboardType.isNull(), completionHandler({ }));

    if (!canAccessPasteboardData(connection, pasteboardName))
        return completionHandler({ });

    completionHandler(PlatformPasteboard(pasteboardName).readString(index, pasteboardType));
}

void WebPasteboardProxy::readURLFromPasteboard(IPC::Connection& connection, size_t index, const String& pasteboardName, CompletionHandler<void(String&& url, String&& title)>&& completionHandler)
{
    if (!canAccessPasteboardData(connection, pasteboardName))
        return completionHandler({ }, { });

    String title;
    String url = PlatformPasteboard(pasteboardName).readURL(index, title);
    completionHandler(WTFMove(url), WTFMove(title));
}

void WebPasteboardProxy::readBufferFromPasteboard(IPC::Connection& connection, size_t index, const String& pasteboardType, const String& pasteboardName, CompletionHandler<void(SharedMemory::Handle&&, uint64_t size)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!pasteboardType.isNull(), completionHandler({ }, 0));

    if (!canAccessPasteboardData(connection, pasteboardName))
        return completionHandler({ }, 0);

    RefPtr<SharedBuffer> buffer = PlatformPasteboard(pasteboardName).readBuffer(index, pasteboardType);
    if (!buffer)
        return completionHandler({ }, 0);
    uint64_t size = buffer->size();
    if (!size)
        return completionHandler({ }, 0);
    RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::allocate(size);
    if (!sharedMemoryBuffer)
        return completionHandler({ }, 0);
    memcpy(sharedMemoryBuffer->data(), buffer->data(), size);
    SharedMemory::Handle handle;
    sharedMemoryBuffer->createHandle(handle, SharedMemory::Protection::ReadOnly);
    completionHandler(WTFMove(handle), size);
}

void WebPasteboardProxy::containsStringSafeForDOMToReadForType(const String& type, const String& pasteboardName, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).containsStringSafeForDOMToReadForType(type));
}

#if PLATFORM(IOS_FAMILY)

void WebPasteboardProxy::writeURLToPasteboard(IPC::Connection& connection, const PasteboardURL& url, const String& pasteboardName)
{
    auto previousChangeCount = PlatformPasteboard(pasteboardName).changeCount();
    PlatformPasteboard(pasteboardName).write(url);
    didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, PlatformPasteboard(pasteboardName).changeCount());
}

void WebPasteboardProxy::writeWebContentToPasteboard(IPC::Connection& connection, const WebCore::PasteboardWebContent& content, const String& pasteboardName)
{
    auto previousChangeCount = PlatformPasteboard(pasteboardName).changeCount();
    PlatformPasteboard(pasteboardName).write(content);
    didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, PlatformPasteboard(pasteboardName).changeCount());
}

void WebPasteboardProxy::writeImageToPasteboard(IPC::Connection& connection, const WebCore::PasteboardImage& pasteboardImage, const String& pasteboardName)
{
    auto previousChangeCount = PlatformPasteboard(pasteboardName).changeCount();
    PlatformPasteboard(pasteboardName).write(pasteboardImage);
    didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, PlatformPasteboard(pasteboardName).changeCount());
}

void WebPasteboardProxy::writeStringToPasteboard(IPC::Connection& connection, const String& pasteboardType, const String& text, const String& pasteboardName)
{
    auto previousChangeCount = PlatformPasteboard(pasteboardName).changeCount();
    PlatformPasteboard(pasteboardName).write(pasteboardType, text);
    didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, PlatformPasteboard(pasteboardName).changeCount());
}

void WebPasteboardProxy::updateSupportedTypeIdentifiers(const Vector<String>& identifiers, const String& pasteboardName)
{
    PlatformPasteboard(pasteboardName).updateSupportedTypeIdentifiers(identifiers);
}

#endif // PLATFORM(IOS_FAMILY)

} // namespace WebKit

#undef MESSAGE_CHECK_COMPLETION
#undef MESSAGE_CHECK_WITH_RETURN_VALUE
#undef MESSAGE_CHECK
