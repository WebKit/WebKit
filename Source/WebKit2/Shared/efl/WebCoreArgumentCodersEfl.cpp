/*
 * Copyright (C) 2012 Samsung Electronics
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include "WebCoreArgumentCoders.h"

#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>

using namespace WebCore;

namespace CoreIPC {

void ArgumentCoder<ResourceRequest>::encode(ArgumentEncoder* encoder, const ResourceRequest& resourceRequest)
{
    encoder->encode(resourceRequest.url().string());
    encoder->encode(resourceRequest.httpHeaderField("Cookie"));
}

bool ArgumentCoder<ResourceRequest>::decode(ArgumentDecoder* decoder, ResourceRequest& resourceRequest)
{
    ResourceRequest request;

    String url;
    if (!decoder->decode(url))
        return false;
    request.setURL(KURL(KURL(), url));

    String cookie;
    if (!decoder->decode(cookie))
        return false;
    request.setHTTPHeaderField("Cookie", cookie);

    resourceRequest = request;
    return true;
}


void ArgumentCoder<ResourceResponse>::encode(ArgumentEncoder* encoder, const ResourceResponse& resourceResponse)
{
    encoder->encode(resourceResponse.mimeType());
}

bool ArgumentCoder<ResourceResponse>::decode(ArgumentDecoder* decoder, ResourceResponse& resourceResponse)
{
    ResourceResponse response;

    String mimeType;
    if (!decoder->decode(mimeType))
        return false;
    response.setMimeType(mimeType);

    resourceResponse = response;
    return true;
}


void ArgumentCoder<ResourceError>::encode(ArgumentEncoder* encoder, const ResourceError& resourceError)
{
    encoder->encode(resourceError.domain());
    encoder->encode(resourceError.errorCode());
    encoder->encode(resourceError.failingURL());
    encoder->encode(resourceError.localizedDescription());
}

bool ArgumentCoder<ResourceError>::decode(ArgumentDecoder* decoder, ResourceError& resourceError)
{
    String domain;
    if (!decoder->decode(domain))
        return false;

    int errorCode;
    if (!decoder->decode(errorCode))
        return false;

    String failingURL;
    if (!decoder->decode(failingURL))
        return false;

    String localizedDescription;
    if (!decoder->decode(localizedDescription))
        return false;

    resourceError = ResourceError(domain, errorCode, failingURL, localizedDescription);
    return true;
}

} // namespace CoreIPC
