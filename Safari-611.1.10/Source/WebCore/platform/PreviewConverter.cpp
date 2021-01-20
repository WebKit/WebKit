/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "config.h"
#include "PreviewConverter.h"

#if ENABLE(PREVIEW_CONVERTER)

#include "PreviewConverterClient.h"
#include "PreviewConverterProvider.h"
#include "SharedBuffer.h"
#include <wtf/RunLoop.h>
#include <wtf/SetForScope.h>

namespace WebCore {

PreviewConverter::~PreviewConverter() = default;

bool PreviewConverter::supportsMIMEType(const String& mimeType)
{
    if (mimeType.isNull())
        return false;

    if (equalLettersIgnoringASCIICase(mimeType, "text/html") || equalLettersIgnoringASCIICase(mimeType, "text/plain"))
        return false;

    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> supportedMIMETypes = platformSupportedMIMETypes();
    return supportedMIMETypes->contains(mimeType);
}

ResourceResponse PreviewConverter::previewResponse() const
{
    auto response = platformPreviewResponse();
    ASSERT(response.mimeType().length());
    response.setIsQuickLook(true);
    return response;
}

const ResourceError& PreviewConverter::previewError() const
{
    return m_previewError;
}

const SharedBuffer& PreviewConverter::previewData() const
{
    return m_previewData.get();
}

void PreviewConverter::updateMainResource()
{
    if (m_isInClientCallback)
        return;

    if (m_state != State::Updating)
        return;

    auto provider = m_provider.get();
    if (!provider) {
        didFailUpdating();
        return;
    }

    provider->provideMainResourceForPreviewConverter(*this, [this, protectedThis = makeRef(*this)](auto buffer) {
        if (buffer)
            appendFromBuffer(*buffer);
        else
            didFailUpdating();
    });
}

void PreviewConverter::appendFromBuffer(const SharedBuffer& buffer)
{
    while (buffer.size() > m_lengthAppended) {
        auto newData = buffer.getSomeData(m_lengthAppended);
        platformAppend(newData);
        m_lengthAppended += newData.size();
    }
}

void PreviewConverter::finishUpdating()
{
    if (m_isInClientCallback)
        return;

    if (m_state != State::Updating)
        return;

    platformFinishedAppending();

    iterateClients([&](auto& client) {
        client.previewConverterDidFinishUpdating(*this);
    });
}

void PreviewConverter::failedUpdating()
{
    if (m_isInClientCallback)
        return;

    if (m_state != State::Updating)
        return;

    m_state = State::FailedUpdating;
    platformFailedAppending();
}

bool PreviewConverter::hasClient(PreviewConverterClient& client) const
{
    return m_clients.contains(&client);
}

void PreviewConverter::addClient(PreviewConverterClient& client)
{
    ASSERT(!hasClient(client));
    m_clients.append(makeWeakPtr(client));
    didAddClient(client);
}

void PreviewConverter::removeClient(PreviewConverterClient& client)
{
    m_clients.removeFirst(&client);
    ASSERT(!hasClient(client));
}

static String& sharedPasswordForTesting()
{
    static NeverDestroyed<String> passwordForTesting;
    return passwordForTesting.get();
}

const String& PreviewConverter::passwordForTesting()
{
    return sharedPasswordForTesting();
}

void PreviewConverter::setPasswordForTesting(const String& password)
{
    sharedPasswordForTesting() = password;
}

template<typename T>
void PreviewConverter::iterateClients(T&& callback)
{
    SetForScope<bool> isInClientCallback { m_isInClientCallback, true };
    auto clientsCopy { m_clients };
    auto protectedThis { makeRef(*this) };

    for (auto& client : clientsCopy) {
        if (client && hasClient(*client))
            callback(*client);
    }
}

void PreviewConverter::didAddClient(PreviewConverterClient& client)
{
    RunLoop::current().dispatch([this, protectedThis = makeRef(*this), weakClient = makeWeakPtr(client)]() {
        if (auto client = weakClient.get())
            replayToClient(*client);
    });
}

void PreviewConverter::didFailConvertingWithError(const ResourceError& error)
{
    m_previewError = error;
    m_state = State::FailedConverting;

    iterateClients([&](auto& client) {
        client.previewConverterDidFailConverting(*this);
    });
}

void PreviewConverter::didFailUpdating()
{
    failedUpdating();

    iterateClients([&](auto& client) {
        client.previewConverterDidFailUpdating(*this);
    });
}

void PreviewConverter::replayToClient(PreviewConverterClient& client)
{
    if (!hasClient(client))
        return;

    SetForScope<bool> isInClientCallback { m_isInClientCallback, true };
    auto protectedThis { makeRef(*this) };

    client.previewConverterDidStartUpdating(*this);

    if (m_state == State::Updating || !hasClient(client))
        return;

    if (m_state == State::FailedUpdating) {
        client.previewConverterDidFailUpdating(*this);
        return;
    }

    ASSERT(m_state >= State::Converting);
    client.previewConverterDidStartConverting(*this);

    if (!m_previewData->isEmpty() && hasClient(client))
        client.previewConverterDidReceiveData(*this, m_previewData.get());

    if (m_state == State::Converting || !hasClient(client))
        return;

    if (m_state == State::FailedConverting) {
        ASSERT(!m_previewError.isNull());
        client.previewConverterDidFailConverting(*this);
        return;
    }

    ASSERT(m_state == State::FinishedConverting);
    ASSERT(!m_previewData->isEmpty());
    ASSERT(m_previewError.isNull());
    client.previewConverterDidFinishConverting(*this);
}

void PreviewConverter::delegateDidReceiveData(const SharedBuffer& data)
{
    auto protectedThis { makeRef(*this) };

    if (m_state == State::Updating) {
        m_provider = nullptr;
        m_state = State::Converting;

        iterateClients([&](auto& client) {
            client.previewConverterDidStartConverting(*this);
        });
    }

    ASSERT(m_state == State::Converting);
    if (data.isEmpty())
        return;

    m_previewData->append(data);

    iterateClients([&](auto& client) {
        client.previewConverterDidReceiveData(*this, data);
    });
}

void PreviewConverter::delegateDidFinishLoading()
{
    ASSERT(m_state == State::Converting);
    m_state = State::FinishedConverting;

    iterateClients([&](auto& client) {
        client.previewConverterDidFinishConverting(*this);
    });
}

void PreviewConverter::delegateDidFailWithError(const ResourceError& error)
{
    if (!isPlatformPasswordError(error)) {
        didFailConvertingWithError(error);
        return;
    }

    ASSERT(m_state == State::Updating);
    auto provider = m_provider.get();
    if (!provider) {
        didFailConvertingWithError(error);
        return;
    }

    provider->providePasswordForPreviewConverter(*this, [this, protectedThis = makeRef(*this)](auto& password) mutable {
        if (m_state != State::Updating)
            return;

        platformUnlockWithPassword(password);
        m_lengthAppended = 0;
        updateMainResource();
        finishUpdating();
    });
}

} // namespace WebCore

#endif // ENABLE(PREVIEW_CONVERTER)
