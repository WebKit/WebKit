
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
#define	S	257
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
#define	KONQ_RULE_SYM	272
#define	KONQ_DECLS_SYM	273
#define	KONQ_VALUE_SYM	274
#define	IMPORTANT_SYM	275
#define	QEMS	276
#define	EMS	277
#define	EXS	278
#define	PXS	279
#define	CMS	280
#define	MMS	281
#define	INS	282
#define	PTS	283
#define	PCS	284
#define	DEGS	285
#define	RADS	286
#define	GRADS	287
#define	MSECS	288
#define	SECS	289
#define	HERZ	290
#define	KHERZ	291
#define	DIMEN	292
#define	PERCENTAGE	293
#define	NUMBER	294
#define	URI	295
#define	FUNCTION	296
#define	UNICODERANGE	297

#line 1 "parser.y"


/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2002-2003 Lars Knoll (knoll@kde.org)
 *  Copyright (c) 2003 Apple Computer
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

#line 84 "parser.y"
typedef union {
    CSSRuleImpl *rule;
    CSSSelector *selector;
    QPtrList<CSSSelector> *selectorList;
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
    char tok;
    Value value;
    ValueList *valueList;
} YYSTYPE;
#line 104 "parser.y"


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



#define	YYFINAL		264
#define	YYFLAG		-32768
#define	YYNTBASE	60

#define YYTRANSLATE(x) ((unsigned)(x) <= 297 ? yytranslate[x] : 109)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
    57,    54,    51,    50,    53,    14,    58,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,    13,    49,     2,
    56,    52,     2,    59,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
    15,     2,    55,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,    47,     2,    48,     2,     2,     2,     2,     2,
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
    40,    41,    42,    43,    44,    45,    46
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     5,     8,    11,    14,    21,    27,    33,    34,    37,
    38,    41,    44,    45,    51,    55,    59,    60,    64,    65,
    69,    71,    73,    75,    77,    79,    81,    88,    92,    96,
    98,   100,   101,   103,   104,   106,   111,   114,   122,   123,
   127,   130,   134,   138,   142,   146,   149,   152,   153,   155,
   157,   163,   165,   170,   173,   175,   179,   182,   185,   189,
   192,   194,   196,   198,   201,   204,   206,   208,   210,   212,
   215,   218,   223,   232,   234,   236,   238,   240,   242,   244,
   246,   248,   251,   255,   262,   264,   267,   269,   273,   275,
   279,   283,   289,   294,   299,   306,   312,   315,   317,   320,
   323,   324,   326,   330,   333,   336,   339,   340,   342,   345,
   348,   351,   354,   357,   359,   361,   364,   367,   370,   373,
   376,   379,   382,   385,   388,   391,   394,   397,   400,   403,
   406,   409,   412,   415,   418,   424,   428,   431,   435,   439,
   442,   448,   452,   454
};

static const short yyrhs[] = {    66,
    65,    67,    68,     0,    61,    64,     0,    62,    64,     0,
    63,    64,     0,    21,    47,    64,    81,    64,    48,     0,
    22,    47,    64,    94,    48,     0,    23,    47,    64,    99,
    48,     0,     0,    64,     3,     0,     0,    65,     4,     0,
    65,     3,     0,     0,    20,    64,    10,    64,    49,     0,
    20,     1,   107,     0,    20,     1,    49,     0,     0,    67,
    70,    65,     0,     0,    68,    69,    65,     0,    81,     0,
    74,     0,    77,     0,    78,     0,   106,     0,   105,     0,
    16,    64,    71,    64,    72,    49,     0,    16,     1,   107,
     0,    16,     1,    49,     0,    10,     0,    44,     0,     0,
    73,     0,     0,    76,     0,    73,    50,    64,    76,     0,
    73,     1,     0,    18,    64,    73,    47,    64,    75,    48,
     0,     0,    75,    81,    64,     0,    11,    64,     0,    17,
     1,   107,     0,    17,     1,    49,     0,    19,     1,   107,
     0,    19,     1,    49,     0,    51,    64,     0,    52,    64,
     0,     0,    53,     0,    51,     0,    82,    47,    64,    94,
    48,     0,    83,     0,    82,    50,    64,    83,     0,    82,
     1,     0,    84,     0,    83,    79,    84,     0,    83,     1,
     0,    85,    64,     0,    85,    86,    64,     0,    86,    64,
     0,    11,     0,    54,     0,    87,     0,    86,    87,     0,
    86,     1,     0,    12,     0,    88,     0,    90,     0,    93,
     0,    14,    11,     0,    11,    64,     0,    15,    64,    89,
    55,     0,    15,    64,    89,    91,    64,    92,    64,    55,
     0,    56,     0,     5,     0,     6,     0,     7,     0,     8,
     0,     9,     0,    11,     0,    10,     0,    13,    11,     0,
    13,    13,    11,     0,    13,    45,    64,    84,    64,    57,
     0,    96,     0,    95,    96,     0,    95,     0,     1,   108,
     1,     0,     1,     0,    96,    49,    64,     0,     1,    49,
    64,     0,     1,   108,     1,    49,    64,     0,    95,    96,
    49,    64,     0,    95,     1,    49,    64,     0,    95,     1,
   108,     1,    49,    64,     0,    97,    13,    64,    99,    98,
     0,    97,     1,     0,    98,     0,    11,    64,     0,    24,
    64,     0,     0,   101,     0,    99,   100,   101,     0,    99,
     1,     0,    58,    64,     0,    50,    64,     0,     0,   102,
     0,    80,   102,     0,    10,    64,     0,    11,    64,     0,
    44,    64,     0,    46,    64,     0,   104,     0,   103,     0,
    43,    64,     0,    42,    64,     0,    28,    64,     0,    29,
    64,     0,    30,    64,     0,    31,    64,     0,    32,    64,
     0,    33,    64,     0,    34,    64,     0,    35,    64,     0,
    36,    64,     0,    37,    64,     0,    38,    64,     0,    39,
    64,     0,    40,    64,     0,    26,    64,     0,    25,    64,
     0,    27,    64,     0,    41,    64,     0,    45,    64,    99,
    57,    64,     0,    45,    64,     1,     0,    12,    64,     0,
    59,     1,   107,     0,    59,     1,    49,     0,     1,   107,
     0,    47,     1,   108,     1,    48,     0,    47,     1,    48,
     0,   107,     0,   108,     1,   107,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   230,   232,   233,   234,   237,   244,   250,   275,   277,   280,
   282,   283,   286,   288,   293,   294,   297,   299,   310,   312,
   322,   324,   325,   326,   327,   328,   331,   344,   347,   352,
   354,   357,   361,   365,   369,   373,   378,   384,   398,   400,
   409,   431,   435,   440,   444,   449,   451,   452,   455,   457,
   460,   480,   494,   508,   514,   518,   537,   543,   548,   553,
   560,   581,   586,   591,   601,   607,   614,   615,   616,   619,
   628,   652,   658,   666,   670,   673,   676,   679,   682,   687,
   689,   692,   698,   704,   712,   716,   721,   724,   730,   738,
   742,   748,   754,   759,   765,   773,   796,   800,   807,   814,
   816,   819,   824,   837,   843,   847,   850,   855,   857,   858,
   859,   865,   866,   867,   869,   874,   876,   877,   878,   879,
   880,   881,   882,   883,   884,   885,   886,   887,   888,   889,
   890,   891,   892,   893,   897,   905,   917,   924,   931,   939,
   965,   967,   970,   972
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","S","SGML_CD",
"INCLUDES","DASHMATCH","BEGINSWITH","ENDSWITH","CONTAINS","STRING","IDENT","HASH",
"':'","'.'","'['","IMPORT_SYM","PAGE_SYM","MEDIA_SYM","FONT_FACE_SYM","CHARSET_SYM",
"KONQ_RULE_SYM","KONQ_DECLS_SYM","KONQ_VALUE_SYM","IMPORTANT_SYM","QEMS","EMS",
"EXS","PXS","CMS","MMS","INS","PTS","PCS","DEGS","RADS","GRADS","MSECS","SECS",
"HERZ","KHERZ","DIMEN","PERCENTAGE","NUMBER","URI","FUNCTION","UNICODERANGE",
"'{'","'}'","';'","','","'+'","'>'","'-'","'*'","']'","'='","')'","'/'","'@'",
"stylesheet","konq_rule","konq_decls","konq_value","maybe_space","maybe_sgml",
"maybe_charset","import_list","rule_list","rule","import","string_or_uri","maybe_media_list",
"media_list","media","ruleset_list","medium","page","font_face","combinator",
"unary_operator","ruleset","selector_list","selector","simple_selector","element_name",
"specifier_list","specifier","class","attrib_id","attrib","match","ident_or_string",
"pseudo","declaration_list","decl_list","declaration","property","prio","expr",
"operator","term","unary_term","function","hexcolor","invalid_at","invalid_rule",
"invalid_block","invalid_block_list", NULL
};
#endif

static const short yyr1[] = {     0,
    60,    60,    60,    60,    61,    62,    63,    64,    64,    65,
    65,    65,    66,    66,    66,    66,    67,    67,    68,    68,
    69,    69,    69,    69,    69,    69,    70,    70,    70,    71,
    71,    72,    72,    73,    73,    73,    73,    74,    75,    75,
    76,    77,    77,    78,    78,    79,    79,    79,    80,    80,
    81,    82,    82,    82,    83,    83,    83,    84,    84,    84,
    85,    85,    86,    86,    86,    87,    87,    87,    87,    88,
    89,    90,    90,    91,    91,    91,    91,    91,    91,    92,
    92,    93,    93,    93,    94,    94,    94,    94,    94,    95,
    95,    95,    95,    95,    95,    96,    96,    96,    97,    98,
    98,    99,    99,    99,   100,   100,   100,   101,   101,   101,
   101,   101,   101,   101,   101,   102,   102,   102,   102,   102,
   102,   102,   102,   102,   102,   102,   102,   102,   102,   102,
   102,   102,   102,   102,   103,   103,   104,   105,   105,   106,
   107,   107,   108,   108
};

static const short yyr2[] = {     0,
     4,     2,     2,     2,     6,     5,     5,     0,     2,     0,
     2,     2,     0,     5,     3,     3,     0,     3,     0,     3,
     1,     1,     1,     1,     1,     1,     6,     3,     3,     1,
     1,     0,     1,     0,     1,     4,     2,     7,     0,     3,
     2,     3,     3,     3,     3,     2,     2,     0,     1,     1,
     5,     1,     4,     2,     1,     3,     2,     2,     3,     2,
     1,     1,     1,     2,     2,     1,     1,     1,     1,     2,
     2,     4,     8,     1,     1,     1,     1,     1,     1,     1,
     1,     2,     3,     6,     1,     2,     1,     3,     1,     3,
     3,     5,     4,     4,     6,     5,     2,     1,     2,     2,
     0,     1,     3,     2,     2,     2,     0,     1,     2,     2,
     2,     2,     2,     1,     1,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     5,     3,     2,     3,     3,     2,
     5,     3,     1,     3
};

static const short yydefact[] = {    13,
     0,     0,     0,     0,     8,     8,     8,    10,     0,     0,
     8,     8,     8,     2,     3,     4,    17,     0,    16,    15,
     9,     8,     0,     0,     0,    12,    11,    19,     0,     0,
    61,    66,     0,     0,     8,    62,     8,     0,     0,    55,
     8,     0,    63,    67,    68,    69,    89,     8,     8,     0,
     0,    85,     0,    98,     8,     8,     8,     8,     8,     8,
     8,     8,     8,     8,     8,     8,     8,     8,     8,     8,
     8,     8,     8,     8,     8,     8,     8,     8,     8,    50,
    49,     0,     0,   102,   108,   115,   114,     0,     0,    10,
   142,   143,     0,    14,    82,     0,     8,    70,     0,     0,
    54,     8,     8,    57,     8,     8,     0,    58,     0,    65,
    60,    64,     8,     0,    99,   100,     6,     0,    86,     8,
    97,     8,   110,   111,   137,   132,   131,   133,   118,   119,
   120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
   130,   134,   117,   116,   112,     0,   113,   109,   104,     7,
     8,     8,     0,     0,     0,     0,     0,     8,     0,     0,
    10,    22,    23,    24,    21,    26,    25,    18,     0,    83,
     0,     8,     0,     5,     0,     0,    46,    47,    56,    59,
    91,    88,     8,     0,     8,    90,     0,   136,     0,   106,
   105,   103,    29,    28,    30,    31,     8,   140,     0,    34,
     0,     0,    20,   141,   144,     8,    71,    75,    76,    77,
    78,    79,    72,    74,     8,     0,     0,     8,    94,     0,
    93,     0,     8,    34,    43,    42,     8,     0,    35,    45,
    44,   139,   138,     0,     0,    51,    92,     8,    96,   135,
     0,     0,    41,    37,     8,     8,    84,    81,    80,     8,
    95,    27,    39,     0,     0,     0,    36,    73,    38,     8,
    40,     0,     0,     0
};

static const short yydefgoto[] = {   262,
     5,     6,     7,    10,    17,     8,    28,    89,   161,    90,
   197,   241,   228,   162,   256,   229,   163,   164,   107,    82,
    37,    38,    39,    40,    41,    42,    43,    44,   173,    45,
   215,   250,    46,    50,    51,    52,    53,    54,    83,   153,
    84,    85,    86,    87,   166,   167,    92,    93
};

static const short yypact[] = {   280,
    11,   -28,    -8,    35,-32768,-32768,-32768,-32768,   -31,    19,
-32768,-32768,-32768,    39,    39,    39,   169,    14,-32768,-32768,
-32768,-32768,   127,   155,   460,-32768,-32768,    73,    58,   106,
-32768,-32768,   194,    82,-32768,-32768,-32768,    40,    34,-32768,
   244,   174,-32768,-32768,-32768,-32768,    -9,-32768,-32768,    54,
   150,    64,    12,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,   526,   287,-32768,-32768,-32768,-32768,   199,   110,-32768,
-32768,-32768,   130,-32768,-32768,   148,-32768,-32768,    17,   109,
-32768,-32768,-32768,-32768,-32768,-32768,   238,    39,   174,-32768,
    39,-32768,-32768,   153,    39,    39,-32768,    47,   113,-32768,
-32768,-32768,    39,    39,    39,    39,    39,    39,    39,    39,
    39,    39,    39,    39,    39,    39,    39,    39,    39,    39,
    39,    39,    39,    39,    39,   416,    39,-32768,-32768,-32768,
-32768,-32768,   497,    67,   116,   118,   181,-32768,   193,   196,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,   169,   143,-32768,
   127,-32768,   128,-32768,   155,   127,    39,    39,-32768,    39,
    39,    71,-32768,   210,-32768,    39,   460,-32768,   338,    39,
    39,-32768,-32768,-32768,-32768,-32768,-32768,-32768,   121,    20,
   159,   168,   169,-32768,-32768,-32768,    39,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,   164,   386,-32768,    39,   180,
    39,   236,-32768,    96,-32768,-32768,-32768,   102,-32768,-32768,
-32768,-32768,-32768,    21,    23,-32768,    39,-32768,-32768,    39,
   167,    94,    39,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
    39,-32768,    39,    20,    24,   293,-32768,-32768,-32768,-32768,
    39,   219,   220,-32768
};

static const short yypgoto[] = {-32768,
-32768,-32768,-32768,    -5,   -81,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,    -1,-32768,-32768,   -24,-32768,-32768,-32768,-32768,
   -85,-32768,    56,   -96,-32768,   195,   -32,-32768,-32768,-32768,
-32768,-32768,-32768,    59,-32768,   184,-32768,    16,   -55,-32768,
    89,   162,-32768,-32768,-32768,-32768,    -6,   -42
};


#define	YYLAST		569


static const short yytable[] = {    14,
    15,    16,    20,   165,   114,    23,    24,    25,   168,   112,
   179,     9,   121,    -8,    29,    18,    30,    19,    11,    21,
    -8,    21,    21,    21,   122,    21,    21,   172,    22,    99,
   227,   100,   248,   249,   104,   108,   111,    18,    12,   113,
   101,    21,   115,   116,   -48,   -48,   -48,   -48,   -48,   123,
   124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
   134,   135,   136,   137,   138,   139,   140,   141,   142,   143,
   144,   145,   146,   147,   206,   184,   112,   247,   258,   203,
   -52,    13,   155,   -52,   105,   106,   102,   -48,    88,   103,
   189,   171,    98,    18,   244,   183,   175,   176,    21,   177,
   178,   117,   244,   180,    18,    91,   227,   181,    21,    -1,
   156,    21,   120,    18,   186,   193,   187,    18,    21,   218,
    31,    32,    33,    34,    35,   195,   157,   158,   159,    21,
   169,   222,   208,   209,   210,   211,   212,    31,    32,    33,
    34,    35,   -33,   246,   -32,   190,   191,   194,   245,   198,
   118,   246,   200,   182,    94,    47,   174,    21,   170,   196,
    48,   185,   205,    36,    18,    48,   207,    18,   160,   225,
   260,    26,    27,    49,   110,   205,    -8,   219,    49,   221,
    36,   199,   213,   214,    -8,    32,    33,    34,    35,    18,
   204,   224,   226,   201,   231,   233,   202,   -87,  -101,   154,
   234,    -8,  -101,  -101,    95,    18,    96,   230,    -8,   235,
   220,   236,   237,   205,    18,   252,   232,   240,   263,   264,
    -8,   243,   242,    -8,    -8,    -8,    18,    -8,   238,   257,
    -8,   217,   251,   216,   119,   109,   149,   239,    97,   253,
   254,   192,    -8,   148,   255,  -107,  -107,  -107,    31,    32,
    33,    34,    35,     0,   261,    32,    33,    34,    35,    49,
  -107,  -107,  -107,  -107,  -107,  -107,  -107,  -107,  -107,  -107,
  -107,  -107,  -107,  -107,  -107,  -107,  -107,  -107,  -107,  -107,
  -107,  -107,     0,  -101,  -101,   151,  -107,   149,  -107,     0,
     0,    36,     0,   152,     0,     0,  -107,  -107,  -107,     1,
     2,     3,     4,    31,    32,    33,    34,    35,     0,     0,
     0,  -107,  -107,  -107,  -107,  -107,  -107,  -107,  -107,  -107,
  -107,  -107,  -107,  -107,  -107,  -107,  -107,  -107,  -107,  -107,
  -107,  -107,  -107,     0,   150,     0,   151,  -107,   149,  -107,
   259,     0,     0,     0,   152,     0,    36,  -107,  -107,  -107,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,  -107,  -107,  -107,  -107,  -107,  -107,  -107,  -107,
  -107,  -107,  -107,  -107,  -107,  -107,  -107,  -107,  -107,  -107,
  -107,  -107,  -107,  -107,     0,     0,   104,   151,  -107,     0,
  -107,     0,     0,     0,   223,   152,   -48,   -48,   -48,   -48,
   -48,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,   188,     0,    21,     0,
     0,     0,     0,     0,     0,    55,    56,    57,     0,     0,
     0,     0,   -53,     0,     0,   -53,   105,   106,     0,   -48,
    58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
    68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
    78,    79,    21,     0,     0,     0,    80,     0,    81,    55,
    56,    57,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,    58,    59,    60,    61,    62,    63,
    64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
    74,    75,    76,    77,    78,    79,    55,    56,    57,     0,
    80,     0,    81,     0,     0,     0,     0,     0,     0,     0,
     0,    58,    59,    60,    61,    62,    63,    64,    65,    66,
    67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
    77,    78,    79,     0,     0,     0,     0,    80,     0,    81,
    58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
    68,    69,    70,    71,    72,    73,    74,    75,    76
};

static const short yycheck[] = {     5,
     6,     7,     9,    89,    47,    11,    12,    13,    90,    42,
   107,     1,     1,     3,     1,    47,    22,    49,    47,     3,
    10,     3,     3,     3,    13,     3,     3,    11,    10,    35,
    11,    37,    10,    11,     1,    41,    42,    47,    47,    49,
     1,     3,    48,    49,    11,    12,    13,    14,    15,    55,
    56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
    66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
    76,    77,    78,    79,   171,   118,   109,    57,    55,   161,
    47,    47,    88,    50,    51,    52,    47,    54,    16,    50,
   146,    97,    11,    47,     1,    49,   102,   103,     3,   105,
   106,    48,     1,   109,    47,    48,    11,   113,     3,     0,
     1,     3,    49,    47,   120,    49,   122,    47,     3,    49,
    11,    12,    13,    14,    15,    10,    17,    18,    19,     3,
     1,   187,     5,     6,     7,     8,     9,    11,    12,    13,
    14,    15,    49,    50,    49,   151,   152,   154,    47,   156,
     1,    50,   158,     1,    49,     1,    48,     3,    11,    44,
    11,    49,   169,    54,    47,    11,   172,    47,    59,    49,
   256,     3,     4,    24,     1,   182,     3,   183,    24,   185,
    54,     1,    55,    56,    11,    12,    13,    14,    15,    47,
    48,   197,   199,     1,   201,   202,     1,    48,    49,     1,
   206,     3,    48,    49,    11,    47,    13,    49,    10,   215,
     1,    48,   218,   220,    47,    49,    49,   223,     0,     0,
    47,   227,   224,    50,    51,    52,    47,    54,    49,   254,
    57,   176,   238,   175,    51,    41,     1,   222,    45,   245,
   246,   153,    44,    82,   250,    10,    11,    12,    11,    12,
    13,    14,    15,    -1,   260,    12,    13,    14,    15,    24,
    25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
    35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
    45,    46,    -1,    48,    49,    50,    51,     1,    53,    -1,
    -1,    54,    -1,    58,    -1,    -1,    10,    11,    12,    20,
    21,    22,    23,    11,    12,    13,    14,    15,    -1,    -1,
    -1,    25,    26,    27,    28,    29,    30,    31,    32,    33,
    34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
    44,    45,    46,    -1,    48,    -1,    50,    51,     1,    53,
    48,    -1,    -1,    -1,    58,    -1,    54,    10,    11,    12,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    25,    26,    27,    28,    29,    30,    31,    32,
    33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
    43,    44,    45,    46,    -1,    -1,     1,    50,    51,    -1,
    53,    -1,    -1,    -1,    57,    58,    11,    12,    13,    14,
    15,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,     1,    -1,     3,    -1,
    -1,    -1,    -1,    -1,    -1,    10,    11,    12,    -1,    -1,
    -1,    -1,    47,    -1,    -1,    50,    51,    52,    -1,    54,
    25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
    35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
    45,    46,     3,    -1,    -1,    -1,    51,    -1,    53,    10,
    11,    12,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    25,    26,    27,    28,    29,    30,
    31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
    41,    42,    43,    44,    45,    46,    10,    11,    12,    -1,
    51,    -1,    53,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    25,    26,    27,    28,    29,    30,    31,    32,    33,
    34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
    44,    45,    46,    -1,    -1,    -1,    -1,    51,    -1,    53,
    25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
    35,    36,    37,    38,    39,    40,    41,    42,    43
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
#line 238 "parser.y"
{
        CSSParser *p = static_cast<CSSParser *>(parser);
        p->rule = yyvsp[-2].rule;
    ;
    break;}
case 6:
#line 245 "parser.y"
{
	/* can be empty */
    ;
    break;}
case 7:
#line 251 "parser.y"
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
#line 288 "parser.y"
{
#ifdef CSS_DEBUG
     kdDebug( 6080 ) << "charset rule: " << qString(yyvsp[-2].string) << endl;
#endif
 ;
    break;}
case 18:
#line 299 "parser.y"
{
     CSSParser *p = static_cast<CSSParser *>(parser);
     if ( yyvsp[-1].rule && p->styleElement && p->styleElement->isCSSStyleSheet() ) {
	 p->styleElement->append( yyvsp[-1].rule );
     } else {
	 delete yyvsp[-1].rule;
     }
 ;
    break;}
case 20:
#line 312 "parser.y"
{
     CSSParser *p = static_cast<CSSParser *>(parser);
     if ( yyvsp[-1].rule && p->styleElement && p->styleElement->isCSSStyleSheet() ) {
	 p->styleElement->append( yyvsp[-1].rule );
     } else {
	 delete yyvsp[-1].rule;
     }
 ;
    break;}
case 27:
#line 332 "parser.y"
{
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "@import: " << qString(yyvsp[-3].string) << endl;
#endif
	CSSParser *p = static_cast<CSSParser *>(parser);
	if ( p->styleElement && p->styleElement->isCSSStyleSheet() )
	    yyval.rule = new CSSImportRuleImpl( p->styleElement, domString(yyvsp[-3].string), yyvsp[-1].mediaList );
	else {
	    yyval.rule = 0;
            delete yyvsp[-1].mediaList;
        }
    ;
    break;}
case 28:
#line 344 "parser.y"
{
        yyval.rule = 0;
    ;
    break;}
case 29:
#line 347 "parser.y"
{
        yyval.rule = 0;
    ;
    break;}
case 32:
#line 358 "parser.y"
{
        yyval.mediaList = 0;
     ;
    break;}
case 34:
#line 366 "parser.y"
{
	yyval.mediaList = 0;
    ;
    break;}
case 35:
#line 369 "parser.y"
{
	yyval.mediaList = new MediaListImpl();
	yyval.mediaList->appendMedium( domString(yyvsp[0].string) );
    ;
    break;}
case 36:
#line 373 "parser.y"
{
	yyval.mediaList = yyvsp[-3].mediaList;
        if (yyval.mediaList)
	    yyval.mediaList->appendMedium( domString(yyvsp[0].string) );
    ;
    break;}
case 37:
#line 378 "parser.y"
{
        delete yyvsp[-1].mediaList;
        yyval.mediaList = 0;
    ;
    break;}
case 38:
#line 385 "parser.y"
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
case 39:
#line 399 "parser.y"
{ yyval.ruleList = 0; ;
    break;}
case 40:
#line 400 "parser.y"
{
      yyval.ruleList = yyvsp[-2].ruleList;
      if ( yyvsp[-1].rule ) {
	  if ( !yyval.ruleList ) yyval.ruleList = new CSSRuleListImpl();
	  yyval.ruleList->append( yyvsp[-1].rule );
      }
  ;
    break;}
case 41:
#line 410 "parser.y"
{
      yyval.string = yyvsp[-1].string;
  ;
    break;}
case 42:
#line 432 "parser.y"
{
      yyval.rule = 0;
    ;
    break;}
case 43:
#line 435 "parser.y"
{
      yyval.rule = 0;
    ;
    break;}
case 44:
#line 441 "parser.y"
{
      yyval.rule = 0;
    ;
    break;}
case 45:
#line 444 "parser.y"
{
      yyval.rule = 0;
    ;
    break;}
case 46:
#line 450 "parser.y"
{ yyval.relation = CSSSelector::Sibling; ;
    break;}
case 47:
#line 451 "parser.y"
{ yyval.relation = CSSSelector::Child; ;
    break;}
case 48:
#line 452 "parser.y"
{ yyval.relation = CSSSelector::Descendant; ;
    break;}
case 49:
#line 456 "parser.y"
{ yyval.val = -1; ;
    break;}
case 50:
#line 457 "parser.y"
{ yyval.val = 1; ;
    break;}
case 51:
#line 461 "parser.y"
{
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "got ruleset" << endl << "  selector:" << endl;
#endif
	CSSParser *p = static_cast<CSSParser *>(parser);
	if ( yyvsp[-4].selectorList && yyvsp[-1].ok && p->numParsedProperties ) {
	    CSSStyleRuleImpl *rule = new CSSStyleRuleImpl( p->styleElement );
	    CSSStyleDeclarationImpl *decl = p->createStyleDeclaration( rule );
	    rule->setSelector( yyvsp[-4].selectorList );
	    rule->setDeclaration(decl);
	    yyval.rule = rule;
	} else {
	    yyval.rule = 0;
	    delete yyvsp[-4].selectorList;
	    p->clearProperties();
	}
    ;
    break;}
case 52:
#line 481 "parser.y"
{
	if ( yyvsp[0].selector ) {
	    yyval.selectorList = new QPtrList<CSSSelector>;
            yyval.selectorList->setAutoDelete( true );
#ifdef CSS_DEBUG
	    kdDebug( 6080 ) << "   got simple selector:" << endl;
	    yyvsp[0].selector->print();
#endif
	    yyval.selectorList->append( yyvsp[0].selector );
	} else {
	    yyval.selectorList = 0;
	}
    ;
    break;}
case 53:
#line 494 "parser.y"
{
	if ( yyvsp[-3].selectorList && yyvsp[0].selector ) {
	    yyval.selectorList = yyvsp[-3].selectorList;
	    yyval.selectorList->append( yyvsp[0].selector );
#ifdef CSS_DEBUG
	    kdDebug( 6080 ) << "   got simple selector:" << endl;
	    yyvsp[0].selector->print();
#endif
	} else {
            delete yyvsp[-3].selectorList;
            delete yyvsp[0].selector;
            yyval.selectorList = 0;
        }
    ;
    break;}
case 54:
#line 508 "parser.y"
{
        delete yyvsp[-1].selectorList;
        yyval.selectorList = 0;
    ;
    break;}
case 55:
#line 515 "parser.y"
{
	yyval.selector = yyvsp[0].selector;
    ;
    break;}
case 56:
#line 518 "parser.y"
{
    	yyval.selector = yyvsp[0].selector;
        if (yyval.selector) {
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
        } else {
            delete yyvsp[-2].selector;
        }
    ;
    break;}
case 57:
#line 537 "parser.y"
{
        delete yyvsp[-1].selector;
        yyval.selector = 0;
    ;
    break;}
case 58:
#line 544 "parser.y"
{
	yyval.selector = new CSSSelector();
	yyval.selector->tag = yyvsp[-1].element;
    ;
    break;}
case 59:
#line 548 "parser.y"
{
	yyval.selector = yyvsp[-1].selector;
	if ( yyval.selector )
            yyval.selector->tag = yyvsp[-2].element;
    ;
    break;}
case 60:
#line 553 "parser.y"
{
	yyval.selector = yyvsp[-1].selector;
	if ( yyval.selector )
            yyval.selector->tag = 0xffffffff;
    ;
    break;}
case 61:
#line 561 "parser.y"
{
	CSSParser *p = static_cast<CSSParser *>(parser);
	DOM::DocumentImpl *doc = p->document();
	QString tag = qString(yyvsp[0].string);
	if ( doc ) {
	    if (doc->isHTMLDocument())
		tag = tag.lower();
	    const DOMString dtag(tag);
#if APPLE_CHANGES
            yyval.element = doc->tagId(0, dtag.implementation(), false);
#else
	    yyval.element = doc->elementNames()->getId(dtag.implementation(), false);
#endif
	} else {
	    yyval.element = khtml::getTagID(tag.lower().ascii(), tag.length());
	    // this case should never happen - only when loading
	    // the default stylesheet - which must not contain unknown tags
// 	    assert($$ != 0);
	}
    ;
    break;}
case 62:
#line 581 "parser.y"
{
	yyval.element = -1;
    ;
    break;}
case 63:
#line 587 "parser.y"
{
	yyval.selector = yyvsp[0].selector;
	yyval.selector->nonCSSHint = static_cast<CSSParser *>(parser)->nonCSSHint;
    ;
    break;}
case 64:
#line 591 "parser.y"
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
case 65:
#line 601 "parser.y"
{
        delete yyvsp[-1].selector;
        yyval.selector = 0;
    ;
    break;}
case 66:
#line 608 "parser.y"
{
	yyval.selector = new CSSSelector();
	yyval.selector->match = CSSSelector::Id;
	yyval.selector->attr = ATTR_ID;
	yyval.selector->value = domString(yyvsp[0].string);
    ;
    break;}
case 70:
#line 620 "parser.y"
{
	yyval.selector = new CSSSelector();
	yyval.selector->match = CSSSelector::List;
	yyval.selector->attr = ATTR_CLASS;
	yyval.selector->value = domString(yyvsp[0].string);
    ;
    break;}
case 71:
#line 629 "parser.y"
{
	CSSParser *p = static_cast<CSSParser *>(parser);
	DOM::DocumentImpl *doc = p->document();

	QString attr = qString(yyvsp[-1].string);
	if ( doc ) {
	    if (doc->isHTMLDocument())
		attr = attr.lower();
	    const DOMString dattr(attr);
#if APPLE_CHANGES
            yyval.attribute = doc->attrId(0, dattr.implementation(), false);
#else
	    yyval.attribute = doc->attrNames()->getId(dattr.implementation(), false);
#endif
	} else {
	    yyval.attribute = khtml::getAttrID(attr.lower().ascii(), attr.length());
	    // this case should never happen - only when loading
	    // the default stylesheet - which must not contain unknown attributes
	    assert(yyval.attribute != 0);
	    }
    ;
    break;}
case 72:
#line 653 "parser.y"
{
	yyval.selector = new CSSSelector();
	yyval.selector->attr = yyvsp[-1].attribute;
	yyval.selector->match = CSSSelector::Set;
    ;
    break;}
case 73:
#line 658 "parser.y"
{
	yyval.selector = new CSSSelector();
	yyval.selector->attr = yyvsp[-5].attribute;
	yyval.selector->match = (CSSSelector::Match)yyvsp[-4].val;
	yyval.selector->value = domString(yyvsp[-2].string);
    ;
    break;}
case 74:
#line 667 "parser.y"
{
	yyval.val = CSSSelector::Exact;
    ;
    break;}
case 75:
#line 670 "parser.y"
{
	yyval.val = CSSSelector::List;
    ;
    break;}
case 76:
#line 673 "parser.y"
{
	yyval.val = CSSSelector::Hyphen;
    ;
    break;}
case 77:
#line 676 "parser.y"
{
	yyval.val = CSSSelector::Begin;
    ;
    break;}
case 78:
#line 679 "parser.y"
{
	yyval.val = CSSSelector::End;
    ;
    break;}
case 79:
#line 682 "parser.y"
{
	yyval.val = CSSSelector::Contain;
    ;
    break;}
case 82:
#line 693 "parser.y"
{
        yyval.selector = new CSSSelector();
        yyval.selector->match = CSSSelector::Pseudo;
        yyval.selector->value = domString(yyvsp[0].string);
    ;
    break;}
case 83:
#line 699 "parser.y"
{
        yyval.selector = new CSSSelector();
        yyval.selector->match = CSSSelector::Pseudo;
        yyval.selector->value = domString(yyvsp[0].string);
    ;
    break;}
case 84:
#line 704 "parser.y"
{
        yyval.selector = new CSSSelector();
        yyval.selector->match = CSSSelector::Pseudo;
        yyval.selector->simpleSelector = yyvsp[-2].selector;
        yyval.selector->value = domString(yyvsp[-4].string);
    ;
    break;}
case 85:
#line 713 "parser.y"
{
	yyval.ok = yyvsp[0].ok;
    ;
    break;}
case 86:
#line 716 "parser.y"
{
	yyval.ok = yyvsp[-1].ok;
	if ( yyvsp[0].ok )
	    yyval.ok = yyvsp[0].ok;
    ;
    break;}
case 87:
#line 721 "parser.y"
{
	yyval.ok = yyvsp[0].ok;
    ;
    break;}
case 88:
#line 724 "parser.y"
{
	yyval.ok = false;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    ;
    break;}
case 89:
#line 730 "parser.y"
{
	yyval.ok = false;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping all declarations" << endl;
#endif
    ;
    break;}
case 90:
#line 739 "parser.y"
{
	yyval.ok = yyvsp[-2].ok;
    ;
    break;}
case 91:
#line 742 "parser.y"
{
	yyval.ok = false;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    ;
    break;}
case 92:
#line 748 "parser.y"
{
	yyval.ok = false;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    ;
    break;}
case 93:
#line 754 "parser.y"
{
	yyval.ok = yyvsp[-3].ok;
	if ( yyvsp[-2].ok )
	    yyval.ok = yyvsp[-2].ok;
    ;
    break;}
case 94:
#line 759 "parser.y"
{
	yyval.ok = yyvsp[-3].ok;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    ;
    break;}
case 95:
#line 765 "parser.y"
{
	yyval.ok = yyvsp[-5].ok;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    ;
    break;}
case 96:
#line 774 "parser.y"
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
case 97:
#line 797 "parser.y"
{
        yyval.ok = false;
    ;
    break;}
case 98:
#line 801 "parser.y"
{
        /* Handle this case: div { text-align: center; !important } Just reduce away the stray !important. */
        yyval.ok = false;
    ;
    break;}
case 99:
#line 808 "parser.y"
{
	QString str = qString(yyvsp[-1].string);
	yyval.prop_id = getPropertyID( str.lower().latin1(), str.length() );
    ;
    break;}
case 100:
#line 815 "parser.y"
{ yyval.b = true; ;
    break;}
case 101:
#line 816 "parser.y"
{ yyval.b = false; ;
    break;}
case 102:
#line 820 "parser.y"
{
	yyval.valueList = new ValueList;
	yyval.valueList->addValue( yyvsp[0].value );
    ;
    break;}
case 103:
#line 824 "parser.y"
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
case 104:
#line 837 "parser.y"
{
        delete yyvsp[-1].valueList;
        yyval.valueList = 0;
    ;
    break;}
case 105:
#line 844 "parser.y"
{
	yyval.tok = '/';
    ;
    break;}
case 106:
#line 847 "parser.y"
{
	yyval.tok = ',';
    ;
    break;}
case 107:
#line 850 "parser.y"
{
        yyval.tok = 0;
  ;
    break;}
case 108:
#line 856 "parser.y"
{ yyval.value = yyvsp[0].value; ;
    break;}
case 109:
#line 857 "parser.y"
{ yyval.value = yyvsp[0].value; yyval.value.fValue *= yyvsp[-1].val; ;
    break;}
case 110:
#line 858 "parser.y"
{ yyval.value.id = 0; yyval.value.string = yyvsp[-1].string; yyval.value.unit = CSSPrimitiveValue::CSS_STRING; ;
    break;}
case 111:
#line 859 "parser.y"
{
      QString str = qString( yyvsp[-1].string );
      yyval.value.id = getValueID( str.lower().latin1(), str.length() );
      yyval.value.unit = CSSPrimitiveValue::CSS_IDENT;
      yyval.value.string = yyvsp[-1].string;
  ;
    break;}
case 112:
#line 865 "parser.y"
{ yyval.value.id = 0; yyval.value.string = yyvsp[-1].string; yyval.value.unit = CSSPrimitiveValue::CSS_URI; ;
    break;}
case 113:
#line 866 "parser.y"
{ yyval.value.id = 0; yyval.value.iValue = 0; yyval.value.unit = CSSPrimitiveValue::CSS_UNKNOWN;/* ### */ ;
    break;}
case 114:
#line 867 "parser.y"
{ yyval.value.id = 0; yyval.value.string = yyvsp[0].string; yyval.value.unit = CSSPrimitiveValue::CSS_RGBCOLOR; ;
    break;}
case 115:
#line 869 "parser.y"
{
      yyval.value = yyvsp[0].value;
  ;
    break;}
case 116:
#line 875 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_NUMBER; ;
    break;}
case 117:
#line 876 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_PERCENTAGE; ;
    break;}
case 118:
#line 877 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_PX; ;
    break;}
case 119:
#line 878 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_CM; ;
    break;}
case 120:
#line 879 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_MM; ;
    break;}
case 121:
#line 880 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_IN; ;
    break;}
case 122:
#line 881 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_PT; ;
    break;}
case 123:
#line 882 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_PC; ;
    break;}
case 124:
#line 883 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_DEG; ;
    break;}
case 125:
#line 884 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_RAD; ;
    break;}
case 126:
#line 885 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_GRAD; ;
    break;}
case 127:
#line 886 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_MS; ;
    break;}
case 128:
#line 887 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_S; ;
    break;}
case 129:
#line 888 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_HZ; ;
    break;}
case 130:
#line 889 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_KHZ; ;
    break;}
case 131:
#line 890 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_EMS; ;
    break;}
case 132:
#line 891 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = Value::Q_EMS; ;
    break;}
case 133:
#line 892 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_EXS; ;
    break;}
case 134:
#line 893 "parser.y"
{ yyval.value.id = 0; yyval.value.string = yyvsp[-1].string; yyval.value.unit = CSSPrimitiveValue::CSS_DIMENSION ;
    break;}
case 135:
#line 898 "parser.y"
{
      Function *f = new Function;
      f->name = yyvsp[-4].string;
      f->args = yyvsp[-2].valueList;
      yyval.value.id = 0;
      yyval.value.unit = Value::Function;
      yyval.value.function = f;
  ;
    break;}
case 136:
#line 906 "parser.y"
{
      yyval.value.id = 0;
      yyval.value.unit = Value::Function;
      yyval.value.function = 0;
  ;
    break;}
case 137:
#line 918 "parser.y"
{ yyval.string = yyvsp[-1].string; ;
    break;}
case 138:
#line 925 "parser.y"
{
	yyval.rule = 0;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipped invalid @-rule" << endl;
#endif
    ;
    break;}
case 139:
#line 931 "parser.y"
{
	yyval.rule = 0;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipped invalid @-rule" << endl;
#endif
    ;
    break;}
case 140:
#line 940 "parser.y"
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
#line 975 "parser.y"


