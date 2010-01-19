%{

/*
 *  Copyright (C) 2002-2003 Lars Knoll (knoll@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 *  Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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
#include "CSSPropertyNames.h"
#include "CSSRuleList.h"
#include "CSSSelector.h"
#include "CSSStyleSheet.h"
#include "Document.h"
#include "HTMLNames.h"
#include "MediaList.h"
#include "WebKitCSSKeyframeRule.h"
#include "WebKitCSSKeyframesRule.h"
#include <wtf/FastMalloc.h>
#include <stdlib.h>
#include <string.h>

using namespace WebCore;
using namespace HTMLNames;

#define YYMALLOC fastMalloc
#define YYFREE fastFree

#define YYENABLE_NLS 0
#define YYLTYPE_IS_TRIVIAL 1
#define YYMAXDEPTH 10000
#define YYDEBUG 0

// FIXME: Replace with %parse-param { CSSParser* parser } once we can depend on bison 2.x
#define YYPARSE_PARAM parser
#define YYLEX_PARAM parser

%}

%pure_parser

%union {
    bool boolean;
    char character;
    int integer;
    double number;
    CSSParserString string;

    CSSRule* rule;
    CSSRuleList* ruleList;
    CSSSelector* selector;
    Vector<CSSSelector*>* selectorList;
    CSSSelector::Relation relation;
    MediaList* mediaList;
    MediaQuery* mediaQuery;
    MediaQuery::Restrictor mediaQueryRestrictor;
    MediaQueryExp* mediaQueryExp;
    CSSParserValue value;
    CSSParserValueList* valueList;
    Vector<MediaQueryExp*>* mediaQueryExpList;
    WebKitCSSKeyframeRule* keyframeRule;
    WebKitCSSKeyframesRule* keyframesRule;
    float val;
}

%{

static inline int cssyyerror(const char*)
{
    return 1;
}

static int cssyylex(YYSTYPE* yylval, void* parser)
{
    return static_cast<CSSParser*>(parser)->lex(yylval);
}

%}

%expect 54

%nonassoc LOWEST_PREC

%left UNIMPORTANT_TOK

%token WHITESPACE SGML_CD
%token TOKEN_EOF 0

%token INCLUDES
%token DASHMATCH
%token BEGINSWITH
%token ENDSWITH
%token CONTAINS

%token <string> STRING
%right <string> IDENT
%token <string> NTH

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
%token WEBKIT_KEYFRAME_RULE_SYM
%token WEBKIT_KEYFRAMES_SYM
%token WEBKIT_VALUE_SYM
%token WEBKIT_MEDIAQUERY_SYM
%token WEBKIT_SELECTOR_SYM
%token WEBKIT_VARIABLES_SYM
%token WEBKIT_DEFINE_SYM
%token VARIABLES_FOR
%token WEBKIT_VARIABLES_DECLS_SYM
%token ATKEYWORD

%token IMPORTANT_SYM
%token MEDIA_ONLY
%token MEDIA_NOT
%token MEDIA_AND

%token <number> REMS
%token <number> QEMS
%token <number> EMS
%token <number> EXS
%token <number> PXS
%token <number> CMS
%token <number> MMS
%token <number> INS
%token <number> PTS
%token <number> PCS
%token <number> DEGS
%token <number> RADS
%token <number> GRADS
%token <number> TURNS
%token <number> MSECS
%token <number> SECS
%token <number> HERZ
%token <number> KHERZ
%token <string> DIMEN
%token <number> PERCENTAGE
%token <number> FLOATTOKEN
%token <number> INTEGER

%token <string> URI
%token <string> FUNCTION
%token <string> NOTFUNCTION

%token <string> UNICODERANGE

%token <string> VARCALL

%type <relation> combinator

%type <rule> charset
%type <rule> ruleset
%type <rule> media
%type <rule> import
%type <rule> namespace
%type <rule> page
%type <rule> font_face
%type <rule> keyframes
%type <rule> invalid_rule
%type <rule> save_block
%type <rule> invalid_at
%type <rule> rule
%type <rule> valid_rule
%type <ruleList> block_rule_list 
%type <rule> block_rule
%type <rule> block_valid_rule
%type <rule> variables_rule
%type <mediaList> variables_media_list

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
%type <mediaQueryExpList> maybe_and_media_query_exp_list

%type <string> keyframe_name
%type <keyframeRule> keyframe_rule
%type <keyframesRule> keyframes_rule
%type <valueList> key_list
%type <value> key

%type <integer> property

%type <selector> specifier
%type <selector> specifier_list
%type <selector> simple_selector
%type <selector> selector
%type <selectorList> selector_list
%type <selector> selector_with_trailing_whitespace
%type <selector> class
%type <selector> attrib
%type <selector> pseudo

%type <boolean> declaration_list
%type <boolean> decl_list
%type <boolean> declaration

%type <boolean> prio

%type <integer> match
%type <integer> unary_operator
%type <character> operator

%type <valueList> expr
%type <value> term
%type <value> unary_term
%type <value> function

%type <string> element_name
%type <string> attr_name

%type <string> variable_name
%type <boolean> variables_declaration_list
%type <boolean> variables_decl_list
%type <boolean> variables_declaration
%type <value> variable_reference

%%

stylesheet:
    maybe_space maybe_charset maybe_sgml rule_list
  | webkit_rule maybe_space
  | webkit_decls maybe_space
  | webkit_value maybe_space
  | webkit_mediaquery maybe_space
  | webkit_selector maybe_space
  | webkit_variables_decls maybe_space
  | webkit_keyframe_rule maybe_space
  ;

webkit_rule:
    WEBKIT_RULE_SYM '{' maybe_space valid_rule maybe_space '}' {
        static_cast<CSSParser*>(parser)->m_rule = $4;
    }
;

webkit_keyframe_rule:
    WEBKIT_KEYFRAME_RULE_SYM '{' maybe_space keyframe_rule maybe_space '}' {
        static_cast<CSSParser*>(parser)->m_keyframe = $4;
    }
;

webkit_decls:
    WEBKIT_DECLS_SYM '{' maybe_space declaration_list '}' {
        /* can be empty */
    }
;

webkit_variables_decls:
    WEBKIT_VARIABLES_DECLS_SYM '{' maybe_space variables_declaration_list '}' {
        /* can be empty */
    }
;

webkit_value:
    WEBKIT_VALUE_SYM '{' maybe_space expr '}' {
        CSSParser* p = static_cast<CSSParser*>(parser);
        if ($4) {
            p->m_valueList = p->sinkFloatingValueList($4);
            int oldParsedProperties = p->m_numParsedProperties;
            if (!p->parseValue(p->m_id, p->m_important))
                p->rollbackLastProperties(p->m_numParsedProperties - oldParsedProperties);
            delete p->m_valueList;
            p->m_valueList = 0;
        }
    }
;

webkit_mediaquery:
     WEBKIT_MEDIAQUERY_SYM WHITESPACE maybe_space media_query '}' {
         CSSParser* p = static_cast<CSSParser*>(parser);
         p->m_mediaQuery = p->sinkFloatingMediaQuery($4);
     }
;

webkit_selector:
    WEBKIT_SELECTOR_SYM '{' maybe_space selector_list '}' {
        if ($4) {
            CSSParser* p = static_cast<CSSParser*>(parser);
            if (p->m_selectorListForParseSelector)
                p->m_selectorListForParseSelector->adoptSelectorVector(*$4);
        }
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

closing_brace:
    '}'
  | %prec LOWEST_PREC TOKEN_EOF
  ;

charset:
  CHARSET_SYM maybe_space STRING maybe_space ';' {
     CSSParser* p = static_cast<CSSParser*>(parser);
     $$ = static_cast<CSSParser*>(parser)->createCharsetRule($3);
     if ($$ && p->m_styleSheet)
         p->m_styleSheet->append($$);
  }
  | CHARSET_SYM error invalid_block {
  }
  | CHARSET_SYM error ';' {
  }
;

rule_list:
   /* empty */
 | rule_list rule maybe_sgml {
     CSSParser* p = static_cast<CSSParser*>(parser);
     if ($2 && p->m_styleSheet)
         p->m_styleSheet->append($2);
 }
 ;

valid_rule:
    ruleset
  | media
  | page
  | font_face
  | keyframes
  | namespace
  | import
  | variables_rule
  ;

rule:
    valid_rule {
        static_cast<CSSParser*>(parser)->m_hadSyntacticallyValidCSSRule = true;
    }
  | invalid_rule
  | invalid_at
  ;

block_rule_list: 
    /* empty */ { $$ = 0; }
  | block_rule_list block_rule maybe_sgml {
      $$ = $1;
      if ($2) {
          if (!$$)
              $$ = static_cast<CSSParser*>(parser)->createRuleList();
          $$->append($2);
      }
  }
  ;

block_valid_rule:
    ruleset
  | page
  | font_face
  | keyframes
  ;

block_rule:
    block_valid_rule
  | invalid_rule
  | invalid_at
  | namespace
  | import
  | variables_rule
  | media
  ;


import:
    IMPORT_SYM maybe_space string_or_uri maybe_space maybe_media_list ';' {
        $$ = static_cast<CSSParser*>(parser)->createImportRule($3, $5);
    }
  | IMPORT_SYM maybe_space string_or_uri maybe_space maybe_media_list invalid_block {
        $$ = 0;
    }
  | IMPORT_SYM error ';' {
        $$ = 0;
    }
  | IMPORT_SYM error invalid_block {
        $$ = 0;
    }
  ;

variables_rule:
    WEBKIT_VARIABLES_SYM maybe_space maybe_media_list '{' maybe_space variables_declaration_list '}' {
        $$ = static_cast<CSSParser*>(parser)->createVariablesRule($3, true);
    }
    |
    WEBKIT_DEFINE_SYM maybe_space variables_media_list '{' maybe_space variables_declaration_list '}' {
        $$ = static_cast<CSSParser*>(parser)->createVariablesRule($3, false);
    }
    ;

variables_media_list:
    /* empty */ {
        $$ = static_cast<CSSParser*>(parser)->createMediaList();
    }
    |
    VARIABLES_FOR WHITESPACE media_list {
        $$ = $3;
    }
    ;

variables_declaration_list:
    variables_declaration {
        $$ = $1;
    }
    | variables_decl_list variables_declaration {
        $$ = $1;
        if ($2)
            $$ = $2;
    }
    | variables_decl_list {
        $$ = $1;
    }
    | error invalid_block_list error {
        $$ = false;
    }
    | error {
        $$ = false;
    }
    | variables_decl_list error {
        $$ = $1;
    }
    ;

variables_decl_list:
    variables_declaration ';' maybe_space {
        $$ = $1;
    }
    | variables_declaration invalid_block_list ';' maybe_space {
        $$ = false;
    }
    | error ';' maybe_space {
        $$ = false;
    }
    | error invalid_block_list error ';' maybe_space {
        $$ = false;
    }
    | variables_decl_list variables_declaration ';' maybe_space {
        $$ = $1;
        if ($2)
            $$ = $2;
    }
    | variables_decl_list error ';' maybe_space {
        $$ = $1;
    }
    | variables_decl_list error invalid_block_list error ';' maybe_space {
        $$ = $1;
    }
    ;

variables_declaration:
    variable_name ':' maybe_space expr {
        $$ = static_cast<CSSParser*>(parser)->addVariable($1, $4);
    }
    |
    variable_name maybe_space '{' maybe_space declaration_list '}' maybe_space {
        $$ = static_cast<CSSParser*>(parser)->addVariableDeclarationBlock($1);
    }
    |
    variable_name error {
        $$ = false;
    }
    |
    variable_name ':' maybe_space error expr {
        $$ = false;
    }
    |
    variable_name ':' maybe_space {
        /* @variables { varname: } Just reduce away this variable with no value. */
        $$ = false;
    }
    |
    variable_name ':' maybe_space error {
        /* if we come across rules with invalid values like this case: @variables { varname: *; }, just discard the property/value pair */
        $$ = false;
    }
    ;

variable_name:
    IDENT maybe_space {
        $$ = $1;
    }
    ;

namespace:
NAMESPACE_SYM maybe_space maybe_ns_prefix string_or_uri maybe_space ';' {
    static_cast<CSSParser*>(parser)->addNamespace($3, $4);
    $$ = 0;
}
| NAMESPACE_SYM maybe_space maybe_ns_prefix string_or_uri maybe_space invalid_block {
    $$ = 0;
}
| NAMESPACE_SYM error invalid_block {
    $$ = 0;
}
| NAMESPACE_SYM error ';' {
    $$ = 0;
}
;

maybe_ns_prefix:
/* empty */ { $$.characters = 0; }
| IDENT maybe_space { $$ = $1; }
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
    '(' maybe_space media_feature maybe_space maybe_media_value ')' maybe_space {
        $3.lower();
        $$ = static_cast<CSSParser*>(parser)->createFloatingMediaQueryExp($3, $5);
    }
    ;

media_query_exp_list:
    media_query_exp {
        CSSParser* p = static_cast<CSSParser*>(parser);
        $$ = p->createFloatingMediaQueryExpList();
        $$->append(p->sinkFloatingMediaQueryExp($1));
    }
    | media_query_exp_list maybe_space MEDIA_AND maybe_space media_query_exp {
        $$ = $1;
        $$->append(static_cast<CSSParser*>(parser)->sinkFloatingMediaQueryExp($5));
    }
    ;

maybe_and_media_query_exp_list:
    /*empty*/ {
        $$ = static_cast<CSSParser*>(parser)->createFloatingMediaQueryExpList();
    }
    | MEDIA_AND maybe_space media_query_exp_list {
        $$ = $3;
    }
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
    media_query_exp_list {
        CSSParser* p = static_cast<CSSParser*>(parser);
        $$ = p->createFloatingMediaQuery(p->sinkFloatingMediaQueryExpList($1));
    }
    |
    maybe_media_restrictor maybe_space medium maybe_and_media_query_exp_list {
        CSSParser* p = static_cast<CSSParser*>(parser);
        $3.lower();
        $$ = p->createFloatingMediaQuery($1, $3, p->sinkFloatingMediaQueryExpList($4));
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
    MEDIA_SYM maybe_space media_list '{' maybe_space block_rule_list save_block {
        $$ = static_cast<CSSParser*>(parser)->createMediaRule($3, $6);
    }
    | MEDIA_SYM maybe_space '{' maybe_space block_rule_list save_block {
        $$ = static_cast<CSSParser*>(parser)->createMediaRule(0, $5);
    }
    ;

medium:
  IDENT maybe_space {
      $$ = $1;
  }
  ;

keyframes:
    WEBKIT_KEYFRAMES_SYM maybe_space keyframe_name maybe_space '{' maybe_space keyframes_rule '}' {
        $$ = $7;
        $7->setNameInternal($3);
    }
    ;
  
keyframe_name:
    IDENT
    | STRING
    ;

keyframes_rule:
    /* empty */ { $$ = static_cast<CSSParser*>(parser)->createKeyframesRule(); }
    | keyframes_rule keyframe_rule maybe_space {
        $$ = $1;
        if ($2)
            $$->append($2);
    }
    ;

keyframe_rule:
    key_list maybe_space '{' maybe_space declaration_list '}' {
        $$ = static_cast<CSSParser*>(parser)->createKeyframeRule($1);
    }
    ;

key_list:
    key {
        CSSParser* p = static_cast<CSSParser*>(parser);
        $$ = p->createFloatingValueList();
        $$->addValue(p->sinkFloatingValue($1));
    }
    | key_list maybe_space ',' maybe_space key {
        CSSParser* p = static_cast<CSSParser*>(parser);
        $$ = $1;
        if ($$)
            $$->addValue(p->sinkFloatingValue($5));
    }
    ;

key:
    PERCENTAGE { $$.id = 0; $$.isInt = false; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_NUMBER; }
    | IDENT {
        $$.id = 0; $$.isInt = false; $$.unit = CSSPrimitiveValue::CSS_NUMBER;
        CSSParserString& str = $1;
        if (equalIgnoringCase("from", str.characters, str.length))
            $$.fValue = 0;
        else if (equalIgnoringCase("to", str.characters, str.length))
            $$.fValue = 100;
        else
            YYERROR;
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
    FONT_FACE_SYM maybe_space
    '{' maybe_space declaration_list '}'  maybe_space {
        $$ = static_cast<CSSParser*>(parser)->createFontFaceRule();
    }
    | FONT_FACE_SYM error invalid_block {
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
  ;

unary_operator:
    '-' { $$ = -1; }
  | '+' { $$ = 1; }
  ;

ruleset:
    selector_list '{' maybe_space declaration_list closing_brace {
        $$ = static_cast<CSSParser*>(parser)->createStyleRule($1);
    }
  ;

selector_list:
    selector %prec UNIMPORTANT_TOK {
        if ($1) {
            CSSParser* p = static_cast<CSSParser*>(parser);
            $$ = p->reusableSelectorVector();
            deleteAllValues(*$$);
            $$->shrink(0);
            $$->append(p->sinkFloatingSelector($1));
        }
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

selector_with_trailing_whitespace:
    selector WHITESPACE {
        $$ = $1;
    }
    ;

selector:
    simple_selector {
        $$ = $1;
    }
    | selector_with_trailing_whitespace
    {
        $$ = $1;
    }
    | selector_with_trailing_whitespace simple_selector
    {
        $$ = $2;
        if (!$1)
            $$ = 0;
        else if ($$) {
            CSSParser* p = static_cast<CSSParser*>(parser);
            CSSSelector* end = $$;
            while (end->tagHistory())
                end = end->tagHistory();
            end->m_relation = CSSSelector::Descendant;
            end->setTagHistory(p->sinkFloatingSelector($1));
            if (Document* doc = p->document())
                doc->setUsesDescendantRules(true);
        }
    }
    | selector combinator simple_selector {
        $$ = $3;
        if (!$1)
            $$ = 0;
        else if ($$) {
            CSSParser* p = static_cast<CSSParser*>(parser);
            CSSSelector* end = $$;
            while (end->tagHistory())
                end = end->tagHistory();
            end->m_relation = $2;
            end->setTagHistory(p->sinkFloatingSelector($1));
            if ($2 == CSSSelector::Child) {
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
    element_name {
        CSSParser* p = static_cast<CSSParser*>(parser);
        $$ = p->createFloatingSelector();
        $$->m_tag = QualifiedName(nullAtom, $1, p->m_defaultNamespace);
    }
    | element_name specifier_list {
        $$ = $2;
        if ($$) {
            CSSParser* p = static_cast<CSSParser*>(parser);
            $$->m_tag = QualifiedName(nullAtom, $1, p->m_defaultNamespace);
        }
    }
    | specifier_list {
        $$ = $1;
        CSSParser* p = static_cast<CSSParser*>(parser);
        if ($$ && p->m_defaultNamespace != starAtom)
            $$->m_tag = QualifiedName(nullAtom, starAtom, p->m_defaultNamespace);
    }
    | namespace_selector element_name {
        AtomicString namespacePrefix = $1;
        CSSParser* p = static_cast<CSSParser*>(parser);
        $$ = p->createFloatingSelector();
        if (p->m_styleSheet)
            $$->m_tag = QualifiedName(namespacePrefix, $2,
                                      p->m_styleSheet->determineNamespace(namespacePrefix));
        else // FIXME: Shouldn't this case be an error?
            $$->m_tag = QualifiedName(nullAtom, $2, p->m_defaultNamespace);
    }
    | namespace_selector element_name specifier_list {
        $$ = $3;
        if ($$) {
            AtomicString namespacePrefix = $1;
            CSSParser* p = static_cast<CSSParser*>(parser);
            if (p->m_styleSheet)
                $$->m_tag = QualifiedName(namespacePrefix, $2,
                                          p->m_styleSheet->determineNamespace(namespacePrefix));
            else // FIXME: Shouldn't this case be an error?
                $$->m_tag = QualifiedName(nullAtom, $2, p->m_defaultNamespace);
        }
    }
    | namespace_selector specifier_list {
        $$ = $2;
        if ($$) {
            AtomicString namespacePrefix = $1;
            CSSParser* p = static_cast<CSSParser*>(parser);
            if (p->m_styleSheet)
                $$->m_tag = QualifiedName(namespacePrefix, starAtom,
                                          p->m_styleSheet->determineNamespace(namespacePrefix));
        }
    }
  ;

element_name:
    IDENT {
        CSSParserString& str = $1;
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
            while (end->tagHistory())
                end = end->tagHistory();
            end->m_relation = CSSSelector::SubSelector;
            end->setTagHistory(p->sinkFloatingSelector($2));
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
        if (!p->m_strict)
            $1.lower();
        $$->m_value = $1;
    }
  | HEX {
        if ($1.characters[0] >= '0' && $1.characters[0] <= '9') {
            $$ = 0;
        } else {
            CSSParser* p = static_cast<CSSParser*>(parser);
            $$ = p->createFloatingSelector();
            $$->m_match = CSSSelector::Id;
            if (!p->m_strict)
                $1.lower();
            $$->m_value = $1;
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
        if (!p->m_strict)
            $2.lower();
        $$->m_value = $2;
    }
  ;

attr_name:
    IDENT maybe_space {
        CSSParserString& str = $1;
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
        $$->setAttribute(QualifiedName(nullAtom, $3, nullAtom));
        $$->m_match = CSSSelector::Set;
    }
    | '[' maybe_space attr_name match maybe_space ident_or_string maybe_space ']' {
        $$ = static_cast<CSSParser*>(parser)->createFloatingSelector();
        $$->setAttribute(QualifiedName(nullAtom, $3, nullAtom));
        $$->m_match = (CSSSelector::Match)$4;
        $$->m_value = $6;
    }
    | '[' maybe_space namespace_selector attr_name ']' {
        AtomicString namespacePrefix = $3;
        CSSParser* p = static_cast<CSSParser*>(parser);
        $$ = p->createFloatingSelector();
        $$->setAttribute(QualifiedName(namespacePrefix, $4,
                                   p->m_styleSheet->determineNamespace(namespacePrefix)));
        $$->m_match = CSSSelector::Set;
    }
    | '[' maybe_space namespace_selector attr_name match maybe_space ident_or_string maybe_space ']' {
        AtomicString namespacePrefix = $3;
        CSSParser* p = static_cast<CSSParser*>(parser);
        $$ = p->createFloatingSelector();
        $$->setAttribute(QualifiedName(namespacePrefix, $4,
                                   p->m_styleSheet->determineNamespace(namespacePrefix)));
        $$->m_match = (CSSSelector::Match)$5;
        $$->m_value = $7;
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
        $$->m_value = $2;
        CSSSelector::PseudoType type = $$->pseudoType();
        if (type == CSSSelector::PseudoUnknown)
            $$ = 0;
        else if (type == CSSSelector::PseudoEmpty ||
                 type == CSSSelector::PseudoFirstChild ||
                 type == CSSSelector::PseudoFirstOfType ||
                 type == CSSSelector::PseudoLastChild ||
                 type == CSSSelector::PseudoLastOfType ||
                 type == CSSSelector::PseudoOnlyChild ||
                 type == CSSSelector::PseudoOnlyOfType) {
            CSSParser* p = static_cast<CSSParser*>(parser);
            Document* doc = p->document();
            if (doc)
                doc->setUsesSiblingRules(true);
        } else if (type == CSSSelector::PseudoFirstLine) {
            CSSParser* p = static_cast<CSSParser*>(parser);
            if (Document* doc = p->document())
                doc->setUsesFirstLineRules(true);
        } else if (type == CSSSelector::PseudoBefore ||
                   type == CSSSelector::PseudoAfter) {
            CSSParser* p = static_cast<CSSParser*>(parser);
            if (Document* doc = p->document())
                doc->setUsesBeforeAfterRules(true);
        }
    }
    | ':' ':' IDENT {
        $$ = static_cast<CSSParser*>(parser)->createFloatingSelector();
        $$->m_match = CSSSelector::PseudoElement;
        $3.lower();
        $$->m_value = $3;
        CSSSelector::PseudoType type = $$->pseudoType();
        if (type == CSSSelector::PseudoUnknown)
            $$ = 0;
        else if (type == CSSSelector::PseudoFirstLine) {
            CSSParser* p = static_cast<CSSParser*>(parser);
            if (Document* doc = p->document())
                doc->setUsesFirstLineRules(true);
        } else if (type == CSSSelector::PseudoBefore ||
                   type == CSSSelector::PseudoAfter) {
            CSSParser* p = static_cast<CSSParser*>(parser);
            if (Document* doc = p->document())
                doc->setUsesBeforeAfterRules(true);
        }
    }
    // used by :nth-*(ax+b)
    | ':' FUNCTION maybe_space NTH maybe_space ')' {
        CSSParser *p = static_cast<CSSParser*>(parser);
        $$ = p->createFloatingSelector();
        $$->m_match = CSSSelector::PseudoClass;
        $$->setArgument($4);
        $$->m_value = $2;
        CSSSelector::PseudoType type = $$->pseudoType();
        if (type == CSSSelector::PseudoUnknown)
            $$ = 0;
        else if (type == CSSSelector::PseudoNthChild ||
                 type == CSSSelector::PseudoNthOfType ||
                 type == CSSSelector::PseudoNthLastChild ||
                 type == CSSSelector::PseudoNthLastOfType) {
            if (p->document())
                p->document()->setUsesSiblingRules(true);
        }
    }
    // used by :nth-*
    | ':' FUNCTION maybe_space INTEGER maybe_space ')' {
        CSSParser *p = static_cast<CSSParser*>(parser);
        $$ = p->createFloatingSelector();
        $$->m_match = CSSSelector::PseudoClass;
        $$->setArgument(String::number($4));
        $$->m_value = $2;
        CSSSelector::PseudoType type = $$->pseudoType();
        if (type == CSSSelector::PseudoUnknown)
            $$ = 0;
        else if (type == CSSSelector::PseudoNthChild ||
                 type == CSSSelector::PseudoNthOfType ||
                 type == CSSSelector::PseudoNthLastChild ||
                 type == CSSSelector::PseudoNthLastOfType) {
            if (p->document())
                p->document()->setUsesSiblingRules(true);
        }
    }
    // used by :nth-*(odd/even) and :lang
    | ':' FUNCTION maybe_space IDENT maybe_space ')' {
        CSSParser *p = static_cast<CSSParser*>(parser);
        $$ = p->createFloatingSelector();
        $$->m_match = CSSSelector::PseudoClass;
        $$->setArgument($4);
        $2.lower();
        $$->m_value = $2;
        CSSSelector::PseudoType type = $$->pseudoType();
        if (type == CSSSelector::PseudoUnknown)
            $$ = 0;
        else if (type == CSSSelector::PseudoNthChild ||
                 type == CSSSelector::PseudoNthOfType ||
                 type == CSSSelector::PseudoNthLastChild ||
                 type == CSSSelector::PseudoNthLastOfType) {
            if (p->document())
                p->document()->setUsesSiblingRules(true);
        }
    }
    // used by :not
    | ':' NOTFUNCTION maybe_space simple_selector maybe_space ')' {
        if (!$4 || $4->simpleSelector() || $4->tagHistory())
            $$ = 0;
        else {
            CSSParser* p = static_cast<CSSParser*>(parser);
            $$ = p->createFloatingSelector();
            $$->m_match = CSSSelector::PseudoClass;
            $$->setSimpleSelector(p->sinkFloatingSelector($4));
            $2.lower();
            $$->m_value = $2;
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
    | decl_list invalid_block_list {
        $$ = $1;
    }
    ;

decl_list:
    declaration ';' maybe_space {
        $$ = $1;
    }
    | declaration invalid_block_list maybe_space {
        $$ = false;
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
            p->m_valueList = p->sinkFloatingValueList($4);
            int oldParsedProperties = p->m_numParsedProperties;
            $$ = p->parseValue($1, $5);
            if (!$$)
                p->rollbackLastProperties(p->m_numParsedProperties - oldParsedProperties);
            delete p->m_valueList;
            p->m_valueList = 0;
        }
    }
    |
    variable_reference maybe_space {
        CSSParser* p = static_cast<CSSParser*>(parser);
        p->m_valueList = new CSSParserValueList;
        p->m_valueList->addValue(p->sinkFloatingValue($1));
        int oldParsedProperties = p->m_numParsedProperties;
        $$ = p->parseValue(CSSPropertyWebkitVariableDeclarationBlock, false);
        if (!$$)
            p->rollbackLastProperties(p->m_numParsedProperties - oldParsedProperties);
        delete p->m_valueList;
        p->m_valueList = 0;
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
    property ':' maybe_space expr prio error {
        /* When we encounter something like p {color: red !important fail;} we should drop the declaration */
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
    |
    property ':' maybe_space error {
        /* if we come across rules with invalid values like this case: p { weight: *; }, just discard the rule */
        $$ = false;
    }
    |
    property invalid_block {
        /* if we come across: div { color{;color:maroon} }, ignore everything within curly brackets */
        $$ = false;
    }
  ;

property:
    IDENT maybe_space {
        $$ = cssPropertyID($1);
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
                CSSParserValue v;
                v.id = 0;
                v.unit = CSSParserValue::Operator;
                v.iValue = $2;
                $$->addValue(v);
            }
            $$->addValue(p->sinkFloatingValue($3));
        }
    }
    | expr invalid_block_list {
        $$ = 0;
    }
    | expr invalid_block_list error {
        $$ = 0;
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
      $$.id = cssValueKeywordID($1);
      $$.unit = CSSPrimitiveValue::CSS_IDENT;
      $$.string = $1;
  }
  /* We might need to actually parse the number from a dimension, but we can't just put something that uses $$.string into unary_term. */
  | DIMEN maybe_space { $$.id = 0; $$.string = $1; $$.unit = CSSPrimitiveValue::CSS_DIMENSION; }
  | unary_operator DIMEN maybe_space { $$.id = 0; $$.string = $2; $$.unit = CSSPrimitiveValue::CSS_DIMENSION; }
  | URI maybe_space { $$.id = 0; $$.string = $1; $$.unit = CSSPrimitiveValue::CSS_URI; }
  | UNICODERANGE maybe_space { $$.id = 0; $$.string = $1; $$.unit = CSSPrimitiveValue::CSS_UNICODE_RANGE; }
  | hexcolor { $$.id = 0; $$.string = $1; $$.unit = CSSPrimitiveValue::CSS_PARSER_HEXCOLOR; }
  | '#' maybe_space { $$.id = 0; $$.string = CSSParserString(); $$.unit = CSSPrimitiveValue::CSS_PARSER_HEXCOLOR; } /* Handle error case: "color: #;" */
  /* FIXME: according to the specs a function can have a unary_operator in front. I know no case where this makes sense */
  | function {
      $$ = $1;
  }
  | variable_reference maybe_space {
      $$ = $1;
  }
  | '%' maybe_space { /* Handle width: %; */
      $$.id = 0; $$.unit = 0;
  }
  ;

unary_term:
  INTEGER maybe_space { $$.id = 0; $$.isInt = true; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_NUMBER; }
  | FLOATTOKEN maybe_space { $$.id = 0; $$.isInt = false; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_NUMBER; }
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
  | TURNS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_TURN; }
  | MSECS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_MS; }
  | SECS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_S; }
  | HERZ maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_HZ; }
  | KHERZ maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_KHZ; }
  | EMS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_EMS; }
  | QEMS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSParserValue::Q_EMS; }
  | EXS maybe_space { $$.id = 0; $$.fValue = $1; $$.unit = CSSPrimitiveValue::CSS_EXS; }
  | REMS maybe_space {
      $$.id = 0;
      $$.fValue = $1;
      $$.unit = CSSPrimitiveValue::CSS_REMS;
      CSSParser* p = static_cast<CSSParser*>(parser);
      if (Document* doc = p->document())
          doc->setUsesRemUnits(true);
  }
  ;

variable_reference:
  VARCALL {
      $$.id = 0;
      $$.string = $1;
      $$.unit = CSSPrimitiveValue::CSS_PARSER_VARIABLE_FUNCTION_SYNTAX;
  }
  ;

function:
    FUNCTION maybe_space expr ')' maybe_space {
        CSSParser* p = static_cast<CSSParser*>(parser);
        CSSParserFunction* f = p->createFloatingFunction();
        f->name = $1;
        f->args = p->sinkFloatingValueList($3);
        $$.id = 0;
        $$.unit = CSSParserValue::Function;
        $$.function = f;
    } |
    FUNCTION maybe_space error {
        CSSParser* p = static_cast<CSSParser*>(parser);
        CSSParserFunction* f = p->createFloatingFunction();
        f->name = $1;
        f->args = 0;
        $$.id = 0;
        $$.unit = CSSParserValue::Function;
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

save_block:
    closing_brace {
        $$ = 0;
    }
  | error closing_brace {
        $$ = 0;
    }
    ;

invalid_at:
    ATKEYWORD error invalid_block {
        $$ = 0;
    }
  | ATKEYWORD error ';' {
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
    '{' error invalid_block_list error closing_brace {
        static_cast<CSSParser*>(parser)->invalidBlockHit();
    }
  | '{' error closing_brace {
        static_cast<CSSParser*>(parser)->invalidBlockHit();
    }
    ;

invalid_block_list:
    invalid_block
  | invalid_block_list error invalid_block
;

%%
