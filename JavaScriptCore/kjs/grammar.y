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
#include "nodes.h"
#include "lexer.h"
#include "internal.h"
#include "CommonIdentifiers.h"
#include "NodeInfo.h"
#include "Parser.h"
#include <wtf/MathExtras.h>

// Not sure why, but yacc doesn't add this define along with the others.
#define yylloc kjsyylloc

#define YYMAXDEPTH 10000
#define YYENABLE_NLS 0

/* default values for bison */
#define YYDEBUG 0 // Set to 1 to debug a parse error.
#define kjsyydebug 0 // Set to 1 to debug a parse error.
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
using namespace std;

static AddNode* makeAddNode(ExpressionNode*, ExpressionNode*);
static LessNode* makeLessNode(ExpressionNode*, ExpressionNode*);
static ExpressionNode* makeAssignNode(ExpressionNode* loc, Operator, ExpressionNode* expr);
static ExpressionNode* makePrefixNode(ExpressionNode* expr, Operator);
static ExpressionNode* makePostfixNode(ExpressionNode* expr, Operator);
static PropertyNode* makeGetterOrSetterPropertyNode(const Identifier &getOrSet, const Identifier& name, ParameterNode*, FunctionBodyNode*);
static ExpressionNode* makeFunctionCallNode(ExpressionNode* func, ArgumentsNode*);
static ExpressionNode* makeTypeOfNode(ExpressionNode*);
static ExpressionNode* makeDeleteNode(ExpressionNode*);
static ExpressionNode* makeNegateNode(ExpressionNode*);
static NumberNode* makeNumberNode(double);
static StatementNode* makeVarStatementNode(ExpressionNode*);
static ExpressionNode* combineVarInitializers(ExpressionNode* list, AssignResolveNode* init);


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

template <typename T> NodeInfo<T> createNodeInfo(T node, ParserRefCountedData<DeclarationStacks::VarStack>* varDecls, 
                                                 ParserRefCountedData<DeclarationStacks::FunctionStack>* funcDecls) 
{
    NodeInfo<T> result = {node, varDecls, funcDecls};
    return result;
}

template <typename T> T mergeDeclarationLists(T decls1, T decls2) 
{
    // decls1 or both are null
    if (!decls1)
        return decls2;
    // only decls1 is non-null
    if (!decls2)
        return decls1;

    // Both are non-null
    decls1->data.append(decls2->data);
    
    // We manually release the declaration lists to avoid accumulating many many
    // unused heap allocated vectors
    decls2->ref();
    decls2->deref();
    return decls1;
}

static void appendToVarDeclarationList(ParserRefCountedData<DeclarationStacks::VarStack>*& varDecls, const Identifier& ident, unsigned attrs)
{
    if (!varDecls)
        varDecls = new ParserRefCountedData<DeclarationStacks::VarStack>;

    varDecls->data.append(make_pair(ident, attrs));

}

static inline void appendToVarDeclarationList(ParserRefCountedData<DeclarationStacks::VarStack>*& varDecls, ConstDeclNode* decl)
{
    unsigned attrs = DeclarationStacks::IsConstant;
    if (decl->m_init)
        attrs |= DeclarationStacks::HasInitializer;        
    appendToVarDeclarationList(varDecls, decl->m_ident, attrs);
}

%}

%union {
    int                 intValue;
    double              doubleValue;
    UString*            string;
    Identifier*         ident;

    // expression subtrees
    ExpressionNode*     expressionNode;
    FuncDeclNode*       funcDeclNode;
    PropertyNode*       propertyNode;
    ArgumentsNode*      argumentsNode;
    ConstDeclNode*      constDeclNode;
    CaseBlockNodeInfo   caseBlockNode;
    CaseClauseNodeInfo  caseClauseNode;
    FuncExprNode*       funcExprNode;

    // statement nodes
    StatementNodeInfo   statementNode;
    FunctionBodyNode*   functionBodyNode;
    ProgramNode*        programNode;

    SourceElementsInfo  sourceElements;
    PropertyList        propertyList;
    ArgumentList        argumentList;
    VarDeclListInfo     varDeclList;
    ConstDeclListInfo   constDeclList;
    ClauseListInfo      clauseList;
    ElementList         elementList;
    ParameterList       parameterList;

    Operator            op;
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
%token <doubleValue> NUMBER
%token <string> STRING
%token <ident> IDENT

/* automatically inserted semicolon */
%token AUTOPLUSPLUS AUTOMINUSMINUS

/* non-terminal types */
%type <expressionNode>  Literal ArrayLiteral

%type <expressionNode>  PrimaryExpr PrimaryExprNoBrace
%type <expressionNode>  MemberExpr MemberExprNoBF /* BF => brace or function */
%type <expressionNode>  NewExpr NewExprNoBF
%type <expressionNode>  CallExpr CallExprNoBF
%type <expressionNode>  LeftHandSideExpr LeftHandSideExprNoBF
%type <expressionNode>  PostfixExpr PostfixExprNoBF
%type <expressionNode>  UnaryExpr UnaryExprNoBF UnaryExprCommon
%type <expressionNode>  MultiplicativeExpr MultiplicativeExprNoBF
%type <expressionNode>  AdditiveExpr AdditiveExprNoBF
%type <expressionNode>  ShiftExpr ShiftExprNoBF
%type <expressionNode>  RelationalExpr RelationalExprNoIn RelationalExprNoBF
%type <expressionNode>  EqualityExpr EqualityExprNoIn EqualityExprNoBF
%type <expressionNode>  BitwiseANDExpr BitwiseANDExprNoIn BitwiseANDExprNoBF
%type <expressionNode>  BitwiseXORExpr BitwiseXORExprNoIn BitwiseXORExprNoBF
%type <expressionNode>  BitwiseORExpr BitwiseORExprNoIn BitwiseORExprNoBF
%type <expressionNode>  LogicalANDExpr LogicalANDExprNoIn LogicalANDExprNoBF
%type <expressionNode>  LogicalORExpr LogicalORExprNoIn LogicalORExprNoBF
%type <expressionNode>  ConditionalExpr ConditionalExprNoIn ConditionalExprNoBF
%type <expressionNode>  AssignmentExpr AssignmentExprNoIn AssignmentExprNoBF
%type <expressionNode>  Expr ExprNoIn ExprNoBF

%type <expressionNode>  ExprOpt ExprNoInOpt

%type <statementNode>   Statement Block
%type <statementNode>   VariableStatement ConstStatement EmptyStatement ExprStatement
%type <statementNode>   IfStatement IterationStatement ContinueStatement
%type <statementNode>   BreakStatement ReturnStatement WithStatement
%type <statementNode>   SwitchStatement LabelledStatement
%type <statementNode>   ThrowStatement TryStatement
%type <statementNode>   DebuggerStatement
%type <statementNode>   SourceElement

%type <expressionNode>  Initializer InitializerNoIn
%type <funcDeclNode>    FunctionDeclaration
%type <funcExprNode>    FunctionExpr
%type <functionBodyNode>  FunctionBody
%type <sourceElements>  SourceElements
%type <parameterList>   FormalParameterList
%type <op>              AssignmentOperator
%type <argumentsNode>   Arguments
%type <argumentList>    ArgumentList
%type <varDeclList>     VariableDeclarationList VariableDeclarationListNoIn
%type <constDeclList>   ConstDeclarationList
%type <constDeclNode>   ConstDeclaration
%type <caseBlockNode>   CaseBlock
%type <caseClauseNode>  CaseClause DefaultClause
%type <clauseList>      CaseClauses CaseClausesOpt
%type <intValue>        Elision ElisionOpt
%type <elementList>     ElementList
%type <propertyNode>    Property
%type <propertyList>    PropertyList
%%

Literal:
    NULLTOKEN                           { $$ = new NullNode; }
  | TRUETOKEN                           { $$ = new TrueNode; }
  | FALSETOKEN                          { $$ = new FalseNode; }
  | NUMBER                              { $$ = makeNumberNode($1); }
  | STRING                              { $$ = new StringNode($1); }
  | '/' /* regexp */                    {
                                            Lexer& l = lexer();
                                            if (!l.scanRegExp())
                                                YYABORT;
                                            $$ = new RegExpNode(l.pattern(), l.flags());
                                        }
  | DIVEQUAL /* regexp with /= */       {
                                            Lexer& l = lexer();
                                            if (!l.scanRegExp())
                                                YYABORT;
                                            $$ = new RegExpNode("=" + l.pattern(), l.flags());
                                        }
;

Property:
    IDENT ':' AssignmentExpr            { $$ = new PropertyNode(*$1, $3, PropertyNode::Constant); }
  | STRING ':' AssignmentExpr           { $$ = new PropertyNode(Identifier(*$1), $3, PropertyNode::Constant); }
  | NUMBER ':' AssignmentExpr           { $$ = new PropertyNode(Identifier(UString::from($1)), $3, PropertyNode::Constant); }
  | IDENT IDENT '(' ')' '{' FunctionBody '}'    { $$ = makeGetterOrSetterPropertyNode(*$1, *$2, 0, $6); DBG($6, @5, @7); if (!$$) YYABORT; }
  | IDENT IDENT '(' FormalParameterList ')' '{' FunctionBody '}'
                                        { $$ = makeGetterOrSetterPropertyNode(*$1, *$2, $4.head, $7); DBG($7, @6, @8); if (!$$) YYABORT; }
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
  | AdditiveExpr '+' MultiplicativeExpr { $$ = makeAddNode($1, $3); }
  | AdditiveExpr '-' MultiplicativeExpr { $$ = new SubNode($1, $3); }
;

AdditiveExprNoBF:
    MultiplicativeExprNoBF
  | AdditiveExprNoBF '+' MultiplicativeExpr
                                        { $$ = makeAddNode($1, $3); }
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
  | RelationalExpr '<' ShiftExpr        { $$ = makeLessNode($1, $3); }
  | RelationalExpr '>' ShiftExpr        { $$ = new GreaterNode($1, $3); }
  | RelationalExpr LE ShiftExpr         { $$ = new LessEqNode($1, $3); }
  | RelationalExpr GE ShiftExpr         { $$ = new GreaterEqNode($1, $3); }
  | RelationalExpr INSTANCEOF ShiftExpr { $$ = new InstanceOfNode($1, $3); }
  | RelationalExpr INTOKEN ShiftExpr    { $$ = new InNode($1, $3); }
;

RelationalExprNoIn:
    ShiftExpr
  | RelationalExprNoIn '<' ShiftExpr    { $$ = makeLessNode($1, $3); }
  | RelationalExprNoIn '>' ShiftExpr    { $$ = new GreaterNode($1, $3); }
  | RelationalExprNoIn LE ShiftExpr     { $$ = new LessEqNode($1, $3); }
  | RelationalExprNoIn GE ShiftExpr     { $$ = new GreaterEqNode($1, $3); }
  | RelationalExprNoIn INSTANCEOF ShiftExpr
                                        { $$ = new InstanceOfNode($1, $3); }
;

RelationalExprNoBF:
    ShiftExprNoBF
  | RelationalExprNoBF '<' ShiftExpr    { $$ = makeLessNode($1, $3); }
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
    '{' '}'                             { $$ = createNodeInfo<StatementNode*>(new BlockNode(0), 0, 0);
                                          DBG($$.m_node, @1, @2); }
  | '{' SourceElements '}'              { $$ = createNodeInfo<StatementNode*>(new BlockNode($2.m_node), $2.m_varDeclarations, $2.m_funcDeclarations);
                                          DBG($$.m_node, @1, @3); }
;

VariableStatement:
    VAR VariableDeclarationList ';'     { $$ = createNodeInfo<StatementNode*>(makeVarStatementNode($2.m_node), $2.m_varDeclarations, $2.m_funcDeclarations);
                                          DBG($$.m_node, @1, @3); }
  | VAR VariableDeclarationList error   { $$ = createNodeInfo<StatementNode*>(makeVarStatementNode($2.m_node), $2.m_varDeclarations, $2.m_funcDeclarations);
                                          DBG($$.m_node, @1, @2);
                                          AUTO_SEMICOLON; }
;

VariableDeclarationList:
    IDENT                               { $$.m_node = 0;
                                          $$.m_varDeclarations = new ParserRefCountedData<DeclarationStacks::VarStack>;
                                          appendToVarDeclarationList($$.m_varDeclarations, *$1, 0);
                                          $$.m_funcDeclarations = 0;
                                        }
  | IDENT Initializer                   { $$.m_node = new AssignResolveNode(*$1, $2);
                                          $$.m_varDeclarations = new ParserRefCountedData<DeclarationStacks::VarStack>;
                                          appendToVarDeclarationList($$.m_varDeclarations, *$1, DeclarationStacks::HasInitializer);
                                          $$.m_funcDeclarations = 0;
                                        }
  | VariableDeclarationList ',' IDENT
                                        { $$.m_node = $1.m_node;
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          appendToVarDeclarationList($$.m_varDeclarations, *$3, 0);
                                          $$.m_funcDeclarations = 0;
                                        }
  | VariableDeclarationList ',' IDENT Initializer
                                        { $$.m_node = combineVarInitializers($1.m_node, new AssignResolveNode(*$3, $4));
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          appendToVarDeclarationList($$.m_varDeclarations, *$3, DeclarationStacks::HasInitializer);
                                          $$.m_funcDeclarations = 0;
                                        }
;

VariableDeclarationListNoIn:
    IDENT                               { $$.m_node = 0;
                                          $$.m_varDeclarations = new ParserRefCountedData<DeclarationStacks::VarStack>;
                                          appendToVarDeclarationList($$.m_varDeclarations, *$1, 0);
                                          $$.m_funcDeclarations = 0;
                                        }
  | IDENT InitializerNoIn               { $$.m_node = new AssignResolveNode(*$1, $2);
                                          $$.m_varDeclarations = new ParserRefCountedData<DeclarationStacks::VarStack>;
                                          appendToVarDeclarationList($$.m_varDeclarations, *$1, DeclarationStacks::HasInitializer);
                                          $$.m_funcDeclarations = 0;
                                        }
  | VariableDeclarationListNoIn ',' IDENT
                                        { $$.m_node = $1.m_node;
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          appendToVarDeclarationList($$.m_varDeclarations, *$3, 0);
                                          $$.m_funcDeclarations = 0;
                                        }
  | VariableDeclarationListNoIn ',' IDENT InitializerNoIn
                                        { $$.m_node = combineVarInitializers($1.m_node, new AssignResolveNode(*$3, $4));
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          appendToVarDeclarationList($$.m_varDeclarations, *$3, DeclarationStacks::HasInitializer);
                                          $$.m_funcDeclarations = 0;
                                        }
;

ConstStatement:
    CONSTTOKEN ConstDeclarationList ';' { $$ = createNodeInfo<StatementNode*>(new ConstStatementNode($2.m_node.head), $2.m_varDeclarations, $2.m_funcDeclarations);
                                          DBG($$.m_node, @1, @3); }
  | CONSTTOKEN ConstDeclarationList error
                                        { $$ = createNodeInfo<StatementNode*>(new ConstStatementNode($2.m_node.head), $2.m_varDeclarations, $2.m_funcDeclarations);
                                          DBG($$.m_node, @1, @2); AUTO_SEMICOLON; }
;

ConstDeclarationList:
    ConstDeclaration                    { $$.m_node.head = $1;
                                          $$.m_node.tail = $$.m_node.head;
                                          $$.m_varDeclarations = new ParserRefCountedData<DeclarationStacks::VarStack>;
                                          appendToVarDeclarationList($$.m_varDeclarations, $1);
                                          $$.m_funcDeclarations = 0; }
  | ConstDeclarationList ',' ConstDeclaration
                                        {  $$.m_node.head = $1.m_node.head;
                                          $1.m_node.tail->m_next = $3;
                                          $$.m_node.tail = $3;
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          appendToVarDeclarationList($$.m_varDeclarations, $3);
                                          $$.m_funcDeclarations = 0; }
;

ConstDeclaration:
    IDENT                               { $$ = new ConstDeclNode(*$1, 0); }
  | IDENT Initializer                   { $$ = new ConstDeclNode(*$1, $2); }
;

Initializer:
    '=' AssignmentExpr                  { $$ = $2; }
;

InitializerNoIn:
    '=' AssignmentExprNoIn              { $$ = $2; }
;

EmptyStatement:
    ';'                                 { $$ = createNodeInfo<StatementNode*>(new EmptyStatementNode(), 0, 0); }
;

ExprStatement:
    ExprNoBF ';'                        { $$ = createNodeInfo<StatementNode*>(new ExprStatementNode($1), 0, 0);
                                          DBG($$.m_node, @1, @2); }
  | ExprNoBF error                      { $$ = createNodeInfo<StatementNode*>(new ExprStatementNode($1), 0, 0);
                                          DBG($$.m_node, @1, @1); AUTO_SEMICOLON; }
;

IfStatement:
    IF '(' Expr ')' Statement %prec IF_WITHOUT_ELSE
                                        { $$ = createNodeInfo<StatementNode*>(new IfNode($3, $5.m_node), $5.m_varDeclarations, $5.m_funcDeclarations);
                                          DBG($$.m_node, @1, @4); }
  | IF '(' Expr ')' Statement ELSE Statement
                                        { $$ = createNodeInfo<StatementNode*>(new IfElseNode($3, $5.m_node, $7.m_node), mergeDeclarationLists($5.m_varDeclarations, $7.m_varDeclarations), mergeDeclarationLists($5.m_funcDeclarations, $7.m_funcDeclarations)); 
                                          DBG($$.m_node, @1, @4); }
;

IterationStatement:
    DO Statement WHILE '(' Expr ')' ';'    { $$ = createNodeInfo<StatementNode*>(new DoWhileNode($2.m_node, $5), $2.m_varDeclarations, $2.m_funcDeclarations);
                                             DBG($$.m_node, @1, @3); }
  | DO Statement WHILE '(' Expr ')' error  { $$ = createNodeInfo<StatementNode*>(new DoWhileNode($2.m_node, $5), $2.m_varDeclarations, $2.m_funcDeclarations);
                                             DBG($$.m_node, @1, @3); } // Always performs automatic semicolon insertion.
  | WHILE '(' Expr ')' Statement        { $$ = createNodeInfo<StatementNode*>(new WhileNode($3, $5.m_node), $5.m_varDeclarations, $5.m_funcDeclarations);
                                          DBG($$.m_node, @1, @4); }
  | FOR '(' ExprNoInOpt ';' ExprOpt ';' ExprOpt ')' Statement
                                        { $$ = createNodeInfo<StatementNode*>(new ForNode($3, $5, $7, $9.m_node, false), $9.m_varDeclarations, $9.m_funcDeclarations);
                                          DBG($$.m_node, @1, @8); 
                                        }
  | FOR '(' VAR VariableDeclarationListNoIn ';' ExprOpt ';' ExprOpt ')' Statement
                                                                            { $$ = createNodeInfo<StatementNode*>(new ForNode($4.m_node, $6, $8, $10.m_node, true),
                                                                              mergeDeclarationLists($4.m_varDeclarations, $10.m_varDeclarations),
                                                                              mergeDeclarationLists($4.m_funcDeclarations, $10.m_funcDeclarations));
                                          DBG($$.m_node, @1, @9); }
  | FOR '(' LeftHandSideExpr INTOKEN Expr ')' Statement
                                        {
                                            ExpressionNode* n = $3;
                                            if (!n->isLocation())
                                                YYABORT;
                                            $$ = createNodeInfo<StatementNode*>(new ForInNode(n, $5, $7.m_node), $7.m_varDeclarations, $7.m_funcDeclarations);
                                            DBG($$.m_node, @1, @6);
                                        }
  | FOR '(' VAR IDENT INTOKEN Expr ')' Statement
                                        { ForInNode *forIn = new ForInNode(*$4, 0, $6, $8.m_node);
                                          appendToVarDeclarationList($8.m_varDeclarations, *$4, DeclarationStacks::HasInitializer);
                                          $$ = createNodeInfo<StatementNode*>(forIn, $8.m_varDeclarations, $8.m_funcDeclarations);
                                          DBG($$.m_node, @1, @7); }
  | FOR '(' VAR IDENT InitializerNoIn INTOKEN Expr ')' Statement
                                        { ForInNode *forIn = new ForInNode(*$4, $5, $7, $9.m_node);
                                          appendToVarDeclarationList($9.m_varDeclarations, *$4, DeclarationStacks::HasInitializer);
                                          $$ = createNodeInfo<StatementNode*>(forIn, $9.m_varDeclarations, $9.m_funcDeclarations);
                                          DBG($$.m_node, @1, @8); }
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
    CONTINUE ';'                        { $$ = createNodeInfo<StatementNode*>(new ContinueNode(), 0, 0);
                                          DBG($$.m_node, @1, @2); }
  | CONTINUE error                      { $$ = createNodeInfo<StatementNode*>(new ContinueNode(), 0, 0);
                                          DBG($$.m_node, @1, @1); AUTO_SEMICOLON; }
  | CONTINUE IDENT ';'                  { $$ = createNodeInfo<StatementNode*>(new ContinueNode(*$2), 0, 0);
                                          DBG($$.m_node, @1, @3); }
  | CONTINUE IDENT error                { $$ = createNodeInfo<StatementNode*>(new ContinueNode(*$2), 0, 0);
                                          DBG($$.m_node, @1, @2); AUTO_SEMICOLON; }
;

BreakStatement:
    BREAK ';'                           { $$ = createNodeInfo<StatementNode*>(new BreakNode(), 0, 0); DBG($$.m_node, @1, @2); }
  | BREAK error                         { $$ = createNodeInfo<StatementNode*>(new BreakNode(), 0, 0); DBG($$.m_node, @1, @1); AUTO_SEMICOLON; }
  | BREAK IDENT ';'                     { $$ = createNodeInfo<StatementNode*>(new BreakNode(*$2), 0, 0); DBG($$.m_node, @1, @3); }
  | BREAK IDENT error                   { $$ = createNodeInfo<StatementNode*>(new BreakNode(*$2), 0, 0); DBG($$.m_node, @1, @2); AUTO_SEMICOLON; }
;

ReturnStatement:
    RETURN ';'                          { $$ = createNodeInfo<StatementNode*>(new ReturnNode(0), 0, 0); DBG($$.m_node, @1, @2); }
  | RETURN error                        { $$ = createNodeInfo<StatementNode*>(new ReturnNode(0), 0, 0); DBG($$.m_node, @1, @1); AUTO_SEMICOLON; }
  | RETURN Expr ';'                     { $$ = createNodeInfo<StatementNode*>(new ReturnNode($2), 0, 0); DBG($$.m_node, @1, @3); }
  | RETURN Expr error                   { $$ = createNodeInfo<StatementNode*>(new ReturnNode($2), 0, 0); DBG($$.m_node, @1, @2); AUTO_SEMICOLON; }
;

WithStatement:
    WITH '(' Expr ')' Statement         { $$ = createNodeInfo<StatementNode*>(new WithNode($3, $5.m_node), $5.m_varDeclarations, $5.m_funcDeclarations);
                                          DBG($$.m_node, @1, @4); }
;

SwitchStatement:
    SWITCH '(' Expr ')' CaseBlock       { $$ = createNodeInfo<StatementNode*>(new SwitchNode($3, $5.m_node), $5.m_varDeclarations, $5.m_funcDeclarations);
                                          DBG($$.m_node, @1, @4); }
;

CaseBlock:
    '{' CaseClausesOpt '}'              { $$ = createNodeInfo<CaseBlockNode*>(new CaseBlockNode($2.m_node.head, 0, 0), $2.m_varDeclarations, $2.m_funcDeclarations); }
  | '{' CaseClausesOpt DefaultClause CaseClausesOpt '}'
                                        { $$ = createNodeInfo<CaseBlockNode*>(new CaseBlockNode($2.m_node.head, $3.m_node, $4.m_node.head),
                                                                              mergeDeclarationLists(mergeDeclarationLists($2.m_varDeclarations, $3.m_varDeclarations), $4.m_varDeclarations),
                                                                              mergeDeclarationLists(mergeDeclarationLists($2.m_funcDeclarations, $3.m_funcDeclarations), $4.m_funcDeclarations)); }
;

CaseClausesOpt:
    /* nothing */                       { $$.m_node.head = 0; $$.m_node.tail = 0; $$.m_varDeclarations = 0; $$.m_funcDeclarations = 0; }
  | CaseClauses
;

CaseClauses:
    CaseClause                          { $$.m_node.head = new ClauseListNode($1.m_node);
                                          $$.m_node.tail = $$.m_node.head;
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          $$.m_funcDeclarations = $1.m_funcDeclarations; }
  | CaseClauses CaseClause              { $$.m_node.head = $1.m_node.head;
                                          $$.m_node.tail = new ClauseListNode($1.m_node.tail, $2.m_node);
                                          $$.m_varDeclarations = mergeDeclarationLists($1.m_varDeclarations, $2.m_varDeclarations);
                                          $$.m_funcDeclarations = mergeDeclarationLists($1.m_funcDeclarations, $2.m_funcDeclarations);
                                        }
;

CaseClause:
    CASE Expr ':'                       { $$ = createNodeInfo<CaseClauseNode*>(new CaseClauseNode($2), 0, 0); }
  | CASE Expr ':' SourceElements        { $$ = createNodeInfo<CaseClauseNode*>(new CaseClauseNode($2, $4.m_node), $4.m_varDeclarations, $4.m_funcDeclarations); }
;

DefaultClause:
    DEFAULT ':'                         { $$ = createNodeInfo<CaseClauseNode*>(new CaseClauseNode(0), 0, 0); }
  | DEFAULT ':' SourceElements          { $$ = createNodeInfo<CaseClauseNode*>(new CaseClauseNode(0, $3.m_node), $3.m_varDeclarations, $3.m_funcDeclarations); }
;

LabelledStatement:
    IDENT ':' Statement                 { $3.m_node->pushLabel(*$1);
                                          $$ = createNodeInfo<StatementNode*>(new LabelNode(*$1, $3.m_node), $3.m_varDeclarations, $3.m_funcDeclarations); }
;

ThrowStatement:
    THROW Expr ';'                      { $$ = createNodeInfo<StatementNode*>(new ThrowNode($2), 0, 0); DBG($$.m_node, @1, @3); }
  | THROW Expr error                    { $$ = createNodeInfo<StatementNode*>(new ThrowNode($2), 0, 0); DBG($$.m_node, @1, @2); AUTO_SEMICOLON; }
;

TryStatement:
    TRY Block FINALLY Block             { $$ = createNodeInfo<StatementNode*>(new TryNode($2.m_node, CommonIdentifiers::shared()->nullIdentifier, 0, $4.m_node),
                                                                              mergeDeclarationLists($2.m_varDeclarations, $4.m_varDeclarations),
                                                                              mergeDeclarationLists($2.m_funcDeclarations, $4.m_funcDeclarations));
                                          DBG($$.m_node, @1, @2); }
  | TRY Block CATCH '(' IDENT ')' Block { $$ = createNodeInfo<StatementNode*>(new TryNode($2.m_node, *$5, $7.m_node, 0),
                                                                              mergeDeclarationLists($2.m_varDeclarations, $7.m_varDeclarations),
                                                                              mergeDeclarationLists($2.m_funcDeclarations, $7.m_funcDeclarations));
                                          DBG($$.m_node, @1, @2); }
  | TRY Block CATCH '(' IDENT ')' Block FINALLY Block
                                        { $$ = createNodeInfo<StatementNode*>(new TryNode($2.m_node, *$5, $7.m_node, $9.m_node),
                                                                              mergeDeclarationLists(mergeDeclarationLists($2.m_varDeclarations, $7.m_varDeclarations), $9.m_varDeclarations),
                                                                              mergeDeclarationLists(mergeDeclarationLists($2.m_funcDeclarations, $7.m_funcDeclarations), $9.m_funcDeclarations));
                                          DBG($$.m_node, @1, @2); }
;

DebuggerStatement:
    DEBUGGER ';'                        { $$ = createNodeInfo<StatementNode*>(new EmptyStatementNode(), 0, 0);
                                          DBG($$.m_node, @1, @2); }
  | DEBUGGER error                      { $$ = createNodeInfo<StatementNode*>(new EmptyStatementNode(), 0, 0);
                                          DBG($$.m_node, @1, @1); AUTO_SEMICOLON; }
;

FunctionDeclaration:
    FUNCTION IDENT '(' ')' '{' FunctionBody '}' { $$ = new FuncDeclNode(*$2, $6); DBG($6, @5, @7); }
  | FUNCTION IDENT '(' FormalParameterList ')' '{' FunctionBody '}'
                                        { $$ = new FuncDeclNode(*$2, $4.head, $7); DBG($7, @6, @8); }
;

FunctionExpr:
    FUNCTION '(' ')' '{' FunctionBody '}' { $$ = new FuncExprNode(CommonIdentifiers::shared()->nullIdentifier, $5); DBG($5, @4, @6); }
  | FUNCTION '(' FormalParameterList ')' '{' FunctionBody '}' { $$ = new FuncExprNode(CommonIdentifiers::shared()->nullIdentifier, $6, $3.head); DBG($6, @5, @7); }
  | FUNCTION IDENT '(' ')' '{' FunctionBody '}' { $$ = new FuncExprNode(*$2, $6); DBG($6, @5, @7); }
  | FUNCTION IDENT '(' FormalParameterList ')' '{' FunctionBody '}' { $$ = new FuncExprNode(*$2, $7, $4.head); DBG($7, @6, @8); }
;

FormalParameterList:
    IDENT                               { $$.head = new ParameterNode(*$1);
                                          $$.tail = $$.head; }
  | FormalParameterList ',' IDENT       { $$.head = $1.head;
                                          $$.tail = new ParameterNode($1.tail, *$3); }
;

FunctionBody:
    /* not in spec */           { $$ = FunctionBodyNode::create(0, 0, 0); }
  | SourceElements              { $$ = FunctionBodyNode::create($1.m_node, $1.m_varDeclarations ? &$1.m_varDeclarations->data : 0, 
                                                                $1.m_funcDeclarations ? &$1.m_funcDeclarations->data : 0);
                                  // As in mergeDeclarationLists() we have to ref/deref to safely get rid of
                                  // the declaration lists.
                                  if ($1.m_varDeclarations) {
                                      $1.m_varDeclarations->ref();
                                      $1.m_varDeclarations->deref();
                                  }
                                  if ($1.m_funcDeclarations) {
                                      $1.m_funcDeclarations->ref();
                                      $1.m_funcDeclarations->deref();
                                  }
                                }
;

Program:
    /* not in spec */                   { parser().didFinishParsing(0, 0, 0, @0.last_line); }
    | SourceElements                    { parser().didFinishParsing($1.m_node, $1.m_varDeclarations, $1.m_funcDeclarations, @1.last_line); }
;

SourceElements:
    SourceElement                       { $$.m_node = new SourceElements;
                                          $$.m_node->append($1.m_node);
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          $$.m_funcDeclarations = $1.m_funcDeclarations;
                                        }
  | SourceElements SourceElement        { $$.m_node->append($2.m_node);
                                          $$.m_varDeclarations = mergeDeclarationLists($1.m_varDeclarations, $2.m_varDeclarations);
                                          $$.m_funcDeclarations = mergeDeclarationLists($1.m_funcDeclarations, $2.m_funcDeclarations);
                                        }
;

SourceElement:
    FunctionDeclaration                 { $$ = createNodeInfo<StatementNode*>($1, 0, new ParserRefCountedData<DeclarationStacks::FunctionStack>); $$.m_funcDeclarations->data.append($1); }
  | Statement                           { $$ = $1; }
;
 
%%

static AddNode* makeAddNode(ExpressionNode* left, ExpressionNode* right)
{
    JSType t1 = left->expectedReturnType();
    JSType t2 = right->expectedReturnType();

    if (t1 == NumberType && t2 == NumberType)
        return new AddNumbersNode(left, right);
    if (t1 == StringType && t2 == StringType)
        return new AddStringsNode(left, right);
    if (t1 == StringType)
        return new AddStringLeftNode(left, right);
    if (t2 == StringType)
        return new AddStringRightNode(left, right);
    return new AddNode(left, right);
}

static LessNode* makeLessNode(ExpressionNode* left, ExpressionNode* right)
{
    JSType t1 = left->expectedReturnType();
    JSType t2 = right->expectedReturnType();
    
    if (t1 == StringType && t2 == StringType)
        return new LessStringsNode(left, right);

    // There are certainly more efficient ways to do this type check if necessary
    if (t1 == NumberType || t1 == BooleanType || t1 == UndefinedType || t1 == NullType ||
        t2 == NumberType || t2 == BooleanType || t2 == UndefinedType || t2 == NullType)
        return new LessNumbersNode(left, right);

    // Neither is certain to be a number, nor were both certain to be strings, so we use the default (slow) implementation.
    return new LessNode(left, right);
}

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
    return yychar == '}' || yychar == 0 || lexer().prevTerminator();
}

static ExpressionNode* combineVarInitializers(ExpressionNode* list, AssignResolveNode* init)
{
    if (!list)
        return init;
    return new VarDeclCommaNode(list, init);
}

// We turn variable declarations into either assignments or empty
// statements (which later get stripped out), because the actual
// declaration work is hoisted up to the start of the function body
static StatementNode* makeVarStatementNode(ExpressionNode* expr)
{
    if (!expr)
        return new EmptyStatementNode();
    return new VarStatementNode(expr);
}

