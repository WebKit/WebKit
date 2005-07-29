%{

/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2002-2003 Lars Knoll (knoll@kde.org)
 *  Copyright (c) 2003 Apple Computer
 *  Copyright (C) 2003 Dirk Mueller (mueller@kde.org)
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <kdom/DOMString.h>
#include <kdom/impl/domattrs.h>
#include <kdom/impl/CDFInterface.h>
#include <kdom/impl/DocumentImpl.h>
#include <kdom/impl/DOMImplementationImpl.h>

#include <kdom/css/impl/CSSRuleImpl.h>
#include <kdom/css/impl/CSSValueImpl.h>
#include <kdom/css/impl/MediaListImpl.h>
#include <kdom/css/impl/CSSRuleListImpl.h>
#include <kdom/css/impl/CSSMediaRuleImpl.h>
#include <kdom/css/impl/CSSStyleRuleImpl.h>
#include <kdom/css/impl/CSSImportRuleImpl.h>
#include <kdom/css/impl/CSSStyleSheetImpl.h>

#include <kdom/css/impl/KDOMCSSParser.h>

#include <kdebug.h>

// #define CSS_DEBUG

using namespace KDOM;

//
// The following file defines the function
//     const struct props *findProp(const char *word, int len)
//
// with 'props->id' a CSS property in the range from CSS_PROP_MIN to
// (and including) CSS_PROP_TOTAL-1

// turn off inlining to void warning with newer gcc
#undef __inline
#define __inline
#include "kdomcssproperties.c"
#include "kdomcssvalues.c"
#undef __inline


#define YYDEBUG 0
#define YYPARSE_PARAM parser
%}

%pure_parser

%union {
    CSSRuleImpl *rule;
    CSSSelector *selector;
    QPtrList<CSSSelector> *selectorList;
    bool ok;
    MediaListImpl *mediaList;
    CSSMediaRuleImpl *mediaRule;
    CSSRuleListImpl *ruleList;
    KDOMCSSParseString string;
    float val;
    int prop_id;
    unsigned int attribute;
    unsigned int element;
    unsigned int ns;
    CSSSelector::Relation relation;
    CSSSelector::Match match;
    bool b;
    char tok;
    KDOMCSSValue value;
    KDOMCSSValueList *valueList;
}

%{

static inline int kdomparsercssyyerror(const char *x ) 
{
#ifdef CSS_DEBUG
    qDebug( "%s", x );
#else
    Q_UNUSED( x );
#endif
    return 1;
}

static int kdomparsercssyylex( YYSTYPE *yylval ) {
    return CSSParser::current()->lex( yylval );
}

#define null 1

%}

/* APPLE_COMPILE_HACK
 *%destructor { delete $$; $$ = 0; } expr;
 *%destructor { delete $$; $$ = 0; } maybe_media_list media_list;
 *%destructor { delete $$; $$ = 0; } selector_list;
 *%destructor { delete $$; $$ = 0; } ruleset_list;
 *%destructor { delete $$; $$ = 0; } specifier specifier_list simple_selector selector class attrib pseudo;
 */

%no-lines
/* APPLE_COMPILE_HACK
 *%verbose
 */

%left UNIMPORTANT_TOK

%token S SGML_CD

%token INCLUDES
%token DASHMATCH
%token BEGINSWITH
%token ENDSWITH
%token CONTAINS

%token <string> STRING

%right <string> IDENT

%token <string> NTH

%nonassoc <string> HASH
%nonassoc ':'
%nonassoc '.'
%nonassoc '['
%nonassoc '*'
%nonassoc error
%left '|'

%left IMPORT_SYM
%token PAGE_SYM
%token MEDIA_SYM
%token FONT_FACE_SYM
%token CHARSET_SYM
%token NAMESPACE_SYM
%token KHTML_RULE_SYM
%token KHTML_DECLS_SYM
%token KHTML_VALUE_SYM

%token IMPORTANT_SYM

%token <val> QEMS
%token <val> EMS
%token <val> EXS
%token <val> PXS
%token <val> CMS
%token <val> MMS
%token <val> INS
%token <val> PTS
%token <val> PCS
%token <val> DEGS
%token <val> RADS
%token <val> GRADS
%token <val> MSECS
%token <val> SECS
%token <val> HERZ
%token <val> KHERZ
%token <string> DIMEN
%token <val> PERCENTAGE
%token <val> NUMBER

%token <string> URI
%token <string> FUNCTION
%token <string> NOTFUNCTION

%token <string> UNICODERANGE

%type <relation> combinator

%type <rule> ruleset
%type <rule> media
%type <rule> import
%type <rule> page
%type <rule> font_face
%type <rule> invalid_rule
%type <rule> invalid_at
%type <rule> rule

%type <string> namespace_selector

%type <string> string_or_uri
%type <string> ident_or_string
%type <string> medium
%type <string> hexcolor
%type <string> maybe_ns_prefix

%type <mediaList> media_list
%type <mediaList> maybe_media_list

%type <ruleList> ruleset_list

%type <prop_id> property

%type <selector> specifier
%type <selector> specifier_list
%type <selector> simple_selector
%type <selector> selector
%type <selectorList> selector_list
%type <selector> class
%type <selector> attrib
%type <selector> pseudo

%type <ok> declaration_block
%type <ok> declaration_list
%type <ok> declaration

%type <b> prio

%type <match> match
%type <val> unary_operator
%type <tok> operator

%type <valueList> expr
%type <value> term
%type <value> unary_term
%type <value> function

%type <element> element_name

%type <attribute> attrib_id

%%

stylesheet:
    maybe_charset maybe_sgml import_list maybe_namespace rule_list
  | khtml_rule maybe_space
  | khtml_decls maybe_space
  | khtml_value maybe_space
  ;

khtml_rule:
    KHTML_RULE_SYM '{' maybe_space ruleset maybe_space '}' {
        CSSParser *p = static_cast<CSSParser *>(parser);
	p->rule = $4;
    }
;

khtml_decls:
    KHTML_DECLS_SYM declaration_block {
	/* can be empty */
    }
;

khtml_value:
    KHTML_VALUE_SYM '{' maybe_space expr '}' {
	CSSParser *p = static_cast<CSSParser *>(parser);
	if ( $4 ) {
	    p->valueList = $4;
#ifdef CSS_DEBUG
	    kdDebug( 6080 ) << "   got property for " << p->id <<
		(p->important?" important":"")<< endl;
	    bool ok =
#endif
		p->parseValue( p->id, p->important );
#ifdef CSS_DEBUG
	    if ( !ok )
		kdDebug( 6080 ) << "     couldn't parse value!" << endl;
#endif
	}
#ifdef CSS_DEBUG
	else
	    kdDebug( 6080 ) << "     no value found!" << endl;
#endif
	delete p->valueList;
	p->valueList = 0;
    }
;

maybe_space:
    /* empty */ %prec UNIMPORTANT_TOK
  | maybe_space S
  ;

maybe_sgml:
    /* empty */
  | maybe_sgml SGML_CD
  | maybe_sgml S
  ;

maybe_charset:
   /* empty */
 | CHARSET_SYM maybe_space STRING maybe_space ';' {
#ifdef CSS_DEBUG
     kdDebug( 6080 ) << "charset rule: " << qString($3) << endl;
#endif
 }
  | CHARSET_SYM error invalid_block
  | CHARSET_SYM error ';'
 ;

import_list:
 /* empty */
 | import_list import maybe_sgml {
     CSSParser *p = static_cast<CSSParser *>(parser);
     if ( $2 && p->styleElement && p->styleElement->isCSSStyleSheet() ) {
	 p->styleElement->append( $2 );
     } else {
	 delete $2;
     }
 }
 ;

import:
    IMPORT_SYM maybe_space string_or_uri maybe_space maybe_media_list ';' {
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "@import: " << qString($3) << endl;
#endif
	CSSParser *p = static_cast<CSSParser *>(parser);
	if ( $5 && p->styleElement && p->styleElement->isCSSStyleSheet() )
	    $$ = new CSSImportRuleImpl( p->styleElement, domString($3), $5 );
	else
	    $$ = 0;
    }
  | IMPORT_SYM error invalid_block {
        $$ = 0;
    }
  | IMPORT_SYM error ';' {
        $$ = 0;
    }
  ;

maybe_namespace
  : /* empty */ %prec UNIMPORTANT_TOK
  | namespace maybe_sgml
;

namespace:
NAMESPACE_SYM maybe_space maybe_ns_prefix string_or_uri maybe_space ';' {
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "@namespace: " << qString($4) << endl;
#endif
      CSSParser *p = static_cast<CSSParser *>(parser);
    if (p->styleElement && p->styleElement->isCSSStyleSheet())
        static_cast<CSSStyleSheetImpl*>(p->styleElement)->addNamespace(p, domString($3), domString($4));
 }
| NAMESPACE_SYM error invalid_block
| NAMESPACE_SYM error ';'
    ;

maybe_ns_prefix:
/* empty */ { $$.string = 0; }
| IDENT S { $$ = $1; }
  ;

rule_list:
   /* empty */
 | rule_list rule maybe_sgml {
     CSSParser *p = static_cast<CSSParser *>(parser);
     if ( $2 && p->styleElement && p->styleElement->isCSSStyleSheet() ) {
	 p->styleElement->append( $2 );
     } else {
	 delete $2;
     }
 }
    ;

rule:
    ruleset
  | media
  | page
  | font_face
  | invalid_rule
  | invalid_at
  | import error { delete $1; $$ = 0; }
    ;

string_or_uri:
    STRING
  | URI
    ;

maybe_media_list:
    /* empty */ {
	$$ = new MediaListImpl();
    }
    | media_list
;


media_list:
    medium {
	$$ = new MediaListImpl();
	$$->appendMedium( domString($1).lower() );
    }
    | media_list ',' maybe_space medium {
	$$ = $1;
	if ($$)
	    $$->appendMedium( domString($4).lower() );
    }
    | media_list error {
       delete $1;
       $$ = 0;
    }
    ;

media:
    MEDIA_SYM maybe_space media_list '{' maybe_space ruleset_list '}' {
	CSSParser *p = static_cast<CSSParser *>(parser);
	if ( $3 && $6 &&
	     p->styleElement && p->styleElement->isCSSStyleSheet() ) {
	    $$ = new CSSMediaRuleImpl( static_cast<CSSStyleSheetImpl*>(p->styleElement), $3, $6 );
	} else {
	    $$ = 0;
	    delete $3;
	    delete $6;
	}
    }
  ;

ruleset_list:
    /* empty */ { $$ = 0; }
  | ruleset_list ruleset maybe_space {
      $$ = $1;
      if ( $2 ) {
	  if ( !$$ ) $$ = new CSSRuleListImpl();
	  $$->append( $2 );
      }
  }
    ;

medium:
  IDENT maybe_space {
      $$ = $1;
  }
  ;

/*
page:
    PAGE_SYM maybe_space IDENT? pseudo_page? maybe_space
    '{' maybe_space declaration [ ';' maybe_space declaration ]* '}' maybe_space
  ;

pseudo_page
  : ':' IDENT
  ;

font_face
  : FONT_FACE_SYM maybe_space
    '{' maybe_space declaration [ ';' maybe_space declaration ]* '}' maybe_space
  ;
*/

page:
    PAGE_SYM error invalid_block {
      $$ = 0;
    }
  | PAGE_SYM error ';' {
      $$ = 0;
    }
    ;

font_face:
    FONT_FACE_SYM error invalid_block {
      $$ = 0;
    }
  | FONT_FACE_SYM error ';' {
      $$ = 0;
    }
;

combinator:
    '+' maybe_space { $$ = CSSSelector::DirectAdjacent; }
  | '~' maybe_space { $$ = CSSSelector::IndirectAdjacent; }
  | '>' maybe_space { $$ = CSSSelector::Child; }
  | /* empty */ { $$ = CSSSelector::Descendant; }
  ;

unary_operator:
    '-' { $$ = -1; }
  | '+' { $$ = 1; }
  ;

ruleset:
    selector_list declaration_block {
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "got ruleset" << endl << "  selector:" << endl;
#endif
	CSSParser *p = static_cast<CSSParser *>(parser);
	if ( $1 && $2 && p->numParsedProperties ) {
	    CSSStyleRuleImpl *rule = new CSSStyleRuleImpl( p->styleElement );
	    CSSStyleDeclarationImpl *decl = p->createStyleDeclaration( rule );
	    rule->setSelector( $1 );
	    rule->setDeclaration(decl);
	    $$ = rule;
	} else {
	    $$ = 0;
	    delete $1;
	    p->clearProperties();
	}
    }
  ;

selector_list:
    selector %prec UNIMPORTANT_TOK {
	if ( $1 ) {
	    $$ = new QPtrList<CSSSelector>;
            $$->setAutoDelete( true );
#ifdef CSS_DEBUG
	    kdDebug( 6080 ) << "   got simple selector:" << endl;
	    $1->print();
#endif
	    $$->append( $1 );
	} else {
	    $$ = 0;
	}
    }
    | selector_list ',' maybe_space selector %prec UNIMPORTANT_TOK {
	if ( $1 && $4 ) {
	    $$ = $1;
	    $$->append( $4 );
#ifdef CSS_DEBUG
	    kdDebug( 6080 ) << "   got simple selector:" << endl;
	    $4->print();
#endif
	} else {
	    delete $1;
	    delete $4;
	    $$ = 0;
	}
    }
  | selector_list error {
	delete $1;
	$$ = 0;
    }
   ;

selector:
    simple_selector {
	$$ = $1;
    }
    | selector combinator simple_selector {
	if ( !$1 || !$3 ) {
	    delete $1;
	    delete $3;
	    $$ = 0;
	} else {
	    $$ = $3;
	    CSSSelector *end = $3;
	    while( end->tagHistory )
			end = end->tagHistory;
	    end->relation = $2;
	    end->tagHistory = $1;
		if ( $2 == CSSSelector::Descendant ||
         $2 == CSSSelector::Child ) {
		CSSParser *p = static_cast<CSSParser *>(parser);
		DocumentImpl *doc = (p ? p->document() : 0);
		if(doc)
			doc->setUsesDescendantRules(true);
		}
	}
    }
    | selector error {
	delete $1;
	$$ = 0;
    }
    ;

namespace_selector:
    /* empty */ '|' { $$.string = 0; $$.length = 0; }
    | '*' '|' { static unsigned short star = '*'; $$.string = &star; $$.length = 1; }
    | IDENT '|' { $$ = $1; }
;

simple_selector:
    element_name maybe_space {
	CSSParser *p = static_cast<CSSParser *>(parser);
    $$ = new CSSSelector(p->interface());
    $$->tag = $1;
    }
    | element_name specifier_list maybe_space {
    $$ = $2;
        if ( $$ )
        $$->tag = $1;
    }
    | specifier_list maybe_space {
    $$ = $1;
        if ( $$ ) {
			CSSParser *p = static_cast<CSSParser *>(parser);
            $$->tag = makeId(p->defaultNamespace, anyLocalName);
		}
    }
    | namespace_selector element_name maybe_space {
		CSSParser *p = static_cast<CSSParser *>(parser);
        $$ = new CSSSelector(p->interface());
        $$->tag = $2;
        if (p->styleElement && p->styleElement->isCSSStyleSheet())
            static_cast<CSSStyleSheetImpl*>(p->styleElement)->determineNamespace($$->tag, domString($1));
    }
    | namespace_selector element_name specifier_list maybe_space {
        $$ = $3;
        if ($$) {
            $$->tag = $2;
			CSSParser *p = static_cast<CSSParser *>(parser);
            if(p->styleElement && p->styleElement->isCSSStyleSheet())
                static_cast<CSSStyleSheetImpl*>(p->styleElement)->determineNamespace($$->tag, domString($1));
        }
    }
    | namespace_selector specifier_list maybe_space {
        $$ = $2;
        if ($$) {
            $$->tag = makeId(anyNamespace, anyLocalName);
			CSSParser *p = static_cast<CSSParser *>(parser);
            if(p->styleElement && p->styleElement->isCSSStyleSheet())
                static_cast<CSSStyleSheetImpl*>(p->styleElement)->determineNamespace($$->tag, domString($1));
        }
    }
  ;

element_name:
    IDENT {
	CSSParser *p = static_cast<CSSParser *>(parser);
	DocumentImpl *doc = p->document();
	QString tag = qString($1);
	if ( doc ) {
	/* FIXME  khtml2 relevant...
		if (doc->isHTMLDocument())
			tag = tag.lower();
	*/

		const DOMString dtag(tag);
		$$ = makeId(p->defaultNamespace, doc->getId(NodeImpl::ElementId, dtag.implementation(), false /* true */));
	} else {
		$$ = makeId(p->defaultNamespace, p->interface()->getTagID(tag.lower().ascii(), tag.length()));
		// this case should never happen - only when loading
		// the default stylesheet - which must not contain unknown tags
		// assert($$ != 0);
	}
	}
	| '*' {
		CSSParser *p = static_cast<CSSParser *>(parser);
		$$ = makeId(p->defaultNamespace, anyLocalName);
	}
 ;

specifier_list:
    specifier {
	$$ = $1;
	$$->nonCSSHint = static_cast<CSSParser *>(parser)->nonCSSHint;
    }
    | specifier_list specifier {
	$$ = $1;
	if ( $$ ) {
            CSSSelector *end = $1;
            while( end->tagHistory )
                end = end->tagHistory;
            end->relation = CSSSelector::SubSelector;
            end->tagHistory = $2;
	}
    }
    | specifier_list error {
	delete $1;
	$$ = 0;
    }
;

specifier:
    HASH {
	CSSParser *p = static_cast<CSSParser *>(parser);
	$$ = new CSSSelector(p->interface());
	$$->match = CSSSelector::Id;
	$$->attr = ATTR_ID;
	$$->value = domString($1);
    }
  | class
  | attrib
  | pseudo
    ;

class:
    '.' IDENT {
	CSSParser *p = static_cast<CSSParser *>(parser);
	$$ = new CSSSelector(p->interface());
	$$->match = CSSSelector::List;
	$$->attr = ATTR_CLASS;
	$$->value = domString($2);
    }
  ;

attrib_id:
    IDENT maybe_space {
	CSSParser *p = static_cast<CSSParser *>(parser);
	DocumentImpl *doc = p->document();

	QString attr = qString($1);
	if(doc)
	{
	/* FIXME  khtml2 relevant...
		if (doc->isHTMLDocument())
			attr = attr.lower();
	*/
		
	    const DOMString dattr(attr);
		$$ = doc->getId(NodeImpl::AttributeId, dattr.implementation(), false);
	} else {
		$$ = doc->implementation()->cdfInterface()->getAttrID(attr.lower().ascii(), attr.length());
		// this case should never happen - only when loading
		// the default stylesheet - which must not contain unknown attributes
		assert($$ != 0);
		}
	}
    ;

attrib:
    '[' maybe_space attrib_id ']' {
	CSSParser *p = static_cast<CSSParser *>(parser);
	$$ = new CSSSelector(p->interface());
	$$->attr = $3;
	$$->match = CSSSelector::Set;
    }
    | '[' maybe_space attrib_id match maybe_space ident_or_string maybe_space ']' {
	CSSParser *p = static_cast<CSSParser *>(parser);
	$$ = new CSSSelector(p->interface());
	$$->attr = $3;
	$$->match = $4;
	$$->value = domString($6);
    }
    | '[' maybe_space namespace_selector '|' attrib_id ']' {
		CSSParser *p = static_cast<CSSParser *>(parser);
        $$ = new CSSSelector(p->interface());
        $$->attr = $5;
        $$->match = CSSSelector::Set;
        if (p->styleElement && p->styleElement->isCSSStyleSheet())
            static_cast<CSSStyleSheetImpl*>(p->styleElement)->determineNamespace($$->attr, domString($3));
    }
    | '[' maybe_space namespace_selector '|' attrib_id match maybe_space ident_or_string maybe_space ']' {
		CSSParser *p = static_cast<CSSParser *>(parser);
        $$ = new CSSSelector(p->interface());
        $$->attr = $5;
        $$->match = (CSSSelector::Match)$6;
        $$->value = domString($8);
        if (p->styleElement && p->styleElement->isCSSStyleSheet())
            static_cast<CSSStyleSheetImpl*>(p->styleElement)->determineNamespace($$->attr, domString($3));
   }
  ;

match:
    '=' {
	$$ = CSSSelector::Exact;
    }
    | INCLUDES {
	$$ = CSSSelector::List;
    }
    | DASHMATCH {
	$$ = CSSSelector::Hyphen;
    }
    | BEGINSWITH {
	$$ = CSSSelector::Begin;
    }
    | ENDSWITH {
	$$ = CSSSelector::End;
    }
    | CONTAINS {
	$$ = CSSSelector::Contain;
    }
    ;

ident_or_string:
    IDENT
  | STRING
    ;

pseudo:
    ':' IDENT {
	CSSParser *p = static_cast<CSSParser *>(parser);
	$$ = new CSSSelector(p->interface());
	$$->match = CSSSelector::PseudoClass;
	$$->value = domString($2);
    }
    |
    ':' ':' IDENT {
	CSSParser *p = static_cast<CSSParser *>(parser);
	$$ = new CSSSelector(p->interface());
	$$->match = CSSSelector::PseudoElement;
        $$->value = domString($3);
    }
    // used by :nth-*
    | ':' FUNCTION NTH ')' {
		CSSParser *p = static_cast<CSSParser *>(parser);
        $$ = new CSSSelector(p->interface());
        $$->match = CSSSelector::PseudoClass;
        $$->string_arg = domString($3);
        $$->value = domString($2);
    }
    // used by :nth-*
    | ':' FUNCTION NUMBER ')' {
		CSSParser *p = static_cast<CSSParser *>(parser);
        $$ = new CSSSelector(p->interface());
        $$->match = CSSSelector::PseudoClass;
        $$->string_arg = QString::number($3);
        $$->value = domString($2);
    }
    // used by :nth-* and :lang
    | ':' FUNCTION IDENT ')' {
		CSSParser *p = static_cast<CSSParser *>(parser);
        $$ = new CSSSelector(p->interface());
        $$->match = CSSSelector::PseudoClass;
        $$->string_arg = domString($3);
        $$->value = domString($2);
    }
    // used by :contains
    | ':' FUNCTION STRING ')' {
		CSSParser *p = static_cast<CSSParser *>(parser);
        $$ = new CSSSelector(p->interface());
        $$->match = CSSSelector::PseudoClass;
        $$->string_arg = domString($3);
        $$->value = domString($2);
    }
    // used only by :not
    | ':' NOTFUNCTION maybe_space simple_selector ')' {
		CSSParser *p = static_cast<CSSParser *>(parser);
        $$ = new CSSSelector(p->interface());
        $$->match = CSSSelector::PseudoClass;
        $$->simpleSelector = $4;
        $$->value = domString($2);
    }
  ;

declaration_block:
    '{' maybe_space declaration '}' {
	$$ = $3;
    }
    | '{' maybe_space error '}' {
	$$ = false;
    }
    | '{' maybe_space declaration_list '}' {
	$$ = $3;
    }
    | '{' maybe_space declaration_list declaration '}' {
	$$ = $3;
	if ( $4 )
	    $$ = $4;
    }
    | '{' maybe_space declaration_list error '}' {
	$$ = $3;
    }
    ;

declaration_list:
    declaration ';' maybe_space {
	$$ = $1;
    }
    |
    error ';' maybe_space {
        $$ = false;
    }
    | declaration_list declaration ';' maybe_space {
	$$ = $1;
	if ( $2 )
	    $$ = $2;
    }
    | declaration_list error ';' maybe_space {
        $$ = $1;
    }
    ;

declaration:
    property ':' maybe_space expr prio {
	$$ = false;
	CSSParser *p = static_cast<CSSParser *>(parser);
	if ( $1 && $4 ) {
	    p->valueList = $4;
#ifdef CSS_DEBUG
	    kdDebug( 6080 ) << "   got property: " << $1 <<
		($5?" important":"")<< endl;
#endif
	        bool ok = p->parseValue( $1, $5 );
                if ( ok )
		    $$ = ok;
#ifdef CSS_DEBUG
	        else
		    kdDebug( 6080 ) << "     couldn't parse value!" << endl;
#endif
	} else {
            delete $4;
        }
	delete p->valueList;
	p->valueList = 0;
    }
    | error invalid_block {
        $$ = false;
    }
  ;

property:
    IDENT maybe_space {
	CSSParser *p = static_cast<CSSParser *>(parser);
	CDFInterface *i = p->interface();
	QString str = qString($1);
	$$ = (i ? i->getPropertyID( str.lower().latin1(), str.length() ) : 0);
    }
  ;

prio:
    IMPORTANT_SYM maybe_space { $$ = true; }
    | /* empty */ { $$ = false; }
  ;

expr:
    term {
	$$ = new KDOMCSSValueList;
	$$->addValue( $1 );
    }
    | expr operator term {
	$$ = $1;
	if ( $$ ) {
	    if ( $2 ) {
		KDOMCSSValue v;
		v.id = 0;
		v.unit = KDOMCSSValue::Operator;
		v.iValue = $2;
		$$->addValue( v );
	    }
	    $$->addValue( $3 );
	}
    }
  ;

operator:
    '/' maybe_space {
	$$ = '/';
    }
  | ',' maybe_space {
	$$ = ',';
    }
  | /* empty */ {
        $$ = 0;
  }
  ;

term:
  unary_term { $$ = $1; }
   | unary_operator unary_term { $$ = $2; $$.fValue *= $1; }
  /* DIMEN is an unary_term, but since we store the string we must not modify fValue */
  | DIMEN maybe_space { $$.id = 0; $$.string = $1; $$.unit = CSS_DIMENSION; }
  | STRING maybe_space { $$.id = 0; $$.string = $1; $$.unit = CSS_STRING; }
  | IDENT maybe_space {
      CSSParser *p = static_cast<CSSParser *>(parser);
	  CDFInterface *i = p->interface();
      QString str = qString( $1 );
	  $$.id = (i ? i->getValueID( str.latin1(), str.length() ) : 0);
      $$.unit = CSS_IDENT;
      $$.string = $1;
  }
  | URI maybe_space { $$.id = 0; $$.string = $1; $$.unit = CSS_URI; }
  | UNICODERANGE maybe_space { $$.id = 0; $$.iValue = 0; $$.unit = CSS_UNKNOWN;/* ### */ }
  | hexcolor { $$.id = 0; $$.string = $1; $$.unit = CSS_RGBCOLOR; }
/* ### according to the specs a function can have a unary_operator in front. I know no case where this makes sense */
  | function {
      $$ = $1;
  }
  ;

unary_term:
  NUMBER maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSS_NUMBER; }
  | PERCENTAGE maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSS_PERCENTAGE; }
  | PXS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSS_PX; }
  | CMS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSS_CM; }
  | MMS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSS_MM; }
  | INS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSS_IN; }
  | PTS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSS_PT; }
  | PCS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSS_PC; }
  | DEGS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSS_DEG; }
  | RADS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSS_RAD; }
  | GRADS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSS_GRAD; }
  | MSECS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSS_MS; }
  | SECS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSS_S; }
  | HERZ maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSS_HZ; }
  | KHERZ maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSS_KHZ; }
  | EMS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSS_EMS; }
  | QEMS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = KDOMCSSValue::Q_EMS; }
  | EXS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSS_EXS; }
    ;


function:
  FUNCTION maybe_space expr ')' maybe_space {
      KDOMCSSFunction *f = new KDOMCSSFunction;
      f->name = $1;
      f->args = $3;
      $$.id = 0;
      $$.unit = KDOMCSSValue::Function;
      $$.function = f;
  }
  | FUNCTION maybe_space error {
      KDOMCSSFunction *f = new KDOMCSSFunction;
      f->name = $1;
      f->args = 0;
      $$.id = 0;
      $$.unit = KDOMCSSValue::Function;
      $$.function = f;
  }

  ;
/*
 * There is a constraint on the color that it must
 * have either 3 or 6 hex-digits (i.e., [0-9a-fA-F])
 * after the "#"; e.g., "#000" is OK, but "#abcd" is not.
 */
hexcolor:
  HASH maybe_space { $$ = $1; }
  ;


/* error handling rules */

invalid_at:
    '@' error invalid_block {
	$$ = 0;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipped invalid @-rule" << endl;
#endif
    }
  | '@' error ';' {
	$$ = 0;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipped invalid @-rule" << endl;
#endif
    }
    ;

invalid_rule:
    error invalid_block {
	$$ = 0;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipped invalid rule" << endl;
#endif
    }
/*
  Seems like the two rules below are trying too much and violating
  http://www.hixie.ch/tests/evil/mixed/csserrorhandling.html

  | error ';' {
	$$ = 0;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipped invalid rule" << endl;
#endif
    }
  | error '}' {
	$$ = 0;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipped invalid rule" << endl;
#endif
    }
*/
    ;

invalid_block:
    '{' error invalid_block_list error '}'
  | '{' error '}'
    ;

invalid_block_list:
    invalid_block
  | invalid_block_list error invalid_block
;

%%

