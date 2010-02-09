/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef JSString_h
#define JSString_h

#include "CallFrame.h"
#include "CommonIdentifiers.h"
#include "Identifier.h"
#include "JSNumberCell.h"
#include "PropertyDescriptor.h"
#include "PropertySlot.h"

namespace JSC {

    class JSString;

    JSString* jsEmptyString(JSGlobalData*);
    JSString* jsEmptyString(ExecState*);
    JSString* jsString(JSGlobalData*, const UString&); // returns empty string if passed null string
    JSString* jsString(ExecState*, const UString&); // returns empty string if passed null string

    JSString* jsSingleCharacterString(JSGlobalData*, UChar);
    JSString* jsSingleCharacterString(ExecState*, UChar);
    JSString* jsSingleCharacterSubstring(JSGlobalData*, const UString&, unsigned offset);
    JSString* jsSingleCharacterSubstring(ExecState*, const UString&, unsigned offset);
    JSString* jsSubstring(JSGlobalData*, const UString&, unsigned offset, unsigned length);
    JSString* jsSubstring(ExecState*, const UString&, unsigned offset, unsigned length);

    // Non-trivial strings are two or more characters long.
    // These functions are faster than just calling jsString.
    JSString* jsNontrivialString(JSGlobalData*, const UString&);
    JSString* jsNontrivialString(ExecState*, const UString&);
    JSString* jsNontrivialString(JSGlobalData*, const char*);
    JSString* jsNontrivialString(ExecState*, const char*);

    // Should be used for strings that are owned by an object that will
    // likely outlive the JSValue this makes, such as the parse tree or a
    // DOM object that contains a UString
    JSString* jsOwnedString(JSGlobalData*, const UString&); 
    JSString* jsOwnedString(ExecState*, const UString&); 

    typedef void (*JSStringFinalizerCallback)(JSString*, void* context);
    JSString* jsStringWithFinalizer(ExecState*, const UString&, JSStringFinalizerCallback callback, void* context);

    class JS_EXPORTCLASS JSString : public JSCell {
    public:
        friend class JIT;
        friend class JSGlobalData;

        // A Rope is a string composed of a set of substrings.
        class Rope : public RefCounted<Rope> {
        public:
            // A Rope is composed from a set of smaller strings called Fibers.
            // Each Fiber in a rope is either UString::Rep or another Rope.
            class Fiber {
            public:
                Fiber() : m_value(0) {}
                Fiber(UString::Rep* string) : m_value(reinterpret_cast<intptr_t>(string)) {}
                Fiber(Rope* rope) : m_value(reinterpret_cast<intptr_t>(rope) | 1) {}

                Fiber(void* nonFiber) : m_value(reinterpret_cast<intptr_t>(nonFiber)) {}

                void deref()
                {
                    if (isRope())
                        rope()->deref();
                    else
                        string()->deref();
                }

                Fiber& ref()
                {
                    if (isString())
                        string()->ref();
                    else
                        rope()->ref();
                    return *this;
                }

                unsigned refAndGetLength()
                {
                    if (isString()) {
                        UString::Rep* rep = string();
                        return rep->ref()->size();
                    } else {
                        Rope* r = rope();
                        r->ref();
                        return r->stringLength();
                    }
                }

                bool isRope() { return m_value & 1; }
                Rope* rope() { return reinterpret_cast<Rope*>(m_value & ~1); }
                bool isString() { return !isRope(); }
                UString::Rep* string() { return reinterpret_cast<UString::Rep*>(m_value); }

                void* nonFiber() { return reinterpret_cast<void*>(m_value); }
            private:
                intptr_t m_value;
            };

            // Creates a Rope comprising of 'ropeLength' Fibers.
            // The Rope is constructed in an uninitialized state - initialize must be called for each Fiber in the Rope.
            static PassRefPtr<Rope> createOrNull(unsigned ropeLength)
            {
                void* allocation;
                if (tryFastMalloc(sizeof(Rope) + (ropeLength - 1) * sizeof(Fiber)).getValue(allocation))
                    return adoptRef(new (allocation) Rope(ropeLength));
                return 0;
            }

            ~Rope();
            void destructNonRecursive();

            void append(unsigned &index, Fiber& fiber)
            {
                m_fibers[index++] = fiber;
                m_stringLength += fiber.refAndGetLength();
            }
            void append(unsigned &index, const UString& string)
            {
                UString::Rep* rep = string.rep();
                m_fibers[index++] = Fiber(rep);
                m_stringLength += rep->ref()->size();
            }
            void append(unsigned& index, JSString* jsString)
            {
                if (jsString->isRope()) {
                    for (unsigned i = 0; i < jsString->m_ropeLength; ++i)
                        append(index, jsString->m_fibers[i]);
                } else
                    append(index, jsString->string());
            }

            unsigned ropeLength() { return m_ropeLength; }
            unsigned stringLength() { return m_stringLength; }
            Fiber& fibers(unsigned index) { return m_fibers[index]; }

        private:
            Rope(unsigned ropeLength) : m_ropeLength(ropeLength), m_stringLength(0) {}
            void* operator new(size_t, void* inPlace) { return inPlace; }
            
            unsigned m_ropeLength;
            unsigned m_stringLength;
            Fiber m_fibers[1];
        };

        ALWAYS_INLINE JSString(JSGlobalData* globalData, const UString& value)
            : JSCell(globalData->stringStructure.get())
            , m_stringLength(value.size())
            , m_value(value)
            , m_ropeLength(0)
        {
            Heap::heap(this)->reportExtraMemoryCost(value.cost());
        }

        enum HasOtherOwnerType { HasOtherOwner };
        JSString(JSGlobalData* globalData, const UString& value, HasOtherOwnerType)
            : JSCell(globalData->stringStructure.get())
            , m_stringLength(value.size())
            , m_value(value)
            , m_ropeLength(0)
        {
        }
        JSString(JSGlobalData* globalData, PassRefPtr<UString::Rep> value, HasOtherOwnerType)
            : JSCell(globalData->stringStructure.get())
            , m_stringLength(value->size())
            , m_value(value)
            , m_ropeLength(0)
        {
        }
        JSString(JSGlobalData* globalData, PassRefPtr<JSString::Rope> rope)
            : JSCell(globalData->stringStructure.get())
            , m_stringLength(rope->stringLength())
            , m_ropeLength(1)
        {
            m_fibers[0] = rope.releaseRef();
        }
        // This constructor constructs a new string by concatenating s1 & s2.
        // This should only be called with ropeLength <= 3.
        JSString(JSGlobalData* globalData, unsigned ropeLength, JSString* s1, JSString* s2)
            : JSCell(globalData->stringStructure.get())
            , m_stringLength(s1->length() + s2->length())
            , m_ropeLength(ropeLength)
        {
            ASSERT(ropeLength <= s_maxInternalRopeLength);
            unsigned index = 0;
            appendStringInConstruct(index, s1);
            appendStringInConstruct(index, s2);
            ASSERT(ropeLength == index);
        }
        // This constructor constructs a new string by concatenating s1 & s2.
        // This should only be called with ropeLength <= 3.
        JSString(JSGlobalData* globalData, unsigned ropeLength, JSString* s1, const UString& u2)
            : JSCell(globalData->stringStructure.get())
            , m_stringLength(s1->length() + u2.size())
            , m_ropeLength(ropeLength)
        {
            ASSERT(ropeLength <= s_maxInternalRopeLength);
            unsigned index = 0;
            appendStringInConstruct(index, s1);
            appendStringInConstruct(index, u2);
            ASSERT(ropeLength == index);
        }
        // This constructor constructs a new string by concatenating s1 & s2.
        // This should only be called with ropeLength <= 3.
        JSString(JSGlobalData* globalData, unsigned ropeLength, const UString& u1, JSString* s2)
            : JSCell(globalData->stringStructure.get())
            , m_stringLength(u1.size() + s2->length())
            , m_ropeLength(ropeLength)
        {
            ASSERT(ropeLength <= s_maxInternalRopeLength);
            unsigned index = 0;
            appendStringInConstruct(index, u1);
            appendStringInConstruct(index, s2);
            ASSERT(ropeLength == index);
        }
        // This constructor constructs a new string by concatenating v1, v2 & v3.
        // This should only be called with ropeLength <= 3 ... which since every
        // value must require a ropeLength of at least one implies that the length
        // for each value must be exactly 1!
        JSString(ExecState* exec, JSValue v1, JSValue v2, JSValue v3)
            : JSCell(exec->globalData().stringStructure.get())
            , m_stringLength(0)
            , m_ropeLength(s_maxInternalRopeLength)
        {
            unsigned index = 0;
            appendValueInConstructAndIncrementLength(exec, index, v1);
            appendValueInConstructAndIncrementLength(exec, index, v2);
            appendValueInConstructAndIncrementLength(exec, index, v3);
            ASSERT(index == s_maxInternalRopeLength);
        }

        JSString(JSGlobalData* globalData, const UString& value, JSStringFinalizerCallback finalizer, void* context)
            : JSCell(globalData->stringStructure.get())
            , m_stringLength(value.size())
            , m_value(value)
            , m_ropeLength(0)
        {
            // nasty hack because we can't union non-POD types
            m_fibers[0] = reinterpret_cast<void*>(reinterpret_cast<ptrdiff_t>(finalizer));
            m_fibers[1] = context;
            Heap::heap(this)->reportExtraMemoryCost(value.cost());
        }

        ~JSString()
        {
            ASSERT(vptr() == JSGlobalData::jsStringVPtr);
            for (unsigned i = 0; i < m_ropeLength; ++i)
                m_fibers[i].deref();

            if (!m_ropeLength && m_fibers[0].nonFiber()) {
                JSStringFinalizerCallback finalizer = reinterpret_cast<JSStringFinalizerCallback>(m_fibers[0].nonFiber());
                finalizer(this, m_fibers[1].nonFiber());
            }
        }

        const UString& value(ExecState* exec) const
        {
            if (isRope())
                resolveRope(exec);
            return m_value;
        }
        const UString tryGetValue() const
        {
            // If this is a rope, m_value should be null -
            // if this is not a rope, m_value should be non-null.
            ASSERT(isRope() == m_value.isNull());
            return m_value;
        }
        unsigned length() { return m_stringLength; }

        bool getStringPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        bool getStringPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);
        bool getStringPropertyDescriptor(ExecState*, const Identifier& propertyName, PropertyDescriptor&);

        bool canGetIndex(unsigned i) { return i < m_stringLength; }
        JSString* getIndex(ExecState*, unsigned);

        static PassRefPtr<Structure> createStructure(JSValue proto) { return Structure::create(proto, TypeInfo(StringType, OverridesGetOwnPropertySlot | NeedsThisConversion), AnonymousSlotCount); }

    private:
        enum VPtrStealingHackType { VPtrStealingHack };
        JSString(VPtrStealingHackType) 
            : JSCell(0)
            , m_ropeLength(0)
        {
        }

        void resolveRope(ExecState*) const;

        void appendStringInConstruct(unsigned& index, const UString& string)
        {
            m_fibers[index++] = Rope::Fiber(string.rep()->ref());
        }

        void appendStringInConstruct(unsigned& index, JSString* jsString)
        {
            if (jsString->isRope()) {
                for (unsigned i = 0; i < jsString->m_ropeLength; ++i)
                    m_fibers[index++] = jsString->m_fibers[i].ref();
            } else
                appendStringInConstruct(index, jsString->string());
        }

        void appendValueInConstructAndIncrementLength(ExecState* exec, unsigned& index, JSValue v)
        {
            if (v.isString()) {
                ASSERT(asCell(v)->isString());
                JSString* s = static_cast<JSString*>(asCell(v));
                ASSERT(s->ropeLength() == 1);
                appendStringInConstruct(index, s);
                m_stringLength += s->length();
            } else {
                UString u(v.toString(exec));
                m_fibers[index++] = Rope::Fiber(u.rep()->ref());
                m_stringLength += u.size();
            }
        }

        virtual JSValue toPrimitive(ExecState*, PreferredPrimitiveType) const;
        virtual bool getPrimitiveNumber(ExecState*, double& number, JSValue& value);
        virtual bool toBoolean(ExecState*) const;
        virtual double toNumber(ExecState*) const;
        virtual JSObject* toObject(ExecState*) const;
        virtual UString toString(ExecState*) const;

        virtual JSObject* toThisObject(ExecState*) const;
        virtual UString toThisString(ExecState*) const;
        virtual JSString* toThisJSString(ExecState*);

        // Actually getPropertySlot, not getOwnPropertySlot (see JSCell).
        virtual bool getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        virtual bool getOwnPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);
        virtual bool getOwnPropertyDescriptor(ExecState*, const Identifier&, PropertyDescriptor&);

        static const unsigned s_maxInternalRopeLength = 3;

        // A string is represented either by a UString or a Rope.
        unsigned m_stringLength;
        mutable UString m_value;
        mutable unsigned m_ropeLength;
        mutable Rope::Fiber m_fibers[s_maxInternalRopeLength];

        bool isRope() const { return m_ropeLength; }
        UString& string() { ASSERT(!isRope()); return m_value; }
        unsigned ropeLength() { return m_ropeLength ? m_ropeLength : 1; }

        friend JSValue jsString(ExecState* exec, JSString* s1, JSString* s2);
        friend JSValue jsString(ExecState* exec, const UString& u1, JSString* s2);
        friend JSValue jsString(ExecState* exec, JSString* s1, const UString& u2);
        friend JSValue jsString(ExecState* exec, Register* strings, unsigned count);
        friend JSValue jsString(ExecState* exec, JSValue thisValue, const ArgList& args);
        friend JSString* jsStringWithFinalizer(ExecState*, const UString&, JSStringFinalizerCallback callback, void* context);
    };

    JSString* asString(JSValue);

    // When an object is created from a different DLL, MSVC changes vptr to a "local" one right after invoking a constructor,
    // see <http://groups.google.com/group/microsoft.public.vc.language/msg/55cdcefeaf770212>.
    // This breaks isJSString(), and we don't need that hack anyway, so we change vptr back to primary one.
    // The below function must be called by any inline function that invokes a JSString constructor.
#if COMPILER(MSVC) && !defined(BUILDING_JavaScriptCore)
    inline JSString* fixupVPtr(JSGlobalData* globalData, JSString* string) { string->setVPtr(globalData->jsStringVPtr); return string; }
#else
    inline JSString* fixupVPtr(JSGlobalData*, JSString* string) { return string; }
#endif

    inline JSString* asString(JSValue value)
    {
        ASSERT(asCell(value)->isString());
        return static_cast<JSString*>(asCell(value));
    }

    inline JSString* jsEmptyString(JSGlobalData* globalData)
    {
        return globalData->smallStrings.emptyString(globalData);
    }

    inline JSString* jsSingleCharacterString(JSGlobalData* globalData, UChar c)
    {
        if (c <= 0xFF)
            return globalData->smallStrings.singleCharacterString(globalData, c);
        return fixupVPtr(globalData, new (globalData) JSString(globalData, UString(&c, 1)));
    }

    inline JSString* jsSingleCharacterSubstring(JSGlobalData* globalData, const UString& s, unsigned offset)
    {
        ASSERT(offset < static_cast<unsigned>(s.size()));
        UChar c = s.data()[offset];
        if (c <= 0xFF)
            return globalData->smallStrings.singleCharacterString(globalData, c);
        return fixupVPtr(globalData, new (globalData) JSString(globalData, UString(UString::Rep::create(s.rep(), offset, 1))));
    }

    inline JSString* jsNontrivialString(JSGlobalData* globalData, const char* s)
    {
        ASSERT(s);
        ASSERT(s[0]);
        ASSERT(s[1]);
        return fixupVPtr(globalData, new (globalData) JSString(globalData, s));
    }

    inline JSString* jsNontrivialString(JSGlobalData* globalData, const UString& s)
    {
        ASSERT(s.size() > 1);
        return fixupVPtr(globalData, new (globalData) JSString(globalData, s));
    }

    inline JSString* JSString::getIndex(ExecState* exec, unsigned i)
    {
        ASSERT(canGetIndex(i));
        return jsSingleCharacterSubstring(&exec->globalData(), value(exec), i);
    }

    inline JSString* jsString(JSGlobalData* globalData, const UString& s)
    {
        int size = s.size();
        if (!size)
            return globalData->smallStrings.emptyString(globalData);
        if (size == 1) {
            UChar c = s.data()[0];
            if (c <= 0xFF)
                return globalData->smallStrings.singleCharacterString(globalData, c);
        }
        return fixupVPtr(globalData, new (globalData) JSString(globalData, s));
    }

    inline JSString* jsStringWithFinalizer(ExecState* exec, const UString& s, JSStringFinalizerCallback callback, void* context)
    {
        ASSERT(s.size() && (s.size() > 1 || s.data()[0] > 0xFF));
        JSGlobalData* globalData = &exec->globalData();
        return fixupVPtr(globalData, new (globalData) JSString(globalData, s, callback, context));
    }

    inline JSString* jsSubstring(JSGlobalData* globalData, const UString& s, unsigned offset, unsigned length)
    {
        ASSERT(offset <= static_cast<unsigned>(s.size()));
        ASSERT(length <= static_cast<unsigned>(s.size()));
        ASSERT(offset + length <= static_cast<unsigned>(s.size()));
        if (!length)
            return globalData->smallStrings.emptyString(globalData);
        if (length == 1) {
            UChar c = s.data()[offset];
            if (c <= 0xFF)
                return globalData->smallStrings.singleCharacterString(globalData, c);
        }
        return fixupVPtr(globalData, new (globalData) JSString(globalData, UString(UString::Rep::create(s.rep(), offset, length)), JSString::HasOtherOwner));
    }

    inline JSString* jsOwnedString(JSGlobalData* globalData, const UString& s)
    {
        int size = s.size();
        if (!size)
            return globalData->smallStrings.emptyString(globalData);
        if (size == 1) {
            UChar c = s.data()[0];
            if (c <= 0xFF)
                return globalData->smallStrings.singleCharacterString(globalData, c);
        }
        return fixupVPtr(globalData, new (globalData) JSString(globalData, s, JSString::HasOtherOwner));
    }

    inline JSString* jsEmptyString(ExecState* exec) { return jsEmptyString(&exec->globalData()); }
    inline JSString* jsString(ExecState* exec, const UString& s) { return jsString(&exec->globalData(), s); }
    inline JSString* jsSingleCharacterString(ExecState* exec, UChar c) { return jsSingleCharacterString(&exec->globalData(), c); }
    inline JSString* jsSingleCharacterSubstring(ExecState* exec, const UString& s, unsigned offset) { return jsSingleCharacterSubstring(&exec->globalData(), s, offset); }
    inline JSString* jsSubstring(ExecState* exec, const UString& s, unsigned offset, unsigned length) { return jsSubstring(&exec->globalData(), s, offset, length); }
    inline JSString* jsNontrivialString(ExecState* exec, const UString& s) { return jsNontrivialString(&exec->globalData(), s); }
    inline JSString* jsNontrivialString(ExecState* exec, const char* s) { return jsNontrivialString(&exec->globalData(), s); }
    inline JSString* jsOwnedString(ExecState* exec, const UString& s) { return jsOwnedString(&exec->globalData(), s); } 

    ALWAYS_INLINE bool JSString::getStringPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
    {
        if (propertyName == exec->propertyNames().length) {
            slot.setValue(jsNumber(exec, m_stringLength));
            return true;
        }

        bool isStrictUInt32;
        unsigned i = propertyName.toStrictUInt32(&isStrictUInt32);
        if (isStrictUInt32 && i < m_stringLength) {
            slot.setValue(jsSingleCharacterSubstring(exec, value(exec), i));
            return true;
        }

        return false;
    }
        
    ALWAYS_INLINE bool JSString::getStringPropertySlot(ExecState* exec, unsigned propertyName, PropertySlot& slot)
    {
        if (propertyName < m_stringLength) {
            slot.setValue(jsSingleCharacterSubstring(exec, value(exec), propertyName));
            return true;
        }

        return false;
    }

    inline bool isJSString(JSGlobalData* globalData, JSValue v) { return v.isCell() && v.asCell()->vptr() == globalData->jsStringVPtr; }

    // --- JSValue inlines ----------------------------

    inline JSString* JSValue::toThisJSString(ExecState* exec)
    {
        return isCell() ? asCell()->toThisJSString(exec) : jsString(exec, toString(exec));
    }

    inline UString JSValue::toString(ExecState* exec) const
    {
        if (isString())
            return static_cast<JSString*>(asCell())->value(exec);
        if (isInt32())
            return exec->globalData().numericStrings.add(asInt32());
        if (isDouble())
            return exec->globalData().numericStrings.add(asDouble());
        if (isTrue())
            return "true";
        if (isFalse())
            return "false";
        if (isNull())
            return "null";
        if (isUndefined())
            return "undefined";
        ASSERT(isCell());
        return asCell()->toString(exec);
    }

    inline UString JSValue::toPrimitiveString(ExecState* exec) const
    {
        if (isString())
            return static_cast<JSString*>(asCell())->value(exec);
        if (isInt32())
            return exec->globalData().numericStrings.add(asInt32());
        if (isDouble())
            return exec->globalData().numericStrings.add(asDouble());
        if (isTrue())
            return "true";
        if (isFalse())
            return "false";
        if (isNull())
            return "null";
        if (isUndefined())
            return "undefined";
        ASSERT(isCell());
        return asCell()->toPrimitive(exec, NoPreference).toString(exec);
    }

} // namespace JSC

#endif // JSString_h
