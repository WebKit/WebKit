%pure_parser

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
#include "JSValue.h"
#include "JSObject.h"
#include "nodes.h"
#include "lexer.h"
#include "JSString.h"
#include "JSGlobalData.h"
#include "CommonIdentifiers.h"
#include "NodeInfo.h"
#include "Parser.h"
#include <wtf/MathExtras.h>

#define YYMAXDEPTH 10000
#define YYENABLE_NLS 0

/* default values for bison */
#define YYDEBUG 0 // Set to 1 to debug a parse error.
#define kjsyydebug 0 // Set to 1 to debug a parse error.
#if !PLATFORM(DARWIN)
    // avoid triggering warnings in older bison
#define YYERROR_VERBOSE
#endif

int kjsyylex(void* lvalp, void* llocp, void* globalPtr);
int kjsyyerror(const char*);
static inline bool allowAutomaticSemicolon(KJS::Lexer&, int);

#define GLOBAL_DATA static_cast<JSGlobalData*>(globalPtr)
#define LEXER (GLOBAL_DATA->lexer)

#define AUTO_SEMICOLON do { if (!allowAutomaticSemicolon(*LEXER, yychar)) YYABORT; } while (0)
#define DBG(l, s, e) (l)->setLoc((s).first_line, (e).last_line)

using namespace KJS;
using namespace std;

static ExpressionNode* makeAssignNode(void*, ExpressionNode* loc, Operator, ExpressionNode* expr, bool locHasAssignments, bool exprHasAssignments);
static ExpressionNode* makePrefixNode(void*, ExpressionNode* expr, Operator);
static ExpressionNode* makePostfixNode(void*, ExpressionNode* expr, Operator);
static PropertyNode* makeGetterOrSetterPropertyNode(void*, const Identifier &getOrSet, const Identifier& name, ParameterNode*, FunctionBodyNode*, const SourceRange&);
static ExpressionNodeInfo makeFunctionCallNode(void*, ExpressionNodeInfo func, ArgumentsNodeInfo);
static ExpressionNode* makeTypeOfNode(void*, ExpressionNode*);
static ExpressionNode* makeDeleteNode(void*, ExpressionNode*);
static ExpressionNode* makeNegateNode(void*, ExpressionNode*);
static NumberNode* makeNumberNode(void*, double);
static StatementNode* makeVarStatementNode(void*, ExpressionNode*);
static ExpressionNode* combineVarInitializers(void*, ExpressionNode* list, AssignResolveNode* init);


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

#define YYPARSE_PARAM globalPtr
#define YYLEX_PARAM globalPtr

template <typename T> NodeDeclarationInfo<T> createNodeDeclarationInfo(T node, ParserRefCountedData<DeclarationStacks::VarStack>* varDecls, 
                                                                       ParserRefCountedData<DeclarationStacks::FunctionStack>* funcDecls,
                                                                       FeatureInfo info) 
{
    ASSERT((info & ~(EvalFeature | ClosureFeature | AssignFeature)) == 0);
    NodeDeclarationInfo<T> result = {node, varDecls, funcDecls, info};
    return result;
}

template <typename T> NodeFeatureInfo<T> createNodeFeatureInfo(T node, FeatureInfo info) 
{
    ASSERT((info & ~(EvalFeature | ClosureFeature | AssignFeature)) == 0);
    NodeFeatureInfo<T> result = {node, info};
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

static void appendToVarDeclarationList(void* globalPtr, ParserRefCountedData<DeclarationStacks::VarStack>*& varDecls, const Identifier& ident, unsigned attrs)
{
    if (!varDecls)
        varDecls = new ParserRefCountedData<DeclarationStacks::VarStack>(GLOBAL_DATA);

    varDecls->data.append(make_pair(ident, attrs));

}

static inline void appendToVarDeclarationList(void* globalPtr, ParserRefCountedData<DeclarationStacks::VarStack>*& varDecls, ConstDeclNode* decl)
{
    unsigned attrs = DeclarationStacks::IsConstant;
    if (decl->m_init)
        attrs |= DeclarationStacks::HasInitializer;        
    appendToVarDeclarationList(globalPtr, varDecls, decl->m_ident, attrs);
}

%}

%union {
    int                 intValue;
    double              doubleValue;
    UString*            string;
    Identifier*         ident;

    // expression subtrees
    ExpressionNodeInfo  expressionNode;
    FuncDeclNode*       funcDeclNode;
    PropertyNodeInfo    propertyNode;
    ArgumentsNodeInfo   argumentsNode;
    ConstDeclNodeInfo   constDeclNode;
    CaseBlockNodeInfo   caseBlockNode;
    CaseClauseNodeInfo  caseClauseNode;
    FuncExprNodeInfo    funcExprNode;

    // statement nodes
    StatementNodeInfo   statementNode;
    FunctionBodyNode*   functionBodyNode;
    ProgramNode*        programNode;

    SourceElementsInfo  sourceElements;
    PropertyListInfo    propertyList;
    ArgumentListInfo    argumentList;
    VarDeclListInfo     varDeclList;
    ConstDeclListInfo   constDeclList;
    ClauseListInfo      clauseList;
    ElementListInfo     elementList;
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
%token <intValue> OPENBRACE        /* { (with char offset) */
%token <intValue> CLOSEBRACE        /* { (with char offset) */

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
    NULLTOKEN                           { $$ = createNodeFeatureInfo<ExpressionNode*>(new NullNode(GLOBAL_DATA), 0); }
  | TRUETOKEN                           { $$ = createNodeFeatureInfo<ExpressionNode*>(new BooleanNode(GLOBAL_DATA, true), 0); }
  | FALSETOKEN                          { $$ = createNodeFeatureInfo<ExpressionNode*>(new BooleanNode(GLOBAL_DATA, false), 0); }
  | NUMBER                              { $$ = createNodeFeatureInfo<ExpressionNode*>(makeNumberNode(GLOBAL_DATA, $1), 0); }
  | STRING                              { $$ = createNodeFeatureInfo<ExpressionNode*>(new StringNode(GLOBAL_DATA, $1), 0); }
  | '/' /* regexp */                    {
                                            Lexer& l = *LEXER;
                                            if (!l.scanRegExp())
                                                YYABORT;
                                            $$ = createNodeFeatureInfo<ExpressionNode*>(new RegExpNode(GLOBAL_DATA, l.pattern(), l.flags()), 0);
                                        }
  | DIVEQUAL /* regexp with /= */       {
                                            Lexer& l = *LEXER;
                                            if (!l.scanRegExp())
                                                YYABORT;
                                            $$ = createNodeFeatureInfo<ExpressionNode*>(new RegExpNode(GLOBAL_DATA, "=" + l.pattern(), l.flags()), 0);
                                        }
;

Property:
    IDENT ':' AssignmentExpr            { $$ = createNodeFeatureInfo<PropertyNode*>(new PropertyNode(GLOBAL_DATA, *$1, $3.m_node, PropertyNode::Constant), $3.m_featureInfo); }
  | STRING ':' AssignmentExpr           { $$ = createNodeFeatureInfo<PropertyNode*>(new PropertyNode(GLOBAL_DATA, Identifier(GLOBAL_DATA, *$1), $3.m_node, PropertyNode::Constant), $3.m_featureInfo); }
  | NUMBER ':' AssignmentExpr           { $$ = createNodeFeatureInfo<PropertyNode*>(new PropertyNode(GLOBAL_DATA, Identifier(GLOBAL_DATA, UString::from($1)), $3.m_node, PropertyNode::Constant), $3.m_featureInfo); }
  | IDENT IDENT '(' ')' OPENBRACE FunctionBody CLOSEBRACE    { $$ = createNodeFeatureInfo<PropertyNode*>(makeGetterOrSetterPropertyNode(globalPtr, *$1, *$2, 0, $6, LEXER->sourceRange($5, $7)), ClosureFeature); DBG($6, @5, @7); if (!$$.m_node) YYABORT; }
  | IDENT IDENT '(' FormalParameterList ')' OPENBRACE FunctionBody CLOSEBRACE
                                        { $$ = createNodeFeatureInfo<PropertyNode*>(makeGetterOrSetterPropertyNode(globalPtr, *$1, *$2, $4.head, $7, LEXER->sourceRange($6, $8)), ClosureFeature); DBG($7, @6, @8); if (!$$.m_node) YYABORT; }
;

PropertyList:
    Property                            { $$.m_node.head = new PropertyListNode(GLOBAL_DATA, $1.m_node); 
                                          $$.m_node.tail = $$.m_node.head;
                                          $$.m_featureInfo = $1.m_featureInfo; }
  | PropertyList ',' Property           { $$.m_node.head = $1.m_node.head;
                                          $$.m_node.tail = new PropertyListNode(GLOBAL_DATA, $3.m_node, $1.m_node.tail);
                                          $$.m_featureInfo = $1.m_featureInfo | $3.m_featureInfo;  }
;

PrimaryExpr:
    PrimaryExprNoBrace
  | OPENBRACE CLOSEBRACE                             { $$ = createNodeFeatureInfo<ExpressionNode*>(new ObjectLiteralNode(GLOBAL_DATA), 0); }
  | OPENBRACE PropertyList CLOSEBRACE                { $$ = createNodeFeatureInfo<ExpressionNode*>(new ObjectLiteralNode(GLOBAL_DATA, $2.m_node.head), $2.m_featureInfo); }
  /* allow extra comma, see http://bugs.webkit.org/show_bug.cgi?id=5939 */
  | OPENBRACE PropertyList ',' CLOSEBRACE            { $$ = createNodeFeatureInfo<ExpressionNode*>(new ObjectLiteralNode(GLOBAL_DATA, $2.m_node.head), $2.m_featureInfo); }
;

PrimaryExprNoBrace:
    THISTOKEN                           { $$ = createNodeFeatureInfo<ExpressionNode*>(new ThisNode(GLOBAL_DATA), 0); }
  | Literal
  | ArrayLiteral
  | IDENT                               { $$ = createNodeFeatureInfo<ExpressionNode*>(new ResolveNode(GLOBAL_DATA, *$1), 0); }
  | '(' Expr ')'                        { $$ = $2; }
;

ArrayLiteral:
    '[' ElisionOpt ']'                  { $$ = createNodeFeatureInfo<ExpressionNode*>(new ArrayNode(GLOBAL_DATA, $2), 0); }
  | '[' ElementList ']'                 { $$ = createNodeFeatureInfo<ExpressionNode*>(new ArrayNode(GLOBAL_DATA, $2.m_node.head), $2.m_featureInfo); }
  | '[' ElementList ',' ElisionOpt ']'  { $$ = createNodeFeatureInfo<ExpressionNode*>(new ArrayNode(GLOBAL_DATA, $4, $2.m_node.head), $2.m_featureInfo); }
;

ElementList:
    ElisionOpt AssignmentExpr           { $$.m_node.head = new ElementNode(GLOBAL_DATA, $1, $2.m_node);
                                          $$.m_node.tail = $$.m_node.head;
                                          $$.m_featureInfo = $2.m_featureInfo; }
  | ElementList ',' ElisionOpt AssignmentExpr
                                        { $$.m_node.head = $1.m_node.head;
                                          $$.m_node.tail = new ElementNode(GLOBAL_DATA, $1.m_node.tail, $3, $4.m_node);
                                          $$.m_featureInfo = $1.m_featureInfo | $4.m_featureInfo; }
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
  | FunctionExpr                        { $$ = createNodeFeatureInfo<ExpressionNode*>($1.m_node, $1.m_featureInfo); }
  | MemberExpr '[' Expr ']'             { $$ = createNodeFeatureInfo<ExpressionNode*>(new BracketAccessorNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | MemberExpr '.' IDENT                { $$ = createNodeFeatureInfo<ExpressionNode*>(new DotAccessorNode(GLOBAL_DATA, $1.m_node, *$3), $1.m_featureInfo); }
  | NEW MemberExpr Arguments            { $$ = createNodeFeatureInfo<ExpressionNode*>(new NewExprNode(GLOBAL_DATA, $2.m_node, $3.m_node), $2.m_featureInfo | $3.m_featureInfo); }
;

MemberExprNoBF:
    PrimaryExprNoBrace
  | MemberExprNoBF '[' Expr ']'         { $$ = createNodeFeatureInfo<ExpressionNode*>(new BracketAccessorNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | MemberExprNoBF '.' IDENT            { $$ = createNodeFeatureInfo<ExpressionNode*>(new DotAccessorNode(GLOBAL_DATA, $1.m_node, *$3), $1.m_featureInfo); }
  | NEW MemberExpr Arguments            { $$ = createNodeFeatureInfo<ExpressionNode*>(new NewExprNode(GLOBAL_DATA, $2.m_node, $3.m_node), $2.m_featureInfo | $3.m_featureInfo); }
;

NewExpr:
    MemberExpr
  | NEW NewExpr                         { $$ = createNodeFeatureInfo<ExpressionNode*>(new NewExprNode(GLOBAL_DATA, $2.m_node), $2.m_featureInfo); }
;

NewExprNoBF:
    MemberExprNoBF
  | NEW NewExpr                         { $$ = createNodeFeatureInfo<ExpressionNode*>(new NewExprNode(GLOBAL_DATA, $2.m_node), $2.m_featureInfo); }
;

CallExpr:
    MemberExpr Arguments                { $$ = makeFunctionCallNode(globalPtr, $1, $2); }
  | CallExpr Arguments                  { $$ = makeFunctionCallNode(globalPtr, $1, $2); }
  | CallExpr '[' Expr ']'               { $$ = createNodeFeatureInfo<ExpressionNode*>(new BracketAccessorNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | CallExpr '.' IDENT                  { $$ = createNodeFeatureInfo<ExpressionNode*>(new DotAccessorNode(GLOBAL_DATA, $1.m_node, *$3), $1.m_featureInfo); }
;

CallExprNoBF:
    MemberExprNoBF Arguments            { $$ = makeFunctionCallNode(globalPtr, $1, $2); }
  | CallExprNoBF Arguments              { $$ = makeFunctionCallNode(globalPtr, $1, $2); }
  | CallExprNoBF '[' Expr ']'           { $$ = createNodeFeatureInfo<ExpressionNode*>(new BracketAccessorNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | CallExprNoBF '.' IDENT              { $$ = createNodeFeatureInfo<ExpressionNode*>(new DotAccessorNode(GLOBAL_DATA, $1.m_node, *$3), $1.m_featureInfo); }
;

Arguments:
    '(' ')'                             { $$ = createNodeFeatureInfo<ArgumentsNode*>(new ArgumentsNode(GLOBAL_DATA), 0); }
  | '(' ArgumentList ')'                { $$ = createNodeFeatureInfo<ArgumentsNode*>(new ArgumentsNode(GLOBAL_DATA, $2.m_node.head), $2.m_featureInfo); }
;

ArgumentList:
    AssignmentExpr                      { $$.m_node.head = new ArgumentListNode(GLOBAL_DATA, $1.m_node);
                                          $$.m_node.tail = $$.m_node.head;
                                          $$.m_featureInfo = $1.m_featureInfo; }
  | ArgumentList ',' AssignmentExpr     { $$.m_node.head = $1.m_node.head;
                                          $$.m_node.tail = new ArgumentListNode(GLOBAL_DATA, $1.m_node.tail, $3.m_node);
                                          $$.m_featureInfo = $1.m_featureInfo | $3.m_featureInfo; }
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
  | LeftHandSideExpr PLUSPLUS           { $$ = createNodeFeatureInfo<ExpressionNode*>(makePostfixNode(GLOBAL_DATA, $1.m_node, OpPlusPlus), $1.m_featureInfo | AssignFeature); }
  | LeftHandSideExpr MINUSMINUS         { $$ = createNodeFeatureInfo<ExpressionNode*>(makePostfixNode(GLOBAL_DATA, $1.m_node, OpMinusMinus), $1.m_featureInfo | AssignFeature); }
;

PostfixExprNoBF:
    LeftHandSideExprNoBF
  | LeftHandSideExprNoBF PLUSPLUS       { $$ = createNodeFeatureInfo<ExpressionNode*>(makePostfixNode(GLOBAL_DATA, $1.m_node, OpPlusPlus), $1.m_featureInfo | AssignFeature); }
  | LeftHandSideExprNoBF MINUSMINUS     { $$ = createNodeFeatureInfo<ExpressionNode*>(makePostfixNode(GLOBAL_DATA, $1.m_node, OpMinusMinus), $1.m_featureInfo | AssignFeature); }
;

UnaryExprCommon:
    DELETETOKEN UnaryExpr               { $$ = createNodeFeatureInfo<ExpressionNode*>(makeDeleteNode(GLOBAL_DATA, $2.m_node), $2.m_featureInfo); }
  | VOIDTOKEN UnaryExpr                 { $$ = createNodeFeatureInfo<ExpressionNode*>(new VoidNode(GLOBAL_DATA, $2.m_node), $2.m_featureInfo); }
  | TYPEOF UnaryExpr                    { $$ = createNodeFeatureInfo<ExpressionNode*>(makeTypeOfNode(GLOBAL_DATA, $2.m_node), $2.m_featureInfo); }
  | PLUSPLUS UnaryExpr                  { $$ = createNodeFeatureInfo<ExpressionNode*>(makePrefixNode(GLOBAL_DATA, $2.m_node, OpPlusPlus), $2.m_featureInfo | AssignFeature); }
  | AUTOPLUSPLUS UnaryExpr              { $$ = createNodeFeatureInfo<ExpressionNode*>(makePrefixNode(GLOBAL_DATA, $2.m_node, OpPlusPlus), $2.m_featureInfo | AssignFeature); }
  | MINUSMINUS UnaryExpr                { $$ = createNodeFeatureInfo<ExpressionNode*>(makePrefixNode(GLOBAL_DATA, $2.m_node, OpMinusMinus), $2.m_featureInfo | AssignFeature); }
  | AUTOMINUSMINUS UnaryExpr            { $$ = createNodeFeatureInfo<ExpressionNode*>(makePrefixNode(GLOBAL_DATA, $2.m_node, OpMinusMinus), $2.m_featureInfo | AssignFeature); }
  | '+' UnaryExpr                       { $$ = createNodeFeatureInfo<ExpressionNode*>(new UnaryPlusNode(GLOBAL_DATA, $2.m_node), $2.m_featureInfo); }
  | '-' UnaryExpr                       { $$ = createNodeFeatureInfo<ExpressionNode*>(makeNegateNode(GLOBAL_DATA, $2.m_node), $2.m_featureInfo); }
  | '~' UnaryExpr                       { $$ = createNodeFeatureInfo<ExpressionNode*>(new BitwiseNotNode(GLOBAL_DATA, $2.m_node), $2.m_featureInfo); }
  | '!' UnaryExpr                       { $$ = createNodeFeatureInfo<ExpressionNode*>(new LogicalNotNode(GLOBAL_DATA, $2.m_node), $2.m_featureInfo); }

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
  | MultiplicativeExpr '*' UnaryExpr    { $$ = createNodeFeatureInfo<ExpressionNode*>(new MultNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | MultiplicativeExpr '/' UnaryExpr    { $$ = createNodeFeatureInfo<ExpressionNode*>(new DivNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | MultiplicativeExpr '%' UnaryExpr    { $$ = createNodeFeatureInfo<ExpressionNode*>(new ModNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
;

MultiplicativeExprNoBF:
    UnaryExprNoBF
  | MultiplicativeExprNoBF '*' UnaryExpr
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new MultNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | MultiplicativeExprNoBF '/' UnaryExpr
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new DivNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | MultiplicativeExprNoBF '%' UnaryExpr
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new ModNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
;

AdditiveExpr:
    MultiplicativeExpr
  | AdditiveExpr '+' MultiplicativeExpr { $$ = createNodeFeatureInfo<ExpressionNode*>(new AddNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | AdditiveExpr '-' MultiplicativeExpr { $$ = createNodeFeatureInfo<ExpressionNode*>(new SubNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
;

AdditiveExprNoBF:
    MultiplicativeExprNoBF
  | AdditiveExprNoBF '+' MultiplicativeExpr
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new AddNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | AdditiveExprNoBF '-' MultiplicativeExpr
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new SubNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
;

ShiftExpr:
    AdditiveExpr
  | ShiftExpr LSHIFT AdditiveExpr       { $$ = createNodeFeatureInfo<ExpressionNode*>(new LeftShiftNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | ShiftExpr RSHIFT AdditiveExpr       { $$ = createNodeFeatureInfo<ExpressionNode*>(new RightShiftNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | ShiftExpr URSHIFT AdditiveExpr      { $$ = createNodeFeatureInfo<ExpressionNode*>(new UnsignedRightShiftNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
;

ShiftExprNoBF:
    AdditiveExprNoBF
  | ShiftExprNoBF LSHIFT AdditiveExpr   { $$ = createNodeFeatureInfo<ExpressionNode*>(new LeftShiftNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | ShiftExprNoBF RSHIFT AdditiveExpr   { $$ = createNodeFeatureInfo<ExpressionNode*>(new RightShiftNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | ShiftExprNoBF URSHIFT AdditiveExpr  { $$ = createNodeFeatureInfo<ExpressionNode*>(new UnsignedRightShiftNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
;

RelationalExpr:
    ShiftExpr
  | RelationalExpr '<' ShiftExpr        { $$ = createNodeFeatureInfo<ExpressionNode*>(new LessNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | RelationalExpr '>' ShiftExpr        { $$ = createNodeFeatureInfo<ExpressionNode*>(new GreaterNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | RelationalExpr LE ShiftExpr         { $$ = createNodeFeatureInfo<ExpressionNode*>(new LessEqNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | RelationalExpr GE ShiftExpr         { $$ = createNodeFeatureInfo<ExpressionNode*>(new GreaterEqNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | RelationalExpr INSTANCEOF ShiftExpr { $$ = createNodeFeatureInfo<ExpressionNode*>(new InstanceOfNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | RelationalExpr INTOKEN ShiftExpr    { $$ = createNodeFeatureInfo<ExpressionNode*>(new InNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
;

RelationalExprNoIn:
    ShiftExpr
  | RelationalExprNoIn '<' ShiftExpr    { $$ = createNodeFeatureInfo<ExpressionNode*>(new LessNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | RelationalExprNoIn '>' ShiftExpr    { $$ = createNodeFeatureInfo<ExpressionNode*>(new GreaterNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | RelationalExprNoIn LE ShiftExpr     { $$ = createNodeFeatureInfo<ExpressionNode*>(new LessEqNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | RelationalExprNoIn GE ShiftExpr     { $$ = createNodeFeatureInfo<ExpressionNode*>(new GreaterEqNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | RelationalExprNoIn INSTANCEOF ShiftExpr
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new InstanceOfNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
;

RelationalExprNoBF:
    ShiftExprNoBF
  | RelationalExprNoBF '<' ShiftExpr    { $$ = createNodeFeatureInfo<ExpressionNode*>(new LessNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | RelationalExprNoBF '>' ShiftExpr    { $$ = createNodeFeatureInfo<ExpressionNode*>(new GreaterNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | RelationalExprNoBF LE ShiftExpr     { $$ = createNodeFeatureInfo<ExpressionNode*>(new LessEqNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | RelationalExprNoBF GE ShiftExpr     { $$ = createNodeFeatureInfo<ExpressionNode*>(new GreaterEqNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | RelationalExprNoBF INSTANCEOF ShiftExpr
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new InstanceOfNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | RelationalExprNoBF INTOKEN ShiftExpr     { $$ = createNodeFeatureInfo<ExpressionNode*>(new InNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
;

EqualityExpr:
    RelationalExpr
  | EqualityExpr EQEQ RelationalExpr    { $$ = createNodeFeatureInfo<ExpressionNode*>(new EqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | EqualityExpr NE RelationalExpr      { $$ = createNodeFeatureInfo<ExpressionNode*>(new NotEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | EqualityExpr STREQ RelationalExpr   { $$ = createNodeFeatureInfo<ExpressionNode*>(new StrictEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | EqualityExpr STRNEQ RelationalExpr  { $$ = createNodeFeatureInfo<ExpressionNode*>(new NotStrictEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
;

EqualityExprNoIn:
    RelationalExprNoIn
  | EqualityExprNoIn EQEQ RelationalExprNoIn
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new EqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | EqualityExprNoIn NE RelationalExprNoIn
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new NotEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | EqualityExprNoIn STREQ RelationalExprNoIn
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new StrictEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | EqualityExprNoIn STRNEQ RelationalExprNoIn
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new NotStrictEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
;

EqualityExprNoBF:
    RelationalExprNoBF
  | EqualityExprNoBF EQEQ RelationalExpr
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new EqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | EqualityExprNoBF NE RelationalExpr  { $$ = createNodeFeatureInfo<ExpressionNode*>(new NotEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | EqualityExprNoBF STREQ RelationalExpr
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new StrictEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
  | EqualityExprNoBF STRNEQ RelationalExpr
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new NotStrictEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
;

BitwiseANDExpr:
    EqualityExpr
  | BitwiseANDExpr '&' EqualityExpr     { $$ = createNodeFeatureInfo<ExpressionNode*>(new BitAndNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
;

BitwiseANDExprNoIn:
    EqualityExprNoIn
  | BitwiseANDExprNoIn '&' EqualityExprNoIn
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new BitAndNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
;

BitwiseANDExprNoBF:
    EqualityExprNoBF
  | BitwiseANDExprNoBF '&' EqualityExpr { $$ = createNodeFeatureInfo<ExpressionNode*>(new BitAndNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
;

BitwiseXORExpr:
    BitwiseANDExpr
  | BitwiseXORExpr '^' BitwiseANDExpr   { $$ = createNodeFeatureInfo<ExpressionNode*>(new BitXOrNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
;

BitwiseXORExprNoIn:
    BitwiseANDExprNoIn
  | BitwiseXORExprNoIn '^' BitwiseANDExprNoIn
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new BitXOrNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
;

BitwiseXORExprNoBF:
    BitwiseANDExprNoBF
  | BitwiseXORExprNoBF '^' BitwiseANDExpr
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new BitXOrNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
;

BitwiseORExpr:
    BitwiseXORExpr
  | BitwiseORExpr '|' BitwiseXORExpr    { $$ = createNodeFeatureInfo<ExpressionNode*>(new BitOrNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
;

BitwiseORExprNoIn:
    BitwiseXORExprNoIn
  | BitwiseORExprNoIn '|' BitwiseXORExprNoIn
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new BitOrNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
;

BitwiseORExprNoBF:
    BitwiseXORExprNoBF
  | BitwiseORExprNoBF '|' BitwiseXORExpr
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new BitOrNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo); }
;

LogicalANDExpr:
    BitwiseORExpr
  | LogicalANDExpr AND BitwiseORExpr    { $$ = createNodeFeatureInfo<ExpressionNode*>(new LogicalAndNode(GLOBAL_DATA, $1.m_node, $3.m_node), $1.m_featureInfo | $3.m_featureInfo); }
;

LogicalANDExprNoIn:
    BitwiseORExprNoIn
  | LogicalANDExprNoIn AND BitwiseORExprNoIn
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new LogicalAndNode(GLOBAL_DATA, $1.m_node, $3.m_node), $1.m_featureInfo | $3.m_featureInfo); }
;

LogicalANDExprNoBF:
    BitwiseORExprNoBF
  | LogicalANDExprNoBF AND BitwiseORExpr
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new LogicalAndNode(GLOBAL_DATA, $1.m_node, $3.m_node), $1.m_featureInfo | $3.m_featureInfo); }
;

LogicalORExpr:
    LogicalANDExpr
  | LogicalORExpr OR LogicalANDExpr     { $$ = createNodeFeatureInfo<ExpressionNode*>(new LogicalOrNode(GLOBAL_DATA, $1.m_node, $3.m_node), $1.m_featureInfo | $3.m_featureInfo); }
;

LogicalORExprNoIn:
    LogicalANDExprNoIn
  | LogicalORExprNoIn OR LogicalANDExprNoIn
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new LogicalOrNode(GLOBAL_DATA, $1.m_node, $3.m_node), $1.m_featureInfo | $3.m_featureInfo); }
;

LogicalORExprNoBF:
    LogicalANDExprNoBF
  | LogicalORExprNoBF OR LogicalANDExpr { $$ = createNodeFeatureInfo<ExpressionNode*>(new LogicalOrNode(GLOBAL_DATA, $1.m_node, $3.m_node), $1.m_featureInfo | $3.m_featureInfo); }
;

ConditionalExpr:
    LogicalORExpr
  | LogicalORExpr '?' AssignmentExpr ':' AssignmentExpr
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new ConditionalNode(GLOBAL_DATA, $1.m_node, $3.m_node, $5.m_node), $1.m_featureInfo | $3.m_featureInfo | $5.m_featureInfo); }
;

ConditionalExprNoIn:
    LogicalORExprNoIn
  | LogicalORExprNoIn '?' AssignmentExprNoIn ':' AssignmentExprNoIn
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new ConditionalNode(GLOBAL_DATA, $1.m_node, $3.m_node, $5.m_node), $1.m_featureInfo | $3.m_featureInfo | $5.m_featureInfo); }
;

ConditionalExprNoBF:
    LogicalORExprNoBF
  | LogicalORExprNoBF '?' AssignmentExpr ':' AssignmentExpr
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(new ConditionalNode(GLOBAL_DATA, $1.m_node, $3.m_node, $5.m_node), $1.m_featureInfo | $3.m_featureInfo | $5.m_featureInfo); }
;

AssignmentExpr:
    ConditionalExpr
  | LeftHandSideExpr AssignmentOperator AssignmentExpr
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(makeAssignNode(GLOBAL_DATA, $1.m_node, $2, $3.m_node, $1.m_featureInfo & AssignFeature, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo | AssignFeature); }
;

AssignmentExprNoIn:
    ConditionalExprNoIn
  | LeftHandSideExpr AssignmentOperator AssignmentExprNoIn
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(makeAssignNode(GLOBAL_DATA, $1.m_node, $2, $3.m_node, $1.m_featureInfo & AssignFeature, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo | AssignFeature); }
;

AssignmentExprNoBF:
    ConditionalExprNoBF
  | LeftHandSideExprNoBF AssignmentOperator AssignmentExpr
                                        { $$ = createNodeFeatureInfo<ExpressionNode*>(makeAssignNode(GLOBAL_DATA, $1.m_node, $2, $3.m_node, $1.m_featureInfo & AssignFeature, $3.m_featureInfo & AssignFeature), $1.m_featureInfo | $3.m_featureInfo | AssignFeature); }
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
  | Expr ',' AssignmentExpr             { $$ = createNodeFeatureInfo<ExpressionNode*>(new CommaNode(GLOBAL_DATA, $1.m_node, $3.m_node), $1.m_featureInfo | $3.m_featureInfo); }
;

ExprNoIn:
    AssignmentExprNoIn
  | ExprNoIn ',' AssignmentExprNoIn     { $$ = createNodeFeatureInfo<ExpressionNode*>(new CommaNode(GLOBAL_DATA, $1.m_node, $3.m_node), $1.m_featureInfo | $3.m_featureInfo); }
;

ExprNoBF:
    AssignmentExprNoBF
  | ExprNoBF ',' AssignmentExpr         { $$ = createNodeFeatureInfo<ExpressionNode*>(new CommaNode(GLOBAL_DATA, $1.m_node, $3.m_node), $1.m_featureInfo | $3.m_featureInfo); }
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
    OPENBRACE CLOSEBRACE                             { $$ = createNodeDeclarationInfo<StatementNode*>(new BlockNode(GLOBAL_DATA, 0), 0, 0, 0);
                                          DBG($$.m_node, @1, @2); }
  | OPENBRACE SourceElements CLOSEBRACE              { $$ = createNodeDeclarationInfo<StatementNode*>(new BlockNode(GLOBAL_DATA, $2.m_node), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_featureInfo);
                                          DBG($$.m_node, @1, @3); }
;

VariableStatement:
    VAR VariableDeclarationList ';'     { $$ = createNodeDeclarationInfo<StatementNode*>(makeVarStatementNode(GLOBAL_DATA, $2.m_node), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_featureInfo);
                                          DBG($$.m_node, @1, @3); }
  | VAR VariableDeclarationList error   { $$ = createNodeDeclarationInfo<StatementNode*>(makeVarStatementNode(GLOBAL_DATA, $2.m_node), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_featureInfo);
                                          DBG($$.m_node, @1, @2);
                                          AUTO_SEMICOLON; }
;

VariableDeclarationList:
    IDENT                               { $$.m_node = 0;
                                          $$.m_varDeclarations = new ParserRefCountedData<DeclarationStacks::VarStack>(GLOBAL_DATA);
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, *$1, 0);
                                          $$.m_funcDeclarations = 0;
                                          $$.m_featureInfo = 0;
                                        }
  | IDENT Initializer                   { $$.m_node = new AssignResolveNode(GLOBAL_DATA, *$1, $2.m_node, $2.m_featureInfo & AssignFeature);
                                          $$.m_varDeclarations = new ParserRefCountedData<DeclarationStacks::VarStack>(GLOBAL_DATA);
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, *$1, DeclarationStacks::HasInitializer);
                                          $$.m_funcDeclarations = 0;
                                          $$.m_featureInfo = $2.m_featureInfo;
                                        }
  | VariableDeclarationList ',' IDENT
                                        { $$.m_node = $1.m_node;
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, *$3, 0);
                                          $$.m_funcDeclarations = 0;
                                          $$.m_featureInfo = $1.m_featureInfo;
                                        }
  | VariableDeclarationList ',' IDENT Initializer
                                        { $$.m_node = combineVarInitializers(GLOBAL_DATA, $1.m_node, new AssignResolveNode(GLOBAL_DATA, *$3, $4.m_node, $4.m_featureInfo & AssignFeature));
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, *$3, DeclarationStacks::HasInitializer);
                                          $$.m_funcDeclarations = 0;
                                          $$.m_featureInfo = $1.m_featureInfo | $4.m_featureInfo;
                                        }
;

VariableDeclarationListNoIn:
    IDENT                               { $$.m_node = 0;
                                          $$.m_varDeclarations = new ParserRefCountedData<DeclarationStacks::VarStack>(GLOBAL_DATA);
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, *$1, 0);
                                          $$.m_funcDeclarations = 0;
                                          $$.m_featureInfo = 0;
                                        }
  | IDENT InitializerNoIn               { $$.m_node = new AssignResolveNode(GLOBAL_DATA, *$1, $2.m_node, $2.m_featureInfo & AssignFeature);
                                          $$.m_varDeclarations = new ParserRefCountedData<DeclarationStacks::VarStack>(GLOBAL_DATA);
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, *$1, DeclarationStacks::HasInitializer);
                                          $$.m_funcDeclarations = 0;
                                          $$.m_featureInfo = $2.m_featureInfo;
                                        }
  | VariableDeclarationListNoIn ',' IDENT
                                        { $$.m_node = $1.m_node;
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, *$3, 0);
                                          $$.m_funcDeclarations = 0;
                                          $$.m_featureInfo = $1.m_featureInfo;
                                        }
  | VariableDeclarationListNoIn ',' IDENT InitializerNoIn
                                        { $$.m_node = combineVarInitializers(GLOBAL_DATA, $1.m_node, new AssignResolveNode(GLOBAL_DATA, *$3, $4.m_node, $4.m_featureInfo & AssignFeature));
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, *$3, DeclarationStacks::HasInitializer);
                                          $$.m_funcDeclarations = 0;
                                          $$.m_featureInfo = $1.m_featureInfo | $4.m_featureInfo;
                                        }
;

ConstStatement:
    CONSTTOKEN ConstDeclarationList ';' { $$ = createNodeDeclarationInfo<StatementNode*>(new ConstStatementNode(GLOBAL_DATA, $2.m_node.head), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_featureInfo);
                                          DBG($$.m_node, @1, @3); }
  | CONSTTOKEN ConstDeclarationList error
                                        { $$ = createNodeDeclarationInfo<StatementNode*>(new ConstStatementNode(GLOBAL_DATA, $2.m_node.head), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_featureInfo);
                                          DBG($$.m_node, @1, @2); AUTO_SEMICOLON; }
;

ConstDeclarationList:
    ConstDeclaration                    { $$.m_node.head = $1.m_node;
                                          $$.m_node.tail = $$.m_node.head;
                                          $$.m_varDeclarations = new ParserRefCountedData<DeclarationStacks::VarStack>(GLOBAL_DATA);
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, $1.m_node);
                                          $$.m_funcDeclarations = 0; 
                                          $$.m_featureInfo = $1.m_featureInfo;
    }
  | ConstDeclarationList ',' ConstDeclaration
                                        {  $$.m_node.head = $1.m_node.head;
                                          $1.m_node.tail->m_next = $3.m_node;
                                          $$.m_node.tail = $3.m_node;
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, $3.m_node);
                                          $$.m_funcDeclarations = 0;
                                          $$.m_featureInfo = $1.m_featureInfo | $3.m_featureInfo;}
;

ConstDeclaration:
    IDENT                               { $$ = createNodeFeatureInfo<ConstDeclNode*>(new ConstDeclNode(GLOBAL_DATA, *$1, 0), 0); }
  | IDENT Initializer                   { $$ = createNodeFeatureInfo<ConstDeclNode*>(new ConstDeclNode(GLOBAL_DATA, *$1, $2.m_node), $2.m_featureInfo); }
;

Initializer:
    '=' AssignmentExpr                  { $$ = $2; }
;

InitializerNoIn:
    '=' AssignmentExprNoIn              { $$ = $2; }
;

EmptyStatement:
    ';'                                 { $$ = createNodeDeclarationInfo<StatementNode*>(new EmptyStatementNode(GLOBAL_DATA), 0, 0, 0); }
;

ExprStatement:
    ExprNoBF ';'                        { $$ = createNodeDeclarationInfo<StatementNode*>(new ExprStatementNode(GLOBAL_DATA, $1.m_node), 0, 0, $1.m_featureInfo);
                                          DBG($$.m_node, @1, @2); }
  | ExprNoBF error                      { $$ = createNodeDeclarationInfo<StatementNode*>(new ExprStatementNode(GLOBAL_DATA, $1.m_node), 0, 0, $1.m_featureInfo);
                                          DBG($$.m_node, @1, @1); AUTO_SEMICOLON; }
;

IfStatement:
    IF '(' Expr ')' Statement %prec IF_WITHOUT_ELSE
                                        { $$ = createNodeDeclarationInfo<StatementNode*>(new IfNode(GLOBAL_DATA, $3.m_node, $5.m_node), $5.m_varDeclarations, $5.m_funcDeclarations, $3.m_featureInfo | $5.m_featureInfo);
                                          DBG($$.m_node, @1, @4); }
  | IF '(' Expr ')' Statement ELSE Statement
                                        { $$ = createNodeDeclarationInfo<StatementNode*>(new IfElseNode(GLOBAL_DATA, $3.m_node, $5.m_node, $7.m_node), 
                                                                                         mergeDeclarationLists($5.m_varDeclarations, $7.m_varDeclarations), mergeDeclarationLists($5.m_funcDeclarations, $7.m_funcDeclarations),
                                                                                         $3.m_featureInfo | $5.m_featureInfo | $7.m_featureInfo); 
                                          DBG($$.m_node, @1, @4); }
;

IterationStatement:
    DO Statement WHILE '(' Expr ')' ';'    { $$ = createNodeDeclarationInfo<StatementNode*>(new DoWhileNode(GLOBAL_DATA, $2.m_node, $5.m_node), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_featureInfo | $5.m_featureInfo);
                                             DBG($$.m_node, @1, @3); }
  | DO Statement WHILE '(' Expr ')' error  { $$ = createNodeDeclarationInfo<StatementNode*>(new DoWhileNode(GLOBAL_DATA, $2.m_node, $5.m_node), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_featureInfo | $5.m_featureInfo);
                                             DBG($$.m_node, @1, @3); } // Always performs automatic semicolon insertion.
  | WHILE '(' Expr ')' Statement        { $$ = createNodeDeclarationInfo<StatementNode*>(new WhileNode(GLOBAL_DATA, $3.m_node, $5.m_node), $5.m_varDeclarations, $5.m_funcDeclarations, $3.m_featureInfo | $5.m_featureInfo);
                                          DBG($$.m_node, @1, @4); }
  | FOR '(' ExprNoInOpt ';' ExprOpt ';' ExprOpt ')' Statement
                                        { $$ = createNodeDeclarationInfo<StatementNode*>(new ForNode(GLOBAL_DATA, $3.m_node, $5.m_node, $7.m_node, $9.m_node, false), $9.m_varDeclarations, $9.m_funcDeclarations, 
                                                                                         $3.m_featureInfo | $5.m_featureInfo | $7.m_featureInfo | $9.m_featureInfo);
                                          DBG($$.m_node, @1, @8); 
                                        }
  | FOR '(' VAR VariableDeclarationListNoIn ';' ExprOpt ';' ExprOpt ')' Statement
                                        { $$ = createNodeDeclarationInfo<StatementNode*>(new ForNode(GLOBAL_DATA, $4.m_node, $6.m_node, $8.m_node, $10.m_node, true),
                                                                                         mergeDeclarationLists($4.m_varDeclarations, $10.m_varDeclarations),
                                                                                         mergeDeclarationLists($4.m_funcDeclarations, $10.m_funcDeclarations),
                                                                                         $4.m_featureInfo | $6.m_featureInfo | $8.m_featureInfo | $10.m_featureInfo);
                                          DBG($$.m_node, @1, @9); }
  | FOR '(' LeftHandSideExpr INTOKEN Expr ')' Statement
                                        {
                                            ExpressionNode* n = $3.m_node;
                                            if (!n->isLocation())
                                                YYABORT;
                                            $$ = createNodeDeclarationInfo<StatementNode*>(new ForInNode(GLOBAL_DATA, $3.m_node, $5.m_node, $7.m_node), $7.m_varDeclarations, $7.m_funcDeclarations,
                                                                                           $3.m_featureInfo | $5.m_featureInfo | $7.m_featureInfo);
                                            DBG($$.m_node, @1, @6);
                                        }
  | FOR '(' VAR IDENT INTOKEN Expr ')' Statement
                                        { ForInNode *forIn = new ForInNode(GLOBAL_DATA, *$4, 0, $6.m_node, $8.m_node);
                                          appendToVarDeclarationList(GLOBAL_DATA, $8.m_varDeclarations, *$4, DeclarationStacks::HasInitializer);
                                          $$ = createNodeDeclarationInfo<StatementNode*>(forIn, $8.m_varDeclarations, $8.m_funcDeclarations, $6.m_featureInfo | $8.m_featureInfo);
                                          DBG($$.m_node, @1, @7); }
  | FOR '(' VAR IDENT InitializerNoIn INTOKEN Expr ')' Statement
                                        { ForInNode *forIn = new ForInNode(GLOBAL_DATA, *$4, $5.m_node, $7.m_node, $9.m_node);
                                          appendToVarDeclarationList(GLOBAL_DATA, $9.m_varDeclarations, *$4, DeclarationStacks::HasInitializer);
                                          $$ = createNodeDeclarationInfo<StatementNode*>(forIn, $9.m_varDeclarations, $9.m_funcDeclarations,
                                                                                         $5.m_featureInfo | $7.m_featureInfo | $9.m_featureInfo);
                                          DBG($$.m_node, @1, @8); }
;

ExprOpt:
    /* nothing */                       { $$ = createNodeFeatureInfo<ExpressionNode*>(0, 0); }
  | Expr
;

ExprNoInOpt:
    /* nothing */                       { $$ = createNodeFeatureInfo<ExpressionNode*>(0, 0); }
  | ExprNoIn
;

ContinueStatement:
    CONTINUE ';'                        { $$ = createNodeDeclarationInfo<StatementNode*>(new ContinueNode(GLOBAL_DATA), 0, 0, 0);
                                          DBG($$.m_node, @1, @2); }
  | CONTINUE error                      { $$ = createNodeDeclarationInfo<StatementNode*>(new ContinueNode(GLOBAL_DATA), 0, 0, 0);
                                          DBG($$.m_node, @1, @1); AUTO_SEMICOLON; }
  | CONTINUE IDENT ';'                  { $$ = createNodeDeclarationInfo<StatementNode*>(new ContinueNode(GLOBAL_DATA, *$2), 0, 0, 0);
                                          DBG($$.m_node, @1, @3); }
  | CONTINUE IDENT error                { $$ = createNodeDeclarationInfo<StatementNode*>(new ContinueNode(GLOBAL_DATA, *$2), 0, 0, 0);
                                          DBG($$.m_node, @1, @2); AUTO_SEMICOLON; }
;

BreakStatement:
    BREAK ';'                           { $$ = createNodeDeclarationInfo<StatementNode*>(new BreakNode(GLOBAL_DATA), 0, 0, 0); DBG($$.m_node, @1, @2); }
  | BREAK error                         { $$ = createNodeDeclarationInfo<StatementNode*>(new BreakNode(GLOBAL_DATA), 0, 0, 0); DBG($$.m_node, @1, @1); AUTO_SEMICOLON; }
  | BREAK IDENT ';'                     { $$ = createNodeDeclarationInfo<StatementNode*>(new BreakNode(GLOBAL_DATA, *$2), 0, 0, 0); DBG($$.m_node, @1, @3); }
  | BREAK IDENT error                   { $$ = createNodeDeclarationInfo<StatementNode*>(new BreakNode(GLOBAL_DATA, *$2), 0, 0, 0); DBG($$.m_node, @1, @2); AUTO_SEMICOLON; }
;

ReturnStatement:
    RETURN ';'                          { $$ = createNodeDeclarationInfo<StatementNode*>(new ReturnNode(GLOBAL_DATA, 0), 0, 0, 0); DBG($$.m_node, @1, @2); }
  | RETURN error                        { $$ = createNodeDeclarationInfo<StatementNode*>(new ReturnNode(GLOBAL_DATA, 0), 0, 0, 0); DBG($$.m_node, @1, @1); AUTO_SEMICOLON; }
  | RETURN Expr ';'                     { $$ = createNodeDeclarationInfo<StatementNode*>(new ReturnNode(GLOBAL_DATA, $2.m_node), 0, 0, $2.m_featureInfo); DBG($$.m_node, @1, @3); }
  | RETURN Expr error                   { $$ = createNodeDeclarationInfo<StatementNode*>(new ReturnNode(GLOBAL_DATA, $2.m_node), 0, 0, $2.m_featureInfo); DBG($$.m_node, @1, @2); AUTO_SEMICOLON; }
;

WithStatement:
    WITH '(' Expr ')' Statement         { $$ = createNodeDeclarationInfo<StatementNode*>(new WithNode(GLOBAL_DATA, $3.m_node, $5.m_node), $5.m_varDeclarations, $5.m_funcDeclarations,
                                                                                         $3.m_featureInfo | $5.m_featureInfo);
                                          DBG($$.m_node, @1, @4); }
;

SwitchStatement:
    SWITCH '(' Expr ')' CaseBlock       { $$ = createNodeDeclarationInfo<StatementNode*>(new SwitchNode(GLOBAL_DATA, $3.m_node, $5.m_node), $5.m_varDeclarations, $5.m_funcDeclarations,
                                                                                         $3.m_featureInfo | $5.m_featureInfo);
                                          DBG($$.m_node, @1, @4); }
;

CaseBlock:
    OPENBRACE CaseClausesOpt CLOSEBRACE              { $$ = createNodeDeclarationInfo<CaseBlockNode*>(new CaseBlockNode(GLOBAL_DATA, $2.m_node.head, 0, 0), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_featureInfo); }
  | OPENBRACE CaseClausesOpt DefaultClause CaseClausesOpt CLOSEBRACE
                                        { $$ = createNodeDeclarationInfo<CaseBlockNode*>(new CaseBlockNode(GLOBAL_DATA, $2.m_node.head, $3.m_node, $4.m_node.head),
                                                                                         mergeDeclarationLists(mergeDeclarationLists($2.m_varDeclarations, $3.m_varDeclarations), $4.m_varDeclarations),
                                                                                         mergeDeclarationLists(mergeDeclarationLists($2.m_funcDeclarations, $3.m_funcDeclarations), $4.m_funcDeclarations),
                                                                                         $2.m_featureInfo | $3.m_featureInfo | $4.m_featureInfo); }
;

CaseClausesOpt:
/* nothing */                       { $$.m_node.head = 0; $$.m_node.tail = 0; $$.m_varDeclarations = 0; $$.m_funcDeclarations = 0; $$.m_featureInfo = 0; }
  | CaseClauses
;

CaseClauses:
    CaseClause                          { $$.m_node.head = new ClauseListNode(GLOBAL_DATA, $1.m_node);
                                          $$.m_node.tail = $$.m_node.head;
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          $$.m_funcDeclarations = $1.m_funcDeclarations; 
                                          $$.m_featureInfo = $1.m_featureInfo; } 
  | CaseClauses CaseClause              { $$.m_node.head = $1.m_node.head;
                                          $$.m_node.tail = new ClauseListNode(GLOBAL_DATA, $1.m_node.tail, $2.m_node);
                                          $$.m_varDeclarations = mergeDeclarationLists($1.m_varDeclarations, $2.m_varDeclarations);
                                          $$.m_funcDeclarations = mergeDeclarationLists($1.m_funcDeclarations, $2.m_funcDeclarations);
                                          $$.m_featureInfo = $1.m_featureInfo | $2.m_featureInfo;
                                        }
;

CaseClause:
    CASE Expr ':'                       { $$ = createNodeDeclarationInfo<CaseClauseNode*>(new CaseClauseNode(GLOBAL_DATA, $2.m_node), 0, 0, $2.m_featureInfo); }
  | CASE Expr ':' SourceElements        { $$ = createNodeDeclarationInfo<CaseClauseNode*>(new CaseClauseNode(GLOBAL_DATA, $2.m_node, $4.m_node), $4.m_varDeclarations, $4.m_funcDeclarations, $2.m_featureInfo | $4.m_featureInfo); }
;

DefaultClause:
    DEFAULT ':'                         { $$ = createNodeDeclarationInfo<CaseClauseNode*>(new CaseClauseNode(GLOBAL_DATA, 0), 0, 0, 0); }
  | DEFAULT ':' SourceElements          { $$ = createNodeDeclarationInfo<CaseClauseNode*>(new CaseClauseNode(GLOBAL_DATA, 0, $3.m_node), $3.m_varDeclarations, $3.m_funcDeclarations, $3.m_featureInfo); }
;

LabelledStatement:
    IDENT ':' Statement                 { $3.m_node->pushLabel(*$1);
                                          $$ = createNodeDeclarationInfo<StatementNode*>(new LabelNode(GLOBAL_DATA, *$1, $3.m_node), $3.m_varDeclarations, $3.m_funcDeclarations, $3.m_featureInfo); }
;

ThrowStatement:
    THROW Expr ';'                      { $$ = createNodeDeclarationInfo<StatementNode*>(new ThrowNode(GLOBAL_DATA, $2.m_node), 0, 0, $2.m_featureInfo); DBG($$.m_node, @1, @3); }
  | THROW Expr error                    { $$ = createNodeDeclarationInfo<StatementNode*>(new ThrowNode(GLOBAL_DATA, $2.m_node), 0, 0, $2.m_featureInfo); DBG($$.m_node, @1, @2); AUTO_SEMICOLON; }
;

TryStatement:
    TRY Block FINALLY Block             { $$ = createNodeDeclarationInfo<StatementNode*>(new TryNode(GLOBAL_DATA, $2.m_node, GLOBAL_DATA->propertyNames->nullIdentifier, 0, $4.m_node),
                                                                                         mergeDeclarationLists($2.m_varDeclarations, $4.m_varDeclarations),
                                                                                         mergeDeclarationLists($2.m_funcDeclarations, $4.m_funcDeclarations),
                                                                                         $2.m_featureInfo | $4.m_featureInfo);
                                          DBG($$.m_node, @1, @2); }
  | TRY Block CATCH '(' IDENT ')' Block { $$ = createNodeDeclarationInfo<StatementNode*>(new TryNode(GLOBAL_DATA, $2.m_node, *$5, $7.m_node, 0),
                                                                                         mergeDeclarationLists($2.m_varDeclarations, $7.m_varDeclarations),
                                                                                         mergeDeclarationLists($2.m_funcDeclarations, $7.m_funcDeclarations),
                                                                                         $2.m_featureInfo | $7.m_featureInfo);
                                          DBG($$.m_node, @1, @2); }
  | TRY Block CATCH '(' IDENT ')' Block FINALLY Block
                                        { $$ = createNodeDeclarationInfo<StatementNode*>(new TryNode(GLOBAL_DATA, $2.m_node, *$5, $7.m_node, $9.m_node),
                                                                                         mergeDeclarationLists(mergeDeclarationLists($2.m_varDeclarations, $7.m_varDeclarations), $9.m_varDeclarations),
                                                                                         mergeDeclarationLists(mergeDeclarationLists($2.m_funcDeclarations, $7.m_funcDeclarations), $9.m_funcDeclarations),
                                                                                         $2.m_featureInfo | $7.m_featureInfo | $9.m_featureInfo);
                                          DBG($$.m_node, @1, @2); }
;

DebuggerStatement:
    DEBUGGER ';'                        { $$ = createNodeDeclarationInfo<StatementNode*>(new DebuggerStatementNode(GLOBAL_DATA), 0, 0, 0);
                                          DBG($$.m_node, @1, @2); }
  | DEBUGGER error                      { $$ = createNodeDeclarationInfo<StatementNode*>(new DebuggerStatementNode(GLOBAL_DATA), 0, 0, 0);
                                          DBG($$.m_node, @1, @1); AUTO_SEMICOLON; }
;

FunctionDeclaration:
    FUNCTION IDENT '(' ')' OPENBRACE FunctionBody CLOSEBRACE { $$ = new FuncDeclNode(GLOBAL_DATA, *$2, $6, LEXER->sourceRange($5, $7)); DBG($6, @5, @7); }
  | FUNCTION IDENT '(' FormalParameterList ')' OPENBRACE FunctionBody CLOSEBRACE
                                        { $$ = new FuncDeclNode(GLOBAL_DATA, *$2, $7, LEXER->sourceRange($6, $8), $4.head); DBG($7, @6, @8); }
;

FunctionExpr:
    FUNCTION '(' ')' OPENBRACE FunctionBody CLOSEBRACE { $$ = createNodeFeatureInfo(new FuncExprNode(GLOBAL_DATA, GLOBAL_DATA->propertyNames->nullIdentifier, $5, LEXER->sourceRange($4, $6)), ClosureFeature); DBG($5, @4, @6); }
  | FUNCTION '(' FormalParameterList ')' OPENBRACE FunctionBody CLOSEBRACE { $$ = createNodeFeatureInfo(new FuncExprNode(GLOBAL_DATA, GLOBAL_DATA->propertyNames->nullIdentifier, $6, LEXER->sourceRange($5, $7), $3.head), ClosureFeature); DBG($6, @5, @7); }
  | FUNCTION IDENT '(' ')' OPENBRACE FunctionBody CLOSEBRACE { $$ = createNodeFeatureInfo(new FuncExprNode(GLOBAL_DATA, *$2, $6, LEXER->sourceRange($5, $7)), ClosureFeature); DBG($6, @5, @7); }
  | FUNCTION IDENT '(' FormalParameterList ')' OPENBRACE FunctionBody CLOSEBRACE { $$ = createNodeFeatureInfo(new FuncExprNode(GLOBAL_DATA, *$2, $7, LEXER->sourceRange($6, $8), $4.head), ClosureFeature); DBG($7, @6, @8); }
;

FormalParameterList:
    IDENT                               { $$.head = new ParameterNode(GLOBAL_DATA, *$1);
                                          $$.tail = $$.head; }
  | FormalParameterList ',' IDENT       { $$.head = $1.head;
                                          $$.tail = new ParameterNode(GLOBAL_DATA, $1.tail, *$3); }
;

FunctionBody:
    /* not in spec */           { $$ = FunctionBodyNode::create(GLOBAL_DATA, 0, 0, 0, false, false); }
  | SourceElements              { $$ = FunctionBodyNode::create(GLOBAL_DATA, $1.m_node, $1.m_varDeclarations ? &$1.m_varDeclarations->data : 0, 
                                                                $1.m_funcDeclarations ? &$1.m_funcDeclarations->data : 0,
                                                                ($1.m_featureInfo & EvalFeature) != 0, ($1.m_featureInfo & ClosureFeature) != 0);
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
    /* not in spec */                   { GLOBAL_DATA->parser->didFinishParsing(new SourceElements(GLOBAL_DATA), 0, 0, false, false, @0.last_line); }
    | SourceElements                    { GLOBAL_DATA->parser->didFinishParsing($1.m_node, $1.m_varDeclarations, $1.m_funcDeclarations, 
                                                                    ($1.m_featureInfo & EvalFeature) != 0, ($1.m_featureInfo & ClosureFeature) != 0,
                                                                    @1.last_line); }
;

SourceElements:
    SourceElement                       { $$.m_node = new SourceElements(GLOBAL_DATA);
                                          $$.m_node->append($1.m_node);
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          $$.m_funcDeclarations = $1.m_funcDeclarations;
                                          $$.m_featureInfo = $1.m_featureInfo;
                                        }
  | SourceElements SourceElement        { $$.m_node->append($2.m_node);
                                          $$.m_varDeclarations = mergeDeclarationLists($1.m_varDeclarations, $2.m_varDeclarations);
                                          $$.m_funcDeclarations = mergeDeclarationLists($1.m_funcDeclarations, $2.m_funcDeclarations);
                                          $$.m_featureInfo = $1.m_featureInfo | $2.m_featureInfo;
                                        }
;

SourceElement:
    FunctionDeclaration                 { $$ = createNodeDeclarationInfo<StatementNode*>($1, 0, new ParserRefCountedData<DeclarationStacks::FunctionStack>(GLOBAL_DATA), ClosureFeature); $$.m_funcDeclarations->data.append($1); }
  | Statement                           { $$ = $1; }
;
 
%%

static ExpressionNode* makeAssignNode(void* globalPtr, ExpressionNode* loc, Operator op, ExpressionNode* expr, bool locHasAssignments, bool exprHasAssignments)
{
    if (!loc->isLocation())
        return new AssignErrorNode(GLOBAL_DATA, loc, op, expr);

    if (loc->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(loc);
        if (op == OpEqual)
            return new AssignResolveNode(GLOBAL_DATA, resolve->identifier(), expr, exprHasAssignments);
        else
            return new ReadModifyResolveNode(GLOBAL_DATA, resolve->identifier(), op, expr, exprHasAssignments);
    }
    if (loc->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(loc);
        if (op == OpEqual)
            return new AssignBracketNode(GLOBAL_DATA, bracket->base(), bracket->subscript(), expr, locHasAssignments, exprHasAssignments);
        else
            return new ReadModifyBracketNode(GLOBAL_DATA, bracket->base(), bracket->subscript(), op, expr, locHasAssignments, exprHasAssignments);
    }
    ASSERT(loc->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(loc);
    if (op == OpEqual)
        return new AssignDotNode(GLOBAL_DATA, dot->base(), dot->identifier(), expr, exprHasAssignments);
    return new ReadModifyDotNode(GLOBAL_DATA, dot->base(), dot->identifier(), op, expr, exprHasAssignments);
}

static ExpressionNode* makePrefixNode(void* globalPtr, ExpressionNode* expr, Operator op)
{ 
    if (!expr->isLocation())
        return new PrefixErrorNode(GLOBAL_DATA, expr, op);
    
    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        if (op == OpPlusPlus)
            return new PreIncResolveNode(GLOBAL_DATA, resolve->identifier());
        else
            return new PreDecResolveNode(GLOBAL_DATA, resolve->identifier());
    }
    if (expr->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(expr);
        if (op == OpPlusPlus)
            return new PreIncBracketNode(GLOBAL_DATA, bracket->base(), bracket->subscript());
        else
            return new PreDecBracketNode(GLOBAL_DATA, bracket->base(), bracket->subscript());
    }
    ASSERT(expr->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(expr);
    if (op == OpPlusPlus)
        return new PreIncDotNode(GLOBAL_DATA, dot->base(), dot->identifier());
    return new PreDecDotNode(GLOBAL_DATA, dot->base(), dot->identifier());
}

static ExpressionNode* makePostfixNode(void* globalPtr, ExpressionNode* expr, Operator op)
{ 
    if (!expr->isLocation())
        return new PostfixErrorNode(GLOBAL_DATA, expr, op);
    
    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        if (op == OpPlusPlus)
            return new PostIncResolveNode(GLOBAL_DATA, resolve->identifier());
        else
            return new PostDecResolveNode(GLOBAL_DATA, resolve->identifier());
    }
    if (expr->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(expr);
        if (op == OpPlusPlus)
            return new PostIncBracketNode(GLOBAL_DATA, bracket->base(), bracket->subscript());
        else
            return new PostDecBracketNode(GLOBAL_DATA, bracket->base(), bracket->subscript());
    }
    ASSERT(expr->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(expr);
    
    if (op == OpPlusPlus)
        return new PostIncDotNode(GLOBAL_DATA, dot->base(), dot->identifier());
    return new PostDecDotNode(GLOBAL_DATA, dot->base(), dot->identifier());
}

static ExpressionNodeInfo makeFunctionCallNode(void* globalPtr, ExpressionNodeInfo func, ArgumentsNodeInfo args)
{
    FeatureInfo features = func.m_featureInfo | args.m_featureInfo;
    if (!func.m_node->isLocation())
        return createNodeFeatureInfo<ExpressionNode*>(new FunctionCallValueNode(GLOBAL_DATA, func.m_node, args.m_node), features);
    if (func.m_node->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(func.m_node);
        const Identifier& identifier = resolve->identifier();
        if (identifier == GLOBAL_DATA->propertyNames->eval)
            return createNodeFeatureInfo<ExpressionNode*>(new EvalFunctionCallNode(GLOBAL_DATA, args.m_node), EvalFeature | features);
        return createNodeFeatureInfo<ExpressionNode*>(new FunctionCallResolveNode(GLOBAL_DATA, identifier, args.m_node), features);
    }
    if (func.m_node->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(func.m_node);
        return createNodeFeatureInfo<ExpressionNode*>(new FunctionCallBracketNode(GLOBAL_DATA, bracket->base(), bracket->subscript(), args.m_node), features);
    }
    ASSERT(func.m_node->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(func.m_node);
    return createNodeFeatureInfo<ExpressionNode*>(new FunctionCallDotNode(GLOBAL_DATA, dot->base(), dot->identifier(), args.m_node), features);
}

static ExpressionNode* makeTypeOfNode(void* globalPtr, ExpressionNode* expr)
{
    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        return new TypeOfResolveNode(GLOBAL_DATA, resolve->identifier());
    }
    return new TypeOfValueNode(GLOBAL_DATA, expr);
}

static ExpressionNode* makeDeleteNode(void* globalPtr, ExpressionNode* expr)
{
    if (!expr->isLocation())
        return new DeleteValueNode(GLOBAL_DATA, expr);
    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        return new DeleteResolveNode(GLOBAL_DATA, resolve->identifier());
    }
    if (expr->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(expr);
        return new DeleteBracketNode(GLOBAL_DATA, bracket->base(), bracket->subscript());
    }
    ASSERT(expr->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(expr);
    return new DeleteDotNode(GLOBAL_DATA, dot->base(), dot->identifier());
}

static PropertyNode* makeGetterOrSetterPropertyNode(void* globalPtr, const Identifier& getOrSet, const Identifier& name, ParameterNode* params, FunctionBodyNode* body, const SourceRange& source)
{
    PropertyNode::Type type;
    if (getOrSet == "get")
        type = PropertyNode::Getter;
    else if (getOrSet == "set")
        type = PropertyNode::Setter;
    else
        return 0;
    return new PropertyNode(GLOBAL_DATA, name, new FuncExprNode(GLOBAL_DATA, GLOBAL_DATA->propertyNames->nullIdentifier, body, source, params), type);
}

static ExpressionNode* makeNegateNode(void* globalPtr, ExpressionNode* n)
{
    if (n->isNumber()) {
        NumberNode* number = static_cast<NumberNode*>(n);

        if (number->value() > 0.0) {
            number->setValue(-number->value());
            return number;
        }
    }

    return new NegateNode(GLOBAL_DATA, n);
}

static NumberNode* makeNumberNode(void* globalPtr, double d)
{
    JSValue* value = JSImmediate::from(d);
    if (value)
        return new ImmediateNumberNode(GLOBAL_DATA, value, d);
    return new NumberNode(GLOBAL_DATA, d);
}

/* called by yyparse on error */
int yyerror(const char *)
{
    return 1;
}

/* may we automatically insert a semicolon ? */
static bool allowAutomaticSemicolon(Lexer& lexer, int yychar)
{
    return yychar == CLOSEBRACE || yychar == 0 || lexer.prevTerminator();
}

static ExpressionNode* combineVarInitializers(void* globalPtr, ExpressionNode* list, AssignResolveNode* init)
{
    if (!list)
        return init;
    return new VarDeclCommaNode(GLOBAL_DATA, list, init);
}

// We turn variable declarations into either assignments or empty
// statements (which later get stripped out), because the actual
// declaration work is hoisted up to the start of the function body
static StatementNode* makeVarStatementNode(void* globalPtr, ExpressionNode* expr)
{
    if (!expr)
        return new EmptyStatementNode(GLOBAL_DATA);
    return new VarStatementNode(GLOBAL_DATA, expr);
}

#undef GLOBAL_DATA
