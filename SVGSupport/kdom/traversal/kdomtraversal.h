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

#ifndef KDOMTRAVERSAL_H
#define KDOMTRAVERSAL_H

// General namespace specific definitions
namespace KDOM
{
	// Constants returned by acceptNode
	enum AcceptCode
	{
		FILTER_ACCEPT                  = 1,
		FILTER_REJECT                  = 2,
		FILTER_SKIP                    = 3
	};

	// Constants for whatToShow
	enum ShowNode
	{
		SHOW_ALL                       = 0xFFFFFFFF,
		SHOW_ELEMENT                   = 0x00000001,
		SHOW_ATTRIBUTE                 = 0x00000002,
		SHOW_TEXT                      = 0x00000004,
		SHOW_CDATA_SECTION             = 0x00000008,
		SHOW_ENTITY_REFERENCE          = 0x00000010,
		SHOW_ENTITY                    = 0x00000020,
		SHOW_PROCESSING_INSTRUCTION    = 0x00000040,
		SHOW_COMMENT                   = 0x00000080,
		SHOW_DOCUMENT                  = 0x00000100,
		SHOW_DOCUMENT_TYPE             = 0x00000200,
		SHOW_DOCUMENT_FRAGMENT         = 0x00000400,
		SHOW_NOTATION                  = 0x00000800
	};
};

#endif

// vim:ts=4:noet
