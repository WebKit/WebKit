/*
 * expression.cc - Copyright 2005 Frerich Raabe <raabe@kde.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "expression.h"
#include "NodeImpl.h"

#ifndef __USE_ISOC99
#  undef __USE_ISOC99
#endif
#define __USE_ISOC99 1

#include <cmath>

using namespace KDOM;
using namespace std;

Value::Value()
{
}

Value::Value( NodeImpl *value )
	: m_type( Nodeset )
{
	m_nodeset.append( value );
}

Value::Value( const DomNodeList &value )
	: m_type( Nodeset ),
	m_nodeset( value )
{
}

Value::Value( bool value )
	: m_type( Boolean ),
	m_bool( value )
{
}

Value::Value( double value )
	: m_type( Number ),
	m_number( value )
{
}

Value::Value( const DomString &value )
	: m_type( String ),
	m_string( value )
{
}

Value::Type Value::type() const
{
	return m_type;
}

bool Value::isNodeset() const
{
	return m_type == Nodeset;
}

bool Value::isBoolean() const
{
	return m_type == Boolean;
}

bool Value::isNumber() const
{
	return m_type == Number;
}

bool Value::isString() const
{
	return m_type == String;
}

DomNodeList &Value::toNodeset()
{
	if ( m_type != Nodeset ) {
		qWarning( "Cannot convert anything to a nodeset." );
	}
	return m_nodeset;
}

const DomNodeList &Value::toNodeset() const
{
	if ( m_type != Nodeset ) {
		qWarning( "Cannot convert anything to a nodeset." );
	}
	return m_nodeset;
}

bool Value::toBoolean() const
{
	switch ( m_type ) {
		case Nodeset:
			return !m_nodeset.isEmpty();
		case Boolean:
			return m_bool;
		case Number:
			return m_number != 0;
		case String:
			return !m_string.isEmpty();
	}
	return bool();
}

double Value::toNumber() const
{
	switch ( m_type ) {
		case Nodeset:
			return Value( toString() ).toNumber();
		case Number:
			return m_number;
		case String: {
			bool canConvert;
			QString s = m_string.stripWhiteSpace();
			double value = s.toDouble( &canConvert );
			if ( canConvert ) {
				return value;
			} else {
				return NAN;
			}
		}
		case Boolean:
			return m_bool ? 1 : 0;
	}
	return double();
}

DomString Value::toString() const
{
	switch ( m_type ) {
		case Nodeset:
			if ( m_nodeset.isEmpty() ) {
				return QString::fromLatin1( "" );
			}
			return stringValue( m_nodeset.first() );
		case String:
			return m_string;
		case Number:
			if ( isnan( m_number ) ) {
				return QString::fromLatin1( "NaN" );
			} else if ( m_number == 0 ) {
				return QString::fromLatin1( "0" );
			} else if ( isinf( m_number ) ) {
				if ( signbit( m_number ) == 0 ) {
					return QString::fromLatin1( "Infinity" );
				} else {
					return QString::fromLatin1( "-Infinity" );
				}
			}
			return QString::number( m_number );
		case Boolean:
			return m_bool ? QString::fromLatin1( "true" )
			              : QString::fromLatin1( "false" );
	}
	return DomString();
}

QString Value::dump() const
{
	QString s = "<value type=\"";
	switch ( m_type ) {
		case Nodeset:
			s += "nodeset";
			break;
		case String:
			s += "string";
			break;
		case Number:
			s += "number";
			break;
		case Boolean:
			s += "boolean";
			break;
	};
	s += "\">" + toString() + "</value>";
	return s;
}

EvaluationContext Expression::s_evaluationContext;

EvaluationContext &Expression::evaluationContext()
{
	return s_evaluationContext;
}

Expression::Expression()
	: m_constantValue( 0 )
{
}

Expression::~Expression()
{
	QValueList<Expression *>::Iterator it, end = m_subExpressions.end();
	for ( it = m_subExpressions.begin(); it != end; ++it ) {
		delete *it;
	}
	delete m_constantValue;
}

Value Expression::evaluate() const
{
	if ( m_constantValue ) {
		return *m_constantValue;
	}
	return doEvaluate();
}

void Expression::addSubExpression( Expression *expr )
{
	m_subExpressions.append( expr );
}

void Expression::optimize()
{
	bool allSubExpressionsConstant = true;
	QValueList<Expression *>::Iterator it, end = m_subExpressions.end();
	for ( it = m_subExpressions.begin(); it != end; ++it ) {
		Expression *expr = *it;
		if ( expr->isConstant() ) {
			expr->optimize();
		} else {
			allSubExpressionsConstant = false;
		}
	}

	if ( allSubExpressionsConstant ) {
		m_constantValue = new Value( doEvaluate() );

		QValueList<Expression *>::Iterator it, end = m_subExpressions.end();
		for ( it = m_subExpressions.begin(); it != end; ++it ) {
			delete *it;
		}
		m_subExpressions.clear();
	}
}

unsigned int Expression::subExprCount() const
{
	return m_subExpressions.count();
}

Expression *Expression::subExpr( unsigned int i )
{
	Q_ASSERT( i < m_subExpressions.count() );
	return m_subExpressions[ i ];
}

const Expression *Expression::subExpr( unsigned int i ) const
{
	Q_ASSERT( i < m_subExpressions.count() );
	return m_subExpressions[ i ];
}

bool Expression::isConstant() const
{
	QValueList<Expression *>::ConstIterator it, end = m_subExpressions.end();
	for ( it = m_subExpressions.begin(); it != end; ++it ) {
		if ( !( *it )->isConstant() ) {
			return false;
		}
	}
	return true;
}

