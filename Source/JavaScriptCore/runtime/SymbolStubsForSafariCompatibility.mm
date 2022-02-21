/*
* Copyright (C) 2021 Apple Inc. All rights reserved.
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

#import "JSExportMacros.h"
#import <wtf/PlatformHave.h>

#if HAVE(SAFARI_FOR_WEBKIT_DEVELOPMENT_REQUIRING_EXTRA_SYMBOLS)

// These linker symbols are needed to launch SafariForWebKitDevelopment on Catalina and Big Sur with an open source WebKit build.
// Remove them after rdar://problem/74245355 is fixed or after we release a Safari version that doesn't need them any more.

namespace WTF {

class String { };

template<typename, typename> class RefPtr { };
template<typename> struct DumbPtrTraits { };
template<typename> class Optional { };

namespace JSONImpl {
class Array { };
class Value { };
class Object { };
class ObjectBase {
    JS_EXPORT_PRIVATE void getArray(String const&, RefPtr<Array, DumbPtrTraits<Array>>&) const;
    JS_EXPORT_PRIVATE void getValue(String const&, RefPtr<Value, DumbPtrTraits<Value>>&) const;
    JS_EXPORT_PRIVATE void getObject(String const&, RefPtr<Object, DumbPtrTraits<Object>>&) const;
};
void ObjectBase::getArray(String const&, RefPtr<Array, DumbPtrTraits<Array>>&) const { }
void ObjectBase::getValue(String const&, RefPtr<Value, DumbPtrTraits<Value>>&) const { }
void ObjectBase::getObject(String const&, RefPtr<Object, DumbPtrTraits<Object>>&) const { }
} // namespace JSONImpl

} // namespace WTF

namespace Inspector {

class BackendDispatcher {
    enum CommonErrorCode { NotUsed };

    JS_EXPORT_PRIVATE void sendResponse(long, WTF::RefPtr<WTF::JSONImpl::Object, WTF::DumbPtrTraits<WTF::JSONImpl::Object>>&&, bool);
    JS_EXPORT_PRIVATE void reportProtocolError(WTF::Optional<long>, CommonErrorCode, const WTF::String&);
};
void BackendDispatcher::sendResponse(long, WTF::RefPtr<WTF::JSONImpl::Object, WTF::DumbPtrTraits<WTF::JSONImpl::Object>>&&, bool) { }
void BackendDispatcher::reportProtocolError(WTF::Optional<long>, CommonErrorCode, const WTF::String&) { }

} // namespace Inspector

#endif
