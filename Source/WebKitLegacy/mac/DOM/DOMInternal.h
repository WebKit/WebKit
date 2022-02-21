/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2006 James G. Speth (speth@end.com)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "DOMNodeFilter.h"
#import "DOMObject.h"
#import "DOMXPathNSResolver.h"
#import <wtf/Forward.h>
#import <wtf/WallTime.h>

namespace JSC {
    class JSObject;
    namespace Bindings {
        class RootObject;
    }
}

namespace WebCore {
    class NodeFilter;
    class XPathNSResolver;
#if ENABLE(TOUCH_EVENTS)
    class Touch;
#endif
}

@interface DOMNodeFilter : DOMObject <DOMNodeFilter>
@end

@interface DOMNativeXPathNSResolver : DOMObject <DOMXPathNSResolver>
@end

// Helper functions for DOM wrappers and gluing to Objective-C

void initializeDOMWrapperHooks();

NSObject* getDOMWrapper(DOMObjectInternal*);
void addDOMWrapper(NSObject* wrapper, DOMObjectInternal*);
void removeDOMWrapper(DOMObjectInternal*);

template <class Source>
inline id getDOMWrapper(Source impl)
{
    return getDOMWrapper(reinterpret_cast<DOMObjectInternal*>(impl));
}

template <class Source>
inline void addDOMWrapper(NSObject* wrapper, Source impl)
{
    addDOMWrapper(wrapper, reinterpret_cast<DOMObjectInternal*>(impl));
}

DOMNodeFilter *kit(WebCore::NodeFilter*);
WebCore::NodeFilter* core(DOMNodeFilter *);

DOMNativeXPathNSResolver *kit(WebCore::XPathNSResolver*);
WebCore::XPathNSResolver* core(DOMNativeXPathNSResolver *);

inline NSTimeInterval kit(WallTime time)
{
    return time.secondsSinceEpoch().value() - NSTimeIntervalSince1970;
}

inline WallTime core(NSTimeInterval sec)
{
    return WallTime::fromRawSeconds(sec + NSTimeIntervalSince1970);
}
