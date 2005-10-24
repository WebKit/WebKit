/*
    Copyright(C) KHTML Team
    Copyright(C) 2005 Frans Englich <frans.englich@telia.com>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or(at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KDOM_SharedPtr_H
#define KDOM_SharedPtr_H

#include <kdebug.h>
#include <QtGlobal>

namespace KDOM
{
	/**
	 * A smart pointer for handling pointers to Shared() classes.
	 *
	 * @see Shared
	 * @author the KHTML team
	 * @author Frans Englich <frans.englich@telia.com>
	 */
	template<class T> class SharedPtr
	{
	public:
		SharedPtr() : m_ptr(0) {}
		explicit SharedPtr(T *ptr) : m_ptr(ptr)
		{
			if(m_ptr)
				m_ptr->ref();
		}

		SharedPtr(const SharedPtr &o) : m_ptr(o.m_ptr)
		{
			if(m_ptr)
				m_ptr->ref();
		}

		template<typename X>
		SharedPtr(const SharedPtr<X> &o) : m_ptr(static_cast<T *>(o.get()))
		{
			if(m_ptr)
				m_ptr->ref();
		}

		~SharedPtr()
		{
			if(m_ptr)
				m_ptr->deref();
		}

		bool isNull() const { return m_ptr == 0; }
		bool notNull() const { return m_ptr != 0; }

		void reset()
		{
			if(m_ptr)
				m_ptr->deref();
			m_ptr = 0;
		}

		void reset(T *o)
		{
			if(o)
				o->ref();
			if(m_ptr)
				m_ptr->deref();
			m_ptr = o;
		}

		T *get() const
		{
			return m_ptr;
		}

		T &operator*() const;

		T *operator->() const;

		bool operator!() const
		{
			return m_ptr == 0;
		}

		operator bool() const
		{
			return m_ptr != 0;
		}

		inline friend bool operator==(const SharedPtr &a, const SharedPtr &b)
		{
			return a.m_ptr == b.m_ptr;
		}

		inline friend bool operator==(const SharedPtr &a, const T *b)
		{
			return a.m_ptr == b;
		}

		inline friend bool operator==(const T *a, const SharedPtr &b)
		{
			return a == b.m_ptr;
		}

		SharedPtr &operator=(const SharedPtr &);

	private:
		T* m_ptr;
	};

	template<typename T>
	SharedPtr<T> &SharedPtr<T>::operator=(const SharedPtr<T> &o)
	{
		if(o.m_ptr)
			o.m_ptr->ref();

		if(m_ptr)
			m_ptr->deref();

		m_ptr = o.m_ptr;

		return *this;
	}

	template<typename T>
	T &SharedPtr<T>::operator*() const
	{
#ifndef NDEBUG
		Q_CHECK_PTR(m_ptr);
		if(!m_ptr)
			kdDebug(26560) << kdBacktrace() << endl;
#endif
		return *m_ptr;
	}

	template<typename T>
	T *SharedPtr<T>::operator->() const
	{
#ifndef NDEBUG
		Q_CHECK_PTR(m_ptr);
		if(!m_ptr)
			kdDebug(26560) << kdBacktrace() << endl;
#endif
		return m_ptr;
	}
};

#endif
// vim:ts=4:noet
