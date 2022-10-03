/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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

#import "WKFoundation.h"

#import <type_traits>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>

namespace API {

class Object;

template<typename ObjectClass> struct ObjectStorage {
    ObjectClass* get() { return reinterpret_cast<ObjectClass*>(&data); }
    ObjectClass& operator*() { return *get(); }
    ObjectClass* operator->() { return get(); }

    typename std::aligned_storage<sizeof(ObjectClass), std::alignment_of<ObjectClass>::value>::type data;
};

API::Object* unwrap(void*);
void* wrap(API::Object*);

}

namespace WebKit {

template<typename WrappedObjectClass> struct WrapperTraits;

template<typename DestinationClass, typename SourceClass> inline DestinationClass *checkedObjCCast(SourceClass *source)
{
    ASSERT([source isKindOfClass:[DestinationClass class]]);
    return (DestinationClass *)source;
}

template<typename ObjectClass> inline typename WrapperTraits<ObjectClass>::WrapperClass *wrapper(ObjectClass& object)
{
    return checkedObjCCast<typename WrapperTraits<ObjectClass>::WrapperClass>(object.wrapper());
}

template<typename ObjectClass> inline typename WrapperTraits<ObjectClass>::WrapperClass *wrapper(ObjectClass* object)
{
    return object ? wrapper(*object) : nil;
}

template<typename ObjectClass> inline typename WrapperTraits<ObjectClass>::WrapperClass *wrapper(const Ref<ObjectClass>& object)
{
    return wrapper(object.get());
}

template<typename ObjectClass> inline typename WrapperTraits<ObjectClass>::WrapperClass *wrapper(const RefPtr<ObjectClass>& object)
{
    return wrapper(object.get());
}

template<typename ObjectClass> inline typename WrapperTraits<ObjectClass>::WrapperClass *wrapper(Ref<ObjectClass>&& object)
{
    return adoptNS(wrapper(object.leakRef())).autorelease();
}

template<typename ObjectClass> inline typename WrapperTraits<ObjectClass>::WrapperClass *wrapper(RefPtr<ObjectClass>&& object)
{
    return object ? wrapper(object.releaseNonNull()) : nil;
}

}

namespace API {

using WebKit::wrapper;

}

@protocol WKObject <NSObject>

@property (readonly) API::Object& _apiObject;

@end

@interface WKObject : NSProxy <WKObject>

- (NSObject *)_web_createTarget NS_RETURNS_RETAINED;

@end
