/*
 * Copyright 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
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

%{

#include "config.h"

#if XPATH_SUPPORT

#include "XPathFunctions.h"
#include "XPathNSResolver.h"
#include "XPathParser.h"
#include "XPathPath.h"
#include "XPathPredicate.h"
#include "XPathVariableReference.h"

#define YYDEBUG 0
#define YYPARSE_PARAM parserParameter
#define PARSER static_cast<Parser*>(parserParameter)

using namespace WebCore;
using namespace XPath;

%}

%pure_parser

%union
{
    Step::Axis axis;
    NumericOp::Opcode numop;
    EqTestOp::Opcode eqop;
    String* str;
    Expression* expr;
    Vector<Predicate*>* predList;
    Vector<Expression*>* argList;
    Step* step;
    LocationPath* locationPath;
}

%{

int xpathyylex(YYSTYPE *yylval) { return Parser::current()->lex(yylval); }
void xpathyyerror(const char *str) { }
    
%}

%left <numop> MULOP RELOP
%left <eqop> EQOP
%left PLUS MINUS
%left OR AND
%token <axis> AXISNAME
%token <str> NODETYPE PI FUNCTIONNAME LITERAL
%token <str> VARIABLEREFERENCE NUMBER
%token DOTDOT SLASHSLASH
%token <str> NAMETEST
%token ERROR

%type <locationPath> LocationPath
%type <locationPath> AbsoluteLocationPath
%type <locationPath> RelativeLocationPath
%type <step> Step
%type <axis> AxisSpecifier
%type <step> DescendantOrSelf
%type <str> NodeTest
%type <expr> Predicate
%type <predList> PredicateList
%type <step> AbbreviatedStep
%type <expr> Expr
%type <expr> PrimaryExpr
%type <expr> FunctionCall
%type <argList> ArgumentList
%type <expr> Argument
%type <expr> UnionExpr
%type <expr> PathExpr
%type <expr> FilterExpr
%type <expr> OrExpr
%type <expr> AndExpr
%type <expr> EqualityExpr
%type <expr> RelationalExpr
%type <expr> AdditiveExpr
%type <expr> MultiplicativeExpr
%type <expr> UnaryExpr

%%

Expr:
    OrExpr
    {
        PARSER->m_topExpr = $1;
    }
    ;

LocationPath:
    RelativeLocationPath
    {
        $$->m_absolute = false;
    }
    |
    AbsoluteLocationPath
    {
        $$->m_absolute = true;
    }
    ;

AbsoluteLocationPath:
    '/'
    {
        $$ = new LocationPath;
        PARSER->registerParseNode($$);
    }
    |
    '/' RelativeLocationPath
    {
        $$ = $2;
    }
    |
    DescendantOrSelf RelativeLocationPath
    {
        $$ = $2;
        $$->m_steps.insert(0, $1);
        PARSER->unregisterParseNode($1);
    }
    ;

RelativeLocationPath:
    Step
    {
        $$ = new LocationPath;
        $$->m_steps.append($1);
        PARSER->unregisterParseNode($1);
        PARSER->registerParseNode($$);
    }
    |
    RelativeLocationPath '/' Step
    {
        $$->m_steps.append($3);
        PARSER->unregisterParseNode($3);
    }
    |
    RelativeLocationPath DescendantOrSelf Step
    {
        $$->m_steps.append($2);
        $$->m_steps.append($3);
        PARSER->unregisterParseNode($2);
        PARSER->unregisterParseNode($3);
    }
    ;

Step:
    NodeTest
    {
        $$ = new Step(Step::ChildAxis, *$1);
        PARSER->deleteString($1);
        PARSER->registerParseNode($$);
    }
    |
    NodeTest PredicateList
    {
        $$ = new Step(Step::ChildAxis, *$1, *$2);
        PARSER->deleteString($1);
        PARSER->deletePredicateVector($2);
        PARSER->registerParseNode($$);
    }
    |
    AxisSpecifier NodeTest
    {
        $$ = new Step($1, *$2);
        PARSER->deleteString($2);
        PARSER->registerParseNode($$);
    }
    |
    AxisSpecifier NodeTest PredicateList
    {
        $$ = new Step($1, *$2, *$3);
        PARSER->deleteString($2);
        PARSER->deletePredicateVector($3);
        PARSER->registerParseNode($$);
    }
    |
    AbbreviatedStep
    ;

AxisSpecifier:
    AXISNAME
    |
    '@'
    {
        $$ = Step::AttributeAxis;
    }
    ;

NodeTest:
    NAMETEST
    {
        int colon = $$->find(':');
        if (colon >= 0) {
            XPathNSResolver* resolver = PARSER->resolver();
            if (!resolver) {
                PARSER->m_gotNamespaceError = true;
                YYABORT;
            }
            PARSER->m_currentNamespaceURI = resolver->lookupNamespaceURI($$->left(colon));
            if (PARSER->m_currentNamespaceURI.isNull()) {
                PARSER->m_gotNamespaceError = true;
                YYABORT;
            }
            $$ = new String($1->substring(colon + 1));
            PARSER->deleteString($1);
            PARSER->registerString($$);
        }
    }
    |
    NODETYPE '(' ')'
    {
        $$ = new String(*$1 + "()");
        PARSER->deleteString($1);
        PARSER->registerString($$);
    }
    |
    PI '(' ')'
    |
    PI '(' LITERAL ')'
    {
        String s = *$1 + " " + *$3;
        $$ = new String(s.deprecatedString().stripWhiteSpace());
        PARSER->deleteString($1);        
        PARSER->deleteString($3);
        PARSER->registerString($$);
    }
    ;

PredicateList:
    Predicate
    {
        $$ = new Vector<Predicate*>;
        $$->append(new Predicate($1));
        PARSER->unregisterParseNode($1);
        PARSER->registerPredicateVector($$);
    }
    |
    PredicateList Predicate
    {
        $$->append(new Predicate($2));
        PARSER->unregisterParseNode($2);
    }
    ;

Predicate:
    '[' Expr ']'
    {
        $$ = $2;
    }
    ;

DescendantOrSelf:
    SLASHSLASH
    {
        $$ = new Step(Step::DescendantOrSelfAxis, "node()");
        PARSER->registerParseNode($$);
    }
    ;

AbbreviatedStep:
    '.'
    {
        $$ = new Step(Step::SelfAxis, "node()");
        PARSER->registerParseNode($$);
    }
    |
    DOTDOT
    {
        $$ = new Step(Step::ParentAxis, "node()");
        PARSER->registerParseNode($$);
    }
    ;

PrimaryExpr:
    VARIABLEREFERENCE
    {
        $$ = new VariableReference(*$1);
        PARSER->deleteString($1);
        PARSER->registerParseNode($$);
    }
    |
    '(' Expr ')'
    {
        $$ = $2;
    }
    |
    LITERAL
    {
        $$ = new StringExpression(*$1);
        PARSER->deleteString($1);        
        PARSER->registerParseNode($$);
    }
    |
    NUMBER
    {
        $$ = new Number($1->deprecatedString().toDouble());
        PARSER->deleteString($1);
        PARSER->registerParseNode($$);
    }
    |
    FunctionCall
    ;

FunctionCall:
    FUNCTIONNAME '(' ')'
    {
        $$ = createFunction(*$1);
        PARSER->deleteString($1);
        PARSER->registerParseNode($$);
    }
    |
    FUNCTIONNAME '(' ArgumentList ')'
    {
        $$ = createFunction(*$1, *$3);
        PARSER->deleteString($1);
        PARSER->deleteExpressionVector($3);
        PARSER->registerParseNode($$);
    }
    ;

ArgumentList:
    Argument
    {
        $$ = new Vector<Expression*>;
        $$->append($1);
        PARSER->unregisterParseNode($1);
        PARSER->registerExpressionVector($$);
    }
    |
    ArgumentList ',' Argument
    {
        $$->append($3);
        PARSER->unregisterParseNode($3);
    }
    ;

Argument:
    Expr
    ;

UnionExpr:
    PathExpr
    |
    UnionExpr '|' PathExpr
    {
        $$ = new Union;
        $$->addSubExpression($1);
        $$->addSubExpression($3);
        PARSER->unregisterParseNode($1);
        PARSER->unregisterParseNode($3);
        PARSER->registerParseNode($$);
    }
    ;

PathExpr:
    LocationPath
    {
        $$ = $1;
    }
    |
    FilterExpr
    |
    FilterExpr '/' RelativeLocationPath
    {
        $3->m_absolute = true;
        $$ = new Path(static_cast<Filter*>($1), $3);
        PARSER->unregisterParseNode($1);
        PARSER->unregisterParseNode($3);
        PARSER->registerParseNode($$);
    }
    |
    FilterExpr DescendantOrSelf RelativeLocationPath
    {
        $3->m_steps.insert(0, $2);
        $3->m_absolute = true;
        $$ = new Path(static_cast<Filter*>($1), $3);
        PARSER->unregisterParseNode($1);
        PARSER->unregisterParseNode($2);
        PARSER->unregisterParseNode($3);
        PARSER->registerParseNode($$);
    }
    ;

FilterExpr:
    PrimaryExpr
    |
    PrimaryExpr PredicateList
    {
        $$ = new Filter($1, *$2);
        PARSER->unregisterParseNode($1);
        PARSER->deletePredicateVector($2);
        PARSER->registerParseNode($$);
    }
    ;

OrExpr:
    AndExpr
    |
    OrExpr OR AndExpr
    {
        $$ = new LogicalOp(LogicalOp::OP_Or, $1, $3);
        PARSER->unregisterParseNode($1);
        PARSER->unregisterParseNode($3);
        PARSER->registerParseNode($$);
    }
    ;

AndExpr:
    EqualityExpr
    |
    AndExpr AND EqualityExpr
    {
        $$ = new LogicalOp(LogicalOp::OP_And, $1, $3);
        PARSER->unregisterParseNode($1);
        PARSER->unregisterParseNode($3);
        PARSER->registerParseNode($$);
    }
    ;

EqualityExpr:
    RelationalExpr
    |
    EqualityExpr EQOP RelationalExpr
    {
        $$ = new EqTestOp($2, $1, $3);
        PARSER->unregisterParseNode($1);
        PARSER->unregisterParseNode($3);
        PARSER->registerParseNode($$);
    }
    ;

RelationalExpr:
    AdditiveExpr
    |
    RelationalExpr RELOP AdditiveExpr
    {
        $$ = new NumericOp($2, $1, $3);
        PARSER->unregisterParseNode($1);
        PARSER->unregisterParseNode($3);
        PARSER->registerParseNode($$);
    }
    ;

AdditiveExpr:
    MultiplicativeExpr
    |
    AdditiveExpr PLUS MultiplicativeExpr
    {
        $$ = new NumericOp(NumericOp::OP_Add, $1, $3);
        PARSER->unregisterParseNode($1);
        PARSER->unregisterParseNode($3);
        PARSER->registerParseNode($$);
    }
    |
    AdditiveExpr MINUS MultiplicativeExpr
    {
        $$ = new NumericOp(NumericOp::OP_Sub, $1, $3);
        PARSER->unregisterParseNode($1);
        PARSER->unregisterParseNode($3);
        PARSER->registerParseNode($$);
    }
    ;

MultiplicativeExpr:
    UnaryExpr
    |
    MultiplicativeExpr MULOP UnaryExpr
    {
        $$ = new NumericOp($2, $1, $3);
        PARSER->unregisterParseNode($1);
        PARSER->unregisterParseNode($3);
        PARSER->registerParseNode($$);
    }
    ;

UnaryExpr:
    UnionExpr
    |
    MINUS UnaryExpr
    {
        $$ = new Negative;
        $$->addSubExpression($2);
        PARSER->unregisterParseNode($2);
        PARSER->registerParseNode($$);
    }
    ;

%%

#endif
