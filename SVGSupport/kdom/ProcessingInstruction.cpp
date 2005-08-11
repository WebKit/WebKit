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

#include "Ecma.h"
#include "DOMException.h"
#include "DOMExceptionImpl.h"
#include "ProcessingInstruction.h"

#include "DOMConstants.h"
#include "ProcessingInstruction.lut.h"
#include "ProcessingInstructionImpl.h"
using namespace KDOM;
using namespace KJS;

/*
@begin ProcessingInstruction::s_hashTable 3
 target	ProcessingInstructionConstants::Target	DontDelete|ReadOnly
 data	ProcessingInstructionConstants::Data	DontDelete
@end
*/

ValueImp *ProcessingInstruction::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case ProcessingInstructionConstants::Target:
			return getDOMString(target());
		case ProcessingInstructionConstants::Data:
			return getDOMString(data());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

void ProcessingInstruction::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case ProcessingInstructionConstants::Data:
		{
			setData(toDOMString(exec, value));
			break;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
}

// The qdom way...
#define impl (static_cast<ProcessingInstructionImpl *>(d))

ProcessingInstruction ProcessingInstruction::null;

ProcessingInstruction::ProcessingInstruction() : Node()
{
}

ProcessingInstruction::ProcessingInstruction(ProcessingInstructionImpl *i) : Node(i)
{
}

ProcessingInstruction::ProcessingInstruction(const ProcessingInstruction &other) : Node()
{
	(*this) = other;
}

ProcessingInstruction::ProcessingInstruction(const Node &other) : Node()
{
	(*this) = other;
}

ProcessingInstruction::~ProcessingInstruction()
{
}

ProcessingInstruction &ProcessingInstruction::operator=(const ProcessingInstruction &other)
{
	Node::operator=(other);
	return *this;
}

KDOM_NODE_DERIVED_ASSIGN_OP(ProcessingInstruction, PROCESSING_INSTRUCTION_NODE)

DOMString ProcessingInstruction::target() const
{
	if(!d)
		return DOMString();

	return impl->target();
}

DOMString ProcessingInstruction::data() const
{
	if(!d)
		return DOMString();

	return impl->data();
}

void ProcessingInstruction::setData(const DOMString &data)
{
	if(d)
		impl->setData(data);
}

// vim:ts=4:noet
