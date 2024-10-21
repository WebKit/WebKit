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

#include "config.h"
#include "SessionStateConversion.h"

#include "SessionState.h"
#include <WebCore/BlobData.h>
#include <WebCore/FormData.h>
#include <WebCore/HistoryItem.h>
#include <wtf/FileSystem.h>

namespace WebKit {
using namespace WebCore;

static HTTPBody toHTTPBody(const FormData& formData)
{
    HTTPBody httpBody;

    for (const auto& formDataElement : formData.elements()) {
        HTTPBody::Element element;

        switchOn(formDataElement.data,
            [&] (const Vector<uint8_t>& bytes) {
                element.data = bytes;
            }, [&] (const FormDataElement::EncodedFileData& fileData) {
                HTTPBody::Element::FileData data;
                data.filePath = fileData.filename;
                data.fileStart = fileData.fileStart;
                if (fileData.fileLength != BlobDataItem::toEndOfFile)
                    data.fileLength = fileData.fileLength;
                data.expectedFileModificationTime = fileData.expectedFileModificationTime;
                element.data = WTFMove(data);
            }, [&] (const FormDataElement::EncodedBlobData& blobData) {
                element.data = blobData.url.string();
            }
        );

        httpBody.elements.append(WTFMove(element));
    }

    return httpBody;
}

Ref<FrameState> toFrameState(const HistoryItem& historyItem)
{
    Ref frameState = FrameState::create();

    frameState->urlString = historyItem.urlString();
    frameState->originalURLString = historyItem.originalURLString();
    frameState->referrer = historyItem.referrer();
    frameState->target = historyItem.target();
    frameState->frameID = historyItem.frameID();

    frameState->setDocumentState(historyItem.documentState());
    if (RefPtr<SerializedScriptValue> stateObject = historyItem.stateObject())
        frameState->stateObjectData = stateObject->wireBytes();

    frameState->documentSequenceNumber = historyItem.documentSequenceNumber();
    frameState->itemSequenceNumber = historyItem.itemSequenceNumber();

    frameState->scrollPosition = historyItem.scrollPosition();
    frameState->shouldRestoreScrollPosition = historyItem.shouldRestoreScrollPosition();
    frameState->pageScaleFactor = historyItem.pageScaleFactor();

    if (FormData* formData = const_cast<HistoryItem&>(historyItem).formData()) {
        HTTPBody httpBody = toHTTPBody(*formData);
        httpBody.contentType = historyItem.formContentType();

        frameState->httpBody = WTFMove(httpBody);
    }

    frameState->identifier = historyItem.identifier();
    frameState->hasCachedPage = historyItem.isInBackForwardCache();
    frameState->shouldOpenExternalURLsPolicy = historyItem.shouldOpenExternalURLsPolicy();
    frameState->sessionStateObject = historyItem.stateObject();
    frameState->wasCreatedByJSWithoutUserInteraction = historyItem.wasCreatedByJSWithoutUserInteraction();
    frameState->wasRestoredFromSession = historyItem.wasRestoredFromSession();
    frameState->policyContainer = historyItem.policyContainer();

    static constexpr auto maxTitleLength = 1000u; // Closest power of 10 above the W3C recommendation for Title length.
    frameState->title = historyItem.title().left(maxTitleLength);

#if PLATFORM(IOS_FAMILY)
    frameState->exposedContentRect = historyItem.exposedContentRect();
    frameState->unobscuredContentRect = historyItem.unobscuredContentRect();
    frameState->minimumLayoutSizeInScrollViewCoordinates = historyItem.minimumLayoutSizeInScrollViewCoordinates();
    frameState->contentSize = historyItem.contentSize();
    frameState->scaleIsInitial = historyItem.scaleIsInitial();
    frameState->obscuredInsets = historyItem.obscuredInsets();
#endif

    frameState->children = historyItem.children().map([](auto& childHistoryItem) {
        return toFrameState(childHistoryItem);
    });

    return frameState;
}

static Ref<FormData> toFormData(const HTTPBody& httpBody)
{
    auto formData = FormData::create();

    for (const auto& element : httpBody.elements) {
        switchOn(element.data, [&] (const Vector<uint8_t>& data) {
            formData->appendData(data.span());
        }, [&] (const HTTPBody::Element::FileData& data) {
            formData->appendFileRange(data.filePath, data.fileStart, data.fileLength.value_or(BlobDataItem::toEndOfFile), data.expectedFileModificationTime);
        }, [&] (const String& blobURLString) {
            formData->appendBlob(URL { blobURLString });
        });
    }

    return formData;
}

static void applyFrameState(HistoryItemClient& client, HistoryItem& historyItem, const FrameState& frameState)
{
    historyItem.setOriginalURLString(frameState.originalURLString);
    historyItem.setReferrer(frameState.referrer);
    historyItem.setTarget(frameState.target);
    historyItem.setFrameID(frameState.frameID);

    historyItem.setDocumentState(frameState.documentState());

    if (frameState.stateObjectData) {
        Vector<uint8_t> stateObjectData = frameState.stateObjectData.value();
        historyItem.setStateObject(SerializedScriptValue::createFromWireBytes(WTFMove(stateObjectData)));
    }

    historyItem.setDocumentSequenceNumber(frameState.documentSequenceNumber);
    historyItem.setItemSequenceNumber(frameState.itemSequenceNumber);

    historyItem.setScrollPosition(frameState.scrollPosition);
    historyItem.setShouldRestoreScrollPosition(frameState.shouldRestoreScrollPosition);
    historyItem.setPageScaleFactor(frameState.pageScaleFactor);

    if (frameState.httpBody) {
        const auto& httpBody = frameState.httpBody.value();
        historyItem.setFormContentType(httpBody.contentType);

        historyItem.setFormData(toFormData(httpBody));
    }

    historyItem.setShouldOpenExternalURLsPolicy(frameState.shouldOpenExternalURLsPolicy);
    historyItem.setStateObject(frameState.sessionStateObject.get());
    historyItem.setWasCreatedByJSWithoutUserInteraction(frameState.wasCreatedByJSWithoutUserInteraction);
    historyItem.setWasRestoredFromSession(frameState.wasRestoredFromSession);
    if (auto policyContainer = frameState.policyContainer)
        historyItem.setPolicyContainer(*policyContainer);

#if PLATFORM(IOS_FAMILY)
    historyItem.setExposedContentRect(frameState.exposedContentRect);
    historyItem.setUnobscuredContentRect(frameState.unobscuredContentRect);
    historyItem.setMinimumLayoutSizeInScrollViewCoordinates(frameState.minimumLayoutSizeInScrollViewCoordinates);
    historyItem.setContentSize(frameState.contentSize);
    historyItem.setScaleIsInitial(frameState.scaleIsInitial);
    historyItem.setObscuredInsets(frameState.obscuredInsets);
#endif

    for (auto& childFrameState : frameState.children) {
        Ref childHistoryItem = HistoryItem::create(client, childFrameState->urlString, { }, { }, childFrameState->identifier);
        applyFrameState(client, childHistoryItem, childFrameState);

        historyItem.addChildItem(WTFMove(childHistoryItem));
    }
}

Ref<HistoryItem> toHistoryItem(HistoryItemClient& client, const FrameState& frameState)
{
    Ref historyItem = HistoryItem::create(client, frameState.urlString, frameState.title, { }, frameState.identifier);
    applyFrameState(client, historyItem, frameState);
    return historyItem;
}

} // namespace WebKit
