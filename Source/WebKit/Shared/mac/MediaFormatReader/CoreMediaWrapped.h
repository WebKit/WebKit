/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEBM_FORMAT_READER)

#include <pal/spi/cocoa/MediaToolboxSPI.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/cf/TypeCastsCF.h>

namespace WebKit {

template<typename> struct CoreMediaTraits;

#define DECLARE_CORE_MEDIA_TRAITS(ClassName) \
namespace WebKit { \
class Media##ClassName; \
template<> struct CoreMediaTraits<Media##ClassName> { \
    using Class = MTPlugin##ClassName##Class; \
    using Ref = MTPlugin##ClassName##Ref; \
    using VTable = MTPlugin##ClassName##VTable; \
}; \
}

template<typename Wrapped>
class CoreMediaWrapped {
    WTF_FORBID_HEAP_ALLOCATION;
    WTF_MAKE_NONCOPYABLE(CoreMediaWrapped);
public:
    using WrapperRef = typename CoreMediaTraits<Wrapped>::Ref;

    void ref() { CFRetain(m_leakedWrapper); }
    void deref() { CFRelease(m_leakedWrapper); }
    WrapperRef wrapper() const { return m_leakedWrapper; }
    virtual ~CoreMediaWrapped() { m_leakedWrapper = nullptr; }

protected:
    using WrapperClass = typename CoreMediaTraits<Wrapped>::Class;

    static Wrapped* unwrap(WrapperRef);

    class Allocator {
    public:
        Allocator(CFAllocatorRef allocator)
            : m_allocator(allocator) { }

        void* allocate(size_t);
        WrapperRef leakWrapper() { return m_wrapper.leakRef(); }

    private:
        RetainPtr<CFAllocatorRef> m_allocator;
        RetainPtr<WrapperRef> m_wrapper;
    };

    explicit CoreMediaWrapped(Allocator&& allocator)
        : m_leakedWrapper(allocator.leakWrapper()) { }

    void* operator new(size_t size, Allocator& allocator) { return allocator.allocate(size); }
    CFAllocatorRef allocator() const { return CFGetAllocator(m_leakedWrapper); }
    virtual void finalize() { this->~CoreMediaWrapped<Wrapped>(); }

    // CMBaseClass
    virtual String debugDescription() const = 0;
    virtual OSStatus copyProperty(CFStringRef, CFAllocatorRef, void* copiedValue) = 0;

private:
    using WrapperVTable = typename CoreMediaTraits<Wrapped>::VTable;

    template<size_t> static constexpr CMBaseClass wrapperClass();
    static const WrapperVTable& vTable();

    WrapperRef m_leakedWrapper { nullptr };
};

bool createWrapper(CFAllocatorRef, const CMBaseVTable*, CMBaseClassID, CMBaseObjectRef*);
void* wrapperStorage(CMBaseObjectRef);
const CMBaseVTable* wrapperVTable(CMBaseObjectRef);

template<typename Wrapped>
Wrapped* CoreMediaWrapped<Wrapped>::unwrap(WrapperRef wrapper)
{
    auto baseObject = reinterpret_cast<CMBaseObjectRef>(wrapper);
    if (&vTable().base != wrapperVTable(baseObject))
        return nullptr;
    return static_cast<Wrapped*>(wrapperStorage(baseObject));
}

template<typename Wrapped>
void* CoreMediaWrapped<Wrapped>::Allocator::allocate(size_t size)
{
    ASSERT(!m_wrapper);
    ASSERT_UNUSED(size, size == sizeof(Wrapped));
    CMBaseObjectRef object = nullptr;
    if (!createWrapper(m_allocator.get(), &vTable().base, Wrapped::wrapperClassID(), &object))
        return nullptr;
    m_wrapper = adoptCF(reinterpret_cast<WrapperRef>(object));
    return unwrap(m_wrapper.get());
}

template<typename Wrapped>
template<size_t size>
constexpr CMBaseClass CoreMediaWrapped<Wrapped>::wrapperClass()
{
    return {
        .version = kCMBaseObject_ClassVersion_1,
        .derivedStorageSize = size,
        .finalize = [](CMBaseObjectRef object) {
            Wrapped::unwrap(object)->finalize();
        },
        .copyDebugDescription = [](CMBaseObjectRef object) {
            return Wrapped::unwrap(object)->debugDescription().createCFString().leakRef();
        },
        .copyProperty = [](CMBaseObjectRef object, CFStringRef key, CFAllocatorRef allocator, void* copiedValue) {
            return Wrapped::unwrap(object)->copyProperty(key, allocator, copiedValue);
        },
    };
};

template<typename Wrapped>
const typename CoreMediaWrapped<Wrapped>::WrapperVTable& CoreMediaWrapped<Wrapped>::vTable()
{
    // CMBaseClass contains 64-bit pointers that aren't 8-byte aligned. To suppress the linker
    // warning about this, we prepend 4 bytes of padding when building.
#if CPU(X86_64)
    constexpr size_t padSize = 4;
#else
    constexpr size_t padSize = 0;
#endif

#pragma pack(push, 4)
    static constexpr struct { uint8_t pad[padSize]; CMBaseClass baseClass; } baseClass { { }, wrapperClass<sizeof(Wrapped)>() };
#pragma pack(pop)
    static constexpr WrapperClass derivedClass = Wrapped::wrapperClass();

#if CPU(X86_64)
    static_assert(sizeof(CMBaseClass::version) != sizeof(void*), "Fig struct fixup is required for CMBaseClass on X86_64");
    static_assert(sizeof(WrapperClass::version) == sizeof(void*), "Fig struct fixup is not required for WrapperClass on X86_64");
#else
    static_assert(sizeof(CMBaseClass::version) == sizeof(void*), "Fig struct fixup only required for CMBaseClass on X86_64");
    static_assert(sizeof(WrapperClass::version) == sizeof(void*), "Fig struct fixup only required for WrapperClass on X86_64");
#endif
    static_assert(alignof(CMBaseClass) == 4, "CMBaseClass must have 4 byte alignment");
    static_assert(alignof(WrapperClass) == sizeof(void*), "WrapperClass must be natually aligned");

IGNORE_WARNINGS_BEGIN("missing-field-initializers")
    static constexpr WrapperVTable vTable {
        { nullptr, &baseClass.baseClass },
        &derivedClass
    };
IGNORE_WARNINGS_END
    return vTable;
}

} // namespace WebKit

#endif // ENABLE(WEBM_FORMAT_READER)
