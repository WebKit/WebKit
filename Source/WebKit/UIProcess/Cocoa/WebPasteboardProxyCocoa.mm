/*
 * Copyright (C) 2014-2022 Apple Inc. All rights reserved.
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
#import "PasteboardAccessIntent.h"
#import "SandboxExtension.h"
#import "WebCoreArgumentCoders.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import "WebProcessMessages.h"
#import "WebProcessProxy.h"
#import <WebCore/Color.h>
#import <WebCore/DataOwnerType.h>
#import <WebCore/LegacyNSPasteboardTypes.h>
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

void WebPasteboardProxy::grantAccessToCurrentTypes(WebProcessProxy& process, const String& pasteboardName)
{
    grantAccess(process, pasteboardName, PasteboardAccessType::Types);
}

void WebPasteboardProxy::grantAccessToCurrentData(WebProcessProxy& process, const String& pasteboardName)
{
    grantAccess(process, pasteboardName, PasteboardAccessType::TypesAndData);
}

void WebPasteboardProxy::grantAccess(WebProcessProxy& process, const String& pasteboardName, PasteboardAccessType type)
{
    if (!m_webProcessProxySet.contains(process))
        return;

    if (pasteboardName.isEmpty()) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto changeCount = PlatformPasteboard(pasteboardName).changeCount();
    auto changeCountsAndProcesses = m_pasteboardNameToAccessInformationMap.find(pasteboardName);
    if (changeCountsAndProcesses != m_pasteboardNameToAccessInformationMap.end() && changeCountsAndProcesses->value.changeCount == changeCount) {
        changeCountsAndProcesses->value.grantAccess(process, type);
        return;
    }

    m_pasteboardNameToAccessInformationMap.set(pasteboardName, PasteboardAccessInformation { changeCount, {{ process, type }} });
}

void WebPasteboardProxy::revokeAccess(WebProcessProxy& process)
{
    for (auto& changeCountAndProcesses : m_pasteboardNameToAccessInformationMap.values())
        changeCountAndProcesses.revokeAccess(process);
}

bool WebPasteboardProxy::canAccessPasteboardTypes(IPC::Connection& connection, const String& pasteboardName) const
{
    return !!accessType(connection, pasteboardName);
}

bool WebPasteboardProxy::canAccessPasteboardData(IPC::Connection& connection, const String& pasteboardName) const
{
    auto type = accessType(connection, pasteboardName);
    return type && *type == PasteboardAccessType::TypesAndData;
}

std::optional<WebPasteboardProxy::PasteboardAccessType> WebPasteboardProxy::accessType(IPC::Connection& connection, const String& pasteboardName) const
{
    MESSAGE_CHECK_WITH_RETURN_VALUE(!pasteboardName.isEmpty(), std::nullopt);

    auto* process = webProcessProxyForConnection(connection);
    MESSAGE_CHECK_WITH_RETURN_VALUE(process, std::nullopt);

    for (auto& page : process->pages()) {
        if (!page)
            continue;
        auto& preferences = page->preferences();
        if (!preferences.domPasteAllowed() || !preferences.javaScriptCanAccessClipboard())
            continue;

        // If a web page already allows JavaScript to programmatically read pasteboard data,
        // then it is possible for any other web page residing in the same web process to send
        // IPC messages that can read pasteboard data by pretending to be the page that has
        // allowed unmitigated pasteboard access from script. As such, there is no security
        // benefit in limiting the scope of pasteboard data access to only the web page that
        // enables programmatic pasteboard access.
        return PasteboardAccessType::TypesAndData;
    }

    auto changeCountAndProcesses = m_pasteboardNameToAccessInformationMap.find(pasteboardName);
    if (changeCountAndProcesses == m_pasteboardNameToAccessInformationMap.end())
        return std::nullopt;

    auto& information = changeCountAndProcesses->value;
    if (information.changeCount != PlatformPasteboard(pasteboardName).changeCount())
        return std::nullopt;

    return information.accessType(*process);
}

void WebPasteboardProxy::didModifyContentsOfPasteboard(IPC::Connection& connection, const String& pasteboardName, int64_t previousChangeCount, int64_t newChangeCount)
{
    auto* process = webProcessProxyForConnection(connection);
    MESSAGE_CHECK(process);

    auto changeCountAndProcesses = m_pasteboardNameToAccessInformationMap.find(pasteboardName);
    if (changeCountAndProcesses != m_pasteboardNameToAccessInformationMap.end() && previousChangeCount == changeCountAndProcesses->value.changeCount) {
        if (auto accessType = changeCountAndProcesses->value.accessType(*process))
            changeCountAndProcesses->value = PasteboardAccessInformation { newChangeCount, {{ *process, *accessType }} };
    }
}

void WebPasteboardProxy::getPasteboardTypes(IPC::Connection& connection, const String& pasteboardName, std::optional<PageIdentifier> pageID, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    if (!canAccessPasteboardTypes(connection, pasteboardName))
        return completionHandler({ });

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Read);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler({ }));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        Vector<String> pasteboardTypes;
        PlatformPasteboard(pasteboardName).getTypes(pasteboardTypes);
        completionHandler(WTFMove(pasteboardTypes));
    });
}

void WebPasteboardProxy::getPasteboardPathnamesForType(IPC::Connection& connection, const String& pasteboardName, const String& pasteboardType, std::optional<PageIdentifier> pageID,
    CompletionHandler<void(Vector<String>&& pathnames, Vector<SandboxExtension::Handle>&& sandboxExtensions)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!pasteboardType.isEmpty(), completionHandler({ }, { }));

    // FIXME: This should consult canAccessPasteboardData() instead, and avoid responding with file paths if it returns false.
    if (!canAccessPasteboardTypes(connection, pasteboardName))
        return completionHandler({ }, { });

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Read);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler({ }, { }));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        Vector<String> pathnames;
        Vector<SandboxExtension::Handle> sandboxExtensions;
        if (webProcessProxyForConnection(connection)) {
            PlatformPasteboard(pasteboardName).getPathnamesForType(pathnames, pasteboardType);
            // On iOS, files are copied into app's container upon paste.
#if PLATFORM(MAC)
            bool needsExtensions = pasteboardType == String(WebCore::legacyFilenamesPasteboardType());
            sandboxExtensions = pathnames.map([needsExtensions](auto& filename) {
                if (!needsExtensions || ![[NSFileManager defaultManager] fileExistsAtPath:filename])
                    return SandboxExtension::Handle { };

                return valueOrDefault(SandboxExtension::createHandle(filename, SandboxExtension::Type::ReadOnly));
            });
#endif
        }
        completionHandler(WTFMove(pathnames), WTFMove(sandboxExtensions));
    });
}

void WebPasteboardProxy::getPasteboardStringForType(IPC::Connection& connection, const String& pasteboardName, const String& pasteboardType, std::optional<PageIdentifier> pageID, CompletionHandler<void(String&&)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!pasteboardType.isEmpty(), completionHandler({ }));

    if (!canAccessPasteboardData(connection, pasteboardName))
        return completionHandler({ });

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Read);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler({ }));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        completionHandler(PlatformPasteboard(pasteboardName).stringForType(pasteboardType));
    });
}

void WebPasteboardProxy::getPasteboardStringsForType(IPC::Connection& connection, const String& pasteboardName, const String& pasteboardType, std::optional<PageIdentifier> pageID, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!pasteboardType.isEmpty(), completionHandler({ }));

    if (!canAccessPasteboardData(connection, pasteboardName))
        return completionHandler({ });

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Read);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler({ }));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        completionHandler(PlatformPasteboard(pasteboardName).allStringsForType(pasteboardType));
    });
}

void WebPasteboardProxy::getPasteboardBufferForType(IPC::Connection& connection, const String& pasteboardName, const String& pasteboardType, std::optional<PageIdentifier> pageID, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!pasteboardType.isEmpty(), completionHandler({ }));

    if (!canAccessPasteboardData(connection, pasteboardName))
        return completionHandler({ });

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Read);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler({ }));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        completionHandler(PlatformPasteboard(pasteboardName).bufferForType(pasteboardType));
    });
}

void WebPasteboardProxy::getPasteboardChangeCount(IPC::Connection& connection, const String& pasteboardName, std::optional<PageIdentifier> pageID, CompletionHandler<void(int64_t)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!pasteboardName.isEmpty(), completionHandler(0));

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Read);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler(0));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        completionHandler(PlatformPasteboard(pasteboardName).changeCount());
    });
}

void WebPasteboardProxy::getPasteboardColor(IPC::Connection& connection, const String& pasteboardName, std::optional<PageIdentifier> pageID, CompletionHandler<void(WebCore::Color&&)>&& completionHandler)
{
    if (!canAccessPasteboardData(connection, pasteboardName))
        return completionHandler({ });

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Read);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler({ }));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        completionHandler(PlatformPasteboard(pasteboardName).color());
    });
}

void WebPasteboardProxy::getPasteboardURL(IPC::Connection& connection, const String& pasteboardName, std::optional<PageIdentifier> pageID, CompletionHandler<void(const String&)>&& completionHandler)
{
    if (!canAccessPasteboardData(connection, pasteboardName))
        return completionHandler({ });

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Read);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler({ }));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        completionHandler(PlatformPasteboard(pasteboardName).url().string());
    });
}

void WebPasteboardProxy::addPasteboardTypes(IPC::Connection& connection, const String& pasteboardName, const Vector<String>& pasteboardTypes, std::optional<PageIdentifier> pageID, CompletionHandler<void(int64_t)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!pasteboardName.isEmpty(), completionHandler(0));

    for (auto& type : pasteboardTypes)
        MESSAGE_CHECK_COMPLETION(!type.isEmpty(), completionHandler(0));

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Write);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler(0));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        auto previousChangeCount = PlatformPasteboard(pasteboardName).changeCount();
        auto newChangeCount = PlatformPasteboard(pasteboardName).addTypes(pasteboardTypes);
        didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, newChangeCount);
        completionHandler(newChangeCount);
    });
}

void WebPasteboardProxy::setPasteboardTypes(IPC::Connection& connection, const String& pasteboardName, const Vector<String>& pasteboardTypes, std::optional<PageIdentifier> pageID, CompletionHandler<void(int64_t)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!pasteboardName.isEmpty(), completionHandler(0));

    for (auto& type : pasteboardTypes)
        MESSAGE_CHECK_COMPLETION(!type.isEmpty(), completionHandler(0));

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Write);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler(0));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        auto previousChangeCount = PlatformPasteboard(pasteboardName).changeCount();
        auto newChangeCount = PlatformPasteboard(pasteboardName).setTypes(pasteboardTypes);
        didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, newChangeCount);
        completionHandler(newChangeCount);
    });
}

void WebPasteboardProxy::setPasteboardURL(IPC::Connection& connection, const PasteboardURL& pasteboardURL, const String& pasteboardName, std::optional<PageIdentifier> pageID, CompletionHandler<void(int64_t)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!pasteboardName.isEmpty(), completionHandler(0));

    auto* process = webProcessProxyForConnection(connection);
    MESSAGE_CHECK_COMPLETION(process, completionHandler(0));

    if (!pasteboardURL.url.isValid())
        return completionHandler(0);

    if (!process->checkURLReceivedFromWebProcess(pasteboardURL.url.string(), CheckBackForwardList::No))
        return completionHandler(0);

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Write);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler(0));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        auto previousChangeCount = PlatformPasteboard(pasteboardName).changeCount();
        auto newChangeCount = PlatformPasteboard(pasteboardName).setURL(pasteboardURL);
        didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, newChangeCount);
        completionHandler(newChangeCount);
    });
}

void WebPasteboardProxy::setPasteboardColor(IPC::Connection& connection, const String& pasteboardName, const WebCore::Color& color, std::optional<PageIdentifier> pageID, CompletionHandler<void(int64_t)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!pasteboardName.isEmpty(), completionHandler(0));

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Write);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler(0));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        auto previousChangeCount = PlatformPasteboard(pasteboardName).changeCount();
        auto newChangeCount = PlatformPasteboard(pasteboardName).setColor(color);
        didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, newChangeCount);
        completionHandler(newChangeCount);
    });
}

void WebPasteboardProxy::setPasteboardStringForType(IPC::Connection& connection, const String& pasteboardName, const String& pasteboardType, const String& string, std::optional<PageIdentifier> pageID, CompletionHandler<void(int64_t)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!pasteboardName.isEmpty(), completionHandler(0));
    MESSAGE_CHECK_COMPLETION(!pasteboardType.isEmpty(), completionHandler(0));

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Write);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler(0));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        auto previousChangeCount = PlatformPasteboard(pasteboardName).changeCount();
        auto newChangeCount = PlatformPasteboard(pasteboardName).setStringForType(string, pasteboardType);
        didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, newChangeCount);
        completionHandler(newChangeCount);
    });
}

void WebPasteboardProxy::containsURLStringSuitableForLoading(IPC::Connection& connection, const String& pasteboardName, std::optional<PageIdentifier> pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canAccessPasteboardTypes(connection, pasteboardName))
        return completionHandler(false);

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Read);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler(false));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        completionHandler(PlatformPasteboard(pasteboardName).containsURLStringSuitableForLoading());
    });
}

void WebPasteboardProxy::urlStringSuitableForLoading(IPC::Connection& connection, const String& pasteboardName, std::optional<PageIdentifier> pageID, CompletionHandler<void(String&& url, String&& title)>&& completionHandler)
{
    if (!canAccessPasteboardData(connection, pasteboardName))
        return completionHandler({ }, { });

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Read);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler({ }, { }));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        String title;
        auto urlString = PlatformPasteboard(pasteboardName).urlStringSuitableForLoading(title);
        completionHandler(WTFMove(urlString), WTFMove(title));
    });
}

void WebPasteboardProxy::setPasteboardBufferForType(IPC::Connection& connection, const String& pasteboardName, const String& pasteboardType, RefPtr<SharedBuffer>&& buffer, std::optional<PageIdentifier> pageID, CompletionHandler<void(int64_t)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!pasteboardName.isEmpty(), completionHandler(0));
    MESSAGE_CHECK_COMPLETION(!pasteboardType.isEmpty(), completionHandler(0));

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Write);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler(0));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        auto previousChangeCount = PlatformPasteboard(pasteboardName).changeCount();
        if (!buffer) {
            auto newChangeCount = PlatformPasteboard(pasteboardName).setBufferForType(nullptr, pasteboardType);
            didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, newChangeCount);
            return completionHandler(newChangeCount);
        }
        auto newChangeCount = PlatformPasteboard(pasteboardName).setBufferForType(buffer.get(), pasteboardType);
        didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, newChangeCount);
        completionHandler(newChangeCount);
    });
}

void WebPasteboardProxy::getNumberOfFiles(IPC::Connection& connection, const String& pasteboardName, std::optional<PageIdentifier> pageID, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    if (!canAccessPasteboardTypes(connection, pasteboardName))
        return completionHandler(0);

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Read);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler(0));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        completionHandler(PlatformPasteboard(pasteboardName).numberOfFiles());
    });
}

void WebPasteboardProxy::typesSafeForDOMToReadAndWrite(IPC::Connection& connection, const String& pasteboardName, const String& origin, std::optional<PageIdentifier> pageID, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!origin.isNull(), completionHandler({ }));

    if (!canAccessPasteboardTypes(connection, pasteboardName))
        return completionHandler({ });

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Read);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler({ }));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        completionHandler(PlatformPasteboard(pasteboardName).typesSafeForDOMToReadAndWrite(origin));
    });
}

void WebPasteboardProxy::writeCustomData(IPC::Connection& connection, const Vector<PasteboardCustomData>& data, const String& pasteboardName, std::optional<PageIdentifier> pageID, CompletionHandler<void(int64_t)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!pasteboardName.isEmpty(), completionHandler(0));

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Write);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler(0));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        auto previousChangeCount = PlatformPasteboard(pasteboardName).changeCount();
        auto newChangeCount = PlatformPasteboard(pasteboardName).write(data);
        didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, newChangeCount);
        completionHandler(newChangeCount);
    });
}

void WebPasteboardProxy::allPasteboardItemInfo(IPC::Connection& connection, const String& pasteboardName, int64_t changeCount, std::optional<PageIdentifier> pageID, CompletionHandler<void(std::optional<Vector<PasteboardItemInfo>>&&)>&& completionHandler)
{
    if (!canAccessPasteboardTypes(connection, pasteboardName))
        return completionHandler({ });

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Read);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler({ }));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        completionHandler(PlatformPasteboard(pasteboardName).allPasteboardItemInfo(changeCount));
    });
}

void WebPasteboardProxy::informationForItemAtIndex(IPC::Connection& connection, size_t index, const String& pasteboardName, int64_t changeCount, std::optional<PageIdentifier> pageID, CompletionHandler<void(std::optional<PasteboardItemInfo>&&)>&& completionHandler)
{
    if (!canAccessPasteboardTypes(connection, pasteboardName))
        return completionHandler(std::nullopt);

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Read);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler(std::nullopt));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        completionHandler(PlatformPasteboard(pasteboardName).informationForItemAtIndex(index, changeCount));
    });
}

void WebPasteboardProxy::getPasteboardItemsCount(IPC::Connection& connection, const String& pasteboardName, std::optional<PageIdentifier> pageID, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    if (!canAccessPasteboardTypes(connection, pasteboardName))
        return completionHandler(0);

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Read);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler(0));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        completionHandler(PlatformPasteboard(pasteboardName).count());
    });
}

void WebPasteboardProxy::readStringFromPasteboard(IPC::Connection& connection, size_t index, const String& pasteboardType, const String& pasteboardName, std::optional<PageIdentifier> pageID, CompletionHandler<void(String&&)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!pasteboardType.isEmpty(), completionHandler({ }));

    if (!canAccessPasteboardData(connection, pasteboardName))
        return completionHandler({ });

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Read);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler({ }));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        completionHandler(PlatformPasteboard(pasteboardName).readString(index, pasteboardType));
    });
}

void WebPasteboardProxy::readURLFromPasteboard(IPC::Connection& connection, size_t index, const String& pasteboardName, std::optional<PageIdentifier> pageID, CompletionHandler<void(String&& url, String&& title)>&& completionHandler)
{
    if (!canAccessPasteboardData(connection, pasteboardName))
        return completionHandler({ }, { });

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Read);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler({ }, { }));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        String title;
        String url = PlatformPasteboard(pasteboardName).readURL(index, title).string();
        completionHandler(WTFMove(url), WTFMove(title));
    });
}

void WebPasteboardProxy::readBufferFromPasteboard(IPC::Connection& connection, std::optional<size_t> index, const String& pasteboardType, const String& pasteboardName, std::optional<PageIdentifier> pageID, CompletionHandler<void(RefPtr<SharedBuffer>&&)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!pasteboardType.isEmpty(), completionHandler({ }));

    if (!canAccessPasteboardData(connection, pasteboardName))
        return completionHandler({ });

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Read);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler({ }));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        completionHandler(PlatformPasteboard(pasteboardName).readBuffer(index, pasteboardType));
    });
}

void WebPasteboardProxy::containsStringSafeForDOMToReadForType(IPC::Connection& connection, const String& type, const String& pasteboardName, std::optional<PageIdentifier> pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canAccessPasteboardTypes(connection, pasteboardName))
        return completionHandler(false);

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Read);
    MESSAGE_CHECK_COMPLETION(dataOwner, completionHandler(false));

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        completionHandler(PlatformPasteboard(pasteboardName).containsStringSafeForDOMToReadForType(type));
    });
}

#if PLATFORM(IOS_FAMILY)

void WebPasteboardProxy::writeURLToPasteboard(IPC::Connection& connection, const PasteboardURL& url, const String& pasteboardName, std::optional<PageIdentifier> pageID)
{
    MESSAGE_CHECK(!pasteboardName.isEmpty());

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Write);
    MESSAGE_CHECK(dataOwner);

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        auto previousChangeCount = PlatformPasteboard(pasteboardName).changeCount();
        PlatformPasteboard(pasteboardName).write(url);
        didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, PlatformPasteboard(pasteboardName).changeCount());
        if (auto process = webProcessProxyForConnection(connection))
            process->send(Messages::WebProcess::DidWriteToPasteboardAsynchronously(pasteboardName), 0);
    });
}

void WebPasteboardProxy::writeWebContentToPasteboard(IPC::Connection& connection, const WebCore::PasteboardWebContent& content, const String& pasteboardName, std::optional<PageIdentifier> pageID)
{
    MESSAGE_CHECK(!pasteboardName.isEmpty());

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Write);
    MESSAGE_CHECK(dataOwner);

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        auto previousChangeCount = PlatformPasteboard(pasteboardName).changeCount();
        PlatformPasteboard(pasteboardName).write(content);
        didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, PlatformPasteboard(pasteboardName).changeCount());
        if (auto process = webProcessProxyForConnection(connection))
            process->send(Messages::WebProcess::DidWriteToPasteboardAsynchronously(pasteboardName), 0);
    });
}

void WebPasteboardProxy::writeImageToPasteboard(IPC::Connection& connection, const WebCore::PasteboardImage& pasteboardImage, const String& pasteboardName, std::optional<PageIdentifier> pageID)
{
    MESSAGE_CHECK(!pasteboardName.isEmpty());

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Write);
    MESSAGE_CHECK(dataOwner);

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        auto previousChangeCount = PlatformPasteboard(pasteboardName).changeCount();
        PlatformPasteboard(pasteboardName).write(pasteboardImage);
        didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, PlatformPasteboard(pasteboardName).changeCount());
        if (auto process = webProcessProxyForConnection(connection))
            process->send(Messages::WebProcess::DidWriteToPasteboardAsynchronously(pasteboardName), 0);
    });
}

void WebPasteboardProxy::writeStringToPasteboard(IPC::Connection& connection, const String& pasteboardType, const String& text, const String& pasteboardName, std::optional<PageIdentifier> pageID)
{
    MESSAGE_CHECK(!pasteboardName.isEmpty());
    MESSAGE_CHECK(!pasteboardType.isEmpty() || text.isEmpty());

    auto dataOwner = determineDataOwner(connection, pasteboardName, pageID, PasteboardAccessIntent::Write);
    MESSAGE_CHECK(dataOwner);

    PlatformPasteboard::performAsDataOwner(*dataOwner, [&] {
        auto previousChangeCount = PlatformPasteboard(pasteboardName).changeCount();
        PlatformPasteboard(pasteboardName).write(pasteboardType, text);
        didModifyContentsOfPasteboard(connection, pasteboardName, previousChangeCount, PlatformPasteboard(pasteboardName).changeCount());
        if (auto process = webProcessProxyForConnection(connection))
            process->send(Messages::WebProcess::DidWriteToPasteboardAsynchronously(pasteboardName), 0);
    });
}

void WebPasteboardProxy::updateSupportedTypeIdentifiers(const Vector<String>& identifiers, const String& pasteboardName, std::optional<PageIdentifier>)
{
    PlatformPasteboard(pasteboardName).updateSupportedTypeIdentifiers(identifiers);
}

#endif // PLATFORM(IOS_FAMILY)

std::optional<DataOwnerType> WebPasteboardProxy::determineDataOwner(IPC::Connection& connection, const String& pasteboardName, std::optional<PageIdentifier> pageID, PasteboardAccessIntent intent) const
{
    MESSAGE_CHECK_WITH_RETURN_VALUE(!pasteboardName.isEmpty(), std::nullopt);

    auto* process = webProcessProxyForConnection(connection);
    MESSAGE_CHECK_WITH_RETURN_VALUE(process, std::nullopt);

    if (!pageID)
        return DataOwnerType::Undefined;

    std::optional<DataOwnerType> result;
    for (auto& page : process->pages()) {
        if (page && page->webPageID() == *pageID) {
            result = page->dataOwnerForPasteboard(intent);
            break;
        }
    }
    // If this message check is hit, then the incoming web page ID doesn't correspond to any page
    // currently known to the UI process.
    MESSAGE_CHECK_WITH_RETURN_VALUE(result.has_value(), std::nullopt);
    return result;
}

void WebPasteboardProxy::PasteboardAccessInformation::grantAccess(WebProcessProxy& process, PasteboardAccessType type)
{
    auto matchIndex = processes.findIf([&](auto& processAndType) {
        return processAndType.first == &process;
    });

    if (matchIndex == notFound) {
        processes.append({ process, type });
        return;
    }

    if (type == PasteboardAccessType::TypesAndData)
        processes[matchIndex].second = type;

    processes.removeAllMatching([](auto& processAndType) {
        return !processAndType.first;
    });
}

void WebPasteboardProxy::PasteboardAccessInformation::revokeAccess(WebProcessProxy& process)
{
    processes.removeFirstMatching([&](auto& processAndType) {
        return processAndType.first == &process;
    });
}

std::optional<WebPasteboardProxy::PasteboardAccessType> WebPasteboardProxy::PasteboardAccessInformation::accessType(WebProcessProxy& process) const
{
    auto matchIndex = processes.findIf([&](auto& processAndType) {
        return processAndType.first == &process;
    });

    if (matchIndex == notFound)
        return std::nullopt;

    return processes[matchIndex].second;
}

#if ENABLE(IPC_TESTING_API)
void WebPasteboardProxy::testIPCSharedMemory(IPC::Connection& connection, const String& pasteboardName, const String& pasteboardType, SharedMemory::Handle&& handle, std::optional<PageIdentifier> pageID, CompletionHandler<void(int64_t, String)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!pasteboardName.isEmpty(), completionHandler(-1, makeString("error")));
    MESSAGE_CHECK_COMPLETION(!pasteboardType.isEmpty(), completionHandler(-1, makeString("error")));

    auto sharedMemoryBuffer = SharedMemory::map(handle, SharedMemory::Protection::ReadOnly);
    if (!sharedMemoryBuffer) {
        completionHandler(-1, makeString("error EOM"));
        return;
    }

    String message = { static_cast<char*>(sharedMemoryBuffer->data()), static_cast<unsigned>(sharedMemoryBuffer->size()) };
    completionHandler(sharedMemoryBuffer->size(), WTFMove(message));
}
#endif

} // namespace WebKit

#undef MESSAGE_CHECK_COMPLETION
#undef MESSAGE_CHECK_WITH_RETURN_VALUE
#undef MESSAGE_CHECK
