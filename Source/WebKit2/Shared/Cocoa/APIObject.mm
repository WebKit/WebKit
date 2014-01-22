/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#import "APIObject.h"

#if WK_API_ENABLED

#import "WKBackForwardListInternal.h"
#import "WKBackForwardListItemInternal.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WKBrowsingContextGroupInternal.h"
#import "WKConnectionInternal.h"
#import "WKNSArray.h"
#import "WKNSData.h"
#import "WKNSDictionary.h"
#import "WKNSError.h"
#import "WKNSString.h"
#import "WKNSURL.h"
#import "WKNSURLAuthenticationChallenge.h"
#import "WKNSURLProtectionSpace.h"
#import "WKNavigationDataInternal.h"
#import "WKProcessClassInternal.h"
#import "WKWebProcessPlugInBrowserContextControllerInternal.h"
#import "WKWebProcessPlugInFrameInternal.h"
#import "WKWebProcessPlugInHitTestResultInternal.h"
#import "WKWebProcessPlugInInternal.h"
#import "WKWebProcessPlugInNodeHandleInternal.h"
#import "WKWebProcessPlugInPageGroupInternal.h"
#import "WKWebProcessPlugInScriptWorldInternal.h"

namespace API {

void Object::ref()
{
    [wrapper() retain];
}

void Object::deref()
{
    [wrapper() release];
}

void* Object::newObject(size_t size, Type type)
{
    NSObject <WKObject> *wrapper;

    // Wrappers that inherit from WKObject store the API::Object in their extra bytes, so they are
    // allocated using NSAllocatedObject. The other wrapper classes contain inline storage for the
    // API::Object, so they are allocated using +alloc.

    switch (type) {
    case Type::Array:
        wrapper = [WKNSArray alloc];
        break;

    case Type::AuthenticationChallenge:
        wrapper = NSAllocateObject([WKNSURLAuthenticationChallenge self], size, nullptr);
        break;

    case Type::BackForwardList:
        wrapper = [WKBackForwardList alloc];
        break;

    case Type::BackForwardListItem:
        wrapper = [WKBackForwardListItem alloc];
        break;

    case Type::Bundle:
        wrapper = [WKWebProcessPlugInController alloc];
        break;

    case Type::BundlePage:
        wrapper = [WKWebProcessPlugInBrowserContextController alloc];
        break;

    case Type::Connection:
        wrapper = NSAllocateObject([WKConnection self], size, nullptr);
        break;

    case Type::Context:
        wrapper = [WKProcessClass alloc];
        break;

    case Type::Data:
        wrapper = [WKNSData alloc];
        break;

    case Type::Dictionary:
        wrapper = [WKNSDictionary alloc];
        break;

    case Type::Error:
        wrapper = NSAllocateObject([WKNSError self], size, nullptr);
        break;

    case Type::NavigationData:
        wrapper = [WKNavigationData alloc];
        break;

    case Type::Page:
        wrapper = [WKBrowsingContextController alloc];
        break;

    case Type::PageGroup:
        wrapper = [WKBrowsingContextGroup alloc];
        break;

    case Type::ProtectionSpace:
        wrapper = NSAllocateObject([WKNSURLProtectionSpace class], size, nullptr);
        break;

    case Type::String:
        wrapper = NSAllocateObject([WKNSString class], size, nullptr);
        break;

    case Type::URL:
        wrapper = NSAllocateObject([WKNSURL class], size, nullptr);
        break;

    case Type::BundleFrame:
        wrapper = [WKWebProcessPlugInFrame alloc];
        break;

    case Type::BundleHitTestResult:
        wrapper = [WKWebProcessPlugInHitTestResult alloc];
        break;

    case Type::BundleNodeHandle:
        wrapper = [WKWebProcessPlugInNodeHandle alloc];
        break;

    case Type::BundlePageGroup:
        wrapper = [WKWebProcessPlugInPageGroup alloc];
        break;

    case Type::BundleScriptWorld:
        wrapper = [WKWebProcessPlugInScriptWorld alloc];
        break;

    default:
        wrapper = NSAllocateObject([WKObject class], size, nullptr);
        break;
    }

    Object& object = wrapper._apiObject;
    object.m_wrapper = wrapper;

    return &object;
}

} // namespace API

#endif // WK_API_ENABLED
