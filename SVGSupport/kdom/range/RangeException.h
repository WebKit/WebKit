/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Gunnstein Lye (gunnstein@netcom.no)
 * (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * (C) 2001 Peter Kelly (pmk@post.com)
 * (C) 2005 John Tapsell (john.tapsell@kdemail.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * This file includes excerpts from the Document Object Model (DOM)
 * Level 2 Specification (Candidate Recommendation)
 * http://www.w3.org/TR/2000/CR-DOM-Level-2-20000510/
 * Copyright © 2000 W3C® (MIT, INRIA, Keio), All Rights Reserved.
 *
 */
#ifndef KDOM_RangeException_H
#define KDOM_RangeException_H

#include <kdom/ecma/Ecma.h>

namespace KDOM
{
	class RangeExceptionImpl;

	// Introduced in DOM Level 2:
	class RangeException
	{
	public:
		RangeException();
		RangeException(unsigned short _code);
		RangeException(const RangeException &other);
		explicit RangeException(RangeExceptionImpl *i);
		virtual ~RangeException();

		// Operators
		RangeException &operator=(const RangeException &other);
		bool operator==(const RangeException &other) const;
		bool operator!=(const RangeException &other) const;

		// 'RangeException' functions
		unsigned short code() const;

		// Internal
		KDOM_INTERNAL_BASE(RangeException)

	protected:
		RangeExceptionImpl *d;

	public: // EcmaScript section
		KDOM_GET

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
	};
}

#endif

// vim:ts=4:noet
