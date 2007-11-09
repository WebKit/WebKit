%{

/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"
#include "nodes.h"
#include "lexer.h"
#include "internal.h"
#include "CommonIdentifiers.h"
#include <wtf/MathExtras.h>

// Not sure why, but yacc doesn't add this define along with the others.
#define yylloc kjsyylloc

#define YYMAXDEPTH 10000
#define YYENABLE_NLS 0

/* default values for bison */
#define YYDEBUG 0
#if !PLATFORM(DARWIN)
    // avoid triggering warnings in older bison
#define YYERROR_VERBOSE
#endif

extern int kjsyylex();
int kjsyyerror(const char *);
static bool allowAutomaticSemicolon();

#define AUTO_SEMICOLON do { if (!allowAutomaticSemicolon()) YYABORT; } while (0)
#define DBG(l, s, e) (l)->setLoc((s).first_line, (e).last_line)

using namespace KJS;

static ExpressionNode* makeAssignNode(ExpressionNode* loc, Operator, ExpressionNode* expr);
static ExpressionNode* makePrefixNode(ExpressionNode* expr, Operator);
static ExpressionNode* makePostfixNode(ExpressionNode* expr, Operator);
static PropertyNode* makeGetterOrSetterPropertyNode(const Identifier &getOrSet, const Identifier& name, ParameterNode*, FunctionBodyNode*);
static ExpressionNode* makeFunctionCallNode(ExpressionNode* func, ArgumentsNode*);
static ExpressionNode* makeTypeOfNode(ExpressionNode*);
static ExpressionNode* makeDeleteNode(ExpressionNode*);
static ExpressionNode* makeNegateNode(ExpressionNode*);
static NumberNode* makeNumberNode(double);

#if COMPILER(MSVC)

#pragma warning(disable: 4065)
#pragma warning(disable: 4244)
#pragma warning(disable: 4702)

// At least some of the time, the declarations of malloc and free that bison
// generates are causing warnings. A way to avoid this is to explicitly define
// the macros so that bison doesn't try to declare malloc and free.
#define YYMALLOC malloc
#define YYFREE free

#endif

%}

%union {
  int                 ival;
  double              dval;
  UString             *ustr;
  Identifier          *ident;
  ExpressionNode      *node;
  StatementNode       *stat;
  ParameterList       param;
  FunctionBodyNode    *body;
  FuncDeclNode        *func;
  FuncExprNode        *funcExpr;
  ProgramNode         *prog;
  AssignExprNode      *init;
  SourceElements      *srcs;
  ArgumentsNode       *args;
  ArgumentList        alist;
  VarDeclNode         *decl;
  VarDeclList         vlist;
  CaseBlockNode       *cblk;
  ClauseList          clist;
  CaseClauseNode      *ccl;
  ElementList         elm;
  Operator            op;
  PropertyList        plist;
  PropertyNode       *pnode;
}

%start Program

/* literals */
%token NULLTOKEN TRUETOKEN FALSETOKEN

/* keywords */
%token BREAK CASE DEFAULT FOR NEW VAR CONSTTOKEN CONTINUE
%token FUNCTION RETURN VOIDTOKEN DELETETOKEN
%token IF THISTOKEN DO WHILE INTOKEN INSTANCEOF TYPEOF
%token SWITCH WITH RESERVED
%token THROW TRY CATCH FINALLY
%token DEBUGGER

/* give an if without an else higher precedence than an else to resolve the ambiguity */
%nonassoc IF_WITHOUT_ELSE
%nonassoc ELSE

/* punctuators */
%token EQEQ NE                     /* == and != */
%token STREQ STRNEQ                /* === and !== */
%token LE GE                       /* < and > */
%token OR AND                      /* || and && */
%token PLUSPLUS MINUSMINUS         /* ++ and --  */
%token LSHIFT                      /* << */
%token RSHIFT URSHIFT              /* >> and >>> */
%token PLUSEQUAL MINUSEQUAL        /* += and -= */
%token MULTEQUAL DIVEQUAL          /* *= and /= */
%token LSHIFTEQUAL                 /* <<= */
%token RSHIFTEQUAL URSHIFTEQUAL    /* >>= and >>>= */
%token ANDEQUAL MODEQUAL           /* &= and %= */
%token XOREQUAL OREQUAL            /* ^= and |= */

/* terminal types */
%token <dval> NUMBER
%token <ustr> STRING
%token <ident> IDENT

/* automatically inserted semicolon */
%token AUTOPLUSPLUS AUTOMINUSMINUS

/* non-terminal types */
%type <node>  Literal ArrayLiteral

%type <node>  PrimaryExpr PrimaryExprNoBrace
%type <node>  MemberExpr MemberExprNoBF /* BF => brace or function */
%type <node>  NewExpr NewExprNoBF
%type <node>  CallExpr CallExprNoBF
%type <node>  LeftHandSideExpr LeftHandSideExprNoBF
%type <node>  PostfixExpr PostfixExprNoBF
%type <node>  UnaryExpr UnaryExprNoBF UnaryExprCommon
%type <node>  MultiplicativeExpr MultiplicativeExprNoBF
%type <node>  AdditiveExpr AdditiveExprNoBF
%type <node>  ShiftExpr ShiftExprNoBF
%type <node>  RelationalExpr RelationalExprNoIn RelationalExprNoBF
%type <node>  EqualityExpr EqualityExprNoIn EqualityExprNoBF
%type <node>  BitwiseANDExpr BitwiseANDExprNoIn BitwiseANDExprNoBF
%type <node>  BitwiseXORExpr BitwiseXORExprNoIn BitwiseXORExprNoBF
%type <node>  BitwiseORExpr BitwiseORExprNoIn BitwiseORExprNoBF
%type <node>  LogicalANDExpr LogicalANDExprNoIn LogicalANDExprNoBF
%type <node>  LogicalORExpr LogicalORExprNoIn LogicalORExprNoBF
%type <node>  ConditionalExpr ConditionalExprNoIn ConditionalExprNoBF
%type <node>  AssignmentExpr AssignmentExprNoIn AssignmentExprNoBF
%type <node>  Expr ExprNoIn ExprNoBF

%type <node>  ExprOpt ExprNoInOpt

%type <stat>  Statement Block
%type <stat>  VariableStatement ConstStatement EmptyStatement ExprStatement
%type <stat>  IfStatement IterationStatement ContinueStatement
%type <stat>  BreakStatement ReturnStatement WithStatement
%type <stat>  SwitchStatement LabelledStatement
%type <stat>  ThrowStatement TryStatement
%type <stat>  DebuggerStatement
%type <stat>  SourceElement

%type <init>  Initializer InitializerNoIn
%type <func>  FunctionDeclaration
%type <funcExpr>  FunctionExpr
%type <body>  FunctionBody
%type <srcs>  SourceElements
%type <param> FormalParameterList
%type <op>    AssignmentOperator
%type <args>  Arguments
%type <alist> ArgumentList
%type <vlist> VariableDeclarationList VariableDeclarationListNoIn ConstDeclarationList
%type <decl>  VariableDeclaration VariableDeclarationNoIn ConstDeclaration
%type <cblk>  CaseBlock
%type <ccl>   CaseClause DefaultClause
%type <clist> CaseClauses CaseClausesOpt
%type <ival>  Elision ElisionOpt
%type <elm>   ElementList
%type <pnode> Property
%type <plist> PropertyList
%%

Literal:
    NULLTOKEN                           { $$ = new NullNode(); }
  | TRUETOKEN                           { $$ = new BooleanNode(true); }
  | FALSETOKEN                          { $$ = new BooleanNode(false); }
  | NUMBER                              { $$ = makeNumberNode($1); }
  | STRING                              { $$ = new StringNode($1); }
  | '/' /* regexp */                    {
                                            Lexer *l = Lexer::curr();
                                            if (!l->scanRegExp()) YYABORT;
                                            $$ = new RegExpNode(l->pattern, l->flags);
                                        }
  | DIVEQUAL /* regexp with /= */       {
                                            Lexer *l = Lexer::curr();
                                            if (!l->scanRegExp()) YYABORT;
                                            $$ = new RegExpNode("=" + l->pattern, l->flags);
                                        }
;

Property:
    IDENT ':' AssignmentExpr            { $$ = new PropertyNode(*$1, $3, PropertyNode::Constant); }
  | STRING ':' AssignmentExpr           { $$ = new PropertyNode(Identifier(*$1), $3, PropertyNode::Constant); }
  | NUMBER ':' AssignmentExpr           { $$ = new PropertyNode(Identifier(UString::from($1)), $3, PropertyNode::Constant); }
  | IDENT IDENT '(' ')' FunctionBody    { $$ = makeGetterOrSetterPropertyNode(*$1, *$2, 0, $5); if (!$$) YYABORT; }
  | IDENT IDENT '(' FormalParameterList ')' FunctionBody
                                        { $$ = makeGetterOrSetterPropertyNode(*$1, *$2, $4.head, $6); if (!$$) YYABORT; }
;

PropertyList:
    Property                            { $$.head = new PropertyListNode($1); 
                                          $$.tail = $$.head; }
  | PropertyList ',' Property           { $$.head = $1.head;
                                          $$.tail = new PropertyListNode($3, $1.tail); }
;

PrimaryExpr:
    PrimaryExprNoBrace
  | '{' '}'                             { $$ = new ObjectLiteralNode(); }
  | '{' PropertyList '}'                { $$ = new ObjectLiteralNode($2.head); }
  /* allow extra comma, see http://bugs.webkit.org/show_bug.cgi?id=5939 */
  | '{' PropertyList ',' '}'            { $$ = new ObjectLiteralNode($2.head); }
;

PrimaryExprNoBrace:
    THISTOKEN                           { $$ = new ThisNode(); }
  | Literal
  | ArrayLiteral
  | IDENT                               { $$ = new ResolveNode(*$1); }
  | '(' Expr ')'                        { $$ = $2; }
;

ArrayLiteral:
    '[' ElisionOpt ']'                  { $$ = new ArrayNode($2); }
  | '[' ElementList ']'                 { $$ = new ArrayNode($2.head); }
  | '[' ElementList ',' ElisionOpt ']'  { $$ = new ArrayNode($4, $2.head); }
;

ElementList:
    ElisionOpt AssignmentExpr           { $$.head = new ElementNode($1, $2); 
                                          $$.tail = $$.head; }
  | ElementList ',' ElisionOpt AssignmentExpr
                                        { $$.head = $1.head;
                                          $$.tail = new ElementNode($1.tail, $3, $4); }
;

ElisionOpt:
    /* nothing */                       { $$ = 0; }
  | Elision
;

Elision:
    ','                                 { $$ = 1; }
  | Elision ','                         { $$ = $1 + 1; }
;

MemberExpr:
    PrimaryExpr
  | FunctionExpr                        { $$ = $1; }
  | MemberExpr '[' Expr ']'             { $$ = new BracketAccessorNode($1, $3); }
  | MemberExpr '.' IDENT                { $$ = new DotAccessorNode($1, *$3); }
  | NEW MemberExpr Arguments            { $$ = new NewExprNode($2, $3); }
;

MemberExprNoBF:
    PrimaryExprNoBrace
  | MemberExprNoBF '[' Expr ']'         { $$ = new BracketAccessorNode($1, $3); }
  | MemberExprNoBF '.' IDENT            { $$ = new DotAccessorNode($1, *$3); }
  | NEW MemberExpr Arguments            { $$ = new NewExprNode($2, $3); }
;

NewExpr:
    MemberExpr
  | NEW NewExpr                         { $$ = new NewExprNode($2); }
;

NewExprNoBF:
    MemberExprNoBF
  | NEW NewExpr                         { $$ = new NewExprNode($2); }
;

CallExpr:
    MemberExpr Arguments                { $$ = makeFunctionCallNode($1, $2); }
  | CallExpr Arguments                  { $$ = makeFunctionCallNode($1, $2); }
  | CallExpr '[' Expr ']'               { $$ = new BracketAccessorNode($1, $3); }
  | CallExpr '.' IDENT                  { $$ = new DotAccessorNode($1, *$3); }
;

CallExprNoBF:
    MemberExprNoBF Arguments            { $$ = makeFunctionCallNode($1, $2); }
  | CallExprNoBF Arguments              { $$ = makeFunctionCallNode($1, $2); }
  | CallExprNoBF '[' Expr ']'           { $$ = new BracketAccessorNode($1, $3); }
  | CallExprNoBF '.' IDENT              { $$ = new DotAccessorNode($1, *$3); }
;

Arguments:
    '(' ')'                             { $$ = new ArgumentsNode(); }
  | '(' ArgumentList ')'                { $$ = new ArgumentsNode($2.head); }
;

ArgumentList:
    AssignmentExpr                      { $$.head = new ArgumentListNode($1);
                                          $$.tail = $$.head; }
  | ArgumentList ',' AssignmentExpr     { $$.head = $1.head;
                                          $$.tail = new ArgumentListNode($1.tail, $3); }
;

LeftHandSideExpr:
    NewExpr
  | CallExpr
;

LeftHandSideExprNoBF:
    NewExprNoBF
  | CallExprNoBF
;

PostfixExpr:
    LeftHandSideExpr
  | LeftHandSideExpr PLUSPLUS           { $$ = makePostfixNode($1, OpPlusPlus); }
  | LeftHandSideExpr MINUSMINUS         { $$ = makePostfixNode($1, OpMinusMinus); }
;

PostfixExprNoBF:
    LeftHandSideExprNoBF
  | LeftHandSideExprNoBF PLUSPLUS       { $$ = makePostfixNode($1, OpPlusPlus); }
  | LeftHandSideExprNoBF MINUSMINUS     { $$ = makePostfixNode($1, OpMinusMinus); }
;

UnaryExprCommon:
    DELETETOKEN UnaryExpr               { $$ = makeDeleteNode($2); }
  | VOIDTOKEN UnaryExpr                 { $$ = new VoidNode($2); }
  | TYPEOF UnaryExpr                    { $$ = makeTypeOfNode($2); }
  | PLUSPLUS UnaryExpr                  { $$ = makePrefixNode($2, OpPlusPlus); }
  | AUTOPLUSPLUS UnaryExpr              { $$ = makePrefixNode($2, OpPlusPlus); }
  | MINUSMINUS UnaryExpr                { $$ = makePrefixNode($2, OpMinusMinus); }
  | AUTOMINUSMINUS UnaryExpr            { $$ = makePrefixNode($2, OpMinusMinus); }
  | '+' UnaryExpr                       { $$ = new UnaryPlusNode($2); }
  | '-' UnaryExpr                       { $$ = makeNegateNode($2); }
  | '~' UnaryExpr                       { $$ = new BitwiseNotNode($2); }
  | '!' UnaryExpr                       { $$ = new LogicalNotNode($2); }

UnaryExpr:
    PostfixExpr
  | UnaryExprCommon
;

UnaryExprNoBF:
    PostfixExprNoBF
  | UnaryExprCommon
;

MultiplicativeExpr:
    UnaryExpr
  | MultiplicativeExpr '*' UnaryExpr    { $$ = new MultNode($1, $3); }
  | MultiplicativeExpr '/' UnaryExpr    { $$ = new DivNode($1, $3); }
  | MultiplicativeExpr '%' UnaryExpr    { $$ = new ModNode($1, $3); }
;

MultiplicativeExprNoBF:
    UnaryExprNoBF
  | MultiplicativeExprNoBF '*' UnaryExpr
                                        { $$ = new MultNode($1, $3); }
  | MultiplicativeExprNoBF '/' UnaryExpr
                                        { $$ = new DivNode($1, $3); }
  | MultiplicativeExprNoBF '%' UnaryExpr
                                        { $$ = new ModNode($1, $3); }
;

AdditiveExpr:
    MultiplicativeExpr
  | AdditiveExpr '+' MultiplicativeExpr { $$ = new AddNode($1, $3); }
  | AdditiveExpr '-' MultiplicativeExpr { $$ = new SubNode($1, $3); }
;

AdditiveExprNoBF:
    MultiplicativeExprNoBF
  | AdditiveExprNoBF '+' MultiplicativeExpr
                                        { $$ = new AddNode($1, $3); }
  | AdditiveExprNoBF '-' MultiplicativeExpr
                                        { $$ = new SubNode($1, $3); }
;

ShiftExpr:
    AdditiveExpr
  | ShiftExpr LSHIFT AdditiveExpr       { $$ = new LeftShiftNode($1, $3); }
  | ShiftExpr RSHIFT AdditiveExpr       { $$ = new RightShiftNode($1, $3); }
  | ShiftExpr URSHIFT AdditiveExpr      { $$ = new UnsignedRightShiftNode($1, $3); }
;

ShiftExprNoBF:
    AdditiveExprNoBF
  | ShiftExprNoBF LSHIFT AdditiveExpr   { $$ = new LeftShiftNode($1, $3); }
  | ShiftExprNoBF RSHIFT AdditiveExpr   { $$ = new RightShiftNode($1, $3); }
  | ShiftExprNoBF URSHIFT AdditiveExpr  { $$ = new UnsignedRightShiftNode($1, $3); }
;

RelationalExpr:
    ShiftExpr
  | RelationalExpr '<' ShiftExpr        { $$ = new LessNode($1, $3); }
  | RelationalExpr '>' ShiftExpr        { $$ = new GreaterNode($1, $3); }
  | RelationalExpr LE ShiftExpr         { $$ = new LessEqNode($1, $3); }
  | RelationalExpr GE ShiftExpr         { $$ = new GreaterEqNode($1, $3); }
  | RelationalExpr INSTANCEOF ShiftExpr { $$ = new InstanceOfNode($1, $3); }
  | RelationalExpr INTOKEN ShiftExpr    { $$ = new InNode($1, $3); }
;

RelationalExprNoIn:
    ShiftExpr
  | RelationalExprNoIn '<' ShiftExpr    { $$ = new LessNode($1, $3); }
  | RelationalExprNoIn '>' ShiftExpr    { $$ = new GreaterNode($1, $3); }
  | RelationalExprNoIn LE ShiftExpr     { $$ = new LessEqNode($1, $3); }
  | RelationalExprNoIn GE ShiftExpr     { $$ = new GreaterEqNode($1, $3); }
  | RelationalExprNoIn INSTANCEOF ShiftExpr
                                        { $$ = new InstanceOfNode($1, $3); }
;

RelationalExprNoBF:
    ShiftExprNoBF
  | RelationalExprNoBF '<' ShiftExpr    { $$ = new LessNode($1, $3); }
  | RelationalExprNoBF '>' ShiftExpr    { $$ = new GreaterNode($1, $3); }
  | RelationalExprNoBF LE ShiftExpr     { $$ = new LessEqNode($1, $3); }
  | RelationalExprNoBF GE ShiftExpr     { $$ = new GreaterEqNode($1, $3); }
  | RelationalExprNoBF INSTANCEOF ShiftExpr
                                        { $$ = new InstanceOfNode($1, $3); }
  | RelationalExprNoBF INTOKEN ShiftExpr     { $$ = new InNode($1, $3); }
;

EqualityExpr:
    RelationalExpr
  | EqualityExpr EQEQ RelationalExpr    { $$ = new EqualNode($1, $3); }
  | EqualityExpr NE RelationalExpr      { $$ = new NotEqualNode($1, $3); }
  | EqualityExpr STREQ RelationalExpr   { $$ = new StrictEqualNode($1, $3); }
  | EqualityExpr STRNEQ RelationalExpr  { $$ = new NotStrictEqualNode($1, $3); }
;

EqualityExprNoIn:
    RelationalExprNoIn
  | EqualityExprNoIn EQEQ RelationalExprNoIn
                                        { $$ = new EqualNode($1, $3); }
  | EqualityExprNoIn NE RelationalExprNoIn
                                        { $$ = new NotEqualNode($1, $3); }
  | EqualityExprNoIn STREQ RelationalExprNoIn
                                        { $$ = new StrictEqualNode($1, $3); }
  | EqualityExprNoIn STRNEQ RelationalExprNoIn
                                        { $$ = new NotStrictEqualNode($1, $3); }
;

EqualityExprNoBF:
    RelationalExprNoBF
  | EqualityExprNoBF EQEQ RelationalExpr
                                        { $$ = new EqualNode($1, $3); }
  | EqualityExprNoBF NE RelationalExpr  { $$ = new NotEqualNode($1, $3); }
  | EqualityExprNoBF STREQ RelationalExpr
                                        { $$ = new StrictEqualNode($1, $3); }
  | EqualityExprNoBF STRNEQ RelationalExpr
                                        { $$ = new NotStrictEqualNode($1, $3); }
;

BitwiseANDExpr:
    EqualityExpr
  | BitwiseANDExpr '&' EqualityExpr     { $$ = new BitAndNode($1, $3); }
;

BitwiseANDExprNoIn:
    EqualityExprNoIn
  | BitwiseANDExprNoIn '&' EqualityExprNoIn
                                        { $$ = new BitAndNode($1, $3); }
;

BitwiseANDExprNoBF:
    EqualityExprNoBF
  | BitwiseANDExprNoBF '&' EqualityExpr { $$ = new BitAndNode($1, $3); }
;

BitwiseXORExpr:
    BitwiseANDExpr
  | BitwiseXORExpr '^' BitwiseANDExpr   { $$ = new BitXOrNode($1, $3); }
;

BitwiseXORExprNoIn:
    BitwiseANDExprNoIn
  | BitwiseXORExprNoIn '^' BitwiseANDExprNoIn
                                        { $$ = new BitXOrNode($1, $3); }
;

BitwiseXORExprNoBF:
    BitwiseANDExprNoBF
  | BitwiseXORExprNoBF '^' BitwiseANDExpr
                                        { $$ = new BitXOrNode($1, $3); }
;

BitwiseORExpr:
    BitwiseXORExpr
  | BitwiseORExpr '|' BitwiseXORExpr    { $$ = new BitOrNode($1, $3); }
;

BitwiseORExprNoIn:
    BitwiseXORExprNoIn
  | BitwiseORExprNoIn '|' BitwiseXORExprNoIn
                                        { $$ = new BitOrNode($1, $3); }
;

BitwiseORExprNoBF:
    BitwiseXORExprNoBF
  | BitwiseORExprNoBF '|' BitwiseXORExpr
                                        { $$ = new BitOrNode($1, $3); }
;

LogicalANDExpr:
    BitwiseORExpr
  | LogicalANDExpr AND BitwiseORExpr    { $$ = new LogicalAndNode($1, $3); }
;

LogicalANDExprNoIn:
    BitwiseORExprNoIn
  | LogicalANDExprNoIn AND BitwiseORExprNoIn
                                        { $$ = new LogicalAndNode($1, $3); }
;

LogicalANDExprNoBF:
    BitwiseORExprNoBF
  | LogicalANDExprNoBF AND BitwiseORExpr
                                        { $$ = new LogicalAndNode($1, $3); }
;

LogicalORExpr:
    LogicalANDExpr
  | LogicalORExpr OR LogicalANDExpr     { $$ = new LogicalOrNode($1, $3); }
;

LogicalORExprNoIn:
    LogicalANDExprNoIn
  | LogicalORExprNoIn OR LogicalANDExprNoIn
                                        { $$ = new LogicalOrNode($1, $3); }
;

LogicalORExprNoBF:
    LogicalANDExprNoBF
  | LogicalORExprNoBF OR LogicalANDExpr { $$ = new LogicalOrNode($1, $3); }
;

ConditionalExpr:
    LogicalORExpr
  | LogicalORExpr '?' AssignmentExpr ':' AssignmentExpr
                                        { $$ = new ConditionalNode($1, $3, $5); }
;

ConditionalExprNoIn:
    LogicalORExprNoIn
  | LogicalORExprNoIn '?' AssignmentExprNoIn ':' AssignmentExprNoIn
                                        { $$ = new ConditionalNode($1, $3, $5); }
;

ConditionalExprNoBF:
    LogicalORExprNoBF
  | LogicalORExprNoBF '?' AssignmentExpr ':' AssignmentExpr
                                        { $$ = new ConditionalNode($1, $3, $5); }
;

AssignmentExpr:
    ConditionalExpr
  | LeftHandSideExpr AssignmentOperator AssignmentExpr
                                        { $$ = makeAssignNode($1, $2, $3); }
;

AssignmentExprNoIn:
    ConditionalExprNoIn
  | LeftHandSideExpr AssignmentOperator AssignmentExprNoIn
                                        { $$ = makeAssignNode($1, $2, $3); }
;

AssignmentExprNoBF:
    ConditionalExprNoBF
  | LeftHandSideExprNoBF AssignmentOperator AssignmentExpr
                                        { $$ = makeAssignNode($1, $2, $3); }
;

AssignmentOperator:
    '='                                 { $$ = OpEqual; }
  | PLUSEQUAL                           { $$ = OpPlusEq; }
  | MINUSEQUAL                          { $$ = OpMinusEq; }
  | MULTEQUAL                           { $$ = OpMultEq; }
  | DIVEQUAL                            { $$ = OpDivEq; }
  | LSHIFTEQUAL                         { $$ = OpLShift; }
  | RSHIFTEQUAL                         { $$ = OpRShift; }
  | URSHIFTEQUAL                        { $$ = OpURShift; }
  | ANDEQUAL                            { $$ = OpAndEq; }
  | XOREQUAL                            { $$ = OpXOrEq; }
  | OREQUAL                             { $$ = OpOrEq; }
  | MODEQUAL                            { $$ = OpModEq; }
;

Expr:
    AssignmentExpr
  | Expr ',' AssignmentExpr             { $$ = new CommaNode($1, $3); }
;

ExprNoIn:
    AssignmentExprNoIn
  | ExprNoIn ',' AssignmentExprNoIn     { $$ = new CommaNode($1, $3); }
;

ExprNoBF:
    AssignmentExprNoBF
  | ExprNoBF ',' AssignmentExpr         { $$ = new CommaNode($1, $3); }
;

Statement:
    Block
  | VariableStatement
  | ConstStatement
  | EmptyStatement
  | ExprStatement
  | IfStatement
  | IterationStatement
  | ContinueStatement
  | BreakStatement
  | ReturnStatement
  | WithStatement
  | SwitchStatement
  | LabelledStatement
  | ThrowStatement
  | TryStatement
  | DebuggerStatement
;

Block:
    '{' '}'                             { $$ = new BlockNode(0); DBG($$, @2, @2); }
  | '{' SourceElements '}'              { $$ = new BlockNode($2); DBG($$, @3, @3); }
;

VariableStatement:
    VAR VariableDeclarationList ';'     { $$ = new VarStatementNode($2.head); DBG($$, @1, @3); }
  | VAR VariableDeclarationList error   { $$ = new VarStatementNode($2.head); DBG($$, @1, @2); AUTO_SEMICOLON; }
;

VariableDeclarationList:
    VariableDeclaration                 { $$.head = $1; 
                                          $$.tail = $$.head; }
  | VariableDeclarationList ',' VariableDeclaration
                                        { $$.head = $1.head;
                                          $1.tail->next = $3;
                                          $$.tail = $3; }
;

VariableDeclarationListNoIn:
    VariableDeclarationNoIn             { $$.head = $1; 
                                          $$.tail = $$.head; }
  | VariableDeclarationListNoIn ',' VariableDeclarationNoIn
                                        { $$.head = $1.head;
                                          $1.tail->next = $3;
                                          $$.tail = $3; }
;

VariableDeclaration:
    IDENT                               { $$ = new VarDeclNode(*$1, 0, VarDeclNode::Variable); }
  | IDENT Initializer                   { $$ = new VarDeclNode(*$1, $2, VarDeclNode::Variable); }
;

VariableDeclarationNoIn:
    IDENT                               { $$ = new VarDeclNode(*$1, 0, VarDeclNode::Variable); }
  | IDENT InitializerNoIn               { $$ = new VarDeclNode(*$1, $2, VarDeclNode::Variable); }
;

ConstStatement:
    CONSTTOKEN ConstDeclarationList ';' { $$ = new VarStatementNode($2.head); DBG($$, @1, @3); }
  | CONSTTOKEN ConstDeclarationList error
                                        { $$ = new VarStatementNode($2.head); DBG($$, @1, @2); AUTO_SEMICOLON; }
;

ConstDeclarationList:
    ConstDeclaration                    { $$.head = $1; 
                                          $$.tail = $$.head; }
  | ConstDeclarationList ',' ConstDeclaration
                                        { $$.head = $1.head;
                                          $1.tail->next = $3;
                                          $$.tail = $3; }
;

ConstDeclaration:
    IDENT                               { $$ = new VarDeclNode(*$1, 0, VarDeclNode::Constant); }
  | IDENT Initializer                   { $$ = new VarDeclNode(*$1, $2, VarDeclNode::Constant); }
;

Initializer:
    '=' AssignmentExpr                  { $$ = new AssignExprNode($2); }
;

InitializerNoIn:
    '=' AssignmentExprNoIn              { $$ = new AssignExprNode($2); }
;

EmptyStatement:
    ';'                                 { $$ = new EmptyStatementNode(); }
;

ExprStatement:
    ExprNoBF ';'                        { $$ = new ExprStatementNode($1); DBG($$, @1, @2); }
  | ExprNoBF error                      { $$ = new ExprStatementNode($1); DBG($$, @1, @1); AUTO_SEMICOLON; }
;

IfStatement:
    IF '(' Expr ')' Statement %prec IF_WITHOUT_ELSE
                                        { $$ = new IfNode($3, $5, 0); DBG($$, @1, @4); }
  | IF '(' Expr ')' Statement ELSE Statement
                                        { $$ = new IfNode($3, $5, $7); DBG($$, @1, @4); }
;

IterationStatement:
    DO Statement WHILE '(' Expr ')' ';'    { $$ = new DoWhileNode($2, $5); DBG($$, @1, @3); }
  | DO Statement WHILE '(' Expr ')' error  { $$ = new DoWhileNode($2, $5); DBG($$, @1, @3); } // Always performs automatic semicolon insertion.
  | WHILE '(' Expr ')' Statement        { $$ = new WhileNode($3, $5); DBG($$, @1, @4); }
  | FOR '(' ExprNoInOpt ';' ExprOpt ';' ExprOpt ')' Statement
                                        { $$ = new ForNode($3, $5, $7, $9); DBG($$, @1, @8); }
  | FOR '(' VAR VariableDeclarationListNoIn ';' ExprOpt ';' ExprOpt ')' Statement
                                        { $$ = new ForNode($4.head, $6, $8, $10); DBG($$, @1, @9); }
  | FOR '(' LeftHandSideExpr INTOKEN Expr ')' Statement
                                        {
                                            ExpressionNode* n = $3;
                                            if (!n->isLocation())
                                                YYABORT;
                                            $$ = new ForInNode(n, $5, $7);
                                            DBG($$, @1, @6);
                                        }
  | FOR '(' VAR IDENT INTOKEN Expr ')' Statement
                                        { $$ = new ForInNode(*$4, 0, $6, $8); DBG($$, @1, @7); }
  | FOR '(' VAR IDENT InitializerNoIn INTOKEN Expr ')' Statement
                                        { $$ = new ForInNode(*$4, $5, $7, $9); DBG($$, @1, @8); }
;

ExprOpt:
    /* nothing */                       { $$ = 0; }
  | Expr
;

ExprNoInOpt:
    /* nothing */                       { $$ = 0; }
  | ExprNoIn
;

ContinueStatement:
    CONTINUE ';'                        { $$ = new ContinueNode(); DBG($$, @1, @2); }
  | CONTINUE error                      { $$ = new ContinueNode(); DBG($$, @1, @1); AUTO_SEMICOLON; }
  | CONTINUE IDENT ';'                  { $$ = new ContinueNode(*$2); DBG($$, @1, @3); }
  | CONTINUE IDENT error                { $$ = new ContinueNode(*$2); DBG($$, @1, @2); AUTO_SEMICOLON; }
;

BreakStatement:
    BREAK ';'                           { $$ = new BreakNode(); DBG($$, @1, @2); }
  | BREAK error                         { $$ = new BreakNode(); DBG($$, @1, @1); AUTO_SEMICOLON; }
  | BREAK IDENT ';'                     { $$ = new BreakNode(*$2); DBG($$, @1, @3); }
  | BREAK IDENT error                   { $$ = new BreakNode(*$2); DBG($$, @1, @2); AUTO_SEMICOLON; }
;

ReturnStatement:
    RETURN ';'                          { $$ = new ReturnNode(0); DBG($$, @1, @2); }
  | RETURN error                        { $$ = new ReturnNode(0); DBG($$, @1, @1); AUTO_SEMICOLON; }
  | RETURN Expr ';'                     { $$ = new ReturnNode($2); DBG($$, @1, @3); }
  | RETURN Expr error                   { $$ = new ReturnNode($2); DBG($$, @1, @2); AUTO_SEMICOLON; }
;

WithStatement:
    WITH '(' Expr ')' Statement         { $$ = new WithNode($3, $5); DBG($$, @1, @4); }
;

SwitchStatement:
    SWITCH '(' Expr ')' CaseBlock       { $$ = new SwitchNode($3, $5); DBG($$, @1, @4); }
;

CaseBlock:
    '{' CaseClausesOpt '}'              { $$ = new CaseBlockNode($2.head, 0, 0); }
  | '{' CaseClausesOpt DefaultClause CaseClausesOpt '}'
                                        { $$ = new CaseBlockNode($2.head, $3, $4.head); }
;

CaseClausesOpt:
    /* nothing */                       { $$.head = 0; $$.tail = 0; }
  | CaseClauses
;

CaseClauses:
    CaseClause                          { $$.head = new ClauseListNode($1);
                                          $$.tail = $$.head; }
  | CaseClauses CaseClause              { $$.head = $1.head; 
                                          $$.tail = new ClauseListNode($1.tail, $2); }
;

CaseClause:
    CASE Expr ':'                       { $$ = new CaseClauseNode($2); }
  | CASE Expr ':' SourceElements        { $$ = new CaseClauseNode($2, $4); }
;

DefaultClause:
    DEFAULT ':'                         { $$ = new CaseClauseNode(0); }
  | DEFAULT ':' SourceElements          { $$ = new CaseClauseNode(0, $3); }
;

LabelledStatement:
    IDENT ':' Statement                 { $3->pushLabel(*$1); $$ = new LabelNode(*$1, $3); }
;

ThrowStatement:
    THROW Expr ';'                      { $$ = new ThrowNode($2); DBG($$, @1, @3); }
  | THROW Expr error                    { $$ = new ThrowNode($2); DBG($$, @1, @2); AUTO_SEMICOLON; }
;

TryStatement:
    TRY Block FINALLY Block             { $$ = new TryNode($2, CommonIdentifiers::shared()->nullIdentifier, 0, $4); DBG($$, @1, @2); }
  | TRY Block CATCH '(' IDENT ')' Block { $$ = new TryNode($2, *$5, $7, 0); DBG($$, @1, @2); }
  | TRY Block CATCH '(' IDENT ')' Block FINALLY Block
                                        { $$ = new TryNode($2, *$5, $7, $9); DBG($$, @1, @2); }
;

DebuggerStatement:
    DEBUGGER ';'                        { $$ = new EmptyStatementNode(); DBG($$, @1, @2); }
  | DEBUGGER error                      { $$ = new EmptyStatementNode(); DBG($$, @1, @1); AUTO_SEMICOLON; }
;

FunctionDeclaration:
    FUNCTION IDENT '(' ')' FunctionBody { $$ = new FuncDeclNode(*$2, $5); }
  | FUNCTION IDENT '(' FormalParameterList ')' FunctionBody
                                        { $$ = new FuncDeclNode(*$2, $4.head, $6); }
;

FunctionExpr:
    FUNCTION '(' ')' FunctionBody       { $$ = new FuncExprNode(CommonIdentifiers::shared()->nullIdentifier, $4); }
  | FUNCTION '(' FormalParameterList ')' FunctionBody
                                        { $$ = new FuncExprNode(CommonIdentifiers::shared()->nullIdentifier, $5, $3.head); }
  | FUNCTION IDENT '(' ')' FunctionBody { $$ = new FuncExprNode(*$2, $5); }
  | FUNCTION IDENT '(' FormalParameterList ')' FunctionBody
                                        { $$ = new FuncExprNode(*$2, $6, $4.head); }
;

FormalParameterList:
    IDENT                               { $$.head = new ParameterNode(*$1);
                                          $$.tail = $$.head; }
  | FormalParameterList ',' IDENT       { $$.head = $1.head;
                                          $$.tail = new ParameterNode($1.tail, *$3); }
;

FunctionBody:
    '{' '}' /* not in spec */           { $$ = new FunctionBodyNode(0); DBG($$, @1, @2); }
  | '{' SourceElements '}'              { $$ = new FunctionBodyNode($2); DBG($$, @1, @3); }
;

Program:
    /* not in spec */                   { Parser::accept(new ProgramNode(0)); }
    | SourceElements                    { Parser::accept(new ProgramNode($1)); }
;

SourceElements:
    SourceElement                       { $$ = new Vector<RefPtr<StatementNode> >();
                                          $$->append($1); }
  | SourceElements SourceElement        { $$->append($2); }
;

SourceElement:
    FunctionDeclaration                 { $$ = $1; }
  | Statement                           { $$ = $1; }
;
 
%%

static ExpressionNode* makeAssignNode(ExpressionNode* loc, Operator op, ExpressionNode* expr)
{
    if (!loc->isLocation())
        return new AssignErrorNode(loc, op, expr);

    if (loc->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(loc);
        if (op == OpEqual)
            return new AssignResolveNode(resolve->identifier(), expr);
        else
            return new ReadModifyResolveNode(resolve->identifier(), op, expr);
    }
    if (loc->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(loc);
        if (op == OpEqual)
            return new AssignBracketNode(bracket->base(), bracket->subscript(), expr);
        else
            return new ReadModifyBracketNode(bracket->base(), bracket->subscript(), op, expr);
    }
    ASSERT(loc->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(loc);
    if (op == OpEqual)
        return new AssignDotNode(dot->base(), dot->identifier(), expr);
    return new ReadModifyDotNode(dot->base(), dot->identifier(), op, expr);
}

static ExpressionNode* makePrefixNode(ExpressionNode* expr, Operator op)
{ 
    if (!expr->isLocation())
        return new PrefixErrorNode(expr, op);
    
    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        if (op == OpPlusPlus)
            return new PreIncResolveNode(resolve->identifier());
        else
            return new PreDecResolveNode(resolve->identifier());
    }
    if (expr->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(expr);
        if (op == OpPlusPlus)
            return new PreIncBracketNode(bracket->base(), bracket->subscript());
        else
            return new PreDecBracketNode(bracket->base(), bracket->subscript());
    }
    ASSERT(expr->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(expr);
    if (op == OpPlusPlus)
        return new PreIncDotNode(dot->base(), dot->identifier());
    return new PreDecDotNode(dot->base(), dot->identifier());
}

static ExpressionNode* makePostfixNode(ExpressionNode* expr, Operator op)
{ 
    if (!expr->isLocation())
        return new PostfixErrorNode(expr, op);
    
    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        if (op == OpPlusPlus)
            return new PostIncResolveNode(resolve->identifier());
        else
            return new PostDecResolveNode(resolve->identifier());
    }
    if (expr->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(expr);
        if (op == OpPlusPlus)
            return new PostIncBracketNode(bracket->base(), bracket->subscript());
        else
            return new PostDecBracketNode(bracket->base(), bracket->subscript());
    }
    ASSERT(expr->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(expr);
    
    if (op == OpPlusPlus)
        return new PostIncDotNode(dot->base(), dot->identifier());
    return new PostDecDotNode(dot->base(), dot->identifier());
}

static ExpressionNode* makeFunctionCallNode(ExpressionNode* func, ArgumentsNode* args)
{
    if (!func->isLocation())
        return new FunctionCallValueNode(func, args);
    if (func->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(func);
        return new FunctionCallResolveNode(resolve->identifier(), args);
    }
    if (func->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(func);
        return new FunctionCallBracketNode(bracket->base(), bracket->subscript(), args);
    }
    ASSERT(func->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(func);
    return new FunctionCallDotNode(dot->base(), dot->identifier(), args);
}

static ExpressionNode* makeTypeOfNode(ExpressionNode* expr)
{
    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        return new TypeOfResolveNode(resolve->identifier());
    }
    return new TypeOfValueNode(expr);
}

static ExpressionNode* makeDeleteNode(ExpressionNode* expr)
{
    if (!expr->isLocation())
        return new DeleteValueNode(expr);
    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        return new DeleteResolveNode(resolve->identifier());
    }
    if (expr->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(expr);
        return new DeleteBracketNode(bracket->base(), bracket->subscript());
    }
    ASSERT(expr->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(expr);
    return new DeleteDotNode(dot->base(), dot->identifier());
}

static PropertyNode* makeGetterOrSetterPropertyNode(const Identifier& getOrSet, const Identifier& name, ParameterNode* params, FunctionBodyNode* body)
{
    PropertyNode::Type type;
    if (getOrSet == "get")
        type = PropertyNode::Getter;
    else if (getOrSet == "set")
        type = PropertyNode::Setter;
    else
        return 0;
    return new PropertyNode(name, new FuncExprNode(CommonIdentifiers::shared()->nullIdentifier, body, params), type);
}

static ExpressionNode* makeNegateNode(ExpressionNode* n)
{
    if (n->isNumber()) {
        NumberNode* number = static_cast<NumberNode*>(n);

        if (number->value() > 0.0) {
            number->setValue(-number->value());
            return number;
        }
    }

    return new NegateNode(n);
}

static NumberNode* makeNumberNode(double d)
{
    JSValue* value = JSImmediate::from(d);
    if (value)
        return new ImmediateNumberNode(value, d);
    return new NumberNode(d);
}

/* called by yyparse on error */
int yyerror(const char *)
{
    return 1;
}

/* may we automatically insert a semicolon ? */
static bool allowAutomaticSemicolon()
{
    return yychar == '}' || yychar == 0 || Lexer::curr()->prevTerminator();
}
