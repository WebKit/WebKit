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

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

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

%}

%pure-parser
%lex-param { parser }
%parse-param { WebCore::XPath::Parser& parser }

%union { 
    WebCore::XPath::NumericOp::Opcode numericOpcode;
    WebCore::XPath::EqTestOp::Opcode equalityTestOpcode;
    StringImpl* string;
    WebCore::XPath::Step::Axis axis;
    WebCore::XPath::LocationPath* locationPath;
    WebCore::XPath::Step::NodeTest* nodeTest;
    Vector<std::unique_ptr<WebCore::XPath::Expression>>* expressionVector;
    WebCore::XPath::Step* step;
    WebCore::XPath::Expression* expression;
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

static int xpathyylex(YYSTYPE* yylval, WebCore::XPath::Parser& parser) { return parser.lex(*yylval); }
static void xpathyyerror(WebCore::XPath::Parser&, const char*) { }

%}

%%

Top:
    Expr
    {
        parser.setParseResult(std::unique_ptr<WebCore::XPath::Expression>($1));
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
        $$ = new WebCore::XPath::LocationPath;
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
        $$->prependStep(std::unique_ptr<WebCore::XPath::Step>($1));
    }
    ;

RelativeLocationPath:
    Step
    {
        $$ = new WebCore::XPath::LocationPath;
        $$->appendStep(std::unique_ptr<WebCore::XPath::Step>($1));
    }
    |
    RelativeLocationPath '/' Step
    {
        $$ = $1;
        $$->appendStep(std::unique_ptr<WebCore::XPath::Step>($3));
    }
    |
    RelativeLocationPath DescendantOrSelf Step
    {
        $$ = $1;
        $$->appendStep(std::unique_ptr<WebCore::XPath::Step>($2));
        $$->appendStep(std::unique_ptr<WebCore::XPath::Step>($3));
    }
    ;

Step:
    NodeTest OptionalPredicateList
    {
        std::unique_ptr<WebCore::XPath::Step::NodeTest> nodeTest($1);
        std::unique_ptr<Vector<std::unique_ptr<WebCore::XPath::Expression>>> predicateList($2);
        if (predicateList)
            $$ = new WebCore::XPath::Step(WebCore::XPath::Step::ChildAxis, WTFMove(*nodeTest), WTFMove(*predicateList));
        else
            $$ = new WebCore::XPath::Step(WebCore::XPath::Step::ChildAxis, WTFMove(*nodeTest));
    }
    |
    NAMETEST OptionalPredicateList
    {
        String nametest = adoptRef($1);
        std::unique_ptr<Vector<std::unique_ptr<WebCore::XPath::Expression>>> predicateList($2);

        AtomString localName;
        AtomString namespaceURI;
        if (!parser.expandQualifiedName(nametest, localName, namespaceURI)) {
            $$ = nullptr;
            YYABORT;
        }

        if (predicateList)
            $$ = new WebCore::XPath::Step(WebCore::XPath::Step::ChildAxis, WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::NameTest, localName, namespaceURI), WTFMove(*predicateList));
        else
            $$ = new WebCore::XPath::Step(WebCore::XPath::Step::ChildAxis, WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::NameTest, localName, namespaceURI));
    }
    |
    AxisSpecifier NodeTest OptionalPredicateList
    {
        std::unique_ptr<WebCore::XPath::Step::NodeTest> nodeTest($2);
        std::unique_ptr<Vector<std::unique_ptr<WebCore::XPath::Expression>>> predicateList($3);

        if (predicateList)
            $$ = new WebCore::XPath::Step($1, WTFMove(*nodeTest), WTFMove(*predicateList));
        else
            $$ = new WebCore::XPath::Step($1, WTFMove(*nodeTest));
    }
    |
    AxisSpecifier NAMETEST OptionalPredicateList
    {
        String nametest = adoptRef($2);
        std::unique_ptr<Vector<std::unique_ptr<WebCore::XPath::Expression>>> predicateList($3);

        AtomString localName;
        AtomString namespaceURI;
        if (!parser.expandQualifiedName(nametest, localName, namespaceURI)) {
            $$ = nullptr;
            YYABORT;
        }

        if (predicateList)
            $$ = new WebCore::XPath::Step($1, WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::NameTest, localName, namespaceURI), WTFMove(*predicateList));
        else
            $$ = new WebCore::XPath::Step($1, WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::NameTest, localName, namespaceURI));
    }
    |
    AbbreviatedStep
    ;

AxisSpecifier:
    AXISNAME
    |
    '@'
    {
        $$ = WebCore::XPath::Step::AttributeAxis;
    }
    ;

NodeTest:
    NODE '(' ')'
    {
        $$ = new WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::AnyNodeTest);
    }
    |
    TEXT_ '(' ')'
    {
        $$ = new WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::TextNodeTest);
    }
    |
    COMMENT '(' ')'
    {
        $$ = new WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::CommentNodeTest);
    }
    |
    PI '(' ')'
    {
        $$ = new WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::ProcessingInstructionNodeTest);
    }
    |
    PI '(' LITERAL ')'
    {
        auto stringImpl = adoptRef($3);
        if (stringImpl)
            stringImpl = stringImpl->trim(deprecatedIsSpaceOrNewline);
        $$ = new WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::ProcessingInstructionNodeTest, stringImpl.get());
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
        $$ = new Vector<std::unique_ptr<WebCore::XPath::Expression>>;
        $$->append(std::unique_ptr<WebCore::XPath::Expression>($1));
    }
    |
    PredicateList Predicate
    {
        $$ = $1;
        $$->append(std::unique_ptr<WebCore::XPath::Expression>($2));
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
        $$ = new WebCore::XPath::Step(WebCore::XPath::Step::DescendantOrSelfAxis, WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::AnyNodeTest));
    }
    ;

AbbreviatedStep:
    '.'
    {
        $$ = new WebCore::XPath::Step(WebCore::XPath::Step::SelfAxis, WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::AnyNodeTest));
    }
    |
    DOTDOT
    {
        $$ = new WebCore::XPath::Step(WebCore::XPath::Step::ParentAxis, WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::AnyNodeTest));
    }
    ;

PrimaryExpr:
    VARIABLEREFERENCE
    {
        String name = adoptRef($1);
        $$ = new WebCore::XPath::VariableReference(name);
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
        $$ = new WebCore::XPath::StringExpression(WTFMove(literal));
    }
    |
    NUMBER
    {
        String numeral = adoptRef($1);
        $$ = new WebCore::XPath::Number(numeral.toDouble());
    }
    |
    FunctionCall
    ;

FunctionCall:
    FUNCTIONNAME '(' ')'
    {
        String name = adoptRef($1);
        $$ = WebCore::XPath::Function::create(name).release();
        if (!$$)
            YYABORT;
    }
    |
    FUNCTIONNAME '(' ArgumentList ')'
    {
        String name = adoptRef($1);
        std::unique_ptr<Vector<std::unique_ptr<WebCore::XPath::Expression>>> argumentList($3);
        $$ = WebCore::XPath::Function::create(name, WTFMove(*argumentList)).release();
        if (!$$)
            YYABORT;
    }
    ;

ArgumentList:
    Argument
    {
        $$ = new Vector<std::unique_ptr<WebCore::XPath::Expression>>;
        $$->append(std::unique_ptr<WebCore::XPath::Expression>($1));
    }
    |
    ArgumentList ',' Argument
    {
        $$ = $1;
        $$->append(std::unique_ptr<WebCore::XPath::Expression>($3));
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
        $$ = new WebCore::XPath::Union(std::unique_ptr<WebCore::XPath::Expression>($1), std::unique_ptr<WebCore::XPath::Expression>($3));
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
        $$ = new WebCore::XPath::Path(std::unique_ptr<WebCore::XPath::Expression>($1), std::unique_ptr<WebCore::XPath::LocationPath>($3));
    }
    |
    FilterExpr DescendantOrSelf RelativeLocationPath
    {
        $3->prependStep(std::unique_ptr<WebCore::XPath::Step>($2));
        $3->setAbsolute();
        $$ = new WebCore::XPath::Path(std::unique_ptr<WebCore::XPath::Expression>($1), std::unique_ptr<WebCore::XPath::LocationPath>($3));
    }
    ;

FilterExpr:
    PrimaryExpr
    |
    PrimaryExpr PredicateList
    {
        std::unique_ptr<Vector<std::unique_ptr<WebCore::XPath::Expression>>> predicateList($2);
        $$ = new WebCore::XPath::Filter(std::unique_ptr<WebCore::XPath::Expression>($1), WTFMove(*predicateList));
    }
    ;

OrExpr:
    AndExpr
    |
    OrExpr OR AndExpr
    {
        $$ = new WebCore::XPath::LogicalOp(WebCore::XPath::LogicalOp::Opcode::Or, std::unique_ptr<WebCore::XPath::Expression>($1), std::unique_ptr<WebCore::XPath::Expression>($3));
    }
    ;

AndExpr:
    EqualityExpr
    |
    AndExpr AND EqualityExpr
    {
        $$ = new WebCore::XPath::LogicalOp(WebCore::XPath::LogicalOp::Opcode::And, std::unique_ptr<WebCore::XPath::Expression>($1), std::unique_ptr<WebCore::XPath::Expression>($3));
    }
    ;

EqualityExpr:
    RelationalExpr
    |
    EqualityExpr EQOP RelationalExpr
    {
        $$ = new WebCore::XPath::EqTestOp($2, std::unique_ptr<WebCore::XPath::Expression>($1), std::unique_ptr<WebCore::XPath::Expression>($3));
    }
    ;

RelationalExpr:
    AdditiveExpr
    |
    RelationalExpr RELOP AdditiveExpr
    {
        $$ = new WebCore::XPath::EqTestOp($2, std::unique_ptr<WebCore::XPath::Expression>($1), std::unique_ptr<WebCore::XPath::Expression>($3));
    }
    ;

AdditiveExpr:
    MultiplicativeExpr
    |
    AdditiveExpr PLUS MultiplicativeExpr
    {
        $$ = new WebCore::XPath::NumericOp(WebCore::XPath::NumericOp::Opcode::Add, std::unique_ptr<WebCore::XPath::Expression>($1), std::unique_ptr<WebCore::XPath::Expression>($3));
    }
    |
    AdditiveExpr MINUS MultiplicativeExpr
    {
        $$ = new WebCore::XPath::NumericOp(WebCore::XPath::NumericOp::Opcode::Sub, std::unique_ptr<WebCore::XPath::Expression>($1), std::unique_ptr<WebCore::XPath::Expression>($3));
    }
    ;

MultiplicativeExpr:
    UnaryExpr
    |
    MultiplicativeExpr MULOP UnaryExpr
    {
        $$ = new WebCore::XPath::NumericOp($2, std::unique_ptr<WebCore::XPath::Expression>($1), std::unique_ptr<WebCore::XPath::Expression>($3));
    }
    ;

UnaryExpr:
    UnionExpr
    |
    MINUS UnaryExpr
    {
        $$ = new WebCore::XPath::Negative(std::unique_ptr<WebCore::XPath::Expression>($2));
    }
    ;

%%

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
