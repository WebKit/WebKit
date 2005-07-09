%{
#include <qstring.h>

namespace KDOM
{
	namespace XPath
	{
   		class ExprNodeImpl;
	};
}

using namespace KDOM;
using namespace KDOM::XPath;

#include "xpath.h"
#include "AxisImpl.h"

#define YY_STACK_USED 0
#define YY_ALWAYS_INTERACTIVE 0
#define YY_NEVER_INTERACTIVE 1
#define YY_MAIN 0
#define YY_NO_UNPUT 1

%}

%option 8bit
%option noyywrap

%%

[0-9]+|[0-9]*"."[0-9]+          { xpathyylval.dval = atof(xpathyytext); return NUMBER; }

"\""[^"]"\""			{
                                  xpathyylval.sval = &(xpathyytext[1]);
                                  xpathyytext[ xpathyyleng-1 ] = '\0';
                                  return LITERAL;
                                }
"'"[^"]"'"			{
                                  xpathyylval.sval = &(xpathyytext[1]);
                                  xpathyytext[ xpathyyleng-1 ] = '\0';
                                  return LITERAL;
                                }

or				{ return OR; }
and				{ return AND; }

"="				{ return EQUALS; }
"<"				{ return LT; }
">"				{ return GT; }
"<="				{ return LTE; }
">="				{ return GTE; }

"+"				{ return PLUS; }
"-"				{ return MINUS; }
"*"				{ return STAR; }

mod				{ return MOD; }
div				{ return DIV; }

"("				{ return LPAREN; }
")"				{ return RPAREN; }

"["				{ return LSQUARE; }
"]"				{ return RSQUARE; }

"|"                             { return BAR; }

"/"                             { return SLASH; }
"//"                            { return SLASHSLASH; }
"."                             { return DOT; }
".."                            { return DOTDOT; }
":"                             { return COLON; }
"::"                            { return COLONCOLON; }

ancestor                        { xpathyylval.ival = AxisImpl::AxisAncestor;          return AXIS; }
"ancestor-or-self"              { xpathyylval.ival = AxisImpl::AxisAncestorOrSelf;    return AXIS; }
attribute                       { xpathyylval.ival = AxisImpl::AxisAttribute;         return AXIS; }
child                           { xpathyylval.ival = AxisImpl::AxisChild;             return AXIS; }
descendant                      { xpathyylval.ival = AxisImpl::AxisDescendant;        return AXIS; }
"descendant-or-self"            { xpathyylval.ival = AxisImpl::AxisDescendantOrSelf;  return AXIS; }
following                       { xpathyylval.ival = AxisImpl::AxisFollowing;         return AXIS; }
"following-sibling"             { xpathyylval.ival = AxisImpl::AxisFollowingSibling;  return AXIS; }
namespace                       { xpathyylval.ival = AxisImpl::AxisNamespace;         return AXIS; }
parent                          { xpathyylval.ival = AxisImpl::AxisParent;            return AXIS; }
preceding                       { xpathyylval.ival = AxisImpl::AxisPreceding;         return AXIS; }
"preceding-sibling"             { xpathyylval.ival = AxisImpl::AxisPrecedingSibling;  return AXIS; }
self                            { xpathyylval.ival = AxisImpl::AxisSelf;              return AXIS; }

text                            { return TEXT; }
node                            { return NODE; }
"processing-instruction"        { return PROCESSING_INSTRUCTION; }
comment                         { return COMMENT; }

"$"[a-zA-Z-]+			{ xpathyylval.sval = &(xpathyytext[1]); return VARIABLE_REFERENCE; }
[a-zA-Z-]+:[a-zA-Z-]+           { xpathyylval.sval = xpathyytext; return QNAME; }
[a-zA-Z-]+			{ xpathyylval.sval = xpathyytext; return NAME; }

","				{ return COMMA; }

%%

// vim:ts=4:noet
