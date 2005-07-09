/*
 * This file is part of the KDE libraries
 *
 * Copyright (C) 2005 Frans Englich 	<frans.englich@telia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "DOMString.h"
#include "Document.h"
#include "NodeImpl.h"
#include <kdom/Shared.h>

#include "DocumentImpl.h"

#include "XPointerExpression.h"
#include "XPointerEvaluator.h"
#include "XPointerResult.h"

#include "XPointerEvaluatorImpl.h"

using namespace KDOM;
using namespace KDOM::XPointer;

XPointerEvaluator XPointerEvaluator::null;

XPointerEvaluator::XPointerEvaluator() : d(0)
{
}

XPointerEvaluator::XPointerEvaluator(XPointerEvaluatorImpl *i) : d(i)
{
}

XPointerEvaluator::XPointerEvaluator(const XPointerEvaluator &other): d(0)
{
	(*this) = other;
}

XPointerEvaluator::~XPointerEvaluator()
{
}

XPointerEvaluator &XPointerEvaluator::operator=(const XPointerEvaluator &other)
{
	if(d != other.d)
		d = other.d;

	return *this;
}

XPointerEvaluator &XPointerEvaluator::operator=(XPointerEvaluatorImpl *other)
{
	if(d != other)
		d = other;

	return *this;
}

XPointerExpression XPointerEvaluator::createXPointer(const DOMString &string,
													 const Node &relatedNode)
{
	if(!d)
		return XPointerExpression::null;

	return XPointerExpression(d->createXPointer(string, static_cast<NodeImpl *>(relatedNode.handle())));
}

XPointerResult XPointerEvaluator::evaluateXPointer(const DOMString &string, const Node &related) const
{
	if(!d)
		return XPointerResult::null;

	return XPointerResult(d->evaluateXPointer(string, static_cast<NodeImpl *>(related.handle())));
}

// vim:ts=4:noet
