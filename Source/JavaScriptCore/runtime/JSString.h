/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2019 Apple Inc. All rights reserved.
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

JSString* jsEmptyString(VM*);
JSString* jsEmptyString(ExecState*);
JSString* jsString(VM*, const String&); // returns empty string if passed null string
JSString* jsString(ExecState*, const String&); // returns empty string if passed null string

JSString* jsSingleCharacterString(VM*, UChar);
JSString* jsSingleCharacterString(ExecState*, UChar);
JSString* jsSubstring(VM*, const String&, unsigned offset, unsigned length);
JSString* jsSubstring(ExecState*, const String&, unsigned offset, unsigned length);

// Non-trivial strings are two or more characters long.
// These functions are faster than just calling jsString.
JSString* jsNontrivialString(VM*, const String&);
JSString* jsNontrivialString(ExecState*, const String&);
JSString* jsNontrivialString(ExecState*, String&&);

// Should be used for strings that are owned by an object that will
// likely outlive the JSValue this makes, such as the parse tree or a
// DOM object that contains a String
JSString* jsOwnedString(VM*, const String&);
JSString* jsOwnedString(ExecState*, const String&);

bool isJSString(JSCell*);
bool isJSString(JSValue);
JSString* asString(JSValue);

struct StringViewWithUnderlyingString {
    StringView view;
    String underlyingString;
};


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
    static const unsigned StructureFlags = Base::StructureFlags | OverridesGetOwnPropertySlot | InterceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero | StructureIsImmortal | OverridesToThis;

    static const bool needsDestruction = true;
    static void destroy(JSCell*);
    
    // We specialize the string subspace to get the fastest possible sweep. This wouldn't be
    // necessary if JSString didn't have a destructor.
    template<typename, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM& vm)
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

    Identifier toIdentifier(ExecState*) const;
    AtomicString toAtomicString(ExecState*) const;
    RefPtr<AtomicStringImpl> toExistingAtomicString(ExecState*) const;

    StringViewWithUnderlyingString viewWithUnderlyingString(ExecState*) const;

    inline bool equal(ExecState*, JSString* other) const;
    const String& value(ExecState*) const;
    inline const String& tryGetValue(bool allocationAllowed = true) const;
    const StringImpl* tryGetValueImpl() const;
    ALWAYS_INLINE unsigned length() const;

    JSValue toPrimitive(ExecState*, PreferredPrimitiveType) const;
    bool toBoolean() const { return !!length(); }
    bool getPrimitiveNumber(ExecState*, double& number, JSValue&) const;
    JSObject* toObject(ExecState*, JSGlobalObject*) const;
    double toNumber(ExecState*) const;

    bool getStringPropertySlot(ExecState*, PropertyName, PropertySlot&);
    bool getStringPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);
    bool getStringPropertyDescriptor(ExecState*, PropertyName, PropertyDescriptor&);

    bool canGetIndex(unsigned i) { return i < length(); }
    JSString* getIndex(ExecState*, unsigned);

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

    JS_EXPORT_PRIVATE bool equalSlowCase(ExecState*, JSString* other) const;
    bool isSubstring() const;

    mutable uintptr_t m_fiber;

private:
    friend class LLIntOffsetsExtractor;

    static JSValue toThis(JSCell*, ExecState*, ECMAMode);

    StringView unsafeView(ExecState*) const;

    friend JSString* jsString(VM*, const String&);
    friend JSString* jsString(ExecState*, JSString*, JSString*);
    friend JSString* jsString(ExecState*, const String&, JSString*);
    friend JSString* jsString(ExecState*, JSString*, const String&);
    friend JSString* jsString(ExecState*, const String&, const String&);
    friend JSString* jsString(ExecState*, JSString*, JSString*, JSString*);
    friend JSString* jsString(ExecState*, const String&, const String&, const String&);
    friend JSString* jsSingleCharacterString(VM*, UChar);
    friend JSString* jsNontrivialString(VM*, const String&);
    friend JSString* jsNontrivialString(VM*, String&&);
    friend JSString* jsSubstring(VM*, const String&, unsigned, unsigned);
    friend JSString* jsSubstring(VM&, ExecState*, JSString*, unsigned, unsigned);
    friend JSString* jsSubstringOfResolved(VM&, GCDeferralContext*, JSString*, unsigned, unsigned);
    friend JSString* jsOwnedString(VM*, const String&);
};

// NOTE: This class cannot override JSString's destructor. JSString's destructor is called directly
// from JSStringSubspace::
class JSRopeString final : public JSString {
    friend class JSString;
public:
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
        JSString* fiber1() const
        {
            return bitwise_cast<JSString*>(static_cast<uintptr_t>(m_fiber1Lower) | (static_cast<uintptr_t>(m_fiber1Upper) << 32));
        }

        void initializeFiber1(JSString* fiber)
        {
            uintptr_t pointer = bitwise_cast<uintptr_t>(fiber);
            m_fiber1Lower = static_cast<uint32_t>(pointer);
            m_fiber1Upper = static_cast<uint16_t>(pointer >> 32);
        }

        JSString* fiber2() const
        {
            return bitwise_cast<JSString*>(static_cast<uintptr_t>(m_fiber2Lower) | (static_cast<uintptr_t>(m_fiber2Upper) << 16));
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
                result = jsEmptyString(&m_vm);
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
        if (base->isSubstring()) {
            JSRopeString* baseRope = jsCast<JSRopeString*>(base);
            initializeSubstringBase(baseRope->substringBase());
            initializeSubstringOffset(baseRope->substringOffset() + offset);
        } else {
            initializeSubstringBase(base);
            initializeSubstringOffset(offset);
        }
        ASSERT(length == this->length());
    }

    enum SubstringOfResolvedTag { SubstringOfResolved };
    JSRopeString(SubstringOfResolvedTag, VM& vm, JSString* base, unsigned offset, unsigned length)
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
    }

    ALWAYS_INLINE void finishCreationSubstring(VM& vm, ExecState* exec)
    {
        Base::finishCreation(vm);
        JSString* updatedBase = substringBase();
        // For now, let's not allow substrings with a rope base.
        // Resolve non-substring rope bases so we don't have to deal with it.
        // FIXME: Evaluate if this would be worth adding more branches.
        if (updatedBase->isRope())
            jsCast<JSRopeString*>(updatedBase)->resolveRope(exec);
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

    static JSRopeString* create(VM& vm, ExecState* exec, JSString* base, unsigned offset, unsigned length)
    {
        JSRopeString* newString = new (NotNull, allocateCell<JSRopeString>(vm.heap)) JSRopeString(vm, base, offset, length);
        newString->finishCreationSubstring(vm, exec);
        ASSERT(newString->length());
        ASSERT(newString->isRope());
        return newString;
    }

    ALWAYS_INLINE static JSRopeString* createSubstringOfResolved(VM& vm, GCDeferralContext* deferralContext, JSString* base, unsigned offset, unsigned length)
    {
        JSRopeString* newString = new (NotNull, allocateCell<JSRopeString>(vm.heap, deferralContext)) JSRopeString(SubstringOfResolved, vm, base, offset, length);
        newString->finishCreationSubstringOfResolved(vm);
        ASSERT(newString->length());
        ASSERT(newString->isRope());
        return newString;
    }

    friend JSValue jsStringFromRegisterArray(ExecState*, Register*, unsigned);
    friend JSValue jsStringFromArguments(ExecState*, JSValue);

    // If nullOrExecForOOM is null, resolveRope() will be do nothing in the event of an OOM error.
    // The rope value will remain a null string in that case.
    JS_EXPORT_PRIVATE const String& resolveRope(ExecState* nullOrExecForOOM) const;
    template<typename Function> const String& resolveRopeWithFunction(ExecState* nullOrExecForOOM, Function&&) const;
    JS_EXPORT_PRIVATE AtomicString resolveRopeToAtomicString(ExecState*) const;
    JS_EXPORT_PRIVATE RefPtr<AtomicStringImpl> resolveRopeToExistingAtomicString(ExecState*) const;
    void resolveRopeSlowCase8(LChar*) const;
    void resolveRopeSlowCase(UChar*) const;
    void outOfMemory(ExecState* nullOrExecForOOM) const;
    void resolveRopeInternal8(LChar*) const;
    void resolveRopeInternal8NoSubstring(LChar*) const;
    void resolveRopeInternal16(UChar*) const;
    void resolveRopeInternal16NoSubstring(UChar*) const;
    StringView unsafeView(ExecState*) const;
    StringViewWithUnderlyingString viewWithUnderlyingString(ExecState*) const;

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

    friend JSString* jsString(ExecState*, JSString*, JSString*);
    friend JSString* jsString(ExecState*, const String&, JSString*);
    friend JSString* jsString(ExecState*, JSString*, const String&);
    friend JSString* jsString(ExecState*, const String&, const String&);
    friend JSString* jsString(ExecState*, JSString*, JSString*, JSString*);
    friend JSString* jsString(ExecState*, const String&, const String&, const String&);
    friend JSString* jsSubstringOfResolved(VM&, GCDeferralContext*, JSString*, unsigned, unsigned);
    friend JSString* jsSubstring(VM&, ExecState*, JSString*, unsigned, unsigned);
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
inline JSString* jsEmptyString(VM* vm)
{
    return vm->smallStrings.emptyString();
}

ALWAYS_INLINE JSString* jsSingleCharacterString(VM* vm, UChar c)
{
    if (validateDFGDoesGC)
        RELEASE_ASSERT(vm->heap.expectDoesGC());
    if (c <= maxSingleCharacterString)
        return vm->smallStrings.singleCharacterString(c);
    return JSString::create(*vm, StringImpl::create(&c, 1));
}

inline JSString* jsNontrivialString(VM* vm, const String& s)
{
    ASSERT(s.length() > 1);
    return JSString::create(*vm, *s.impl());
}

inline JSString* jsNontrivialString(VM* vm, String&& s)
{
    ASSERT(s.length() > 1);
    return JSString::create(*vm, s.releaseImpl().releaseNonNull());
}

ALWAYS_INLINE Identifier JSString::toIdentifier(ExecState* exec) const
{
    return Identifier::fromString(exec, toAtomicString(exec));
}

ALWAYS_INLINE AtomicString JSString::toAtomicString(ExecState* exec) const
{
    if (validateDFGDoesGC)
        RELEASE_ASSERT(vm()->heap.expectDoesGC());
    if (isRope())
        return static_cast<const JSRopeString*>(this)->resolveRopeToAtomicString(exec);
    return AtomicString(valueInternal());
}

ALWAYS_INLINE RefPtr<AtomicStringImpl> JSString::toExistingAtomicString(ExecState* exec) const
{
    if (validateDFGDoesGC)
        RELEASE_ASSERT(vm()->heap.expectDoesGC());
    if (isRope())
        return static_cast<const JSRopeString*>(this)->resolveRopeToExistingAtomicString(exec);
    if (valueInternal().impl()->isAtomic())
        return static_cast<AtomicStringImpl*>(valueInternal().impl());
    return AtomicStringImpl::lookUp(valueInternal().impl());
}

inline const String& JSString::value(ExecState* exec) const
{
    if (validateDFGDoesGC)
        RELEASE_ASSERT(vm()->heap.expectDoesGC());
    if (isRope())
        return static_cast<const JSRopeString*>(this)->resolveRope(exec);
    return valueInternal();
}

inline const String& JSString::tryGetValue(bool allocationAllowed) const
{
    if (allocationAllowed) {
        if (validateDFGDoesGC)
            RELEASE_ASSERT(vm()->heap.expectDoesGC());
        if (isRope()) {
            // Pass nullptr for the ExecState so that resolveRope does not throw in the event of an OOM error.
            return static_cast<const JSRopeString*>(this)->resolveRope(nullptr);
        }
    } else
        RELEASE_ASSERT(!isRope());
    return valueInternal();
}

inline JSString* JSString::getIndex(ExecState* exec, unsigned i)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    ASSERT(canGetIndex(i));
    StringView view = unsafeView(exec);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return jsSingleCharacterString(exec, view[i]);
}

inline JSString* jsString(VM* vm, const String& s)
{
    int size = s.length();
    if (!size)
        return vm->smallStrings.emptyString();
    if (size == 1) {
        UChar c = s.characterAt(0);
        if (c <= maxSingleCharacterString)
            return vm->smallStrings.singleCharacterString(c);
    }
    return JSString::create(*vm, *s.impl());
}

inline JSString* jsSubstring(VM& vm, ExecState* exec, JSString* s, unsigned offset, unsigned length)
{
    ASSERT(offset <= s->length());
    ASSERT(length <= s->length());
    ASSERT(offset + length <= s->length());
    if (!length)
        return vm.smallStrings.emptyString();
    if (!offset && length == s->length())
        return s;
    return JSRopeString::create(vm, exec, s, offset, length);
}

inline JSString* jsSubstringOfResolved(VM& vm, GCDeferralContext* deferralContext, JSString* s, unsigned offset, unsigned length)
{
    ASSERT(offset <= s->length());
    ASSERT(length <= s->length());
    ASSERT(offset + length <= s->length());
    if (!length)
        return vm.smallStrings.emptyString();
    if (!offset && length == s->length())
        return s;
    return JSRopeString::createSubstringOfResolved(vm, deferralContext, s, offset, length);
}

inline JSString* jsSubstringOfResolved(VM& vm, JSString* s, unsigned offset, unsigned length)
{
    return jsSubstringOfResolved(vm, nullptr, s, offset, length);
}

inline JSString* jsSubstring(ExecState* exec, JSString* s, unsigned offset, unsigned length)
{
    return jsSubstring(exec->vm(), exec, s, offset, length);
}

inline JSString* jsSubstring(VM* vm, const String& s, unsigned offset, unsigned length)
{
    ASSERT(offset <= s.length());
    ASSERT(length <= s.length());
    ASSERT(offset + length <= s.length());
    if (!length)
        return vm->smallStrings.emptyString();
    if (length == 1) {
        UChar c = s.characterAt(offset);
        if (c <= maxSingleCharacterString)
            return vm->smallStrings.singleCharacterString(c);
    }
    auto impl = StringImpl::createSubstringSharingImpl(*s.impl(), offset, length);
    if (impl->isSubString())
        return JSString::createHasOtherOwner(*vm, WTFMove(impl));
    return JSString::create(*vm, WTFMove(impl));
}

inline JSString* jsOwnedString(VM* vm, const String& s)
{
    int size = s.length();
    if (!size)
        return vm->smallStrings.emptyString();
    if (size == 1) {
        UChar c = s.characterAt(0);
        if (c <= maxSingleCharacterString)
            return vm->smallStrings.singleCharacterString(c);
    }
    return JSString::createHasOtherOwner(*vm, *s.impl());
}

inline JSString* jsEmptyString(ExecState* exec) { return jsEmptyString(&exec->vm()); }
inline JSString* jsString(ExecState* exec, const String& s) { return jsString(&exec->vm(), s); }
inline JSString* jsSingleCharacterString(ExecState* exec, UChar c) { return jsSingleCharacterString(&exec->vm(), c); }
inline JSString* jsSubstring(ExecState* exec, const String& s, unsigned offset, unsigned length) { return jsSubstring(&exec->vm(), s, offset, length); }
inline JSString* jsNontrivialString(ExecState* exec, const String& s) { return jsNontrivialString(&exec->vm(), s); }
inline JSString* jsNontrivialString(ExecState* exec, String&& s) { return jsNontrivialString(&exec->vm(), WTFMove(s)); }
inline JSString* jsOwnedString(ExecState* exec, const String& s) { return jsOwnedString(&exec->vm(), s); }

ALWAYS_INLINE JSString* jsStringWithCache(ExecState* exec, const String& s)
{
    VM& vm = exec->vm();
    StringImpl* stringImpl = s.impl();
    if (!stringImpl || !stringImpl->length())
        return jsEmptyString(&vm);

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

ALWAYS_INLINE bool JSString::getStringPropertySlot(ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (propertyName == vm.propertyNames->length) {
        slot.setValue(this, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly, jsNumber(length()));
        return true;
    }

    Optional<uint32_t> index = parseIndex(propertyName);
    if (index && index.value() < length()) {
        JSValue value = getIndex(exec, index.value());
        RETURN_IF_EXCEPTION(scope, false);
        slot.setValue(this, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly, value);
        return true;
    }

    return false;
}

ALWAYS_INLINE bool JSString::getStringPropertySlot(ExecState* exec, unsigned propertyName, PropertySlot& slot)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (propertyName < length()) {
        JSValue value = getIndex(exec, propertyName);
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

ALWAYS_INLINE StringView JSRopeString::unsafeView(ExecState* exec) const
{
    if (validateDFGDoesGC)
        RELEASE_ASSERT(vm()->heap.expectDoesGC());
    if (isSubstring()) {
        auto& base = substringBase()->valueInternal();
        if (base.is8Bit())
            return StringView(base.characters8() + substringOffset(), length());
        return StringView(base.characters16() + substringOffset(), length());
    }
    return resolveRope(exec);
}

ALWAYS_INLINE StringViewWithUnderlyingString JSRopeString::viewWithUnderlyingString(ExecState* exec) const
{
    if (validateDFGDoesGC)
        RELEASE_ASSERT(vm()->heap.expectDoesGC());
    if (isSubstring()) {
        auto& base = substringBase()->valueInternal();
        if (base.is8Bit())
            return { { base.characters8() + substringOffset(), length() }, base };
        return { { base.characters16() + substringOffset(), length() }, base };
    }
    auto& string = resolveRope(exec);
    return { string, string };
}

ALWAYS_INLINE StringView JSString::unsafeView(ExecState* exec) const
{
    if (validateDFGDoesGC)
        RELEASE_ASSERT(vm()->heap.expectDoesGC());
    if (isRope())
        return static_cast<const JSRopeString*>(this)->unsafeView(exec);
    return valueInternal();
}

ALWAYS_INLINE StringViewWithUnderlyingString JSString::viewWithUnderlyingString(ExecState* exec) const
{
    if (isRope())
        return static_cast<const JSRopeString&>(*this).viewWithUnderlyingString(exec);
    return { valueInternal(), valueInternal() };
}

inline bool JSString::isSubstring() const
{
    return m_fiber & JSRopeString::isSubstringInPointer;
}

// --- JSValue inlines ----------------------------

inline bool JSValue::toBoolean(ExecState* exec) const
{
    if (isInt32())
        return asInt32();
    if (isDouble())
        return asDouble() > 0.0 || asDouble() < 0.0; // false for NaN
    if (isCell())
        return asCell()->toBoolean(exec);
    return isTrue(); // false, null, and undefined all convert to false.
}

inline JSString* JSValue::toString(ExecState* exec) const
{
    if (isString())
        return asString(asCell());
    bool returnEmptyStringOnError = true;
    return toStringSlowCase(exec, returnEmptyStringOnError);
}

inline JSString* JSValue::toStringOrNull(ExecState* exec) const
{
    if (isString())
        return asString(asCell());
    bool returnEmptyStringOnError = false;
    return toStringSlowCase(exec, returnEmptyStringOnError);
}

inline String JSValue::toWTFString(ExecState* exec) const
{
    if (isString())
        return static_cast<JSString*>(asCell())->value(exec);
    return toWTFStringSlowCase(exec);
}

} // namespace JSC
