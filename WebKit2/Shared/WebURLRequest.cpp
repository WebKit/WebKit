/*
 *  WebURLRequest.cpp
 *  WebKit2
 *
 *  Created by Sam Weinig on 8/30/10.
 *  Copyright 2010 Apple Inc. All rights reserved.
 *
 */

#include "WebURLRequest.h"

namespace WebKit {

WebURLRequest::WebURLRequest(const WebCore::KURL& url)
    : m_request(url)
{
}

} // namespace WebKit
