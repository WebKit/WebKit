/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KDOM_DOMLookup_H
#define KDOM_DOMLookup_H

#ifndef APPLE_CHANGES
#include <ostream>
#endif

#include <kdebug.h>

#include <kjs/value.h>
#include <kjs/object.h>
#include <kjs/lookup.h>
#include <kjs/interpreter.h>

#include <kdom/ecma/ScriptInterpreter.h>

namespace KDOM
{
    /**
     * Helper method for property lookups.
     *
     * This method does it all (looking in the hashtable, checking for function
     * overrides, creating the function or retrieving from cache, calling
     * getValueProperty in case of a non-function property, forwarding to parent[s] if
     * unknown property).
     *
     * Template arguments:
     * @param FuncImp the class which implements this object's functions
     * @param ThisImp the class of "this". It must implement the getValueProperty(exec,token) method,
     * for non-function properties, and the getInParents() method (auto-generated).
     *
     * Method arguments:
     * @param exec execution state, as usual
     * @param propertyName the property we're looking for
     * @param table the static hashtable for this class
     * @param thisObj "this"
     */
    template<class FuncImp, class ThisImp>
    inline KJS::ValueImp *lookupGet(KJS::ExecState *exec,
                                const KJS::Identifier &propertyName,
                                const KJS::HashTable *table,
                                const ThisImp *thisObj, // the 'impl' object
                                const KJS::ObjectImp *bridge)
    {
#if 0
        const KJS::HashEntry *entry = KJS::Lookup::findEntry(table, propertyName);

        if(!entry) // not found, forward to parents
            return thisObj->getInParents(exec, propertyName, bridge);

        if(entry->attr & KJS::Function)
            return KJS::lookupOrCreateFunction<FuncImp>(exec, propertyName,
                                                        const_cast<KJS::ObjectImp *>(bridge),
                                                        entry->value, entry->params, entry->attr);

        return thisObj->getValueProperty(exec, entry->value);
#endif
                return KJS::Undefined();
    }

    /**
     * Simplified version of lookupGet in case there are no functions, only "values".
     * Using this instead of lookupGet removes the need for a FuncImp class.
     */
    template <class ThisImp>
    inline KJS::ValueImp *lookupGetValue(KJS::ExecState *exec,
                                     const KJS::Identifier &propertyName,
                                     const KJS::HashTable *table,
                                     const ThisImp *thisObj, // the 'impl' object
                                     const KJS::ObjectImp *bridge)
    {
#if 0
        const KJS::HashEntry *entry = KJS::Lookup::findEntry(table, propertyName);

        if(!entry) // not found, forward to parents
            return thisObj->getInParents(exec, propertyName, bridge);

        if(entry->attr & KJS::Function)
            kdError(26004) << "Function bit set! Shouldn't happen in lookupGetValue! propertyName was " << propertyName.qstring() << endl;

        return thisObj->getValueProperty(exec, entry->value);
#endif
                return KJS::Undefined();
    }

    /**
     * This one is for "put".
     * Lookup hash entry for property to be set, and set the value.
     * The "this" class must implement putValueProperty.
     * If it returns false, put() will return false, and KDOMRequest will set a dynamic property in ObjectImp
     */
    template <class ThisImp>
    inline bool lookupPut(KJS::ExecState *exec,
                          const KJS::Identifier &propertyName,
                          KJS::ValueImp *value,
                          int attr,
                          const KJS::HashTable *table,
                          ThisImp *thisObj)
    {
        const KJS::HashEntry *entry = KJS::Lookup::findEntry(table, propertyName);

        if(!entry) // not found, forward to parents
            return thisObj->putInParents(exec, propertyName, value, attr);
        else if(entry->attr & KJS::Function) // Function: put as override property
            return false;
        else if(entry->attr & KJS::ReadOnly && !(attr & KJS::Internal)) // readonly! Can't put!
        {
#ifdef KJS_VERBOSE
            kdWarning(26004) <<" Attempt to change value of readonly property '" << propertyName.qstring() << "'" << endl;
#endif
            return true; // "we did it" -> don't put override property
        }
        else
        {
            thisObj->putValueProperty(exec, entry->value, value, attr);
            return true;
        }
    }
}

// These macros are used by the generated files in kdom/bindings/js/*.
// IDLCodeGeneratorJs.pm knows how to use them, read it's source :-)
#define ECMA_IMPLEMENT_CONSTRUCTOR(ClassName, Class) \
    KJS::ValueImp *ClassName::get(KJS::ExecState *, const KJS::Identifier &propertyName, const KJS::ObjectImp *) const { \
        const KJS::HashEntry *entry = KJS::Lookup::findEntry(&s_hashTable, propertyName); \
        if(entry) return KJS::Number(entry->value); \
        return KJS::Undefined(); \
    } \
    bool ClassName::hasProperty(KJS::ExecState *, const KJS::Identifier &propertyName) const { \
        return KJS::Lookup::findEntry(&s_hashTable, propertyName); \
    } \
    KJS::ObjectImp *ClassName::prototype(KJS::ExecState *exec) const { \
        return exec->interpreter()->builtinObjectPrototype(); \
    } \
    const KJS::ClassInfo ClassName::s_classInfo = { Class "Constructor", 0, &s_hashTable, 0 }; \
    KJS::ValueImp *get##ClassName(KJS::ExecState *exec) { \
        return KJS::cacheGlobalObject<ClassName>(exec, "[[" Class ".constructor]]"); \
    }
 
#define ECMA_DEFINE_CONSTRUCTOR(ClassName) \
    class ClassName : public KJS::ObjectImp { \
    public: \
        ClassName(KJS::ExecState *) : KJS::ObjectImp() { } \
        KJS::ValueImp *get(KJS::ExecState *exec, const KJS::Identifier &propertyName, const KJS::ObjectImp *obj) const; \
        bool hasProperty(KJS::ExecState *exec, const KJS::Identifier &propertyName) const; \
        KJS::ObjectImp *prototype(KJS::ExecState *exec) const; \
        \
        static const KJS::ClassInfo s_classInfo; \
        static const struct KJS::HashTable s_hashTable; \
    }; \
    KJS::ValueImp *get##ClassName(KJS::ExecState *exec);

#define ECMA_DEFINE_PROTOTYPE(ClassProto) \
    class ClassProto : public KJS::ObjectImp { \
    public: \
        static KJS::ObjectImp *self(KJS::ExecState *exec); \
        ClassProto(KJS::ExecState *exec) : KJS::ObjectImp(exec->interpreter()->builtinObjectPrototype()) { } \
        virtual const KJS::ClassInfo *classInfo() const { return &info; } \
        static const KJS::ClassInfo info; \
        KJS::ValueImp *get(KJS::ExecState *exec, const KJS::Identifier &propertyName) const; \
        bool hasProperty(KJS::ExecState *exec, const KJS::Identifier &propertyName) const; \
        static const struct KJS::HashTable s_hashTable; \
    };

#define ECMA_IMPLEMENT_PROTOTYPE(ClassName, ClassProto, ClassFunc) \
    KJS::ValueImp *ClassProto::get(KJS::ExecState *exec, const KJS::Identifier &propertyName) const { \
        return KJS::Undefined(); \
    } \
    bool ClassProto::hasProperty(KJS::ExecState *exec, const KJS::Identifier &propertyName) const { \
        return KJS::ObjectImp::hasProperty(exec, propertyName); \
    } \
    KJS::ObjectImp *ClassProto::self(KJS::ExecState *exec) { \
        return KJS::cacheGlobalObject<ClassProto>(exec, "[[" ClassName ".prototype]]"); \
    } \
    const KJS::ClassInfo ClassProto::info = { ClassName, 0, &s_hashTable, 0 };

#define ECMA_IMPLEMENT_PROTOFUNC(ClassFunc, Class, ClassImpl) \
    class ClassFunc : public KJS::ObjectImp { \
    public: \
        ClassFunc(KJS::ExecState *exec, int i, int len) : KJS::ObjectImp(/* proto? */), id(i) { \
            put(exec, "length", KJS::Number(len), KJS::DontDelete | KJS::ReadOnly | KJS::DontEnum); \
        } \
        /** Used by callAsFunction() to check the type of thisObj. Generated code */ \
        ClassImpl *cast(KJS::ExecState *exec, const KJS::ObjectImp *bridge) const; \
        \
        virtual bool implementsCall() const { return true; } \
        virtual KJS::ValueImp *callAsFunction(KJS::ExecState *exec, KJS::ObjectImp *thisObj, const KJS::List &args); \
        \
    private: \
        int id; \
    };

// To be used when casting the type of an argument
#define KDOM_CHECK(DOMObjImpl, DOMObjWrapper, theObj) \
    DOMObjImpl *obj = cast(exec, static_cast<KJS::ObjectImp *>(theObj)); \
    if(!obj) { \
        QString errMsg = QString::fromLatin1("Attempt at calling a function that expects a ") + \
                         QString::fromLatin1(DOMObjWrapper::s_classInfo.className) + \
                         QString::fromLatin1(" on a ") + \
                         QString::fromLatin1(theObj->classInfo()->className); \
        kdDebug(26004) << k_funcinfo << " " << errMsg << endl; \
        KJS::ObjectImp *err = KJS::throwError(exec, KJS::TypeError, errMsg.ascii()); \
        return err; \
    }

// To be used in all call() implementations!
// Can't use if (!thisObj.inherits(&ClassName::s_classInfo) since
// we don't use the (single-parent) inheritance of ClassInfo...
#define KDOM_CHECK_THIS(DOMObjImpl, DOMObjWrapper) KDOM_CHECK(DOMObjImpl, DOMObjWrapper, thisObj)

// Helpers to hide exception stuff...
// used within get/putValueProprety
#define KDOM_ENTER_SAFE try {

#define KDOM_LEAVE_SAFE_COMMON(Exception) \
    catch(Exception##Impl *e) { \
        KJS::ObjectImp *err = KJS::throwError(exec, KJS::GeneralError, \
                                             QString::fromLatin1("%1 %2"). \
                                             arg(QString::fromLatin1(Exception##Wrapper::s_classInfo.className)). \
                                             arg(QString::number(e->code())).latin1()); \
        err->put(exec, "code", KJS::Number(e->code())); \
    }

// The first exceptions uses this macro...
#define KDOM_LEAVE_SAFE(Exception) \
    } \
    KDOM_LEAVE_SAFE_COMMON(Exception)

// ... and all others this one
#define KDOM_LEAVE_SAFE_NEXT(Exception) \
    KDOM_LEAVE_SAFE_COMMON(Exception)

#define KDOM_LEAVE_CALL_SAFE_COMMON(Exception) \
    catch(Exception##Impl *e) { \
        KJS::ObjectImp *err = KJS::throwError(exec, KJS::GeneralError, \
                                             QString::fromLatin1("%1 %2"). \
                                             arg(QString::fromLatin1(Exception##Wrapper::s_classInfo.className)). \
                                             arg(QString::number(e->code())).latin1()); \
        err->put(exec, "code", KJS::Number(e->code())); \
        return err; \
    }

// The first exceptions uses this macro...
#define KDOM_LEAVE_CALL_SAFE(Exception) \
    } \
    KDOM_LEAVE_CALL_SAFE_COMMON(Exception)

// ... and all others this one.
#define KDOM_LEAVE_CALL_SAFE_NEXT(Exception) \
    KDOM_LEAVE_CALL_SAFE_COMMON(Exception)

#endif

// vim:ts=4:noet
