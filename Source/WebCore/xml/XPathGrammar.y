/*
 * Copyright 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
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

#include "XPathFunctions.h"
#include "XPathParser.h"
#include "XPathPath.h"
#include "XPathStep.h"
#include "XPathVariableReference.h"

#if COMPILER(MSVC)
// See https://msdn.microsoft.com/en-us/library/1wea5zwe.aspx
#pragma warning(disable: 4701)
#endif

#define YYMALLOC fastMalloc
#define YYFREE fastFree

#define YYENABLE_NLS 0
#define YYLTYPE_IS_TRIVIAL 1
#define YYDEBUG 0
#define YYMAXDEPTH 10000

using namespace WebCore;
using namespace XPath;

%}

%pure-parser
%lex-param { parser }
%parse-param { Parser& parser }

%union { 
    NumericOp::Opcode numericOpcode; 
    EqTestOp::Opcode equalityTestOpcode;
    StringImpl* string;
    Step::Axis axis;
    LocationPath* locationPath;
    Step::NodeTest* nodeTest;
    Vector<std::unique_ptr<Expression>>* expressionVector;
    Step* step;
    Expression* expression; 
}
%left <numericOpcode> MULOP

%left <equalityTestOpcode> EQOP RELOP

%left PLUS MINUS

%left OR AND

%token <string> FUNCTIONNAME LITERAL NAMETEST NUMBER NODETYPE VARIABLEREFERENCE
%destructor { if ($$) $$->deref(); } FUNCTIONNAME LITERAL NAMETEST NUMBER NODETYPE VARIABLEREFERENCE

%token <axis> AXISNAME
%type <axis> AxisSpecifier

%token COMMENT DOTDOT PI NODE SLASHSLASH TEXT_ XPATH_ERROR

%type <locationPath> LocationPath AbsoluteLocationPath RelativeLocationPath
%destructor { delete $$; } LocationPath AbsoluteLocationPath RelativeLocationPath

%type <nodeTest> NodeTest
%destructor { delete $$; } NodeTest

%type <expressionVector> ArgumentList PredicateList OptionalPredicateList
%destructor { delete $$; } ArgumentList PredicateList OptionalPredicateList

%type <step> Step AbbreviatedStep DescendantOrSelf
%destructor { delete $$; } Step AbbreviatedStep DescendantOrSelf

%type <expression> AdditiveExpr AndExpr Argument EqualityExpr Expr FilterExpr FunctionCall MultiplicativeExpr OrExpr PathExpr Predicate PrimaryExpr RelationalExpr UnaryExpr UnionExpr
%destructor { delete $$; } AdditiveExpr AndExpr Argument EqualityExpr Expr FilterExpr FunctionCall MultiplicativeExpr OrExpr PathExpr Predicate PrimaryExpr RelationalExpr UnaryExpr UnionExpr



%{

static int xpathyylex(YYSTYPE* yylval, Parser& parser) { return parser.lex(*yylval); }
static void xpathyyerror(Parser&, const char*) { }

%}

%%

Top:
    Expr
    {
        parser.setParseResult(std::unique_ptr<Expression>($1));
    }
    ;

Expr:
    OrExpr
    ;

LocationPath:
    AbsoluteLocationPath
    {
        $$ = $1;
        $$->setAbsolute();
    }
    |
    RelativeLocationPath
    ;

AbsoluteLocationPath:
    '/'
    {
        $$ = new LocationPath;
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
        $$->prependStep(std::unique_ptr<Step>($1));
    }
    ;

RelativeLocationPath:
    Step
    {
        $$ = new LocationPath;
        $$->appendStep(std::unique_ptr<Step>($1));
    }
    |
    RelativeLocationPath '/' Step
    {
        $$ = $1;
        $$->appendStep(std::unique_ptr<Step>($3));
    }
    |
    RelativeLocationPath DescendantOrSelf Step
    {
        $$ = $1;
        $$->appendStep(std::unique_ptr<Step>($2));
        $$->appendStep(std::unique_ptr<Step>($3));
    }
    ;

Step:
    NodeTest OptionalPredicateList
    {
        std::unique_ptr<Step::NodeTest> nodeTest($1);
        std::unique_ptr<Vector<std::unique_ptr<Expression>>> predicateList($2);
        if (predicateList)
            $$ = new Step(Step::ChildAxis, WTFMove(*nodeTest), WTFMove(*predicateList));
        else
            $$ = new Step(Step::ChildAxis, WTFMove(*nodeTest));
    }
    |
    NAMETEST OptionalPredicateList
    {
        String nametest = adoptRef($1);
        std::unique_ptr<Vector<std::unique_ptr<Expression>>> predicateList($2);

        String localName;
        String namespaceURI;
        if (!parser.expandQualifiedName(nametest, localName, namespaceURI)) {
            $$ = nullptr;
            YYABORT;
        }

        if (predicateList)
            $$ = new Step(Step::ChildAxis, Step::NodeTest(Step::NodeTest::NameTest, localName, namespaceURI), WTFMove(*predicateList));
        else
            $$ = new Step(Step::ChildAxis, Step::NodeTest(Step::NodeTest::NameTest, localName, namespaceURI));
    }
    |
    AxisSpecifier NodeTest OptionalPredicateList
    {
        std::unique_ptr<Step::NodeTest> nodeTest($2);
        std::unique_ptr<Vector<std::unique_ptr<Expression>>> predicateList($3);

        if (predicateList)
            $$ = new Step($1, WTFMove(*nodeTest), WTFMove(*predicateList));
        else
            $$ = new Step($1, WTFMove(*nodeTest));
    }
    |
    AxisSpecifier NAMETEST OptionalPredicateList
    {
        String nametest = adoptRef($2);
        std::unique_ptr<Vector<std::unique_ptr<Expression>>> predicateList($3);

        String localName;
        String namespaceURI;
        if (!parser.expandQualifiedName(nametest, localName, namespaceURI)) {
            $$ = nullptr;
            YYABORT;
        }

        if (predicateList)
            $$ = new Step($1, Step::NodeTest(Step::NodeTest::NameTest, localName, namespaceURI), WTFMove(*predicateList));
        else
            $$ = new Step($1, Step::NodeTest(Step::NodeTest::NameTest, localName, namespaceURI));
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
    NODE '(' ')'
    {
        $$ = new Step::NodeTest(Step::NodeTest::AnyNodeTest);
    }
    |
    TEXT_ '(' ')'
    {
        $$ = new Step::NodeTest(Step::NodeTest::TextNodeTest);
    }
    |
    COMMENT '(' ')'
    {
        $$ = new Step::NodeTest(Step::NodeTest::CommentNodeTest);
    }
    |
    PI '(' ')'
    {
        $$ = new Step::NodeTest(Step::NodeTest::ProcessingInstructionNodeTest);
    }
    |
    PI '(' LITERAL ')'
    {
        String literal = adoptRef($3);
        $$ = new Step::NodeTest(Step::NodeTest::ProcessingInstructionNodeTest, literal.stripWhiteSpace());
    }
    ;

OptionalPredicateList:
    /* empty */
    {
        $$ = nullptr;
    }
    |
    PredicateList
    ;

PredicateList:
    Predicate
    {
        $$ = new Vector<std::unique_ptr<Expression>>;
        $$->append(std::unique_ptr<Expression>($1));
    }
    |
    PredicateList Predicate
    {
        $$ = $1;
        $$->append(std::unique_ptr<Expression>($2));
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
        $$ = new Step(Step::DescendantOrSelfAxis, Step::NodeTest(Step::NodeTest::AnyNodeTest));
    }
    ;

AbbreviatedStep:
    '.'
    {
        $$ = new Step(Step::SelfAxis, Step::NodeTest(Step::NodeTest::AnyNodeTest));
    }
    |
    DOTDOT
    {
        $$ = new Step(Step::ParentAxis, Step::NodeTest(Step::NodeTest::AnyNodeTest));
    }
    ;

PrimaryExpr:
    VARIABLEREFERENCE
    {
        String name = adoptRef($1);
        $$ = new VariableReference(name);
    }
    |
    '(' Expr ')'
    {
        $$ = $2;
    }
    |
    LITERAL
    {
        String literal = adoptRef($1);
        $$ = new StringExpression(WTFMove(literal));
    }
    |
    NUMBER
    {
        String numeral = adoptRef($1);
        $$ = new Number(numeral.toDouble());
    }
    |
    FunctionCall
    ;

FunctionCall:
    FUNCTIONNAME '(' ')'
    {
        String name = adoptRef($1);
        $$ = XPath::Function::create(name).release();
        if (!$$)
            YYABORT;
    }
    |
    FUNCTIONNAME '(' ArgumentList ')'
    {
        String name = adoptRef($1);
        std::unique_ptr<Vector<std::unique_ptr<Expression>>> argumentList($3);
        $$ = XPath::Function::create(name, WTFMove(*argumentList)).release();
        if (!$$)
            YYABORT;
    }
    ;

ArgumentList:
    Argument
    {
        $$ = new Vector<std::unique_ptr<Expression>>;
        $$->append(std::unique_ptr<Expression>($1));
    }
    |
    ArgumentList ',' Argument
    {
        $$ = $1;
        $$->append(std::unique_ptr<Expression>($3));
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
        $$ = new Union(std::unique_ptr<Expression>($1), std::unique_ptr<Expression>($3));
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
        $3->setAbsolute();
        $$ = new Path(std::unique_ptr<Expression>($1), std::unique_ptr<LocationPath>($3));
    }
    |
    FilterExpr DescendantOrSelf RelativeLocationPath
    {
        $3->prependStep(std::unique_ptr<Step>($2));
        $3->setAbsolute();
        $$ = new Path(std::unique_ptr<Expression>($1), std::unique_ptr<LocationPath>($3));
    }
    ;

FilterExpr:
    PrimaryExpr
    |
    PrimaryExpr PredicateList
    {
        std::unique_ptr<Vector<std::unique_ptr<Expression>>> predicateList($2);
        $$ = new Filter(std::unique_ptr<Expression>($1), WTFMove(*predicateList));
    }
    ;

OrExpr:
    AndExpr
    |
    OrExpr OR AndExpr
    {
        $$ = new LogicalOp(LogicalOp::OP_Or, std::unique_ptr<Expression>($1), std::unique_ptr<Expression>($3));
    }
    ;

AndExpr:
    EqualityExpr
    |
    AndExpr AND EqualityExpr
    {
        $$ = new LogicalOp(LogicalOp::OP_And, std::unique_ptr<Expression>($1), std::unique_ptr<Expression>($3));
    }
    ;

EqualityExpr:
    RelationalExpr
    |
    EqualityExpr EQOP RelationalExpr
    {
        $$ = new EqTestOp($2, std::unique_ptr<Expression>($1), std::unique_ptr<Expression>($3));
    }
    ;

RelationalExpr:
    AdditiveExpr
    |
    RelationalExpr RELOP AdditiveExpr
    {
        $$ = new EqTestOp($2, std::unique_ptr<Expression>($1), std::unique_ptr<Expression>($3));
    }
    ;

AdditiveExpr:
    MultiplicativeExpr
    |
    AdditiveExpr PLUS MultiplicativeExpr
    {
        $$ = new NumericOp(NumericOp::OP_Add, std::unique_ptr<Expression>($1), std::unique_ptr<Expression>($3));
    }
    |
    AdditiveExpr MINUS MultiplicativeExpr
    {
        $$ = new NumericOp(NumericOp::OP_Sub, std::unique_ptr<Expression>($1), std::unique_ptr<Expression>($3));
    }
    ;

MultiplicativeExpr:
    UnaryExpr
    |
    MultiplicativeExpr MULOP UnaryExpr
    {
        $$ = new NumericOp($2, std::unique_ptr<Expression>($1), std::unique_ptr<Expression>($3));
    }
    ;

UnaryExpr:
    UnionExpr
    |
    MINUS UnaryExpr
    {
        $$ = new Negative(std::unique_ptr<Expression>($2));
    }
    ;

%%
