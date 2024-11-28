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


#import "Logging.h"
#import "WKFoundation.h"

#import <type_traits>
#import <wtf/ObjCRuntimeExtras.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/spi/cocoa/objcSPI.h>

namespace API {

class Object;

template<typename ObjectClass> struct ObjectStorage {
    ObjectClass* get() { return reinterpret_cast<ObjectClass*>(&data); }
    ObjectClass& operator*() { return *get(); }
    ObjectClass* operator->() { return get(); }

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    typename std::aligned_storage<sizeof(ObjectClass), std::alignment_of<ObjectClass>::value>::type data;
    ALLOW_DEPRECATED_DECLARATIONS_END
};

API::Object* unwrap(void*);
void* wrap(API::Object*);

}

namespace WebKit {

template<typename WrappedObjectClass> struct WrapperTraits;

template<typename DestinationClass, typename SourceClass> inline DestinationClass *checkedObjCCast(SourceClass *source)
{
    return checked_objc_cast<DestinationClass>(source);
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

template<typename ObjectClass> inline RetainPtr<typename WrapperTraits<ObjectClass>::WrapperClass> wrapper(Ref<ObjectClass>&& object)
{
    return wrapper(object.get());
}

template<typename ObjectClass> inline RetainPtr<typename WrapperTraits<ObjectClass>::WrapperClass> wrapper(RefPtr<ObjectClass>&& object)
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

#if HAVE(OBJC_CUSTOM_DEALLOC)

// This macro ensures WebKit ObjC objects of a specified class are deallocated on the main thread.
// Use this macro in the ObjC implementation file.

#define WK_OBJECT_DEALLOC_ON_MAIN_THREAD(objcClass) \
+ (void)initialize \
{ \
    if (self == objcClass.class) \
        _class_setCustomDeallocInitiation(self); \
} \
\
- (void)_objc_initiateDealloc \
{ \
    if (isMainRunLoop()) \
        _objc_deallocOnMainThreadHelper((__bridge void *)self); \
    else \
        dispatch_async_f(dispatch_get_main_queue(), (__bridge void *)self, _objc_deallocOnMainThreadHelper); \
} \
\
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

// This macro ensures WebKit ObjC objects and their C++ implementation are safely deallocated on the main thread.
// Use this macro in the ObjC implementation file if you don't require a custom dealloc method.

#define WK_OBJECT_DEALLOC_IMPL_ON_MAIN_THREAD(objcClass, implClass, storageVar) \
WK_OBJECT_DEALLOC_ON_MAIN_THREAD(objcClass); \
\
- (void)dealloc \
{ \
    ASSERT(isMainRunLoop()); \
    storageVar->~implClass(); \
} \
\
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#else

#define WK_OBJECT_DEALLOC_ON_MAIN_THREAD(objcClass) \
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#define WK_OBJECT_DEALLOC_IMPL_ON_MAIN_THREAD(objcClass, implClass, storageVar) \
- (void)dealloc \
{ \
    ASSERT(isMainRunLoop()); \
    storageVar->~implClass(); \
} \
\
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#endif // HAVE(OBJC_CUSTOM_DEALLOC)

#define WK_OBJECT_DISABLE_DISABLE_KVC_IVAR_ACCESS \
+ (BOOL)accessInstanceVariablesDirectly \
{ \
    return !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ThrowOnKVCInstanceVariableAccess); \
} \
using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int
