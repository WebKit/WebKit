%{
#include "functions.h"
#include "path.h"
#include "predicate.h"
#include "util.h"
#include <qvaluevector.h>
#include <qpair.h>

extern int xpathyylex();
extern void xpathyyerror(const char *msg);
extern void initTokenizer(QString str);

Path *parseStatement( const DomString &statement );

static Path *_lastPath;

%}

%union
{
	Step::AxisType axisType;
	int        num;
	DomString *str;
	Expression *expr;
	QValueList<Predicate *> *predList;
	QValueList<Expression *> *argList;
	Step *step;
	Path *path;
}

%{
#include "expression.h"
#include "util.h"
#include "variablereference.h"
%}

%left <num> MULOP RELOP EQOP
%left <str> PLUS MINUS
%left <str> OR AND
%token <axisType> AXISNAME
%token <str> NODETYPE PI FUNCTIONNAME LITERAL
%token <str> VARIABLEREFERENCE NUMBER
%token <str> DOTDOT SLASHSLASH NAMETEST
%token ERROR

%type <path> LocationPath
%type <path> AbsoluteLocationPath
%type <path> RelativeLocationPath
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

LocationPath:
	RelativeLocationPath
	{
		$$->m_absolute = false;
		_lastPath = $$;
	}
	|
	AbsoluteLocationPath
	{
		$$->m_absolute = true;
		_lastPath = $$;
	}
	;

AbsoluteLocationPath:
	'/'
	{
		$$ = new Path;
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
		$$->m_steps.prepend( $1 );
	}
	;

RelativeLocationPath:
	Step
	{
		$$ = new Path;
		$$->m_steps.append( $1 );
	}
	|
	RelativeLocationPath '/' Step
	{
		$$->m_steps.append( $3 );
	}
	|
	RelativeLocationPath DescendantOrSelf Step
	{
		$$->m_steps.append( $2 );
		$$->m_steps.append( $3 );
	}
	;

Step:
	NodeTest
	{
		$$ = new Step( Step::ChildAxis, *$1 );
		delete $1;
	}
	|
	NodeTest PredicateList
	{
		$$ = new Step( Step::ChildAxis, *$1, *$2 );
		delete $1;
		delete $2;
	}
	|
	AxisSpecifier NodeTest
	{
		$$ = new Step( $1, *$2 );
		delete $2;
	}
	|
	AxisSpecifier NodeTest PredicateList
	{
		$$ = new Step( $1, *$2,  *$3 );
		delete $2;
		delete $3;
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
	|
	NODETYPE '(' ')'
	|
	PI '(' LITERAL ')'
	{
		DomString s = *$1 + " " + *$3;
		s = s.stripWhiteSpace();
		$$ = new DomString( s );
		delete $1;
		delete $3;
	}
	;

PredicateList:
	Predicate
	{
		$$ = new QValueList<Predicate *>;
		$$->append( new Predicate( $1 ) );
	}
	|
	PredicateList Predicate
	{
		$$->append( new Predicate( $2 ) );
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
		$$ = new Step( Step::DescendantOrSelfAxis, "node()" );
	}
	;

AbbreviatedStep:
	'.'
	{
		$$ = new Step( Step::SelfAxis, "node()" );
	}
	|
	DOTDOT
	{
		$$ = new Step( Step::ParentAxis, "node()" );
	}
	;

Expr:
	OrExpr
	;

PrimaryExpr:
	VARIABLEREFERENCE
	{
		$$ = new VariableReference( *$1 );
		delete $1;
	}
	|
	'(' Expr ')'
	{
		$$ = $2;
	}
	|
	LITERAL
	{
		$$ = new String( *$1 );
		delete $1;
	}
	|
	NUMBER
	{
		$$ = new Number( $1->toDouble() );
		delete $1;
	}
	|
	FunctionCall
	;

FunctionCall:
	FUNCTIONNAME '(' ')'
	{
		$$ = FunctionLibrary::self().getFunction( $1->ascii() );
		delete $1;
	}
	|
	FUNCTIONNAME '(' ArgumentList ')'
	{
		$$ = FunctionLibrary::self().getFunction( $1->ascii(), *$3 );
		delete $1;
		delete $3
	}
	;

ArgumentList:
	Argument
	{
		$$ = new QValueList<Expression *>;
		$$->append( $1 );
	}
	|
	ArgumentList ',' Argument
	{
		$$->append( $3 );
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
		$$->addSubExpression( $1 );
		$$->addSubExpression( $3 );
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
	|
	FilterExpr DescendantOrSelf RelativeLocationPath
	;

FilterExpr:
	PrimaryExpr
	|
	FilterExpr Predicate
	;

OrExpr:
	AndExpr
	|
	OrExpr OR AndExpr
	{
		$$ = new LogicalOp( LogicalOp::OP_Or, $1, $3 );
	}
	;

AndExpr:
	EqualityExpr
	|
	AndExpr AND EqualityExpr
	{
		$$ = new LogicalOp( LogicalOp::OP_And, $1, $3 );
	}
	;

EqualityExpr:
	RelationalExpr
	|
	EqualityExpr EQOP RelationalExpr
	{
		$$ = new EqTestOp( $2, $1, $3 );
	}
	;

RelationalExpr:
	AdditiveExpr
	|
	RelationalExpr RELOP AdditiveExpr
	{
		$$ = new NumericOp( $2, $1, $3 );
	}
	;

AdditiveExpr:
	MultiplicativeExpr
	|
	AdditiveExpr PLUS MultiplicativeExpr
	{
		$$ = new NumericOp( NumericOp::OP_Add, $1, $3 );
	}
	|
	AdditiveExpr MINUS MultiplicativeExpr
	{
		$$ = new NumericOp( NumericOp::OP_Sub, $1, $3 );
	}
	;

MultiplicativeExpr:
	UnaryExpr
	|
	MultiplicativeExpr MULOP UnaryExpr
	{
		qDebug("%i", $2);
		$$ = new NumericOp( $2, $1, $3 );
	}
	;

UnaryExpr:
	UnionExpr
	|
	MINUS UnaryExpr
	{
		$$ = new Negative;
		$$->addSubExpression( $2 );
	}
	;

%%

Path *parseStatement( const DomString &statement )
{
//	yydebug=1;

	initTokenizer( statement );
	yyparse();
	return _lastPath;
}

