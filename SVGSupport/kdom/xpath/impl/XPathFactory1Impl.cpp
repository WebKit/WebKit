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

#include <kdebug.h>

#include "DOMString.h"

#include "boolean_fnxp1.h"
#include "ceiling_fnxp1.h"
#include "concat_fnxp1.h"
#include "contains_fnxp1.h"
#include "false_fnxp1.h"
#include "floor_fnxp1.h"
#include "normalize-space_fnxp1.h"
#include "not_fnxp1.h"
#include "number_fnxp1.h"
#include "round_fnxp1.h"
#include "starts-with_fnxp1.h"
#include "string-length_fnxp1.h"
#include "string_fnxp1.h"
#include "substring-after_fnxp1.h"
#include "substring-before_fnxp1.h"
#include "substring_fnxp1.h"
#include "true_fnxp1.h"

#include "AxisImpl.h"
#include "LiteralImpl.h"
#include "OperatorImpl.h"
#include "StepImpl.h"
#include "VariableRefImpl.h"

#include "XPathFactory1Impl.h"

using namespace KDOM;
using namespace KDOM::XPath;

XPathFactory1Impl::XPathFactory1Impl()
{
}

XPathFactory1Impl::~XPathFactory1Impl()
{
}

OperatorImpl *XPathFactory1Impl::_createOperator(OperatorImpl::OperatorId id)
{
	return new OperatorImpl(id);
}

AxisImpl *XPathFactory1Impl::_createAxis(AxisImpl::AxisId id)
{
	return new AxisImpl(id);
}

StepImpl *XPathFactory1Impl::_createStep(StepImpl::StepId id)
{
	return new StepImpl(id);
}

LiteralImpl *XPathFactory1Impl::_createLiteral(ValueImpl *value)
{
	return new LiteralImpl(value);
}

FunctionCallImpl *XPathFactory1Impl::_createFunctionCall(const char *name)
{
	/* These can be ordered by usage statistics in order to optimize. */

	if(name == "starts-with")
		return new starts_withXP1FunctionCallImpl();
	else if(name == "string-length")
		return new string_lengthXP1FunctionCallImpl();
	else if(name == "string")
		return new stringXP1FunctionCallImpl();
	else if(name == "contains")
		return new containsXP1FunctionCallImpl();
	else if(name == "substring-after")
		return new substring_afterXP1FunctionCallImpl();
	else if(name == "substring-before")
		return new substring_beforeXP1FunctionCallImpl();
	else if(name == "substring")
		return new substringXP1FunctionCallImpl();
	else if(name == "boolean")
		return new booleanXP1FunctionCallImpl();
	else if(name == "ceiling")
		return new ceilingXP1FunctionCallImpl();
	else if(name == "concat")
		return new concatXP1FunctionCallImpl();
	else if(name == "false")
		return new falseXP1FunctionCallImpl();
	else if(name == "floor")
		return new floorXP1FunctionCallImpl();
	else if(name == "normalize-space")
		return new normalize_spaceXP1FunctionCallImpl();
	else if(name == "not")
		return new notXP1FunctionCallImpl();
	else if(name == "number")
		return new numberXP1FunctionCallImpl();
	else if(name == "round")
		return new roundXP1FunctionCallImpl();
	else if(name == "true")
		return new trueXP1FunctionCallImpl();

	kdDebug() << "createFunctionCall: Failed; " << name << endl;

	return 0;
}

VariableRefImpl *XPathFactory1Impl::_createVariableRef(const char *name)
{
	return new VariableRefImpl(DOMString(name));
}

// vim:ts=4:noet
