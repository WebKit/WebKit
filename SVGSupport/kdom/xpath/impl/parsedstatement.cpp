/*
 * parsedstatement.cc - Copyright 2005 Frerich Raabe <raabe@kde.org>
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
#include "parsedstatement.h"
#include "path.h"

#include "NodeImpl.h"

using namespace KDOM;

extern Path *parseStatement( const DomString &statement );

ParsedStatement::ParsedStatement()
	: m_path( 0 )
{
}

ParsedStatement::ParsedStatement( const DomString &statement )
	: m_path( 0 )
{
	parse( statement );
}

ParsedStatement::~ParsedStatement()
{
	delete m_path;
}

void ParsedStatement::parse( const DomString &statement )
{
	delete m_path;
	m_path = parseStatement( statement );
}

void ParsedStatement::optimize()
{
	if ( !m_path ) {
		return;
	}
	m_path->optimize();
}

Value ParsedStatement::evaluate( NodeImpl *context ) const
{
	Expression::evaluationContext().node = context;
	return m_path->evaluate();
}

QString ParsedStatement::dump() const
{
	return m_path->dump();
}

