
/*  A Bison parser, made from parser.y
    by GNU Bison version 1.28  */

#define YYBISON 1  /* Identify Bison output.  */

#define yyparse cssyyparse
#define yylex cssyylex
#define yyerror cssyyerror
#define yylval cssyylval
#define yychar cssyychar
#define yydebug cssyydebug
#define yynerrs cssyynerrs
#define	WHITESPACE	257
#define	SGML_CD	258
#define	INCLUDES	259
#define	DASHMATCH	260
#define	BEGINSWITH	261
#define	ENDSWITH	262
#define	CONTAINS	263
#define	STRING	264
#define	IDENT	265
#define	HASH	266
#define	IMPORT_SYM	267
#define	PAGE_SYM	268
#define	MEDIA_SYM	269
#define	FONT_FACE_SYM	270
#define	CHARSET_SYM	271
#define	NAMESPACE_SYM	272
#define	KHTML_RULE_SYM	273
#define	KHTML_DECLS_SYM	274
#define	KHTML_VALUE_SYM	275
#define	IMPORTANT_SYM	276
#define	QEMS	277
#define	EMS	278
#define	EXS	279
#define	PXS	280
#define	CMS	281
#define	MMS	282
#define	INS	283
#define	PTS	284
#define	PCS	285
#define	DEGS	286
#define	RADS	287
#define	GRADS	288
#define	MSECS	289
#define	SECS	290
#define	HERZ	291
#define	KHERZ	292
#define	DIMEN	293
#define	PERCENTAGE	294
#define	NUMBER	295
#define	URI	296
#define	FUNCTION	297
#define	UNICODERANGE	298

#line 1 "parser.y"


/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2002-2003 Lars Knoll (knoll@kde.org)
 *  Copyright (C) 2004 Apple Computer, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <stdlib.h>

#include <dom/dom_string.h>
#include <xml/dom_docimpl.h>
#include <css/css_ruleimpl.h>
#include <css/css_stylesheetimpl.h>
#include <css/css_valueimpl.h>
#include <misc/htmlhashes.h>
#include "cssparser.h"

#include "xml_namespace_table.h"
    
#include <assert.h>
#include <kdebug.h>
// #define CSS_DEBUG

using namespace DOM;

//
// The following file defines the function
//     const struct props *findProp(const char *word, int len)
//
// with 'props->id' a CSS property in the range from CSS_PROP_MIN to
// (and including) CSS_PROP_TOTAL-1

// turn off inlining to void warning with newer gcc
#undef __inline
#define __inline
#include "cssproperties.c"
#include "cssvalues.c"
#undef __inline

int DOM::getPropertyID(const char *tagStr, int len)
{
    const struct props *propsPtr = findProp(tagStr, len);
    if (!propsPtr)
        return 0;

    return propsPtr->id;
}

static inline int getValueID(const char *tagStr, int len)
{
    const struct css_value *val = findValue(tagStr, len);
    if (!val)
        return 0;

    return val->id;
}


#define YYDEBUG 0
#define YYMAXDEPTH 0
#define YYPARSE_PARAM parser

#line 86 "parser.y"
typedef union {
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
    int element;
    CSSSelector::Relation relation;
    bool b;
    int i;
    char tok;
    Value value;
    ValueList *valueList;
} YYSTYPE;
#line 106 "parser.y"


static inline int cssyyerror(const char *x ) {
#ifdef CSS_DEBUG
    qDebug( x );
#else
    Q_UNUSED( x );
#endif
    return 1;
}

static int cssyylex( YYSTYPE *yylval ) {
    return CSSParser::current()->lex( yylval );
}


#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		312
#define	YYFLAG		-32768
#define	YYNTBASE	63

#define YYTRANSLATE(x) ((unsigned)(x) <= 298 ? yytranslate[x] : 117)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,    61,     2,     2,     2,     2,     2,
    59,    55,    52,    51,    54,    14,    60,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,    13,    50,     2,
    58,    53,     2,    62,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
    15,     2,    57,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,    48,    56,    49,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     1,     3,     4,     5,     6,
     7,     8,     9,    10,    11,    12,    16,    17,    18,    19,
    20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
    30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
    40,    41,    42,    43,    44,    45,    46,    47
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     6,     9,    12,    15,    22,    28,    34,    35,    38,
    39,    42,    45,    46,    52,    56,    60,    61,    65,    66,
    70,    71,    75,    77,    79,    81,    83,    85,    87,    89,
    96,   100,   104,   111,   115,   119,   120,   123,   125,   127,
   128,   130,   131,   133,   138,   141,   149,   150,   154,   157,
   161,   165,   169,   173,   176,   179,   180,   182,   184,   190,
   192,   197,   200,   202,   206,   209,   210,   212,   214,   217,
   221,   224,   229,   235,   240,   242,   244,   246,   249,   252,
   254,   256,   258,   260,   263,   266,   271,   280,   287,   298,
   300,   302,   304,   306,   308,   310,   312,   314,   317,   321,
   328,   330,   333,   335,   339,   341,   345,   350,   354,   360,
   365,   370,   377,   383,   386,   393,   395,   399,   402,   405,
   406,   408,   412,   415,   418,   421,   422,   424,   427,   430,
   433,   436,   440,   443,   446,   448,   451,   453,   456,   459,
   462,   465,   468,   471,   474,   477,   480,   483,   486,   489,
   492,   495,   498,   501,   504,   507,   513,   517,   520,   524,
   528,   530,   533,   539,   543,   545
};

static const short yyrhs[] = {    69,
    68,    70,    71,    72,     0,    64,    67,     0,    65,    67,
     0,    66,    67,     0,    22,    48,    67,    87,    67,    49,
     0,    23,    48,    67,   101,    49,     0,    24,    48,    67,
   106,    49,     0,     0,    67,     3,     0,     0,    68,     4,
     0,    68,     3,     0,     0,    20,    67,    10,    67,    50,
     0,    20,     1,   115,     0,    20,     1,    50,     0,     0,
    70,    74,    68,     0,     0,    71,    75,    68,     0,     0,
    72,    73,    68,     0,    87,     0,    80,     0,    83,     0,
    84,     0,   114,     0,   112,     0,   113,     0,    16,    67,
    77,    67,    78,    50,     0,    16,     1,   115,     0,    16,
     1,    50,     0,    21,    67,    76,    77,    67,    50,     0,
    21,     1,   115,     0,    21,     1,    50,     0,     0,    11,
     3,     0,    10,     0,    45,     0,     0,    79,     0,     0,
    82,     0,    79,    51,    67,    82,     0,    79,     1,     0,
    18,    67,    79,    48,    67,    81,    49,     0,     0,    81,
    87,    67,     0,    11,    67,     0,    17,     1,   115,     0,
    17,     1,    50,     0,    19,     1,   115,     0,    19,     1,
    50,     0,    52,    67,     0,    53,    67,     0,     0,    54,
     0,    52,     0,    88,    48,    67,   101,    49,     0,    89,
     0,    88,    51,    67,    89,     0,    88,     1,     0,    91,
     0,    89,    85,    91,     0,    89,     1,     0,     0,    55,
     0,    11,     0,    92,    67,     0,    92,    93,    67,     0,
    93,    67,     0,    90,    56,    92,    67,     0,    90,    56,
    92,    93,    67,     0,    90,    56,    93,    67,     0,    11,
     0,    55,     0,    94,     0,    93,    94,     0,    93,     1,
     0,    12,     0,    95,     0,    97,     0,   100,     0,    14,
    11,     0,    11,    67,     0,    15,    67,    96,    57,     0,
    15,    67,    96,    98,    67,    99,    67,    57,     0,    15,
    67,    90,    56,    96,    57,     0,    15,    67,    90,    56,
    96,    98,    67,    99,    67,    57,     0,    58,     0,     5,
     0,     6,     0,     7,     0,     8,     0,     9,     0,    11,
     0,    10,     0,    13,    11,     0,    13,    13,    11,     0,
    13,    46,    67,    91,    67,    59,     0,   103,     0,   102,
   103,     0,   102,     0,     1,   116,     1,     0,     1,     0,
   103,    50,    67,     0,   103,   116,    50,    67,     0,     1,
    50,    67,     0,     1,   116,     1,    50,    67,     0,   102,
   103,    50,    67,     0,   102,     1,    50,    67,     0,   102,
     1,   116,     1,    50,    67,     0,   104,    13,    67,   106,
   105,     0,   104,     1,     0,   104,    13,    67,     1,   106,
   105,     0,   105,     0,   104,    13,    67,     0,    11,    67,
     0,    25,    67,     0,     0,   108,     0,   106,   107,   108,
     0,   106,     1,     0,    60,    67,     0,    51,    67,     0,
     0,   109,     0,    86,   109,     0,    10,    67,     0,    11,
    67,     0,    42,    67,     0,    86,    42,    67,     0,    45,
    67,     0,    47,    67,     0,   111,     0,    61,    67,     0,
   110,     0,    44,    67,     0,    43,    67,     0,    29,    67,
     0,    30,    67,     0,    31,    67,     0,    32,    67,     0,
    33,    67,     0,    34,    67,     0,    35,    67,     0,    36,
    67,     0,    37,    67,     0,    38,    67,     0,    39,    67,
     0,    40,    67,     0,    41,    67,     0,    27,    67,     0,
    26,    67,     0,    28,    67,     0,    46,    67,   106,    59,
    67,     0,    46,    67,     1,     0,    12,    67,     0,    62,
     1,   115,     0,    62,     1,    50,     0,    74,     0,     1,
   115,     0,    48,     1,   116,     1,    49,     0,    48,     1,
    49,     0,   115,     0,   116,     1,   115,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   238,   240,   241,   242,   245,   252,   258,   283,   285,   288,
   290,   291,   294,   296,   301,   302,   305,   307,   317,   319,
   322,   324,   334,   336,   337,   338,   339,   340,   341,   344,
   357,   360,   365,   374,   375,   378,   380,   383,   385,   388,
   392,   396,   400,   404,   409,   415,   429,   431,   440,   462,
   466,   471,   475,   480,   482,   483,   486,   488,   491,   510,
   522,   536,   542,   546,   575,   581,   583,   584,   587,   592,
   597,   602,   609,   618,   629,   646,   651,   655,   665,   671,
   681,   682,   683,   686,   698,   718,   724,   730,   738,   749,
   753,   756,   759,   762,   765,   770,   772,   775,   789,   796,
   805,   809,   814,   817,   823,   831,   835,   838,   844,   850,
   855,   861,   869,   892,   896,   904,   909,   916,   923,   925,
   928,   933,   946,   952,   956,   959,   964,   966,   967,   968,
   975,   976,   977,   978,   979,   980,   982,   987,   989,   990,
   991,   992,   993,   994,   995,   996,   997,   998,   999,  1000,
  1001,  1002,  1003,  1004,  1005,  1009,  1017,  1032,  1039,  1046,
  1054,  1064,  1090,  1092,  1095,  1097
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","WHITESPACE",
"SGML_CD","INCLUDES","DASHMATCH","BEGINSWITH","ENDSWITH","CONTAINS","STRING",
"IDENT","HASH","':'","'.'","'['","IMPORT_SYM","PAGE_SYM","MEDIA_SYM","FONT_FACE_SYM",
"CHARSET_SYM","NAMESPACE_SYM","KHTML_RULE_SYM","KHTML_DECLS_SYM","KHTML_VALUE_SYM",
"IMPORTANT_SYM","QEMS","EMS","EXS","PXS","CMS","MMS","INS","PTS","PCS","DEGS",
"RADS","GRADS","MSECS","SECS","HERZ","KHERZ","DIMEN","PERCENTAGE","NUMBER","URI",
"FUNCTION","UNICODERANGE","'{'","'}'","';'","','","'+'","'>'","'-'","'*'","'|'",
"']'","'='","')'","'/'","'#'","'@'","stylesheet","khtml_rule","khtml_decls",
"khtml_value","maybe_space","maybe_sgml","maybe_charset","import_list","namespace_list",
"rule_list","rule","import","namespace","maybe_ns_prefix","string_or_uri","maybe_media_list",
"media_list","media","ruleset_list","medium","page","font_face","combinator",
"unary_operator","ruleset","selector_list","selector","namespace_selector","simple_selector",
"element_name","specifier_list","specifier","class","attrib_id","attrib","match",
"ident_or_string","pseudo","declaration_list","decl_list","declaration","property",
"prio","expr","operator","term","unary_term","function","hexcolor","invalid_at",
"invalid_import","invalid_rule","invalid_block","invalid_block_list", NULL
};
#endif

static const short yyr1[] = {     0,
    63,    63,    63,    63,    64,    65,    66,    67,    67,    68,
    68,    68,    69,    69,    69,    69,    70,    70,    71,    71,
    72,    72,    73,    73,    73,    73,    73,    73,    73,    74,
    74,    74,    75,    75,    75,    76,    76,    77,    77,    78,
    78,    79,    79,    79,    79,    80,    81,    81,    82,    83,
    83,    84,    84,    85,    85,    85,    86,    86,    87,    88,
    88,    88,    89,    89,    89,    90,    90,    90,    91,    91,
    91,    91,    91,    91,    92,    92,    93,    93,    93,    94,
    94,    94,    94,    95,    96,    97,    97,    97,    97,    98,
    98,    98,    98,    98,    98,    99,    99,   100,   100,   100,
   101,   101,   101,   101,   101,   102,   102,   102,   102,   102,
   102,   102,   103,   103,   103,   103,   103,   104,   105,   105,
   106,   106,   106,   107,   107,   107,   108,   108,   108,   108,
   108,   108,   108,   108,   108,   108,   108,   109,   109,   109,
   109,   109,   109,   109,   109,   109,   109,   109,   109,   109,
   109,   109,   109,   109,   109,   110,   110,   111,   112,   112,
   113,   114,   115,   115,   116,   116
};

static const short yyr2[] = {     0,
     5,     2,     2,     2,     6,     5,     5,     0,     2,     0,
     2,     2,     0,     5,     3,     3,     0,     3,     0,     3,
     0,     3,     1,     1,     1,     1,     1,     1,     1,     6,
     3,     3,     6,     3,     3,     0,     2,     1,     1,     0,
     1,     0,     1,     4,     2,     7,     0,     3,     2,     3,
     3,     3,     3,     2,     2,     0,     1,     1,     5,     1,
     4,     2,     1,     3,     2,     0,     1,     1,     2,     3,
     2,     4,     5,     4,     1,     1,     1,     2,     2,     1,
     1,     1,     1,     2,     2,     4,     8,     6,    10,     1,
     1,     1,     1,     1,     1,     1,     1,     2,     3,     6,
     1,     2,     1,     3,     1,     3,     4,     3,     5,     4,
     4,     6,     5,     2,     6,     1,     3,     2,     2,     0,
     1,     3,     2,     2,     2,     0,     1,     2,     2,     2,
     2,     3,     2,     2,     1,     2,     1,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     5,     3,     2,     3,     3,
     1,     2,     5,     3,     1,     3
};

static const short yydefact[] = {    13,
     0,     0,     0,     0,     8,     8,     8,    10,     0,     0,
     8,     8,     8,     2,     3,     4,    17,     0,    16,    15,
     9,     8,    66,     0,     0,    12,    11,    19,     0,     0,
    75,    80,     0,     0,     8,    76,     8,     0,     0,     0,
    63,     8,     0,    77,    81,    82,    83,   105,     8,     8,
     0,     0,   101,     0,   116,     8,     8,     8,     8,     8,
     8,     8,     8,     8,     8,     8,     8,     8,     8,     8,
     8,     8,     8,     8,     8,     8,     8,     8,     8,     8,
    58,    57,     8,     0,     0,   121,   127,   137,   135,     0,
    21,    10,   164,   165,     0,    14,    98,     0,     8,    84,
    66,     0,    62,     8,     8,    65,     8,     8,    66,     0,
    69,     0,    79,    71,    78,     8,     0,   118,   119,     6,
     0,   102,     8,     0,   114,     8,   129,   130,   158,   154,
   153,   155,   140,   141,   142,   143,   144,   145,   146,   147,
   148,   149,   150,   151,   152,   131,   139,   138,   133,     0,
   134,   136,     8,   128,   123,     7,     8,     8,     0,     0,
     0,     0,     0,    10,    18,     0,    99,    66,     8,    67,
     0,     0,     5,     0,    66,    54,    55,    64,    75,    76,
     8,     0,    70,   108,   104,     8,     0,     8,   106,     0,
     8,     0,   157,     0,   132,   125,   124,   122,    32,    31,
    38,    39,     8,     0,    36,     0,     0,     8,     0,     0,
    10,   161,    24,    25,    26,    23,    28,    29,    27,    20,
   163,   166,     8,    85,     0,    91,    92,    93,    94,    95,
    86,    90,     8,     0,     0,    72,     0,    74,     8,   111,
     0,   110,   107,     0,     0,     8,    42,    35,    34,     0,
     0,   162,     0,    42,     0,     0,    22,     0,     8,     0,
     0,    59,    73,   109,     8,     0,   113,   156,     8,     0,
     0,    43,    37,     8,    51,    50,     0,    53,    52,   160,
   159,   100,    88,     8,    97,    96,     8,   112,   115,    49,
    30,    45,     8,     0,     8,     0,     0,     0,    33,    47,
     8,    87,    44,    66,     0,    46,     8,    89,    48,     0,
     0,     0
};

static const short yydefgoto[] = {   310,
     5,     6,     7,   224,    17,     8,    28,    91,   163,   211,
    92,   164,   251,   203,   270,   271,   213,   304,   272,   214,
   215,   109,    84,    37,    38,    39,    40,    41,    42,    43,
    44,    45,   172,    46,   233,   287,    47,    51,    52,    53,
    54,    55,    85,   159,    86,    87,    88,    89,   217,   218,
   219,    94,    95
};

static const short yypact[] = {   172,
    22,   -32,   -19,   -15,-32768,-32768,-32768,-32768,    73,    14,
-32768,-32768,-32768,    37,    37,    37,   106,    43,-32768,-32768,
-32768,-32768,   136,   222,   534,-32768,-32768,    31,   140,     4,
    -6,-32768,   130,    42,-32768,    49,-32768,    51,   229,    58,
-32768,   276,   202,-32768,-32768,-32768,-32768,   150,-32768,-32768,
    75,   115,   156,    13,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,   608,   390,-32768,-32768,-32768,-32768,   395,
   116,-32768,-32768,-32768,   145,-32768,-32768,   151,-32768,-32768,
    35,   135,-32768,-32768,-32768,-32768,-32768,-32768,   290,   348,
    37,   202,-32768,    37,-32768,-32768,   170,    37,    37,-32768,
   176,   127,-32768,    38,-32768,-32768,    37,    37,    37,    37,
    37,    37,    37,    37,    37,    37,    37,    37,    37,    37,
    37,    37,    37,    37,    37,    37,    37,    37,    37,   489,
    37,    37,-32768,-32768,-32768,-32768,-32768,-32768,   572,   179,
   183,   264,   117,-32768,   106,   163,-32768,   136,    -6,-32768,
   152,    87,-32768,   222,   136,    37,    37,-32768,-32768,-32768,
   276,   202,    37,    37,   187,-32768,   200,-32768,    37,   191,
-32768,   338,-32768,   437,    37,    37,    37,-32768,-32768,-32768,
-32768,-32768,-32768,   212,    17,   191,   219,-32768,   230,   250,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,   106,
-32768,-32768,-32768,    37,   241,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,   207,   541,    37,   202,    37,-32768,    37,
   228,    37,    37,   572,   286,-32768,   171,-32768,-32768,   256,
   165,-32768,   245,    32,   260,   294,   106,    24,-32768,   161,
   109,-32768,    37,    37,-32768,   286,-32768,    37,-32768,   213,
    50,-32768,-32768,-32768,-32768,-32768,   112,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,    37,-32768,    37,
-32768,-32768,-32768,   105,-32768,   109,    28,    32,-32768,    37,
-32768,-32768,-32768,   340,    34,-32768,-32768,-32768,    37,   266,
   269,-32768
};

static const short yypgoto[] = {-32768,
-32768,-32768,-32768,    -1,   -84,-32768,-32768,-32768,-32768,-32768,
   144,-32768,-32768,    48,-32768,    25,-32768,-32768,    45,-32768,
-32768,-32768,-32768,  -162,-32768,   181,   257,   -87,   247,   -23,
   -28,-32768,   168,-32768,   134,   101,-32768,   232,-32768,   352,
-32768,  -236,  -147,-32768,   248,   324,-32768,-32768,-32768,-32768,
-32768,    -7,   -35
};


#define	YYLAST		652


static const short yytable[] = {    10,
   216,    20,   194,    14,    15,    16,    21,   165,   267,    23,
    24,    25,   117,   125,   115,    11,    21,   124,   112,    21,
    30,   178,     9,    22,    -8,   126,    21,   250,    12,   289,
    21,    -8,    13,   101,    21,   102,    21,    21,   190,    21,
   111,   114,   269,    29,   245,   169,    90,   118,   119,   -68,
   292,   103,   100,    96,   127,   128,   129,   130,   131,   132,
   133,   134,   135,   136,   137,   138,   139,   140,   141,   142,
   143,   144,   145,   146,   147,   148,   149,   150,   151,   220,
   223,   152,   282,   115,   302,   187,   182,   191,   161,   170,
   308,   226,   227,   228,   229,   230,   266,   168,   104,   -41,
   293,   105,   174,   175,   -67,   176,   177,    21,    26,    27,
   183,    21,   292,   110,   184,   121,    -1,   206,   285,   286,
    18,   189,    19,   120,   192,    49,   257,    31,    32,    33,
    34,    35,    90,   207,   208,   209,   162,    21,    21,    50,
    97,   307,    98,   231,   232,   166,    31,    32,    33,    34,
    35,   195,   200,   115,   299,   196,   197,   237,   222,   295,
   205,   167,   293,  -103,  -120,   226,   227,   228,   229,   230,
   185,    36,   -66,    21,   201,    99,   188,   222,   210,   236,
   238,   269,   222,   173,   240,    21,   242,    18,    93,   243,
    36,     1,   201,     2,     3,     4,   249,    18,   252,   116,
   241,   247,   113,    18,    -8,   123,   254,   225,   115,   202,
    18,   221,    -8,    32,    33,    34,    35,   283,   232,   253,
   -40,   258,    48,    18,    21,   186,    18,   202,   199,   106,
   255,   261,    49,   222,    18,   263,   239,   264,    18,   -56,
   -56,   -56,   -56,   -56,   268,   276,    50,   279,   281,    -8,
   256,   259,    -8,    -8,    -8,   262,    -8,    -8,   273,    18,
    -8,   248,   291,   288,   204,   311,    -8,   290,   312,  -120,
  -120,  -120,   294,    -8,    -8,    18,   -60,   265,   277,   -60,
   107,   108,   296,   -56,   -56,   297,   155,    32,    33,    34,
    35,   298,    18,   300,   275,  -126,  -126,  -126,   274,   305,
    31,    32,    33,    34,    35,   309,   212,    18,    -8,   278,
    50,  -126,  -126,  -126,  -126,  -126,  -126,  -126,  -126,  -126,
  -126,  -126,  -126,  -126,  -126,  -126,  -126,  -126,  -126,  -126,
  -126,  -126,  -126,  -120,  -120,  -120,   157,  -126,   244,  -126,
    21,    18,   303,   280,    36,   158,  -126,    56,    57,    58,
    31,    32,    33,    34,    35,   235,   181,   171,   179,    32,
    33,    34,    35,    59,    60,    61,    62,    63,    64,    65,
    66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
    76,    77,    78,    79,    80,  -117,  -117,  -117,   306,    81,
   155,    82,   260,   284,    36,   160,   301,    -8,    83,  -126,
  -126,  -126,   180,   122,    -8,   234,   198,   154,     0,     0,
     0,     0,     0,     0,     0,  -126,  -126,  -126,  -126,  -126,
  -126,  -126,  -126,  -126,  -126,  -126,  -126,  -126,  -126,  -126,
  -126,  -126,  -126,  -126,  -126,  -126,  -126,   155,   156,    -8,
   157,  -126,     0,  -126,     0,     0,  -126,  -126,  -126,   158,
  -126,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,  -126,  -126,  -126,  -126,  -126,  -126,  -126,  -126,
  -126,  -126,  -126,  -126,  -126,  -126,  -126,  -126,  -126,  -126,
  -126,  -126,  -126,  -126,     0,     0,     0,   157,  -126,   193,
  -126,    21,     0,     0,     0,   246,   158,  -126,    56,    57,
    58,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,    59,    60,    61,    62,    63,    64,
    65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
    75,    76,    77,    78,    79,    80,    21,     0,     0,     0,
    81,   106,    82,    56,    57,    58,     0,     0,     0,    83,
     0,   -56,   -56,   -56,   -56,   -56,     0,     0,     0,    59,
    60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
    70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
    80,    56,    57,    58,     0,    81,     0,    82,   -61,     0,
     0,   -61,   107,   108,    83,   -56,   -56,    59,    60,    61,
    62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
    72,    73,    74,    75,    76,    77,    78,    79,    80,     0,
     0,     0,     0,    81,     0,    82,     0,     0,     0,     0,
     0,     0,    83,    59,    60,    61,    62,    63,    64,    65,
    66,    67,    68,    69,    70,    71,    72,    73,    74,   153,
    76,    77
};

static const short yycheck[] = {     1,
   163,     9,   150,     5,     6,     7,     3,    92,   245,    11,
    12,    13,    48,     1,    43,    48,     3,    53,    42,     3,
    22,   109,     1,    10,     3,    13,     3,    11,    48,   266,
     3,    10,    48,    35,     3,    37,     3,     3,     1,     3,
    42,    43,    11,     1,   192,    11,    16,    49,    50,    56,
     1,     1,    11,    50,    56,    57,    58,    59,    60,    61,
    62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
    72,    73,    74,    75,    76,    77,    78,    79,    80,   164,
   168,    83,    59,   112,    57,   121,   110,    50,    90,    55,
    57,     5,     6,     7,     8,     9,   244,    99,    48,    50,
    51,    51,   104,   105,    56,   107,   108,     3,     3,     4,
   112,     3,     1,    56,   116,     1,     0,     1,    10,    11,
    48,   123,    50,    49,   126,    11,   211,    11,    12,    13,
    14,    15,    16,    17,    18,    19,    21,     3,     3,    25,
    11,   304,    13,    57,    58,     1,    11,    12,    13,    14,
    15,   153,   160,   182,    50,   157,   158,   181,   166,    48,
   162,    11,    51,    49,    50,     5,     6,     7,     8,     9,
     1,    55,    56,     3,    10,    46,    50,   185,    62,   181,
   182,    11,   190,    49,   186,     3,   188,    48,    49,   191,
    55,    20,    10,    22,    23,    24,   204,    48,   206,    50,
     1,   203,     1,    48,     3,    50,   208,    56,   237,    45,
    48,    49,    11,    12,    13,    14,    15,    57,    58,     1,
    50,   223,     1,    48,     3,    50,    48,    45,    50,     1,
     1,   233,    11,   241,    48,   237,    50,   239,    48,    11,
    12,    13,    14,    15,   246,   253,    25,   255,   256,    48,
     1,    11,    51,    52,    53,    49,    55,    56,     3,    48,
    59,    50,    50,   265,     1,     0,     3,   269,     0,    48,
    49,    50,   274,    10,    11,    48,    48,    50,   254,    51,
    52,    53,   284,    55,    56,   287,     1,    12,    13,    14,
    15,   293,    48,   295,    50,    10,    11,    12,   251,   301,
    11,    12,    13,    14,    15,   307,   163,    48,    45,    50,
    25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
    35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
    45,    46,    47,    48,    49,    50,    51,    52,     1,    54,
     3,    48,   298,    50,    55,    60,    61,    10,    11,    12,
    11,    12,    13,    14,    15,   175,   110,   101,    11,    12,
    13,    14,    15,    26,    27,    28,    29,    30,    31,    32,
    33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
    43,    44,    45,    46,    47,    48,    49,    50,    49,    52,
     1,    54,   225,   260,    55,     1,   296,     3,    61,    10,
    11,    12,    55,    52,    10,   174,   159,    84,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    26,    27,    28,    29,    30,
    31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
    41,    42,    43,    44,    45,    46,    47,     1,    49,    45,
    51,    52,    -1,    54,    -1,    -1,    10,    11,    12,    60,
    61,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    26,    27,    28,    29,    30,    31,    32,    33,
    34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
    44,    45,    46,    47,    -1,    -1,    -1,    51,    52,     1,
    54,     3,    -1,    -1,    -1,    59,    60,    61,    10,    11,
    12,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    26,    27,    28,    29,    30,    31,
    32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
    42,    43,    44,    45,    46,    47,     3,    -1,    -1,    -1,
    52,     1,    54,    10,    11,    12,    -1,    -1,    -1,    61,
    -1,    11,    12,    13,    14,    15,    -1,    -1,    -1,    26,
    27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
    37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
    47,    10,    11,    12,    -1,    52,    -1,    54,    48,    -1,
    -1,    51,    52,    53,    61,    55,    56,    26,    27,    28,
    29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
    39,    40,    41,    42,    43,    44,    45,    46,    47,    -1,
    -1,    -1,    -1,    52,    -1,    54,    -1,    -1,    -1,    -1,
    -1,    -1,    61,    26,    27,    28,    29,    30,    31,    32,
    33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
    43,    44
};
#define YYPURE 1

/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/share/bison.simple"
/* This file comes from bison-1.28.  */

/* Skeleton output parser for bison,
   Copyright (C) 1984, 1989, 1990 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* This is the parser code that is written into each bison parser
  when the %semantic_parser declaration is not specified in the grammar.
  It was written by Richard Stallman by simplifying the hairy parser
  used when %semantic_parser is specified.  */

#ifndef YYSTACK_USE_ALLOCA
#ifdef alloca
#define YYSTACK_USE_ALLOCA
#else /* alloca not defined */
#ifdef __GNUC__
#define YYSTACK_USE_ALLOCA
#define alloca __builtin_alloca
#else /* not GNU C.  */
#if (!defined (__STDC__) && defined (sparc)) || defined (__sparc__) || defined (__sparc) || defined (__sgi) || (defined (__sun) && defined (__i386))
#define YYSTACK_USE_ALLOCA
#include <alloca.h>
#else /* not sparc */
/* We think this test detects Watcom and Microsoft C.  */
/* This used to test MSDOS, but that is a bad idea
   since that symbol is in the user namespace.  */
#if (defined (_MSDOS) || defined (_MSDOS_)) && !defined (__TURBOC__)
#if 0 /* No need for malloc.h, which pollutes the namespace;
	 instead, just don't use alloca.  */
#include <malloc.h>
#endif
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
/* I don't know what this was needed for, but it pollutes the namespace.
   So I turned it off.   rms, 2 May 1997.  */
/* #include <malloc.h>  */
 #pragma alloca
#define YYSTACK_USE_ALLOCA
#else /* not MSDOS, or __TURBOC__, or _AIX */
#if 0
#ifdef __hpux /* haible@ilog.fr says this works for HPUX 9.05 and up,
		 and on HPUX 10.  Eventually we can turn this on.  */
#define YYSTACK_USE_ALLOCA
#define alloca __builtin_alloca
#endif /* __hpux */
#endif
#endif /* not _AIX */
#endif /* not MSDOS, or __TURBOC__ */
#endif /* not sparc */
#endif /* not GNU C */
#endif /* alloca not defined */
#endif /* YYSTACK_USE_ALLOCA not defined */

#ifdef YYSTACK_USE_ALLOCA
#define YYSTACK_ALLOC alloca
#else
#define YYSTACK_ALLOC malloc
#endif

/* Note: there must be only one dollar sign in this file.
   It is replaced by the list of actions, each action
   as one case of the switch.  */

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	goto yyacceptlab
#define YYABORT 	goto yyabortlab
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.
   This remains here temporarily to ease the
   transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(token, value) \
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    { yychar = (token), yylval = (value);			\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { yyerror ("syntax error: cannot back up"); YYERROR; }	\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

#ifndef YYPURE
#define YYLEX		yylex()
#endif

#ifdef YYPURE
#ifdef YYLSP_NEEDED
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, &yylloc, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval, &yylloc)
#endif
#else /* not YYLSP_NEEDED */
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval)
#endif
#endif /* not YYLSP_NEEDED */
#endif

/* If nonreentrant, generate the variables here */

#ifndef YYPURE

int	yychar;			/*  the lookahead symbol		*/
YYSTYPE	yylval;			/*  the semantic value of the		*/
				/*  lookahead symbol			*/

#ifdef YYLSP_NEEDED
YYLTYPE yylloc;			/*  location data for the lookahead	*/
				/*  symbol				*/
#endif

int yynerrs;			/*  number of parse errors so far       */
#endif  /* not YYPURE */

#if YYDEBUG != 0
int yydebug;			/*  nonzero means print parse trace	*/
/* Since this is uninitialized, it does not stop multiple parsers
   from coexisting.  */
#endif

/*  YYINITDEPTH indicates the initial size of the parser's stacks	*/

#ifndef	YYINITDEPTH
#define YYINITDEPTH 200
#endif

/*  YYMAXDEPTH is the maximum size the stacks can grow to
    (effective only if the built-in stack extension method is used).  */

#if YYMAXDEPTH == 0
#undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif

/* Define __yy_memcpy.  Note that the size argument
   should be passed with type unsigned int, because that is what the non-GCC
   definitions require.  With GCC, __builtin_memcpy takes an arg
   of type size_t, but it can handle unsigned int.  */

#if __GNUC__ > 1		/* GNU C and GNU C++ define this.  */
#define __yy_memcpy(TO,FROM,COUNT)	__builtin_memcpy(TO,FROM,COUNT)
#else				/* not GNU C or C++ */
#ifndef __cplusplus

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (to, from, count)
     char *to;
     char *from;
     unsigned int count;
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#else /* __cplusplus */

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (char *to, char *from, unsigned int count)
{
  register char *t = to;
  register char *f = from;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#endif
#endif

#line 217 "/usr/share/bison.simple"

/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
#ifdef __cplusplus
#define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else /* not __cplusplus */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
#endif /* not __cplusplus */
#else /* not YYPARSE_PARAM */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif /* not YYPARSE_PARAM */

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
#ifdef YYPARSE_PARAM
int yyparse (void *);
#else
int yyparse (void);
#endif
#endif

int
yyparse(YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  register int yystate;
  register int yyn;
  register short *yyssp;
  register YYSTYPE *yyvsp;
  int yyerrstatus;	/*  number of tokens to shift before error messages enabled */
  int yychar1 = 0;		/*  lookahead token as an internal (translated) token number */

  short	yyssa[YYINITDEPTH];	/*  the state stack			*/
  YYSTYPE yyvsa[YYINITDEPTH];	/*  the semantic value stack		*/

  short *yyss = yyssa;		/*  refer to the stacks thru separate pointers */
  YYSTYPE *yyvs = yyvsa;	/*  to allow yyoverflow to reallocate them elsewhere */

#ifdef YYLSP_NEEDED
  YYLTYPE yylsa[YYINITDEPTH];	/*  the location stack			*/
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;

#define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
#define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  int yystacksize = YYINITDEPTH;
  int yyfree_stacks = 0;

#ifdef YYPURE
  int yychar;
  YYSTYPE yylval;
  int yynerrs;
#ifdef YYLSP_NEEDED
  YYLTYPE yylloc;
#endif
#endif

  YYSTYPE yyval;		/*  the variable used to return		*/
				/*  semantic values from the action	*/
				/*  routines				*/

  int yylen;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Starting parse\n");
#endif

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss - 1;
  yyvsp = yyvs;
#ifdef YYLSP_NEEDED
  yylsp = yyls;
#endif

/* Push a new state, which is found in  yystate  .  */
/* In all cases, when you get here, the value and location stacks
   have just been pushed. so pushing a state here evens the stacks.  */
yynewstate:

  *++yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Give user a chance to reallocate the stack */
      /* Use copies of these so that the &'s don't force the real ones into memory. */
      YYSTYPE *yyvs1 = yyvs;
      short *yyss1 = yyss;
#ifdef YYLSP_NEEDED
      YYLTYPE *yyls1 = yyls;
#endif

      /* Get the current used size of the three stacks, in elements.  */
      int size = yyssp - yyss + 1;

#ifdef yyoverflow
      /* Each stack pointer address is followed by the size of
	 the data in use in that stack, in bytes.  */
#ifdef YYLSP_NEEDED
      /* This used to be a conditional around just the two extra args,
	 but that might be undefined if yyoverflow is a macro.  */
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yyls1, size * sizeof (*yylsp),
		 &yystacksize);
#else
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yystacksize);
#endif

      yyss = yyss1; yyvs = yyvs1;
#ifdef YYLSP_NEEDED
      yyls = yyls1;
#endif
#else /* no yyoverflow */
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	{
	  yyerror("parser stack overflow");
	  if (yyfree_stacks)
	    {
	      free (yyss);
	      free (yyvs);
#ifdef YYLSP_NEEDED
	      free (yyls);
#endif
	    }
	  return 2;
	}
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;
#ifndef YYSTACK_USE_ALLOCA
      yyfree_stacks = 1;
#endif
      yyss = (short *) YYSTACK_ALLOC (yystacksize * sizeof (*yyssp));
      __yy_memcpy ((char *)yyss, (char *)yyss1,
		   size * (unsigned int) sizeof (*yyssp));
      yyvs = (YYSTYPE *) YYSTACK_ALLOC (yystacksize * sizeof (*yyvsp));
      __yy_memcpy ((char *)yyvs, (char *)yyvs1,
		   size * (unsigned int) sizeof (*yyvsp));
#ifdef YYLSP_NEEDED
      yyls = (YYLTYPE *) YYSTACK_ALLOC (yystacksize * sizeof (*yylsp));
      __yy_memcpy ((char *)yyls, (char *)yyls1,
		   size * (unsigned int) sizeof (*yylsp));
#endif
#endif /* no yyoverflow */

      yyssp = yyss + size - 1;
      yyvsp = yyvs + size - 1;
#ifdef YYLSP_NEEDED
      yylsp = yyls + size - 1;
#endif

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Stack size increased to %d\n", yystacksize);
#endif

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Entering state %d\n", yystate);
#endif

  goto yybackup;
 yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Reading a token: ");
#endif
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Now at end of input.\n");
#endif
    }
  else
    {
      yychar1 = YYTRANSLATE(yychar);

#if YYDEBUG != 0
      if (yydebug)
	{
	  fprintf (stderr, "Next token is %d (%s", yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise meaning
	     of a token, for further debugging info.  */
#ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
#endif
	  fprintf (stderr, ")\n");
	}
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting token %d (%s), ", yychar, yytname[yychar1]);
#endif

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* count tokens shifted since error; after three, turn off error status.  */
  if (yyerrstatus) yyerrstatus--;

  yystate = yyn;
  goto yynewstate;

/* Do the default action for the current state.  */
yydefault:

  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;

/* Do a reduction.  yyn is the number of a rule to reduce with.  */
yyreduce:
  yylen = yyr2[yyn];
  if (yylen > 0)
    yyval = yyvsp[1-yylen]; /* implement default value of the action */

#if YYDEBUG != 0
  if (yydebug)
    {
      int i;

      fprintf (stderr, "Reducing via rule %d (line %d), ",
	       yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (i = yyprhs[yyn]; yyrhs[i] > 0; i++)
	fprintf (stderr, "%s ", yytname[yyrhs[i]]);
      fprintf (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif


  switch (yyn) {

case 5:
#line 246 "parser.y"
{
        CSSParser *p = static_cast<CSSParser *>(parser);
        p->rule = yyvsp[-2].rule;
    ;
    break;}
case 6:
#line 253 "parser.y"
{
	/* can be empty */
    ;
    break;}
case 7:
#line 259 "parser.y"
{
	CSSParser *p = static_cast<CSSParser *>(parser);
	if ( yyvsp[-1].valueList ) {
	    p->valueList = yyvsp[-1].valueList;
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
    ;
    break;}
case 14:
#line 296 "parser.y"
{
#ifdef CSS_DEBUG
     kdDebug( 6080 ) << "charset rule: " << qString(yyvsp[-2].string) << endl;
#endif
 ;
    break;}
case 18:
#line 307 "parser.y"
{
     CSSParser *p = static_cast<CSSParser *>(parser);
     if ( yyvsp[-1].rule && p->styleElement && p->styleElement->isCSSStyleSheet() ) {
	 p->styleElement->append( yyvsp[-1].rule );
     } else {
	 delete yyvsp[-1].rule;
     }
 ;
    break;}
case 22:
#line 324 "parser.y"
{
     CSSParser *p = static_cast<CSSParser *>(parser);
     if ( yyvsp[-1].rule && p->styleElement && p->styleElement->isCSSStyleSheet() ) {
	 p->styleElement->append( yyvsp[-1].rule );
     } else {
	 delete yyvsp[-1].rule;
     }
 ;
    break;}
case 30:
#line 345 "parser.y"
{
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "@import: " << qString(yyvsp[-3].string) << endl;
#endif
	CSSParser *p = static_cast<CSSParser *>(parser);
	if ( yyvsp[-1].mediaList && p->styleElement && p->styleElement->isCSSStyleSheet() )
	    yyval.rule = new CSSImportRuleImpl( p->styleElement, domString(yyvsp[-3].string), yyvsp[-1].mediaList );
	else {
	    yyval.rule = 0;
            delete yyvsp[-1].mediaList;
        }
    ;
    break;}
case 31:
#line 357 "parser.y"
{
        yyval.rule = 0;
    ;
    break;}
case 32:
#line 360 "parser.y"
{
        yyval.rule = 0;
    ;
    break;}
case 33:
#line 366 "parser.y"
{
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "@namespace: " << qString(yyvsp[-2].string) << endl;
#endif
    CSSParser *p = static_cast<CSSParser *>(parser);
    if (p->styleElement && p->styleElement->isCSSStyleSheet())
        static_cast<CSSStyleSheetImpl*>(p->styleElement)->addNamespace(p, domString(yyvsp[-3].string), domString(yyvsp[-2].string));
;
    break;}
case 36:
#line 379 "parser.y"
{ yyval.string.string = 0; ;
    break;}
case 37:
#line 380 "parser.y"
{ yyval.string = yyvsp[-1].string; ;
    break;}
case 40:
#line 389 "parser.y"
{
        yyval.mediaList = new MediaListImpl();
     ;
    break;}
case 42:
#line 397 "parser.y"
{
	yyval.mediaList = 0;
    ;
    break;}
case 43:
#line 400 "parser.y"
{
	yyval.mediaList = new MediaListImpl();
	yyval.mediaList->appendMedium( domString(yyvsp[0].string).lower() );
    ;
    break;}
case 44:
#line 404 "parser.y"
{
	yyval.mediaList = yyvsp[-3].mediaList;
        if (yyval.mediaList)
	    yyval.mediaList->appendMedium( domString(yyvsp[0].string) );
    ;
    break;}
case 45:
#line 409 "parser.y"
{
        delete yyvsp[-1].mediaList;
        yyval.mediaList = 0;
    ;
    break;}
case 46:
#line 416 "parser.y"
{
	CSSParser *p = static_cast<CSSParser *>(parser);
	if ( yyvsp[-4].mediaList && yyvsp[-1].ruleList &&
	     p->styleElement && p->styleElement->isCSSStyleSheet() ) {
	    yyval.rule = new CSSMediaRuleImpl( static_cast<CSSStyleSheetImpl*>(p->styleElement), yyvsp[-4].mediaList, yyvsp[-1].ruleList );
	} else {
	    yyval.rule = 0;
	    delete yyvsp[-4].mediaList;
	    delete yyvsp[-1].ruleList;
	}
    ;
    break;}
case 47:
#line 430 "parser.y"
{ yyval.ruleList = 0; ;
    break;}
case 48:
#line 431 "parser.y"
{
      yyval.ruleList = yyvsp[-2].ruleList;
      if ( yyvsp[-1].rule ) {
	  if ( !yyval.ruleList ) yyval.ruleList = new CSSRuleListImpl();
	  yyval.ruleList->append( yyvsp[-1].rule );
      }
  ;
    break;}
case 49:
#line 441 "parser.y"
{
      yyval.string = yyvsp[-1].string;
  ;
    break;}
case 50:
#line 463 "parser.y"
{
      yyval.rule = 0;
    ;
    break;}
case 51:
#line 466 "parser.y"
{
      yyval.rule = 0;
    ;
    break;}
case 52:
#line 472 "parser.y"
{
      yyval.rule = 0;
    ;
    break;}
case 53:
#line 475 "parser.y"
{
      yyval.rule = 0;
    ;
    break;}
case 54:
#line 481 "parser.y"
{ yyval.relation = CSSSelector::Sibling; ;
    break;}
case 55:
#line 482 "parser.y"
{ yyval.relation = CSSSelector::Child; ;
    break;}
case 56:
#line 483 "parser.y"
{ yyval.relation = CSSSelector::Descendant; ;
    break;}
case 57:
#line 487 "parser.y"
{ yyval.i = -1; ;
    break;}
case 58:
#line 488 "parser.y"
{ yyval.i = 1; ;
    break;}
case 59:
#line 492 "parser.y"
{
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "got ruleset" << endl << "  selector:" << endl;
#endif
	CSSParser *p = static_cast<CSSParser *>(parser);
	if ( yyvsp[-4].selector ) {
            CSSStyleRuleImpl *rule = new CSSStyleRuleImpl( p->styleElement );
            CSSMutableStyleDeclarationImpl *decl = p->createStyleDeclaration( rule );
            rule->setSelector( yyvsp[-4].selector );
            rule->setDeclaration(decl);
            yyval.rule = rule;
	} else {
	    yyval.rule = 0;
	    p->clearProperties();
	}
    ;
    break;}
case 60:
#line 511 "parser.y"
{
	if ( yyvsp[0].selector ) {
	    yyval.selector = yyvsp[0].selector;
#ifdef CSS_DEBUG
	    kdDebug( 6080 ) << "   got simple selector:" << endl;
	    yyvsp[0].selector->print();
#endif
	} else {
	    yyval.selector = 0;
	}
    ;
    break;}
case 61:
#line 522 "parser.y"
{
	if ( yyvsp[-3].selector && yyvsp[0].selector ) {
	    yyval.selector = yyvsp[-3].selector;
	    yyval.selector->append( yyvsp[0].selector );
#ifdef CSS_DEBUG
	    kdDebug( 6080 ) << "   got simple selector:" << endl;
	    yyvsp[0].selector->print();
#endif
	} else {
            delete yyvsp[-3].selector;
            delete yyvsp[0].selector;
            yyval.selector = 0;
        }
    ;
    break;}
case 62:
#line 536 "parser.y"
{
        delete yyvsp[-1].selector;
        yyval.selector = 0;
    ;
    break;}
case 63:
#line 543 "parser.y"
{
	yyval.selector = yyvsp[0].selector;
    ;
    break;}
case 64:
#line 546 "parser.y"
{
    	yyval.selector = yyvsp[0].selector;
        if (!yyvsp[-2].selector) {
            delete yyvsp[0].selector;
            yyval.selector = 0;
        }
        else if (yyval.selector) {
            CSSSelector *end = yyval.selector;
            while( end->tagHistory )
                end = end->tagHistory;
            end->relation = yyvsp[-1].relation;
            end->tagHistory = yyvsp[-2].selector;
            if ( yyvsp[-1].relation == CSSSelector::Descendant ||
                yyvsp[-1].relation == CSSSelector::Child ) {
                CSSParser *p = static_cast<CSSParser *>(parser);
                DOM::DocumentImpl *doc = p->document();
                if ( doc )
                    doc->setUsesDescendantRules(true);
            }
            else if (yyvsp[-1].relation == CSSSelector::Sibling) {
                CSSParser *p = static_cast<CSSParser *>(parser);
                DOM::DocumentImpl *doc = p->document();
                if (doc)
                    doc->setUsesSiblingRules(true);
            }
        } else {
            delete yyvsp[-2].selector;
        }
    ;
    break;}
case 65:
#line 575 "parser.y"
{
        delete yyvsp[-1].selector;
        yyval.selector = 0;
    ;
    break;}
case 66:
#line 582 "parser.y"
{ yyval.string.string = 0; yyval.string.length = 0; ;
    break;}
case 67:
#line 583 "parser.y"
{ static unsigned short star = '*'; yyval.string.string = &star; yyval.string.length = 1; ;
    break;}
case 68:
#line 584 "parser.y"
{ yyval.string = yyvsp[0].string; ;
    break;}
case 69:
#line 588 "parser.y"
{
	yyval.selector = new CSSSelector();
	yyval.selector->tag = yyvsp[-1].element;
    ;
    break;}
case 70:
#line 592 "parser.y"
{
	yyval.selector = yyvsp[-1].selector;
	if ( yyval.selector )
            yyval.selector->tag = yyvsp[-2].element;
    ;
    break;}
case 71:
#line 597 "parser.y"
{
	yyval.selector = yyvsp[-1].selector;
        if (yyval.selector)
            yyval.selector->tag = makeId(static_cast<CSSParser*>(parser)->defaultNamespace, anyLocalName);;
    ;
    break;}
case 72:
#line 602 "parser.y"
{
        yyval.selector = new CSSSelector();
        yyval.selector->tag = yyvsp[-1].element;
        CSSParser *p = static_cast<CSSParser *>(parser);
        if (p->styleElement && p->styleElement->isCSSStyleSheet())
            static_cast<CSSStyleSheetImpl*>(p->styleElement)->determineNamespace(yyval.selector->tag, domString(yyvsp[-3].string));
    ;
    break;}
case 73:
#line 609 "parser.y"
{
        yyval.selector = yyvsp[-1].selector;
        if (yyval.selector) {
            yyval.selector->tag = yyvsp[-2].element;
            CSSParser *p = static_cast<CSSParser *>(parser);
            if (p->styleElement && p->styleElement->isCSSStyleSheet())
                static_cast<CSSStyleSheetImpl*>(p->styleElement)->determineNamespace(yyval.selector->tag, domString(yyvsp[-4].string));
        }
    ;
    break;}
case 74:
#line 618 "parser.y"
{
        yyval.selector = yyvsp[-1].selector;
        if (yyval.selector) {
            yyval.selector->tag = makeId(anyNamespace, anyLocalName);
            CSSParser *p = static_cast<CSSParser *>(parser);
            if (p->styleElement && p->styleElement->isCSSStyleSheet())
                static_cast<CSSStyleSheetImpl*>(p->styleElement)->determineNamespace(yyval.selector->tag, domString(yyvsp[-3].string));
        }
    ;
    break;}
case 75:
#line 630 "parser.y"
{
	CSSParser *p = static_cast<CSSParser *>(parser);
	DOM::DocumentImpl *doc = p->document();
	QString tag = qString(yyvsp[0].string);
	if ( doc ) {
	    if (doc->isHTMLDocument())
		tag = tag.lower();
	    const DOMString dtag(tag);
            yyval.element = makeId(p->defaultNamespace, doc->tagId(0, dtag.implementation(), false));
	} else {
	    yyval.element = makeId(p->defaultNamespace, khtml::getTagID(tag.lower().ascii(), tag.length()));
	    // this case should never happen - only when loading
	    // the default stylesheet - which must not contain unknown tags
// 	    assert($$ != 0);
	}
    ;
    break;}
case 76:
#line 646 "parser.y"
{
	yyval.element = makeId(static_cast<CSSParser*>(parser)->defaultNamespace, anyLocalName);
    ;
    break;}
case 77:
#line 652 "parser.y"
{
	yyval.selector = yyvsp[0].selector;
    ;
    break;}
case 78:
#line 655 "parser.y"
{
	yyval.selector = yyvsp[-1].selector;
        if (yyval.selector) {
            CSSSelector *end = yyvsp[-1].selector;
            while( end->tagHistory )
                end = end->tagHistory;
            end->relation = CSSSelector::SubSelector;
            end->tagHistory = yyvsp[0].selector;
        }
    ;
    break;}
case 79:
#line 665 "parser.y"
{
        delete yyvsp[-1].selector;
        yyval.selector = 0;
    ;
    break;}
case 80:
#line 672 "parser.y"
{
	yyval.selector = new CSSSelector();
	yyval.selector->match = CSSSelector::Id;
	yyval.selector->attr = ATTR_ID;
        CSSParser *p = static_cast<CSSParser *>(parser);
        if (!p->strict)
            yyvsp[0].string.lower();
	yyval.selector->value = atomicString(yyvsp[0].string);
    ;
    break;}
case 84:
#line 687 "parser.y"
{
        yyval.selector = new CSSSelector();
	yyval.selector->match = CSSSelector::Class;
	yyval.selector->attr = ATTR_CLASS;
        CSSParser *p = static_cast<CSSParser *>(parser);
        if (!p->strict)
            yyvsp[0].string.lower();
	yyval.selector->value = atomicString(yyvsp[0].string);
    ;
    break;}
case 85:
#line 699 "parser.y"
{
	CSSParser *p = static_cast<CSSParser *>(parser);
	DOM::DocumentImpl *doc = p->document();

	QString attr = qString(yyvsp[-1].string);
	if ( doc ) {
	    if (doc->isHTMLDocument())
		attr = attr.lower();
	    const DOMString dattr(attr);
            yyval.attribute = doc->attrId(0, dattr.implementation(), false);
	} else {
	    yyval.attribute = khtml::getAttrID(attr.lower().ascii(), attr.length());
	    // this case should never happen - only when loading
	    // the default stylesheet - which must not contain unknown attributes
	    assert(yyval.attribute != 0);
        }
    ;
    break;}
case 86:
#line 719 "parser.y"
{
	yyval.selector = new CSSSelector();
	yyval.selector->attr = yyvsp[-1].attribute;
	yyval.selector->match = CSSSelector::Set;
    ;
    break;}
case 87:
#line 724 "parser.y"
{
	yyval.selector = new CSSSelector();
	yyval.selector->attr = yyvsp[-5].attribute;
	yyval.selector->match = (CSSSelector::Match)yyvsp[-4].i;
	yyval.selector->value = atomicString(yyvsp[-2].string);
    ;
    break;}
case 88:
#line 730 "parser.y"
{
        yyval.selector = new CSSSelector();
        yyval.selector->attr = yyvsp[-1].attribute;
        yyval.selector->match = CSSSelector::Set;
        CSSParser *p = static_cast<CSSParser *>(parser);
        if (p->styleElement && p->styleElement->isCSSStyleSheet())
            static_cast<CSSStyleSheetImpl*>(p->styleElement)->determineNamespace(yyval.selector->attr, domString(yyvsp[-3].string));
    ;
    break;}
case 89:
#line 738 "parser.y"
{
        yyval.selector = new CSSSelector();
        yyval.selector->attr = yyvsp[-5].attribute;
        yyval.selector->match = (CSSSelector::Match)yyvsp[-4].i;
        yyval.selector->value = atomicString(yyvsp[-2].string);
        CSSParser *p = static_cast<CSSParser *>(parser);
        if (p->styleElement && p->styleElement->isCSSStyleSheet())
            static_cast<CSSStyleSheetImpl*>(p->styleElement)->determineNamespace(yyval.selector->attr, domString(yyvsp[-7].string));
    ;
    break;}
case 90:
#line 750 "parser.y"
{
	yyval.i = CSSSelector::Exact;
    ;
    break;}
case 91:
#line 753 "parser.y"
{
	yyval.i = CSSSelector::List;
    ;
    break;}
case 92:
#line 756 "parser.y"
{
	yyval.i = CSSSelector::Hyphen;
    ;
    break;}
case 93:
#line 759 "parser.y"
{
	yyval.i = CSSSelector::Begin;
    ;
    break;}
case 94:
#line 762 "parser.y"
{
	yyval.i = CSSSelector::End;
    ;
    break;}
case 95:
#line 765 "parser.y"
{
	yyval.i = CSSSelector::Contain;
    ;
    break;}
case 98:
#line 776 "parser.y"
{
        yyval.selector = new CSSSelector();
        yyval.selector->match = CSSSelector::Pseudo;
        yyvsp[0].string.lower();
        yyval.selector->value = atomicString(yyvsp[0].string);
        if (yyval.selector->value == "empty" || yyval.selector->value == "only-child" ||
            yyval.selector->value == "first-child" || yyval.selector->value == "last-child") {
            CSSParser *p = static_cast<CSSParser *>(parser);
            DOM::DocumentImpl *doc = p->document();
            if (doc)
                doc->setUsesSiblingRules(true);
        }
    ;
    break;}
case 99:
#line 790 "parser.y"
{
        yyval.selector = new CSSSelector();
        yyval.selector->match = CSSSelector::Pseudo;
        yyvsp[0].string.lower();
        yyval.selector->value = atomicString(yyvsp[0].string);
    ;
    break;}
case 100:
#line 796 "parser.y"
{
        yyval.selector = new CSSSelector();
        yyval.selector->match = CSSSelector::Pseudo;
        yyval.selector->simpleSelector = yyvsp[-2].selector;
        yyvsp[-4].string.lower();
        yyval.selector->value = atomicString(yyvsp[-4].string);
    ;
    break;}
case 101:
#line 806 "parser.y"
{
	yyval.ok = yyvsp[0].ok;
    ;
    break;}
case 102:
#line 809 "parser.y"
{
	yyval.ok = yyvsp[-1].ok;
	if ( yyvsp[0].ok )
	    yyval.ok = yyvsp[0].ok;
    ;
    break;}
case 103:
#line 814 "parser.y"
{
	yyval.ok = yyvsp[0].ok;
    ;
    break;}
case 104:
#line 817 "parser.y"
{
	yyval.ok = false;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    ;
    break;}
case 105:
#line 823 "parser.y"
{
	yyval.ok = false;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping all declarations" << endl;
#endif
    ;
    break;}
case 106:
#line 832 "parser.y"
{
	yyval.ok = yyvsp[-2].ok;
    ;
    break;}
case 107:
#line 835 "parser.y"
{
        yyval.ok = false;
    ;
    break;}
case 108:
#line 838 "parser.y"
{
	yyval.ok = false;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    ;
    break;}
case 109:
#line 844 "parser.y"
{
	yyval.ok = false;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    ;
    break;}
case 110:
#line 850 "parser.y"
{
	yyval.ok = yyvsp[-3].ok;
	if ( yyvsp[-2].ok )
	    yyval.ok = yyvsp[-2].ok;
    ;
    break;}
case 111:
#line 855 "parser.y"
{
	yyval.ok = yyvsp[-3].ok;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    ;
    break;}
case 112:
#line 861 "parser.y"
{
	yyval.ok = yyvsp[-5].ok;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    ;
    break;}
case 113:
#line 870 "parser.y"
{
	yyval.ok = false;
	CSSParser *p = static_cast<CSSParser *>(parser);
	if ( yyvsp[-4].prop_id && yyvsp[-1].valueList ) {
	    p->valueList = yyvsp[-1].valueList;
#ifdef CSS_DEBUG
	    kdDebug( 6080 ) << "   got property: " << yyvsp[-4].prop_id <<
		(yyvsp[0].b?" important":"")<< endl;
#endif
	    bool ok = p->parseValue( yyvsp[-4].prop_id, yyvsp[0].b );
	    if ( ok )
		yyval.ok = ok;
#ifdef CSS_DEBUG
	    else
		kdDebug( 6080 ) << "     couldn't parse value!" << endl;
#endif
	} else {
            delete yyvsp[-1].valueList;
        }
	delete p->valueList;
	p->valueList = 0;
    ;
    break;}
case 114:
#line 893 "parser.y"
{
        yyval.ok = false;
    ;
    break;}
case 115:
#line 897 "parser.y"
{
        /* The default movable type template has letter-spacing: .none;  Handle this by looking for
        error tokens at the start of an expr, recover the expr and then treat as an error, cleaning
        up and deleting the shifted expr.  */
        delete yyvsp[-1].valueList;
        yyval.ok = false;
    ;
    break;}
case 116:
#line 905 "parser.y"
{
        /* Handle this case: div { text-align: center; !important } Just reduce away the stray !important. */
        yyval.ok = false;
    ;
    break;}
case 117:
#line 910 "parser.y"
{
        /* div { font-family: } Just reduce away this property with no value. */
        yyval.ok = false;
    ;
    break;}
case 118:
#line 917 "parser.y"
{
	QString str = qString(yyvsp[-1].string);
	yyval.prop_id = getPropertyID( str.lower().latin1(), str.length() );
    ;
    break;}
case 119:
#line 924 "parser.y"
{ yyval.b = true; ;
    break;}
case 120:
#line 925 "parser.y"
{ yyval.b = false; ;
    break;}
case 121:
#line 929 "parser.y"
{
	yyval.valueList = new ValueList;
	yyval.valueList->addValue( yyvsp[0].value );
    ;
    break;}
case 122:
#line 933 "parser.y"
{
        yyval.valueList = yyvsp[-2].valueList;
	if ( yyval.valueList ) {
            if ( yyvsp[-1].tok ) {
                Value v;
                v.id = 0;
                v.unit = Value::Operator;
                v.iValue = yyvsp[-1].tok;
                yyval.valueList->addValue( v );
            }
            yyval.valueList->addValue( yyvsp[0].value );
        }
    ;
    break;}
case 123:
#line 946 "parser.y"
{
        delete yyvsp[-1].valueList;
        yyval.valueList = 0;
    ;
    break;}
case 124:
#line 953 "parser.y"
{
	yyval.tok = '/';
    ;
    break;}
case 125:
#line 956 "parser.y"
{
	yyval.tok = ',';
    ;
    break;}
case 126:
#line 959 "parser.y"
{
        yyval.tok = 0;
  ;
    break;}
case 127:
#line 965 "parser.y"
{ yyval.value = yyvsp[0].value; ;
    break;}
case 128:
#line 966 "parser.y"
{ yyval.value = yyvsp[0].value; yyval.value.fValue *= yyvsp[-1].i; ;
    break;}
case 129:
#line 967 "parser.y"
{ yyval.value.id = 0; yyval.value.string = yyvsp[-1].string; yyval.value.unit = CSSPrimitiveValue::CSS_STRING; ;
    break;}
case 130:
#line 968 "parser.y"
{
      QString str = qString( yyvsp[-1].string );
      yyval.value.id = getValueID( str.lower().latin1(), str.length() );
      yyval.value.unit = CSSPrimitiveValue::CSS_IDENT;
      yyval.value.string = yyvsp[-1].string;
  ;
    break;}
case 131:
#line 975 "parser.y"
{ yyval.value.id = 0; yyval.value.string = yyvsp[-1].string; yyval.value.unit = CSSPrimitiveValue::CSS_DIMENSION ;
    break;}
case 132:
#line 976 "parser.y"
{ yyval.value.id = 0; yyval.value.string = yyvsp[-1].string; yyval.value.unit = CSSPrimitiveValue::CSS_DIMENSION ;
    break;}
case 133:
#line 977 "parser.y"
{ yyval.value.id = 0; yyval.value.string = yyvsp[-1].string; yyval.value.unit = CSSPrimitiveValue::CSS_URI; ;
    break;}
case 134:
#line 978 "parser.y"
{ yyval.value.id = 0; yyval.value.iValue = 0; yyval.value.unit = CSSPrimitiveValue::CSS_UNKNOWN;/* ### */ ;
    break;}
case 135:
#line 979 "parser.y"
{ yyval.value.id = 0; yyval.value.string = yyvsp[0].string; yyval.value.unit = CSSPrimitiveValue::CSS_RGBCOLOR; ;
    break;}
case 136:
#line 980 "parser.y"
{ yyval.value.id = 0; yyval.value.string = ParseString(); yyval.value.unit = CSSPrimitiveValue::CSS_RGBCOLOR; ;
    break;}
case 137:
#line 982 "parser.y"
{
      yyval.value = yyvsp[0].value;
  ;
    break;}
case 138:
#line 988 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_NUMBER; ;
    break;}
case 139:
#line 989 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_PERCENTAGE; ;
    break;}
case 140:
#line 990 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_PX; ;
    break;}
case 141:
#line 991 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_CM; ;
    break;}
case 142:
#line 992 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_MM; ;
    break;}
case 143:
#line 993 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_IN; ;
    break;}
case 144:
#line 994 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_PT; ;
    break;}
case 145:
#line 995 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_PC; ;
    break;}
case 146:
#line 996 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_DEG; ;
    break;}
case 147:
#line 997 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_RAD; ;
    break;}
case 148:
#line 998 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_GRAD; ;
    break;}
case 149:
#line 999 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_MS; ;
    break;}
case 150:
#line 1000 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_S; ;
    break;}
case 151:
#line 1001 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_HZ; ;
    break;}
case 152:
#line 1002 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_KHZ; ;
    break;}
case 153:
#line 1003 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_EMS; ;
    break;}
case 154:
#line 1004 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = Value::Q_EMS; ;
    break;}
case 155:
#line 1005 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_EXS; ;
    break;}
case 156:
#line 1010 "parser.y"
{
      Function *f = new Function;
      f->name = yyvsp[-4].string;
      f->args = yyvsp[-2].valueList;
      yyval.value.id = 0;
      yyval.value.unit = Value::Function;
      yyval.value.function = f;
  ;
    break;}
case 157:
#line 1018 "parser.y"
{
      Function *f = new Function;
      f->name = yyvsp[-2].string;
      f->args = 0;
      yyval.value.id = 0;
      yyval.value.unit = Value::Function;
      yyval.value.function = f;
  ;
    break;}
case 158:
#line 1033 "parser.y"
{ yyval.string = yyvsp[-1].string; ;
    break;}
case 159:
#line 1040 "parser.y"
{
	yyval.rule = 0;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipped invalid @-rule" << endl;
#endif
    ;
    break;}
case 160:
#line 1046 "parser.y"
{
	yyval.rule = 0;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipped invalid @-rule" << endl;
#endif
    ;
    break;}
case 161:
#line 1055 "parser.y"
{
        delete yyvsp[0].rule;
        yyval.rule = 0;
#ifdef CSS_DEBUG
        kdDebug( 6080 ) << "skipped invalid import" << endl;
#endif
    ;
    break;}
case 162:
#line 1065 "parser.y"
{
	yyval.rule = 0;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipped invalid rule" << endl;
#endif
    ;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 543 "/usr/share/bison.simple"

  yyvsp -= yylen;
  yyssp -= yylen;
#ifdef YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;

#ifdef YYLSP_NEEDED
  yylsp++;
  if (yylen == 0)
    {
      yylsp->first_line = yylloc.first_line;
      yylsp->first_column = yylloc.first_column;
      yylsp->last_line = (yylsp-1)->last_line;
      yylsp->last_column = (yylsp-1)->last_column;
      yylsp->text = 0;
    }
  else
    {
      yylsp->last_line = (yylsp+yylen-1)->last_line;
      yylsp->last_column = (yylsp+yylen-1)->last_column;
    }
#endif

  /* Now "shift" the result of the reduction.
     Determine what state that goes to,
     based on the state we popped back to
     and the rule number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;

yyerrlab:   /* here on detecting error */

  if (! yyerrstatus)
    /* If not already recovering from an error, report this error.  */
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  int size = 0;
	  char *msg;
	  int x, count;

	  count = 0;
	  /* Start X at -yyn if nec to avoid negative indexes in yycheck.  */
	  for (x = (yyn < 0 ? -yyn : 0);
	       x < (sizeof(yytname) / sizeof(char *)); x++)
	    if (yycheck[x + yyn] == x)
	      size += strlen(yytname[x]) + 15, count++;
	  msg = (char *) malloc(size + 15);
	  if (msg != 0)
	    {
	      strcpy(msg, "parse error");

	      if (count < 5)
		{
		  count = 0;
		  for (x = (yyn < 0 ? -yyn : 0);
		       x < (sizeof(yytname) / sizeof(char *)); x++)
		    if (yycheck[x + yyn] == x)
		      {
			strcat(msg, count == 0 ? ", expecting `" : " or `");
			strcat(msg, yytname[x]);
			strcat(msg, "'");
			count++;
		      }
		}
	      yyerror(msg);
	      free(msg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exceeded");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror("parse error");
    }

  goto yyerrlab1;
yyerrlab1:   /* here on error raised explicitly by an action */

  if (yyerrstatus == 3)
    {
      /* if just tried and failed to reuse lookahead token after an error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Discarding token %d (%s).\n", yychar, yytname[yychar1]);
#endif

      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token
     after shifting the error token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;

yyerrdefault:  /* current state does not do anything special for the error token. */

#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */
  yyn = yydefact[yystate];  /* If its default is to accept any token, ok.  Otherwise pop it.*/
  if (yyn) goto yydefault;
#endif

yyerrpop:   /* pop the current state because it cannot handle the error token */

  if (yyssp == yyss) YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#ifdef YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "Error: state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

yyerrhandle:

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting error token, ");
#endif

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;

 yyacceptlab:
  /* YYACCEPT comes here.  */
  if (yyfree_stacks)
    {
      free (yyss);
      free (yyvs);
#ifdef YYLSP_NEEDED
      free (yyls);
#endif
    }
  return 0;

 yyabortlab:
  /* YYABORT comes here.  */
  if (yyfree_stacks)
    {
      free (yyss);
      free (yyvs);
#ifdef YYLSP_NEEDED
      free (yyls);
#endif
    }
  return 1;
}
#line 1100 "parser.y"


