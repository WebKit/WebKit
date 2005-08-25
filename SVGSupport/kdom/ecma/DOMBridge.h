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

#ifndef KDOM_DOMBridge_H
#define KDOM_DOMBridge_H

#include <kdebug.h>

#include <kjs/object.h>

namespace KJS
{
	class ValueImp;
	class UString;
	class ExecState;
};

namespace KDOM
{
	// A 'DOMBridge' inherits from KJS::ObjectImp, and operates on the
	// generated DOMClassWrapper classes located in kdom/bindings/js.
	//
	// If your wrapper contains writable properties, use 'DOMRWBridge'.
	//
	// The DOMClassWrapper functions support multiple inheritance.
	// This serves as a 'glue' between the 'flat' kjs inheritance structure
	// (one class <-> one parent) and our auto-generated MI ecma concept.
	template<class DOMObjWrapper, class DOMObjImpl>
	class DOMBridge : public KJS::ObjectImp
	{
	public:
		DOMBridge(KJS::ExecState *exec, DOMObjImpl *impl)
		: KJS::ObjectImp((m_wrapper = new DOMObjWrapper(impl))->prototype(exec)) { }

		DOMObjWrapper *wrapper() const { return m_wrapper; }

		virtual KJS::ValueImp *get(KJS::ExecState *exec, const KJS::Identifier &propertyName) const
		{
#if DEBUG_BRIDGE > 0
			kdDebug(26004) << "DOMBridge::get(), " << propertyName.qstring()
						   << " Name: " << classInfo()->className
						   << " Wrapper object: " << m_wrapper << endl;
#endif
			// Look for standard properties (e.g. those in the hashtables)
			KJS::ValueImp *val = m_wrapper->get(exec, propertyName, this);

			if(val->type() != KJS::UndefinedType)
				return val;

			// Not found -> forward to ObjectImp.
			val = KJS::ObjectImp::get(exec, propertyName);
#if DEBUG_BRIDGE > 0
			if(val->type() == KJS::UndefinedType)
			{
				kdDebug(26004) << "WARNING: " << propertyName.qstring()
							   << " not found in... Name: " << classInfo()->className
							   << " Wrapper object: " << m_wrapper << " on line: "
							   << exec->context().curStmtFirstLine() << endl;
			}
#endif
			return val;
		}

		virtual bool hasProperty(KJS::ExecState *exec, const KJS::Identifier &propertyName) const
		{
#if DEBUG_BRIDGE > 0
			kdDebug(26004) << "DOMBridge::hasProperty(), " << propertyName.qstring()
						   << " Name: " << classInfo()->className
						   << " Wrapper object: " << m_wrapper << endl;
#endif

			if(m_wrapper->hasProperty(exec, propertyName))
				return true;

			return KJS::ObjectImp::hasProperty(exec, propertyName);
		}

		virtual const KJS::ClassInfo *classInfo() const { return &DOMObjWrapper::s_classInfo; }

	protected:
		DOMObjWrapper *m_wrapper;
	};

	// Base class for readwrite bridges
	// T must also implement put (use KDOM_PUT in the header file)
	template<class DOMObjWrapper, class DOMObjImpl>
	class DOMRWBridge : public DOMBridge<DOMObjWrapper, DOMObjImpl>
	{
	public:
		DOMRWBridge(KJS::ExecState *exec, DOMObjImpl *impl)
		: DOMBridge<DOMObjWrapper, DOMObjImpl>(exec, impl) { }

		virtual void put(KJS::ExecState *exec, const KJS::Identifier &propertyName, KJS::ValueImp *value, int attr)
		{
/*
#if DEBUG_BRIDGE > 0
			kdDebug(26004) << "DOMRWBridge::put(), " << propertyName.qstring()
						   << " Name: " << classInfo()->className
						   << " Wrapper object: " << m_wrapper << endl;
#endif

			// Try to see if we know this property (and need to take special action)
			if(m_wrapper->put(exec, propertyName, value, attr))
				return;
*/
			// We don't -> set property in ObjectImp.
			KJS::ObjectImp::put(exec, propertyName, value, attr);
		}
	};
};

#endif

// vim:ts=4:noet
