%{

/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
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
 *  Foundation, Inc., 51 Franklin Steet, Fifth Floor, Boston, MA  02110-1301  USA
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

// Not sure why, but yacc doesn't add this define along with the others.
#define yylloc kjsyylloc

/* default values for bison */
#define YYDEBUG 0
#if !__APPLE__ /* work around the fact that YYERROR_VERBOSE causes a compiler warning in bison code */
#define YYERROR_VERBOSE
#endif

extern int kjsyylex();
int kjsyyerror(const char *);
static bool allowAutomaticSemicolon();

#define AUTO_SEMICOLON do { if (!allowAutomaticSemicolon()) YYABORT; } while (0)
#define DBG(l, s, e) (l)->setLoc((s).first_line, (e).last_line, Parser::sid)

using namespace KJS;

static bool makeAssignNode(Node*& result, Node *loc, Operator op, Node *expr);
static bool makePrefixNode(Node*& result, Node *expr, Operator op);
static bool makePostfixNode(Node*& result, Node *expr, Operator op);
static bool makeGetterOrSetterPropertyNode(PropertyNode*& result, Identifier &getOrSet, Identifier& name, ParameterNode *params, FunctionBodyNode *body);
static Node *makeFunctionCallNode(Node *func, ArgumentsNode *args);
static Node *makeTypeOfNode(Node *expr);
static Node *makeDeleteNode(Node *expr);

%}

%union {
  int                 ival;
  double              dval;
  UString             *ustr;
  Identifier          *ident;
  Node                *node;
  StatementNode       *stat;
  ParameterNode       *param;
  FunctionBodyNode    *body;
  FuncDeclNode        *func;
  FuncExprNode        *funcExpr;
  ProgramNode         *prog;
  AssignExprNode      *init;
  SourceElementsNode  *srcs;
  StatListNode        *slist;
  ArgumentsNode       *args;
  ArgumentListNode    *alist;
  VarDeclNode         *decl;
  VarDeclListNode     *vlist;
  CaseBlockNode       *cblk;
  ClauseListNode      *clist;
  CaseClauseNode      *ccl;
  ElementNode         *elm;
  Operator            op;
  PropertyListNode   *plist;
  PropertyNode       *pnode;
  PropertyNameNode   *pname;
}

%start Program

/* literals */
%token NULLTOKEN TRUETOKEN FALSETOKEN
%token STRING NUMBER

/* keywords */
%token BREAK CASE DEFAULT FOR NEW VAR CONST CONTINUE
%token FUNCTION RETURN VOID DELETE
%token IF THIS DO WHILE IN INSTANCEOF TYPEOF
%token SWITCH WITH RESERVED
%token THROW TRY CATCH FINALLY

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
%type <stat>  SourceElement

%type <slist> StatementList
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
%type <clist> CaseClauses  CaseClausesOpt
%type <ival>  Elision ElisionOpt
%type <elm>   ElementList
%type <pname> PropertyName
%type <pnode> Property
%type <plist> PropertyList
%%

Literal:
    NULLTOKEN                           { $$ = new NullNode(); }
  | TRUETOKEN                           { $$ = new BooleanNode(true); }
  | FALSETOKEN                          { $$ = new BooleanNode(false); }
  | NUMBER                              { $$ = new NumberNode($1); }
  | STRING                              { $$ = new StringNode($1); }
  | '/' /* regexp */                    {
                                            Lexer *l = Lexer::curr();
                                            if (!l->scanRegExp()) YYABORT;
                                            $$ = new RegExpNode(l->pattern, l->flags);
                                        }
  | DIVEQUAL /* regexp with /= */       {
                                            Lexer *l = Lexer::curr();
                                            if (!l->scanRegExp()) YYABORT;
                                            $$ = new RegExpNode(UString('=') + l->pattern, l->flags);
                                        }
;

PropertyName:
    IDENT                               { $$ = new PropertyNameNode(*$1); }
  | STRING                              { $$ = new PropertyNameNode(Identifier(*$1)); }
  | NUMBER                              { $$ = new PropertyNameNode($1); }
;

Property:
    PropertyName ':' AssignmentExpr     { $$ = new PropertyNode($1, $3, PropertyNode::Constant); }
  | IDENT IDENT '(' ')' FunctionBody    { if (!makeGetterOrSetterPropertyNode($$, *$1, *$2, 0, $5)) YYABORT; }
  | IDENT IDENT '(' FormalParameterList ')' FunctionBody
                                        { if (!makeGetterOrSetterPropertyNode($$, *$1, *$2, $4, $6)) YYABORT; }
;

PropertyList:
    Property                            { $$ = new PropertyListNode($1); }
  | PropertyList ',' Property           { $$ = new PropertyListNode($3, $1); }
;

PrimaryExpr:
    PrimaryExprNoBrace
  | '{' '}'                             { $$ = new ObjectLiteralNode(); }
  | '{' PropertyList '}'                { $$ = new ObjectLiteralNode($2); }
  /* allow extra comma, see http://bugzilla.opendarwin.org/show_bug.cgi?id=5939 */
  | '{' PropertyList ',' '}'            { $$ = new ObjectLiteralNode($2); }
;

PrimaryExprNoBrace:
    THIS                                { $$ = new ThisNode(); }
  | Literal
  | ArrayLiteral
  | IDENT                               { $$ = new ResolveNode(*$1); }
  | '(' Expr ')'                        { $$ = $2->isResolveNode() ? $2 : new GroupNode($2); }
;

ArrayLiteral:
    '[' ElisionOpt ']'                  { $$ = new ArrayNode($2); }
  | '[' ElementList ']'                 { $$ = new ArrayNode($2); }
  | '[' ElementList ',' ElisionOpt ']'  { $$ = new ArrayNode($4, $2); }
;

ElementList:
    ElisionOpt AssignmentExpr           { $$ = new ElementNode($1, $2); }
  | ElementList ',' ElisionOpt AssignmentExpr
                                        { $$ = new ElementNode($1, $3, $4); }
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
  | '(' ArgumentList ')'                { $$ = new ArgumentsNode($2); }
;

ArgumentList:
    AssignmentExpr                      { $$ = new ArgumentListNode($1); }
  | ArgumentList ',' AssignmentExpr     { $$ = new ArgumentListNode($1, $3); }
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
  | LeftHandSideExpr PLUSPLUS           { if (!makePostfixNode($$, $1, OpPlusPlus)) YYABORT; }
  | LeftHandSideExpr MINUSMINUS         { if (!makePostfixNode($$, $1, OpMinusMinus)) YYABORT; }
;

PostfixExprNoBF:
    LeftHandSideExprNoBF
  | LeftHandSideExprNoBF PLUSPLUS       { if (!makePostfixNode($$, $1, OpPlusPlus)) YYABORT; }
  | LeftHandSideExprNoBF MINUSMINUS     { if (!makePostfixNode($$, $1, OpMinusMinus)) YYABORT; }
;

UnaryExprCommon:
    DELETE UnaryExpr                    { $$ = makeDeleteNode($2); }
  | VOID UnaryExpr                      { $$ = new VoidNode($2); }
  | TYPEOF UnaryExpr                    { $$ = makeTypeOfNode($2); }
  | PLUSPLUS UnaryExpr                  { if (!makePrefixNode($$, $2, OpPlusPlus)) YYABORT; }
  | AUTOPLUSPLUS UnaryExpr              { if (!makePrefixNode($$, $2, OpPlusPlus)) YYABORT; }
  | MINUSMINUS UnaryExpr                { if (!makePrefixNode($$, $2, OpMinusMinus)) YYABORT; }
  | AUTOMINUSMINUS UnaryExpr            { if (!makePrefixNode($$, $2, OpMinusMinus)) YYABORT; }
  | '+' UnaryExpr                       { $$ = new UnaryPlusNode($2); }
  | '-' UnaryExpr                       { $$ = new NegateNode($2); }
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
  | MultiplicativeExpr '*' UnaryExpr    { $$ = new MultNode($1, $3, '*'); }
  | MultiplicativeExpr '/' UnaryExpr    { $$ = new MultNode($1, $3, '/'); }
  | MultiplicativeExpr '%' UnaryExpr    { $$ = new MultNode($1, $3,'%'); }
;

MultiplicativeExprNoBF:
    UnaryExprNoBF
  | MultiplicativeExprNoBF '*' UnaryExpr
                                        { $$ = new MultNode($1, $3, '*'); }
  | MultiplicativeExprNoBF '/' UnaryExpr
                                        { $$ = new MultNode($1, $3, '/'); }
  | MultiplicativeExprNoBF '%' UnaryExpr
                                        { $$ = new MultNode($1, $3,'%'); }
;

AdditiveExpr:
    MultiplicativeExpr
  | AdditiveExpr '+' MultiplicativeExpr { $$ = new AddNode($1, $3, '+'); }
  | AdditiveExpr '-' MultiplicativeExpr { $$ = new AddNode($1, $3, '-'); }
;

AdditiveExprNoBF:
    MultiplicativeExprNoBF
  | AdditiveExprNoBF '+' MultiplicativeExpr
                                        { $$ = new AddNode($1, $3, '+'); }
  | AdditiveExprNoBF '-' MultiplicativeExpr
                                        { $$ = new AddNode($1, $3, '-'); }
;

ShiftExpr:
    AdditiveExpr
  | ShiftExpr LSHIFT AdditiveExpr       { $$ = new ShiftNode($1, OpLShift, $3); }
  | ShiftExpr RSHIFT AdditiveExpr       { $$ = new ShiftNode($1, OpRShift, $3); }
  | ShiftExpr URSHIFT AdditiveExpr      { $$ = new ShiftNode($1, OpURShift, $3); }
;

ShiftExprNoBF:
    AdditiveExprNoBF
  | ShiftExprNoBF LSHIFT AdditiveExpr   { $$ = new ShiftNode($1, OpLShift, $3); }
  | ShiftExprNoBF RSHIFT AdditiveExpr   { $$ = new ShiftNode($1, OpRShift, $3); }
  | ShiftExprNoBF URSHIFT AdditiveExpr  { $$ = new ShiftNode($1, OpURShift, $3); }
;

RelationalExpr:
    ShiftExpr
  | RelationalExpr '<' ShiftExpr        { $$ = new RelationalNode($1, OpLess, $3); }
  | RelationalExpr '>' ShiftExpr        { $$ = new RelationalNode($1, OpGreater, $3); }
  | RelationalExpr LE ShiftExpr         { $$ = new RelationalNode($1, OpLessEq, $3); }
  | RelationalExpr GE ShiftExpr         { $$ = new RelationalNode($1, OpGreaterEq, $3); }
  | RelationalExpr INSTANCEOF ShiftExpr { $$ = new RelationalNode($1, OpInstanceOf, $3); }
  | RelationalExpr IN ShiftExpr         { $$ = new RelationalNode($1, OpIn, $3); }
;

RelationalExprNoIn:
    ShiftExpr
  | RelationalExprNoIn '<' ShiftExpr    { $$ = new RelationalNode($1, OpLess, $3); }
  | RelationalExprNoIn '>' ShiftExpr    { $$ = new RelationalNode($1, OpGreater, $3); }
  | RelationalExprNoIn LE ShiftExpr     { $$ = new RelationalNode($1, OpLessEq, $3); }
  | RelationalExprNoIn GE ShiftExpr     { $$ = new RelationalNode($1, OpGreaterEq, $3); }
  | RelationalExprNoIn INSTANCEOF ShiftExpr
                                        { $$ = new RelationalNode($1, OpInstanceOf, $3); }
;

RelationalExprNoBF:
    ShiftExprNoBF
  | RelationalExprNoBF '<' ShiftExpr    { $$ = new RelationalNode($1, OpLess, $3); }
  | RelationalExprNoBF '>' ShiftExpr    { $$ = new RelationalNode($1, OpGreater, $3); }
  | RelationalExprNoBF LE ShiftExpr     { $$ = new RelationalNode($1, OpLessEq, $3); }
  | RelationalExprNoBF GE ShiftExpr     { $$ = new RelationalNode($1, OpGreaterEq, $3); }
  | RelationalExprNoBF INSTANCEOF ShiftExpr
                                        { $$ = new RelationalNode($1, OpInstanceOf, $3); }
  | RelationalExprNoBF IN ShiftExpr     { $$ = new RelationalNode($1, OpIn, $3); }
;

EqualityExpr:
    RelationalExpr
  | EqualityExpr EQEQ RelationalExpr    { $$ = new EqualNode($1, OpEqEq, $3); }
  | EqualityExpr NE RelationalExpr      { $$ = new EqualNode($1, OpNotEq, $3); }
  | EqualityExpr STREQ RelationalExpr   { $$ = new EqualNode($1, OpStrEq, $3); }
  | EqualityExpr STRNEQ RelationalExpr  { $$ = new EqualNode($1, OpStrNEq, $3);}
;

EqualityExprNoIn:
    RelationalExprNoIn
  | EqualityExprNoIn EQEQ RelationalExprNoIn
                                        { $$ = new EqualNode($1, OpEqEq, $3); }
  | EqualityExprNoIn NE RelationalExprNoIn
                                        { $$ = new EqualNode($1, OpNotEq, $3); }
  | EqualityExprNoIn STREQ RelationalExprNoIn
                                        { $$ = new EqualNode($1, OpStrEq, $3); }
  | EqualityExprNoIn STRNEQ RelationalExprNoIn
                                        { $$ = new EqualNode($1, OpStrNEq, $3);}
;

EqualityExprNoBF:
    RelationalExprNoBF
  | EqualityExprNoBF EQEQ RelationalExpr
                                        { $$ = new EqualNode($1, OpEqEq, $3); }
  | EqualityExprNoBF NE RelationalExpr  { $$ = new EqualNode($1, OpNotEq, $3); }
  | EqualityExprNoBF STREQ RelationalExpr
                                        { $$ = new EqualNode($1, OpStrEq, $3); }
  | EqualityExprNoBF STRNEQ RelationalExpr
                                        { $$ = new EqualNode($1, OpStrNEq, $3);}
;

BitwiseANDExpr:
    EqualityExpr
  | BitwiseANDExpr '&' EqualityExpr     { $$ = new BitOperNode($1, OpBitAnd, $3); }
;

BitwiseANDExprNoIn:
    EqualityExprNoIn
  | BitwiseANDExprNoIn '&' EqualityExprNoIn
                                        { $$ = new BitOperNode($1, OpBitAnd, $3); }
;

BitwiseANDExprNoBF:
    EqualityExprNoBF
  | BitwiseANDExprNoBF '&' EqualityExpr { $$ = new BitOperNode($1, OpBitAnd, $3); }
;

BitwiseXORExpr:
    BitwiseANDExpr
  | BitwiseXORExpr '^' BitwiseANDExpr   { $$ = new BitOperNode($1, OpBitXOr, $3); }
;

BitwiseXORExprNoIn:
    BitwiseANDExprNoIn
  | BitwiseXORExprNoIn '^' BitwiseANDExprNoIn
                                        { $$ = new BitOperNode($1, OpBitXOr, $3); }
;

BitwiseXORExprNoBF:
    BitwiseANDExprNoBF
  | BitwiseXORExprNoBF '^' BitwiseANDExpr
                                        { $$ = new BitOperNode($1, OpBitXOr, $3); }
;

BitwiseORExpr:
    BitwiseXORExpr
  | BitwiseORExpr '|' BitwiseXORExpr    { $$ = new BitOperNode($1, OpBitOr, $3); }
;

BitwiseORExprNoIn:
    BitwiseXORExprNoIn
  | BitwiseORExprNoIn '|' BitwiseXORExprNoIn
                                        { $$ = new BitOperNode($1, OpBitOr, $3); }
;

BitwiseORExprNoBF:
    BitwiseXORExprNoBF
  | BitwiseORExprNoBF '|' BitwiseXORExpr
                                        { $$ = new BitOperNode($1, OpBitOr, $3); }
;

LogicalANDExpr:
    BitwiseORExpr
  | LogicalANDExpr AND BitwiseORExpr    { $$ = new BinaryLogicalNode($1, OpAnd, $3); }
;

LogicalANDExprNoIn:
    BitwiseORExprNoIn
  | LogicalANDExprNoIn AND BitwiseORExprNoIn
                                        { $$ = new BinaryLogicalNode($1, OpAnd, $3); }
;

LogicalANDExprNoBF:
    BitwiseORExprNoBF
  | LogicalANDExprNoBF AND BitwiseORExpr
                                        { $$ = new BinaryLogicalNode($1, OpAnd, $3); }
;

LogicalORExpr:
    LogicalANDExpr
  | LogicalORExpr OR LogicalANDExpr     { $$ = new BinaryLogicalNode($1, OpOr, $3); }
;

LogicalORExprNoIn:
    LogicalANDExprNoIn
  | LogicalORExprNoIn OR LogicalANDExprNoIn
                                        { $$ = new BinaryLogicalNode($1, OpOr, $3); }
;

LogicalORExprNoBF:
    LogicalANDExprNoBF
  | LogicalORExprNoBF OR LogicalANDExpr { $$ = new BinaryLogicalNode($1, OpOr, $3); }
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
                                        { if (!makeAssignNode($$, $1, $2, $3)) YYABORT; }
;

AssignmentExprNoIn:
    ConditionalExprNoIn
  | LeftHandSideExpr AssignmentOperator AssignmentExprNoIn
                                        { if (!makeAssignNode($$, $1, $2, $3)) YYABORT; }
;

AssignmentExprNoBF:
    ConditionalExprNoBF
  | LeftHandSideExprNoBF AssignmentOperator AssignmentExpr
                                        { if (!makeAssignNode($$, $1, $2, $3)) YYABORT; }
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
;

Block:
    '{' '}'                             { $$ = new BlockNode(0); DBG($$, @2, @2); }
  | '{' SourceElements '}'              { $$ = new BlockNode($2); DBG($$, @3, @3); }
;

StatementList:
    Statement                           { $$ = new StatListNode($1); }
  | StatementList Statement             { $$ = new StatListNode($1, $2); }
;

VariableStatement:
    VAR VariableDeclarationList ';'     { $$ = new VarStatementNode($2); DBG($$, @1, @3); }
  | VAR VariableDeclarationList error   { $$ = new VarStatementNode($2); DBG($$, @1, @2); AUTO_SEMICOLON; }
;

VariableDeclarationList:
    VariableDeclaration                 { $$ = new VarDeclListNode($1); }
  | VariableDeclarationList ',' VariableDeclaration
                                        { $$ = new VarDeclListNode($1, $3); }
;

VariableDeclarationListNoIn:
    VariableDeclarationNoIn             { $$ = new VarDeclListNode($1); }
  | VariableDeclarationListNoIn ',' VariableDeclarationNoIn
                                        { $$ = new VarDeclListNode($1, $3); }
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
    CONST ConstDeclarationList ';'      { $$ = new VarStatementNode($2); DBG($$, @1, @3); }
  | CONST ConstDeclarationList error    { $$ = new VarStatementNode($2); DBG($$, @1, @2); AUTO_SEMICOLON; }
;

ConstDeclarationList:
    ConstDeclaration                    { $$ = new VarDeclListNode($1); }
  | ConstDeclarationList ',' ConstDeclaration
                                        { $$ = new VarDeclListNode($1, $3); }
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
    DO Statement WHILE '(' Expr ')'     { $$ = new DoWhileNode($2, $5); DBG($$, @1, @3);}
  | WHILE '(' Expr ')' Statement        { $$ = new WhileNode($3, $5); DBG($$, @1, @4); }
  | FOR '(' ExprNoInOpt ';' ExprOpt ';' ExprOpt ')' Statement
                                        { $$ = new ForNode($3, $5, $7, $9); DBG($$, @1, @8); }
  | FOR '(' VAR VariableDeclarationListNoIn ';' ExprOpt ';' ExprOpt ')' Statement
                                        { $$ = new ForNode($4, $6, $8, $10); DBG($$, @1, @9); }
  | FOR '(' LeftHandSideExpr IN Expr ')' Statement
                                        {
                                            Node *n = $3->nodeInsideAllParens();
                                            if (!n->isLocation())
                                                YYABORT;
                                            $$ = new ForInNode(n, $5, $7);
                                            DBG($$, @1, @6);
                                        }
  | FOR '(' VAR IDENT IN Expr ')' Statement
                                        { $$ = new ForInNode(*$4, 0, $6, $8); DBG($$, @1, @7); }
  | FOR '(' VAR IDENT InitializerNoIn IN Expr ')' Statement
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
    '{' CaseClausesOpt '}'              { $$ = new CaseBlockNode($2, 0, 0); }
  | '{' CaseClausesOpt DefaultClause CaseClausesOpt '}'
                                        { $$ = new CaseBlockNode($2, $3, $4); }
;

CaseClausesOpt:
    /* nothing */                       { $$ = 0; }
  | CaseClauses
;

CaseClauses:
    CaseClause                          { $$ = new ClauseListNode($1); }
  | CaseClauses CaseClause              { $$ = new ClauseListNode($1, $2); }
;

CaseClause:
    CASE Expr ':'                       { $$ = new CaseClauseNode($2); }
  | CASE Expr ':' StatementList         { $$ = new CaseClauseNode($2, $4); }
;

DefaultClause:
    DEFAULT ':'                         { $$ = new CaseClauseNode(0); }
  | DEFAULT ':' StatementList           { $$ = new CaseClauseNode(0, $3); }
;

LabelledStatement:
    IDENT ':' Statement                 { $3->pushLabel(*$1); $$ = new LabelNode(*$1, $3); }
;

ThrowStatement:
    THROW Expr ';'                      { $$ = new ThrowNode($2); DBG($$, @1, @3); }
  | THROW Expr error                    { $$ = new ThrowNode($2); DBG($$, @1, @2); AUTO_SEMICOLON; }
;

TryStatement:
    TRY Block FINALLY Block             { $$ = new TryNode($2, Identifier::null(), 0, $4); DBG($$, @1, @2); }
  | TRY Block CATCH '(' IDENT ')' Block { $$ = new TryNode($2, *$5, $7, 0); DBG($$, @1, @2); }
  | TRY Block CATCH '(' IDENT ')' Block FINALLY Block
                                        { $$ = new TryNode($2, *$5, $7, $9); DBG($$, @1, @2); }
;

FunctionDeclaration:
    FUNCTION IDENT '(' ')' FunctionBody { $$ = new FuncDeclNode(*$2, $5); }
  | FUNCTION IDENT '(' FormalParameterList ')' FunctionBody
                                        { $$ = new FuncDeclNode(*$2, $4, $6); }
;

FunctionExpr:
    FUNCTION '(' ')' FunctionBody       { $$ = new FuncExprNode(Identifier::null(), $4); }
  | FUNCTION '(' FormalParameterList ')' FunctionBody
                                        { $$ = new FuncExprNode(Identifier::null(), $5, $3); }
  | FUNCTION IDENT '(' ')' FunctionBody { $$ = new FuncExprNode(*$2, $5); }
  | FUNCTION IDENT '(' FormalParameterList ')' FunctionBody
                                        { $$ = new FuncExprNode(*$2, $6, $4); }
;

FormalParameterList:
    IDENT                               { $$ = new ParameterNode(*$1); }
  | FormalParameterList ',' IDENT       { $$ = new ParameterNode($1, *$3); }
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
    SourceElement                       { $$ = new SourceElementsNode($1); }
  | SourceElements SourceElement        { $$ = new SourceElementsNode($1, $2); }
;

SourceElement:
    FunctionDeclaration                 { $$ = $1; }
  | Statement                           { $$ = $1; }
;
 
%%

static bool makeAssignNode(Node*& result, Node *loc, Operator op, Node *expr)
{ 
    Node *n = loc->nodeInsideAllParens();

    if (!n->isLocation())
        return false;

    if (n->isResolveNode()) {
        ResolveNode *resolve = static_cast<ResolveNode *>(n);
        result = new AssignResolveNode(resolve->identifier(), op, expr);
    } else if (n->isBracketAccessorNode()) {
        BracketAccessorNode *bracket = static_cast<BracketAccessorNode *>(n);
        result = new AssignBracketNode(bracket->base(), bracket->subscript(), op, expr);
    } else {
        assert(n->isDotAccessorNode());
        DotAccessorNode *dot = static_cast<DotAccessorNode *>(n);
        result = new AssignDotNode(dot->base(), dot->identifier(), op, expr);
    }

    return true;
}

static bool makePrefixNode(Node*& result, Node *expr, Operator op)
{ 
    Node *n = expr->nodeInsideAllParens();

    if (!n->isLocation())
        return false;
    
    if (n->isResolveNode()) {
        ResolveNode *resolve = static_cast<ResolveNode *>(n);
        result = new PrefixResolveNode(resolve->identifier(), op);
    } else if (n->isBracketAccessorNode()) {
        BracketAccessorNode *bracket = static_cast<BracketAccessorNode *>(n);
        result = new PrefixBracketNode(bracket->base(), bracket->subscript(), op);
    } else {
        assert(n->isDotAccessorNode());
        DotAccessorNode *dot = static_cast<DotAccessorNode *>(n);
        result = new PrefixDotNode(dot->base(), dot->identifier(), op);
    }

    return true;
}

static bool makePostfixNode(Node*& result, Node *expr, Operator op)
{ 
    Node *n = expr->nodeInsideAllParens();

    if (!n->isLocation())
        return false;
    
    if (n->isResolveNode()) {
        ResolveNode *resolve = static_cast<ResolveNode *>(n);
        result = new PostfixResolveNode(resolve->identifier(), op);
    } else if (n->isBracketAccessorNode()) {
        BracketAccessorNode *bracket = static_cast<BracketAccessorNode *>(n);
        result = new PostfixBracketNode(bracket->base(), bracket->subscript(), op);
    } else {
        assert(n->isDotAccessorNode());
        DotAccessorNode *dot = static_cast<DotAccessorNode *>(n);
        result = new PostfixDotNode(dot->base(), dot->identifier(), op);
    }

    return true;
}

static Node *makeFunctionCallNode(Node *func, ArgumentsNode *args)
{
    Node *n = func->nodeInsideAllParens();
    
    if (!n->isLocation())
        return new FunctionCallValueNode(func, args);
    else if (n->isResolveNode()) {
        ResolveNode *resolve = static_cast<ResolveNode *>(n);
        return new FunctionCallResolveNode(resolve->identifier(), args);
    } else if (n->isBracketAccessorNode()) {
        BracketAccessorNode *bracket = static_cast<BracketAccessorNode *>(n);
        if (n != func)
            return new FunctionCallParenBracketNode(bracket->base(), bracket->subscript(), args);
        else
            return new FunctionCallBracketNode(bracket->base(), bracket->subscript(), args);
    } else {
        assert(n->isDotAccessorNode());
        DotAccessorNode *dot = static_cast<DotAccessorNode *>(n);
        if (n != func)
            return new FunctionCallParenDotNode(dot->base(), dot->identifier(), args);
        else
            return new FunctionCallDotNode(dot->base(), dot->identifier(), args);
    }
}

static Node *makeTypeOfNode(Node *expr)
{
    Node *n = expr->nodeInsideAllParens();

    if (n->isResolveNode()) {
        ResolveNode *resolve = static_cast<ResolveNode *>(n);
        return new TypeOfResolveNode(resolve->identifier());
    } else
        return new TypeOfValueNode(n);
}

static Node *makeDeleteNode(Node *expr)
{
    Node *n = expr->nodeInsideAllParens();
    
    if (!n->isLocation())
        return new DeleteValueNode(expr);
    else if (n->isResolveNode()) {
        ResolveNode *resolve = static_cast<ResolveNode *>(n);
        return new DeleteResolveNode(resolve->identifier());
    } else if (n->isBracketAccessorNode()) {
        BracketAccessorNode *bracket = static_cast<BracketAccessorNode *>(n);
        return new DeleteBracketNode(bracket->base(), bracket->subscript());
    } else {
        assert(n->isDotAccessorNode());
        DotAccessorNode *dot = static_cast<DotAccessorNode *>(n);
        return new DeleteDotNode(dot->base(), dot->identifier());
    }
}

static bool makeGetterOrSetterPropertyNode(PropertyNode*& result, Identifier& getOrSet, Identifier& name, ParameterNode *params, FunctionBodyNode *body)
{
    PropertyNode::Type type;
    
    if (getOrSet == "get")
        type = PropertyNode::Getter;
    else if (getOrSet == "set")
        type = PropertyNode::Setter;
    else
        return false;
    
    result = new PropertyNode(new PropertyNameNode(name), 
                              new FuncExprNode(Identifier::null(), body, params), type);

    return true;
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
