/*
    Copyright(C) 2004 Richard Moore <rich@kde.org>
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

#ifndef KDOM_XPath_StepImpl_H
#define KDOM_XPath_StepImpl_H

#include <kdom/xpath/impl/ExprNodeImpl.h>

#include <qstring.h>

namespace KDOM
{
namespace XPath
{
	class ContextImpl;

	/**
 	 * A Location Step in the AST.
 	 */
	class StepImpl : public ExprNodeImpl
	{
	public:

    		StepImpl(unsigned short id);
    		~StepImpl();
		
		enum StepId
		{
			StepDot,
			StepDotDot,
			StepQName,
			StepStar,
			StepNameTest,
			StepProcessingInstruction,
			StepText,
			StepNode,
			StepComment
		};

    		virtual ValueImpl *evaluate(ContextImpl *context) const;
    		virtual QString dump() const;
		
    		void setName(const QString &test);
    		void setStar(const bool yes);
	
	private:
    		unsigned short m_id;
    		QString m_name;
    		bool m_star;
	};

};
};
#endif
