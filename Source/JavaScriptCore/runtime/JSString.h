/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2020 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "ArgList.h"
#include "CallFrame.h"
#include "CommonIdentifiers.h"
#include "GetVM.h"
#include "Identifier.h"
#include "PropertyDescriptor.h"
#include "PropertySlot.h"
#include "Structure.h"
#include "ThrowScope.h"
#include <array>
#include <wtf/CheckedArithmetic.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/text/StringView.h>

namespace JSC {

class JSString;
class JSRopeString;
class LLIntOffsetsExtractor;

JSString* jsEmptyString(VM&);
JSString* jsString(VM&, const String&); // returns empty string if passed null string

JSString* jsSingleCharacterString(VM&, UChar);
JSString* jsSubstring(VM&, const String&, unsigned offset, unsigned length);

// Non-trivial strings are two or more characters long.
// These functions are faster than just calling jsString.
JSString* jsNontrivialString(VM&, const String&);
JSString* jsNontrivialString(VM&, String&&);

// Should be used for strings that are owned by an object that will
// likely outlive the JSValue this makes, such as the parse tree or a
// DOM object that contains a String
JSString* jsOwnedString(VM&, const String&);

bool isJSString(JSCell*);
bool isJSString(JSValue);
JSString* asString(JSValue);

// In 64bit architecture, JSString and JSRopeString have the following memory layout to make sizeof(JSString) == 16 and sizeof(JSRopeString) == 32.
// JSString has only one pointer. We use it for String. length() and is8Bit() queries go to StringImpl. In JSRopeString, we reuse the above pointer
// place for the 1st fiber. JSRopeString has three fibers so its size is 48. To keep length and is8Bit flag information in JSRopeString, JSRopeString
// encodes these information into the fiber pointers. is8Bit flag is encoded in the 1st fiber pointer. length is embedded directly, and two fibers
// are compressed into 12bytes. isRope information is encoded in the first fiber's LSB.
//
// Since length of JSRopeString should be frequently accessed compared to each fiber, we put length in contiguous 32byte field, and compress 2nd
// and 3rd fibers into the following 80byte fields. One problem is that now 2nd and 3rd fibers are split. Storing and loading 2nd and 3rd fibers
// are not one pointer load operation. To make concurrent collector work correctly, we must initialize 2nd and 3rd fibers at JSRopeString creation
// and we must not modify these part later.
//
//              0                        8        10               16                       32                                     48
// JSString     [   ID      ][  header  ][   String pointer      0]
// JSRopeString [   ID      ][  header  ][   1st fiber         xyz][  length  ][2nd lower32][2nd upper16][3rd lower16][3rd upper32]
//                                                               ^
//                                            x:(is8Bit),y:(isSubstring),z:(isRope) bit flags
class JSString : public JSCell {
public:
    friend class JIT;
    friend class VM;
    friend class SpecializedThunkJIT;
    friend class JSRopeString;
    friend class MarkStack;
    friend class SlotVisitor;
    friend class SmallStrings;

    typedef JSCell Base;
    // Do we really need OverridesGetOwnPropertySlot?
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=212956
    // Do we really need InterceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero?
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=212958
    static constexpr unsigned StructureFlags = Base::StructureFlags | OverridesGetOwnPropertySlot | InterceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero | StructureIsImmortal | OverridesToThis;

    static constexpr bool needsDestruction = true;
    static ALWAYS_INLINE void destroy(JSCell* cell)
    {
        static_cast<JSString*>(cell)->JSString::~JSString();
    }
    
    // We specialize the string subspace to get the fastest possible sweep. This wouldn't be
    // necessary if JSString didn't have a destructor.
    template<typename, SubspaceAccess>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.stringSpace;
    }
    
    // We employ overflow checks in many places with the assumption that MaxLength
    // is INT_MAX. Hence, it cannot be changed into another length value without
    // breaking all the bounds and overflow checks that assume this.
    static constexpr unsigned MaxLength = std::numeric_limits<int32_t>::max();
    static_assert(MaxLength == String::MaxLength, "");

    static constexpr uintptr_t isRopeInPointer = 0x1;

private:
    String& uninitializedValueInternal() const
    {
        return *bitwise_cast<String*>(&m_fiber);
    }

    String& valueInternal() const
    {
        ASSERT(!isRope());
        return uninitializedValueInternal();
    }

    JSString(VM& vm, Ref<StringImpl>&& value)
        : JSCell(vm, vm.stringStructure.get())
    {
        new (&uninitializedValueInternal()) String(WTFMove(value));
    }

    JSString(VM& vm)
        : JSCell(vm, vm.stringStructure.get())
        , m_fiber(isRopeInPointer)
    {
    }

    void finishCreation(VM& vm, unsigned length)
    {
        ASSERT_UNUSED(length, length > 0);
        ASSERT(!valueInternal().isNull());
        Base::finishCreation(vm);
    }

    void finishCreation(VM& vm, unsigned length, size_t cost)
    {
        ASSERT_UNUSED(length, length > 0);
        ASSERT(!valueInternal().isNull());
        Base::finishCreation(vm);
        vm.heap.reportExtraMemoryAllocated(cost);
    }

    static JSString* createEmptyString(VM&);

    static JSString* create(VM& vm, Ref<StringImpl>&& value)
    {
        unsigned length = value->length();
        ASSERT(length > 0);
        size_t cost = value->cost();
        JSString* newString = new (NotNull, allocateCell<JSString>(vm.heap)) JSString(vm, WTFMove(value));
        newString->finishCreation(vm, length, cost);
        return newString;
    }
    static JSString* createHasOtherOwner(VM& vm, Ref<StringImpl>&& value)
    {
        unsigned length = value->length();
        JSString* newString = new (NotNull, allocateCell<JSString>(vm.heap)) JSString(vm, WTFMove(value));
        newString->finishCreation(vm, length);
        return newString;
    }

protected:
    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
    }

public:
    ~JSString();

    Identifier toIdentifier(JSGlobalObject*) const;
    AtomString toAtomString(JSGlobalObject*) const;
    RefPtr<AtomStringImpl> toExistingAtomString(JSGlobalObject*) const;

    StringViewWithUnderlyingString viewWithUnderlyingString(JSGlobalObject*) const;

    inline bool equal(JSGlobalObject*, JSString* other) const;
    const String& value(JSGlobalObject*) const;
    inline const String& tryGetValue(bool allocationAllowed = true) const;
    const StringImpl* getValueImpl() const;
    const StringImpl* tryGetValueImpl() const;
    ALWAYS_INLINE unsigned length() const;

    JSValue toPrimitive(JSGlobalObject*, PreferredPrimitiveType) const;
    bool toBoolean() const { return !!length(); }
    bool getPrimitiveNumber(JSGlobalObject*, double& number, JSValue&) const;
    JSObject* toObject(JSGlobalObject*) const;
    double toNumber(JSGlobalObject*) const;

    bool getStringPropertySlot(JSGlobalObject*, PropertyName, PropertySlot&);
    bool getStringPropertySlot(JSGlobalObject*, unsigned propertyName, PropertySlot&);
    bool getStringPropertyDescriptor(JSGlobalObject*, PropertyName, PropertyDescriptor&);

    bool canGetIndex(unsigned i) { return i < length(); }
    JSString* getIndex(JSGlobalObject*, unsigned);

    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    static ptrdiff_t offsetOfValue() { return OBJECT_OFFSETOF(JSString, m_fiber); }

    DECLARE_EXPORT_INFO;

    static void dumpToStream(const JSCell*, PrintStream&);
    static size_t estimatedSize(JSCell*, VM&);
    static void visitChildren(JSCell*, SlotVisitor&);

    ALWAYS_INLINE bool isRope() const
    {
        return m_fiber & isRopeInPointer;
    }

    bool is8Bit() const;

protected:
    friend class JSValue;

    JS_EXPORT_PRIVATE bool equalSlowCase(JSGlobalObject*, JSString* other) const;
    bool isSubstring() const;

    mutable uintptr_t m_fiber;

private:
    friend class LLIntOffsetsExtractor;

    static JSValue toThis(JSCell*, JSGlobalObject*, ECMAMode);

    StringView unsafeView(JSGlobalObject*) const;

    friend JSString* jsString(VM&, const String&);
    friend JSString* jsString(JSGlobalObject*, JSString*, JSString*);
    friend JSString* jsString(JSGlobalObject*, const String&, JSString*);
    friend JSString* jsString(JSGlobalObject*, JSString*, const String&);
    friend JSString* jsString(JSGlobalObject*, const String&, const String&);
    friend JSString* jsString(JSGlobalObject*, JSString*, JSString*, JSString*);
    friend JSString* jsString(JSGlobalObject*, const String&, const String&, const String&);
    friend JSString* jsSingleCharacterString(VM&, UChar);
    friend JSString* jsNontrivialString(VM&, const String&);
    friend JSString* jsNontrivialString(VM&, String&&);
    friend JSString* jsSubstring(VM&, const String&, unsigned, unsigned);
    friend JSString* jsSubstring(VM&, JSGlobalObject*, JSString*, unsigned, unsigned);
    friend JSString* jsSubstringOfResolved(VM&, GCDeferralContext*, JSString*, unsigned, unsigned);
    friend JSString* jsOwnedString(VM&, const String&);
};

// NOTE: This class cannot override JSString's destructor. JSString's destructor is called directly
// from JSStringSubspace::
class JSRopeString final : public JSString {
    friend class JSString;
public:
    template<typename, SubspaceAccess>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.ropeStringSpace;
    }

    // We use lower 3bits of fiber0 for flags. These bits are usable due to alignment, and it is OK even in 32bit architecture.
    static constexpr uintptr_t is8BitInPointer = static_cast<uintptr_t>(StringImpl::flagIs8Bit());
    static constexpr uintptr_t isSubstringInPointer = 0x2;
    static_assert(is8BitInPointer == 0b100, "");
    static_assert(isSubstringInPointer == 0b010, "");
    static_assert(isRopeInPointer == 0b001, "");
    static constexpr uintptr_t stringMask = ~(isRopeInPointer | is8BitInPointer | isSubstringInPointer);
#if CPU(ADDRESS64)
    static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "");
    class CompactFibers {
    public:
        static constexpr uintptr_t addressMask = (1ULL << OS_CONSTANT(EFFECTIVE_ADDRESS_WIDTH)) - 1;
        JSString* fiber1() const
        {
#if CPU(LITTLE_ENDIAN)
            return bitwise_cast<JSString*>(WTF::unalignedLoad<uintptr_t>(&m_fiber1Lower) & addressMask);
#else
            return bitwise_cast<JSString*>(static_cast<uintptr_t>(m_fiber1Lower) | (static_cast<uintptr_t>(m_fiber1Upper) << 32));
#endif
        }

        void initializeFiber1(JSString* fiber)
        {
            uintptr_t pointer = bitwise_cast<uintptr_t>(fiber);
            m_fiber1Lower = static_cast<uint32_t>(pointer);
            m_fiber1Upper = static_cast<uint16_t>(pointer >> 32);
        }

        JSString* fiber2() const
        {
#if CPU(LITTLE_ENDIAN)
            return bitwise_cast<JSString*>(WTF::unalignedLoad<uintptr_t>(&m_fiber1Upper) >> 16);
#else
            return bitwise_cast<JSString*>(static_cast<uintptr_t>(m_fiber2Lower) | (static_cast<uintptr_t>(m_fiber2Upper) << 16));
#endif
        }
        void initializeFiber2(JSString* fiber)
        {
            uintptr_t pointer = bitwise_cast<uintptr_t>(fiber);
            m_fiber2Lower = static_cast<uint16_t>(pointer);
            m_fiber2Upper = static_cast<uint32_t>(pointer >> 16);
        }

        unsigned length() const { return m_length; }
        void initializeLength(unsigned length)
        {
            m_length = length;
        }

        static ptrdiff_t offsetOfLength() { return OBJECT_OFFSETOF(CompactFibers, m_length); }
        static ptrdiff_t offsetOfFiber1() { return OBJECT_OFFSETOF(CompactFibers, m_length); }
        static ptrdiff_t offsetOfFiber2() { return OBJECT_OFFSETOF(CompactFibers, m_fiber1Upper); }

    private:
        friend class LLIntOffsetsExtractor;

        uint32_t m_length { 0 };
        uint32_t m_fiber1Lower { 0 };
        uint16_t m_fiber1Upper { 0 };
        uint16_t m_fiber2Lower { 0 };
        uint32_t m_fiber2Upper { 0 };
    };
    static_assert(sizeof(CompactFibers) == sizeof(void*) * 2, "");
#else
    class CompactFibers {
    public:
        JSString* fiber1() const
        {
            return m_fiber1;
        }
        void initializeFiber1(JSString* fiber)
        {
            m_fiber1 = fiber;
        }

        JSString* fiber2() const
        {
            return m_fiber2;
        }
        void initializeFiber2(JSString* fiber)
        {
            m_fiber2 = fiber;
        }

        unsigned length() const { return m_length; }
        void initializeLength(unsigned length)
        {
            m_length = length;
        }

        static ptrdiff_t offsetOfLength() { return OBJECT_OFFSETOF(CompactFibers, m_length); }
        static ptrdiff_t offsetOfFiber1() { return OBJECT_OFFSETOF(CompactFibers, m_fiber1); }
        static ptrdiff_t offsetOfFiber2() { return OBJECT_OFFSETOF(CompactFibers, m_fiber2); }

    private:
        friend class LLIntOffsetsExtractor;

        uint32_t m_length { 0 };
        JSString* m_fiber1 { nullptr };
        JSString* m_fiber2 { nullptr };
    };
#endif

    template <class OverflowHandler = CrashOnOverflow>
    class RopeBuilder : public OverflowHandler {
        WTF_FORBID_HEAP_ALLOCATION;
    public:
        RopeBuilder(VM& vm)
            : m_vm(vm)
        {
        }

        bool append(JSString* jsString)
        {
            if (UNLIKELY(this->hasOverflowed()))
                return false;
            if (!jsString->length())
                return true;
            if (m_strings.size() == JSRopeString::s_maxInternalRopeLength)
                expand();

            static_assert(JSString::MaxLength == std::numeric_limits<int32_t>::max(), "");
            auto sum = checkedSum<int32_t>(m_length, jsString->length());
            if (sum.hasOverflowed()) {
                this->overflowed();
                return false;
            }
            ASSERT(static_cast<unsigned>(sum.unsafeGet()) <= MaxLength);
            m_strings.append(jsString);
            m_length = static_cast<unsigned>(sum.unsafeGet());
            return true;
        }

        JSString* release()
        {
            RELEASE_ASSERT(!this->hasOverflowed());
            JSString* result = nullptr;
            switch (m_strings.size()) {
            case 0: {
                ASSERT(!m_length);
                result = jsEmptyString(m_vm);
                break;
            }
            case 1: {
                result = asString(m_strings.at(0));
                break;
            }
            case 2: {
                result = JSRopeString::create(m_vm, asString(m_strings.at(0)), asString(m_strings.at(1)));
                break;
            }
            case 3: {
                result = JSRopeString::create(m_vm, asString(m_strings.at(0)), asString(m_strings.at(1)), asString(m_strings.at(2)));
                break;
            }
            default:
                ASSERT_NOT_REACHED();
                break;
            }
            ASSERT(result->length() == m_length);
            m_strings.clear();
            m_length = 0;
            return result;
        }

        unsigned length() const
        {
            ASSERT(!this->hasOverflowed());
            return m_length;
        }

    private:
        void expand();

        VM& m_vm;
        MarkedArgumentBuffer m_strings;
        unsigned m_length { 0 };
    };

    inline unsigned length() const
    {
        return m_compactFibers.length();
    }

private:
    friend class LLIntOffsetsExtractor;

    void convertToNonRope(String&&) const;

    void initializeIs8Bit(bool flag) const
    {
        if (flag)
            m_fiber |= is8BitInPointer;
        else
            m_fiber &= ~is8BitInPointer;
    }

    void initializeIsSubstring(bool flag) const
    {
        if (flag)
            m_fiber |= isSubstringInPointer;
        else
            m_fiber &= ~isSubstringInPointer;
    }

    ALWAYS_INLINE void initializeLength(unsigned length)
    {
        ASSERT(length <= MaxLength);
        m_compactFibers.initializeLength(length);
    }

    JSRopeString(VM& vm)
        : JSString(vm)
    {
        initializeIsSubstring(false);
        initializeLength(0);
        initializeIs8Bit(true);
        initializeFiber0(nullptr);
        initializeFiber1(nullptr);
        initializeFiber2(nullptr);
    }

    JSRopeString(VM& vm, JSString* s1, JSString* s2)
        : JSString(vm)
    {
        ASSERT(!sumOverflows<int32_t>(s1->length(), s2->length()));
        initializeIsSubstring(false);
        initializeLength(s1->length() + s2->length());
        initializeIs8Bit(s1->is8Bit() && s2->is8Bit());
        initializeFiber0(s1);
        initializeFiber1(s2);
        initializeFiber2(nullptr);
        ASSERT((s1->length() + s2->length()) == length());
    }

    JSRopeString(VM& vm, JSString* s1, JSString* s2, JSString* s3)
        : JSString(vm)
    {
        ASSERT(!sumOverflows<int32_t>(s1->length(), s2->length(), s3->length()));
        initializeIsSubstring(false);
        initializeLength(s1->length() + s2->length() + s3->length());
        initializeIs8Bit(s1->is8Bit() && s2->is8Bit() &&  s3->is8Bit());
        initializeFiber0(s1);
        initializeFiber1(s2);
        initializeFiber2(s3);
        ASSERT((s1->length() + s2->length() + s3->length()) == length());
    }

    JSRopeString(VM& vm, JSString* base, unsigned offset, unsigned length)
        : JSString(vm)
    {
        RELEASE_ASSERT(!sumOverflows<int32_t>(offset, length));
        RELEASE_ASSERT(offset + length <= base->length());
        initializeIsSubstring(true);
        initializeLength(length);
        initializeIs8Bit(base->is8Bit());
        initializeSubstringBase(base);
        initializeSubstringOffset(offset);
        ASSERT(length == this->length());
        ASSERT(!base->isRope());
    }

    ALWAYS_INLINE void finishCreationSubstringOfResolved(VM& vm)
    {
        Base::finishCreation(vm);
    }

public:
    static ptrdiff_t offsetOfLength() { return OBJECT_OFFSETOF(JSRopeString, m_compactFibers) + CompactFibers::offsetOfLength(); } // 32byte width.
    static ptrdiff_t offsetOfFlags() { return offsetOfValue(); }
    static ptrdiff_t offsetOfFiber0() { return offsetOfValue(); }
    static ptrdiff_t offsetOfFiber1() { return OBJECT_OFFSETOF(JSRopeString, m_compactFibers) + CompactFibers::offsetOfFiber1(); }
    static ptrdiff_t offsetOfFiber2() { return OBJECT_OFFSETOF(JSRopeString, m_compactFibers) + CompactFibers::offsetOfFiber2(); }

    static constexpr unsigned s_maxInternalRopeLength = 3;

    // This JSRopeString is only used to simulate half-baked JSRopeString in DFG and FTL MakeRope. If OSR exit happens in
    // the middle of MakeRope due to string length overflow, we have half-baked JSRopeString which is the same to the result
    // of this function. This half-baked JSRopeString will not be exposed to users, but still collectors can see it due to
    // the conservative stack scan. This JSRopeString is used to test the collector with such a half-baked JSRopeString.
    // Because this JSRopeString breaks the JSString's invariant (only one singleton JSString can be zero length), almost all the
    // operations in JS fail to handle this string correctly.
    static JSRopeString* createNullForTesting(VM& vm)
    {
        JSRopeString* newString = new (NotNull, allocateCell<JSRopeString>(vm.heap)) JSRopeString(vm);
        newString->finishCreation(vm);
        ASSERT(!newString->length());
        ASSERT(newString->isRope());
        ASSERT(newString->fiber0() == nullptr);
        return newString;
    }

private:
    static JSRopeString* create(VM& vm, JSString* s1, JSString* s2)
    {
        JSRopeString* newString = new (NotNull, allocateCell<JSRopeString>(vm.heap)) JSRopeString(vm, s1, s2);
        newString->finishCreation(vm);
        ASSERT(newString->length());
        ASSERT(newString->isRope());
        return newString;
    }
    static JSRopeString* create(VM& vm, JSString* s1, JSString* s2, JSString* s3)
    {
        JSRopeString* newString = new (NotNull, allocateCell<JSRopeString>(vm.heap)) JSRopeString(vm, s1, s2, s3);
        newString->finishCreation(vm);
        ASSERT(newString->length());
        ASSERT(newString->isRope());
        return newString;
    }

    ALWAYS_INLINE static JSRopeString* createSubstringOfResolved(VM& vm, GCDeferralContext* deferralContext, JSString* base, unsigned offset, unsigned length)
    {
        JSRopeString* newString = new (NotNull, allocateCell<JSRopeString>(vm.heap, deferralContext)) JSRopeString(vm, base, offset, length);
        newString->finishCreationSubstringOfResolved(vm);
        ASSERT(newString->length());
        ASSERT(newString->isRope());
        return newString;
    }

    friend JSValue jsStringFromRegisterArray(JSGlobalObject*, Register*, unsigned);

    // If nullOrExecForOOM is null, resolveRope() will be do nothing in the event of an OOM error.
    // The rope value will remain a null string in that case.
    JS_EXPORT_PRIVATE const String& resolveRope(JSGlobalObject* nullOrGlobalObjectForOOM) const;
    template<typename Function> const String& resolveRopeWithFunction(JSGlobalObject* nullOrGlobalObjectForOOM, Function&&) const;
    JS_EXPORT_PRIVATE AtomString resolveRopeToAtomString(JSGlobalObject*) const;
    JS_EXPORT_PRIVATE RefPtr<AtomStringImpl> resolveRopeToExistingAtomString(JSGlobalObject*) const;
    template<typename CharacterType> NEVER_INLINE void resolveRopeSlowCase(CharacterType*) const;
    template<typename CharacterType> void resolveRopeInternalNoSubstring(CharacterType*) const;
    void outOfMemory(JSGlobalObject* nullOrGlobalObjectForOOM) const;
    void resolveRopeInternal8(LChar*) const;
    void resolveRopeInternal16(UChar*) const;
    StringView unsafeView(JSGlobalObject*) const;
    StringViewWithUnderlyingString viewWithUnderlyingString(JSGlobalObject*) const;

    JSString* fiber0() const
    {
        return bitwise_cast<JSString*>(m_fiber & stringMask);
    }

    JSString* fiber1() const
    {
        return m_compactFibers.fiber1();
    }

    JSString* fiber2() const
    {
        return m_compactFibers.fiber2();
    }

    JSString* fiber(unsigned i) const
    {
        ASSERT(!isSubstring());
        ASSERT(i < s_maxInternalRopeLength);
        switch (i) {
        case 0:
            return fiber0();
        case 1:
            return fiber1();
        case 2:
            return fiber2();
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    void initializeFiber0(JSString* fiber)
    {
        uintptr_t pointer = bitwise_cast<uintptr_t>(fiber);
        ASSERT(!(pointer & ~stringMask));
        m_fiber = (pointer | (m_fiber & ~stringMask));
    }

    void initializeFiber1(JSString* fiber)
    {
        m_compactFibers.initializeFiber1(fiber);
    }

    void initializeFiber2(JSString* fiber)
    {
        m_compactFibers.initializeFiber2(fiber);
    }

    void initializeSubstringBase(JSString* fiber)
    {
        initializeFiber1(fiber);
    }

    JSString* substringBase() const { return fiber1(); }

    void initializeSubstringOffset(unsigned offset)
    {
        m_compactFibers.initializeFiber2(bitwise_cast<JSString*>(static_cast<uintptr_t>(offset)));
    }

    unsigned substringOffset() const
    {
        return static_cast<unsigned>(bitwise_cast<uintptr_t>(fiber2()));
    }

    static_assert(s_maxInternalRopeLength >= 2, "");
    mutable CompactFibers m_compactFibers;

    friend JSString* jsString(JSGlobalObject*, JSString*, JSString*);
    friend JSString* jsString(JSGlobalObject*, const String&, JSString*);
    friend JSString* jsString(JSGlobalObject*, JSString*, const String&);
    friend JSString* jsString(JSGlobalObject*, const String&, const String&);
    friend JSString* jsString(JSGlobalObject*, JSString*, JSString*, JSString*);
    friend JSString* jsString(JSGlobalObject*, const String&, const String&, const String&);
    friend JSString* jsSubstringOfResolved(VM&, GCDeferralContext*, JSString*, unsigned, unsigned);
    friend JSString* jsSubstring(VM&, JSGlobalObject*, JSString*, unsigned, unsigned);
};

JS_EXPORT_PRIVATE JSString* jsStringWithCacheSlowCase(VM&, StringImpl&);

// JSString::is8Bit is safe to be called concurrently. Concurrent threads can access is8Bit even if the main thread
// is in the middle of converting JSRopeString to JSString.
ALWAYS_INLINE bool JSString::is8Bit() const
{
    uintptr_t pointer = m_fiber;
    if (pointer & isRopeInPointer) {
        // Do not load m_fiber twice. We should use the information in pointer.
        // Otherwise, JSRopeString may be converted to JSString between the first and second accesses.
        return pointer & JSRopeString::is8BitInPointer;
    }
    return bitwise_cast<StringImpl*>(pointer)->is8Bit();
}

// JSString::length is safe to be called concurrently. Concurrent threads can access length even if the main thread
// is in the middle of converting JSRopeString to JSString. This is OK because we never override the length bits
// when we resolve a JSRopeString.
ALWAYS_INLINE unsigned JSString::length() const
{
    uintptr_t pointer = m_fiber;
    if (pointer & isRopeInPointer)
        return jsCast<const JSRopeString*>(this)->length();
    return bitwise_cast<StringImpl*>(pointer)->length();
}

inline const StringImpl* JSString::getValueImpl() const
{
    ASSERT(!isRope());
    return bitwise_cast<StringImpl*>(m_fiber);
}

inline const StringImpl* JSString::tryGetValueImpl() const
{
    uintptr_t pointer = m_fiber;
    if (pointer & isRopeInPointer)
        return nullptr;
    return bitwise_cast<StringImpl*>(pointer);
}

inline JSString* asString(JSValue value)
{
    ASSERT(value.asCell()->isString());
    return jsCast<JSString*>(value.asCell());
}

// This MUST NOT GC.
inline JSString* jsEmptyString(VM& vm)
{
    return vm.smallStrings.emptyString();
}

ALWAYS_INLINE JSString* jsSingleCharacterString(VM& vm, UChar c)
{
    if constexpr (validateDFGDoesGC)
        vm.heap.verifyCanGC();
    if (c <= maxSingleCharacterString)
        return vm.smallStrings.singleCharacterString(c);
    return JSString::create(vm, StringImpl::create(&c, 1));
}

inline JSString* jsNontrivialString(VM& vm, const String& s)
{
    ASSERT(s.length() > 1);
    return JSString::create(vm, *s.impl());
}

inline JSString* jsNontrivialString(VM& vm, String&& s)
{
    ASSERT(s.length() > 1);
    return JSString::create(vm, s.releaseImpl().releaseNonNull());
}

ALWAYS_INLINE Identifier JSString::toIdentifier(JSGlobalObject* globalObject) const
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    AtomString atomString = toAtomString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    return Identifier::fromString(vm, atomString);
}

ALWAYS_INLINE AtomString JSString::toAtomString(JSGlobalObject* globalObject) const
{
    if constexpr (validateDFGDoesGC)
        vm().heap.verifyCanGC();
    if (isRope())
        return static_cast<const JSRopeString*>(this)->resolveRopeToAtomString(globalObject);
    return AtomString(valueInternal());
}

ALWAYS_INLINE RefPtr<AtomStringImpl> JSString::toExistingAtomString(JSGlobalObject* globalObject) const
{
    if constexpr (validateDFGDoesGC)
        vm().heap.verifyCanGC();
    if (isRope())
        return static_cast<const JSRopeString*>(this)->resolveRopeToExistingAtomString(globalObject);
    if (valueInternal().impl()->isAtom())
        return static_cast<AtomStringImpl*>(valueInternal().impl());
    return AtomStringImpl::lookUp(valueInternal().impl());
}

inline const String& JSString::value(JSGlobalObject* globalObject) const
{
    if constexpr (validateDFGDoesGC)
        vm().heap.verifyCanGC();
    if (isRope())
        return static_cast<const JSRopeString*>(this)->resolveRope(globalObject);
    return valueInternal();
}

inline const String& JSString::tryGetValue(bool allocationAllowed) const
{
    if (allocationAllowed) {
        if constexpr (validateDFGDoesGC)
            vm().heap.verifyCanGC();
        if (isRope()) {
            // Pass nullptr for the JSGlobalObject so that resolveRope does not throw in the event of an OOM error.
            return static_cast<const JSRopeString*>(this)->resolveRope(nullptr);
        }
    } else
        RELEASE_ASSERT(!isRope());
    return valueInternal();
}

inline JSString* JSString::getIndex(JSGlobalObject* globalObject, unsigned i)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    ASSERT(canGetIndex(i));
    StringView view = unsafeView(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return jsSingleCharacterString(vm, view[i]);
}

inline JSString* jsString(VM& vm, const String& s)
{
    int size = s.length();
    if (!size)
        return vm.smallStrings.emptyString();
    if (size == 1) {
        UChar c = s.characterAt(0);
        if (c <= maxSingleCharacterString)
            return vm.smallStrings.singleCharacterString(c);
    }
    return JSString::create(vm, *s.impl());
}

inline JSString* jsSubstring(VM& vm, JSGlobalObject* globalObject, JSString* base, unsigned offset, unsigned length)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(offset <= base->length());
    ASSERT(length <= base->length());
    ASSERT(offset + length <= base->length());
    if (!length)
        return vm.smallStrings.emptyString();
    if (!offset && length == base->length())
        return base;

    // For now, let's not allow substrings with a rope base.
    // Resolve non-substring rope bases so we don't have to deal with it.
    // FIXME: Evaluate if this would be worth adding more branches.
    if (base->isSubstring()) {
        JSRopeString* baseRope = jsCast<JSRopeString*>(base);
        base = baseRope->substringBase();
        offset = baseRope->substringOffset() + offset;
        ASSERT(!base->isRope());
    } else if (base->isRope()) {
        jsCast<JSRopeString*>(base)->resolveRope(globalObject);
        RETURN_IF_EXCEPTION(scope, nullptr);
    }
    return jsSubstringOfResolved(vm, nullptr, base, offset, length);
}

inline JSString* jsSubstringOfResolved(VM& vm, GCDeferralContext* deferralContext, JSString* s, unsigned offset, unsigned length)
{
    ASSERT(offset <= s->length());
    ASSERT(length <= s->length());
    ASSERT(offset + length <= s->length());
    ASSERT(!s->isRope());
    if (!length)
        return vm.smallStrings.emptyString();
    if (!offset && length == s->length())
        return s;
    if (length == 1) {
        auto& base = s->valueInternal();
        UChar character = base.characterAt(offset);
        if (character <= maxSingleCharacterString)
            return vm.smallStrings.singleCharacterString(character);
    }
    return JSRopeString::createSubstringOfResolved(vm, deferralContext, s, offset, length);
}

inline JSString* jsSubstringOfResolved(VM& vm, JSString* s, unsigned offset, unsigned length)
{
    return jsSubstringOfResolved(vm, nullptr, s, offset, length);
}

inline JSString* jsSubstring(JSGlobalObject* globalObject, JSString* s, unsigned offset, unsigned length)
{
    return jsSubstring(getVM(globalObject), globalObject, s, offset, length);
}

inline JSString* jsSubstring(VM& vm, const String& s, unsigned offset, unsigned length)
{
    ASSERT(offset <= s.length());
    ASSERT(length <= s.length());
    ASSERT(offset + length <= s.length());
    if (!length)
        return vm.smallStrings.emptyString();
    if (length == 1) {
        UChar c = s.characterAt(offset);
        if (c <= maxSingleCharacterString)
            return vm.smallStrings.singleCharacterString(c);
    }
    auto impl = StringImpl::createSubstringSharingImpl(*s.impl(), offset, length);
    if (impl->isSubString())
        return JSString::createHasOtherOwner(vm, WTFMove(impl));
    return JSString::create(vm, WTFMove(impl));
}

inline JSString* jsOwnedString(VM& vm, const String& s)
{
    int size = s.length();
    if (!size)
        return vm.smallStrings.emptyString();
    if (size == 1) {
        UChar c = s.characterAt(0);
        if (c <= maxSingleCharacterString)
            return vm.smallStrings.singleCharacterString(c);
    }
    return JSString::createHasOtherOwner(vm, *s.impl());
}

ALWAYS_INLINE JSString* jsStringWithCache(VM& vm, const String& s)
{
    StringImpl* stringImpl = s.impl();
    if (!stringImpl || !stringImpl->length())
        return jsEmptyString(vm);

    if (stringImpl->length() == 1) {
        UChar singleCharacter = (*stringImpl)[0u];
        if (singleCharacter <= maxSingleCharacterString)
            return vm.smallStrings.singleCharacterString(static_cast<unsigned char>(singleCharacter));
    }

    if (JSString* lastCachedString = vm.lastCachedString.get()) {
        if (lastCachedString->tryGetValueImpl() == stringImpl)
            return lastCachedString;
    }

    return jsStringWithCacheSlowCase(vm, *stringImpl);
}

ALWAYS_INLINE bool JSString::getStringPropertySlot(JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (propertyName == vm.propertyNames->length) {
        slot.setValue(this, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly, jsNumber(length()));
        return true;
    }

    Optional<uint32_t> index = parseIndex(propertyName);
    if (index && index.value() < length()) {
        JSValue value = getIndex(globalObject, index.value());
        RETURN_IF_EXCEPTION(scope, false);
        slot.setValue(this, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly, value);
        return true;
    }

    return false;
}

ALWAYS_INLINE bool JSString::getStringPropertySlot(JSGlobalObject* globalObject, unsigned propertyName, PropertySlot& slot)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (propertyName < length()) {
        JSValue value = getIndex(globalObject, propertyName);
        RETURN_IF_EXCEPTION(scope, false);
        slot.setValue(this, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly, value);
        return true;
    }

    return false;
}

inline bool isJSString(JSCell* cell)
{
    return cell->type() == StringType;
}

inline bool isJSString(JSValue v)
{
    return v.isCell() && isJSString(v.asCell());
}

ALWAYS_INLINE StringView JSRopeString::unsafeView(JSGlobalObject* globalObject) const
{
    if constexpr (validateDFGDoesGC)
        vm().heap.verifyCanGC();
    if (isSubstring()) {
        auto& base = substringBase()->valueInternal();
        if (base.is8Bit())
            return StringView(base.characters8() + substringOffset(), length());
        return StringView(base.characters16() + substringOffset(), length());
    }
    return resolveRope(globalObject);
}

ALWAYS_INLINE StringViewWithUnderlyingString JSRopeString::viewWithUnderlyingString(JSGlobalObject* globalObject) const
{
    if constexpr (validateDFGDoesGC)
        vm().heap.verifyCanGC();
    if (isSubstring()) {
        auto& base = substringBase()->valueInternal();
        if (base.is8Bit())
            return { { base.characters8() + substringOffset(), length() }, base };
        return { { base.characters16() + substringOffset(), length() }, base };
    }
    auto& string = resolveRope(globalObject);
    return { string, string };
}

ALWAYS_INLINE StringView JSString::unsafeView(JSGlobalObject* globalObject) const
{
    if constexpr (validateDFGDoesGC)
        vm().heap.verifyCanGC();
    if (isRope())
        return static_cast<const JSRopeString*>(this)->unsafeView(globalObject);
    return valueInternal();
}

ALWAYS_INLINE StringViewWithUnderlyingString JSString::viewWithUnderlyingString(JSGlobalObject* globalObject) const
{
    if (isRope())
        return static_cast<const JSRopeString&>(*this).viewWithUnderlyingString(globalObject);
    return { valueInternal(), valueInternal() };
}

inline bool JSString::isSubstring() const
{
    return m_fiber & JSRopeString::isSubstringInPointer;
}

// --- JSValue inlines ----------------------------

inline bool JSValue::toBoolean(JSGlobalObject* globalObject) const
{
    if (isInt32())
        return asInt32();
    if (isDouble())
        return asDouble() > 0.0 || asDouble() < 0.0; // false for NaN
    if (isCell())
        return asCell()->toBoolean(globalObject);
#if USE(BIGINT32)
    if (isBigInt32())
        return !!bigInt32AsInt32();
#endif
    return isTrue(); // false, null, and undefined all convert to false.
}

inline JSString* JSValue::toString(JSGlobalObject* globalObject) const
{
    if (isString())
        return asString(asCell());
    bool returnEmptyStringOnError = true;
    return toStringSlowCase(globalObject, returnEmptyStringOnError);
}

inline JSString* JSValue::toStringOrNull(JSGlobalObject* globalObject) const
{
    if (isString())
        return asString(asCell());
    bool returnEmptyStringOnError = false;
    return toStringSlowCase(globalObject, returnEmptyStringOnError);
}

inline String JSValue::toWTFString(JSGlobalObject* globalObject) const
{
    if (isString())
        return static_cast<JSString*>(asCell())->value(globalObject);
    return toWTFStringSlowCase(globalObject);
}

} // namespace JSC
