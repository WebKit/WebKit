%{

/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2002-2003 Lars Knoll (knoll@kde.org)
 *  Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#include "config.h"

#include "DocumentImpl.h"
#include "css_ruleimpl.h"
#include "css_stylesheetimpl.h"
#include "css_valueimpl.h"
#include "cssparser.h"
#include "PlatformString.h"
#include "htmlnames.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#if SVG_SUPPORT
#include "ksvgcssproperties.h"
#include "ksvgcssvalues.h"
#endif

using namespace WebCore;
using namespace HTMLNames;

//
// The following file defines the function
//     const struct props *findProp(const char *word, int len)
//
// with 'props->id' a CSS property in the range from CSS_PROP_MIN to
// (and including) CSS_PROP_TOTAL-1

#include "cssproperties.c"
#include "cssvalues.c"

namespace WebCore {

int getPropertyID(const char *tagStr, int len)
{
    QString prop;

    if (len && tagStr[0] == '-') {
        prop = QString(tagStr, len);
        if (prop.startsWith("-apple-")) {
            prop = "-khtml-" + prop.mid(7);
            tagStr = prop.ascii();
        } else if (prop.startsWith("-webkit-")) {
            prop = "-khtml-" + prop.mid(8);
            len--;
            tagStr = prop.ascii();
        }
        
        // Honor the use of -khtml-opacity (for Safari 1.1).
        if (prop == "-khtml-opacity") {
            const char * const opacity = "opacity";
            tagStr = opacity;
            len = strlen(opacity);
        }
    }
    
    const struct props *propsPtr = findProp(tagStr, len);
    if (!propsPtr)
        return 0;

    return propsPtr->id;
}

}

static inline int getValueID(const char *tagStr, int len)
{
    QString prop;
    if (len && tagStr[0] == '-') {
        prop = QString(tagStr, len);
        if (prop.startsWith("-apple-")) {
            prop = "-khtml-" + prop.mid(7);
            tagStr = prop.ascii();
        } else if (prop.startsWith("-moz-")) {
            prop = "-khtml-" + prop.mid(5);
            len += 2;
            tagStr = prop.ascii();
        }
    }

    const struct css_value *val = findValue(tagStr, len);
    if (!val)
        return 0;

    return val->id;
}

#define YYDEBUG 0
#define YYPARSE_PARAM parser

%}

%pure_parser

%union {
    CSSRuleImpl *rule;
    CSSSelector *selector;
    bool ok;
    MediaListImpl *mediaList;
    CSSMediaRuleImpl *mediaRule;
    CSSRuleListImpl *ruleList;
    ParseString string;
    float val;
    int prop_id;
    int attribute;
    CSSSelector::Relation relation;
    bool b;
    int i;
    char tok;
    Value value;
    ValueList *valueList;
}

%{

static inline int cssyyerror(const char *) { return 1; }
static int cssyylex(YYSTYPE *yylval) { return CSSParser::current()->lex(yylval); }

%}

//%expect 37

%token WHITESPACE SGML_CD

%token INCLUDES
%token DASHMATCH
%token BEGINSWITH
%token ENDSWITH
%token CONTAINS

%token <string> STRING

%token <string> IDENT

%token <string> HASH
%token ':'
%token '.'
%token '['

%token <string> '*'

%token IMPORT_SYM
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

%token <string> UNICODERANGE

%type <relation> combinator

%type <rule> ruleset
%type <rule> media
%type <rule> import
%type <rule> page
%type <rule> font_face
%type <rule> invalid_rule
%type <rule> invalid_at
%type <rule> invalid_import
%type <rule> rule

%type <string> maybe_ns_prefix

%type <string> namespace_selector

%type <string> string_or_uri
%type <string> ident_or_string
%type <string> medium
%type <string> hexcolor

%type <mediaList> media_list
%type <mediaList> maybe_media_list

%type <ruleList> ruleset_list

%type <prop_id> property

%type <selector> specifier
%type <selector> specifier_list
%type <selector> simple_selector
%type <selector> selector
%type <selector> selector_list
%type <selector> class
%type <selector> attrib
%type <selector> pseudo

%type <ok> declaration_list
%type <ok> decl_list
%type <ok> declaration

%type <b> prio

%type <i> match
%type <i> unary_operator
%type <tok> operator

%type <valueList> expr
%type <value> term
%type <value> unary_term
%type <value> function

%type <string> element_name
%type <string> attr_name

%%

stylesheet:
    maybe_charset maybe_sgml import_list namespace_list rule_list
  | khtml_rule maybe_space
  | khtml_decls maybe_space
  | khtml_value maybe_space
  ;

khtml_rule:
    KHTML_RULE_SYM '{' maybe_space ruleset maybe_space '}' {
        static_cast<CSSParser*>(parser)->rule = $4;
    }
;

khtml_decls:
    KHTML_DECLS_SYM '{' maybe_space declaration_list '}' {
        /* can be empty */
    }
;

khtml_value:
    KHTML_VALUE_SYM '{' maybe_space expr '}' {
        CSSParser *p = static_cast<CSSParser *>(parser);
        if ($4) {
            p->valueList = p->sinkFloatingValueList($4);
            p->parseValue(p->id, p->important);
            delete p->valueList;
            p->valueList = 0;
        }
    }
;

maybe_space:
    /* empty */
  | maybe_space WHITESPACE
  ;

maybe_sgml:
    /* empty */
  | maybe_sgml SGML_CD
  | maybe_sgml WHITESPACE
  ;

maybe_charset:
   /* empty */
  | CHARSET_SYM maybe_space STRING maybe_space ';'
  | CHARSET_SYM error invalid_block
  | CHARSET_SYM error ';'
 ;

import_list:
 /* empty */
 | import_list import maybe_sgml {
     CSSParser* p = static_cast<CSSParser*>(parser);
     if ($2 && p->styleElement && p->styleElement->isCSSStyleSheet())
         p->styleElement->append($2);
 }
 ;

namespace_list:
/* empty */
| namespace_list namespace maybe_sgml
;

rule_list:
   /* empty */
 | rule_list rule maybe_sgml {
     CSSParser* p = static_cast<CSSParser*>(parser);
     if ($2 && p->styleElement && p->styleElement->isCSSStyleSheet())
         p->styleElement->append($2);
 }
 ;

rule:
    ruleset
  | media
  | page
  | font_face
  | invalid_rule
  | invalid_at
  | invalid_import
    ;

import:
    IMPORT_SYM maybe_space string_or_uri maybe_space maybe_media_list ';' {
        $$ = static_cast<CSSParser *>(parser)->createImportRule($3, $5);
    }
  | IMPORT_SYM error invalid_block {
        $$ = 0;
    }
  | IMPORT_SYM error ';' {
        $$ = 0;
    }
  ;

namespace:
NAMESPACE_SYM maybe_space maybe_ns_prefix string_or_uri maybe_space ';' {
    CSSParser *p = static_cast<CSSParser *>(parser);
    if (p->styleElement && p->styleElement->isCSSStyleSheet())
        static_cast<CSSStyleSheetImpl*>(p->styleElement)->addNamespace(p, atomicString($3), atomicString($4));
}
| NAMESPACE_SYM error invalid_block
| NAMESPACE_SYM error ';'
;

maybe_ns_prefix:
/* empty */ { $$.string = 0; }
| IDENT WHITESPACE { $$ = $1; }
;

string_or_uri:
STRING
| URI
;

maybe_media_list:
     /* empty */ {
        $$ = static_cast<CSSParser*>(parser)->createMediaList();
     }
     | media_list
;


media_list:
    /* empty */ {
        $$ = 0;
    }
    | medium {
        $$ = static_cast<CSSParser*>(parser)->createMediaList();
        $$->appendMedium( domString($1).lower() );
    }
    | media_list ',' maybe_space medium {
        $$ = $1;
        if ($$)
            $$->appendMedium( domString($4) );
    }
    | media_list error {
        $$ = 0;
    }
    ;

media:
    MEDIA_SYM maybe_space media_list '{' maybe_space ruleset_list '}' {
        $$ = static_cast<CSSParser*>(parser)->createMediaRule($3, $6);
    }
    ;

ruleset_list:
    /* empty */ { $$ = 0; }
    | ruleset_list ruleset maybe_space {
        $$ = $1;
        if ($2) {
            if (!$$)
                $$ = static_cast<CSSParser*>(parser)->createRuleList();
            $$->append($2);
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
    selector_list '{' maybe_space declaration_list '}' {
        $$ = static_cast<CSSParser*>(parser)->createStyleRule($1);
    }
  ;

selector_list:
    selector {
        $$ = $1;
    }
    | selector_list ',' maybe_space selector {
        if ($1 && $4) {
            CSSParser* p = static_cast<CSSParser*>(parser);
            $$ = $1;
            $$->append(p->sinkFloatingSelector($4));
        } else {
            $$ = 0;
        }
    }
  | selector_list error {
        $$ = 0;
    }
   ;

selector:
    simple_selector {
        $$ = $1;
    }
    | selector combinator simple_selector {
        $$ = $3;
        if (!$1) {
            $$ = 0;
        }
        else if ($$) {
            CSSParser* p = static_cast<CSSParser*>(parser);
            CSSSelector* end = $$;
            while (end->tagHistory)
                end = end->tagHistory;
            end->relation = $2;
            end->tagHistory = p->sinkFloatingSelector($1);
            if ($2 == CSSSelector::Descendant || $2 == CSSSelector::Child) {
                if (WebCore::DocumentImpl* doc = p->document())
                    doc->setUsesDescendantRules(true);
            } else if ($2 == CSSSelector::DirectAdjacent || $2 == CSSSelector::IndirectAdjacent) {
                if (WebCore::DocumentImpl* doc = p->document())
                    doc->setUsesSiblingRules(true);
            }
        }
    }
    | selector error {
        $$ = 0;
    }
    ;

namespace_selector:
    /* empty */ { $$.string = 0; $$.length = 0; }
    | '*' { static unsigned short star = '*'; $$.string = &star; $$.length = 1; }
    | IDENT { $$ = $1; }
;

simple_selector:
    element_name maybe_space {
        CSSParser *p = static_cast<CSSParser *>(parser);
        $$ = p->createFloatingSelector();
        $$->tag = QualifiedName(nullAtom, atomicString($1), p->defaultNamespace);
    }
    | element_name specifier_list maybe_space {
        $$ = $2;
        if ($$) {
            CSSParser *p = static_cast<CSSParser *>(parser);
            $$->tag = QualifiedName(nullAtom, atomicString($1), p->defaultNamespace);
        }
    }
    | specifier_list maybe_space {
        $$ = $1;
        CSSParser *p = static_cast<CSSParser *>(parser);
        if ($$ && p->defaultNamespace != starAtom)
            $$->tag = QualifiedName(nullAtom, starAtom, p->defaultNamespace);
    }
    | namespace_selector '|' element_name maybe_space {
        AtomicString namespacePrefix = atomicString($1);
        CSSParser *p = static_cast<CSSParser *>(parser);
        $$ = p->createFloatingSelector();
        if (p->styleElement && p->styleElement->isCSSStyleSheet())
            $$->tag = QualifiedName(namespacePrefix,
                                    atomicString($3),
                                    static_cast<CSSStyleSheetImpl*>(p->styleElement)->determineNamespace(namespacePrefix));
        else // FIXME: Shouldn't this case be an error?
            $$->tag = QualifiedName(nullAtom, atomicString($3), p->defaultNamespace);
    }
    | namespace_selector '|' element_name specifier_list maybe_space {
        $$ = $4;
        if ($$) {
            AtomicString namespacePrefix = atomicString($1);
            CSSParser *p = static_cast<CSSParser *>(parser);
            if (p->styleElement && p->styleElement->isCSSStyleSheet())
                $$->tag = QualifiedName(namespacePrefix,
                                        atomicString($3),
                                        static_cast<CSSStyleSheetImpl*>(p->styleElement)->determineNamespace(namespacePrefix));
            else // FIXME: Shouldn't this case be an error?
                $$->tag = QualifiedName(nullAtom, atomicString($3), p->defaultNamespace);
        }
    }
    | namespace_selector '|' specifier_list maybe_space {
        $$ = $3;
        if ($$) {
            AtomicString namespacePrefix = atomicString($1);
            CSSParser *p = static_cast<CSSParser *>(parser);
            if (p->styleElement && p->styleElement->isCSSStyleSheet())
                $$->tag = QualifiedName(namespacePrefix,
                                        starAtom,
                                        static_cast<CSSStyleSheetImpl*>(p->styleElement)->determineNamespace(namespacePrefix));
        }
    }
  ;

element_name:
    IDENT {
        ParseString& str = $1;
        CSSParser *p = static_cast<CSSParser *>(parser);
        DOM::DocumentImpl *doc = p->document();
        if (doc && doc->isHTMLDocument())
            str.lower();
        $$ = str;
    }
    | '*' {
        static unsigned short star = '*';
        $$.string = &star;
        $$.length = 1;
    }
  ;

specifier_list:
    specifier {
        $$ = $1;
    }
    | specifier_list specifier {
        $$ = $1;
        if ($$) {
            CSSParser* p = static_cast<CSSParser*>(parser);
            CSSSelector* end = $1;
            while (end->tagHistory)
                end = end->tagHistory;
            end->relation = CSSSelector::SubSelector;
            end->tagHistory = p->sinkFloatingSelector($2);
        }
    }
    | specifier_list error {
        $$ = 0;
    }
;

specifier:
    HASH {
        CSSParser *p = static_cast<CSSParser *>(parser);
        $$ = p->createFloatingSelector();
        $$->match = CSSSelector::Id;
        if (!p->strict)
            $1.lower();
        $$->attr = idAttr;
        $$->value = atomicString($1);
    }
  | class
  | attrib
  | pseudo
    ;

class:
    '.' IDENT {
        CSSParser *p = static_cast<CSSParser *>(parser);
        $$ = p->createFloatingSelector();
        $$->match = CSSSelector::Class;
        if (!p->strict)
            $2.lower();
        $$->attr = classAttr;
        $$->value = atomicString($2);
    }
  ;

attr_name:
    IDENT maybe_space {
        ParseString& str = $1;
        CSSParser *p = static_cast<CSSParser *>(parser);
        DOM::DocumentImpl *doc = p->document();
        if (doc && doc->isHTMLDocument())
            str.lower();
        $$ = str;
    }
    ;

attrib:
    '[' maybe_space attr_name ']' {
        $$ = static_cast<CSSParser*>(parser)->createFloatingSelector();
        $$->attr = QualifiedName(nullAtom, atomicString($3), nullAtom);
        $$->match = CSSSelector::Set;
    }
    | '[' maybe_space attr_name match maybe_space ident_or_string maybe_space ']' {
        $$ = static_cast<CSSParser*>(parser)->createFloatingSelector();
        $$->attr = QualifiedName(nullAtom, atomicString($3), nullAtom);
        $$->match = (CSSSelector::Match)$4;
        $$->value = atomicString($6);
    }
    | '[' maybe_space namespace_selector '|' attr_name ']' {
        AtomicString namespacePrefix = atomicString($3);
        CSSParser *p = static_cast<CSSParser *>(parser);
        $$ = p->createFloatingSelector();
        $$->attr = QualifiedName(namespacePrefix,
                                 atomicString($5),
                                 static_cast<CSSStyleSheetImpl*>(p->styleElement)->determineNamespace(namespacePrefix));
        $$->match = CSSSelector::Set;
    }
    | '[' maybe_space namespace_selector '|' attr_name match maybe_space ident_or_string maybe_space ']' {
        AtomicString namespacePrefix = atomicString($3);
        CSSParser *p = static_cast<CSSParser *>(parser);
        $$ = p->createFloatingSelector();
        $$->attr = QualifiedName(namespacePrefix,
                                 atomicString($5),
                                 static_cast<CSSStyleSheetImpl*>(p->styleElement)->determineNamespace(namespacePrefix));
        $$->match = (CSSSelector::Match)$6;
        $$->value = atomicString($8);
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
        $$ = static_cast<CSSParser*>(parser)->createFloatingSelector();
        $$->match = CSSSelector::PseudoClass;
        $2.lower();
        $$->value = atomicString($2);
        if ($$->value == "empty" || $$->value == "only-child" ||
            $$->value == "first-child" || $$->value == "last-child") {
            CSSParser *p = static_cast<CSSParser *>(parser);
            DOM::DocumentImpl *doc = p->document();
            if (doc)
                doc->setUsesSiblingRules(true);
        }
    }
    | ':' ':' IDENT {
        $$ = static_cast<CSSParser*>(parser)->createFloatingSelector();
        $$->match = CSSSelector::PseudoElement;
        $3.lower();
        $$->value = atomicString($3);
    }
    | ':' FUNCTION maybe_space simple_selector maybe_space ')' {
        CSSParser* p = static_cast<CSSParser*>(parser);
        $$ = p->createFloatingSelector();
        $$->match = CSSSelector::PseudoClass;
        $$->simpleSelector = p->sinkFloatingSelector($4);
        $2.lower();
        $$->value = atomicString($2);
    }
  ;

declaration_list:
    declaration {
        $$ = $1;
    }
    | decl_list declaration {
        $$ = $1;
        if ( $2 )
            $$ = $2;
    }
    | decl_list {
        $$ = $1;
    }
    | error invalid_block_list error {
        $$ = false;
    }
    | error {
        $$ = false;
    }
    ;

decl_list:
    declaration ';' maybe_space {
        $$ = $1;
    }
    | declaration invalid_block_list ';' maybe_space {
        $$ = false;
    }
    | error ';' maybe_space {
        $$ = false;
    }
    | error invalid_block_list error ';' maybe_space {
        $$ = false;
    }
    | decl_list declaration ';' maybe_space {
        $$ = $1;
        if ( $2 )
            $$ = $2;
    }
    | decl_list error ';' maybe_space {
        $$ = $1;
    }
    | decl_list error invalid_block_list error ';' maybe_space {
        $$ = $1;
    }
    ;

declaration:
    property ':' maybe_space expr prio {
        $$ = false;
        CSSParser *p = static_cast<CSSParser *>(parser);
        if ($1 && $4) {
            p->valueList = p->sinkFloatingValueList($4);
            $$ = p->parseValue($1, $5);
            delete p->valueList;
            p->valueList = 0;
        }
    }
    |
    property error {
        $$ = false;
    }
    |
    property ':' maybe_space error expr prio {
        /* The default movable type template has letter-spacing: .none;  Handle this by looking for
        error tokens at the start of an expr, recover the expr and then treat as an error, cleaning
        up and deleting the shifted expr.  */
        $$ = false;
    }
    |
    prio {
        /* Handle this case: div { text-align: center; !important } Just reduce away the stray !important. */
        $$ = false;
    }
    |
    property ':' maybe_space {
        /* div { font-family: } Just reduce away this property with no value. */
        $$ = false;
    }
  ;

property:
    IDENT maybe_space {
        QString str = qString($1);
        $$ = getPropertyID( str.lower().latin1(), str.length() );
#if SVG_SUPPORT
      if ($$ == 0)
          $$ = getSVGCSSPropertyID(str.lower().latin1(), str.length());
#endif
    }
  ;

prio:
    IMPORTANT_SYM maybe_space { $$ = true; }
    | /* empty */ { $$ = false; }
  ;

expr:
    term {
        CSSParser* p = static_cast<CSSParser*>(parser);
        $$ = p->createFloatingValueList();
        $$->addValue(p->sinkFloatingValue($1));
    }
    | expr operator term {
        CSSParser* p = static_cast<CSSParser*>(parser);
        $$ = $1;
        if ($$) {
            if ($2) {
                Value v;
                v.id = 0;
                v.unit = Value::Operator;
                v.iValue = $2;
                $$->addValue(v);
            }
            $$->addValue(p->sinkFloatingValue($3));
        }
    }
    | expr error {
        $$ = 0;
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
  | STRING maybe_space { $$.id = 0; $$.string = $1; $$.unit = CSSPrimitiveValue::CSS_STRING; }
  | IDENT maybe_space {
      QString str = qString( $1 );
      $$.id = getValueID( str.lower().latin1(), str.length() );
#if SVG_SUPPORT
      if ($$.id == 0)
          $$.id = getSVGCSSValueID(str.lower().latin1(), str.length());
#endif
      $$.unit = CSSPrimitiveValue::CSS_IDENT;
      $$.string = $1;
  }
  /* We might need to actually parse the number from a dimension, but we can't just put something that uses $$.string into unary_term. */
  | DIMEN maybe_space { $$.id = 0; $$.string = $1; $$.unit = CSSPrimitiveValue::CSS_DIMENSION }
  | unary_operator DIMEN maybe_space { $$.id = 0; $$.string = $2; $$.unit = CSSPrimitiveValue::CSS_DIMENSION }
  | URI maybe_space { $$.id = 0; $$.string = $1; $$.unit = CSSPrimitiveValue::CSS_URI; }
  | UNICODERANGE maybe_space { $$.id = 0; $$.iValue = 0; $$.unit = CSSPrimitiveValue::CSS_UNKNOWN;/* ### */ }
  | hexcolor { $$.id = 0; $$.string = $1; $$.unit = CSSPrimitiveValue::CSS_RGBCOLOR; }
  | '#' maybe_space { $$.id = 0; $$.string = ParseString(); $$.unit = CSSPrimitiveValue::CSS_RGBCOLOR; } /* Handle error case: "color: #;" */
/* ### according to the specs a function can have a unary_operator in front. I know no case where this makes sense */
  | function {
      $$ = $1;
  }
  ;

unary_term:
  NUMBER maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_NUMBER; }
  | PERCENTAGE maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_PERCENTAGE; }
  | PXS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_PX; }
  | CMS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_CM; }
  | MMS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_MM; }
  | INS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_IN; }
  | PTS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_PT; }
  | PCS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_PC; }
  | DEGS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_DEG; }
  | RADS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_RAD; }
  | GRADS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_GRAD; }
  | MSECS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_MS; }
  | SECS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_S; }
  | HERZ maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_HZ; }
  | KHERZ maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_KHZ; }
  | EMS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_EMS; }
  | QEMS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = Value::Q_EMS; }
  | EXS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_EXS; }
    ;


function:
    FUNCTION maybe_space expr ')' maybe_space {
        CSSParser* p = static_cast<CSSParser*>(parser);
        Function* f = p->createFloatingFunction();
        f->name = $1;
        f->args = p->sinkFloatingValueList($3);
        $$.id = 0;
        $$.unit = Value::Function;
        $$.function = f;
    } |
    FUNCTION maybe_space error {
        CSSParser* p = static_cast<CSSParser*>(parser);
        Function* f = p->createFloatingFunction();
        f->name = $1;
        f->args = 0;
        $$.id = 0;
        $$.unit = Value::Function;
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
    }
  | '@' error ';' {
        $$ = 0;
    }
    ;

invalid_import:
    import {
        $$ = 0;
    }
    ;

invalid_rule:
    error invalid_block {
        $$ = 0;
    }
/*
  Seems like the two rules below are trying too much and violating
  http://www.hixie.ch/tests/evil/mixed/csserrorhandling.html

  | error ';' {
        $$ = 0;
    }
  | error '}' {
        $$ = 0;
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
