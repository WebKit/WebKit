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

#ifndef KDOM_DOMConfigurationImpl_H
#define KDOM_DOMConfigurationImpl_H

#include <kdom/Shared.h>
#include <kdom/DOMError.h>
#include <kdom/DOMErrorHandler.h>

namespace KDOM
{
	class DOMUserDataImpl;
	class DOMStringListImpl;
	class DOMStringImpl;
	// Introduced in DOM Level 3:
	class DOMConfigurationImpl : public Shared
	{
	public:
		DOMConfigurationImpl();
		virtual ~DOMConfigurationImpl();

		void setParameter(DOMStringImpl *name, DOMUserDataImpl *value);
		void setParameter(DOMStringImpl *name, bool value);
		DOMUserDataImpl *getParameter(DOMStringImpl *name) const;
		bool canSetParameter(DOMStringImpl *name, DOMUserDataImpl *value) const;
		DOMStringListImpl *parameterNames() const;

		// Internal
		DOMErrorHandler errHandler();
		bool getParameter(int flag) const;
		void setParameter(int flag, bool b);

		DOMStringImpl *normalizeCharacters(DOMStringImpl *data) const;

	private:
		DOMErrorHandler m_errHandler;
		int m_flags;
		static DOMStringListImpl *m_paramNames;
	};
};

#endif

// vim:ts=4:noet
