%pure_parser

%{

/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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
static inline bool allowAutomaticSemicolon(JSC::Lexer&, int);

#define GLOBAL_DATA static_cast<JSGlobalData*>(globalPtr)
#define LEXER (GLOBAL_DATA->lexer)

#define AUTO_SEMICOLON do { if (!allowAutomaticSemicolon(*LEXER, yychar)) YYABORT; } while (0)
#define SET_EXCEPTION_LOCATION(node, start, divot, end) node->setExceptionSourceCode((divot), (divot) - (start), (end) - (divot))
#define DBG(l, s, e) (l)->setLoc((s).first_line, (e).last_line)

using namespace JSC;
using namespace std;

static ExpressionNode* makeAssignNode(void*, ExpressionNode* loc, Operator, ExpressionNode* expr, bool locHasAssignments, bool exprHasAssignments, int start, int divot, int end);
static ExpressionNode* makePrefixNode(void*, ExpressionNode* expr, Operator, int start, int divot, int end);
static ExpressionNode* makePostfixNode(void*, ExpressionNode* expr, Operator, int start, int divot, int end);
static PropertyNode* makeGetterOrSetterPropertyNode(void*, const Identifier &getOrSet, const Identifier& name, ParameterNode*, FunctionBodyNode*, const SourceCode&);
static ExpressionNodeInfo makeFunctionCallNode(void*, ExpressionNodeInfo func, ArgumentsNodeInfo, int start, int divot, int end);
static ExpressionNode* makeTypeOfNode(void*, ExpressionNode*);
static ExpressionNode* makeDeleteNode(void*, ExpressionNode*, int start, int divot, int end);
static ExpressionNode* makeNegateNode(void*, ExpressionNode*);
static NumberNode* makeNumberNode(void*, double);
static ExpressionNode* makeBitwiseNotNode(void*, ExpressionNode*);
static ExpressionNode* makeMultNode(void*, ExpressionNode*, ExpressionNode*, bool rightHasAssignments);
static ExpressionNode* makeDivNode(void*, ExpressionNode*, ExpressionNode*, bool rightHasAssignments);
static ExpressionNode* makeAddNode(void*, ExpressionNode*, ExpressionNode*, bool rightHasAssignments);
static ExpressionNode* makeSubNode(void*, ExpressionNode*, ExpressionNode*, bool rightHasAssignments);
static ExpressionNode* makeLeftShiftNode(void*, ExpressionNode*, ExpressionNode*, bool rightHasAssignments);
static ExpressionNode* makeRightShiftNode(void*, ExpressionNode*, ExpressionNode*, bool rightHasAssignments);
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
                                                                       CodeFeatures info,
                                                                       int numConstants) 
{
    ASSERT((info & ~AllFeatures) == 0);
    NodeDeclarationInfo<T> result = {node, varDecls, funcDecls, info, numConstants};
    return result;
}

template <typename T> NodeInfo<T> createNodeInfo(T node, CodeFeatures info, int numConstants)
{
    ASSERT((info & ~AllFeatures) == 0);
    NodeInfo<T> result = {node, info, numConstants};
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
    Identifier*         ident;

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
    NULLTOKEN                           { $$ = createNodeInfo<ExpressionNode*>(new NullNode(GLOBAL_DATA), 0, 1); }
  | TRUETOKEN                           { $$ = createNodeInfo<ExpressionNode*>(new BooleanNode(GLOBAL_DATA, true), 0, 1); }
  | FALSETOKEN                          { $$ = createNodeInfo<ExpressionNode*>(new BooleanNode(GLOBAL_DATA, false), 0, 1); }
  | NUMBER                              { $$ = createNodeInfo<ExpressionNode*>(makeNumberNode(GLOBAL_DATA, $1), 0, 1); }
  | STRING                              { $$ = createNodeInfo<ExpressionNode*>(new StringNode(GLOBAL_DATA, *$1), 0, 1); }
  | '/' /* regexp */                    {
                                            Lexer& l = *LEXER;
                                            if (!l.scanRegExp())
                                                YYABORT;
                                            RegExpNode* node = new RegExpNode(GLOBAL_DATA, l.pattern(), l.flags());
                                            int size = l.pattern().size() + 2; // + 2 for the two /'s
                                            SET_EXCEPTION_LOCATION(node, @1.first_column, @1.first_column + size, @1.first_column + size);
                                            $$ = createNodeInfo<ExpressionNode*>(node, 0, 0);
                                        }
  | DIVEQUAL /* regexp with /= */       {
                                            Lexer& l = *LEXER;
                                            if (!l.scanRegExp())
                                                YYABORT;
                                            RegExpNode* node = new RegExpNode(GLOBAL_DATA, "=" + l.pattern(), l.flags());
                                            int size = l.pattern().size() + 2; // + 2 for the two /'s
                                            SET_EXCEPTION_LOCATION(node, @1.first_column, @1.first_column + size, @1.first_column + size);
                                            $$ = createNodeInfo<ExpressionNode*>(node, 0, 0);
                                        }
;

Property:
    IDENT ':' AssignmentExpr            { $$ = createNodeInfo<PropertyNode*>(new PropertyNode(GLOBAL_DATA, *$1, $3.m_node, PropertyNode::Constant), $3.m_features, $3.m_numConstants); }
  | STRING ':' AssignmentExpr           { $$ = createNodeInfo<PropertyNode*>(new PropertyNode(GLOBAL_DATA, *$1, $3.m_node, PropertyNode::Constant), $3.m_features, $3.m_numConstants); }
  | NUMBER ':' AssignmentExpr           { $$ = createNodeInfo<PropertyNode*>(new PropertyNode(GLOBAL_DATA, Identifier(GLOBAL_DATA, UString::from($1)), $3.m_node, PropertyNode::Constant), $3.m_features, $3.m_numConstants); }
  | IDENT IDENT '(' ')' OPENBRACE FunctionBody CLOSEBRACE    { $$ = createNodeInfo<PropertyNode*>(makeGetterOrSetterPropertyNode(globalPtr, *$1, *$2, 0, $6, LEXER->sourceCode($5, $7, @5.first_line)), ClosureFeature, 0); DBG($6, @5, @7); if (!$$.m_node) YYABORT; }
  | IDENT IDENT '(' FormalParameterList ')' OPENBRACE FunctionBody CLOSEBRACE
                                                             {
                                                                 $$ = createNodeInfo<PropertyNode*>(makeGetterOrSetterPropertyNode(globalPtr, *$1, *$2, $4.m_node.head, $7, LEXER->sourceCode($6, $8, @6.first_line)), $4.m_features | ClosureFeature, 0); 
                                                                 if ($4.m_features & ArgumentsFeature)
                                                                     $7->setUsesArguments(); 
                                                                 DBG($7, @6, @8); 
                                                                 if (!$$.m_node) 
                                                                     YYABORT; 
                                                             }
;

PropertyList:
    Property                            { $$.m_node.head = new PropertyListNode(GLOBAL_DATA, $1.m_node); 
                                          $$.m_node.tail = $$.m_node.head;
                                          $$.m_features = $1.m_features;
                                          $$.m_numConstants = $1.m_numConstants; }
  | PropertyList ',' Property           { $$.m_node.head = $1.m_node.head;
                                          $$.m_node.tail = new PropertyListNode(GLOBAL_DATA, $3.m_node, $1.m_node.tail);
                                          $$.m_features = $1.m_features | $3.m_features;
                                          $$.m_numConstants = $1.m_numConstants + $3.m_numConstants; }
;

PrimaryExpr:
    PrimaryExprNoBrace
  | OPENBRACE CLOSEBRACE                             { $$ = createNodeInfo<ExpressionNode*>(new ObjectLiteralNode(GLOBAL_DATA), 0, 0); }
  | OPENBRACE PropertyList CLOSEBRACE                { $$ = createNodeInfo<ExpressionNode*>(new ObjectLiteralNode(GLOBAL_DATA, $2.m_node.head), $2.m_features, $2.m_numConstants); }
  /* allow extra comma, see http://bugs.webkit.org/show_bug.cgi?id=5939 */
  | OPENBRACE PropertyList ',' CLOSEBRACE            { $$ = createNodeInfo<ExpressionNode*>(new ObjectLiteralNode(GLOBAL_DATA, $2.m_node.head), $2.m_features, $2.m_numConstants); }
;

PrimaryExprNoBrace:
    THISTOKEN                           { $$ = createNodeInfo<ExpressionNode*>(new ThisNode(GLOBAL_DATA), ThisFeature, 0); }
  | Literal
  | ArrayLiteral
  | IDENT                               { $$ = createNodeInfo<ExpressionNode*>(new ResolveNode(GLOBAL_DATA, *$1, @1.first_column), (*$1 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0, 0); }
  | '(' Expr ')'                        { $$ = $2; }
;

ArrayLiteral:
    '[' ElisionOpt ']'                  { $$ = createNodeInfo<ExpressionNode*>(new ArrayNode(GLOBAL_DATA, $2), 0, $2 ? 1 : 0); }
  | '[' ElementList ']'                 { $$ = createNodeInfo<ExpressionNode*>(new ArrayNode(GLOBAL_DATA, $2.m_node.head), $2.m_features, $2.m_numConstants); }
  | '[' ElementList ',' ElisionOpt ']'  { $$ = createNodeInfo<ExpressionNode*>(new ArrayNode(GLOBAL_DATA, $4, $2.m_node.head), $2.m_features, $4 ? $2.m_numConstants + 1 : $2.m_numConstants); }
;

ElementList:
    ElisionOpt AssignmentExpr           { $$.m_node.head = new ElementNode(GLOBAL_DATA, $1, $2.m_node);
                                          $$.m_node.tail = $$.m_node.head;
                                          $$.m_features = $2.m_features;
                                          $$.m_numConstants = $2.m_numConstants; }
  | ElementList ',' ElisionOpt AssignmentExpr
                                        { $$.m_node.head = $1.m_node.head;
                                          $$.m_node.tail = new ElementNode(GLOBAL_DATA, $1.m_node.tail, $3, $4.m_node);
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
  | MemberExpr '[' Expr ']'             { BracketAccessorNode* node = new BracketAccessorNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @1.last_column, @4.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); 
                                        }
  | MemberExpr '.' IDENT                { DotAccessorNode* node = new DotAccessorNode(GLOBAL_DATA, $1.m_node, *$3);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @1.last_column, @3.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features, $1.m_numConstants);
                                        }
  | NEW MemberExpr Arguments            { NewExprNode* node = new NewExprNode(GLOBAL_DATA, $2.m_node, $3.m_node);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @2.last_column, @3.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $2.m_features | $3.m_features, $2.m_numConstants + $3.m_numConstants);
                                        }
;

MemberExprNoBF:
    PrimaryExprNoBrace
  | MemberExprNoBF '[' Expr ']'         { BracketAccessorNode* node = new BracketAccessorNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @1.last_column, @4.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); 
                                        }
  | MemberExprNoBF '.' IDENT            { DotAccessorNode* node = new DotAccessorNode(GLOBAL_DATA, $1.m_node, *$3);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @1.last_column, @3.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features, $1.m_numConstants);
                                        }
  | NEW MemberExpr Arguments            { NewExprNode* node = new NewExprNode(GLOBAL_DATA, $2.m_node, $3.m_node);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @2.last_column, @3.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $2.m_features | $3.m_features, $2.m_numConstants + $3.m_numConstants);
                                        }
;

NewExpr:
    MemberExpr
  | NEW NewExpr                         { NewExprNode* node = new NewExprNode(GLOBAL_DATA, $2.m_node);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @2.last_column, @2.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $2.m_features, $2.m_numConstants); 
                                        }
;

NewExprNoBF:
    MemberExprNoBF
  | NEW NewExpr                         { NewExprNode* node = new NewExprNode(GLOBAL_DATA, $2.m_node);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @2.last_column, @2.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $2.m_features, $2.m_numConstants);
                                        }
;

CallExpr:
    MemberExpr Arguments                { $$ = makeFunctionCallNode(globalPtr, $1, $2, @1.first_column, @1.last_column, @2.last_column); }
  | CallExpr Arguments                  { $$ = makeFunctionCallNode(globalPtr, $1, $2, @1.first_column, @1.last_column, @2.last_column); }
  | CallExpr '[' Expr ']'               { BracketAccessorNode* node = new BracketAccessorNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @1.last_column, @4.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); 
                                        }
  | CallExpr '.' IDENT                  { DotAccessorNode* node = new DotAccessorNode(GLOBAL_DATA, $1.m_node, *$3);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @1.last_column, @3.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features, $1.m_numConstants); }
;

CallExprNoBF:
    MemberExprNoBF Arguments            { $$ = makeFunctionCallNode(globalPtr, $1, $2, @1.first_column, @1.last_column, @2.last_column); }
  | CallExprNoBF Arguments              { $$ = makeFunctionCallNode(globalPtr, $1, $2, @1.first_column, @1.last_column, @2.last_column); }
  | CallExprNoBF '[' Expr ']'           { BracketAccessorNode* node = new BracketAccessorNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @1.last_column, @4.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); 
                                        }
  | CallExprNoBF '.' IDENT              { DotAccessorNode* node = new DotAccessorNode(GLOBAL_DATA, $1.m_node, *$3);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @1.last_column, @3.last_column);
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features, $1.m_numConstants); 
                                        }
;

Arguments:
    '(' ')'                             { $$ = createNodeInfo<ArgumentsNode*>(new ArgumentsNode(GLOBAL_DATA), 0, 0); }
  | '(' ArgumentList ')'                { $$ = createNodeInfo<ArgumentsNode*>(new ArgumentsNode(GLOBAL_DATA, $2.m_node.head), $2.m_features, $2.m_numConstants); }
;

ArgumentList:
    AssignmentExpr                      { $$.m_node.head = new ArgumentListNode(GLOBAL_DATA, $1.m_node);
                                          $$.m_node.tail = $$.m_node.head;
                                          $$.m_features = $1.m_features;
                                          $$.m_numConstants = $1.m_numConstants; }
  | ArgumentList ',' AssignmentExpr     { $$.m_node.head = $1.m_node.head;
                                          $$.m_node.tail = new ArgumentListNode(GLOBAL_DATA, $1.m_node.tail, $3.m_node);
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
  | VOIDTOKEN UnaryExpr                 { $$ = createNodeInfo<ExpressionNode*>(new VoidNode(GLOBAL_DATA, $2.m_node), $2.m_features, $2.m_numConstants + 1); }
  | TYPEOF UnaryExpr                    { $$ = createNodeInfo<ExpressionNode*>(makeTypeOfNode(GLOBAL_DATA, $2.m_node), $2.m_features, $2.m_numConstants); }
  | PLUSPLUS UnaryExpr                  { $$ = createNodeInfo<ExpressionNode*>(makePrefixNode(GLOBAL_DATA, $2.m_node, OpPlusPlus, @1.first_column, @2.first_column + 1, @2.last_column), $2.m_features | AssignFeature, $2.m_numConstants); }
  | AUTOPLUSPLUS UnaryExpr              { $$ = createNodeInfo<ExpressionNode*>(makePrefixNode(GLOBAL_DATA, $2.m_node, OpPlusPlus, @1.first_column, @2.first_column + 1, @2.last_column), $2.m_features | AssignFeature, $2.m_numConstants); }
  | MINUSMINUS UnaryExpr                { $$ = createNodeInfo<ExpressionNode*>(makePrefixNode(GLOBAL_DATA, $2.m_node, OpMinusMinus, @1.first_column, @2.first_column + 1, @2.last_column), $2.m_features | AssignFeature, $2.m_numConstants); }
  | AUTOMINUSMINUS UnaryExpr            { $$ = createNodeInfo<ExpressionNode*>(makePrefixNode(GLOBAL_DATA, $2.m_node, OpMinusMinus, @1.first_column, @2.first_column + 1, @2.last_column), $2.m_features | AssignFeature, $2.m_numConstants); }
  | '+' UnaryExpr                       { $$ = createNodeInfo<ExpressionNode*>(new UnaryPlusNode(GLOBAL_DATA, $2.m_node), $2.m_features, $2.m_numConstants); }
  | '-' UnaryExpr                       { $$ = createNodeInfo<ExpressionNode*>(makeNegateNode(GLOBAL_DATA, $2.m_node), $2.m_features, $2.m_numConstants); }
  | '~' UnaryExpr                       { $$ = createNodeInfo<ExpressionNode*>(makeBitwiseNotNode(GLOBAL_DATA, $2.m_node), $2.m_features, $2.m_numConstants); }
  | '!' UnaryExpr                       { $$ = createNodeInfo<ExpressionNode*>(new LogicalNotNode(GLOBAL_DATA, $2.m_node), $2.m_features, $2.m_numConstants); }

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
  | MultiplicativeExpr '%' UnaryExpr    { $$ = createNodeInfo<ExpressionNode*>(new ModNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

MultiplicativeExprNoBF:
    UnaryExprNoBF
  | MultiplicativeExprNoBF '*' UnaryExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(makeMultNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | MultiplicativeExprNoBF '/' UnaryExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(makeDivNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | MultiplicativeExprNoBF '%' UnaryExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(new ModNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
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
  | ShiftExpr URSHIFT AdditiveExpr      { $$ = createNodeInfo<ExpressionNode*>(new UnsignedRightShiftNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

ShiftExprNoBF:
    AdditiveExprNoBF
  | ShiftExprNoBF LSHIFT AdditiveExpr   { $$ = createNodeInfo<ExpressionNode*>(makeLeftShiftNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | ShiftExprNoBF RSHIFT AdditiveExpr   { $$ = createNodeInfo<ExpressionNode*>(makeRightShiftNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | ShiftExprNoBF URSHIFT AdditiveExpr  { $$ = createNodeInfo<ExpressionNode*>(new UnsignedRightShiftNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

RelationalExpr:
    ShiftExpr
  | RelationalExpr '<' ShiftExpr        { $$ = createNodeInfo<ExpressionNode*>(new LessNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExpr '>' ShiftExpr        { $$ = createNodeInfo<ExpressionNode*>(new GreaterNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExpr LE ShiftExpr         { $$ = createNodeInfo<ExpressionNode*>(new LessEqNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExpr GE ShiftExpr         { $$ = createNodeInfo<ExpressionNode*>(new GreaterEqNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExpr INSTANCEOF ShiftExpr { InstanceOfNode* node = new InstanceOfNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @3.first_column, @3.last_column);  
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExpr INTOKEN ShiftExpr    { InNode* node = new InNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @3.first_column, @3.last_column);  
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

RelationalExprNoIn:
    ShiftExpr
  | RelationalExprNoIn '<' ShiftExpr    { $$ = createNodeInfo<ExpressionNode*>(new LessNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExprNoIn '>' ShiftExpr    { $$ = createNodeInfo<ExpressionNode*>(new GreaterNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExprNoIn LE ShiftExpr     { $$ = createNodeInfo<ExpressionNode*>(new LessEqNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExprNoIn GE ShiftExpr     { $$ = createNodeInfo<ExpressionNode*>(new GreaterEqNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExprNoIn INSTANCEOF ShiftExpr
                                        { InstanceOfNode* node = new InstanceOfNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @3.first_column, @3.last_column);  
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

RelationalExprNoBF:
    ShiftExprNoBF
  | RelationalExprNoBF '<' ShiftExpr    { $$ = createNodeInfo<ExpressionNode*>(new LessNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExprNoBF '>' ShiftExpr    { $$ = createNodeInfo<ExpressionNode*>(new GreaterNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExprNoBF LE ShiftExpr     { $$ = createNodeInfo<ExpressionNode*>(new LessEqNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExprNoBF GE ShiftExpr     { $$ = createNodeInfo<ExpressionNode*>(new GreaterEqNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExprNoBF INSTANCEOF ShiftExpr
                                        { InstanceOfNode* node = new InstanceOfNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @3.first_column, @3.last_column);  
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | RelationalExprNoBF INTOKEN ShiftExpr 
                                        { InNode* node = new InNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @3.first_column, @3.last_column);  
                                          $$ = createNodeInfo<ExpressionNode*>(node, $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

EqualityExpr:
    RelationalExpr
  | EqualityExpr EQEQ RelationalExpr    { $$ = createNodeInfo<ExpressionNode*>(new EqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | EqualityExpr NE RelationalExpr      { $$ = createNodeInfo<ExpressionNode*>(new NotEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | EqualityExpr STREQ RelationalExpr   { $$ = createNodeInfo<ExpressionNode*>(new StrictEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | EqualityExpr STRNEQ RelationalExpr  { $$ = createNodeInfo<ExpressionNode*>(new NotStrictEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

EqualityExprNoIn:
    RelationalExprNoIn
  | EqualityExprNoIn EQEQ RelationalExprNoIn
                                        { $$ = createNodeInfo<ExpressionNode*>(new EqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | EqualityExprNoIn NE RelationalExprNoIn
                                        { $$ = createNodeInfo<ExpressionNode*>(new NotEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | EqualityExprNoIn STREQ RelationalExprNoIn
                                        { $$ = createNodeInfo<ExpressionNode*>(new StrictEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | EqualityExprNoIn STRNEQ RelationalExprNoIn
                                        { $$ = createNodeInfo<ExpressionNode*>(new NotStrictEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

EqualityExprNoBF:
    RelationalExprNoBF
  | EqualityExprNoBF EQEQ RelationalExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(new EqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | EqualityExprNoBF NE RelationalExpr  { $$ = createNodeInfo<ExpressionNode*>(new NotEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | EqualityExprNoBF STREQ RelationalExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(new StrictEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
  | EqualityExprNoBF STRNEQ RelationalExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(new NotStrictEqualNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

BitwiseANDExpr:
    EqualityExpr
  | BitwiseANDExpr '&' EqualityExpr     { $$ = createNodeInfo<ExpressionNode*>(new BitAndNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

BitwiseANDExprNoIn:
    EqualityExprNoIn
  | BitwiseANDExprNoIn '&' EqualityExprNoIn
                                        { $$ = createNodeInfo<ExpressionNode*>(new BitAndNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

BitwiseANDExprNoBF:
    EqualityExprNoBF
  | BitwiseANDExprNoBF '&' EqualityExpr { $$ = createNodeInfo<ExpressionNode*>(new BitAndNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

BitwiseXORExpr:
    BitwiseANDExpr
  | BitwiseXORExpr '^' BitwiseANDExpr   { $$ = createNodeInfo<ExpressionNode*>(new BitXOrNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

BitwiseXORExprNoIn:
    BitwiseANDExprNoIn
  | BitwiseXORExprNoIn '^' BitwiseANDExprNoIn
                                        { $$ = createNodeInfo<ExpressionNode*>(new BitXOrNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

BitwiseXORExprNoBF:
    BitwiseANDExprNoBF
  | BitwiseXORExprNoBF '^' BitwiseANDExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(new BitXOrNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

BitwiseORExpr:
    BitwiseXORExpr
  | BitwiseORExpr '|' BitwiseXORExpr    { $$ = createNodeInfo<ExpressionNode*>(new BitOrNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

BitwiseORExprNoIn:
    BitwiseXORExprNoIn
  | BitwiseORExprNoIn '|' BitwiseXORExprNoIn
                                        { $$ = createNodeInfo<ExpressionNode*>(new BitOrNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

BitwiseORExprNoBF:
    BitwiseXORExprNoBF
  | BitwiseORExprNoBF '|' BitwiseXORExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(new BitOrNode(GLOBAL_DATA, $1.m_node, $3.m_node, $3.m_features & AssignFeature), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

LogicalANDExpr:
    BitwiseORExpr
  | LogicalANDExpr AND BitwiseORExpr    { $$ = createNodeInfo<ExpressionNode*>(new LogicalOpNode(GLOBAL_DATA, $1.m_node, $3.m_node, OpLogicalAnd), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

LogicalANDExprNoIn:
    BitwiseORExprNoIn
  | LogicalANDExprNoIn AND BitwiseORExprNoIn
                                        { $$ = createNodeInfo<ExpressionNode*>(new LogicalOpNode(GLOBAL_DATA, $1.m_node, $3.m_node, OpLogicalAnd), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

LogicalANDExprNoBF:
    BitwiseORExprNoBF
  | LogicalANDExprNoBF AND BitwiseORExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(new LogicalOpNode(GLOBAL_DATA, $1.m_node, $3.m_node, OpLogicalAnd), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

LogicalORExpr:
    LogicalANDExpr
  | LogicalORExpr OR LogicalANDExpr     { $$ = createNodeInfo<ExpressionNode*>(new LogicalOpNode(GLOBAL_DATA, $1.m_node, $3.m_node, OpLogicalOr), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

LogicalORExprNoIn:
    LogicalANDExprNoIn
  | LogicalORExprNoIn OR LogicalANDExprNoIn
                                        { $$ = createNodeInfo<ExpressionNode*>(new LogicalOpNode(GLOBAL_DATA, $1.m_node, $3.m_node, OpLogicalOr), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

LogicalORExprNoBF:
    LogicalANDExprNoBF
  | LogicalORExprNoBF OR LogicalANDExpr { $$ = createNodeInfo<ExpressionNode*>(new LogicalOpNode(GLOBAL_DATA, $1.m_node, $3.m_node, OpLogicalOr), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

ConditionalExpr:
    LogicalORExpr
  | LogicalORExpr '?' AssignmentExpr ':' AssignmentExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(new ConditionalNode(GLOBAL_DATA, $1.m_node, $3.m_node, $5.m_node), $1.m_features | $3.m_features | $5.m_features, $1.m_numConstants + $3.m_numConstants + $5.m_numConstants); }
;

ConditionalExprNoIn:
    LogicalORExprNoIn
  | LogicalORExprNoIn '?' AssignmentExprNoIn ':' AssignmentExprNoIn
                                        { $$ = createNodeInfo<ExpressionNode*>(new ConditionalNode(GLOBAL_DATA, $1.m_node, $3.m_node, $5.m_node), $1.m_features | $3.m_features | $5.m_features, $1.m_numConstants + $3.m_numConstants + $5.m_numConstants); }
;

ConditionalExprNoBF:
    LogicalORExprNoBF
  | LogicalORExprNoBF '?' AssignmentExpr ':' AssignmentExpr
                                        { $$ = createNodeInfo<ExpressionNode*>(new ConditionalNode(GLOBAL_DATA, $1.m_node, $3.m_node, $5.m_node), $1.m_features | $3.m_features | $5.m_features, $1.m_numConstants + $3.m_numConstants + $5.m_numConstants); }
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
  | Expr ',' AssignmentExpr             { $$ = createNodeInfo<ExpressionNode*>(new CommaNode(GLOBAL_DATA, $1.m_node, $3.m_node), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

ExprNoIn:
    AssignmentExprNoIn
  | ExprNoIn ',' AssignmentExprNoIn     { $$ = createNodeInfo<ExpressionNode*>(new CommaNode(GLOBAL_DATA, $1.m_node, $3.m_node), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
;

ExprNoBF:
    AssignmentExprNoBF
  | ExprNoBF ',' AssignmentExpr         { $$ = createNodeInfo<ExpressionNode*>(new CommaNode(GLOBAL_DATA, $1.m_node, $3.m_node), $1.m_features | $3.m_features, $1.m_numConstants + $3.m_numConstants); }
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
    OPENBRACE CLOSEBRACE                             { $$ = createNodeDeclarationInfo<StatementNode*>(new BlockNode(GLOBAL_DATA, 0), 0, 0, 0, 0);
                                          DBG($$.m_node, @1, @2); }
  | OPENBRACE SourceElements CLOSEBRACE              { $$ = createNodeDeclarationInfo<StatementNode*>(new BlockNode(GLOBAL_DATA, $2.m_node), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_features, $2.m_numConstants);
                                          DBG($$.m_node, @1, @3); }
;

VariableStatement:
    VAR VariableDeclarationList ';'     { $$ = createNodeDeclarationInfo<StatementNode*>(makeVarStatementNode(GLOBAL_DATA, $2.m_node), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_features, $2.m_numConstants);
                                          DBG($$.m_node, @1, @3); }
  | VAR VariableDeclarationList error   { $$ = createNodeDeclarationInfo<StatementNode*>(makeVarStatementNode(GLOBAL_DATA, $2.m_node), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_features, $2.m_numConstants);
                                          DBG($$.m_node, @1, @2);
                                          AUTO_SEMICOLON; }
;

VariableDeclarationList:
    IDENT                               { $$.m_node = 0;
                                          $$.m_varDeclarations = new ParserRefCountedData<DeclarationStacks::VarStack>(GLOBAL_DATA);
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, *$1, 0);
                                          $$.m_funcDeclarations = 0;
                                          $$.m_features = (*$1 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0;
                                          $$.m_numConstants = 0;
                                        }
  | IDENT Initializer                   { AssignResolveNode* node = new AssignResolveNode(GLOBAL_DATA, *$1, $2.m_node, $2.m_features & AssignFeature);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @2.first_column + 1, @2.last_column);
                                          $$.m_node = node;
                                          $$.m_varDeclarations = new ParserRefCountedData<DeclarationStacks::VarStack>(GLOBAL_DATA);
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
                                        { AssignResolveNode* node = new AssignResolveNode(GLOBAL_DATA, *$3, $4.m_node, $4.m_features & AssignFeature);
                                          SET_EXCEPTION_LOCATION(node, @3.first_column, @4.first_column + 1, @4.last_column);
                                          $$.m_node = combineVarInitializers(GLOBAL_DATA, $1.m_node, node);
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, *$3, DeclarationStacks::HasInitializer);
                                          $$.m_funcDeclarations = 0;
                                          $$.m_features = $1.m_features | ((*$3 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0) | $4.m_features;
                                          $$.m_numConstants = $1.m_numConstants + $4.m_numConstants;
                                        }
;

VariableDeclarationListNoIn:
    IDENT                               { $$.m_node = 0;
                                          $$.m_varDeclarations = new ParserRefCountedData<DeclarationStacks::VarStack>(GLOBAL_DATA);
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, *$1, 0);
                                          $$.m_funcDeclarations = 0;
                                          $$.m_features = (*$1 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0;
                                          $$.m_numConstants = 0;
                                        }
  | IDENT InitializerNoIn               { AssignResolveNode* node = new AssignResolveNode(GLOBAL_DATA, *$1, $2.m_node, $2.m_features & AssignFeature);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @2.first_column + 1, @2.last_column);
                                          $$.m_node = node;
                                          $$.m_varDeclarations = new ParserRefCountedData<DeclarationStacks::VarStack>(GLOBAL_DATA);
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
                                        { AssignResolveNode* node = new AssignResolveNode(GLOBAL_DATA, *$3, $4.m_node, $4.m_features & AssignFeature);
                                          SET_EXCEPTION_LOCATION(node, @3.first_column, @4.first_column + 1, @4.last_column);
                                          $$.m_node = combineVarInitializers(GLOBAL_DATA, $1.m_node, node);
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, *$3, DeclarationStacks::HasInitializer);
                                          $$.m_funcDeclarations = 0;
                                          $$.m_features = $1.m_features | ((*$3 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0) | $4.m_features;
                                          $$.m_numConstants = $1.m_numConstants + $4.m_numConstants;
                                        }
;

ConstStatement:
    CONSTTOKEN ConstDeclarationList ';' { $$ = createNodeDeclarationInfo<StatementNode*>(new ConstStatementNode(GLOBAL_DATA, $2.m_node.head), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_features, $2.m_numConstants);
                                          DBG($$.m_node, @1, @3); }
  | CONSTTOKEN ConstDeclarationList error
                                        { $$ = createNodeDeclarationInfo<StatementNode*>(new ConstStatementNode(GLOBAL_DATA, $2.m_node.head), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_features, $2.m_numConstants);
                                          DBG($$.m_node, @1, @2); AUTO_SEMICOLON; }
;

ConstDeclarationList:
    ConstDeclaration                    { $$.m_node.head = $1.m_node;
                                          $$.m_node.tail = $$.m_node.head;
                                          $$.m_varDeclarations = new ParserRefCountedData<DeclarationStacks::VarStack>(GLOBAL_DATA);
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, $1.m_node);
                                          $$.m_funcDeclarations = 0; 
                                          $$.m_features = $1.m_features;
                                          $$.m_numConstants = $1.m_numConstants;
    }
  | ConstDeclarationList ',' ConstDeclaration
                                        {  $$.m_node.head = $1.m_node.head;
                                          $1.m_node.tail->m_next = $3.m_node;
                                          $$.m_node.tail = $3.m_node;
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          appendToVarDeclarationList(GLOBAL_DATA, $$.m_varDeclarations, $3.m_node);
                                          $$.m_funcDeclarations = 0;
                                          $$.m_features = $1.m_features | $3.m_features;
                                          $$.m_numConstants = $1.m_numConstants + $3.m_numConstants; }
;

ConstDeclaration:
    IDENT                               { $$ = createNodeInfo<ConstDeclNode*>(new ConstDeclNode(GLOBAL_DATA, *$1, 0), (*$1 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0, 0); }
  | IDENT Initializer                   { $$ = createNodeInfo<ConstDeclNode*>(new ConstDeclNode(GLOBAL_DATA, *$1, $2.m_node), ((*$1 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0) | $2.m_features, $2.m_numConstants); }
;

Initializer:
    '=' AssignmentExpr                  { $$ = $2; }
;

InitializerNoIn:
    '=' AssignmentExprNoIn              { $$ = $2; }
;

EmptyStatement:
    ';'                                 { $$ = createNodeDeclarationInfo<StatementNode*>(new EmptyStatementNode(GLOBAL_DATA), 0, 0, 0, 0); }
;

ExprStatement:
    ExprNoBF ';'                        { $$ = createNodeDeclarationInfo<StatementNode*>(new ExprStatementNode(GLOBAL_DATA, $1.m_node), 0, 0, $1.m_features, $1.m_numConstants);
                                          DBG($$.m_node, @1, @2); }
  | ExprNoBF error                      { $$ = createNodeDeclarationInfo<StatementNode*>(new ExprStatementNode(GLOBAL_DATA, $1.m_node), 0, 0, $1.m_features, $1.m_numConstants);
                                          DBG($$.m_node, @1, @1); AUTO_SEMICOLON; }
;

IfStatement:
    IF '(' Expr ')' Statement %prec IF_WITHOUT_ELSE
                                        { $$ = createNodeDeclarationInfo<StatementNode*>(new IfNode(GLOBAL_DATA, $3.m_node, $5.m_node), $5.m_varDeclarations, $5.m_funcDeclarations, $3.m_features | $5.m_features, $3.m_numConstants + $5.m_numConstants);
                                          DBG($$.m_node, @1, @4); }
  | IF '(' Expr ')' Statement ELSE Statement
                                        { $$ = createNodeDeclarationInfo<StatementNode*>(new IfElseNode(GLOBAL_DATA, $3.m_node, $5.m_node, $7.m_node), 
                                                                                         mergeDeclarationLists($5.m_varDeclarations, $7.m_varDeclarations), mergeDeclarationLists($5.m_funcDeclarations, $7.m_funcDeclarations),
                                                                                         $3.m_features | $5.m_features | $7.m_features,
                                                                                         $3.m_numConstants + $5.m_numConstants + $7.m_numConstants); 
                                          DBG($$.m_node, @1, @4); }
;

IterationStatement:
    DO Statement WHILE '(' Expr ')' ';'    { $$ = createNodeDeclarationInfo<StatementNode*>(new DoWhileNode(GLOBAL_DATA, $2.m_node, $5.m_node), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_features | $5.m_features, $2.m_numConstants + $5.m_numConstants);
                                             DBG($$.m_node, @1, @3); }
  | DO Statement WHILE '(' Expr ')' error  { $$ = createNodeDeclarationInfo<StatementNode*>(new DoWhileNode(GLOBAL_DATA, $2.m_node, $5.m_node), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_features | $5.m_features, $2.m_numConstants + $5.m_numConstants);
                                             DBG($$.m_node, @1, @3); } // Always performs automatic semicolon insertion.
  | WHILE '(' Expr ')' Statement        { $$ = createNodeDeclarationInfo<StatementNode*>(new WhileNode(GLOBAL_DATA, $3.m_node, $5.m_node), $5.m_varDeclarations, $5.m_funcDeclarations, $3.m_features | $5.m_features, $3.m_numConstants + $5.m_numConstants);
                                          DBG($$.m_node, @1, @4); }
  | FOR '(' ExprNoInOpt ';' ExprOpt ';' ExprOpt ')' Statement
                                        { $$ = createNodeDeclarationInfo<StatementNode*>(new ForNode(GLOBAL_DATA, $3.m_node, $5.m_node, $7.m_node, $9.m_node, false), $9.m_varDeclarations, $9.m_funcDeclarations, 
                                                                                         $3.m_features | $5.m_features | $7.m_features | $9.m_features,
                                                                                         $3.m_numConstants + $5.m_numConstants + $7.m_numConstants + $9.m_numConstants);
                                          DBG($$.m_node, @1, @8); 
                                        }
  | FOR '(' VAR VariableDeclarationListNoIn ';' ExprOpt ';' ExprOpt ')' Statement
                                        { $$ = createNodeDeclarationInfo<StatementNode*>(new ForNode(GLOBAL_DATA, $4.m_node, $6.m_node, $8.m_node, $10.m_node, true),
                                                                                         mergeDeclarationLists($4.m_varDeclarations, $10.m_varDeclarations),
                                                                                         mergeDeclarationLists($4.m_funcDeclarations, $10.m_funcDeclarations),
                                                                                         $4.m_features | $6.m_features | $8.m_features | $10.m_features,
                                                                                         $4.m_numConstants + $6.m_numConstants + $8.m_numConstants + $10.m_numConstants);
                                          DBG($$.m_node, @1, @9); }
  | FOR '(' LeftHandSideExpr INTOKEN Expr ')' Statement
                                        {
                                            ForInNode* node = new ForInNode(GLOBAL_DATA, $3.m_node, $5.m_node, $7.m_node);
                                            SET_EXCEPTION_LOCATION(node, @3.first_column, @3.last_column, @5.last_column);
                                            $$ = createNodeDeclarationInfo<StatementNode*>(node, $7.m_varDeclarations, $7.m_funcDeclarations,
                                                                                           $3.m_features | $5.m_features | $7.m_features,
                                                                                           $3.m_numConstants + $5.m_numConstants + $7.m_numConstants);
                                            DBG($$.m_node, @1, @6);
                                        }
  | FOR '(' VAR IDENT INTOKEN Expr ')' Statement
                                        { ForInNode *forIn = new ForInNode(GLOBAL_DATA, *$4, 0, $6.m_node, $8.m_node, @5.first_column, @5.first_column - @4.first_column, @6.last_column - @5.first_column);
                                          SET_EXCEPTION_LOCATION(forIn, @4.first_column, @5.first_column + 1, @6.last_column);
                                          appendToVarDeclarationList(GLOBAL_DATA, $8.m_varDeclarations, *$4, DeclarationStacks::HasInitializer);
                                          $$ = createNodeDeclarationInfo<StatementNode*>(forIn, $8.m_varDeclarations, $8.m_funcDeclarations, ((*$4 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0) | $6.m_features | $8.m_features, $6.m_numConstants + $8.m_numConstants);
                                          DBG($$.m_node, @1, @7); }
  | FOR '(' VAR IDENT InitializerNoIn INTOKEN Expr ')' Statement
                                        { ForInNode *forIn = new ForInNode(GLOBAL_DATA, *$4, $5.m_node, $7.m_node, $9.m_node, @5.first_column, @5.first_column - @4.first_column, @5.last_column - @5.first_column);
                                          SET_EXCEPTION_LOCATION(forIn, @4.first_column, @6.first_column + 1, @7.last_column);
                                          appendToVarDeclarationList(GLOBAL_DATA, $9.m_varDeclarations, *$4, DeclarationStacks::HasInitializer);
                                          $$ = createNodeDeclarationInfo<StatementNode*>(forIn, $9.m_varDeclarations, $9.m_funcDeclarations,
                                                                                         ((*$4 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0) | $5.m_features | $7.m_features | $9.m_features,
                                                                                         $5.m_numConstants + $7.m_numConstants + $9.m_numConstants);
                                          DBG($$.m_node, @1, @8); }
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
    CONTINUE ';'                        { ContinueNode* node = new ContinueNode(GLOBAL_DATA);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @1.last_column, @1.last_column); 
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, 0, 0);
                                          DBG($$.m_node, @1, @2); }
  | CONTINUE error                      { ContinueNode* node = new ContinueNode(GLOBAL_DATA);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @1.last_column, @1.last_column); 
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, 0, 0);
                                          DBG($$.m_node, @1, @1); AUTO_SEMICOLON; }
  | CONTINUE IDENT ';'                  { ContinueNode* node = new ContinueNode(GLOBAL_DATA, *$2);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @2.last_column, @2.last_column); 
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, 0, 0);
                                          DBG($$.m_node, @1, @3); }
  | CONTINUE IDENT error                { ContinueNode* node = new ContinueNode(GLOBAL_DATA, *$2);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @2.last_column, @2.last_column); 
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, 0, 0);
                                          DBG($$.m_node, @1, @2); AUTO_SEMICOLON; }
;

BreakStatement:
    BREAK ';'                           { BreakNode* node = new BreakNode(GLOBAL_DATA);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @1.last_column, @1.last_column);
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, 0, 0); DBG($$.m_node, @1, @2); }
  | BREAK error                         { BreakNode* node = new BreakNode(GLOBAL_DATA);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @1.last_column, @1.last_column);
                                          $$ = createNodeDeclarationInfo<StatementNode*>(new BreakNode(GLOBAL_DATA), 0, 0, 0, 0); DBG($$.m_node, @1, @1); AUTO_SEMICOLON; }
  | BREAK IDENT ';'                     { BreakNode* node = new BreakNode(GLOBAL_DATA, *$2);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @2.last_column, @2.last_column);
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, 0, 0); DBG($$.m_node, @1, @3); }
  | BREAK IDENT error                   { BreakNode* node = new BreakNode(GLOBAL_DATA, *$2);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @2.last_column, @2.last_column);
                                          $$ = createNodeDeclarationInfo<StatementNode*>(new BreakNode(GLOBAL_DATA, *$2), 0, 0, 0, 0); DBG($$.m_node, @1, @2); AUTO_SEMICOLON; }
;

ReturnStatement:
    RETURN ';'                          { ReturnNode* node = new ReturnNode(GLOBAL_DATA, 0); 
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @1.last_column, @1.last_column); 
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, 0, 0); DBG($$.m_node, @1, @2); }
  | RETURN error                        { ReturnNode* node = new ReturnNode(GLOBAL_DATA, 0); 
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @1.last_column, @1.last_column); 
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, 0, 0); DBG($$.m_node, @1, @1); AUTO_SEMICOLON; }
  | RETURN Expr ';'                     { ReturnNode* node = new ReturnNode(GLOBAL_DATA, $2.m_node); 
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @2.last_column, @2.last_column);
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, $2.m_features, $2.m_numConstants); DBG($$.m_node, @1, @3); }
  | RETURN Expr error                   { ReturnNode* node = new ReturnNode(GLOBAL_DATA, $2.m_node); 
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @2.last_column, @2.last_column); 
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, $2.m_features, $2.m_numConstants); DBG($$.m_node, @1, @2); AUTO_SEMICOLON; }
;

WithStatement:
    WITH '(' Expr ')' Statement         { $$ = createNodeDeclarationInfo<StatementNode*>(new WithNode(GLOBAL_DATA, $3.m_node, $5.m_node, @3.last_column, @3.last_column - @3.first_column),
                                                                                         $5.m_varDeclarations, $5.m_funcDeclarations, $3.m_features | $5.m_features | WithFeature, $3.m_numConstants + $5.m_numConstants);
                                          DBG($$.m_node, @1, @4); }
;

SwitchStatement:
    SWITCH '(' Expr ')' CaseBlock       { $$ = createNodeDeclarationInfo<StatementNode*>(new SwitchNode(GLOBAL_DATA, $3.m_node, $5.m_node), $5.m_varDeclarations, $5.m_funcDeclarations,
                                                                                         $3.m_features | $5.m_features, $3.m_numConstants + $5.m_numConstants);
                                          DBG($$.m_node, @1, @4); }
;

CaseBlock:
    OPENBRACE CaseClausesOpt CLOSEBRACE              { $$ = createNodeDeclarationInfo<CaseBlockNode*>(new CaseBlockNode(GLOBAL_DATA, $2.m_node.head, 0, 0), $2.m_varDeclarations, $2.m_funcDeclarations, $2.m_features, $2.m_numConstants); }
  | OPENBRACE CaseClausesOpt DefaultClause CaseClausesOpt CLOSEBRACE
                                        { $$ = createNodeDeclarationInfo<CaseBlockNode*>(new CaseBlockNode(GLOBAL_DATA, $2.m_node.head, $3.m_node, $4.m_node.head),
                                                                                         mergeDeclarationLists(mergeDeclarationLists($2.m_varDeclarations, $3.m_varDeclarations), $4.m_varDeclarations),
                                                                                         mergeDeclarationLists(mergeDeclarationLists($2.m_funcDeclarations, $3.m_funcDeclarations), $4.m_funcDeclarations),
                                                                                         $2.m_features | $3.m_features | $4.m_features,
                                                                                         $2.m_numConstants + $3.m_numConstants + $4.m_numConstants); }
;

CaseClausesOpt:
/* nothing */                       { $$.m_node.head = 0; $$.m_node.tail = 0; $$.m_varDeclarations = 0; $$.m_funcDeclarations = 0; $$.m_features = 0; $$.m_numConstants = 0; }
  | CaseClauses
;

CaseClauses:
    CaseClause                          { $$.m_node.head = new ClauseListNode(GLOBAL_DATA, $1.m_node);
                                          $$.m_node.tail = $$.m_node.head;
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          $$.m_funcDeclarations = $1.m_funcDeclarations; 
                                          $$.m_features = $1.m_features;
                                          $$.m_numConstants = $1.m_numConstants; }
  | CaseClauses CaseClause              { $$.m_node.head = $1.m_node.head;
                                          $$.m_node.tail = new ClauseListNode(GLOBAL_DATA, $1.m_node.tail, $2.m_node);
                                          $$.m_varDeclarations = mergeDeclarationLists($1.m_varDeclarations, $2.m_varDeclarations);
                                          $$.m_funcDeclarations = mergeDeclarationLists($1.m_funcDeclarations, $2.m_funcDeclarations);
                                          $$.m_features = $1.m_features | $2.m_features;
                                          $$.m_numConstants = $1.m_numConstants + $2.m_numConstants;
                                        }
;

CaseClause:
    CASE Expr ':'                       { $$ = createNodeDeclarationInfo<CaseClauseNode*>(new CaseClauseNode(GLOBAL_DATA, $2.m_node), 0, 0, $2.m_features, $2.m_numConstants); }
  | CASE Expr ':' SourceElements        { $$ = createNodeDeclarationInfo<CaseClauseNode*>(new CaseClauseNode(GLOBAL_DATA, $2.m_node, $4.m_node), $4.m_varDeclarations, $4.m_funcDeclarations, $2.m_features | $4.m_features, $2.m_numConstants + $4.m_numConstants); }
;

DefaultClause:
    DEFAULT ':'                         { $$ = createNodeDeclarationInfo<CaseClauseNode*>(new CaseClauseNode(GLOBAL_DATA, 0), 0, 0, 0, 0); }
  | DEFAULT ':' SourceElements          { $$ = createNodeDeclarationInfo<CaseClauseNode*>(new CaseClauseNode(GLOBAL_DATA, 0, $3.m_node), $3.m_varDeclarations, $3.m_funcDeclarations, $3.m_features, $3.m_numConstants); }
;

LabelledStatement:
    IDENT ':' Statement                 { LabelNode* node = new LabelNode(GLOBAL_DATA, *$1, $3.m_node);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @2.last_column, @2.last_column);
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, $3.m_varDeclarations, $3.m_funcDeclarations, $3.m_features, $3.m_numConstants); }
;

ThrowStatement:
    THROW Expr ';'                      { ThrowNode* node = new ThrowNode(GLOBAL_DATA, $2.m_node);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @2.last_column, @2.last_column);
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, $2.m_features, $2.m_numConstants); DBG($$.m_node, @1, @2);
                                        }
  | THROW Expr error                    { ThrowNode* node = new ThrowNode(GLOBAL_DATA, $2.m_node);
                                          SET_EXCEPTION_LOCATION(node, @1.first_column, @2.last_column, @2.last_column);
                                          $$ = createNodeDeclarationInfo<StatementNode*>(node, 0, 0, $2.m_features, $2.m_numConstants); DBG($$.m_node, @1, @2); AUTO_SEMICOLON; 
                                        }
;

TryStatement:
    TRY Block FINALLY Block             { $$ = createNodeDeclarationInfo<StatementNode*>(new TryNode(GLOBAL_DATA, $2.m_node, GLOBAL_DATA->propertyNames->nullIdentifier, 0, $4.m_node),
                                                                                         mergeDeclarationLists($2.m_varDeclarations, $4.m_varDeclarations),
                                                                                         mergeDeclarationLists($2.m_funcDeclarations, $4.m_funcDeclarations),
                                                                                         $2.m_features | $4.m_features,
                                                                                         $2.m_numConstants + $4.m_numConstants);
                                          DBG($$.m_node, @1, @2); }
  | TRY Block CATCH '(' IDENT ')' Block { $$ = createNodeDeclarationInfo<StatementNode*>(new TryNode(GLOBAL_DATA, $2.m_node, *$5, $7.m_node, 0),
                                                                                         mergeDeclarationLists($2.m_varDeclarations, $7.m_varDeclarations),
                                                                                         mergeDeclarationLists($2.m_funcDeclarations, $7.m_funcDeclarations),
                                                                                         $2.m_features | $7.m_features | CatchFeature,
                                                                                         $2.m_numConstants + $7.m_numConstants);
                                          DBG($$.m_node, @1, @2); }
  | TRY Block CATCH '(' IDENT ')' Block FINALLY Block
                                        { $$ = createNodeDeclarationInfo<StatementNode*>(new TryNode(GLOBAL_DATA, $2.m_node, *$5, $7.m_node, $9.m_node),
                                                                                         mergeDeclarationLists(mergeDeclarationLists($2.m_varDeclarations, $7.m_varDeclarations), $9.m_varDeclarations),
                                                                                         mergeDeclarationLists(mergeDeclarationLists($2.m_funcDeclarations, $7.m_funcDeclarations), $9.m_funcDeclarations),
                                                                                         $2.m_features | $7.m_features | $9.m_features | CatchFeature,
                                                                                         $2.m_numConstants + $7.m_numConstants + $9.m_numConstants);
                                          DBG($$.m_node, @1, @2); }
;

DebuggerStatement:
    DEBUGGER ';'                        { $$ = createNodeDeclarationInfo<StatementNode*>(new DebuggerStatementNode(GLOBAL_DATA), 0, 0, 0, 0);
                                          DBG($$.m_node, @1, @2); }
  | DEBUGGER error                      { $$ = createNodeDeclarationInfo<StatementNode*>(new DebuggerStatementNode(GLOBAL_DATA), 0, 0, 0, 0);
                                          DBG($$.m_node, @1, @1); AUTO_SEMICOLON; }
;

FunctionDeclaration:
    FUNCTION IDENT '(' ')' OPENBRACE FunctionBody CLOSEBRACE { $$ = createNodeInfo(new FuncDeclNode(GLOBAL_DATA, *$2, $6, LEXER->sourceCode($5, $7, @5.first_line)), ((*$2 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0) | ClosureFeature, 0); DBG($6, @5, @7); }
  | FUNCTION IDENT '(' FormalParameterList ')' OPENBRACE FunctionBody CLOSEBRACE
      { 
          $$ = createNodeInfo(new FuncDeclNode(GLOBAL_DATA, *$2, $7, LEXER->sourceCode($6, $8, @6.first_line), $4.m_node.head), ((*$2 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0) | $4.m_features | ClosureFeature, 0); 
          if ($4.m_features & ArgumentsFeature)
              $7->setUsesArguments(); 
          DBG($7, @6, @8); 
      }
;

FunctionExpr:
    FUNCTION '(' ')' OPENBRACE FunctionBody CLOSEBRACE { $$ = createNodeInfo(new FuncExprNode(GLOBAL_DATA, GLOBAL_DATA->propertyNames->nullIdentifier, $5, LEXER->sourceCode($4, $6, @4.first_line)), ClosureFeature, 0); DBG($5, @4, @6); }
    | FUNCTION '(' FormalParameterList ')' OPENBRACE FunctionBody CLOSEBRACE 
      { 
          $$ = createNodeInfo(new FuncExprNode(GLOBAL_DATA, GLOBAL_DATA->propertyNames->nullIdentifier, $6, LEXER->sourceCode($5, $7, @5.first_line), $3.m_node.head), $3.m_features | ClosureFeature, 0); 
          if ($3.m_features & ArgumentsFeature) 
              $6->setUsesArguments();
          DBG($6, @5, @7); 
      }
  | FUNCTION IDENT '(' ')' OPENBRACE FunctionBody CLOSEBRACE { $$ = createNodeInfo(new FuncExprNode(GLOBAL_DATA, *$2, $6, LEXER->sourceCode($5, $7, @5.first_line)), ClosureFeature, 0); DBG($6, @5, @7); }
  | FUNCTION IDENT '(' FormalParameterList ')' OPENBRACE FunctionBody CLOSEBRACE 
      { 
          $$ = createNodeInfo(new FuncExprNode(GLOBAL_DATA, *$2, $7, LEXER->sourceCode($6, $8, @6.first_line), $4.m_node.head), $4.m_features | ClosureFeature, 0); 
          if ($4.m_features & ArgumentsFeature)
              $7->setUsesArguments();
          DBG($7, @6, @8); 
      }
;

FormalParameterList:
    IDENT                               { $$.m_node.head = new ParameterNode(GLOBAL_DATA, *$1);
                                          $$.m_features = (*$1 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0;
                                          $$.m_node.tail = $$.m_node.head; }
  | FormalParameterList ',' IDENT       { $$.m_node.head = $1.m_node.head;
                                          $$.m_features = $1.m_features | ((*$3 == GLOBAL_DATA->propertyNames->arguments) ? ArgumentsFeature : 0);
                                          $$.m_node.tail = new ParameterNode(GLOBAL_DATA, $1.m_node.tail, *$3);  }
;

FunctionBody:
    /* not in spec */           { $$ = FunctionBodyNode::create(GLOBAL_DATA, 0, 0, 0, NoFeatures, 0); }
  | SourceElements              { $$ = FunctionBodyNode::create(GLOBAL_DATA, $1.m_node, $1.m_varDeclarations ? &$1.m_varDeclarations->data : 0, 
                                                                $1.m_funcDeclarations ? &$1.m_funcDeclarations->data : 0,
                                                                $1.m_features, $1.m_numConstants);
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
    /* not in spec */                   { GLOBAL_DATA->parser->didFinishParsing(new SourceElements(GLOBAL_DATA), 0, 0, NoFeatures, @0.last_line, 0); }
    | SourceElements                    { GLOBAL_DATA->parser->didFinishParsing($1.m_node, $1.m_varDeclarations, $1.m_funcDeclarations, $1.m_features, 
                                                                                @1.last_line, $1.m_numConstants); }
;

SourceElements:
    SourceElement                       { $$.m_node = new SourceElements(GLOBAL_DATA);
                                          $$.m_node->append($1.m_node);
                                          $$.m_varDeclarations = $1.m_varDeclarations;
                                          $$.m_funcDeclarations = $1.m_funcDeclarations;
                                          $$.m_features = $1.m_features;
                                          $$.m_numConstants = $1.m_numConstants;
                                        }
  | SourceElements SourceElement        { $$.m_node->append($2.m_node);
                                          $$.m_varDeclarations = mergeDeclarationLists($1.m_varDeclarations, $2.m_varDeclarations);
                                          $$.m_funcDeclarations = mergeDeclarationLists($1.m_funcDeclarations, $2.m_funcDeclarations);
                                          $$.m_features = $1.m_features | $2.m_features;
                                          $$.m_numConstants = $1.m_numConstants + $2.m_numConstants;
                                        }
;

SourceElement:
    FunctionDeclaration                 { $$ = createNodeDeclarationInfo<StatementNode*>($1.m_node, 0, new ParserRefCountedData<DeclarationStacks::FunctionStack>(GLOBAL_DATA), $1.m_features, 0); $$.m_funcDeclarations->data.append($1.m_node); }
  | Statement                           { $$ = $1; }
;
 
%%

static ExpressionNode* makeAssignNode(void* globalPtr, ExpressionNode* loc, Operator op, ExpressionNode* expr, bool locHasAssignments, bool exprHasAssignments, int start, int divot, int end)
{
    if (!loc->isLocation())
        return new AssignErrorNode(GLOBAL_DATA, loc, op, expr, divot, divot - start, end - divot);

    if (loc->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(loc);
        if (op == OpEqual) {
            AssignResolveNode* node = new AssignResolveNode(GLOBAL_DATA, resolve->identifier(), expr, exprHasAssignments);
            SET_EXCEPTION_LOCATION(node, start, divot, end);
            return node;
        } else
            return new ReadModifyResolveNode(GLOBAL_DATA, resolve->identifier(), op, expr, exprHasAssignments, divot, divot - start, end - divot);
    }
    if (loc->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(loc);
        if (op == OpEqual)
            return new AssignBracketNode(GLOBAL_DATA, bracket->base(), bracket->subscript(), expr, locHasAssignments, exprHasAssignments, bracket->divot(), bracket->divot() - start, end - bracket->divot());
        else {
            ReadModifyBracketNode* node = new ReadModifyBracketNode(GLOBAL_DATA, bracket->base(), bracket->subscript(), op, expr, locHasAssignments, exprHasAssignments, divot, divot - start, end - divot);
            node->setSubexpressionInfo(bracket->divot(), bracket->endOffset());
            return node;
        }
    }
    ASSERT(loc->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(loc);
    if (op == OpEqual)
        return new AssignDotNode(GLOBAL_DATA, dot->base(), dot->identifier(), expr, exprHasAssignments, dot->divot(), dot->divot() - start, end - dot->divot());

    ReadModifyDotNode* node = new ReadModifyDotNode(GLOBAL_DATA, dot->base(), dot->identifier(), op, expr, exprHasAssignments, divot, divot - start, end - divot);
    node->setSubexpressionInfo(dot->divot(), dot->endOffset());
    return node;
}

static ExpressionNode* makePrefixNode(void* globalPtr, ExpressionNode* expr, Operator op, int start, int divot, int end)
{
    if (!expr->isLocation())
        return new PrefixErrorNode(GLOBAL_DATA, expr, op, divot, divot - start, end - divot);
    
    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        return new PrefixResolveNode(GLOBAL_DATA, resolve->identifier(), op, divot, divot - start, end - divot);
    }
    if (expr->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(expr);
        PrefixBracketNode* node = new PrefixBracketNode(GLOBAL_DATA, bracket->base(), bracket->subscript(), op, divot, divot - start, end - divot);
        node->setSubexpressionInfo(bracket->divot(), bracket->startOffset());
        return node;
    }
    ASSERT(expr->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(expr);
    PrefixDotNode* node = new PrefixDotNode(GLOBAL_DATA, dot->base(), dot->identifier(), op, divot, divot - start, end - divot);
    node->setSubexpressionInfo(dot->divot(), dot->startOffset());
    return node;
}

static ExpressionNode* makePostfixNode(void* globalPtr, ExpressionNode* expr, Operator op, int start, int divot, int end)
{ 
    if (!expr->isLocation())
        return new PostfixErrorNode(GLOBAL_DATA, expr, op, divot, divot - start, end - divot);
    
    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        return new PostfixResolveNode(GLOBAL_DATA, resolve->identifier(), op, divot, divot - start, end - divot);
    }
    if (expr->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(expr);
        PostfixBracketNode* node = new PostfixBracketNode(GLOBAL_DATA, bracket->base(), bracket->subscript(), op, divot, divot - start, end - divot);
        node->setSubexpressionInfo(bracket->divot(), bracket->endOffset());
        return node;
        
    }
    ASSERT(expr->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(expr);
    PostfixDotNode* node = new PostfixDotNode(GLOBAL_DATA, dot->base(), dot->identifier(), op, divot, divot - start, end - divot);
    node->setSubexpressionInfo(dot->divot(), dot->endOffset());
    return node;
}

static ExpressionNodeInfo makeFunctionCallNode(void* globalPtr, ExpressionNodeInfo func, ArgumentsNodeInfo args, int start, int divot, int end)
{
    CodeFeatures features = func.m_features | args.m_features;
    int numConstants = func.m_numConstants + args.m_numConstants;
    if (!func.m_node->isLocation())
        return createNodeInfo<ExpressionNode*>(new FunctionCallValueNode(GLOBAL_DATA, func.m_node, args.m_node, divot, divot - start, end - divot), features, numConstants);
    if (func.m_node->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(func.m_node);
        const Identifier& identifier = resolve->identifier();
        if (identifier == GLOBAL_DATA->propertyNames->eval)
            return createNodeInfo<ExpressionNode*>(new EvalFunctionCallNode(GLOBAL_DATA, args.m_node, divot, divot - start, end - divot), EvalFeature | features, numConstants);
        return createNodeInfo<ExpressionNode*>(new FunctionCallResolveNode(GLOBAL_DATA, identifier, args.m_node, divot, divot - start, end - divot), features, numConstants);
    }
    if (func.m_node->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(func.m_node);
        FunctionCallBracketNode* node = new FunctionCallBracketNode(GLOBAL_DATA, bracket->base(), bracket->subscript(), args.m_node, divot, divot - start, end - divot);
        node->setSubexpressionInfo(bracket->divot(), bracket->endOffset());
        return createNodeInfo<ExpressionNode*>(node, features, numConstants);
    }
    ASSERT(func.m_node->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(func.m_node);
    FunctionCallDotNode* node = new FunctionCallDotNode(GLOBAL_DATA, dot->base(), dot->identifier(), args.m_node, divot, divot - start, end - divot);
    node->setSubexpressionInfo(dot->divot(), dot->endOffset());
    return createNodeInfo<ExpressionNode*>(node, features, numConstants);
}

static ExpressionNode* makeTypeOfNode(void* globalPtr, ExpressionNode* expr)
{
    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        return new TypeOfResolveNode(GLOBAL_DATA, resolve->identifier());
    }
    return new TypeOfValueNode(GLOBAL_DATA, expr);
}

static ExpressionNode* makeDeleteNode(void* globalPtr, ExpressionNode* expr, int start, int divot, int end)
{
    if (!expr->isLocation())
        return new DeleteValueNode(GLOBAL_DATA, expr);
    if (expr->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(expr);
        return new DeleteResolveNode(GLOBAL_DATA, resolve->identifier(), divot, divot - start, end - divot);
    }
    if (expr->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(expr);
        return new DeleteBracketNode(GLOBAL_DATA, bracket->base(), bracket->subscript(), divot, divot - start, end - divot);
    }
    ASSERT(expr->isDotAccessorNode());
    DotAccessorNode* dot = static_cast<DotAccessorNode*>(expr);
    return new DeleteDotNode(GLOBAL_DATA, dot->base(), dot->identifier(), divot, divot - start, end - divot);
}

static PropertyNode* makeGetterOrSetterPropertyNode(void* globalPtr, const Identifier& getOrSet, const Identifier& name, ParameterNode* params, FunctionBodyNode* body, const SourceCode& source)
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

static ExpressionNode* makeBitwiseNotNode(void* globalPtr, ExpressionNode* expr)
{
    if (expr->isNumber())
        return makeNumberNode(globalPtr, ~toInt32(static_cast<NumberNode*>(expr)->value()));
    return new BitwiseNotNode(GLOBAL_DATA, expr);
}

static ExpressionNode* makeMultNode(void* globalPtr, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    expr1 = expr1->stripUnaryPlus();
    expr2 = expr2->stripUnaryPlus();

    if (expr1->isNumber() && expr2->isNumber())
        return makeNumberNode(globalPtr, static_cast<NumberNode*>(expr1)->value() * static_cast<NumberNode*>(expr2)->value());

    if (expr1->isNumber() && static_cast<NumberNode*>(expr1)->value() == 1)
        return new UnaryPlusNode(GLOBAL_DATA, expr2);

    if (expr2->isNumber() && static_cast<NumberNode*>(expr2)->value() == 1)
        return new UnaryPlusNode(GLOBAL_DATA, expr1);

    return new MultNode(GLOBAL_DATA, expr1, expr2, rightHasAssignments);
}

static ExpressionNode* makeDivNode(void* globalPtr, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    expr1 = expr1->stripUnaryPlus();
    expr2 = expr2->stripUnaryPlus();

    if (expr1->isNumber() && expr2->isNumber())
        return makeNumberNode(globalPtr, static_cast<NumberNode*>(expr1)->value() / static_cast<NumberNode*>(expr2)->value());
    return new DivNode(GLOBAL_DATA, expr1, expr2, rightHasAssignments);
}

static ExpressionNode* makeAddNode(void* globalPtr, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber())
        return makeNumberNode(globalPtr, static_cast<NumberNode*>(expr1)->value() + static_cast<NumberNode*>(expr2)->value());
    return new AddNode(GLOBAL_DATA, expr1, expr2, rightHasAssignments);
}

static ExpressionNode* makeSubNode(void* globalPtr, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    expr1 = expr1->stripUnaryPlus();
    expr2 = expr2->stripUnaryPlus();

    if (expr1->isNumber() && expr2->isNumber())
        return makeNumberNode(globalPtr, static_cast<NumberNode*>(expr1)->value() - static_cast<NumberNode*>(expr2)->value());
    return new SubNode(GLOBAL_DATA, expr1, expr2, rightHasAssignments);
}

static ExpressionNode* makeLeftShiftNode(void* globalPtr, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber())
        return makeNumberNode(globalPtr, toInt32(static_cast<NumberNode*>(expr1)->value()) << (toUInt32(static_cast<NumberNode*>(expr2)->value()) & 0x1f));
    return new LeftShiftNode(GLOBAL_DATA, expr1, expr2, rightHasAssignments);
}

static ExpressionNode* makeRightShiftNode(void* globalPtr, ExpressionNode* expr1, ExpressionNode* expr2, bool rightHasAssignments)
{
    if (expr1->isNumber() && expr2->isNumber())
        return makeNumberNode(globalPtr, toInt32(static_cast<NumberNode*>(expr1)->value()) >> (toUInt32(static_cast<NumberNode*>(expr2)->value()) & 0x1f));
    return new RightShiftNode(GLOBAL_DATA, expr1, expr2, rightHasAssignments);
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
