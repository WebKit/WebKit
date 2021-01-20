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
            [&] (const Vector<char>& bytes) {
                element.type = HTTPBody::Element::Type::Data;
                element.data = bytes;
            }, [&] (const FormDataElement::EncodedFileData& fileData) {
                element.filePath = fileData.filename;
                element.fileStart = fileData.fileStart;
                if (fileData.fileLength != BlobDataItem::toEndOfFile)
                    element.fileLength = fileData.fileLength;
                element.expectedFileModificationTime = fileData.expectedFileModificationTime;
            }, [&] (const FormDataElement::EncodedBlobData& blobData) {
                element.blobURLString = blobData.url.string();
            }
        );

        httpBody.elements.append(WTFMove(element));
    }

    return httpBody;
}

static FrameState toFrameState(const HistoryItem& historyItem)
{
    FrameState frameState;

    frameState.urlString = historyItem.urlString();
    frameState.originalURLString = historyItem.originalURLString();
    frameState.referrer = historyItem.referrer();
    frameState.target = historyItem.target();

    frameState.documentState = historyItem.documentState();
    if (RefPtr<SerializedScriptValue> stateObject = historyItem.stateObject())
        frameState.stateObjectData = stateObject->data();

    frameState.documentSequenceNumber = historyItem.documentSequenceNumber();
    frameState.itemSequenceNumber = historyItem.itemSequenceNumber();

    frameState.scrollPosition = historyItem.scrollPosition();
    frameState.shouldRestoreScrollPosition = historyItem.shouldRestoreScrollPosition();
    frameState.pageScaleFactor = historyItem.pageScaleFactor();

    if (FormData* formData = const_cast<HistoryItem&>(historyItem).formData()) {
        HTTPBody httpBody = toHTTPBody(*formData);
        httpBody.contentType = historyItem.formContentType();

        frameState.httpBody = WTFMove(httpBody);
    }

#if PLATFORM(IOS_FAMILY)
    frameState.exposedContentRect = historyItem.exposedContentRect();
    frameState.unobscuredContentRect = historyItem.unobscuredContentRect();
    frameState.minimumLayoutSizeInScrollViewCoordinates = historyItem.minimumLayoutSizeInScrollViewCoordinates();
    frameState.contentSize = historyItem.contentSize();
    frameState.scaleIsInitial = historyItem.scaleIsInitial();
    frameState.obscuredInsets = historyItem.obscuredInsets();
#endif

    for (auto& childHistoryItem : historyItem.children()) {
        FrameState childFrameState = toFrameState(childHistoryItem);
        frameState.children.append(WTFMove(childFrameState));
    }

    return frameState;
}

BackForwardListItemState toBackForwardListItemState(const WebCore::HistoryItem& historyItem)
{
    BackForwardListItemState state;
    state.identifier = historyItem.identifier();
    state.pageState.title = historyItem.title();
    state.pageState.mainFrameState = toFrameState(historyItem);
    state.pageState.shouldOpenExternalURLsPolicy = historyItem.shouldOpenExternalURLsPolicy();
    state.pageState.sessionStateObject = historyItem.stateObject();
    state.hasCachedPage = historyItem.isInBackForwardCache();
    return state;
}

static Ref<FormData> toFormData(const HTTPBody& httpBody)
{
    auto formData = FormData::create();

    for (const auto& element : httpBody.elements) {
        switch (element.type) {
        case HTTPBody::Element::Type::Data:
            formData->appendData(element.data.data(), element.data.size());
            break;

        case HTTPBody::Element::Type::File:
            formData->appendFileRange(element.filePath, element.fileStart, element.fileLength.valueOr(BlobDataItem::toEndOfFile), element.expectedFileModificationTime);
            break;

        case HTTPBody::Element::Type::Blob:
            formData->appendBlob(URL(URL(), element.blobURLString));
            break;
        }
    }

    return formData;
}

static void applyFrameState(HistoryItem& historyItem, const FrameState& frameState)
{
    historyItem.setOriginalURLString(frameState.originalURLString);
    historyItem.setReferrer(frameState.referrer);
    historyItem.setTarget(frameState.target);

    historyItem.setDocumentState(frameState.documentState);

    if (frameState.stateObjectData) {
        Vector<uint8_t> stateObjectData = frameState.stateObjectData.value();
        historyItem.setStateObject(SerializedScriptValue::adopt(WTFMove(stateObjectData)));
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

#if PLATFORM(IOS_FAMILY)
    historyItem.setExposedContentRect(frameState.exposedContentRect);
    historyItem.setUnobscuredContentRect(frameState.unobscuredContentRect);
    historyItem.setMinimumLayoutSizeInScrollViewCoordinates(frameState.minimumLayoutSizeInScrollViewCoordinates);
    historyItem.setContentSize(frameState.contentSize);
    historyItem.setScaleIsInitial(frameState.scaleIsInitial);
    historyItem.setObscuredInsets(frameState.obscuredInsets);
#endif

    for (const auto& childFrameState : frameState.children) {
        Ref<HistoryItem> childHistoryItem = HistoryItem::create(childFrameState.urlString, { }, { });
        applyFrameState(childHistoryItem, childFrameState);

        historyItem.addChildItem(WTFMove(childHistoryItem));
    }
}

Ref<HistoryItem> toHistoryItem(const BackForwardListItemState& itemState)
{
    Ref<HistoryItem> historyItem = HistoryItem::create(itemState.pageState.mainFrameState.urlString, itemState.pageState.title, { }, itemState.identifier);
    historyItem->setShouldOpenExternalURLsPolicy(itemState.pageState.shouldOpenExternalURLsPolicy);
    historyItem->setStateObject(itemState.pageState.sessionStateObject.get());
    applyFrameState(historyItem, itemState.pageState.mainFrameState);

    return historyItem;
}

} // namespace WebKit
