//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ObjCPtr.h:
//      Implements smart pointer for Objective-C objects

#ifndef COMMON_APPLE_OBJCPTR_H_
#define COMMON_APPLE_OBJCPTR_H_

#ifndef __OBJC__
#    error For Objective-C++ only.
#endif

#import <Foundation/Foundation.h>
#import <type_traits>
#import <utility>
#import "common/platform.h"

namespace angle
{

// Smart pointer for holding Objective-C objects. Use adoptObjCPtr for create functions
// that return owned reference, e.g. functions that begin with 'new', 'copy', 'create'.
template <typename T>
class ObjCPtr
{
  public:
    using PtrType = std::remove_pointer_t<T> *;

    constexpr ObjCPtr() = default;
    constexpr ObjCPtr(std::nullptr_t other) {}
    ObjCPtr(PtrType other);
    ObjCPtr(const ObjCPtr &other);
    template <typename U>
    constexpr ObjCPtr(ObjCPtr<U> &&other);
    ~ObjCPtr();
    ObjCPtr &operator=(const ObjCPtr &other);
    ObjCPtr &operator=(PtrType other);
    template <typename U>
    constexpr ObjCPtr &operator=(ObjCPtr<U> &&other);

    [[nodiscard]] constexpr PtrType leakObject();
    void reset();

    constexpr explicit operator bool() const { return get(); }
    constexpr operator PtrType() const { return get(); }
    constexpr PtrType get() const { return mObject; }
    constexpr void swap(ObjCPtr<T> &other);

    template <typename U>
    friend ObjCPtr<std::remove_pointer_t<U>> adoptObjCPtr(U NS_RELEASES_ARGUMENT);

  private:
    struct AdoptTag
    {};
    constexpr ObjCPtr(PtrType other, AdoptTag);

    PtrType mObject = nil;
};

template <typename T>
ObjCPtr(T) -> ObjCPtr<std::remove_pointer_t<T>>;

template <typename T>
ObjCPtr<T>::ObjCPtr(PtrType other) : mObject(other)
{
#if !__has_feature(objc_arc)
    [mObject retain];
#endif
}

template <typename T>
ObjCPtr<T>::ObjCPtr(const ObjCPtr &other) : ObjCPtr(other.mObject)
{}

template <typename T>
template <typename U>
constexpr ObjCPtr<T>::ObjCPtr(ObjCPtr<U> &&other) : mObject(other.leakObject())
{}

template <typename T>
ObjCPtr<T>::~ObjCPtr()
{
#if !__has_feature(objc_arc)
    [mObject release];
#endif
}

template <typename T>
ObjCPtr<T> &ObjCPtr<T>::operator=(const ObjCPtr &other)
{
    ObjCPtr temp = other;
    swap(temp);
    return *this;
}

template <typename T>
template <typename U>
constexpr ObjCPtr<T> &ObjCPtr<T>::operator=(ObjCPtr<U> &&other)
{
    ObjCPtr temp = std::move(other);
    swap(temp);
    return *this;
}

template <typename T>
ObjCPtr<T> &ObjCPtr<T>::operator=(PtrType other)
{
    ObjCPtr temp = other;
    swap(temp);
    return *this;
}

template <typename T>
constexpr ObjCPtr<T>::ObjCPtr(PtrType other, AdoptTag) : mObject(other)
{}

template <typename T>
constexpr void ObjCPtr<T>::swap(ObjCPtr<T> &other)
{
    // std::swap is constexpr only in c++20.
    auto object   = other.mObject;
    other.mObject = mObject;
    mObject       = object;
}

template <typename T>
constexpr typename ObjCPtr<T>::PtrType ObjCPtr<T>::leakObject()
{
    // std::exchange is constexper only in c++20.
    auto object = mObject;
    mObject     = nullptr;
    return object;
}

template <typename T>
void ObjCPtr<T>::reset()
{
    *this = {};
}

template <typename T, typename U>
constexpr bool operator==(const ObjCPtr<T> &a, const ObjCPtr<U> &b)
{
    return a.get() == b.get();
}

template <typename T, typename U>
constexpr bool operator==(const ObjCPtr<T> &a, U *b)
{
    return a.get() == b;
}

template <typename T, typename U>
constexpr bool operator==(T *a, const ObjCPtr<U> &b)
{
    return a == b.get();
}

template <typename U>
ObjCPtr<std::remove_pointer_t<U>> adoptObjCPtr(U NS_RELEASES_ARGUMENT other)
{
    using ResultType = ObjCPtr<std::remove_pointer_t<U>>;
    return ResultType(other, typename ResultType::AdoptTag{});
}

}  // namespace angle

#endif
