/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
                  2004 Rob Buis <buis@kde.org>

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

#include <kdom/Shared.h>
#include <kdom/ecma/DOMLookup.h>

namespace KJS
{
	class Value;
	class UString;
	class ExecState;
};

namespace KDOM
{
	// Base class for all bridges
	// The T class must provide prototype(exec), get and hasProperty
	// and have a static s_classInfo object
	template<class T>
	class DOMBridge : public KJS::ObjectImp
	{
	public:
		// Example: T=Element, it's impl class is called ElementImpl, and a synonym for it is Element::Private
		DOMBridge(KJS::ExecState *exec, typename T::Private *impl) : KJS::ObjectImp(T(impl).prototype(exec)), m_impl(impl)
		{
			Shared *sharedImpl = dynamic_cast<Shared *>(m_impl);
			if(sharedImpl)
				sharedImpl->ref();
		}

		~DOMBridge()
		{
			Shared *sharedImpl = dynamic_cast<Shared *>(m_impl);
			if(sharedImpl)
				sharedImpl->deref();
		}

		typename T::Private *impl() const { return m_impl; }

		virtual KJS::Value get(KJS::ExecState *exec, const KJS::Identifier &propertyName) const
		{
			kdDebug(26004) << "DOMBridge::get(), " << propertyName.qstring() << " Name: " << classInfo()->className << " Object: " << this->m_impl << endl;

			// Look for standard properties (e.g. those in the hashtables)
			T obj(m_impl);
			KJS::Value val = obj.get(exec, propertyName, this);

			if(val.type() != KJS::UndefinedType)
				return val;

			// Not found -> forward to ObjectImp.
			val = KJS::ObjectImp::get(exec, propertyName);
			if(val.type() == KJS::UndefinedType)
				kdDebug(26004) << "WARNING: " << propertyName.qstring() << " not found in... Name: " << classInfo()->className << " Object: " << m_impl << " on line : " << exec->context().curStmtFirstLine() << endl;

			return val;
		}

		virtual bool hasProperty(KJS::ExecState *exec, const KJS::Identifier &propertyName) const
		{
			kdDebug(26004) << "DOMBridge::hasProperty(), " << propertyName.qstring() << " Name: " << classInfo()->className << " Object: " << m_impl << endl;

			T obj(m_impl);
			if(obj.hasProperty(exec, propertyName))
				return true;

			return KJS::ObjectImp::hasProperty(exec, propertyName);
		}

		virtual const KJS::ClassInfo *classInfo() const { return &T::s_classInfo; }

	protected:
		typename T::Private *m_impl;
	};

	// Base class for readwrite bridges
	// T must also implement put (use KDOM_PUT in the header file)
	template<class T>
	class DOMRWBridge : public DOMBridge<T>
	{
	public:
		DOMRWBridge(KJS::ExecState *exec, typename T::Private *impl) : DOMBridge<T>(exec, impl) { }

		virtual void put(KJS::ExecState *exec, const KJS::Identifier &propertyName, const KJS::Value &value, int attr)
		{
//			if(!(attr & KJS::Internal))
			kdDebug(26004) << "DOMRWBridge::put(), " << propertyName.qstring() << " Name: " << this->classInfo()->className << " Object: " << this->m_impl << endl;

			// Try to see if we know this property (and need to take special action)
			T obj(this->m_impl);
			if(obj.put(exec, propertyName, value, attr))
				return;

			// We don't -> set property in ObjectImp.
			KJS::ObjectImp::put(exec, propertyName, value, attr);
		}
	};

	// Base class for "constructor" bridges. Only used by cacheGlobalBridge.
	template<class T>
	class DOMBridgeCtor : public KJS::ObjectImp
	{
	public:
		DOMBridgeCtor(KJS::ExecState *exec, T *impl) : KJS::ObjectImp(impl->prototype(exec)), m_impl(impl) { }

		T *impl() const { return m_impl; }

		virtual KJS::Value get(KJS::ExecState *exec, const KJS::Identifier &propertyName) const
		{
			kdDebug(26004) << "DOMBridgeCtor::get(), " << propertyName.qstring() << " Name: " << classInfo()->className << " Object: " << this->m_impl << endl;

			// Look for standard properties (e.g. those in the hashtables)
			KJS::Value val = m_impl->get(exec, propertyName, this);

			if(val.type() != KJS::UndefinedType)
				return val;

			// Not found -> forward to ObjectImp.
			val = KJS::ObjectImp::get(exec, propertyName);
			if(val.type() == KJS::UndefinedType)
				kdDebug(26004) << "WARNING: " << propertyName.qstring() << " not found in... Name: " << classInfo()->className << " Object: " << m_impl << " on line : " << exec->context().curStmtFirstLine() << endl;

			return val;
		}

		virtual bool hasProperty(KJS::ExecState *exec, const KJS::Identifier &propertyName) const
		{
			kdDebug(26004) << "DOMBridgeCtor::hasProperty(), " << propertyName.qstring() << " Name: " << classInfo()->className << " Object: " << m_impl << endl;

			if(m_impl->hasProperty(exec, propertyName))
				return true;

			return KJS::ObjectImp::hasProperty(exec, propertyName);
		}

		virtual const KJS::ClassInfo *classInfo() const { return &T::s_classInfo; }

	protected:
		T *m_impl;
	};
};

#endif

// vim:ts=4:noet
