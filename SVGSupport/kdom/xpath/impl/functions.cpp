/*
 * functions.cc - Copyright 2005 Frerich Raabe <raabe@kde.org>
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
#include "functions.h"

#include "NamedAttrMapImpl.h"
#include "NodeImpl.h"
#include "DOMStringImpl.h"

#include <cmath>
using std::isnan;
using std::isinf;

using namespace KDOM;

#define DEFINE_FUNCTION_CREATOR(Class) \
static Function *create##Class() { return new Class; }

class Interval
{
    public:
        static const int Inf =-1;

        Interval();
        Interval( int value );
        Interval( int min, int max );

        bool contains( int value ) const;

        QString asString() const;

    private:
        int m_min;
        int m_max;
};

class FunLast : public Function
{
    public:
        virtual bool isConstant() const;

    private:
        virtual Value doEvaluate() const;
};

class FunPosition : public Function
{
    public:
        virtual bool isConstant() const;

    private:
        virtual Value doEvaluate() const;
};

class FunCount : public Function
{
    public:
        virtual bool isConstant() const;

    private:
        virtual Value doEvaluate() const;
};

class FunLocalName : public Function
{
    public:
        virtual bool isConstant() const;

    private:
        virtual Value doEvaluate() const;
};

class FunNamespaceURI : public Function
{
    public:
        virtual bool isConstant() const;

    private:
        virtual Value doEvaluate() const;
};

class FunName : public Function
{
    public:
        virtual bool isConstant() const;

    private:
        virtual Value doEvaluate() const;
};

class FunString : public Function
{
    private:
        virtual Value doEvaluate() const;
};

class FunConcat : public Function
{
    private:
        virtual Value doEvaluate() const;
};

class FunStartsWith : public Function
{
    private:
        virtual Value doEvaluate() const;
};

class FunContains : public Function
{
    private:
        virtual Value doEvaluate() const;
};

class FunSubstringBefore : public Function
{
    private:
        virtual Value doEvaluate() const;
};

class FunSubstringAfter : public Function
{
    private:
        virtual Value doEvaluate() const;
};

class FunSubstring : public Function
{
    private:
        virtual Value doEvaluate() const;
};

class FunStringLength : public Function
{
    private:
        virtual Value doEvaluate() const;
};

class FunNormalizeSpace : public Function
{
    private:
        virtual Value doEvaluate() const;
};

class FunTranslate : public Function
{
    private:
        virtual Value doEvaluate() const;
};

class FunBoolean : public Function
{
    private:
        virtual Value doEvaluate() const;
};

class FunNot : public Function
{
    private:
        virtual Value doEvaluate() const;
};

class FunTrue : public Function
{
    public:
        virtual bool isConstant() const;

    private:
        virtual Value doEvaluate() const;
};

class FunFalse : public Function
{
    public:
        virtual bool isConstant() const;

    private:
        virtual Value doEvaluate() const;
};

class FunLang : public Function
{
    public:
        virtual bool isConstant() const;

    private:
        virtual Value doEvaluate() const;
};

class FunNumber : public Function
{
    private:
        virtual Value doEvaluate() const;
};

class FunSum : public Function
{
    private:
        virtual Value doEvaluate() const;
};

class FunFloor : public Function
{
    private:
        virtual Value doEvaluate() const;
};

class FunCeiling : public Function
{
    private:
        virtual Value doEvaluate() const;
};

class FunRound : public Function
{
    private:
        virtual Value doEvaluate() const;
};

DEFINE_FUNCTION_CREATOR( FunLast )
DEFINE_FUNCTION_CREATOR( FunPosition )
DEFINE_FUNCTION_CREATOR( FunCount )
DEFINE_FUNCTION_CREATOR( FunLocalName )
DEFINE_FUNCTION_CREATOR( FunNamespaceURI )
DEFINE_FUNCTION_CREATOR( FunName )

DEFINE_FUNCTION_CREATOR( FunString )
DEFINE_FUNCTION_CREATOR( FunConcat )
DEFINE_FUNCTION_CREATOR( FunStartsWith )
DEFINE_FUNCTION_CREATOR( FunContains )
DEFINE_FUNCTION_CREATOR( FunSubstringBefore )
DEFINE_FUNCTION_CREATOR( FunSubstringAfter )
DEFINE_FUNCTION_CREATOR( FunSubstring )
DEFINE_FUNCTION_CREATOR( FunStringLength )
DEFINE_FUNCTION_CREATOR( FunNormalizeSpace )
DEFINE_FUNCTION_CREATOR( FunTranslate )

DEFINE_FUNCTION_CREATOR( FunBoolean )
DEFINE_FUNCTION_CREATOR( FunNot )
DEFINE_FUNCTION_CREATOR( FunTrue )
DEFINE_FUNCTION_CREATOR( FunFalse )
DEFINE_FUNCTION_CREATOR( FunLang )

DEFINE_FUNCTION_CREATOR( FunNumber )
DEFINE_FUNCTION_CREATOR( FunSum )
DEFINE_FUNCTION_CREATOR( FunFloor )
DEFINE_FUNCTION_CREATOR( FunCeiling )
DEFINE_FUNCTION_CREATOR( FunRound )

#undef DEFINE_FUNCTION_CREATOR

Interval::Interval()
    : m_min( Inf ),
    m_max( Inf )
{
}

Interval::Interval( int value )
    : m_min( value ),
    m_max( value )
{
}

Interval::Interval( int min, int max )
    : m_min( min ),
    m_max( max )
{
}

bool Interval::contains( int value ) const
{
    if ( m_min == Inf && m_max == Inf ) {
        return true;
    }

    if ( m_min == Inf ) {
        return value <= m_max;
    }

    if ( m_max == Inf ) {
        return value >= m_min;
    }

    return value >= m_min && value <= m_max;
}

QString Interval::asString() const
{
    QString s = "[";

    if ( m_min == Inf ) {
        s += "-Infinity";
    } else {
        s += QString::number( m_min );
    }

    s += "..";

    if ( m_max == Inf ) {
        s += "Infinity";
    } else {
        s += QString::number( m_max );
    }

    s += "]";

    return s;
}

void Function::setArguments( const Q3ValueList<Expression *> &args )
{
    Q3ValueList<Expression *>::ConstIterator it, end = args.end();
    for ( it = args.begin(); it != end; ++it ) {
        addSubExpression( *it );
    }
}

void Function::setName( const QString &name )
{
    m_name = name;
}

QString Function::dump() const
{
    if ( argCount() == 0 ) {
        return QString( "<function name=\"%1\"/>" ).arg( name() );
    }

    QString s = QString( "<function name=\"%1\">" ).arg( name() );
    for ( unsigned int i = 0; i < argCount(); ++i ) {
        s += "<operand>" + arg( i )->dump() + "</operand>";
    }
    s += "</function>";
    return s;
}


Expression *Function::arg( int i )
{
    return subExpr( i );
}

const Expression *Function::arg( int i ) const
{
    return subExpr( i );
}

unsigned int Function::argCount() const
{
    return subExprCount();
}

QString Function::name() const
{
    return m_name;
}

Value FunLast::doEvaluate() const
{
    return Value( double( Expression::evaluationContext().size ) );
}

bool FunLast::isConstant() const
{
    return false;
}

Value FunPosition::doEvaluate() const
{
    return Value( double( Expression::evaluationContext().position ) );
}

bool FunPosition::isConstant() const
{
    return false;
}

bool FunLocalName::isConstant() const
{
    return false;
}

Value FunLocalName::doEvaluate() const
{
    NodeImpl *node = 0;
    if ( argCount() > 0 ) {
        Value a = arg( 0 )->evaluate();
        if ( a.isNodeset() ) {
            node = a.toNodeset().first();
        }
    }

    if ( !node ) {
        node = evaluationContext().node;
    }

    return Value( node->localName()->string() );
}

bool FunNamespaceURI::isConstant() const
{
    return false;
}

Value FunNamespaceURI::doEvaluate() const
{
    NodeImpl *node = 0;
    if ( argCount() > 0 ) {
        Value a = arg( 0 )->evaluate();
        if ( a.isNodeset() ) {
            node = a.toNodeset().first();
        }
    }

    if ( !node ) {
        node = evaluationContext().node;
    }

    return Value( node->namespaceURI()->string() );
}

bool FunName::isConstant() const
{
    return false;
}

Value FunName::doEvaluate() const
{
    NodeImpl *node = 0;
    if ( argCount() > 0 ) {
        Value a = arg( 0 )->evaluate();
        if ( a.isNodeset() ) {
            node = a.toNodeset().first();
        }
    }

    if ( !node ) {
        node = evaluationContext().node;
    }

    QString s = node->namespaceURI()->string();
    s += ":";
    s += node->localName()->string();

    return Value( s );
}

Value FunCount::doEvaluate() const
{
    Value a = arg( 0 )->evaluate();
    if ( !a.isNodeset() ) {
        qWarning( "count() expects <nodeset>" );
        return Value( 0.0 );
    }
    return Value( double( a.toNodeset().count() ) );
}

bool FunCount::isConstant() const
{
    return false;
}

Value FunString::doEvaluate() const
{
    if ( argCount() == 0 ) {
        DomString s = Value( Expression::evaluationContext().node ).toString();
        return Value( s );
    }
    return Value( arg( 0 )->evaluate().toString() );
}

Value FunConcat::doEvaluate() const
{
    QString str;
    for ( unsigned int i = 0; i < argCount(); ++i ) {
        Value p = arg( i )->evaluate();
        if ( p.isString() ) {
            str.append( p.toString() );
        } else {
            qWarning( "concat() expects <string, string>" );
        }
    }
    return Value( DomString( str ) );
}

Value FunStartsWith::doEvaluate() const
{
    Value arg1 = arg( 0 )->evaluate();
    Value arg2 = arg( 1 )->evaluate();

    if ( !arg1.isString() || !arg2.isString() ) {
        qWarning( "starts-with() expects <string, string>" );
        return Value( "" );
    }

    DomString s1 = arg1.toString();
    DomString s2 = arg2.toString();

    if ( s2.isEmpty() ) {
        return Value( true );
    }

    return Value( s1.startsWith( s2 ) );
}

Value FunContains::doEvaluate() const
{
    Value arg1 = arg( 0 )->evaluate();
    Value arg2 = arg( 1 )->evaluate();

    if ( !arg1.isString() || !arg2.isString() ) {
        qWarning( "contains() expects <string, string>" );
        return Value( "" );
    }

    DomString s1 = arg1.toString();
    DomString s2 = arg2.toString();

    if ( s2.isEmpty() ) {
        return Value( true );
    }

    return Value( s1.find( s2 ) > -1 );
}

Value FunSubstringBefore::doEvaluate() const
{
    Value arg1 = arg( 0 )->evaluate();
    Value arg2 = arg( 1 )->evaluate();

    if ( !arg1.isString() || !arg2.isString() ) {
        qWarning( "substring-before() expects <string, string>" );
        return Value( "" );
    }

    QString s1 = arg1.toString();
    QString s2 = arg2.toString();

    if ( s2.isEmpty() ) {
        return Value( "" );
    }

    int i = s1.find( s2 );
    if ( i == -1 ) {
        return Value( "" );
    }

    return Value( DomString( s1.left( i ) ) );
}

Value FunSubstringAfter::doEvaluate() const
{
    Value arg1 = arg( 0 )->evaluate();
    Value arg2 = arg( 1 )->evaluate();

    if ( !arg1.isString() || !arg2.isString() ) {
        qWarning( "substring-after() expects <string, string>" );
        return Value( "" );
    }

    QString s1 = arg1.toString();
    QString s2 = arg2.toString();

    if ( s2.isEmpty() ) {
        return Value( arg1.toString() );
    }

    int i = s1.find( s2 );
    if ( i == -1 ) {
        return Value( "" );
    }

    return Value( DomString( s1.mid( i + 1 ) ) );
}

Value FunSubstring::doEvaluate() const
{
    Value arg1 = arg( 0 )->evaluate();
    Value arg2 = arg( 1 )->evaluate();
    bool haveLength = false;
    Value arg3;
    if ( argCount() == 3 ) {
        arg3 = arg( 2 )->evaluate();
        haveLength = true;
    }

    if ( haveLength && ( !arg1.isString() || !arg2.isNumber() || !arg3.isNumber() ) ||
         !haveLength && ( !arg1.isString() || !arg2.isNumber() ) ) {
        qWarning( "substring() expects <string, number> or <string, number, number>" );
        return Value( "" );
    }

    QString s = arg1.toString();
    long pos = long( qRound( arg2.toNumber() ) );
    long len = haveLength ? long( qRound( arg3.toNumber() ) ) : -1;

    if ( pos > long( s.length() ) ) {
        return Value( "" );
    }

    if ( haveLength && pos < 1 ) {
        len -= 1 - pos;
        pos = 1;
        if ( len < 1 ) {
            return Value( "" );
        }
    }

    return Value( DomString( s.mid( pos - 1, len ) ) );
}

Value FunStringLength::doEvaluate() const
{
    if ( argCount() == 0 ) {
        DomString s = Value( Expression::evaluationContext().node ).toString();
        return Value( double( s.length() ) );
    }

    Value a = arg( 0 )->evaluate();

    if ( !a.isString() ) {
        qWarning( "string-length() expects <string>" );
        return Value( 0.0 );
    }

    return Value( double( a.toString().length() ) );
}

Value FunNormalizeSpace::doEvaluate() const
{
    if ( argCount() == 0 ) {
        DomString s = Value( Expression::evaluationContext().node ).toString();
        return Value( DomString( s.simplifyWhiteSpace() ) );
    }

    Value a = arg( 0 )->evaluate();

    if ( !a.isString() ) {
        qWarning( "normalize-space() expects <string>" );
        return Value( 0.0 );
    }

    QString s = a.toString();
    s = s.simplifyWhiteSpace();
    return Value( DomString( s ) );
}

Value FunTranslate::doEvaluate() const
{
    Value arg1 = arg( 0 )->evaluate();
    Value arg2 = arg( 1 )->evaluate();
    Value arg3 = arg( 2 )->evaluate();

    if ( !arg1.isString() || !arg2.isString() || !arg3.isString() ) {
        qWarning( "translate() expects <string, string, string>" );
        return Value( "" );
    }

    QString s1 = arg1.toString();
    QString s2 = arg2.toString();
    QString s3 = arg3.toString();
    QString newString;

    for ( unsigned long i1 = 0; i1 < s1.length(); ++i1 ) {
        QChar ch = s1[ i1 ];
        long i2 = s2.find( ch );
        if ( i2 == -1 ) {
            newString += ch;
        } else if ( static_cast<unsigned long>( i2 ) < s3.length() ) {
            newString += s3[ i2 ];
        }
    }

    return Value( DomString( newString ) );
}

Value FunBoolean::doEvaluate() const
{
    return Value( arg( 0 )->evaluate().toBoolean() );
}

Value FunNot::doEvaluate() const
{
    Value a = arg( 0 )->evaluate();

    if ( !a.isBoolean() ) {
        qWarning( "not() expects <boolean>" );
        return Value( true );
    }

    return Value( !a.toBoolean() );
}

Value FunTrue::doEvaluate() const
{
    return Value( true );
}

bool FunTrue::isConstant() const
{
    return true;
}

Value FunLang::doEvaluate() const
{
    Value v = arg( 0 )->evaluate();
    if ( !v.isString() ) {
        qWarning( "lang() expects <string>" );
        return Value( false );
    }
    QString lang = v.toString();

    NodeImpl *langNode = 0;
    NodeImpl *node = evaluationContext().node;

    DOMStringImpl xms( "xms" );
    DOMStringImpl *xmsnsURI = node->lookupNamespaceURI( &xms );

    while ( node ) {
        NamedAttrMapImpl *attrs = node->attributes( true /* r/o */ );
        DOMStringImpl l( "lang" );
        langNode = attrs->getNamedItemNS( xmsnsURI, &l );
        if ( langNode ) {
            break;
        }
        node = node->parentNode();
    }

    if ( !langNode ) {
        return Value( false );
    }

    QString langNodeValue = langNode->nodeValue()->string();

    // extract 'en' out of 'en-us'
    langNodeValue = langNodeValue.left( langNodeValue.find( '-' ) );

    return Value( langNodeValue.lower() == lang.lower() );
}

bool FunLang::isConstant() const
{
    return false;
}

Value FunFalse::doEvaluate() const
{
    return Value( false );
}

bool FunFalse::isConstant() const
{
    return true;
}

Value FunNumber::doEvaluate() const
{
    return Value( arg( 0 )->evaluate().toNumber() );
}

Value FunSum::doEvaluate() const
{
    Value a = arg( 0 )->evaluate();
    if ( !a.isNodeset() ) {
        qWarning( "sum() expects <nodeset>" );
        return Value( 0.0 );
    }

    double sum = 0.0;
    const DomNodeList nodes = a.toNodeset();
    DomNodeList::ConstIterator it, end = nodes.end();
    for ( it = nodes.begin(); it != end; ++it ) {
        sum += Value( stringValue( *it ) ).toNumber();
    }
    return Value( sum );
}

Value FunFloor::doEvaluate() const
{
    Value a = arg( 0 )->evaluate();

    if ( !a.isNumber() ) {
        qWarning( "floor() expects <number>" );
        return Value( 0.0 );
    }

    const double num = a.toNumber();

    if ( isnan( num ) || isinf( num ) ) {
        return Value( num );
    }

    return Value( floor( num ) );
}

Value FunCeiling::doEvaluate() const
{
    Value a = arg( 0 )->evaluate();

    if ( !a.isNumber() ) {
        qWarning( "ceiling() expects <number>" );
        return Value( 0.0 );
    }

    const double num = a.toNumber();

    if ( isnan( num ) || isinf( num ) ) {
        return Value( num );
    }

    return Value( ceil( num ) );
}

Value FunRound::doEvaluate() const
{
    Value a = arg( 0 )->evaluate();

    if ( !a.isNumber() ) {
        qWarning( "round() expects <number>" );
        return Value( 0.0 );
    }

    return Value( (double)qRound( a.toNumber() ) );
}

struct FunctionLibrary::FunctionRec
{
    typedef Function *(*FactoryFn )();

    FunctionRec() {}
    FunctionRec( FactoryFn factoryFn_, Interval args_ )
        : factoryFn( factoryFn_ ),
        args( args_ )
    {
    }

    FactoryFn factoryFn;
    Interval args;
};

FunctionLibrary *FunctionLibrary::s_instance = 0;

FunctionLibrary &FunctionLibrary::self()
{
    if ( !s_instance ) {
        s_instance = new FunctionLibrary;
    }
    return *s_instance;
}

FunctionLibrary::FunctionLibrary()
    : m_functionDict( 32 )
{
    m_functionDict.insert( "last", new FunctionRec( &createFunLast, 0 ) );
    m_functionDict.insert( "position", new FunctionRec( &createFunPosition, 0 ) );
    m_functionDict.insert( "count", new FunctionRec( &createFunCount, 1 ) );
    m_functionDict.insert( "sum", new FunctionRec( &createFunSum, 1 ) );
    m_functionDict.insert( "local-name", new FunctionRec( &createFunLocalName, Interval( 0, 1 ) ) );
    m_functionDict.insert( "namespace-uri", new FunctionRec( &createFunNamespaceURI, Interval( 0, 1 ) ) );
    m_functionDict.insert( "name", new FunctionRec( &createFunName, Interval( 0, 1 ) ) );

    m_functionDict.insert( "string", new FunctionRec( &createFunString, Interval( 0, 1 ) ) );
    m_functionDict.insert( "concat", new FunctionRec( &createFunConcat, Interval( 2, Interval::Inf ) ) );
    m_functionDict.insert( "starts-with", new FunctionRec( &createFunStartsWith, 2 ) );
    m_functionDict.insert( "contains", new FunctionRec( &createFunContains, 2 ) );
    m_functionDict.insert( "substring-before", new FunctionRec( &createFunSubstringBefore, 2 ) );
    m_functionDict.insert( "substring-after", new FunctionRec( &createFunSubstringAfter, 2 ) );
    m_functionDict.insert( "substring", new FunctionRec( &createFunSubstring, Interval( 2, 3 ) ) );
    m_functionDict.insert( "string-length", new FunctionRec( &createFunStringLength, 1 ) );
    m_functionDict.insert( "normalize-space", new FunctionRec( &createFunNormalizeSpace, 1 ) );
    m_functionDict.insert( "translate", new FunctionRec( &createFunTranslate, 3 ) );

    m_functionDict.insert( "boolean", new FunctionRec( &createFunBoolean, 1 ) );
    m_functionDict.insert( "not", new FunctionRec( &createFunNot, 1 ) );
    m_functionDict.insert( "true", new FunctionRec( &createFunTrue, 0 ) );
    m_functionDict.insert( "false", new FunctionRec( &createFunFalse, 0 ) );
    m_functionDict.insert( "lang", new FunctionRec( &createFunLang, 1 ) );

    m_functionDict.insert( "number", new FunctionRec( &createFunNumber, 1 ) );
    m_functionDict.insert( "floor", new FunctionRec( &createFunFloor, 1 ) );
    m_functionDict.insert( "ceiling", new FunctionRec( &createFunCeiling, 1 ) );
    m_functionDict.insert( "round", new FunctionRec( &createFunRound, 1 ) );

    m_functionDict.setAutoDelete( true );
}

Function *FunctionLibrary::getFunction( const char *name,
                                        const Q3ValueList<Expression *> &args ) const
{
    if ( !m_functionDict.find( name ) ) {
        qWarning( "Function '%s' not supported by this implementation.", name );
        return 0;
    }

    FunctionRec functionRec = *m_functionDict[ name ];
    if ( !functionRec.args.contains( args.count() ) ) {
        qWarning( "Function '%s' requires %s arguments, but %d given.", name, functionRec.args.asString().latin1(), args.count() );
        return 0;
    }

    Function *function = functionRec.factoryFn();
    function->setArguments( args );
    function->setName( name );
    return function;
}

class FunctionLibraryDeleter
{
    public:
        ~FunctionLibraryDeleter() {
            delete FunctionLibrary::s_instance;
        }
};

static FunctionLibraryDeleter functionLibraryDeleter;

