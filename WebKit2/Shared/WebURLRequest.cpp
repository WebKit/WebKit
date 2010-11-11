/*
 *  WebURLRequest.cpp
 *  WebKit2
 *
 *  Created by Sam Weinig on 8/30/10.
 *  Copyright 2010 Apple Inc. All rights reserved.
 *
 */

#include "WebURLRequest.h"

using namespace WebCore;

namespace WebKit {

WebURLRequest::WebURLRequest(const ResourceRequest& request)
    : m_request(request)
{
}

} // namespace WebKit
