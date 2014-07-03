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

using namespace WebCore;

namespace WebKit {

static HTTPBody toHTTPBody(const FormData& formData)
{
    HTTPBody httpBody;

    for (const auto& formDataElement : formData.elements()) {
        HTTPBody::Element element;

        switch (formDataElement.m_type) {
        case FormDataElement::Type::Data:
            element.type = HTTPBody::Element::Type::Data;
            element.data = formDataElement.m_data;
            break;

        case FormDataElement::Type::EncodedFile:
            element.filePath = formDataElement.m_filename;
            element.fileStart = formDataElement.m_fileStart;
            if (formDataElement.m_fileLength != BlobDataItem::toEndOfFile)
                element.fileLength = formDataElement.m_fileLength;
            if (formDataElement.m_expectedFileModificationTime != invalidFileTime())
                element.expectedFileModificationTime = formDataElement.m_expectedFileModificationTime;
            break;

        case FormDataElement::Type::EncodedBlob:
            element.blobURLString = formDataElement.m_url.string();
            break;
        }

        httpBody.elements.append(WTF::move(element));
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

    frameState.scrollPoint = historyItem.scrollPoint();
    frameState.pageScaleFactor = historyItem.pageScaleFactor();

    if (FormData* formData = const_cast<HistoryItem&>(historyItem).formData()) {
        HTTPBody httpBody = toHTTPBody(*formData);
        httpBody.contentType = historyItem.formContentType();

        frameState.httpBody = WTF::move(httpBody);
    }

#if PLATFORM(IOS)
    frameState.exposedContentRect = historyItem.exposedContentRect();
    frameState.unobscuredContentRect = historyItem.unobscuredContentRect();
    frameState.minimumLayoutSizeInScrollViewCoordinates = historyItem.minimumLayoutSizeInScrollViewCoordinates();
    frameState.contentSize = historyItem.contentSize();
    frameState.scaleIsInitial = historyItem.scaleIsInitial();
#endif

    for (auto& childHistoryItem : historyItem.children()) {
        FrameState childFrameState = toFrameState(*childHistoryItem);
        frameState.children.append(WTF::move(childFrameState));
    }

    return frameState;
}

PageState toPageState(const WebCore::HistoryItem& historyItem)
{
    PageState pageState;

    pageState.title = historyItem.title();
    pageState.mainFrameState = toFrameState(historyItem);

    return pageState;
}

static PassRefPtr<FormData> toFormData(const HTTPBody& httpBody)
{
    RefPtr<FormData> formData = FormData::create();

    for (const auto& element : httpBody.elements) {
        switch (element.type) {
        case HTTPBody::Element::Type::Data:
            formData->appendData(element.data.data(), element.data.size());
            break;

        case HTTPBody::Element::Type::File:
            formData->appendFileRange(element.filePath, element.fileStart, element.fileLength.valueOr(BlobDataItem::toEndOfFile), element.expectedFileModificationTime.valueOr(invalidFileTime()));
            break;

        case HTTPBody::Element::Type::Blob:
            formData->appendBlob(URL(URL(), element.blobURLString));
            break;
        }
    }

    return formData.release();
}

static void applyFrameState(HistoryItem& historyItem, const FrameState& frameState)
{
    historyItem.setOriginalURLString(frameState.originalURLString);
    historyItem.setReferrer(frameState.referrer);
    historyItem.setTarget(frameState.target);

    historyItem.setDocumentState(frameState.documentState);

    if (frameState.stateObjectData) {
        Vector<uint8_t> stateObjectData = frameState.stateObjectData.value();
        historyItem.setStateObject(SerializedScriptValue::adopt(stateObjectData));
    }

    historyItem.setDocumentSequenceNumber(frameState.documentSequenceNumber);
    historyItem.setItemSequenceNumber(frameState.itemSequenceNumber);

    historyItem.setScrollPoint(frameState.scrollPoint);
    historyItem.setPageScaleFactor(frameState.pageScaleFactor);

    if (frameState.httpBody) {
        const auto& httpBody = frameState.httpBody.value();
        historyItem.setFormContentType(httpBody.contentType);

        historyItem.setFormData(toFormData(httpBody));
    }

#if PLATFORM(IOS)
    historyItem.setExposedContentRect(frameState.exposedContentRect);
    historyItem.setUnobscuredContentRect(frameState.unobscuredContentRect);
    historyItem.setMinimumLayoutSizeInScrollViewCoordinates(frameState.minimumLayoutSizeInScrollViewCoordinates);
    historyItem.setContentSize(frameState.contentSize);
    historyItem.setScaleIsInitial(frameState.scaleIsInitial);
#endif

    for (const auto& childFrameState : frameState.children) {
        RefPtr<HistoryItem> childHistoryItem = HistoryItem::create(childFrameState.urlString, String());
        applyFrameState(*childHistoryItem, childFrameState);

        historyItem.addChildItem(childHistoryItem.release());
    }
}

PassRefPtr<HistoryItem> toHistoryItem(const PageState& pageState)
{
    RefPtr<HistoryItem> historyItem = HistoryItem::create(pageState.mainFrameState.urlString, pageState.title);
    applyFrameState(*historyItem, pageState.mainFrameState);

    return historyItem.release();
}

} // namespace WebKit
