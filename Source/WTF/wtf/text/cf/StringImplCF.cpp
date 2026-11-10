/*
 * Copyright (C) 2006, 2009, 2012 Apple Inc. All rights reserved.
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
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include <wtf/text/StringImpl.h>

#if USE(CF)

#include <wtf/DebugHeap.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RetainPtr.h>
#include <wtf/cf/VectorCF.h>
#include <wtf/text/StringBuffer.h>

namespace WTF {

namespace StringWrapperCFAllocator {

    DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER_AND_EXPORT(StringWrapperCFAllocator, WTF_INTERNAL);
    DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StringWrapperCFAllocator);

    NODELETE static RefPtr<StringImpl>& currentString()
    {
        static NeverDestroyed<RefPtr<StringImpl>> currentString;
        return currentString;
    }

    struct StringImplWrapper {
        const RefPtr<StringImpl> m_stringImpl;
    };

    NODELETE static const void* retain(const void* info)
    {
        return info;
    }

    NODELETE NO_RETURN_DUE_TO_ASSERT
    static void release(const void*)
    {
        ASSERT_NOT_REACHED();
    }

    NODELETE static CFStringRef copyDescription(const void*)
    {
        return CFSTR("WTF::String-based allocator");
    }

    static void* allocate(CFIndex size, CFOptionFlags, void*)
    {
        RefPtr<StringImpl> underlyingString;
        if (isMainThread())
            underlyingString = std::exchange(currentString(), nullptr);

        auto [ wrapper, trailingBytes ] = createWithTrailingBytes<StringImplWrapper, StringWrapperCFAllocatorMalloc>(size, StringImplWrapper { WTF::move(underlyingString) });
        return trailingBytes;
    }

    static void* reallocate(void* pointer, CFIndex newSize, CFOptionFlags, void*)
    {
        auto [ prevWrapper, prevTrailingBytes ] = fromTrailingBytes<StringImplWrapper>(pointer);
        auto [ wrapper, trailingBytes ] = reallocWithTrailingBytes<StringImplWrapper, StringWrapperCFAllocatorMalloc>(prevWrapper, newSize);
        ASSERT(!wrapper->m_stringImpl);
        return trailingBytes;
    }

    static void deallocate(void* pointer, void*)
    {
        auto [ wrapper, trailingBytes ] = fromTrailingBytes<StringImplWrapper>(pointer);
        if (!wrapper->m_stringImpl)
            destroyWithTrailingBytes<StringImplWrapper, StringWrapperCFAllocatorMalloc>(wrapper);
        else {
            ensureOnMainThread([wrapper = wrapper] {
                destroyWithTrailingBytes<StringImplWrapper, StringWrapperCFAllocatorMalloc>(wrapper);
            });
        }
    }

    NODELETE static CFIndex preferredSize(CFIndex size, CFOptionFlags, void*)
    {
        // FIXME: If FastMalloc provided a "good size" callback, we'd want to use it here.
        // Note that this optimization would help performance for strings created with the
        // allocator that are mutable, and those typically are only created by callers who
        // make a new string using the old string's allocator, such as some of the call
        // sites in CFURL.
        return size;
    }

    static CFAllocatorRef allocatorSingleton()
    {
        ASSERT(isMainThread());
        static NeverDestroyed allocator = [] {
            initializeMainThread();
            CFAllocatorContext context = { 0, nullptr, retain, release, copyDescription, allocate, reallocate, deallocate, preferredSize };
            return adoptCF(CFAllocatorCreate(nullptr, &context));
        }();
        return allocator.get().get();
    }

}

RetainPtr<CFStringRef> StringImpl::createCFString()
{
    if (!m_length || !isMainThread()) {
        if (is8Bit()) {
            auto characters = span8();
            return adoptCF(CFStringCreateWithBytes(nullptr, byteCast<UInt8>(characters.data()), characters.size(), kCFStringEncodingISOLatin1, false));
        }
        auto characters = span16();
        return adoptCF(CFStringCreateWithCharacters(nullptr, reinterpret_cast<const UniChar*>(characters.data()), characters.size()));
    }

    // Put pointer to the StringImpl in a global so the allocator can store it with the CFString.
    ASSERT(!StringWrapperCFAllocator::currentString());
    StringWrapperCFAllocator::currentString() = this;

    RetainPtr<CFStringRef> string;
    if (is8Bit()) {
        auto characters = span8();
        string = adoptCF(CFStringCreateWithBytesNoCopy(StringWrapperCFAllocator::allocatorSingleton(), byteCast<UInt8>(characters.data()), characters.size(), kCFStringEncodingISOLatin1, false, kCFAllocatorNull));
    } else {
        auto characters = span16();
        string = adoptCF(CFStringCreateWithCharactersNoCopy(StringWrapperCFAllocator::allocatorSingleton(), reinterpret_cast<const UniChar*>(characters.data()), characters.size(), kCFAllocatorNull));
    }
    // CoreFoundation might not have to allocate anything, we clear currentString() in case we did not execute allocate().
    StringWrapperCFAllocator::currentString() = nullptr;

    return string;
}

RefPtr<StringImpl> StringImpl::create(CFStringRef str)
{
    if (!str)
        return nullptr;

    // We cannot use CFIndex here since CFStringGetLength can return values larger than
    // CFIndex can hold.  (<rdar://problem/6806478>)
    size_t size = CFStringGetLength(str);
    if (!size)
        return StringImpl::empty();

    // Fast path: check if CF exposes its internal buffer directly.
    if (auto span = CFStringGetLatin1CStringSpan(str); span.data())
        return StringImpl::create(span);

    if (auto span = CFStringGetCharactersSpan(str); span.data())
        return StringImpl::create8BitIfPossible(span);

    // Slow path: try converting to Latin1, then fall back to UTF-16.
    {
        StringBuffer<Latin1Character> buffer(size);
        CFIndex usedBufLen;
        CFIndex convertedSize = CFStringGetBytes(str, CFRangeMake(0, size), kCFStringEncodingISOLatin1, 0, false, byteCast<UInt8>(buffer.characters()), size, &usedBufLen);
        if (static_cast<size_t>(convertedSize) == size && static_cast<size_t>(usedBufLen) == size)
            return StringImpl::adopt(WTF::move(buffer));
    }

    StringBuffer<char16_t> ucharBuffer(size);
    CFStringCopyCharactersSpan(str, ucharBuffer.span());
    return StringImpl::adopt(WTF::move(ucharBuffer));
}

// On StringImpl creation we could check if the allocator is the StringWrapperCFAllocator.
// If it is, then we could find the original StringImpl and just return that. But to
// do that we'd have to compute the offset from CFStringRef to the allocated block;
// the CFStringRef is *not* at the start of an allocated block. Testing shows 1000x
// more calls to createCFString than calls to the create functions with the appropriate
// allocator, so it's probably not urgent optimize that case.

}

#endif // USE(CF)
