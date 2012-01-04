/*
 * Copyright (C) 2009, 2010, 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "ResourceRequest.h"

#include "BlobRegistryImpl.h"
#include <BlackBerryPlatformClient.h>
#include <network/NetworkRequest.h>
#include <wtf/text/CString.h>

using BlackBerry::Platform::NetworkRequest;

namespace WebCore {

unsigned initializeMaximumHTTPConnectionCountPerHost()
{
    return 6;
}

static inline NetworkRequest::CachePolicy platformCachePolicyForRequest(const ResourceRequest& request)
{
    switch (request.cachePolicy()) {
    case WebCore::UseProtocolCachePolicy:
        return NetworkRequest::UseProtocolCachePolicy;
    case WebCore::ReloadIgnoringCacheData:
        return NetworkRequest::ReloadIgnoringCacheData;
    case WebCore::ReturnCacheDataElseLoad:
        return NetworkRequest::ReturnCacheDataElseLoad;
    case WebCore::ReturnCacheDataDontLoad:
        return NetworkRequest::ReturnCacheDataDontLoad;
    default:
        ASSERT_NOT_REACHED();
        return NetworkRequest::UseProtocolCachePolicy;
    }
}

static inline NetworkRequest::TargetType platformTargetTypeForRequest(const ResourceRequest& request)
{
    if (request.isXMLHTTPRequest())
        return NetworkRequest::TargetIsXMLHTTPRequest;

    switch (request.targetType()) {
    case ResourceRequest::TargetIsMainFrame:
        return NetworkRequest::TargetIsMainFrame;
    case ResourceRequest::TargetIsSubframe:
        return NetworkRequest::TargetIsSubframe;
    case ResourceRequest::TargetIsSubresource:
        return NetworkRequest::TargetIsSubresource;
    case ResourceRequest::TargetIsStyleSheet:
        return NetworkRequest::TargetIsStyleSheet;
    case ResourceRequest::TargetIsScript:
        return NetworkRequest::TargetIsScript;
    case ResourceRequest::TargetIsFontResource:
        return NetworkRequest::TargetIsFontResource;
    case ResourceRequest::TargetIsImage:
        return NetworkRequest::TargetIsImage;
    case ResourceRequest::TargetIsObject:
        return NetworkRequest::TargetIsObject;
    case ResourceRequest::TargetIsMedia:
        return NetworkRequest::TargetIsMedia;
    case ResourceRequest::TargetIsWorker:
        return NetworkRequest::TargetIsWorker;
    case ResourceRequest::TargetIsSharedWorker:
        return NetworkRequest::TargetIsSharedWorker;
    default:
        ASSERT_NOT_REACHED();
        return NetworkRequest::TargetIsUnknown;
    }
}

void ResourceRequest::initializePlatformRequest(NetworkRequest& platformRequest, bool isInitial) const
{
    // If this is the initial load, skip the request body and headers.
    if (isInitial)
        platformRequest.setRequestInitial(timeoutInterval());
    else {
        platformRequest.setRequestUrl(url().string().utf8().data(),
                httpMethod().latin1().data(),
                platformCachePolicyForRequest(*this),
                platformTargetTypeForRequest(*this),
                timeoutInterval());

        if (httpBody() && !httpBody()->isEmpty()) {
            const Vector<FormDataElement>& elements = httpBody()->elements();
            // Use setData for simple forms because it is slightly more efficient.
            if (elements.size() == 1 && elements[0].m_type == FormDataElement::data)
                platformRequest.setData(elements[0].m_data.data(), elements[0].m_data.size());
            else {
                for (unsigned i = 0; i < elements.size(); ++i) {
                    const FormDataElement& element = elements[i];
                    if (element.m_type == FormDataElement::data)
                        platformRequest.addMultipartData(element.m_data.data(), element.m_data.size());
                    else if (element.m_type == FormDataElement::encodedFile)
                        platformRequest.addMultipartFilename(element.m_filename.characters(), element.m_filename.length());
#if ENABLE(BLOB)
                    else if (element.m_type == FormDataElement::encodedBlob) {
                        RefPtr<BlobStorageData> blobData = static_cast<BlobRegistryImpl&>(blobRegistry()).getBlobDataFromURL(KURL(ParsedURLString, element.m_blobURL));
                        if (blobData) {
                            for (size_t j = 0; j < blobData->items().size(); ++j) {
                                const BlobDataItem& blobItem = blobData->items()[j];
                                if (blobItem.type == BlobDataItem::Data)
                                    platformRequest.addMultipartData(blobItem.data->data() + static_cast<int>(blobItem.offset), static_cast<int>(blobItem.length));
                                else {
                                    ASSERT(blobItem.type == BlobDataItem::File);
                                    platformRequest.addMultipartFilename(blobItem.path.characters(), blobItem.path.length(), blobItem.offset, blobItem.length, blobItem.expectedModificationTime);
                                }
                            }
                        }
                    }
#endif
                    else
                        ASSERT_NOT_REACHED(); // unknown type
                }
            }
        }

        for (HTTPHeaderMap::const_iterator it = httpHeaderFields().begin(); it != httpHeaderFields().end(); ++it) {
            String key = it->first;
            String value = it->second;
            if (!key.isEmpty() && !value.isEmpty())
                platformRequest.addHeader(key.latin1().data(), value.latin1().data());
        }

        // Locale has the form "en-US". Construct accept language like "en-US, en;q=0.8".
        std::string locale = BlackBerry::Platform::Client::get()->getLocale();
        // POSIX locale has '_' instead of '-'.
        // Replace to conform to HTTP spec.
        size_t underscore = locale.find('_');
        if (underscore != std::string::npos)
            locale.replace(underscore, 1, "-");
        std::string acceptLanguage = locale + ", " + locale.substr(0, 2) + ";q=0.8";
        platformRequest.addHeader("Accept-Language", acceptLanguage.c_str());
    }
}

PassOwnPtr<CrossThreadResourceRequestData> ResourceRequest::doPlatformCopyData(PassOwnPtr<CrossThreadResourceRequestData> data) const
{
    data->m_token = m_token;
    data->m_anchorText = m_anchorText;
    data->m_isXMLHTTPRequest = m_isXMLHTTPRequest;
    data->m_mustHandleInternally = m_mustHandleInternally;
    return data;
}

void ResourceRequest::doPlatformAdopt(PassOwnPtr<CrossThreadResourceRequestData> data)
{
    m_token = data->m_token;
    m_anchorText = data->m_anchorText;
    m_isXMLHTTPRequest = data->m_isXMLHTTPRequest;
    m_mustHandleInternally = data->m_mustHandleInternally;
    m_forceDownload = data->m_forceDownload;
}

} // namespace WebCore
