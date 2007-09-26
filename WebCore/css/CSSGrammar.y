%{

/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2002-2003 Lars Knoll (knoll@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007 Apple Inc.
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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

#include "CSSMediaRule.h"
#include "CSSParser.h"
#include "CSSPrimitiveValue.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "CSSSelector.h"
#include "CSSStyleSheet.h"
#include "Document.h"
#include "HTMLNames.h"
#include "MediaList.h"
#include "MediaQuery.h"
#include "MediaQueryExp.h"
#include "PlatformString.h"
#include <stdlib.h>
#include <string.h>

#if ENABLE(SVG)
#include "ksvgcssproperties.h"
#include "ksvgcssvalues.h"
#endif

using namespace WebCore;
using namespace HTMLNames;

// The following file defines the function
//     const struct props *findProp(const char *word, int len)
//
// with 'props->id' a CSS property in the range from CSS_PROP_MIN to
// (and including) CSS_PROP_TOTAL-1

#include "CSSPropertyNames.c"
#include "CSSValueKeywords.c"

namespace WebCore {

int getPropertyID(const char* tagStr, int len)
{
    DeprecatedString prop;

    if (len && tagStr[0] == '-') {
        prop = DeprecatedString(tagStr, len);
        if (prop.startsWith("-apple-")) {
            prop = "-webkit-" + prop.mid(7);
            tagStr = prop.ascii();
            len++;
        } else if (prop.startsWith("-khtml-")) {
            prop = "-webkit-" + prop.mid(7);
            len++;
            tagStr = prop.ascii();
        }

        // Honor the use of old-style opacity (for Safari 1.1).
        if (prop == "-webkit-opacity") {
            const char * const opacity = "opacity";
            tagStr = opacity;
            len = strlen(opacity);
        }
    }

    const struct props* propsPtr = findProp(tagStr, len);
    if (!propsPtr)
        return 0;

    return propsPtr->id;
}

} // namespace WebCore

static inline int getValueID(const char* tagStr, int len)
{
    DeprecatedString prop;
    if (len && tagStr[0] == '-') {
        prop = DeprecatedString(tagStr, len);
        if (prop.startsWith("-apple-")) {
            prop = "-webkit-" + prop.mid(7);
            tagStr = prop.ascii();
            len++;
        } else if (prop.startsWith("-khtml-")) {
            prop = "-webkit-" + prop.mid(7);
            len++;
            tagStr = prop.ascii();
        }
    }

    const struct css_value* val = findValue(tagStr, len);
    if (!val)
        return 0;

    return val->id;
}

#define YYENABLE_NLS 0
#define YYLTYPE_IS_TRIVIAL 1
#define YYMAXDEPTH 10000
#define YYDEBUG 0
#define YYPARSE_PARAM parser

%}

%pure_parser

%union {
    CSSRule* rule;
    CSSSelector* selector;
    bool ok;
    MediaList *mediaList;
    CSSMediaRule* mediaRule;
    CSSRuleList* ruleList;
    ParseString string;
    float val;
    int prop_id;
    int attribute;
    CSSSelector::Relation relation;
    bool b;
    int i;
    char tok;
    Value value;
    ValueList* valueList;

    MediaQuery* mediaQuery;
    MediaQueryExp* mediaQueryExp;
    Vector<MediaQueryExp*>* mediaQueryExpList;
    MediaQuery::Restrictor mediaQueryRestrictor;
}

%{

static inline int cssyyerror(const char*) { return 1; }
static int cssyylex(YYSTYPE* yylval) { return CSSParser::current()->lex(yylval); }

%}

//%expect 37

%left UNIMPORTANT_TOK

%token WHITESPACE SGML_CD

%token INCLUDES
%token DASHMATCH
%token BEGINSWITH
%token ENDSWITH
%token CONTAINS

%token <string> STRING

%right <string> IDENT

%nonassoc <string> HEX
%nonassoc <string> IDSEL
%nonassoc ':'
%nonassoc '.'
%nonassoc '['
%nonassoc <string> '*'
%nonassoc error
%left '|'

%token IMPORT_SYM
%token PAGE_SYM
%token MEDIA_SYM
%token FONT_FACE_SYM
%token CHARSET_SYM
%token NAMESPACE_SYM
%token WEBKIT_RULE_SYM
%token WEBKIT_DECLS_SYM
%token WEBKIT_VALUE_SYM
%token WEBKIT_MEDIAQUERY_SYM

%token IMPORTANT_SYM
%token MEDIA_ONLY
%token MEDIA_NOT
%token MEDIA_AND

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
%token <val> FLOAT
%token <val> INTEGER

%token <string> URI
%token <string> FUNCTION
%token <string> NOTFUNCTION

%token <string> UNICODERANGE

%type <relation> combinator

%type <rule> charset
%type <rule> ruleset
%type <rule> ruleset_or_import
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

%type <string> media_feature
%type <mediaList> media_list
%type <mediaList> maybe_media_list
%type <mediaQuery> media_query
%type <mediaQueryRestrictor> maybe_media_restrictor
%type <valueList> maybe_media_value
%type <mediaQueryExp> media_query_exp
%type <mediaQueryExpList> media_query_exp_list
%type <mediaQueryExpList> maybe_media_query_exp_list

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
  | webkit_rule maybe_space
  | webkit_decls maybe_space
  | webkit_value maybe_space
  | webkit_mediaquery maybe_space
  ;

ruleset_or_import:
   ruleset |
   import
;

webkit_rule:
    WEBKIT_RULE_SYM '{' maybe_space ruleset_or_import maybe_space '}' {
        static_cast<CSSParser*>(parser)->rule = $4;
    }
;

webkit_decls:
    WEBKIT_DECLS_SYM '{' maybe_space declaration_list '}' {
        /* can be empty */
    }
;

webkit_value:
    WEBKIT_VALUE_SYM '{' maybe_space expr '}' {
        CSSParser* p = static_cast<CSSParser*>(parser);
        if ($4) {
            p->valueList = p->sinkFloatingValueList($4);
            int oldParsedProperties = p->numParsedProperties;
            if (!p->parseValue(p->id, p->important))
                p->rollbackLastProperties(p->numParsedProperties - oldParsedProperties);
            delete p->valueList;
            p->valueList = 0;
        }
    }
;

webkit_mediaquery:
     WEBKIT_MEDIAQUERY_SYM WHITESPACE maybe_space media_query '}' {
         CSSParser* p = static_cast<CSSParser*>(parser);
         p->mediaQuery = p->sinkFloatingMediaQuery($4);
     }
;

maybe_space:
    /* empty */ %prec UNIMPORTANT_TOK
  | maybe_space WHITESPACE
  ;

maybe_sgml:
    /* empty */
  | maybe_sgml SGML_CD
  | maybe_sgml WHITESPACE
  ;

maybe_charset:
   /* empty */
  | charset {
  }
;

charset:
  CHARSET_SYM maybe_space STRING maybe_space ';' {
     CSSParser* p = static_cast<CSSParser*>(parser);
     $$ = static_cast<CSSParser*>(parser)->createCharsetRule($3);
     if ($$ && p->styleElement && p->styleElement->isCSSStyleSheet())
         p->styleElement->append($$);
  }
  | CHARSET_SYM error invalid_block {
  }
  | CHARSET_SYM error ';' {
  }
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
        $$ = static_cast<CSSParser*>(parser)->createImportRule($3, $5);
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
    CSSParser* p = static_cast<CSSParser*>(parser);
    if (p->styleElement && p->styleElement->isCSSStyleSheet())
        static_cast<CSSStyleSheet*>(p->styleElement)->addNamespace(p, atomicString($3), atomicString($4));
}
| NAMESPACE_SYM error invalid_block
| NAMESPACE_SYM error ';'
;

maybe_ns_prefix:
/* empty */ { $$.characters = 0; }
| IDENT WHITESPACE { $$ = $1; }
;

string_or_uri:
STRING
| URI
;

media_feature:
    IDENT maybe_space {
        $$ = $1;
    }
    ;

maybe_media_value:
    /*empty*/ {
        $$ = 0;
    }
    | ':' maybe_space expr maybe_space {
        $$ = $3;
    }
    ;

media_query_exp:
    MEDIA_AND maybe_space '(' maybe_space media_feature maybe_space maybe_media_value ')' maybe_space {
        $5.lower();
        $$ = static_cast<CSSParser*>(parser)->createFloatingMediaQueryExp(atomicString($5), $7);
    }
    ;

media_query_exp_list:
    media_query_exp {
      CSSParser* p = static_cast<CSSParser*>(parser);
      $$ = p->createFloatingMediaQueryExpList();
      $$->append(p->sinkFloatingMediaQueryExp($1));
    }
    | media_query_exp_list media_query_exp {
      $$ = $1;
      $$->append(static_cast<CSSParser*>(parser)->sinkFloatingMediaQueryExp($2));
    }
    ;

maybe_media_query_exp_list:
    /*empty*/ {
        $$ = static_cast<CSSParser*>(parser)->createFloatingMediaQueryExpList();
    }
    | media_query_exp_list
    ;

maybe_media_restrictor:
    /*empty*/ {
        $$ = MediaQuery::None;
    }
    | MEDIA_ONLY {
        $$ = MediaQuery::Only;
    }
    | MEDIA_NOT {
        $$ = MediaQuery::Not;
    }
    ;

media_query:
    maybe_media_restrictor maybe_space medium maybe_media_query_exp_list {
        CSSParser* p = static_cast<CSSParser*>(parser);
        $3.lower();
        $$ = p->createFloatingMediaQuery($1, domString($3), p->sinkFloatingMediaQueryExpList($4));
    }
    ;

maybe_media_list:
     /* empty */ {
        $$ = static_cast<CSSParser*>(parser)->createMediaList();
     }
     | media_list
     ;

media_list:
    media_query {
        CSSParser* p = static_cast<CSSParser*>(parser);
        $$ = p->createMediaList();
        $$->appendMediaQuery(p->sinkFloatingMediaQuery($1));
    }
    | media_list ',' maybe_space media_query {
        $$ = $1;
        if ($$)
            $$->appendMediaQuery(static_cast<CSSParser*>(parser)->sinkFloatingMediaQuery($4));
    }
    | media_list error {
        $$ = 0;
    }
    ;

media:
    MEDIA_SYM maybe_space media_list '{' maybe_space ruleset_list '}' {
        $$ = static_cast<CSSParser*>(parser)->createMediaRule($3, $6);
    }
    | MEDIA_SYM maybe_space '{' maybe_space ruleset_list '}' {
        $$ = static_cast<CSSParser*>(parser)->createMediaRule(0, $5);
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
    selector %prec UNIMPORTANT_TOK {
        $$ = $1;
    }
    | selector_list ',' maybe_space selector %prec UNIMPORTANT_TOK {
        if ($1 && $4) {
            CSSParser* p = static_cast<CSSParser*>(parser);
            $$ = $1;
            $$->append(p->sinkFloatingSelector($4));
        } else
            $$ = 0;
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
        if (!$1)
            $$ = 0;
        else if ($$) {
            CSSParser* p = static_cast<CSSParser*>(parser);
            CSSSelector* end = $$;
            while (end->m_tagHistory)
                end = end->m_tagHistory;
            end->m_relation = $2;
            end->m_tagHistory = p->sinkFloatingSelector($1);
            if ($2 == CSSSelector::Descendant || $2 == CSSSelector::Child) {
                if (Document* doc = p->document())
                    doc->setUsesDescendantRules(true);
            } else if ($2 == CSSSelector::DirectAdjacent || $2 == CSSSelector::IndirectAdjacent) {
                if (Document* doc = p->document())
                    doc->setUsesSiblingRules(true);
            }
        }
    }
    | selector error {
        $$ = 0;
    }
    ;

namespace_selector:
    /* empty */ '|' { $$.characters = 0; $$.length = 0; }
    | '*' '|' { static UChar star = '*'; $$.characters = &star; $$.length = 1; }
    | IDENT '|' { $$ = $1; }
;

simple_selector:
    element_name maybe_space {
        CSSParser* p = static_cast<CSSParser*>(parser);
        $$ = p->createFloatingSelector();
        $$->m_tag = QualifiedName(nullAtom, atomicString($1), p->defaultNamespace);
    }
    | element_name specifier_list maybe_space {
        $$ = $2;
        if ($$) {
            CSSParser* p = static_cast<CSSParser*>(parser);
            $$->m_tag = QualifiedName(nullAtom, atomicString($1), p->defaultNamespace);
        }
    }
    | specifier_list maybe_space {
        $$ = $1;
        CSSParser* p = static_cast<CSSParser*>(parser);
        if ($$ && p->defaultNamespace != starAtom)
            $$->m_tag = QualifiedName(nullAtom, starAtom, p->defaultNamespace);
    }
    | namespace_selector element_name maybe_space {
        AtomicString namespacePrefix = atomicString($1);
        CSSParser* p = static_cast<CSSParser*>(parser);
        $$ = p->createFloatingSelector();
        if (p->styleElement && p->styleElement->isCSSStyleSheet())
            $$->m_tag = QualifiedName(namespacePrefix,
                                    atomicString($2),
                                    static_cast<CSSStyleSheet*>(p->styleElement)->determineNamespace(namespacePrefix));
        else // FIXME: Shouldn't this case be an error?
            $$->m_tag = QualifiedName(nullAtom, atomicString($2), p->defaultNamespace);
    }
    | namespace_selector element_name specifier_list maybe_space {
        $$ = $3;
        if ($$) {
            AtomicString namespacePrefix = atomicString($1);
            CSSParser* p = static_cast<CSSParser*>(parser);
            if (p->styleElement && p->styleElement->isCSSStyleSheet())
                $$->m_tag = QualifiedName(namespacePrefix,
                                          atomicString($2),
                                          static_cast<CSSStyleSheet*>(p->styleElement)->determineNamespace(namespacePrefix));
            else // FIXME: Shouldn't this case be an error?
                $$->m_tag = QualifiedName(nullAtom, atomicString($2), p->defaultNamespace);
        }
    }
    | namespace_selector specifier_list maybe_space {
        $$ = $2;
        if ($$) {
            AtomicString namespacePrefix = atomicString($1);
            CSSParser* p = static_cast<CSSParser*>(parser);
            if (p->styleElement && p->styleElement->isCSSStyleSheet())
                $$->m_tag = QualifiedName(namespacePrefix,
                                          starAtom,
                                          static_cast<CSSStyleSheet*>(p->styleElement)->determineNamespace(namespacePrefix));
        }
    }
  ;

element_name:
    IDENT {
        ParseString& str = $1;
        CSSParser* p = static_cast<CSSParser*>(parser);
        Document* doc = p->document();
        if (doc && doc->isHTMLDocument())
            str.lower();
        $$ = str;
    }
    | '*' {
        static UChar star = '*';
        $$.characters = &star;
        $$.length = 1;
    }
  ;

specifier_list:
    specifier {
        $$ = $1;
    }
    | specifier_list specifier {
        if (!$2)
            $$ = 0;
        else if ($1) {
            $$ = $1;
            CSSParser* p = static_cast<CSSParser*>(parser);
            CSSSelector* end = $1;
            while (end->m_tagHistory)
                end = end->m_tagHistory;
            end->m_relation = CSSSelector::SubSelector;
            end->m_tagHistory = p->sinkFloatingSelector($2);
        }
    }
    | specifier_list error {
        $$ = 0;
    }
;

specifier:
    IDSEL {
        CSSParser* p = static_cast<CSSParser*>(parser);
        $$ = p->createFloatingSelector();
        $$->m_match = CSSSelector::Id;
        if (!p->strict)
            $1.lower();
        $$->m_attr = idAttr;
        $$->m_value = atomicString($1);
    }
  | HEX {
        if ($1.characters[0] >= '0' && $1.characters[0] <= '9') {
            $$ = 0;
        } else {
            CSSParser* p = static_cast<CSSParser*>(parser);
            $$ = p->createFloatingSelector();
            $$->m_match = CSSSelector::Id;
            if (!p->strict)
                $1.lower();
            $$->m_attr = idAttr;
            $$->m_value = atomicString($1);
        }
    }
  | class
  | attrib
  | pseudo
    ;

class:
    '.' IDENT {
        CSSParser* p = static_cast<CSSParser*>(parser);
        $$ = p->createFloatingSelector();
        $$->m_match = CSSSelector::Class;
        if (!p->strict)
            $2.lower();
        $$->m_attr = classAttr;
        $$->m_value = atomicString($2);
    }
  ;

attr_name:
    IDENT maybe_space {
        ParseString& str = $1;
        CSSParser* p = static_cast<CSSParser*>(parser);
        Document* doc = p->document();
        if (doc && doc->isHTMLDocument())
            str.lower();
        $$ = str;
    }
    ;

attrib:
    '[' maybe_space attr_name ']' {
        $$ = static_cast<CSSParser*>(parser)->createFloatingSelector();
        $$->m_attr = QualifiedName(nullAtom, atomicString($3), nullAtom);
        $$->m_match = CSSSelector::Set;
    }
    | '[' maybe_space attr_name match maybe_space ident_or_string maybe_space ']' {
        $$ = static_cast<CSSParser*>(parser)->createFloatingSelector();
        $$->m_attr = QualifiedName(nullAtom, atomicString($3), nullAtom);
        $$->m_match = (CSSSelector::Match)$4;
        $$->m_value = atomicString($6);
    }
    | '[' maybe_space namespace_selector attr_name ']' {
        AtomicString namespacePrefix = atomicString($3);
        CSSParser* p = static_cast<CSSParser*>(parser);
        $$ = p->createFloatingSelector();
        $$->m_attr = QualifiedName(namespacePrefix,
                                   atomicString($4),
                                   static_cast<CSSStyleSheet*>(p->styleElement)->determineNamespace(namespacePrefix));
        $$->m_match = CSSSelector::Set;
    }
    | '[' maybe_space namespace_selector attr_name match maybe_space ident_or_string maybe_space ']' {
        AtomicString namespacePrefix = atomicString($3);
        CSSParser* p = static_cast<CSSParser*>(parser);
        $$ = p->createFloatingSelector();
        $$->m_attr = QualifiedName(namespacePrefix,
                                   atomicString($4),
                                   static_cast<CSSStyleSheet*>(p->styleElement)->determineNamespace(namespacePrefix));
        $$->m_match = (CSSSelector::Match)$5;
        $$->m_value = atomicString($7);
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
        $$->m_match = CSSSelector::PseudoClass;
        $2.lower();
        $$->m_value = atomicString($2);
        CSSSelector::PseudoType type = $$->pseudoType();
        if (type == CSSSelector::PseudoUnknown)
            $$ = 0;
        else if (type == CSSSelector::PseudoEmpty ||
                 type == CSSSelector::PseudoFirstChild) {
            CSSParser* p = static_cast<CSSParser*>(parser);
            Document* doc = p->document();
            if (doc)
                doc->setUsesSiblingRules(true);
        } else if (type == CSSSelector::PseudoFirstLine) {
            CSSParser* p = static_cast<CSSParser*>(parser);
            if (Document* doc = p->document())
                doc->setUsesFirstLineRules(true);
        }
    }
    | ':' ':' IDENT {
        $$ = static_cast<CSSParser*>(parser)->createFloatingSelector();
        $$->m_match = CSSSelector::PseudoElement;
        $3.lower();
        $$->m_value = atomicString($3);
        CSSSelector::PseudoType type = $$->pseudoType();
        if (type == CSSSelector::PseudoUnknown)
            $$ = 0;
        else if (type == CSSSelector::PseudoFirstLine) {
            CSSParser* p = static_cast<CSSParser*>(parser);
            if (Document* doc = p->document())
                doc->setUsesFirstLineRules(true);
        }
    }
    // used by :lang
    | ':' FUNCTION IDENT ')' {
        $$ = static_cast<CSSParser*>(parser)->createFloatingSelector();
        $$->m_match = CSSSelector::PseudoClass;
        $$->m_argument = atomicString($3);
        $2.lower();
        $$->m_value = atomicString($2);
        if ($$->pseudoType() == CSSSelector::PseudoUnknown)
            $$ = 0;
    }
    // used by :not
    | ':' NOTFUNCTION maybe_space simple_selector ')' {
        if (!$4)
            $$ = 0;
        else {
            CSSParser* p = static_cast<CSSParser*>(parser);
            $$ = p->createFloatingSelector();
            $$->m_match = CSSSelector::PseudoClass;
            $$->m_simpleSelector = p->sinkFloatingSelector($4);
            $2.lower();
            $$->m_value = atomicString($2);
        }
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
    | decl_list error {
        $$ = $1;
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
        if ($2)
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
        CSSParser* p = static_cast<CSSParser*>(parser);
        if ($1 && $4) {
            p->valueList = p->sinkFloatingValueList($4);
            int oldParsedProperties = p->numParsedProperties;
            $$ = p->parseValue($1, $5);
            if (!$$)
                p->rollbackLastProperties(p->numParsedProperties - oldParsedProperties);
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
    IMPORTANT_SYM maybe_space {
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
        $1.lower();
        DeprecatedString str = deprecatedString($1);
        const char* s = str.ascii();
        int l = str.length();
        $$ = getPropertyID(s, l);
#if ENABLE(SVG)
        if ($$ == 0)
            $$ = SVG::getSVGCSSPropertyID(s, l);
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
      DeprecatedString str = deprecatedString($1);
      $$.id = getValueID(str.lower().latin1(), str.length());
#if ENABLE(SVG)
      if ($$.id == 0)
          $$.id = SVG::getSVGCSSValueID(str.lower().latin1(), str.length());
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
/* FIXME: according to the specs a function can have a unary_operator in front. I know no case where this makes sense */
  | function {
      $$ = $1;
  }
  ;

unary_term:
  INTEGER maybe_space { $$.id = 0; $$.isInt = true; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_NUMBER; }
  | FLOAT maybe_space { $$.id = 0; $$.isInt = false; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_NUMBER; }
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
  HEX maybe_space { $$ = $1; }
  | IDSEL maybe_space { $$ = $1; }
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
