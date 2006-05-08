%{
#include "config.h"

#include "XPathParser.h"
#include "XPathFunctions.h"
#include "XPathPath.h"
#include "XPathStep.h"
#include "XPathPredicate.h"
#include "XPathUtil.h"
#include "XPathVariableReference.h"
    
#include "XPathNSResolver.h"
#include "XPathEvaluator.h"
#include "ExceptionCode.h"

#define YYDEBUG 0
#define YYPARSE_PARAM parser

using namespace WebCore;
using namespace XPath;

%}

%pure_parser

%union
{
    Step::AxisType axisType;
    int        num;
    String *str;
    Expression* expr;
    Vector<Predicate*>* predList;
    Vector<Expression*>* argList;
    Step* step;
    LocationPath *locationPath;
}

%{

int xpathyylex(YYSTYPE *yylval) { return Parser::current()->lex(yylval); }
void xpathyyerror(const char *str) { }
    
%}

%left <num> MULOP RELOP EQOP
%left <str> PLUS MINUS
%left <str> OR AND
%token <axisType> AXISNAME
%token <str> NODETYPE PI FUNCTIONNAME LITERAL
%token <str> VARIABLEREFERENCE NUMBER
%token <str> DOTDOT SLASHSLASH NAMETEST
%token ERROR

%type <locationPath> LocationPath
%type <locationPath> AbsoluteLocationPath
%type <locationPath> RelativeLocationPath
%type <step> Step
%type <axisType> AxisSpecifier
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
        static_cast<Parser*>(parser)->m_topExpr = $1;
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
        static_cast<Parser*>(parser)->registerParseNode($$);
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
        static_cast<Parser*>(parser)->unregisterParseNode($1);
    }
    ;

RelativeLocationPath:
    Step
    {
        $$ = new LocationPath;
        $$->m_steps.append($1);
        static_cast<Parser*>(parser)->registerParseNode($$);
        static_cast<Parser*>(parser)->unregisterParseNode($1);
    }
    |
    RelativeLocationPath '/' Step
    {
        $$->m_steps.append($3);
        static_cast<Parser*>(parser)->unregisterParseNode($3);
    }
    |
    RelativeLocationPath DescendantOrSelf Step
    {
        $$->m_steps.append($2);
        $$->m_steps.append($3);
        static_cast<Parser*>(parser)->unregisterParseNode($2);
        static_cast<Parser*>(parser)->unregisterParseNode($3);
    }
    ;

Step:
    NodeTest
    {
        $$ = new Step(Step::ChildAxis, *$1);
        delete $1;
        static_cast<Parser*>(parser)->registerParseNode($$);
        static_cast<Parser*>(parser)->unregisterString($1);
    }
    |
    NodeTest PredicateList
    {
        $$ = new Step(Step::ChildAxis, *$1, *$2);
        delete $1;
        delete $2;
        static_cast<Parser*>(parser)->registerParseNode($$);
        static_cast<Parser*>(parser)->unregisterString($1);
        static_cast<Parser*>(parser)->unregisterPredicateVector($2);
    }
    |
    AxisSpecifier NodeTest
    {
        $$ = new Step($1, *$2);
        delete $2;
        static_cast<Parser*>(parser)->registerParseNode($$);
        static_cast<Parser*>(parser)->unregisterString($2);
    }
    |
    AxisSpecifier NodeTest PredicateList
    {
        $$ = new Step($1, *$2,  *$3);
        delete $2;
        delete $3;
        static_cast<Parser*>(parser)->registerParseNode($$);
        static_cast<Parser*>(parser)->unregisterString($2);
        static_cast<Parser*>(parser)->unregisterPredicateVector($3);
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
        const int colon = $1->find(':');
        if (colon > -1) {
            String prefix($1->left(colon));
            XPathNSResolver *resolver = Expression::evaluationContext().resolver;
            if (!resolver || resolver->lookupNamespaceURI(prefix).isNull()) {
                static_cast<Parser*>(parser)->m_gotNamespaceError = true;
                YYABORT;
            }
        }
    }
    |
    NODETYPE '(' ')'
    {
        $$ = new String(*$1 + "()");
        delete $1;
        static_cast<Parser*>(parser)->registerString($$);
        static_cast<Parser*>(parser)->unregisterString($1);
    }
    |
    PI '(' ')'
    |
    PI '(' LITERAL ')'
    {
        String s = *$1 + " " + *$3;
        $$ = new String(s.deprecatedString().stripWhiteSpace());
        delete $1;
        delete $3;
        static_cast<Parser*>(parser)->registerString($$);
        static_cast<Parser*>(parser)->unregisterString($1);        
        static_cast<Parser*>(parser)->unregisterString($3);
    }
    ;

PredicateList:
    Predicate
    {
        $$ = new Vector<Predicate*>;
        $$->append(new Predicate($1));
        static_cast<Parser*>(parser)->registerPredicateVector($$);
        static_cast<Parser*>(parser)->unregisterParseNode($1);
    }
    |
    PredicateList Predicate
    {
        $$->append(new Predicate($2));
        static_cast<Parser*>(parser)->unregisterParseNode($2);
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
        static_cast<Parser*>(parser)->registerParseNode($$);
    }
    ;

AbbreviatedStep:
    '.'
    {
        $$ = new Step(Step::SelfAxis, "node()");
        static_cast<Parser*>(parser)->registerParseNode($$);
    }
    |
    DOTDOT
    {
        $$ = new Step(Step::ParentAxis, "node()");
        static_cast<Parser*>(parser)->registerParseNode($$);
    }
    ;

PrimaryExpr:
    VARIABLEREFERENCE
    {
        $$ = new VariableReference(*$1);
        delete $1;
        static_cast<Parser*>(parser)->registerParseNode($$);
        static_cast<Parser*>(parser)->unregisterString($1);
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
        delete $1;
        static_cast<Parser*>(parser)->registerParseNode($$);
        static_cast<Parser*>(parser)->unregisterString($1);        
    }
    |
    NUMBER
    {
        $$ = new Number($1->deprecatedString().toDouble());
        delete $1;
        static_cast<Parser*>(parser)->registerParseNode($$);
        static_cast<Parser*>(parser)->unregisterString($1);
    }
    |
    FunctionCall
    ;

FunctionCall:
    FUNCTIONNAME '(' ')'
    {
        $$ = FunctionLibrary::self().createFunction($1->deprecatedString().latin1());
        delete $1;
        static_cast<Parser*>(parser)->registerParseNode($$);
        static_cast<Parser*>(parser)->unregisterString($1);
    }
    |
    FUNCTIONNAME '(' ArgumentList ')'
    {
        $$ = FunctionLibrary::self().createFunction($1->deprecatedString().latin1(), *$3);
        delete $1;
        delete $3;
        static_cast<Parser*>(parser)->registerParseNode($$);
        static_cast<Parser*>(parser)->unregisterString($1);
        static_cast<Parser*>(parser)->unregisterExpressionVector($3);
    }
    ;

ArgumentList:
    Argument
    {
        $$ = new Vector<Expression*>;
        $$->append($1);
        static_cast<Parser*>(parser)->registerExpressionVector($$);
        static_cast<Parser*>(parser)->unregisterParseNode($1);
    }
    |
    ArgumentList ',' Argument
    {
        $$->append($3);
        static_cast<Parser*>(parser)->unregisterParseNode($3);
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
        static_cast<Parser*>(parser)->registerParseNode($$);
        static_cast<Parser*>(parser)->unregisterParseNode($1);
        static_cast<Parser*>(parser)->unregisterParseNode($3);
    }
    ;

PathExpr:
    LocationPath
    {
        $$ = $1;
    }
    |
    FilterExpr
    {
        $$ = $1;
    }
    |
    FilterExpr '/' RelativeLocationPath
    {
        $3->m_absolute = true;
        $$ = new Path(static_cast<Filter*>($1), $3);
        static_cast<Parser*>(parser)->registerParseNode($$);
        static_cast<Parser*>(parser)->unregisterParseNode($1);
        static_cast<Parser*>(parser)->unregisterParseNode($3);
    }
    |
    FilterExpr DescendantOrSelf RelativeLocationPath
    {
        $3->m_steps.insert(0, $2);
        $3->m_absolute = true;
        $$ = new Path(static_cast<Filter*>($1), $3);
        static_cast<Parser*>(parser)->registerParseNode($$);
        static_cast<Parser*>(parser)->unregisterParseNode($1);
        static_cast<Parser*>(parser)->unregisterParseNode($3);
    }
    ;

FilterExpr:
    PrimaryExpr
    {
        $$ = $1;
    }
    |
    PrimaryExpr PredicateList
    {
        $$ = new Filter($1, *$2);
        delete $2;
        static_cast<Parser*>(parser)->registerParseNode($$);
        static_cast<Parser*>(parser)->unregisterParseNode($1);
        static_cast<Parser*>(parser)->unregisterPredicateVector($2);
    }
    ;

OrExpr:
    AndExpr
    |
    OrExpr OR AndExpr
    {
        $$ = new LogicalOp(LogicalOp::OP_Or, $1, $3);
        static_cast<Parser*>(parser)->registerParseNode($$);
        static_cast<Parser*>(parser)->unregisterParseNode($1);
        static_cast<Parser*>(parser)->unregisterParseNode($3);
    }
    ;

AndExpr:
    EqualityExpr
    |
    AndExpr AND EqualityExpr
    {
        $$ = new LogicalOp(LogicalOp::OP_And, $1, $3);
        static_cast<Parser*>(parser)->registerParseNode($$);
        static_cast<Parser*>(parser)->unregisterParseNode($1);
        static_cast<Parser*>(parser)->unregisterParseNode($3);
    }
    ;

EqualityExpr:
    RelationalExpr
    |
    EqualityExpr EQOP RelationalExpr
    {
        $$ = new EqTestOp($2, $1, $3);
        static_cast<Parser*>(parser)->registerParseNode($$);
        static_cast<Parser*>(parser)->unregisterParseNode($1);
        static_cast<Parser*>(parser)->unregisterParseNode($3);
    }
    ;

RelationalExpr:
    AdditiveExpr
    |
    RelationalExpr RELOP AdditiveExpr
    {
        $$ = new NumericOp($2, $1, $3);
        static_cast<Parser*>(parser)->registerParseNode($$);
        static_cast<Parser*>(parser)->unregisterParseNode($1);
        static_cast<Parser*>(parser)->unregisterParseNode($3);
    }
    ;

AdditiveExpr:
    MultiplicativeExpr
    |
    AdditiveExpr PLUS MultiplicativeExpr
    {
        $$ = new NumericOp(NumericOp::OP_Add, $1, $3);
        static_cast<Parser*>(parser)->registerParseNode($$);
        static_cast<Parser*>(parser)->unregisterParseNode($1);
        static_cast<Parser*>(parser)->unregisterParseNode($3);
    }
    |
    AdditiveExpr MINUS MultiplicativeExpr
    {
        $$ = new NumericOp(NumericOp::OP_Sub, $1, $3);
        static_cast<Parser*>(parser)->registerParseNode($$);
        static_cast<Parser*>(parser)->unregisterParseNode($1);
        static_cast<Parser*>(parser)->unregisterParseNode($3);
    }
    ;

MultiplicativeExpr:
    UnaryExpr
    |
    MultiplicativeExpr MULOP UnaryExpr
    {
        $$ = new NumericOp($2, $1, $3);
        static_cast<Parser*>(parser)->registerParseNode($$);
        static_cast<Parser*>(parser)->unregisterParseNode($1);
        static_cast<Parser*>(parser)->unregisterParseNode($3);
    }
    ;

UnaryExpr:
    UnionExpr
    |
    MINUS UnaryExpr
    {
        $$ = new Negative;
        $$->addSubExpression($2);
        static_cast<Parser*>(parser)->registerParseNode($$);
        static_cast<Parser*>(parser)->unregisterParseNode($2);
    }
    ;


