/*
 *  WebURLRequest.cpp
 *  WebKit2
 *
 *  Created by Sam Weinig on 8/30/10.
 *  Copyright 2010 Apple Inc. All rights reserved.
 *
 */

#include "config.h"
#include "WebURLRequest.h"

using namespace WebCore;

namespace WebKit {

PassRefPtr<WebURLRequest> WebURLRequest::create(const KURL& url)
{
    return adoptRef(new WebURLRequest(ResourceRequest(url)));
}

WebURLRequest::WebURLRequest(const ResourceRequest& request)
    : m_request(request)
{
}

} // namespace WebKit
