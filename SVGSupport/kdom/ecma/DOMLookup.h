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
#include <iostream>
#endif
#include <kjs/value.h>
#include <kjs/object.h>
#include <kjs/lookup.h>
#include <kjs/interpreter.h> // for ExecState

#include <kdom/ecma/ScriptInterpreter.h>

// NOTE: Those macros are probably very hard to understand first,
//       so don't hesitate to contact us - whenever someone plans
//       to reuse this code - except for KSVG :)

#define KDOM_GET_COMMON \
public: \
    \
    /* The standard hasProperty call, auto-generated. Looks in hashtable, forwards to parents. */ \
    bool hasProperty(KJS::ExecState *exec, const KJS::Identifier &propertyName) const; \
    \
    /* get() method, called by DOMBridge::get */ \
    KJS::ValueImp *get(KJS::ExecState *exec, const KJS::Identifier &propertyName, const KJS::ObjectImp* bridge) const; \
    \
    /* Called by lookupGet(). Auto-generated. Forwards to the parent which has the given property. */ \
    KJS::ValueImp *getInParents(KJS::ExecState *exec, const KJS::Identifier &propertyName, const KJS::ObjectImp* bridge) const; \
    \
    KJS::ObjectImp *prototype(KJS::ExecState *exec) const;\
	\
	KJS::ObjectImp *bridge(KJS::ExecState *) const; \
    \
    static const KJS::ClassInfo s_classInfo; \
    \
    static const struct KJS::HashTable s_hashTable;

/**
 * For classes with properties to read, and a hashtable.
 */
#define KDOM_GET \
    KDOM_GET_COMMON \
    KJS::ValueImp *cache(KJS::ExecState *exec) const; 

/**
 * Same as KDOM_GET, but for base classes(kalyptus helps finding them). The 
 * difference is that cache() is virtual.
 */
#define KDOM_BASECLASS_GET \
    KDOM_GET_COMMON \
    virtual KJS::ValueImp *cache(KJS::ExecState *exec) const; 

/**
 * For read-write classes only, i.e. those which support put().
 */
#define KDOM_PUT \
    \
    /* put() method, called by DOMBridge::put */ \
    bool put(KJS::ExecState *exec, const KJS::Identifier &propertyName, \
			 KJS::ValueImp *value, int attr); \
	\
    /* Called by lookupPut. Auto-generated. Looks in hashtable, forwards to parents. */ \
    bool putInParents(KJS::ExecState *exec, const KJS::Identifier &propertyName, \
					  KJS::ValueImp *value, int attr);

/**
 * For classes which inherit a read-write class, 
 * but have no read-write property themselves.
 */
#define KDOM_FORWARDPUT \
    \
    /* put() method, called by DOMBridge::put */ \
    bool put(KJS::ExecState *exec, const KJS::Identifier &propertyName, KJS::ValueImp *value, int attr);

/**
 * Used in generatedata.cpp
 *
 * @param p1 exec
 * @param p2 propertyName
 * @param p3 bridge
 * @internal
 */
#define GET_METHOD_ARGS KJS::ExecState *p1, const KJS::Identifier &p2, const KJS::ObjectImp *p3

/**
 * Used in generated.cpp
 *
 * @param p1 exec
 * @param p2 propertyName
 * @param p3 bridge
 * @param p4 attr
 * @internal
 */
#define PUT_METHOD_ARGS KJS::ExecState *p1, const KJS::Identifier &p2, KJS::ValueImp *p3, int p4

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

// Useful macros for dealing with ecma constructors (enum types, for example)
#define ECMA_IMPLEMENT_CONSTRUCTOR(Space, ClassName, Class) \
 KJS::ValueImp *ClassName::get(KJS::ExecState *, const KJS::Identifier &propertyName, const KJS::ObjectImp *) const { \
 const KJS::HashEntry *entry = KJS::Lookup::findEntry(&s_hashTable, propertyName); \
 if(entry) \
   return KJS::Number(entry->value); \
 return KJS::Undefined(); \
 } \
 bool ClassName::hasProperty(KJS::ExecState *, const KJS::Identifier &propertyName) const { \
  return KJS::Lookup::findEntry(&s_hashTable, propertyName); \
 } \
 KJS::ObjectImp *ClassName::prototype(KJS::ExecState *exec) const { \
  return exec->interpreter()->builtinObjectPrototype(); \
 } \
 const KJS::ClassInfo ClassName::s_classInfo = { Class "Constructor", 0, &s_hashTable, 0 }; \
 KJS::ValueImp *Space::get##ClassName(KJS::ExecState *exec) { \
  return KDOM::cacheGlobalBridge<ClassName>(exec, "[[" Class ".constructor]]"); \
 }
 
// Just defines the constructor prototype (which will be called by the GlobalObject)
#define ECMA_DEFINE_CONSTRUCTOR(Space, ClassName) \
  namespace Space { \
  class ClassName : public KJS::ObjectImp { \
  public: \
    ClassName(KJS::ExecState *exec) : KJS::ObjectImp() {} \
    KJS::ValueImp *get(KJS::ExecState *exec, const KJS::Identifier &propertyName, const KJS::ObjectImp *obj) const; \
    bool hasProperty(KJS::ExecState *exec, const KJS::Identifier &propertyName) const; \
    KJS::ObjectImp *prototype(KJS::ExecState *exec) const; \
    static const KJS::ClassInfo s_classInfo; \
    static const struct KJS::HashTable s_hashTable; \
  }; \
  KJS::ValueImp *get##ClassName(KJS::ExecState *exec); \
  }

// Same as kjs' DEFINE_PROTOTYPE, but with a pointer to the hashtable too, and no ClassName here
// The ClassProto ctor(exec) must be public, so we can use KJS::cacheGlobalObject... (Niko)
#define ECMA_DEFINE_PROTOTYPE(Space, ClassProto) \
  namespace Space { \
  class ClassProto : public KJS::ObjectImp { \
  public: \
    static KJS::ObjectImp *self(KJS::ExecState *exec); \
    ClassProto( KJS::ExecState *exec ) \
      : KJS::ObjectImp( exec->interpreter()->builtinObjectPrototype() ) {} \
    virtual const KJS::ClassInfo *classInfo() const { return &info; } \
    static const KJS::ClassInfo info; \
    KJS::ValueImp *get(KJS::ExecState *exec, const KJS::Identifier &propertyName) const; \
    bool hasProperty(KJS::ExecState *exec, const KJS::Identifier &propertyName) const; \
    \
    static const struct KJS::HashTable s_hashTable; \
  }; \
  }

// same as IMPLEMENT_PROTOTYPE but in the KDOM namespace, and with ClassName here
// so that KDOM_DEFINE_PROTOTYPE can be put in a header file ('info' defined here)
#define ECMA_IMPLEMENT_PROTOTYPE(Space,ClassName,ClassProto,ClassFunc) \
    KJS::ValueImp *Space::ClassProto::get(KJS::ExecState *exec, const KJS::Identifier &propertyName) const \
    { \
        /* KJS::ValueImp *result; \
        if (KJS::lookupGetOwnFunction<ClassFunc,KJS::ObjectImp>(exec, propertyName, &s_hashTable, this, result)) \
        return result;  */ \
        return KJS::Undefined(); \
      /* return KJS::lookupGetFunction<ClassFunc,KJS::ObjectImp>(exec, propertyName, &s_hashTable, this ); */ \
    } \
    bool Space::ClassProto::hasProperty(KJS::ExecState *exec, const KJS::Identifier &propertyName) const \
    { /*stupid but we need this to have a common macro for the declaration*/ \
      return KJS::ObjectImp::hasProperty(exec, propertyName); \
    } \
    KJS::ObjectImp *Space::ClassProto::self(KJS::ExecState *exec) \
    { \
      return KJS::cacheGlobalObject<ClassProto>( exec, "[[" ClassName ".prototype]]" ); \
    } \
    const KJS::ClassInfo ClassProto::info = { ClassName, 0, &s_hashTable, 0 }; \

#define ECMA_IMPLEMENT_PROTOFUNC(Space,ClassFunc,Class) \
  namespace Space { \
  class ClassFunc : public KJS::ObjectImp { \
  public: \
    ClassFunc(KJS::ExecState *exec, int i, int len) \
       : KJS::ObjectImp( /*proto? */ ), id(i) { \
       put(exec, "length", KJS::Number(len), KJS::DontDelete | KJS::ReadOnly | KJS::DontEnum); \
    } \
    /** Used by callAsFunction() to check the type of thisObj. Generated code */ \
    Class cast(KJS::ExecState *exec, const KJS::ObjectImp *bridge) const; \
    \
    virtual bool implementsCall() const { return true; } \
	\
    /** You need to implement that one */ \
    virtual KJS::ValueImp *callAsFunction(KJS::ExecState *exec, KJS::ObjectImp *thisObj, const KJS::List &args); \
	\
  private: \
    int id; \
  }; \
  /* Is eventually generated, if KDOM_CAST is given */ \
     Class to##Class(KJS::ExecState *exec, const KJS::ObjectImp *bridge); \
  }

// To be used when casting the type of an argument
#define KDOM_CHECK(ClassName, theObj) \
    ClassName obj = cast(exec, theObj); \
    if(obj == ClassName::null) { \
	    kdDebug(26004) << k_funcinfo << " Wrong object type: expected " << ClassName::s_classInfo.className << " got " << thisObj->classInfo()->className << endl; \
        ObjectImp *err = Error::create(exec,TypeError); \
      	exec->setException(err); \
	    return err; \
    }

// To be used in all callAsFunction() implementations!
// Can't use if (!thisObj.inherits(&ClassName::s_classInfo) since we don't
// use the (single-parent) inheritance of ClassInfo...
#define KDOM_CHECK_THIS(ClassName) KDOM_CHECK(ClassName, thisObj)

// Helpers to hide exception stuff...
// used within get/putValueProprety
#define KDOM_ENTER_SAFE try {
#define KDOM_LEAVE_SAFE(Exception) } catch(Exception::Private *e) { KJS::ObjectImp *err = KJS::Error::create(exec, KJS::GeneralError, QString::fromLatin1("%1 %2").arg(QString::fromLatin1(Exception(e).s_classInfo.className)).arg(QString::number(e->code())).latin1()); err->put(exec, "code", KJS::Number(e->code())); exec->setException(err); }
#define KDOM_LEAVE_CALL_SAFE(Exception) } catch(Exception::Private *e) { KJS::ObjectImp *err = KJS::Error::create(exec, KJS::GeneralError, QString::fromLatin1("%1 %2").arg(QString::fromLatin1(Exception(e).s_classInfo.className)).arg(QString::number(e->code())).latin1()); err->put(exec, "code", KJS::Number(e->code())); exec->setException(err); return err; }

// Just a marker for kalyptus
#define KDOM_CAST ;
#define KDOM_DEFINE_CAST(Class) Class to##Class(KJS::ExecState *exec, const KJS::ObjectImp *bridge);

// KDOM Internal helpers - convenience functions
#define KDOM_DEFINE_CONSTRUCTOR(ClassName) ECMA_DEFINE_CONSTRUCTOR(KDOM, ClassName)
#define KDOM_IMPLEMENT_CONSTRUCTOR(ClassName, Class) ECMA_IMPLEMENT_CONSTRUCTOR(KDOM, ClassName, Class)

#define KDOM_DEFINE_PROTOTYPE(ClassName) ECMA_DEFINE_PROTOTYPE(KDOM, ClassName)
#define KDOM_IMPLEMENT_PROTOFUNC(ClassFunc, Class) ECMA_IMPLEMENT_PROTOFUNC(KDOM, ClassFunc, Class)

#define KDOM_IMPLEMENT_PROTOTYPE(ClassName,ClassProto,ClassFunc) ECMA_IMPLEMENT_PROTOTYPE(KDOM, ClassName, ClassProto, ClassFunc)

#endif

// vim:ts=4:noet
