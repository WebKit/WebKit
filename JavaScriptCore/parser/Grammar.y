%pure_parser

%{

/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "JSObject.h"
#include "JSString.h"
#include "Lexer.h"
#include "NodeConstructors.h"
#include "NodeInfo.h"
#include <stdlib.h>
#include <string.h>
#include <wtf/MathExtras.h>

#define YYMALLOC fastMalloc
#define YYFREE fastFree

#define YYMAXDEPTH 10000
#define YYENABLE_NLS 0

// Default values for bison.
#define YYDEBUG 0 // Set to 1 to debug a parse error.
#define jscyydebug 0 // Set to 1 to debug a parse error.
#if !OS(DARWIN)
// Avoid triggering warnings in older bison by not setting this on the Darwin platform.
// FIXME: Is this still needed?
#define YYERROR_VERBOSE
#endif

int jscyyerror(const char*);

static inline bool allowAutomaticSemicolon(JSC::Lexer&, int);

#define GLOBAL_DATA static_cast<JSGlobalData*>(globalPtr)
#define AUTO_SEMICOLON do { if (!allowAutomaticSemicolon(*GLOBAL_DATA->lexer, yychar)) YYABORT; } while (0)

using namespace JSC;
using namespace std;

static ExpressionNode* makeAssignNode(JSGlobalData*, ExpressionNode* left, Operator, ExpressionNode* right, bool leftHasAssignments, bool rightHasAssignments, int start, int divot, int end);
static ExpressionNode* makePrefixNode(JSGlobalData*, ExpressionNode*, Operator, int start, int divot, int end);
static ExpressionNode* makePostfixNode(JSGlobalData*, ExpressionNode*, Operator, int start, int divot, int end);
static PropertyNode* makeGetterOrSetterPropertyNode(JSGlobalData*, const Identifier& getOrSet, const Identifier& name, ParameterNode*, FunctionBodyNode*, const SourceCode&);
static ExpressionNodeInfo makeFunctionCallNode(JSGlobalData*, ExpressionNodeInfo function, ArgumentsNodeInfo, int start, int divot, int end);
static ExpressionNode* makeTypeOfNode(JSGlobalData*, ExpressionNode*);
static ExpressionNode* makeDeleteNode(JSGlobalData*, ExpressionNode*, int start, int divot, int end);
static ExpressionNode* makeNegateNode(JSGlobalData*, ExpressionNode*);
static NumberNode* makeNumberNode(JSGlobalData*, double);
static ExpressionNode* makeBitwiseNotNode(JSGlobalData*, ExpressionNode*);
static ExpressionNode* makeMultNode(JSGlobalData*, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
static ExpressionNode* makeDivNode(JSGlobalData*, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
static ExpressionNode* makeAddNode(JSGlobalData*, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
static ExpressionNode* makeSubNode(JSGlobalData*, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
static ExpressionNode* makeLeftShiftNode(JSGlobalData*, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
static ExpressionNode* makeRightShiftNode(JSGlobalData*, ExpressionNode* left, ExpressionNode* right, bool rightHasAssignments);
static StatementNode* makeVarStatementNode(JSGlobalData*, ExpressionNode*);
static ExpressionNode* combineCommaNodes(JSGlobalData*, ExpressionNode* list, ExpressionNode* init);

#if COMPILER(MSVC)

#pragma warning(disable: 4065)
#pragma warning(disable: 4244)
#pragma warning(disable: 4702)

#endif

#define YYPARSE_PARAM globalPtr
#define YYLEX_PARAM globalPtr

template <typename T> inline NodeDeclarationInfo<T> createNodeDeclarationInfo(T node,
    ParserArenaData<DeclarationStacks::VarStack>* varDecls,
    ParserArenaData<DeclarationStacks::FunctionStack>* funcDecls,
    CodeFeatures info, int numConstants) 
{
    ASSERT((info & ~AllFeatures) == 0);
    NodeDeclarationInfo<T> result = { node, varDecls, funcDecls, info, numConstants };
    return result;
}

template <typename T> inline NodeInfo<T> createNodeInfo(T node, CodeFeatures info, int numConstants)
{
    ASSERT((info & ~AllFeatures) == 0);
    NodeInfo<T> result = { node, info, numConstants };
    return result;
}

template <typename T> inline T mergeDeclarationLists(T decls1, T decls2) 
{
    // decls1 or both are null
    if (!decls1)
        return decls2;
    // only decls1 is non-null
    if (!decls2)
        return decls1;

    // Both are non-null
    decls1->data.append(decls2->data);
    
    // Manually release as much as possible from the now-defunct declaration lists
    // to avoid accumulating so many unused heap allocated vectors.
    decls2->data.clear();

    return decls1;
}

static inline void appendToVarDeclarationList(JSGlobalData* globalData, ParserArenaData<DeclarationStacks::VarStack>*& varDecls, const Identifier& ident, unsigned attrs)
{
    if (!varDecls)
        varDecls = new (globalData) ParserArenaData<DeclarationStacks::VarStack>;

    varDecls->data.append(make_pair(&ident, attrs));
}

static inline void appendToVarDeclarationList(JSGlobalData* globalData, ParserArenaData<DeclarationStacks::VarStack>*& varDecls, ConstDeclNode* decl)
{
    unsigned attrs = DeclarationStacks::IsConstant;
    if (decl->hasInitializer())
        attrs |= DeclarationStacks::HasInitializer;        
    appendToVarDeclarationList(globalData, varDecls, decl->ident(), attrs);
}

%}

%union {
    int                 intValue;
    double              doubleValue;
    const Identifier*   ident;

    // expression subtrees
    ExpressionNodeInfo  expressionNode;
    FuncDeclNodeInfo    funcDeclNode;
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
    ParameterListInfo   parameterList;

    Operator            op;
}

%{

template <typename T> inline void setStatementLocation(StatementNode* statement, const T& start, const T& end)
{
    statement->setLoc(start.first_line, end.last_line);
}

static inline void setExceptionLocation(ThrowableExpressionData* node, unsigned start, unsigned divot, unsigned end)
{
    node->setExceptionSourceCode(divot, divot - start, end - divot);
}

%}

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
%token <intValue> CLOSEBRACE       /* } (with char offset) */

/* terminal types */
%token <doubleValue> NUMBER
%token <ident> IDENT STRING

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

%type <expressionNode>  Initializer InitializerNoIn
%type <statementNode>   FunctionDeclaration
%type <funcExprNode>    FunctionExpr
%type <functionBodyNode> FunctionBody
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

// FIXME: There are currently two versions of the grammar in this file, the normal one, and the NoNodes version used for
// lazy recompilation of FunctionBodyNodes.  We should move to generating the two versions from a script to avoid bugs.
// In the mean time, make sure to make any changes to the grammar in both versions.

Literal:
    NULLTOKEN                           { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) NullNode(GLOBAL_DATA), 0, 1); }
  | TRUETOKEN                           { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) BooleanNode(GLOBAL_DATA, true), 0, 1); }
  | FALSETOKEN                          { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) BooleanNode(GLOBAL_DATA, false), 0, 1); }
  | NUMBER                              { $$ = createNodeInfo<ExpressionNode*>(makeNumberNode(GLOBAL_DATA, $1), 0, 1); }
  | STRING                              { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) StringNode(GLOBAL_DATA, *$1), 0, 1); }
  | '/' /* regexp */                    {
                                            Lexer& l = *GLOBAL_DATA->lexer;
                                            const Identifier* pattern;
                                            const Identifier* flags;
                                            if (!l.scanRegExp(pattern, flags))
                                                YYABORT;
                                            RegExpNode* node = new (GLOBAL_DATA) RegExpNode(GLOBAL_DATA, *pattern, *flags);
                                            int size = pattern->size() + 2; // + 2 for the two /'s
                                            setExceptionLocation(node, @1.first_column, @1.first_column + size, @1.first_column + size);
                                            $$ = createNodeInfo<ExpressionNode*>(node, 0, 0);
                                        }
  | DIVEQUAL /* regexp with /= */       {
                                            Lexer& l = *GLOBAL_DATA->lexer;
                                            const Identifier* pattern;
                                            const Identifier* flags;
                                            if (!l.scanRegExp(pattern, flags, '='))
                                                YYABORT;
                                            RegExpNode* node = new (GLOBAL_DATA) RegExpNode(GLOBAL_DATA, *pattern, *flags);
                                            int size = pattern->size() + 2; // + 2 for the two /'s
                                            setExceptionLocation(node, @1.first_column, @1.first_column + size, @1.first_column + size);
                                            $$ = createNodeInfo<ExpressionNode*>(node, 0, 0);
                                        }
;

Property:
    IDENT ':' AssignmentExpr            { $$ = createNodeInfo<PropertyNode*>(new (GLOBAL_DATA) PropertyNode(GLOBAL_DATA, *$1, $3.m_node, PropertyNode::Constant), $3.m_features, $3.m_numConstants); }
  | STRING ':' AssignmentExpr           { $$ = createNodeInfo<PropertyNode*>(new (GLOBAL_DATA) PropertyNode(GLOBAL_DATA, *$1, $3.m_node, PropertyNode::Constant), $3.m_features, $3.m_numConstants); }
  | NUMBER ':' AssignmentExpr           { $$ = createNodeInfo<PropertyNode*>(new (GLOBAL_DATA) PropertyNode(GLOBAL_DATA, $1, $3.m_node, PropertyNode::Constant), $3.m_features, $3.m_numConstants); }
  | IDENT IDENT '(' ')' OPENBRACE FunctionBody CLOSEBRACE    { $$ = createNodeInfo<PropertyNode*>(makeGetterOrSetterPropertyNode(GLOBAL_DATA, *$1, *$2, 0, $6, GLOBAL_DATA->lexer->sourceCode($5, $7, @5.first_line)), ClosureFeature, 0); setStatementLocation($6, @5, @7); if (!$$.m_node) YYABORT; }
  | IDENT IDENT '(' FormalParameterList ')' OPENBRACE FunctionBody CLOSEBRACE
                                                             {
                                                                 $$ = createNodeInfo<PropertyNode*>(makeGetterOrSetterPropertyNode(GLOBAL_DATA, *$1, *$2, $4.m_node.head, $7, GLOBAL_DATA->lexer->sourceCode($6, $8, @6.first_line)), $4.m_features | ClosureFeature, 0); 
                                                                 if ($4.m_features & ArgumentsFeature)
                                                                     $7->setUsesArguments(); 
                                                                 setStatementLocation($7, @6, @8); 
                                                                 if (!$$.m_node) 
                                                                     YYABORT; 
                                                             }
;

PropertyList:
    Property                            { $$.m_node.head = new (GLOBAL_DATA) PropertyListNode(GLOBAL_DATA, $1.m_node); 
                                          $$.m_node.tail = $$.m_node.head;
                                          $$.m_features = $1.m_features;
                                          $$.m_numConstants = $1.m_numConstants; }
  | PropertyList ',' Property           { $$.m_node.head = $1.m_node.head;
                                          $$.m_node.tail = new (GLOBAL_DATA) PropertyListNode(GLOBAL_DATA, $3.m_node, $1.m_node.tail);
                                          $$.m_features = $1.m_features | $3.m_features;
                                          $$.m_numConstants = $1.m_numConstants + $3.m_numConstants; }
;

PrimaryExpr:
    PrimaryExprNoBrace
  | OPENBRACE CLOSEBRACE                             { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) ObjectLiteralNode(GLOBAL_DATA), 0, 0); }
  | OPENBRACE PropertyList CLOSEBRACE                { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) ObjectLiteralNode(GLOBAL_DATA, $2.m_node.head), $2.m_features, $2.m_numConstants); }
  /* allow extra comma, see http://bugs.webkit.org/show_bug.cgi?id=5939 */
  | OPENBRACE PropertyList ',' CLOSEBRACE            { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) ObjectLiteralNode(GLOBAL_DATA, $2.m_node.head), $2.m_features, $2.m_numConstants); }
;

PrimaryExprNoBrace:
    THISTOKEN                           { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) ThisNode(GLOBAL_DATA), ThisFeature, 0); }
  | Literal
  | ArrayLiteral
  | IDENT                               { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) ResolveNode(GLOBAL_DATA, *$1, @1.first_column), (*$1 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0, 0); }
  | '(' Expr ')'                        { $$ = $2; }
;

ArrayLiteral:
    '[' ElisionOpt ']'                  { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) ArrayNode(GLOBAL_DATA, $2), 0, $2 ? 1 : 0); }
  | '[' ElementList ']'                 { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) ArrayNode(GLOBAL_DATA, $2.m_node.head), $2.m_features, $2.m_numConstants); }
  | '[' ElementList ',' ElisionOpt ']'  { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) ArrayNode(GLOBAL_DATA, $4, $2.m_node.head), $2.m_features, $4 ? $2.m_numConstants + 1 : $2.m_numConstants); }
;

ElementList:
    ElisionOpt AssignmentExpr           { $$.m_node.head = new (GLOBAL_DATA) ElementNode(GLOBAL_DATA, $1, $2.m_node);
                                          $$.m_node.tail = $$.m_node.head;
                                          $$.m_features = $2.m_features;
                                          $$.m_numConstants = $2.m_numConstants; }
  | ElementList ',' ElisionOpt AssignmentExpr
                                        { $$.m_node.head = $1.m_node.head;
                                          $$.m_node.tail = new (GLOBAL_DATA) ElementNode(GLOBAL_DATA, $1.m_node.tail, $3, $4.m_node);
                                          $$.m_features = $1.m_features | $4.m_features;
                                          $$.m_numConstants = $1.m_numConstants + $4.m_numConstants; }
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
  | FunctionExpr                        { $$ = createNodeInfo<ExpressionNode*>($1.m_node, $1.m_features, $1.m_numConstants); }
  | MemberExpr '[' Expr ']'             { BracketAccessorNode* node = new (GLOBAL_DATA) BracketAccessorNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature);
                                          setExceptionLocation(node, @1.first_column, @1.last_column, @4.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); 
                                        }
  | MemberExpr '.' IDENT                { DotAccessorNode* node = new (GLOBAL_DATA) DotAccessorNode(GLOBAL_DATA, $1.m_node, *$3);
                                          setExceptionLocation(node, @1.first_column, @1.last_column, @3.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features, $1.m_numConstants);
                                        }
  | NEW MemberExpr Arguments            { NewExprNode* node = new (GLOBAL_DATA) NewExprNode(GLOBAL_DATA, $2.m_node, $3.m_node);
                                          setExceptionLocation(node, @1.first_column, @2.last_column, @3.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $2.m_features | $3.m_features, $2.m_numConstants + $3.m_numConstants);
                                        }
;

MemberExprNoBF:
    PrimaryExprNoBrace
  | MemberExprNoBF '[' Expr ']'         { BracketAccessorNode* node = new (GLOBAL_DATA) BracketAccessorNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature);
                                          setExceptionLocation(node, @1.first_column, @1.last_column, @4.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); 
                                        }
  | MemberExprNoBF '.' IDENT            { DotAccessorNode* node = new (GLOBAL_DATA) DotAccessorNode(GLOBAL_DATA, $1.m_node, *$3);
                                          setExceptionLocation(node, @1.first_column, @1.last_column, @3.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features, $1.m_numConstants);
                                        }
  | NEW MemberExpr Arguments            { NewExprNode* node = new (GLOBAL_DATA) NewExprNode(GLOBAL_DATA, $2.m_node, $3.m_node);
                                          setExceptionLocation(node, @1.first_column, @2.last_column, @3.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $2.m_features | $3.m_features, $2.m_numConstants + $3.m_numConstants);
                                        }
;

NewExpr:
    MemberExpr
  | NEW NewExpr                         { NewExprNode* node = new (GLOBAL_DATA) NewExprNode(GLOBAL_DATA, $2.m_node);
                                          setExceptionLocation(node, @1.first_column, @2.last_column, @2.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $2.m_features, $2.m_numConstants); 
                                        }
;

NewExprNoBF:
    MemberExprNoBF
  | NEW NewExpr                         { NewExprNode* node = new (GLOBAL_DATA) NewExprNode(GLOBAL_DATA, $2.m_node);
                                          setExceptionLocation(node, @1.first_column, @2.last_column, @2.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $2.m_features, $2.m_numConstants);
                                        }
;

CallExpr:
    MemberExpr Arguments                { $$ = makeFunctionCallNode(GLOBAL_DATA, $1, $2, @1.first_column, @1.last_column, @2.last_column); }
  | CallExpr Arguments                  { $$ = makeFunctionCallNode(GLOBAL_DATA, $1, $2, @1.first_column, @1.last_column, @2.last_column); }
  | CallExpr '[' Expr ']'               { BracketAccessorNode* node = new (GLOBAL_DATA) BracketAccessorNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature);
                                          setExceptionLocation(node, @1.first_column, @1.last_column, @4.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); 
                                        }
  | CallExpr '.' IDENT                  { DotAccessorNode* node = new (GLOBAL_DATA) DotAccessorNode(GLOBAL_DATA, $1.m_node, *$3);
                                          setExceptionLocation(node, @1.first_column, @1.last_column, @3.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features, $1.m_numConstants); }
;

CallExprNoBF:
    MemberExprNoBF Arguments            { $$ = makeFunctionCallNode(GLOBAL_DATA, $1, $2, @1.first_column, @1.last_column, @2.last_column); }
  | CallExprNoBF Arguments              { $$ = makeFunctionCallNode(GLOBAL_DATA, $1, $2, @1.first_column, @1.last_column, @2.last_column); }
  | CallExprNoBF '[' Expr ']'           { BracketAccessorNode* node = new (GLOBAL_DATA) BracketAccessorNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature);
                                          setExceptionLocation(node, @1.first_column, @1.last_column, @4.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); 
                                        }
  | CallExprNoBF '.' IDENT              { DotAccessorNode* node = new (GLOBAL_DATA) DotAccessorNode(GLOBAL_DATA, $1.m_node, *$3);
                                          setExceptionLocation(node, @1.first_column, @1.last_column, @3.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features, $1.m_numConstants); 
                                        }
;

Arguments:
    '(' ')'                             { $$ = createNodeInfo<ArgumentsNode*>(new (GLOBAL_DATA) ArgumentsNode(GLOBAL_DATA), 0, 0); }
  | '(' ArgumentList ')'                { $$ = createNodeInfo<ArgumentsNode*>(new (GLOBAL_DATA) ArgumentsNode(GLOBAL_DATA, $2.m_node.head), $2.m_features, $2.m_numConstants); }
;

ArgumentList:
    AssignmentExpr                      { $$.m_node.head = new (GLOBAL_DATA) ArgumentListNode(GLOBAL_DATA, $1.m_node);
                                          $$.m_node.tail = $$.m_node.head;
                                          $$.m_features = $1.m_features;
                                          $$.m_numConstants = $1.m_numConstants; }
  | ArgumentList ',' AssignmentExpr     { $$.m_node.head = $1.m_node.head;
                                          $$.m_node.tail = new (GLOBAL_DATA) ArgumentListNode(GLOBAL_DATA, $1.m_node.tail, $3.m_node);
                                          $$.m_features = $1.m_features | $3.m_features;
                                          $$.m_numConstants = $1.m_numConstants + $3.m_numConstants; }
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
  | LeftHandSideExpr PLUSPLUS           { $$ = createNodeInfo<ExpressionNode*>(makePostfixNode(GLOBAL_DATA, $1.m_node, OpPlusPlus, @1.first_column, @1.last_column, @2.last_column), $1.m_features | AssignFeature, $1.m_numConstants); }
  | LeftHandSideExpr MINUSMINUS         { $$ = createNodeInfo<ExpressionNode*>(makePostfixNode(GLOBAL_DATA, $1.m_node, OpMinusMinus, @1.first_column, @1.last_column, @2.last_column), $1.m_features | AssignFeature, $1.m_numConstants); }
;

PostfixExprNoBF:
    LeftHandSideExprNoBF
  | LeftHandSideExprNoBF PLUSPLUS       { $$ = createNodeInfo<ExpressionNode*>(makePostfixNode(GLOBAL_DATA, $1.m_node, OpPlusPlus, @1.first_column, @1.last_column, @2.last_column), $1.m_features | AssignFeature, $1.m_numConstants); }
  | LeftHandSideExprNoBF MINUSMINUS     { $$ = createNodeInfo<ExpressionNode*>(makePostfixNode(GLOBAL_DATA, $1.m_node, OpMinusMinus, @1.first_column, @1.last_column, @2.last_column), $1.m_features | AssignFeature, $1.m_numConstants); }
;

UnaryExprCommon:
    DELETETOKEN UnaryExpr               { $$ = createNodeInfo<ExpressionNode*>(makeDeleteNode(GLOBAL_DATA, $2.m_node, @1.first_column, @2.last_column, @2.last_column), $2.m_features, $2.m_numConstants); }
  | VOIDTOKEN UnaryExpr                 { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) VoidNode(GLOBAL_DATA, $2.m_node), $2.m_features, $2.m_numConstants + 1); }
  | TYPEOF UnaryExpr                    { $$ = createNodeInfo<ExpressionNode*>(makeTypeOfNode(GLOBAL_DATA, $2.m_node), $2.m_features, $2.m_numConstants); }
  | PLUSPLUS UnaryExpr                  { $$ = createNodeInfo<ExpressionNode*>(makePrefixNode(GLOBAL_DATA, $2.m_node, OpPlusPlus, @1.first_column, @2.first_column + 1, @2.last_column), $2.m_features | AssignFeature, $2.m_numConstants); }
  | AUTOPLUSPLUS UnaryExpr              { $$ = createNodeInfo<ExpressionNode*>(makePrefixNode(GLOBAL_DATA, $2.m_node, OpPlusPlus, @1.first_column, @2.first_column + 1, @2.last_column), $2.m_features | AssignFeature, $2.m_numConstants); }
  | MINUSMINUS UnaryExpr                { $$ = createNodeInfo<ExpressionNode*>(makePrefixNode(GLOBAL_DATA, $2.m_node, OpMinusMinus, @1.first_column, @2.first_column + 1, @2.last_column), $2.m_features | AssignFeature, $2.m_numConstants); }
  | AUTOMINUSMINUS UnaryExpr            { $$ = createNodeInfo<ExpressionNode*>(makePrefixNode(GLOBAL_DATA, $2.m_node, OpMinusMinus, @1.first_column, @2.first_column + 1, @2.last_column), $2.m_features | AssignFeature, $2.m_numConstants); }
  | '+' UnaryExpr                       { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) UnaryPlusNode(GLOBAL_DATA, $2.m_node), $2.m_features, $2.m_numConstants); }
  | '-' UnaryExpr                       { $$ = createNodeInfo<ExpressionNode*>(makeNegateNode(GLOBAL_DATA, $2.m_node), $2.m_features, $2.m_numConstants); }
  | '~' UnaryExpr                       { $$ = createNodeInfo<ExpressionNode*>(makeBitwiseNotNode(GLOBAL_DATA, $2.m_node), $2.m_features, $2.m_numConstants); }
  | '!' UnaryExpr                       { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) LogicalNotNode(GLOBAL_DATA, $2.m_node), $2.m_features, $2.m_numConstants); }

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
  | MultiplicativeExpr '*' UnaryExpr    { $$ = createNodeInfo<ExpressionNode*>(makeMultNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | MultiplicativeExpr '/' UnaryExpr    { $$ = createNodeInfo<ExpressionNode*>(makeDivNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | MultiplicativeExpr '%' UnaryExpr    { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) ModNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

MultiplicativeExprNoBF:
    UnaryExprNoBF
  | MultiplicativeExprNoBF '*' UnaryExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(makeMultNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | MultiplicativeExprNoBF '/' UnaryExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(makeDivNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | MultiplicativeExprNoBF '%' UnaryExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) ModNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

AdditiveExpr:
    MultiplicativeExpr
  | AdditiveExpr '+' MultiplicativeExpr { $$ = createNodeInfo<ExpressionNode*>(makeAddNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | AdditiveExpr '-' MultiplicativeExpr { $$ = createNodeInfo<ExpressionNode*>(makeSubNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

AdditiveExprNoBF:
    MultiplicativeExprNoBF
  | AdditiveExprNoBF '+' MultiplicativeExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(makeAddNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | AdditiveExprNoBF '-' MultiplicativeExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(makeSubNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

ShiftExpr:
    AdditiveExpr
  | ShiftExpr LSHIFT AdditiveExpr       { $$ = createNodeInfo<ExpressionNode*>(makeLeftShiftNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | ShiftExpr RSHIFT AdditiveExpr       { $$ = createNodeInfo<ExpressionNode*>(makeRightShiftNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | ShiftExpr URSHIFT AdditiveExpr      { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) UnsignedRightShiftNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

ShiftExprNoBF:
    AdditiveExprNoBF
  | ShiftExprNoBF LSHIFT AdditiveExpr   { $$ = createNodeInfo<ExpressionNode*>(makeLeftShiftNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | ShiftExprNoBF RSHIFT AdditiveExpr   { $$ = createNodeInfo<ExpressionNode*>(makeRightShiftNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | ShiftExprNoBF URSHIFT AdditiveExpr  { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) UnsignedRightShiftNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

RelationalExpr:
    ShiftExpr
  | RelationalExpr '<' ShiftExpr        { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) LessNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExpr '>' ShiftExpr        { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) GreaterNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExpr LE ShiftExpr         { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) LessEqNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExpr GE ShiftExpr         { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) GreaterEqNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExpr INSTANCEOF ShiftExpr { InstanceOfNode* node = new (GLOBAL_DATA) InstanceOfNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature);
                                          setExceptionLocation(node, @1.first_column, @3.first_column, @3.last_column);  
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExpr INTOKEN ShiftExpr    { InNode* node = new (GLOBAL_DATA) InNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature);
                                          setExceptionLocation(node, @1.first_column, @3.first_column, @3.last_column);  
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

RelationalExprNoIn:
    ShiftExpr
  | RelationalExprNoIn '<' ShiftExpr    { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) LessNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExprNoIn '>' ShiftExpr    { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) GreaterNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExprNoIn LE ShiftExpr     { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) LessEqNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExprNoIn GE ShiftExpr     { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) GreaterEqNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExprNoIn INSTANCEOF ShiftExpr
                                        { InstanceOfNode* node = new (GLOBAL_DATA) InstanceOfNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature);
                                          setExceptionLocation(node, @1.first_column, @3.first_column, @3.last_column);  
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

RelationalExprNoBF:
    ShiftExprNoBF
  | RelationalExprNoBF '<' ShiftExpr    { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) LessNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExprNoBF '>' ShiftExpr    { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) GreaterNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExprNoBF LE ShiftExpr     { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) LessEqNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExprNoBF GE ShiftExpr     { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) GreaterEqNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExprNoBF INSTANCEOF ShiftExpr
                                        { InstanceOfNode* node = new (GLOBAL_DATA) InstanceOfNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature);
                                          setExceptionLocation(node, @1.first_column, @3.first_column, @3.last_column);  
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExprNoBF INTOKEN ShiftExpr 
                                        { InNode* node = new (GLOBAL_DATA) InNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature);
                                          setExceptionLocation(node, @1.first_column, @3.first_column, @3.last_column);  
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

EqualityExpr:
    RelationalExpr
  | EqualityExpr EQEQ RelationalExpr    { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) EqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | EqualityExpr NE RelationalExpr      { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) NotEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | EqualityExpr STREQ RelationalExpr   { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) StrictEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | EqualityExpr STRNEQ RelationalExpr  { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) NotStrictEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

EqualityExprNoIn:
    RelationalExprNoIn
  | EqualityExprNoIn EQEQ RelationalExprNoIn
                                        { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) EqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | EqualityExprNoIn NE RelationalExprNoIn
                                        { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) NotEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | EqualityExprNoIn STREQ RelationalExprNoIn
                                        { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) StrictEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | EqualityExprNoIn STRNEQ RelationalExprNoIn
                                        { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) NotStrictEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

EqualityExprNoBF:
    RelationalExprNoBF
  | EqualityExprNoBF EQEQ RelationalExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) EqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | EqualityExprNoBF NE RelationalExpr  { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) NotEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | EqualityExprNoBF STREQ RelationalExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) StrictEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | EqualityExprNoBF STRNEQ RelationalExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) NotStrictEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

BitwiseANDExpr:
    EqualityExpr
  | BitwiseANDExpr '&' EqualityExpr     { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) BitAndNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

BitwiseANDExprNoIn:
    EqualityExprNoIn
  | BitwiseANDExprNoIn '&' EqualityExprNoIn
                                        { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) BitAndNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

BitwiseANDExprNoBF:
    EqualityExprNoBF
  | BitwiseANDExprNoBF '&' EqualityExpr { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) BitAndNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

BitwiseXORExpr:
    BitwiseANDExpr
  | BitwiseXORExpr '^' BitwiseANDExpr   { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) BitXOrNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

BitwiseXORExprNoIn:
    BitwiseANDExprNoIn
  | BitwiseXORExprNoIn '^' BitwiseANDExprNoIn
                                        { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) BitXOrNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

BitwiseXORExprNoBF:
    BitwiseANDExprNoBF
  | BitwiseXORExprNoBF '^' BitwiseANDExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) BitXOrNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

BitwiseORExpr:
    BitwiseXORExpr
  | BitwiseORExpr '|' BitwiseXORExpr    { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) BitOrNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

BitwiseORExprNoIn:
    BitwiseXORExprNoIn
  | BitwiseORExprNoIn '|' BitwiseXORExprNoIn
                                        { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) BitOrNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

BitwiseORExprNoBF:
    BitwiseXORExprNoBF
  | BitwiseORExprNoBF '|' BitwiseXORExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) BitOrNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

LogicalANDExpr:
    BitwiseORExpr
  | LogicalANDExpr AND BitwiseORExpr    { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) LogicalOpNode(GLOBAL_DATA, $1.m_node, $3.m_node, OpLogicalAnd), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

LogicalANDExprNoIn:
    BitwiseORExprNoIn
  | LogicalANDExprNoIn AND BitwiseORExprNoIn
                                        { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) LogicalOpNode(GLOBAL_DATA, $1.m_node, $3.m_node, OpLogicalAnd), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

LogicalANDExprNoBF:
    BitwiseORExprNoBF
  | LogicalANDExprNoBF AND BitwiseORExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) LogicalOpNode(GLOBAL_DATA, $1.m_node, $3.m_node, OpLogicalAnd), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

LogicalORExpr:
    LogicalANDExpr
  | LogicalORExpr OR LogicalANDExpr     { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) LogicalOpNode(GLOBAL_DATA, $1.m_node, $3.m_node, OpLogicalOr), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

LogicalORExprNoIn:
    LogicalANDExprNoIn
  | LogicalORExprNoIn OR LogicalANDExprNoIn
                                        { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) LogicalOpNode(GLOBAL_DATA, $1.m_node, $3.m_node, OpLogicalOr), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

LogicalORExprNoBF:
    LogicalANDExprNoBF
  | LogicalORExprNoBF OR LogicalANDExpr { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) LogicalOpNode(GLOBAL_DATA, $1.m_node, $3.m_node, OpLogicalOr), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

ConditionalExpr:
    LogicalORExpr
  | LogicalORExpr '?' AssignmentExpr ':' AssignmentExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) ConditionalNode(GLOBAL_DATA, $1.m_node, $3.m_node, $5.m_node), $1.m_features | $3.m_features | $5.m_features, $1.m_numConstants + $3.m_numConstants + $5.m_numConstants); }
;

ConditionalExprNoIn:
    LogicalORExprNoIn
  | LogicalORExprNoIn '?' AssignmentExprNoIn ':' AssignmentExprNoIn
                                        { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) ConditionalNode(GLOBAL_DATA, $1.m_node, $3.m_node, $5.m_node), $1.m_features | $3.m_features | $5.m_features, $1.m_numConstants + $3.m_numConstants + $5.m_numConstants); }
;

ConditionalExprNoBF:
    LogicalORExprNoBF
  | LogicalORExprNoBF '?' AssignmentExpr ':' AssignmentExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(new (GLOBAL_DATA) ConditionalNode(GLOBAL_DATA, $1.m_node, $3.m_node, $5.m_node), $1.m_features | $3.m_features | $5.m_features, $1.m_numConstants + $3.m_numConstants + $5.m_numConstants); }
;

AssignmentExpr:
    ConditionalExpr
  | LeftHandSideExpr AssignmentOperator AssignmentExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(makeAssignNode(GLOBAL_DATA, $1.m_node, $2, $3.m_node, $1.m_features & AssignFeature, $3.m_features & AssignFeature, 
                                                                                                     @1.first_column, @2.first_column + 1, @3.last_column), $1.m_features | $3.m_features | AssignFeature, $1.m_numConstants + $3.m_numConstants); 
                                        }
;

AssignmentExprNoIn:
    ConditionalExprNoIn
  | LeftHandSideExpr AssignmentOperator AssignmentExprNoIn
                                        { $$ = createNodeInfo<ExpressionNode*>(makeAssignNode(GLOBAL_DATA, $1.m_node, $2, $3.m_node, $1.m_features & AssignFeature, $3.m_features & AssignFeature, 
                                                                                                     @1.first_column, @2.first_column + 1, @3.last_column), $1.m_features | $3.m_features | AssignFeature, $1.m_numConstants + $3.m_numConstants);
                                        }
;

AssignmentExprNoBF:
    ConditionalExprNoBF
  | LeftHandSideExprNoBF AssignmentOperator AssignmentExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(makeAssignNode(GLOBAL_DATA, $1.m_node, $2, $3.m_node, $1.m_features & AssignFeature, $3.m_features & AssignFeature,
                                                                                                     @1.first_column, @2.first_column + 1, @3.last_column), $1.m_features | $3.m_features | AssignFeature, $1.m_numConstants + $3.m_numConstants); 
                                        }
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
  | Expr ',' AssignmentExpr             { $$ = createNodeInfo<ExpressionNode*>(combineCommaNodes(GLOBAL_DATA, $1.m_node, $3.m_node), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

ExprNoIn:
    AssignmentExprNoIn
  | ExprNoIn ',' AssignmentExprNoIn     { $$ = createNodeInfo<ExpressionNode*>(combineCommaNodes(GLOBAL_DATA, $1.m_node, $3.m_node), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

ExprNoBF:
    AssignmentExprNoBF
  | ExprNoBF ',' AssignmentExpr         { $$ = createNodeInfo<ExpressionNode*>(combineCommaNodes(GLOBAL_DATA, $1.m_node, $3.m_node), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

Statement:
    Block
  | VariableStatement
  | ConstStatement
  | FunctionDeclaration
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
    OPENBRACE CLOSEBRACE                { $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) BlockNode(GLOBAL_DATA, 0), 0, 0, 0, 0);
                                          setStatementLocation($$.m_node, @1, @2); }
  | OPENBRACE SourceElements CLOSEBRACE { $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) BlockNode(GLOBAL_DATA, $2.m_node), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_features, $2.m_numConstants);
                                          setStatementLocation($$.m_node, @1, @3); }
;

VariableStatement:
    VAR VariableDeclarationList ';'     { $$ = createNodeDeclarationInfo<StatementNode*>(makeVarStatementNode(GLOBAL_DATA, $2.m_node), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_features, $2.m_numConstants);
                                          setStatementLocation($$.m_node, @1, @3); }
  | VAR VariableDeclarationList error   { $$ = createNodeDeclarationInfo<StatementNode*>(makeVarStatementNode(GLOBAL_DATA, $2.m_node), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_features, $2.m_numConstants);
                                          setStatementLocation($$.m_node, @1, @2);
                                          AUTO_SEMICOLON; }
;

VariableDeclarationList:
    IDENT                               { $$.m_node = 0;
                                          $$.m_varDeclarations = new (GLOBAL_DATA) ParserArenaData<DeclarationStacks::VarStack>;
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, *$1, 0);
                                          $$.m_funcDeclarations = 0;
                                          $$.m_features = (*$1 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0;
                                          $$.m_numConstants = 0;
                                        }
  | IDENT Initializer                   { AssignResolveNode* node = new (GLOBAL_DATA) AssignResolveNode(GLOBAL_DATA, *$1, $2.m_node, $2.m_features & AssignFeature);
                                          setExceptionLocation(node, @1.first_column, @2.first_column + 1, @2.last_column);
                                          $$.m_node = node;
                                          $$.m_varDeclarations = new (GLOBAL_DATA) ParserArenaData<DeclarationStacks::VarStack>;
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, *$1, DeclarationStacks::HasInitializer);
                                          $$.m_funcDeclarations = 0;
                                          $$.m_features = ((*$1 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0) | $2.m_features;
                                          $$.m_numConstants = $2.m_numConstants;
                                        }
  | VariableDeclarationList ',' IDENT
                                        { $$.m_node = $1.m_node;
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, *$3, 0);
                                          $$.m_funcDeclarations = 0;
                                          $$.m_features = $1.m_features | ((*$3 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0);
                                          $$.m_numConstants = $1.m_numConstants;
                                        }
  | VariableDeclarationList ',' IDENT Initializer
                                        { AssignResolveNode* node = new (GLOBAL_DATA) AssignResolveNode(GLOBAL_DATA, *$3, $4.m_node, $4.m_features & AssignFeature);
                                          setExceptionLocation(node, @3.first_column, @4.first_column + 1, @4.last_column);
                                          $$.m_node = combineCommaNodes(GLOBAL_DATA, $1.m_node, node);
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, *$3, DeclarationStacks::HasInitializer);
                                          $$.m_funcDeclarations = 0;
                                          $$.m_features = $1.m_features | ((*$3 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0) | $4.m_features;
                                          $$.m_numConstants = $1.m_numConstants + $4.m_numConstants;
                                        }
;

VariableDeclarationListNoIn:
    IDENT                               { $$.m_node = 0;
                                          $$.m_varDeclarations = new (GLOBAL_DATA) ParserArenaData<DeclarationStacks::VarStack>;
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, *$1, 0);
                                          $$.m_funcDeclarations = 0;
                                          $$.m_features = (*$1 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0;
                                          $$.m_numConstants = 0;
                                        }
  | IDENT InitializerNoIn               { AssignResolveNode* node = new (GLOBAL_DATA) AssignResolveNode(GLOBAL_DATA, *$1, $2.m_node, $2.m_features & AssignFeature);
                                          setExceptionLocation(node, @1.first_column, @2.first_column + 1, @2.last_column);
                                          $$.m_node = node;
                                          $$.m_varDeclarations = new (GLOBAL_DATA) ParserArenaData<DeclarationStacks::VarStack>;
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, *$1, DeclarationStacks::HasInitializer);
                                          $$.m_funcDeclarations = 0;
                                          $$.m_features = ((*$1 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0) | $2.m_features;
                                          $$.m_numConstants = $2.m_numConstants;
                                        }
  | VariableDeclarationListNoIn ',' IDENT
                                        { $$.m_node = $1.m_node;
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, *$3, 0);
                                          $$.m_funcDeclarations = 0;
                                          $$.m_features = $1.m_features | ((*$3 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0);
                                          $$.m_numConstants = $1.m_numConstants;
                                        }
  | VariableDeclarationListNoIn ',' IDENT InitializerNoIn
                                        { AssignResolveNode* node = new (GLOBAL_DATA) AssignResolveNode(GLOBAL_DATA, *$3, $4.m_node, $4.m_features & AssignFeature);
                                          setExceptionLocation(node, @3.first_column, @4.first_column + 1, @4.last_column);
                                          $$.m_node = combineCommaNodes(GLOBAL_DATA, $1.m_node, node);
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, *$3, DeclarationStacks::HasInitializer);
                                          $$.m_funcDeclarations = 0;
                                          $$.m_features = $1.m_features | ((*$3 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0) | $4.m_features;
                                          $$.m_numConstants = $1.m_numConstants + $4.m_numConstants;
                                        }
;

ConstStatement:
    CONSTTOKEN ConstDeclarationList ';' { $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) ConstStatementNode(GLOBAL_DATA, $2.m_node.head), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_features, $2.m_numConstants);
                                          setStatementLocation($$.m_node, @1, @3); }
  | CONSTTOKEN ConstDeclarationList error
                                        { $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) ConstStatementNode(GLOBAL_DATA, $2.m_node.head), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_features, $2.m_numConstants);
                                          setStatementLocation($$.m_node, @1, @2); AUTO_SEMICOLON; }
;

ConstDeclarationList:
    ConstDeclaration                    { $$.m_node.head = $1.m_node;
                                          $$.m_node.tail = $$.m_node.head;
                                          $$.m_varDeclarations = new (GLOBAL_DATA) ParserArenaData<DeclarationStacks::VarStack>;
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, $1.m_node);
                                          $$.m_funcDeclarations = 0; 
                                          $$.m_features = $1.m_features;
                                          $$.m_numConstants = $1.m_numConstants;
    }
  | ConstDeclarationList ',' ConstDeclaration
                                        { $$.m_node.head = $1.m_node.head;
                                          $1.m_node.tail->m_next = $3.m_node;
                                          $$.m_node.tail = $3.m_node;
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, $3.m_node);
                                          $$.m_funcDeclarations = 0;
                                          $$.m_features = $1.m_features | $3.m_features;
                                          $$.m_numConstants = $1.m_numConstants + $3.m_numConstants; }
;

ConstDeclaration:
    IDENT                               { $$ = createNodeInfo<ConstDeclNode*>(new (GLOBAL_DATA) ConstDeclNode(GLOBAL_DATA, *$1, 0), (*$1 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0, 0); }
  | IDENT Initializer                   { $$ = createNodeInfo<ConstDeclNode*>(new (GLOBAL_DATA) ConstDeclNode(GLOBAL_DATA, *$1, $2.m_node), ((*$1 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0) | $2.m_features, $2.m_numConstants); }
;

Initializer:
    '=' AssignmentExpr                  { $$ = $2; }
;

InitializerNoIn:
    '=' AssignmentExprNoIn              { $$ = $2; }
;

EmptyStatement:
    ';'                                 { $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) EmptyStatementNode(GLOBAL_DATA), 0, 0, 0, 0); }
;

ExprStatement:
    ExprNoBF ';'                        { $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) ExprStatementNode(GLOBAL_DATA, $1.m_node), 0, 0, $1.m_features, $1.m_numConstants);
                                          setStatementLocation($$.m_node, @1, @2); }
  | ExprNoBF error                      { $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) ExprStatementNode(GLOBAL_DATA, $1.m_node), 0, 0, $1.m_features, $1.m_numConstants);
                                          setStatementLocation($$.m_node, @1, @1); AUTO_SEMICOLON; }
;

IfStatement:
    IF '(' Expr ')' Statement %prec IF_WITHOUT_ELSE
                                        { $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) IfNode(GLOBAL_DATA, $3.m_node, $5.m_node), $5.m_varDeclarations, $5.m_funcDeclarations, $3.m_features | $5.m_features, $3.m_numConstants + $5.m_numConstants);
                                          setStatementLocation($$.m_node, @1, @4); }
  | IF '(' Expr ')' Statement ELSE Statement
                                        { $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) IfElseNode(GLOBAL_DATA, $3.m_node, $5.m_node, $7.m_node), 
                                                                                         mergeDeclarationLists($5.m_varDeclarations, $7.m_varDeclarations),
                                                                                         mergeDeclarationLists($5.m_funcDeclarations, $7.m_funcDeclarations),
                                                                                         $3.m_features | $5.m_features | $7.m_features,
                                                                                         $3.m_numConstants + $5.m_numConstants + $7.m_numConstants); 
                                          setStatementLocation($$.m_node, @1, @4); }
;

IterationStatement:
    DO Statement WHILE '(' Expr ')' ';'    { $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) DoWhileNode(GLOBAL_DATA, $2.m_node, $5.m_node), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_features | $5.m_features, $2.m_numConstants + $5.m_numConstants);
                                             setStatementLocation($$.m_node, @1, @3); }
  | DO Statement WHILE '(' Expr ')' error  { $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) DoWhileNode(GLOBAL_DATA, $2.m_node, $5.m_node), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_features | $5.m_features, $2.m_numConstants + $5.m_numConstants);
                                             setStatementLocation($$.m_node, @1, @3); } // Always performs automatic semicolon insertion.
  | WHILE '(' Expr ')' Statement        { $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) WhileNode(GLOBAL_DATA, $3.m_node, $5.m_node), $5.m_varDeclarations, $5.m_funcDeclarations, $3.m_features | $5.m_features, $3.m_numConstants + $5.m_numConstants);
                                          setStatementLocation($$.m_node, @1, @4); }
  | FOR '(' ExprNoInOpt ';' ExprOpt ';' ExprOpt ')' Statement
                                        { $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) ForNode(GLOBAL_DATA, $3.m_node, $5.m_node, $7.m_node, $9.m_node, false), $9.m_varDeclarations, $9.m_funcDeclarations, 
                                                                                         $3.m_features | $5.m_features | $7.m_features | $9.m_features,
                                                                                         $3.m_numConstants + $5.m_numConstants + $7.m_numConstants + $9.m_numConstants);
                                          setStatementLocation($$.m_node, @1, @8); 
                                        }
  | FOR '(' VAR VariableDeclarationListNoIn ';' ExprOpt ';' ExprOpt ')' Statement
                                        { $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) ForNode(GLOBAL_DATA, $4.m_node, $6.m_node, $8.m_node, $10.m_node, true),
                                                                                         mergeDeclarationLists($4.m_varDeclarations, $10.m_varDeclarations),
                                                                                         mergeDeclarationLists($4.m_funcDeclarations, $10.m_funcDeclarations),
                                                                                         $4.m_features | $6.m_features | $8.m_features | $10.m_features,
                                                                                         $4.m_numConstants + $6.m_numConstants + $8.m_numConstants + $10.m_numConstants);
                                          setStatementLocation($$.m_node, @1, @9); }
  | FOR '(' LeftHandSideExpr INTOKEN Expr ')' Statement
                                        {
                                            ForInNode* node = new (GLOBAL_DATA) ForInNode(GLOBAL_DATA, $3.m_node, $5.m_node, $7.m_node);
                                            setExceptionLocation(node, @3.first_column, @3.last_column, @5.last_column);
                                            $$ = createNodeDeclarationInfo<StatementNode*>(node, $7.m_varDeclarations, $7.m_funcDeclarations,
                                                                                           $3.m_features | $5.m_features | $7.m_features,
                                                                                           $3.m_numConstants + $5.m_numConstants + $7.m_numConstants);
                                            setStatementLocation($$.m_node, @1, @6);
                                        }
  | FOR '(' VAR IDENT INTOKEN Expr ')' Statement
                                        { ForInNode *forIn = new (GLOBAL_DATA) ForInNode(GLOBAL_DATA, *$4, 0, $6.m_node, $8.m_node, @5.first_column, @5.first_column - @4.first_column, @6.last_column - @5.first_column);
                                          setExceptionLocation(forIn, @4.first_column, @5.first_column + 1, @6.last_column);
                                          appendToVarDeclarationList(GLOBAL_DATA, $8.m_varDeclarations, *$4, DeclarationStacks::HasInitializer);
                                          $$ = createNodeDeclarationInfo<StatementNode*>(forIn, $8.m_varDeclarations, $8.m_funcDeclarations, ((*$4 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0) | $6.m_features | $8.m_features, $6.m_numConstants + $8.m_numConstants);
                                          setStatementLocation($$.m_node, @1, @7); }
  | FOR '(' VAR IDENT InitializerNoIn INTOKEN Expr ')' Statement
                                        { ForInNode *forIn = new (GLOBAL_DATA) ForInNode(GLOBAL_DATA, *$4, $5.m_node, $7.m_node, $9.m_node, @5.first_column, @5.first_column - @4.first_column, @5.last_column - @5.first_column);
                                          setExceptionLocation(forIn, @4.first_column, @6.first_column + 1, @7.last_column);
                                          appendToVarDeclarationList(GLOBAL_DATA, $9.m_varDeclarations, *$4, DeclarationStacks::HasInitializer);
                                          $$ = createNodeDeclarationInfo<StatementNode*>(forIn, $9.m_varDeclarations, $9.m_funcDeclarations,
                                                                                         ((*$4 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0) | $5.m_features | $7.m_features | $9.m_features,
                                                                                         $5.m_numConstants + $7.m_numConstants + $9.m_numConstants);
                                          setStatementLocation($$.m_node, @1, @8); }
;

ExprOpt:
    /* nothing */                       { $$ = createNodeInfo<ExpressionNode*>(0, 0, 0); }
  | Expr
;

ExprNoInOpt:
    /* nothing */                       { $$ = createNodeInfo<ExpressionNode*>(0, 0, 0); }
  | ExprNoIn
;

ContinueStatement:
    CONTINUE ';'                        { ContinueNode* node = new (GLOBAL_DATA) ContinueNode(GLOBAL_DATA);
                                          setExceptionLocation(node, @1.first_column, @1.last_column, @1.last_column); 
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, 0, 0);
                                          setStatementLocation($$.m_node, @1, @2); }
  | CONTINUE error                      { ContinueNode* node = new (GLOBAL_DATA) ContinueNode(GLOBAL_DATA);
                                          setExceptionLocation(node, @1.first_column, @1.last_column, @1.last_column); 
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, 0, 0);
                                          setStatementLocation($$.m_node, @1, @1); AUTO_SEMICOLON; }
  | CONTINUE IDENT ';'                  { ContinueNode* node = new (GLOBAL_DATA) ContinueNode(GLOBAL_DATA, *$2);
                                          setExceptionLocation(node, @1.first_column, @2.last_column, @2.last_column); 
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, 0, 0);
                                          setStatementLocation($$.m_node, @1, @3); }
  | CONTINUE IDENT error                { ContinueNode* node = new (GLOBAL_DATA) ContinueNode(GLOBAL_DATA, *$2);
                                          setExceptionLocation(node, @1.first_column, @2.last_column, @2.last_column); 
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, 0, 0);
                                          setStatementLocation($$.m_node, @1, @2); AUTO_SEMICOLON; }
;

BreakStatement:
    BREAK ';'                           { BreakNode* node = new (GLOBAL_DATA) BreakNode(GLOBAL_DATA);
                                          setExceptionLocation(node, @1.first_column, @1.last_column, @1.last_column);
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, 0, 0); setStatementLocation($$.m_node, @1, @2); }
  | BREAK error                         { BreakNode* node = new (GLOBAL_DATA) BreakNode(GLOBAL_DATA);
                                          setExceptionLocation(node, @1.first_column, @1.last_column, @1.last_column);
                                          $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) BreakNode(GLOBAL_DATA), 0, 0, 0, 0); setStatementLocation($$.m_node, @1, @1); AUTO_SEMICOLON; }
  | BREAK IDENT ';'                     { BreakNode* node = new (GLOBAL_DATA) BreakNode(GLOBAL_DATA, *$2);
                                          setExceptionLocation(node, @1.first_column, @2.last_column, @2.last_column);
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, 0, 0); setStatementLocation($$.m_node, @1, @3); }
  | BREAK IDENT error                   { BreakNode* node = new (GLOBAL_DATA) BreakNode(GLOBAL_DATA, *$2);
                                          setExceptionLocation(node, @1.first_column, @2.last_column, @2.last_column);
                                          $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) BreakNode(GLOBAL_DATA, *$2), 0, 0, 0, 0); setStatementLocation($$.m_node, @1, @2); AUTO_SEMICOLON; }
;

ReturnStatement:
    RETURN ';'                          { ReturnNode* node = new (GLOBAL_DATA) ReturnNode(GLOBAL_DATA, 0); 
                                          setExceptionLocation(node, @1.first_column, @1.last_column, @1.last_column); 
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, 0, 0); setStatementLocation($$.m_node, @1, @2); }
  | RETURN error                        { ReturnNode* node = new (GLOBAL_DATA) ReturnNode(GLOBAL_DATA, 0); 
                                          setExceptionLocation(node, @1.first_column, @1.last_column, @1.last_column); 
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, 0, 0); setStatementLocation($$.m_node, @1, @1); AUTO_SEMICOLON; }
  | RETURN Expr ';'                     { ReturnNode* node = new (GLOBAL_DATA) ReturnNode(GLOBAL_DATA, $2.m_node); 
                                          setExceptionLocation(node, @1.first_column, @2.last_column, @2.last_column);
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, $2.m_features, $2.m_numConstants); setStatementLocation($$.m_node, @1, @3); }
  | RETURN Expr error                   { ReturnNode* node = new (GLOBAL_DATA) ReturnNode(GLOBAL_DATA, $2.m_node); 
                                          setExceptionLocation(node, @1.first_column, @2.last_column, @2.last_column); 
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, $2.m_features, $2.m_numConstants); setStatementLocation($$.m_node, @1, @2); AUTO_SEMICOLON; }
;

WithStatement:
    WITH '(' Expr ')' Statement         { $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) WithNode(GLOBAL_DATA, $3.m_node, $5.m_node, @3.last_column, @3.last_column - @3.first_column),
                                                                                         $5.m_varDeclarations, $5.m_funcDeclarations, $3.m_features | $5.m_features | WithFeature, $3.m_numConstants + $5.m_numConstants);
                                          setStatementLocation($$.m_node, @1, @4); }
;

SwitchStatement:
    SWITCH '(' Expr ')' CaseBlock       { $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) SwitchNode(GLOBAL_DATA, $3.m_node, $5.m_node), $5.m_varDeclarations, $5.m_funcDeclarations,
                                                                                         $3.m_features | $5.m_features, $3.m_numConstants + $5.m_numConstants);
                                          setStatementLocation($$.m_node, @1, @4); }
;

CaseBlock:
    OPENBRACE CaseClausesOpt CLOSEBRACE              { $$ = createNodeDeclarationInfo<CaseBlockNode*>(new (GLOBAL_DATA) CaseBlockNode(GLOBAL_DATA, $2.m_node.head, 0, 0), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_features, $2.m_numConstants); }
  | OPENBRACE CaseClausesOpt DefaultClause CaseClausesOpt CLOSEBRACE
                                        { $$ = createNodeDeclarationInfo<CaseBlockNode*>(new (GLOBAL_DATA) CaseBlockNode(GLOBAL_DATA, $2.m_node.head, $3.m_node, $4.m_node.head),
                                                                                         mergeDeclarationLists(mergeDeclarationLists($2.m_varDeclarations, $3.m_varDeclarations), $4.m_varDeclarations),
                                                                                         mergeDeclarationLists(mergeDeclarationLists($2.m_funcDeclarations, $3.m_funcDeclarations), $4.m_funcDeclarations),
                                                                                         $2.m_features | $3.m_features | $4.m_features,
                                                                                         $2.m_numConstants + $3.m_numConstants + $4.m_numConstants); }
;

CaseClausesOpt:
  /* nothing */                         { $$.m_node.head = 0; $$.m_node.tail = 0; $$.m_varDeclarations = 0; $$.m_funcDeclarations = 0; $$.m_features = 0; $$.m_numConstants = 0; }
  | CaseClauses
;

CaseClauses:
    CaseClause                          { $$.m_node.head = new (GLOBAL_DATA) ClauseListNode(GLOBAL_DATA, $1.m_node);
                                          $$.m_node.tail = $$.m_node.head;
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          $$.m_funcDeclarations = $1.m_funcDeclarations; 
                                          $$.m_features = $1.m_features;
                                          $$.m_numConstants = $1.m_numConstants; }
  | CaseClauses CaseClause              { $$.m_node.head = $1.m_node.head;
                                          $$.m_node.tail = new (GLOBAL_DATA) ClauseListNode(GLOBAL_DATA, $1.m_node.tail, $2.m_node);
                                          $$.m_varDeclarations = mergeDeclarationLists($1.m_varDeclarations, $2.m_varDeclarations);
                                          $$.m_funcDeclarations = mergeDeclarationLists($1.m_funcDeclarations, $2.m_funcDeclarations);
                                          $$.m_features = $1.m_features | $2.m_features;
                                          $$.m_numConstants = $1.m_numConstants + $2.m_numConstants;
                                        }
;

CaseClause:
    CASE Expr ':'                       { $$ = createNodeDeclarationInfo<CaseClauseNode*>(new (GLOBAL_DATA) CaseClauseNode(GLOBAL_DATA, $2.m_node), 0, 0, $2.m_features, $2.m_numConstants); }
  | CASE Expr ':' SourceElements        { $$ = createNodeDeclarationInfo<CaseClauseNode*>(new (GLOBAL_DATA) CaseClauseNode(GLOBAL_DATA, $2.m_node, $4.m_node), $4.m_varDeclarations, $4.m_funcDeclarations, $2.m_features | $4.m_features, $2.m_numConstants + $4.m_numConstants); }
;

DefaultClause:
    DEFAULT ':'                         { $$ = createNodeDeclarationInfo<CaseClauseNode*>(new (GLOBAL_DATA) CaseClauseNode(GLOBAL_DATA, 0), 0, 0, 0, 0); }
  | DEFAULT ':' SourceElements          { $$ = createNodeDeclarationInfo<CaseClauseNode*>(new (GLOBAL_DATA) CaseClauseNode(GLOBAL_DATA, 0, $3.m_node), $3.m_varDeclarations, $3.m_funcDeclarations, $3.m_features, $3.m_numConstants); }
;

LabelledStatement:
    IDENT ':' Statement                 { LabelNode* node = new (GLOBAL_DATA) LabelNode(GLOBAL_DATA, *$1, $3.m_node);
                                          setExceptionLocation(node, @1.first_column, @2.last_column, @2.last_column);
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, $3.m_varDeclarations, $3.m_funcDeclarations, $3.m_features, $3.m_numConstants); }
;

ThrowStatement:
    THROW Expr ';'                      { ThrowNode* node = new (GLOBAL_DATA) ThrowNode(GLOBAL_DATA, $2.m_node);
                                          setExceptionLocation(node, @1.first_column, @2.last_column, @2.last_column);
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, $2.m_features, $2.m_numConstants); setStatementLocation($$.m_node, @1, @2);
                                        }
  | THROW Expr error                    { ThrowNode* node = new (GLOBAL_DATA) ThrowNode(GLOBAL_DATA, $2.m_node);
                                          setExceptionLocation(node, @1.first_column, @2.last_column, @2.last_column);
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, $2.m_features, $2.m_numConstants); setStatementLocation($$.m_node, @1, @2); AUTO_SEMICOLON; 
                                        }
;

TryStatement:
    TRY Block FINALLY Block             { $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) TryNode(GLOBAL_DATA, $2.m_node, GLOBAL_DATA->propertyNames->nullIdentifier, false, 0, $4.m_node),
                                                                                         mergeDeclarationLists($2.m_varDeclarations, $4.m_varDeclarations),
                                                                                         mergeDeclarationLists($2.m_funcDeclarations, $4.m_funcDeclarations),
                                                                                         $2.m_features | $4.m_features,
                                                                                         $2.m_numConstants + $4.m_numConstants);
                                          setStatementLocation($$.m_node, @1, @2); }
  | TRY Block CATCH '(' IDENT ')' Block { $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) TryNode(GLOBAL_DATA, $2.m_node, *$5, ($7.m_features & EvalFeature) != 0, $7.m_node, 0),
                                                                                         mergeDeclarationLists($2.m_varDeclarations, $7.m_varDeclarations),
                                                                                         mergeDeclarationLists($2.m_funcDeclarations, $7.m_funcDeclarations),
                                                                                         $2.m_features | $7.m_features | CatchFeature,
                                                                                         $2.m_numConstants + $7.m_numConstants);
                                          setStatementLocation($$.m_node, @1, @2); }
  | TRY Block CATCH '(' IDENT ')' Block FINALLY Block
                                        { $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) TryNode(GLOBAL_DATA, $2.m_node, *$5, ($7.m_features & EvalFeature) != 0, $7.m_node, $9.m_node),
                                                                                         mergeDeclarationLists(mergeDeclarationLists($2.m_varDeclarations, $7.m_varDeclarations), $9.m_varDeclarations),
                                                                                         mergeDeclarationLists(mergeDeclarationLists($2.m_funcDeclarations, $7.m_funcDeclarations), $9.m_funcDeclarations),
                                                                                         $2.m_features | $7.m_features | $9.m_features | CatchFeature,
                                                                                         $2.m_numConstants + $7.m_numConstants + $9.m_numConstants);
                                          setStatementLocation($$.m_node, @1, @2); }
;

DebuggerStatement:
    DEBUGGER ';'                        { $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) DebuggerStatementNode(GLOBAL_DATA), 0, 0, 0, 0);
                                          setStatementLocation($$.m_node, @1, @2); }
  | DEBUGGER error                      { $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) DebuggerStatementNode(GLOBAL_DATA), 0, 0, 0, 0);
                                          setStatementLocation($$.m_node, @1, @1); AUTO_SEMICOLON; }
;

FunctionDeclaration:
    FUNCTION IDENT '(' ')' OPENBRACE FunctionBody CLOSEBRACE { $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) FuncDeclNode(GLOBAL_DATA, *$2, $6, GLOBAL_DATA->lexer->sourceCode($5, $7, @5.first_line)), 0, new (GLOBAL_DATA) ParserArenaData<DeclarationStacks::FunctionStack>, ((*$2 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0) | ClosureFeature, 0); setStatementLocation($6, @5, @7); $$.m_funcDeclarations->data.append(static_cast<FuncDeclNode*>($$.m_node)->body()); }
  | FUNCTION IDENT '(' FormalParameterList ')' OPENBRACE FunctionBody CLOSEBRACE
      {
          $$ = createNodeDeclarationInfo<StatementNode*>(new (GLOBAL_DATA) FuncDeclNode(GLOBAL_DATA, *$2, $7, GLOBAL_DATA->lexer->sourceCode($6, $8, @6.first_line), $4.m_node.head), 0, new (GLOBAL_DATA) ParserArenaData<DeclarationStacks::FunctionStack>, ((*$2 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0) | $4.m_features | ClosureFeature, 0);
          if ($4.m_features & ArgumentsFeature)
              $7->setUsesArguments();
          setStatementLocation($7, @6, @8);
          $$.m_funcDeclarations->data.append(static_cast<FuncDeclNode*>($$.m_node)->body());
      }
;

FunctionExpr:
    FUNCTION '(' ')' OPENBRACE FunctionBody CLOSEBRACE { $$ = createNodeInfo(new (GLOBAL_DATA) FuncExprNode(GLOBAL_DATA, GLOBAL_DATA->propertyNames->nullIdentifier, $5, GLOBAL_DATA->lexer->sourceCode($4, $6, @4.first_line)), ClosureFeature, 0); setStatementLocation($5, @4, @6); }
    | FUNCTION '(' FormalParameterList ')' OPENBRACE FunctionBody CLOSEBRACE
      {
          $$ = createNodeInfo(new (GLOBAL_DATA) FuncExprNode(GLOBAL_DATA, GLOBAL_DATA->propertyNames->nullIdentifier, $6, GLOBAL_DATA->lexer->sourceCode($5, $7, @5.first_line), $3.m_node.head), $3.m_features | ClosureFeature, 0);
          if ($3.m_features & ArgumentsFeature)
              $6->setUsesArguments();
          setStatementLocation($6, @5, @7);
      }
  | FUNCTION IDENT '(' ')' OPENBRACE FunctionBody CLOSEBRACE { $$ = createNodeInfo(new (GLOBAL_DATA) FuncExprNode(GLOBAL_DATA, *$2, $6, GLOBAL_DATA->lexer->sourceCode($5, $7, @5.first_line)), ClosureFeature, 0); setStatementLocation($6, @5, @7); }
  | FUNCTION IDENT '(' FormalParameterList ')' OPENBRACE FunctionBody CLOSEBRACE
      {
          $$ = createNodeInfo(new (GLOBAL_DATA) FuncExprNode(GLOBAL_DATA, *$2, $7, GLOBAL_DATA->lexer->sourceCode($6, $8, @6.first_line), $4.m_node.head), $4.m_features | ClosureFeature, 0); 
          if ($4.m_features & ArgumentsFeature)
              $7->setUsesArguments();
          setStatementLocation($7, @6, @8);
      }
;

FormalParameterList:
    IDENT                               { $$.m_node.head = new (GLOBAL_DATA) ParameterNode(GLOBAL_DATA, *$1);
                                          $$.m_features = (*$1 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0;
                                          $$.m_node.tail = $$.m_node.head; }
  | FormalParameterList ',' IDENT       { $$.m_node.head = $1.m_node.head;
                                          $$.m_features = $1.m_features | ((*$3 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0);
                                          $$.m_node.tail = new (GLOBAL_DATA) ParameterNode(GLOBAL_DATA, $1.m_node.tail, *$3);  }
;

FunctionBody:
    /* not in spec */                   { $$ = FunctionBodyNode::create(GLOBAL_DATA); }
  | SourceElements_NoNode               { $$ = FunctionBodyNode::create(GLOBAL_DATA); }
;

Program:
    /* not in spec */                   { GLOBAL_DATA->parser->didFinishParsing(new (GLOBAL_DATA) SourceElements(GLOBAL_DATA), 0, 0, NoFeatures, @0.last_line, 0); }
    | SourceElements                    { GLOBAL_DATA->parser->didFinishParsing($1.m_node, $1.m_varDeclarations, $1.m_funcDeclarations, $1.m_features, 
                                                                                @1.last_line, $1.m_numConstants); }
;

SourceElements:
    Statement                           { $$.m_node = new (GLOBAL_DATA) SourceElements(GLOBAL_DATA);
                                          $$.m_node->append($1.m_node);
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          $$.m_funcDeclarations = $1.m_funcDeclarations;
                                          $$.m_features = $1.m_features;
                                          $$.m_numConstants = $1.m_numConstants;
                                        }
  | SourceElements Statement            { $$.m_node->append($2.m_node);
                                          $$.m_varDeclarations = mergeDeclarationLists($1.m_varDeclarations, $2.m_varDeclarations);
                                          $$.m_funcDeclarations = mergeDeclarationLists($1.m_funcDeclarations, $2.m_funcDeclarations);
                                          $$.m_features = $1.m_features | $2.m_features;
                                          $$.m_numConstants = $1.m_numConstants + $2.m_numConstants;
                                        }
;
 
// Start NoNodes

Literal_NoNode:
    NULLTOKEN
  | TRUETOKEN
  | FALSETOKEN
  | NUMBER { }
  | STRING { }
  | '/' /* regexp */ { if (!GLOBAL_DATA->lexer->skipRegExp()) YYABORT; }
  | DIVEQUAL /* regexp with /= */ { if (!GLOBAL_DATA->lexer->skipRegExp()) YYABORT; }
;

Property_NoNode:
    IDENT ':' AssignmentExpr_NoNode { }
  | STRING ':' AssignmentExpr_NoNode { }
  | NUMBER ':' AssignmentExpr_NoNode { }
  | IDENT IDENT '(' ')' OPENBRACE FunctionBody_NoNode CLOSEBRACE { if (*$1 != "get" && *$1 != "set") YYABORT; }
  | IDENT IDENT '(' FormalParameterList_NoNode ')' OPENBRACE FunctionBody_NoNode CLOSEBRACE { if (*$1 != "get" && *$1 != "set") YYABORT; }
;

PropertyList_NoNode:
    Property_NoNode
  | PropertyList_NoNode ',' Property_NoNode
;

PrimaryExpr_NoNode:
    PrimaryExprNoBrace_NoNode
  | OPENBRACE CLOSEBRACE { }
  | OPENBRACE PropertyList_NoNode CLOSEBRACE { }
  /* allow extra comma, see http://bugs.webkit.org/show_bug.cgi?id=5939 */
  | OPENBRACE PropertyList_NoNode ',' CLOSEBRACE { }
;

PrimaryExprNoBrace_NoNode:
    THISTOKEN
  | Literal_NoNode
  | ArrayLiteral_NoNode
  | IDENT { }
  | '(' Expr_NoNode ')'
;

ArrayLiteral_NoNode:
    '[' ElisionOpt_NoNode ']'
  | '[' ElementList_NoNode ']'
  | '[' ElementList_NoNode ',' ElisionOpt_NoNode ']'
;

ElementList_NoNode:
    ElisionOpt_NoNode AssignmentExpr_NoNode
  | ElementList_NoNode ',' ElisionOpt_NoNode AssignmentExpr_NoNode
;

ElisionOpt_NoNode:
    /* nothing */
  | Elision_NoNode
;

Elision_NoNode:
    ','
  | Elision_NoNode ','
;

MemberExpr_NoNode:
    PrimaryExpr_NoNode
  | FunctionExpr_NoNode
  | MemberExpr_NoNode '[' Expr_NoNode ']'
  | MemberExpr_NoNode '.' IDENT
  | NEW MemberExpr_NoNode Arguments_NoNode
;

MemberExprNoBF_NoNode:
    PrimaryExprNoBrace_NoNode
  | MemberExprNoBF_NoNode '[' Expr_NoNode ']'
  | MemberExprNoBF_NoNode '.' IDENT
  | NEW MemberExpr_NoNode Arguments_NoNode
;

NewExpr_NoNode:
    MemberExpr_NoNode
  | NEW NewExpr_NoNode
;

NewExprNoBF_NoNode:
    MemberExprNoBF_NoNode
  | NEW NewExpr_NoNode
;

CallExpr_NoNode:
    MemberExpr_NoNode Arguments_NoNode
  | CallExpr_NoNode Arguments_NoNode
  | CallExpr_NoNode '[' Expr_NoNode ']'
  | CallExpr_NoNode '.' IDENT
;

CallExprNoBF_NoNode:
    MemberExprNoBF_NoNode Arguments_NoNode
  | CallExprNoBF_NoNode Arguments_NoNode
  | CallExprNoBF_NoNode '[' Expr_NoNode ']'
  | CallExprNoBF_NoNode '.' IDENT
;

Arguments_NoNode:
    '(' ')'
  | '(' ArgumentList_NoNode ')'
;

ArgumentList_NoNode:
    AssignmentExpr_NoNode
  | ArgumentList_NoNode ',' AssignmentExpr_NoNode
;

LeftHandSideExpr_NoNode:
    NewExpr_NoNode
  | CallExpr_NoNode
;

LeftHandSideExprNoBF_NoNode:
    NewExprNoBF_NoNode
  | CallExprNoBF_NoNode
;

PostfixExpr_NoNode:
    LeftHandSideExpr_NoNode
  | LeftHandSideExpr_NoNode PLUSPLUS
  | LeftHandSideExpr_NoNode MINUSMINUS
;

PostfixExprNoBF_NoNode:
    LeftHandSideExprNoBF_NoNode
  | LeftHandSideExprNoBF_NoNode PLUSPLUS
  | LeftHandSideExprNoBF_NoNode MINUSMINUS
;

UnaryExprCommon_NoNode:
    DELETETOKEN UnaryExpr_NoNode
  | VOIDTOKEN UnaryExpr_NoNode
  | TYPEOF UnaryExpr_NoNode
  | PLUSPLUS UnaryExpr_NoNode
  | AUTOPLUSPLUS UnaryExpr_NoNode
  | MINUSMINUS UnaryExpr_NoNode
  | AUTOMINUSMINUS UnaryExpr_NoNode
  | '+' UnaryExpr_NoNode
  | '-' UnaryExpr_NoNode
  | '~' UnaryExpr_NoNode
  | '!' UnaryExpr_NoNode

UnaryExpr_NoNode:
    PostfixExpr_NoNode
  | UnaryExprCommon_NoNode
;

UnaryExprNoBF_NoNode:
    PostfixExprNoBF_NoNode
  | UnaryExprCommon_NoNode
;

MultiplicativeExpr_NoNode:
    UnaryExpr_NoNode
  | MultiplicativeExpr_NoNode '*' UnaryExpr_NoNode
  | MultiplicativeExpr_NoNode '/' UnaryExpr_NoNode
  | MultiplicativeExpr_NoNode '%' UnaryExpr_NoNode
;

MultiplicativeExprNoBF_NoNode:
    UnaryExprNoBF_NoNode
  | MultiplicativeExprNoBF_NoNode '*' UnaryExpr_NoNode
  | MultiplicativeExprNoBF_NoNode '/' UnaryExpr_NoNode
  | MultiplicativeExprNoBF_NoNode '%' UnaryExpr_NoNode
;

AdditiveExpr_NoNode:
    MultiplicativeExpr_NoNode
  | AdditiveExpr_NoNode '+' MultiplicativeExpr_NoNode
  | AdditiveExpr_NoNode '-' MultiplicativeExpr_NoNode
;

AdditiveExprNoBF_NoNode:
    MultiplicativeExprNoBF_NoNode
  | AdditiveExprNoBF_NoNode '+' MultiplicativeExpr_NoNode
  | AdditiveExprNoBF_NoNode '-' MultiplicativeExpr_NoNode
;

ShiftExpr_NoNode:
    AdditiveExpr_NoNode
  | ShiftExpr_NoNode LSHIFT AdditiveExpr_NoNode
  | ShiftExpr_NoNode RSHIFT AdditiveExpr_NoNode
  | ShiftExpr_NoNode URSHIFT AdditiveExpr_NoNode
;

ShiftExprNoBF_NoNode:
    AdditiveExprNoBF_NoNode
  | ShiftExprNoBF_NoNode LSHIFT AdditiveExpr_NoNode
  | ShiftExprNoBF_NoNode RSHIFT AdditiveExpr_NoNode
  | ShiftExprNoBF_NoNode URSHIFT AdditiveExpr_NoNode
;

RelationalExpr_NoNode:
    ShiftExpr_NoNode
  | RelationalExpr_NoNode '<' ShiftExpr_NoNode
  | RelationalExpr_NoNode '>' ShiftExpr_NoNode
  | RelationalExpr_NoNode LE ShiftExpr_NoNode
  | RelationalExpr_NoNode GE ShiftExpr_NoNode
  | RelationalExpr_NoNode INSTANCEOF ShiftExpr_NoNode
  | RelationalExpr_NoNode INTOKEN ShiftExpr_NoNode
;

RelationalExprNoIn_NoNode:
    ShiftExpr_NoNode
  | RelationalExprNoIn_NoNode '<' ShiftExpr_NoNode
  | RelationalExprNoIn_NoNode '>' ShiftExpr_NoNode
  | RelationalExprNoIn_NoNode LE ShiftExpr_NoNode
  | RelationalExprNoIn_NoNode GE ShiftExpr_NoNode
  | RelationalExprNoIn_NoNode INSTANCEOF ShiftExpr_NoNode
;

RelationalExprNoBF_NoNode:
    ShiftExprNoBF_NoNode
  | RelationalExprNoBF_NoNode '<' ShiftExpr_NoNode
  | RelationalExprNoBF_NoNode '>' ShiftExpr_NoNode
  | RelationalExprNoBF_NoNode LE ShiftExpr_NoNode
  | RelationalExprNoBF_NoNode GE ShiftExpr_NoNode
  | RelationalExprNoBF_NoNode INSTANCEOF ShiftExpr_NoNode
  | RelationalExprNoBF_NoNode INTOKEN ShiftExpr_NoNode
;

EqualityExpr_NoNode:
    RelationalExpr_NoNode
  | EqualityExpr_NoNode EQEQ RelationalExpr_NoNode
  | EqualityExpr_NoNode NE RelationalExpr_NoNode
  | EqualityExpr_NoNode STREQ RelationalExpr_NoNode
  | EqualityExpr_NoNode STRNEQ RelationalExpr_NoNode
;

EqualityExprNoIn_NoNode:
    RelationalExprNoIn_NoNode
  | EqualityExprNoIn_NoNode EQEQ RelationalExprNoIn_NoNode
  | EqualityExprNoIn_NoNode NE RelationalExprNoIn_NoNode
  | EqualityExprNoIn_NoNode STREQ RelationalExprNoIn_NoNode
  | EqualityExprNoIn_NoNode STRNEQ RelationalExprNoIn_NoNode
;

EqualityExprNoBF_NoNode:
    RelationalExprNoBF_NoNode
  | EqualityExprNoBF_NoNode EQEQ RelationalExpr_NoNode
  | EqualityExprNoBF_NoNode NE RelationalExpr_NoNode
  | EqualityExprNoBF_NoNode STREQ RelationalExpr_NoNode
  | EqualityExprNoBF_NoNode STRNEQ RelationalExpr_NoNode
;

BitwiseANDExpr_NoNode:
    EqualityExpr_NoNode
  | BitwiseANDExpr_NoNode '&' EqualityExpr_NoNode
;

BitwiseANDExprNoIn_NoNode:
    EqualityExprNoIn_NoNode
  | BitwiseANDExprNoIn_NoNode '&' EqualityExprNoIn_NoNode
;

BitwiseANDExprNoBF_NoNode:
    EqualityExprNoBF_NoNode
  | BitwiseANDExprNoBF_NoNode '&' EqualityExpr_NoNode
;

BitwiseXORExpr_NoNode:
    BitwiseANDExpr_NoNode
  | BitwiseXORExpr_NoNode '^' BitwiseANDExpr_NoNode
;

BitwiseXORExprNoIn_NoNode:
    BitwiseANDExprNoIn_NoNode
  | BitwiseXORExprNoIn_NoNode '^' BitwiseANDExprNoIn_NoNode
;

BitwiseXORExprNoBF_NoNode:
    BitwiseANDExprNoBF_NoNode
  | BitwiseXORExprNoBF_NoNode '^' BitwiseANDExpr_NoNode
;

BitwiseORExpr_NoNode:
    BitwiseXORExpr_NoNode
  | BitwiseORExpr_NoNode '|' BitwiseXORExpr_NoNode
;

BitwiseORExprNoIn_NoNode:
    BitwiseXORExprNoIn_NoNode
  | BitwiseORExprNoIn_NoNode '|' BitwiseXORExprNoIn_NoNode
;

BitwiseORExprNoBF_NoNode:
    BitwiseXORExprNoBF_NoNode
  | BitwiseORExprNoBF_NoNode '|' BitwiseXORExpr_NoNode
;

LogicalANDExpr_NoNode:
    BitwiseORExpr_NoNode
  | LogicalANDExpr_NoNode AND BitwiseORExpr_NoNode
;

LogicalANDExprNoIn_NoNode:
    BitwiseORExprNoIn_NoNode
  | LogicalANDExprNoIn_NoNode AND BitwiseORExprNoIn_NoNode
;

LogicalANDExprNoBF_NoNode:
    BitwiseORExprNoBF_NoNode
  | LogicalANDExprNoBF_NoNode AND BitwiseORExpr_NoNode
;

LogicalORExpr_NoNode:
    LogicalANDExpr_NoNode
  | LogicalORExpr_NoNode OR LogicalANDExpr_NoNode
;

LogicalORExprNoIn_NoNode:
    LogicalANDExprNoIn_NoNode
  | LogicalORExprNoIn_NoNode OR LogicalANDExprNoIn_NoNode
;

LogicalORExprNoBF_NoNode:
    LogicalANDExprNoBF_NoNode
  | LogicalORExprNoBF_NoNode OR LogicalANDExpr_NoNode
;

ConditionalExpr_NoNode:
    LogicalORExpr_NoNode
  | LogicalORExpr_NoNode '?' AssignmentExpr_NoNode ':' AssignmentExpr_NoNode
;

ConditionalExprNoIn_NoNode:
    LogicalORExprNoIn_NoNode
  | LogicalORExprNoIn_NoNode '?' AssignmentExprNoIn_NoNode ':' AssignmentExprNoIn_NoNode
;

ConditionalExprNoBF_NoNode:
    LogicalORExprNoBF_NoNode
  | LogicalORExprNoBF_NoNode '?' AssignmentExpr_NoNode ':' AssignmentExpr_NoNode
;

AssignmentExpr_NoNode:
    ConditionalExpr_NoNode
  | LeftHandSideExpr_NoNode AssignmentOperator_NoNode AssignmentExpr_NoNode
;

AssignmentExprNoIn_NoNode:
    ConditionalExprNoIn_NoNode
  | LeftHandSideExpr_NoNode AssignmentOperator_NoNode AssignmentExprNoIn_NoNode
;

AssignmentExprNoBF_NoNode:
    ConditionalExprNoBF_NoNode
  | LeftHandSideExprNoBF_NoNode AssignmentOperator_NoNode AssignmentExpr_NoNode
;

AssignmentOperator_NoNode:
    '='
  | PLUSEQUAL
  | MINUSEQUAL
  | MULTEQUAL
  | DIVEQUAL
  | LSHIFTEQUAL
  | RSHIFTEQUAL
  | URSHIFTEQUAL
  | ANDEQUAL
  | XOREQUAL
  | OREQUAL
  | MODEQUAL
;

Expr_NoNode:
    AssignmentExpr_NoNode
  | Expr_NoNode ',' AssignmentExpr_NoNode
;

ExprNoIn_NoNode:
    AssignmentExprNoIn_NoNode
  | ExprNoIn_NoNode ',' AssignmentExprNoIn_NoNode
;

ExprNoBF_NoNode:
    AssignmentExprNoBF_NoNode
  | ExprNoBF_NoNode ',' AssignmentExpr_NoNode
;

Statement_NoNode:
    Block_NoNode
  | VariableStatement_NoNode
  | ConstStatement_NoNode
  | FunctionDeclaration_NoNode
  | EmptyStatement_NoNode
  | ExprStatement_NoNode
  | IfStatement_NoNode
  | IterationStatement_NoNode
  | ContinueStatement_NoNode
  | BreakStatement_NoNode
  | ReturnStatement_NoNode
  | WithStatement_NoNode
  | SwitchStatement_NoNode
  | LabelledStatement_NoNode
  | ThrowStatement_NoNode
  | TryStatement_NoNode
  | DebuggerStatement_NoNode
;

Block_NoNode:
    OPENBRACE CLOSEBRACE { }
  | OPENBRACE SourceElements_NoNode CLOSEBRACE { }
;

VariableStatement_NoNode:
    VAR VariableDeclarationList_NoNode ';'
  | VAR VariableDeclarationList_NoNode error { AUTO_SEMICOLON; }
;

VariableDeclarationList_NoNode:
    IDENT { }
  | IDENT Initializer_NoNode { }
  | VariableDeclarationList_NoNode ',' IDENT
  | VariableDeclarationList_NoNode ',' IDENT Initializer_NoNode
;

VariableDeclarationListNoIn_NoNode:
    IDENT { }
  | IDENT InitializerNoIn_NoNode { }
  | VariableDeclarationListNoIn_NoNode ',' IDENT
  | VariableDeclarationListNoIn_NoNode ',' IDENT InitializerNoIn_NoNode
;

ConstStatement_NoNode:
    CONSTTOKEN ConstDeclarationList_NoNode ';'
  | CONSTTOKEN ConstDeclarationList_NoNode error { AUTO_SEMICOLON; }
;

ConstDeclarationList_NoNode:
    ConstDeclaration_NoNode
  | ConstDeclarationList_NoNode ',' ConstDeclaration_NoNode
;

ConstDeclaration_NoNode:
    IDENT { }
  | IDENT Initializer_NoNode { }
;

Initializer_NoNode:
    '=' AssignmentExpr_NoNode
;

InitializerNoIn_NoNode:
    '=' AssignmentExprNoIn_NoNode
;

EmptyStatement_NoNode:
    ';'
;

ExprStatement_NoNode:
    ExprNoBF_NoNode ';'
  | ExprNoBF_NoNode error { AUTO_SEMICOLON; }
;

IfStatement_NoNode:
    IF '(' Expr_NoNode ')' Statement_NoNode %prec IF_WITHOUT_ELSE
  | IF '(' Expr_NoNode ')' Statement_NoNode ELSE Statement_NoNode
;

IterationStatement_NoNode:
    DO Statement_NoNode WHILE '(' Expr_NoNode ')' ';'
  | DO Statement_NoNode WHILE '(' Expr_NoNode ')' error // Always performs automatic semicolon insertion
  | WHILE '(' Expr_NoNode ')' Statement_NoNode
  | FOR '(' ExprNoInOpt_NoNode ';' ExprOpt_NoNode ';' ExprOpt_NoNode ')' Statement_NoNode
  | FOR '(' VAR VariableDeclarationListNoIn_NoNode ';' ExprOpt_NoNode ';' ExprOpt_NoNode ')' Statement_NoNode
  | FOR '(' LeftHandSideExpr_NoNode INTOKEN Expr_NoNode ')' Statement_NoNode
  | FOR '(' VAR IDENT INTOKEN Expr_NoNode ')' Statement_NoNode
  | FOR '(' VAR IDENT InitializerNoIn_NoNode INTOKEN Expr_NoNode ')' Statement_NoNode
;

ExprOpt_NoNode:
    /* nothing */
  | Expr_NoNode
;

ExprNoInOpt_NoNode:
    /* nothing */
  | ExprNoIn_NoNode
;

ContinueStatement_NoNode:
    CONTINUE ';'
  | CONTINUE error { AUTO_SEMICOLON; }
  | CONTINUE IDENT ';'
  | CONTINUE IDENT error { AUTO_SEMICOLON; }
;

BreakStatement_NoNode:
    BREAK ';'
  | BREAK error { AUTO_SEMICOLON; }
  | BREAK IDENT ';'
  | BREAK IDENT error { AUTO_SEMICOLON; }
;

ReturnStatement_NoNode:
    RETURN ';'
  | RETURN error { AUTO_SEMICOLON; }
  | RETURN Expr_NoNode ';'
  | RETURN Expr_NoNode error { AUTO_SEMICOLON; }
;

WithStatement_NoNode:
    WITH '(' Expr_NoNode ')' Statement_NoNode
;

SwitchStatement_NoNode:
    SWITCH '(' Expr_NoNode ')' CaseBlock_NoNode
;

CaseBlock_NoNode:
    OPENBRACE CaseClausesOpt_NoNode CLOSEBRACE { }
  | OPENBRACE CaseClausesOpt_NoNode DefaultClause_NoNode CaseClausesOpt_NoNode CLOSEBRACE { }
;

CaseClausesOpt_NoNode:
    /* nothing */
  | CaseClauses_NoNode
;

CaseClauses_NoNode:
    CaseClause_NoNode
  | CaseClauses_NoNode CaseClause_NoNode
;

CaseClause_NoNode:
    CASE Expr_NoNode ':'
  | CASE Expr_NoNode ':' SourceElements_NoNode
;

DefaultClause_NoNode:
    DEFAULT ':'
  | DEFAULT ':' SourceElements_NoNode
;

LabelledStatement_NoNode:
    IDENT ':' Statement_NoNode { }
;

ThrowStatement_NoNode:
    THROW Expr_NoNode ';'
  | THROW Expr_NoNode error { AUTO_SEMICOLON; }
;

TryStatement_NoNode:
    TRY Block_NoNode FINALLY Block_NoNode
  | TRY Block_NoNode CATCH '(' IDENT ')' Block_NoNode
  | TRY Block_NoNode CATCH '(' IDENT ')' Block_NoNode FINALLY Block_NoNode
;

DebuggerStatement_NoNode:
    DEBUGGER ';'
  | DEBUGGER error { AUTO_SEMICOLON; }
;

FunctionDeclaration_NoNode:
    FUNCTION IDENT '(' ')' OPENBRACE FunctionBody_NoNode CLOSEBRACE
  | FUNCTION IDENT '(' FormalParameterList_NoNode ')' OPENBRACE FunctionBody_NoNode CLOSEBRACE
;

FunctionExpr_NoNode:
    FUNCTION '(' ')' OPENBRACE FunctionBody_NoNode CLOSEBRACE
  | FUNCTION '(' FormalParameterList_NoNode ')' OPENBRACE FunctionBody_NoNode CLOSEBRACE
  | FUNCTION IDENT '(' ')' OPENBRACE FunctionBody_NoNode CLOSEBRACE
  | FUNCTION IDENT '(' FormalParameterList_NoNode ')' OPENBRACE FunctionBody_NoNode CLOSEBRACE
;

FormalParameterList_NoNode:
    IDENT { }
  | FormalParameterList_NoNode ',' IDENT
;

FunctionBody_NoNode:
    /* not in spec */
  | SourceElements_NoNode
;

SourceElements_NoNode:
    Statement_NoNode
  | SourceElements_NoNode Statement_NoNode
;

// End NoNodes

%%

#undef GLOBAL_DATA

static ExpressionNode* makeAssignNode(JSGlobalData* globalData, ExpressionNode* loc, Operator op, ExpressionNode* expr, bool locHasAssignments, bool exprHasAssignments, int start, int divot, int end)
{
    if (!loc->isLocation())
        return new (globalData) AssignErrorNode(globalData, loc, op, expr, divot, divot - start, end - divot);

    if (loc->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(loc);
        if (op == OpEqual) {
            AssignResolveNode* node = new (globalData) AssignResolveNode(globalData, resolve->identifier(), expr, exprHasAssignments);
            setExceptionLocation(node, start, divot, end);
            return node;
        } else
            return new (globalData) ReadModifyResolveNode(globalData, resolve->identifier(), op, expr, exprHasAssignments, divot, divot - start, end - divot);
    }
    if (loc->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(loc);
        if (op == OpEqual)
            return new (globalData) AssignBracketNode(globalData, bracket->base(), bracket->subscript(), expr, locHasAssignments, exprHasAssignments, bracket->divot(), bracket->divot() - start, end - bracket->divot());
        else {
            ReadModifyBracketNode* node = new (globalData) ReadModifyBracketNode(globalData, bracket->base(), bracket->subscript(), op, expr, locHasAssignments, exprHasAssignments, divot, divot - start, end - divot);
            node->setSubexpressionInfo(bracket->divot(), bracket->endOffset());
            return node;
        }
    }
    ASSERT(loc->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(loc);
    if (op == OpEqual)
        return new (globalData) AssignDotNode(globalData, dot->base(), dot->identifier(), expr, exprHasAssignments, dot->divot(), dot->divot() - start, end - dot->divot());

    ReadModifyDotNode* node = new (globalData) ReadModifyDotNode(globalData, dot->base(), dot->identifier(), op, expr, exprHasAssignments, divot, divot - start, end - divot);
    node->setSubexpressionInfo(dot->divot(), dot->endOffset());
    return node;
}

static ExpressionNode* makePrefixNode(JSGlobalData* globalData, ExpressionNode* expr, Operator op, int start, int divot, int end)
{
    if (!expr->isLocation())
        return new (globalData) PrefixErrorNode(globalData, expr, op, divot, divot - start, end - divot);
    
    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        return new (globalData) PrefixResolveNode(globalData, resolve->identifier(), op, divot, divot - start, end - divot);
    }
    if (expr->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(expr);
        PrefixBracketNode* node = new (globalData) PrefixBracketNode(globalData, bracket->base(), bracket->subscript(), op, divot, divot - start, end - divot);
        node->setSubexpressionInfo(bracket->divot(), bracket->startOffset());
        return node;
    }
    ASSERT(expr->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(expr);
    PrefixDotNode* node = new (globalData) PrefixDotNode(globalData, dot->base(), dot->identifier(), op, divot, divot - start, end - divot);
    node->setSubexpressionInfo(dot->divot(), dot->startOffset());
    return node;
}

static ExpressionNode* makePostfixNode(JSGlobalData* globalData, ExpressionNode* expr, Operator op, int start, int divot, int end)
{ 
    if (!expr->isLocation())
        return new (globalData) PostfixErrorNode(globalData, expr, op, divot, divot - start, end - divot);
    
    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        return new (globalData) PostfixResolveNode(globalData, resolve->identifier(), op, divot, divot - start, end - divot);
    }
    if (expr->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(expr);
        PostfixBracketNode* node = new (globalData) PostfixBracketNode(globalData, bracket->base(), bracket->subscript(), op, divot, divot - start, end - divot);
        node->setSubexpressionInfo(bracket->divot(), bracket->endOffset());
        return node;
        
    }
    ASSERT(expr->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(expr);
    PostfixDotNode* node = new (globalData) PostfixDotNode(globalData, dot->base(), dot->identifier(), op, divot, divot - start, end - divot);
    node->setSubexpressionInfo(dot->divot(), dot->endOffset());
    return node;
}

static ExpressionNodeInfo makeFunctionCallNode(JSGlobalData* globalData, ExpressionNodeInfo func, ArgumentsNodeInfo args, int start, int divot, int end)
{
    CodeFeatures features = func.m_features | args.m_features;
    int numConstants = func.m_numConstants + args.m_numConstants;
    if (!func.m_node->isLocation())
        return createNodeInfo<ExpressionNode*>(new (globalData) FunctionCallValueNode(globalData, func.m_node, args.m_node, divot, divot - start, end - divot), features, numConstants);
    if (func.m_node->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(func.m_node);
        const Identifier& identifier = resolve->identifier();
        if (identifier == globalData->propertyNames->eval)
            return createNodeInfo<ExpressionNode*>(new (globalData) EvalFunctionCallNode(globalData, args.m_node, divot, divot - start, end - divot), EvalFeature | features, numConstants);
        return createNodeInfo<ExpressionNode*>(new (globalData) FunctionCallResolveNode(globalData, identifier, args.m_node, divot, divot - start, end - divot), features, numConstants);
    }
    if (func.m_node->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(func.m_node);
        FunctionCallBracketNode* node = new (globalData) FunctionCallBracketNode(globalData, bracket->base(), bracket->subscript(), args.m_node, divot, divot - start, end - divot);
        node->setSubexpressionInfo(bracket->divot(), bracket->endOffset());
        return createNodeInfo<ExpressionNode*>(node, features, numConstants);
    }
    ASSERT(func.m_node->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(func.m_node);
    FunctionCallDotNode* node;
    if (dot->identifier() == globalData->propertyNames->call)
        node = new (globalData) CallFunctionCallDotNode(globalData, dot->base(), dot->identifier(), args.m_node, divot, divot - start, end - divot);
    else if (dot->identifier() == globalData->propertyNames->apply)
        node = new (globalData) ApplyFunctionCallDotNode(globalData, dot->base(), dot->identifier(), args.m_node, divot, divot - start, end - divot);
    else
        node = new (globalData) FunctionCallDotNode(globalData, dot->base(), dot->identifier(), args.m_node, divot, divot - start, end - divot);
    node->setSubexpressionInfo(dot->divot(), dot->endOffset());
    return createNodeInfo<ExpressionNode*>(node, features, numConstants);
}

static ExpressionNode* makeTypeOfNode(JSGlobalData* globalData, ExpressionNode* expr)
{
    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        return new (globalData) TypeOfResolveNode(globalData, resolve->identifier());
    }
    return new (globalData) TypeOfValueNode(globalData, expr);
}

static ExpressionNode* makeDeleteNode(JSGlobalData* globalData, ExpressionNode* expr, int start, int divot, int end)
{
    if (!expr->isLocation())
        return new (globalData) DeleteValueNode(globalData, expr);
    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        return new (globalData) DeleteResolveNode(globalData, resolve->identifier(), divot, divot - start, end - divot);
    }
    if (expr->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(expr);
        return new (globalData) DeleteBracketNode(globalData, bracket->base(), bracket->subscript(), divot, divot - start, end - divot);
    }
    ASSERT(expr->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(expr);
    return new (globalData) DeleteDotNode(globalData, dot->base(), dot->identifier(), divot, divot - start, end - divot);
}

static PropertyNode* makeGetterOrSetterPropertyNode(JSGlobalData* globalData, const Identifier& getOrSet, const Identifier& name, ParameterNode* params, FunctionBodyNode* body, const SourceCode& source)
{
    PropertyNode::Type type;
    if (getOrSet == "get")
        type = PropertyNode::Getter;
    else if (getOrSet == "set")
        type = PropertyNode::Setter;
    else
        return 0;
    return new (globalData) PropertyNode(globalData, name, new (globalData) FuncExprNode(globalData, globalData->propertyNames->nullIdentifier, body, source, params), type);
}

static ExpressionNode* makeNegateNode(JSGlobalData* globalData, ExpressionNode* n)
{
    if (n->isNumber()) {
        NumberNode* numberNode = static_cast<NumberNode*>(n);
        numberNode->setValue(-numberNode->value());
        return numberNode;
    }

    return new (globalData) NegateNode(globalData, n);
}

static NumberNode* makeNumberNode(JSGlobalData* globalData, double d)
{
    return new (globalData) NumberNode(globalData, d);
}

static ExpressionNode* makeBitwiseNotNode(JSGlobalData* globalData, ExpressionNode* expr)
{
    if (expr->isNumber())
        return makeNumberNode(globalData, ~toInt32(static_cast<NumberNode*>(expr)->value()));
    return new (globalData) BitwiseNotNode(globalData, expr);
}

static ExpressionNode* makeMultNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    expr1 = expr1->stripUnaryPlus();
    expr2 = expr2->stripUnaryPlus();

    if (expr1->isNumber() && expr2->isNumber())
        return makeNumberNode(globalData, static_cast<NumberNode*>(expr1)->value() * static_cast<NumberNode*>(expr2)->value());

    if (expr1->isNumber() && static_cast<NumberNode*>(expr1)->value() == 1)
        return new (globalData) UnaryPlusNode(globalData, expr2);

    if (expr2->isNumber() && static_cast<NumberNode*>(expr2)->value() == 1)
        return new (globalData) UnaryPlusNode(globalData, expr1);

    return new (globalData) MultNode(globalData, expr1, expr2, rightHasAssignments);
}

static ExpressionNode* makeDivNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    expr1 = expr1->stripUnaryPlus();
    expr2 = expr2->stripUnaryPlus();

    if (expr1->isNumber() && expr2->isNumber())
        return makeNumberNode(globalData, static_cast<NumberNode*>(expr1)->value() / static_cast<NumberNode*>(expr2)->value());
    return new (globalData) DivNode(globalData, expr1, expr2, rightHasAssignments);
}

static ExpressionNode* makeAddNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber())
        return makeNumberNode(globalData, static_cast<NumberNode*>(expr1)->value() + static_cast<NumberNode*>(expr2)->value());
    return new (globalData) AddNode(globalData, expr1, expr2, rightHasAssignments);
}

static ExpressionNode* makeSubNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    expr1 = expr1->stripUnaryPlus();
    expr2 = expr2->stripUnaryPlus();

    if (expr1->isNumber() && expr2->isNumber())
        return makeNumberNode(globalData, static_cast<NumberNode*>(expr1)->value() - static_cast<NumberNode*>(expr2)->value());
    return new (globalData) SubNode(globalData, expr1, expr2, rightHasAssignments);
}

static ExpressionNode* makeLeftShiftNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber())
        return makeNumberNode(globalData, toInt32(static_cast<NumberNode*>(expr1)->value()) << (toUInt32(static_cast<NumberNode*>(expr2)->value()) & 0x1f));
    return new (globalData) LeftShiftNode(globalData, expr1, expr2, rightHasAssignments);
}

static ExpressionNode* makeRightShiftNode(JSGlobalData* globalData, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber())
        return makeNumberNode(globalData, toInt32(static_cast<NumberNode*>(expr1)->value()) >> (toUInt32(static_cast<NumberNode*>(expr2)->value()) & 0x1f));
    return new (globalData) RightShiftNode(globalData, expr1, expr2, rightHasAssignments);
}

// Called by yyparse on error.
int yyerror(const char*)
{
    return 1;
}

// May we automatically insert a semicolon?
static bool allowAutomaticSemicolon(Lexer& lexer, int yychar)
{
    return yychar == CLOSEBRACE || yychar == 0 || lexer.prevTerminator();
}

static ExpressionNode* combineCommaNodes(JSGlobalData* globalData, ExpressionNode* list, ExpressionNode* init)
{
    if (!list)
        return init;
    if (list->isCommaNode()) {
        static_cast<CommaNode*>(list)->append(init);
        return list;
    }
    return new (globalData) CommaNode(globalData, list, init);
}

// We turn variable declarations into either assignments or empty
// statements (which later get stripped out), because the actual
// declaration work is hoisted up to the start of the function body
static StatementNode* makeVarStatementNode(JSGlobalData* globalData, ExpressionNode* expr)
{
    if (!expr)
        return new (globalData) EmptyStatementNode(globalData);
    return new (globalData) VarStatementNode(globalData, expr);
}
