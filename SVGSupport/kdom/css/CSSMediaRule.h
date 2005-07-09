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

#ifndef KDOM_CSSMediaRule_H
#define KDOM_CSSMediaRule_H

#include <kdom/css/CSSRule.h>
#include <kdom/css/CSSRuleList.h>

namespace KDOM
{
	class MediaList;
	class CSSMediaRuleImpl;
	class CSSMediaRule : public CSSRule 
	{
	public:
		CSSMediaRule();
		explicit CSSMediaRule(CSSMediaRuleImpl *i);
		CSSMediaRule(const CSSMediaRule &other);
		CSSMediaRule(const CSSRule &other);
		~CSSMediaRule();

		// Operators
		CSSMediaRule &operator=(const CSSMediaRule &other);
		CSSMediaRule &operator=(const CSSRule &other);

		// 'CSSMediaRule' functions
		MediaList media() const;
		CSSRuleList cssRules() const;
		unsigned long insertRule(const DOMString &rule, unsigned long index);
		void deleteRule(unsigned long index);

		// Internal
		KDOM_INTERNAL(CSSMediaRule)

	public: // EcmaScript section
		KDOM_GET
		KDOM_PUT

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KDOM_DEFINE_PROTOTYPE(CSSMediaRuleProto)
KDOM_IMPLEMENT_PROTOFUNC(CSSMediaRuleProtoFunc, CSSMediaRule)

#endif

// vim:ts=4:noet
