
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
#define	KHTML_RULE_SYM	272
#define	KHTML_DECLS_SYM	273
#define	KHTML_VALUE_SYM	274
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



#define	YYFINAL		275
#define	YYFLAG		-32768
#define	YYNTBASE	61

#define YYTRANSLATE(x) ((unsigned)(x) <= 297 ? yytranslate[x] : 110)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,    59,     2,     2,     2,     2,     2,
    57,    54,    51,    50,    53,    14,    58,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,    13,    49,     2,
    56,    52,     2,    60,     2,     2,     2,     2,     2,     2,
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
   279,   284,   288,   294,   299,   304,   311,   317,   320,   327,
   329,   333,   336,   339,   340,   342,   346,   349,   352,   355,
   356,   358,   361,   364,   367,   370,   374,   377,   380,   382,
   385,   387,   390,   393,   396,   399,   402,   405,   408,   411,
   414,   417,   420,   423,   426,   429,   432,   435,   438,   441,
   447,   451,   454,   458,   462,   465,   471,   475,   477
};

static const short yyrhs[] = {    67,
    66,    68,    69,     0,    62,    65,     0,    63,    65,     0,
    64,    65,     0,    21,    47,    65,    82,    65,    48,     0,
    22,    47,    65,    95,    48,     0,    23,    47,    65,   100,
    48,     0,     0,    65,     3,     0,     0,    66,     4,     0,
    66,     3,     0,     0,    20,    65,    10,    65,    49,     0,
    20,     1,   108,     0,    20,     1,    49,     0,     0,    68,
    71,    66,     0,     0,    69,    70,    66,     0,    82,     0,
    75,     0,    78,     0,    79,     0,   107,     0,   106,     0,
    16,    65,    72,    65,    73,    49,     0,    16,     1,   108,
     0,    16,     1,    49,     0,    10,     0,    44,     0,     0,
    74,     0,     0,    77,     0,    74,    50,    65,    77,     0,
    74,     1,     0,    18,    65,    74,    47,    65,    76,    48,
     0,     0,    76,    82,    65,     0,    11,    65,     0,    17,
     1,   108,     0,    17,     1,    49,     0,    19,     1,   108,
     0,    19,     1,    49,     0,    51,    65,     0,    52,    65,
     0,     0,    53,     0,    51,     0,    83,    47,    65,    95,
    48,     0,    84,     0,    83,    50,    65,    84,     0,    83,
     1,     0,    85,     0,    84,    80,    85,     0,    84,     1,
     0,    86,    65,     0,    86,    87,    65,     0,    87,    65,
     0,    11,     0,    54,     0,    88,     0,    87,    88,     0,
    87,     1,     0,    12,     0,    89,     0,    91,     0,    94,
     0,    14,    11,     0,    11,    65,     0,    15,    65,    90,
    55,     0,    15,    65,    90,    92,    65,    93,    65,    55,
     0,    56,     0,     5,     0,     6,     0,     7,     0,     8,
     0,     9,     0,    11,     0,    10,     0,    13,    11,     0,
    13,    13,    11,     0,    13,    45,    65,    85,    65,    57,
     0,    97,     0,    96,    97,     0,    96,     0,     1,   109,
     1,     0,     1,     0,    97,    49,    65,     0,    97,   109,
    49,    65,     0,     1,    49,    65,     0,     1,   109,     1,
    49,    65,     0,    96,    97,    49,    65,     0,    96,     1,
    49,    65,     0,    96,     1,   109,     1,    49,    65,     0,
    98,    13,    65,   100,    99,     0,    98,     1,     0,    98,
    13,    65,     1,   100,    99,     0,    99,     0,    98,    13,
    65,     0,    11,    65,     0,    24,    65,     0,     0,   102,
     0,   100,   101,   102,     0,   100,     1,     0,    58,    65,
     0,    50,    65,     0,     0,   103,     0,    81,   103,     0,
    10,    65,     0,    11,    65,     0,    41,    65,     0,    81,
    41,    65,     0,    44,    65,     0,    46,    65,     0,   105,
     0,    59,    65,     0,   104,     0,    43,    65,     0,    42,
    65,     0,    28,    65,     0,    29,    65,     0,    30,    65,
     0,    31,    65,     0,    32,    65,     0,    33,    65,     0,
    34,    65,     0,    35,    65,     0,    36,    65,     0,    37,
    65,     0,    38,    65,     0,    39,    65,     0,    40,    65,
     0,    26,    65,     0,    25,    65,     0,    27,    65,     0,
    45,    65,   100,    57,    65,     0,    45,    65,     1,     0,
    12,    65,     0,    60,     1,   108,     0,    60,     1,    49,
     0,     1,   108,     0,    47,     1,   109,     1,    48,     0,
    47,     1,    48,     0,   108,     0,   109,     1,   108,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   230,   232,   233,   234,   237,   244,   250,   275,   277,   280,
   282,   283,   286,   288,   293,   294,   297,   299,   310,   312,
   322,   324,   325,   326,   327,   328,   331,   344,   347,   352,
   354,   357,   361,   365,   369,   373,   378,   384,   398,   400,
   409,   431,   435,   440,   444,   449,   451,   452,   455,   457,
   460,   480,   494,   508,   514,   518,   541,   547,   552,   557,
   564,   585,   590,   595,   605,   611,   618,   619,   620,   623,
   632,   656,   662,   670,   674,   677,   680,   683,   686,   691,
   693,   696,   702,   708,   716,   720,   725,   728,   734,   742,
   746,   749,   755,   761,   766,   772,   780,   803,   807,   815,
   820,   827,   834,   836,   839,   844,   857,   863,   867,   870,
   875,   877,   878,   879,   886,   887,   888,   889,   890,   891,
   893,   898,   900,   901,   902,   903,   904,   905,   906,   907,
   908,   909,   910,   911,   912,   913,   914,   915,   916,   920,
   928,   943,   950,   957,   965,   991,   993,   996,   998
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","S","SGML_CD",
"INCLUDES","DASHMATCH","BEGINSWITH","ENDSWITH","CONTAINS","STRING","IDENT","HASH",
"':'","'.'","'['","IMPORT_SYM","PAGE_SYM","MEDIA_SYM","FONT_FACE_SYM","CHARSET_SYM",
"KHTML_RULE_SYM","KHTML_DECLS_SYM","KHTML_VALUE_SYM","IMPORTANT_SYM","QEMS",
"EMS","EXS","PXS","CMS","MMS","INS","PTS","PCS","DEGS","RADS","GRADS","MSECS",
"SECS","HERZ","KHERZ","DIMEN","PERCENTAGE","NUMBER","URI","FUNCTION","UNICODERANGE",
"'{'","'}'","';'","','","'+'","'>'","'-'","'*'","']'","'='","')'","'/'","'#'",
"'@'","stylesheet","khtml_rule","khtml_decls","khtml_value","maybe_space","maybe_sgml",
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
    61,    61,    61,    61,    62,    63,    64,    65,    65,    66,
    66,    66,    67,    67,    67,    67,    68,    68,    69,    69,
    70,    70,    70,    70,    70,    70,    71,    71,    71,    72,
    72,    73,    73,    74,    74,    74,    74,    75,    76,    76,
    77,    78,    78,    79,    79,    80,    80,    80,    81,    81,
    82,    83,    83,    83,    84,    84,    84,    85,    85,    85,
    86,    86,    87,    87,    87,    88,    88,    88,    88,    89,
    90,    91,    91,    92,    92,    92,    92,    92,    92,    93,
    93,    94,    94,    94,    95,    95,    95,    95,    95,    96,
    96,    96,    96,    96,    96,    96,    97,    97,    97,    97,
    97,    98,    99,    99,   100,   100,   100,   101,   101,   101,
   102,   102,   102,   102,   102,   102,   102,   102,   102,   102,
   102,   103,   103,   103,   103,   103,   103,   103,   103,   103,
   103,   103,   103,   103,   103,   103,   103,   103,   103,   104,
   104,   105,   106,   106,   107,   108,   108,   109,   109
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
     4,     3,     5,     4,     4,     6,     5,     2,     6,     1,
     3,     2,     2,     0,     1,     3,     2,     2,     2,     0,
     1,     2,     2,     2,     2,     3,     2,     2,     1,     2,
     1,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     5,
     3,     2,     3,     3,     2,     5,     3,     1,     3
};

static const short yydefact[] = {    13,
     0,     0,     0,     0,     8,     8,     8,    10,     0,     0,
     8,     8,     8,     2,     3,     4,    17,     0,    16,    15,
     9,     8,     0,     0,     0,    12,    11,    19,     0,     0,
    61,    66,     0,     0,     8,    62,     8,     0,     0,    55,
     8,     0,    63,    67,    68,    69,    89,     8,     8,     0,
     0,    85,     0,   100,     8,     8,     8,     8,     8,     8,
     8,     8,     8,     8,     8,     8,     8,     8,     8,     8,
     8,     8,     8,     8,     8,     8,     8,     8,     8,    50,
    49,     8,     0,     0,   105,   111,   121,   119,     0,     0,
    10,   147,   148,     0,    14,    82,     0,     8,    70,     0,
     0,    54,     8,     8,    57,     8,     8,     0,    58,     0,
    65,    60,    64,     8,     0,   102,   103,     6,     0,    86,
     8,     0,    98,     8,   113,   114,   142,   138,   137,   139,
   124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
   134,   135,   136,   115,   123,   122,   117,     0,   118,   120,
     8,   112,   107,     7,     8,     8,     0,     0,     0,     0,
     0,     8,     0,     0,    10,    22,    23,    24,    21,    26,
    25,    18,     0,    83,     0,     8,     0,     5,     0,     0,
    46,    47,    56,    59,    92,    88,     8,     0,     8,    90,
     0,     8,     0,   141,     0,   116,   109,   108,   106,    29,
    28,    30,    31,     8,   145,     0,    34,     0,     0,    20,
   146,   149,     8,    71,    75,    76,    77,    78,    79,    72,
    74,     8,     0,     0,     8,    95,     0,    94,    91,     0,
     0,     8,    34,    43,    42,     8,     0,    35,    45,    44,
   144,   143,     0,     0,    51,    93,     8,     0,    97,   140,
     0,     0,    41,    37,     8,     8,    84,    81,    80,     8,
    96,    99,    27,    39,     0,     0,     0,    36,    73,    38,
     8,    40,     0,     0,     0
};

static const short yydefgoto[] = {   273,
     5,     6,     7,    10,    17,     8,    28,    90,   165,    91,
   204,   251,   237,   166,   267,   238,   167,   168,   108,    83,
    37,    38,    39,    40,    41,    42,    43,    44,   177,    45,
   222,   260,    46,    50,    51,    52,    53,    54,    84,   157,
    85,    86,    87,    88,   170,   171,    93,    94
};

static const short yypact[] = {   223,
    13,   -28,   -18,    39,-32768,-32768,-32768,-32768,     0,    15,
-32768,-32768,-32768,    87,    87,    87,    31,    99,-32768,-32768,
-32768,-32768,   129,   540,   494,-32768,-32768,   118,   159,    84,
-32768,-32768,   222,    92,-32768,-32768,-32768,    44,   178,-32768,
   247,   162,-32768,-32768,-32768,-32768,    98,-32768,-32768,    69,
   107,   119,    11,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,   608,   348,-32768,-32768,-32768,-32768,   194,   110,
-32768,-32768,-32768,   125,-32768,-32768,   109,-32768,-32768,   101,
    82,-32768,-32768,-32768,-32768,-32768,-32768,   147,    87,   162,
-32768,    87,-32768,-32768,   148,    87,    87,-32768,   139,   120,
-32768,    32,-32768,-32768,    87,    87,    87,    87,    87,    87,
    87,    87,    87,    87,    87,    87,    87,    87,    87,    87,
    87,    87,    87,    87,    87,    87,    87,   450,    87,    87,
-32768,-32768,-32768,-32768,-32768,-32768,   573,   175,   195,   149,
   193,-32768,   214,   217,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,    31,   163,-32768,   129,-32768,    33,-32768,   540,   129,
    87,    87,-32768,    87,    87,   187,-32768,   225,-32768,    87,
   149,-32768,   297,-32768,   399,    87,    87,    87,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,   205,   103,   216,   254,    31,
-32768,-32768,-32768,    87,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,   100,   543,-32768,    87,   263,    87,    87,   573,
   246,-32768,   104,-32768,-32768,-32768,    45,-32768,-32768,-32768,
-32768,-32768,    18,    17,-32768,    87,-32768,   246,-32768,    87,
   174,    47,    87,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
    87,-32768,-32768,    87,   103,    23,   124,-32768,-32768,-32768,
-32768,    87,   237,   240,-32768
};

static const short yypgoto[] = {-32768,
-32768,-32768,-32768,    -5,   -82,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,     8,-32768,-32768,   -17,-32768,-32768,-32768,-32768,
   -86,-32768,    73,   -95,-32768,   208,   -31,-32768,-32768,-32768,
-32768,-32768,-32768,    85,-32768,   218,-32768,  -226,  -117,-32768,
   111,   219,-32768,-32768,-32768,-32768,    -6,   -37
};


#define	YYLAST		651


static const short yytable[] = {    14,
    15,    16,    20,   169,   249,    23,    24,    25,   172,   115,
   113,   123,   183,     9,   122,    -8,    30,    21,    11,    21,
    21,   262,    -8,   124,    22,    21,   258,   259,    12,   100,
   195,   101,   191,    26,    27,   109,   112,   215,   216,   217,
   218,   219,   116,   117,   102,   254,    18,   254,    19,   125,
   126,   127,   128,   129,   130,   131,   132,   133,   134,   135,
   136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
   146,   147,   148,   149,   257,   231,   150,   269,   113,   213,
   192,   188,   210,   159,    21,    13,    21,   220,   221,    21,
   103,   255,   175,   104,   256,   -33,   256,   179,   180,    29,
   181,   182,    99,    21,   184,    21,    21,   119,   185,    -1,
   160,   176,   248,   236,   236,   190,   118,    48,   193,   174,
    31,    32,    33,    34,    35,   173,   161,   162,   163,   178,
    49,    21,    95,    89,    31,    32,    33,    34,    35,    31,
    32,    33,    34,    35,    18,   196,   114,   245,   186,   197,
   198,   201,   -32,   205,   -87,  -104,   207,    31,    32,    33,
    34,    35,   111,    36,    -8,    18,   212,   121,   189,   164,
   214,   270,    -8,    32,    33,    34,    35,    36,   105,   212,
   271,   226,    36,   228,   212,    18,   229,   187,   -48,   -48,
   -48,   -48,   -48,   206,   158,    18,    -8,    21,   233,   235,
    36,   240,   242,    -8,   202,    18,    92,   243,    -8,    18,
   211,    -8,    -8,    -8,   208,    -8,   244,   209,    -8,   246,
   212,    18,   263,   200,   -52,   227,   250,   -52,   106,   107,
   253,   -48,    96,    18,    97,   225,   274,    -8,   203,   275,
   252,   261,     1,     2,     3,     4,   153,   268,   110,   264,
   265,    18,   224,   234,   266,  -110,  -110,  -110,    32,    33,
    34,    35,    18,   223,   239,   272,    98,   199,   120,    49,
  -110,  -110,  -110,  -110,  -110,  -110,  -110,  -110,  -110,  -110,
  -110,  -110,  -110,  -110,  -110,  -110,  -110,  -110,  -110,  -110,
  -110,  -110,  -104,  -104,  -104,   155,  -110,   230,  -110,    21,
    18,   152,   241,   156,  -110,     0,    55,    56,    57,    18,
     0,   247,     0,     0,     0,     0,     0,     0,     0,     0,
     0,    58,    59,    60,    61,    62,    63,    64,    65,    66,
    67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
    77,    78,    79,  -101,  -101,  -101,     0,    80,   153,    81,
     0,     0,     0,     0,     0,    82,     0,  -110,  -110,  -110,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,  -110,  -110,  -110,  -110,  -110,  -110,  -110,  -110,
  -110,  -110,  -110,  -110,  -110,  -110,  -110,  -110,  -110,  -110,
  -110,  -110,  -110,  -110,     0,   154,     0,   155,  -110,   153,
  -110,     0,     0,     0,     0,   156,  -110,     0,  -110,  -110,
  -110,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,  -110,  -110,  -110,  -110,  -110,  -110,  -110,
  -110,  -110,  -110,  -110,  -110,  -110,  -110,  -110,  -110,  -110,
  -110,  -110,  -110,  -110,  -110,     0,     0,     0,   155,  -110,
   194,  -110,    21,     0,     0,   232,   156,  -110,     0,    55,
    56,    57,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,    58,    59,    60,    61,    62,    63,
    64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
    74,    75,    76,    77,    78,    79,    21,     0,     0,     0,
    80,     0,    81,    55,    56,    57,     0,     0,    82,     0,
     0,     0,     0,     0,     0,     0,     0,     0,    58,    59,
    60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
    70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
    47,     0,    21,   105,    80,     0,    81,     0,     0,     0,
    48,     0,    82,   -48,   -48,   -48,   -48,   -48,     0,     0,
     0,     0,     0,    49,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,    55,    56,    57,     0,  -104,  -104,  -104,   -53,
     0,     0,   -53,   106,   107,     0,   -48,    58,    59,    60,
    61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
    71,    72,    73,    74,    75,    76,    77,    78,    79,     0,
     0,     0,     0,    80,     0,    81,     0,     0,     0,     0,
     0,    82,    58,    59,    60,    61,    62,    63,    64,    65,
    66,    67,    68,    69,    70,    71,    72,    73,   151,    75,
    76
};

static const short yycheck[] = {     5,
     6,     7,     9,    90,   231,    11,    12,    13,    91,    47,
    42,     1,   108,     1,    52,     3,    22,     3,    47,     3,
     3,   248,    10,    13,    10,     3,    10,    11,    47,    35,
   148,    37,     1,     3,     4,    41,    42,     5,     6,     7,
     8,     9,    48,    49,     1,     1,    47,     1,    49,    55,
    56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
    66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
    76,    77,    78,    79,    57,   193,    82,    55,   110,   175,
    49,   119,   165,    89,     3,    47,     3,    55,    56,     3,
    47,    47,    98,    50,    50,    49,    50,   103,   104,     1,
   106,   107,    11,     3,   110,     3,     3,     1,   114,     0,
     1,    11,   230,    11,    11,   121,    48,    11,   124,    11,
    11,    12,    13,    14,    15,     1,    17,    18,    19,    48,
    24,     3,    49,    16,    11,    12,    13,    14,    15,    11,
    12,    13,    14,    15,    47,   151,    49,    48,     1,   155,
   156,   158,    49,   160,    48,    49,   162,    11,    12,    13,
    14,    15,     1,    54,     3,    47,   173,    49,    49,    60,
   176,    48,    11,    12,    13,    14,    15,    54,     1,   186,
   267,   187,    54,   189,   191,    47,   192,    49,    11,    12,
    13,    14,    15,     1,     1,    47,     3,     3,   204,   206,
    54,   208,   209,    10,    10,    47,    48,   213,    47,    47,
    48,    50,    51,    52,     1,    54,   222,     1,    57,   225,
   227,    47,    49,    49,    47,     1,   232,    50,    51,    52,
   236,    54,    11,    47,    13,    49,     0,    44,    44,     0,
   233,   247,    20,    21,    22,    23,     1,   265,    41,   255,
   256,    47,   180,    49,   260,    10,    11,    12,    12,    13,
    14,    15,    47,   179,    49,   271,    45,   157,    51,    24,
    25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
    35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
    45,    46,    47,    48,    49,    50,    51,     1,    53,     3,
    47,    83,    49,    58,    59,    -1,    10,    11,    12,    47,
    -1,    49,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    25,    26,    27,    28,    29,    30,    31,    32,    33,
    34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
    44,    45,    46,    47,    48,    49,    -1,    51,     1,    53,
    -1,    -1,    -1,    -1,    -1,    59,    -1,    10,    11,    12,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    25,    26,    27,    28,    29,    30,    31,    32,
    33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
    43,    44,    45,    46,    -1,    48,    -1,    50,    51,     1,
    53,    -1,    -1,    -1,    -1,    58,    59,    -1,    10,    11,
    12,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    25,    26,    27,    28,    29,    30,    31,
    32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
    42,    43,    44,    45,    46,    -1,    -1,    -1,    50,    51,
     1,    53,     3,    -1,    -1,    57,    58,    59,    -1,    10,
    11,    12,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    25,    26,    27,    28,    29,    30,
    31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
    41,    42,    43,    44,    45,    46,     3,    -1,    -1,    -1,
    51,    -1,    53,    10,    11,    12,    -1,    -1,    59,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    25,    26,
    27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
    37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
     1,    -1,     3,     1,    51,    -1,    53,    -1,    -1,    -1,
    11,    -1,    59,    11,    12,    13,    14,    15,    -1,    -1,
    -1,    -1,    -1,    24,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    10,    11,    12,    -1,    47,    48,    49,    47,
    -1,    -1,    50,    51,    52,    -1,    54,    25,    26,    27,
    28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
    38,    39,    40,    41,    42,    43,    44,    45,    46,    -1,
    -1,    -1,    -1,    51,    -1,    53,    -1,    -1,    -1,    -1,
    -1,    59,    25,    26,    27,    28,    29,    30,    31,    32,
    33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
    43
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
	if ( yyvsp[-1].mediaList && p->styleElement && p->styleElement->isCSSStyleSheet() )
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
        yyval.mediaList = new MediaListImpl();
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
	yyval.mediaList->appendMedium( domString(yyvsp[0].string).lower() );
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
        } else {
            delete yyvsp[-2].selector;
        }
    ;
    break;}
case 57:
#line 541 "parser.y"
{
        delete yyvsp[-1].selector;
        yyval.selector = 0;
    ;
    break;}
case 58:
#line 548 "parser.y"
{
	yyval.selector = new CSSSelector();
	yyval.selector->tag = yyvsp[-1].element;
    ;
    break;}
case 59:
#line 552 "parser.y"
{
	yyval.selector = yyvsp[-1].selector;
	if ( yyval.selector )
            yyval.selector->tag = yyvsp[-2].element;
    ;
    break;}
case 60:
#line 557 "parser.y"
{
	yyval.selector = yyvsp[-1].selector;
	if ( yyval.selector )
            yyval.selector->tag = 0xffffffff;
    ;
    break;}
case 61:
#line 565 "parser.y"
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
#line 585 "parser.y"
{
	yyval.element = -1;
    ;
    break;}
case 63:
#line 591 "parser.y"
{
	yyval.selector = yyvsp[0].selector;
	yyval.selector->nonCSSHint = static_cast<CSSParser *>(parser)->nonCSSHint;
    ;
    break;}
case 64:
#line 595 "parser.y"
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
#line 605 "parser.y"
{
        delete yyvsp[-1].selector;
        yyval.selector = 0;
    ;
    break;}
case 66:
#line 612 "parser.y"
{
	yyval.selector = new CSSSelector();
	yyval.selector->match = CSSSelector::Id;
	yyval.selector->attr = ATTR_ID;
	yyval.selector->value = domString(yyvsp[0].string);
    ;
    break;}
case 70:
#line 624 "parser.y"
{
	yyval.selector = new CSSSelector();
	yyval.selector->match = CSSSelector::List;
	yyval.selector->attr = ATTR_CLASS;
	yyval.selector->value = domString(yyvsp[0].string);
    ;
    break;}
case 71:
#line 633 "parser.y"
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
#line 657 "parser.y"
{
	yyval.selector = new CSSSelector();
	yyval.selector->attr = yyvsp[-1].attribute;
	yyval.selector->match = CSSSelector::Set;
    ;
    break;}
case 73:
#line 662 "parser.y"
{
	yyval.selector = new CSSSelector();
	yyval.selector->attr = yyvsp[-5].attribute;
	yyval.selector->match = (CSSSelector::Match)yyvsp[-4].val;
	yyval.selector->value = domString(yyvsp[-2].string);
    ;
    break;}
case 74:
#line 671 "parser.y"
{
	yyval.val = CSSSelector::Exact;
    ;
    break;}
case 75:
#line 674 "parser.y"
{
	yyval.val = CSSSelector::List;
    ;
    break;}
case 76:
#line 677 "parser.y"
{
	yyval.val = CSSSelector::Hyphen;
    ;
    break;}
case 77:
#line 680 "parser.y"
{
	yyval.val = CSSSelector::Begin;
    ;
    break;}
case 78:
#line 683 "parser.y"
{
	yyval.val = CSSSelector::End;
    ;
    break;}
case 79:
#line 686 "parser.y"
{
	yyval.val = CSSSelector::Contain;
    ;
    break;}
case 82:
#line 697 "parser.y"
{
        yyval.selector = new CSSSelector();
        yyval.selector->match = CSSSelector::Pseudo;
        yyval.selector->value = domString(yyvsp[0].string);
    ;
    break;}
case 83:
#line 703 "parser.y"
{
        yyval.selector = new CSSSelector();
        yyval.selector->match = CSSSelector::Pseudo;
        yyval.selector->value = domString(yyvsp[0].string);
    ;
    break;}
case 84:
#line 708 "parser.y"
{
        yyval.selector = new CSSSelector();
        yyval.selector->match = CSSSelector::Pseudo;
        yyval.selector->simpleSelector = yyvsp[-2].selector;
        yyval.selector->value = domString(yyvsp[-4].string);
    ;
    break;}
case 85:
#line 717 "parser.y"
{
	yyval.ok = yyvsp[0].ok;
    ;
    break;}
case 86:
#line 720 "parser.y"
{
	yyval.ok = yyvsp[-1].ok;
	if ( yyvsp[0].ok )
	    yyval.ok = yyvsp[0].ok;
    ;
    break;}
case 87:
#line 725 "parser.y"
{
	yyval.ok = yyvsp[0].ok;
    ;
    break;}
case 88:
#line 728 "parser.y"
{
	yyval.ok = false;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    ;
    break;}
case 89:
#line 734 "parser.y"
{
	yyval.ok = false;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping all declarations" << endl;
#endif
    ;
    break;}
case 90:
#line 743 "parser.y"
{
	yyval.ok = yyvsp[-2].ok;
    ;
    break;}
case 91:
#line 746 "parser.y"
{
        yyval.ok = false;
    ;
    break;}
case 92:
#line 749 "parser.y"
{
	yyval.ok = false;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    ;
    break;}
case 93:
#line 755 "parser.y"
{
	yyval.ok = false;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    ;
    break;}
case 94:
#line 761 "parser.y"
{
	yyval.ok = yyvsp[-3].ok;
	if ( yyvsp[-2].ok )
	    yyval.ok = yyvsp[-2].ok;
    ;
    break;}
case 95:
#line 766 "parser.y"
{
	yyval.ok = yyvsp[-3].ok;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    ;
    break;}
case 96:
#line 772 "parser.y"
{
	yyval.ok = yyvsp[-5].ok;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    ;
    break;}
case 97:
#line 781 "parser.y"
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
case 98:
#line 804 "parser.y"
{
        yyval.ok = false;
    ;
    break;}
case 99:
#line 808 "parser.y"
{
        /* The default movable type template has letter-spacing: .none;  Handle this by looking for
        error tokens at the start of an expr, recover the expr and then treat as an error, cleaning
        up and deleting the shifted expr.  */
        delete yyvsp[-1].valueList;
        yyval.ok = false;
    ;
    break;}
case 100:
#line 816 "parser.y"
{
        /* Handle this case: div { text-align: center; !important } Just reduce away the stray !important. */
        yyval.ok = false;
    ;
    break;}
case 101:
#line 821 "parser.y"
{
        /* div { font-family: } Just reduce away this property with no value. */
        yyval.ok = false;
    ;
    break;}
case 102:
#line 828 "parser.y"
{
	QString str = qString(yyvsp[-1].string);
	yyval.prop_id = getPropertyID( str.lower().latin1(), str.length() );
    ;
    break;}
case 103:
#line 835 "parser.y"
{ yyval.b = true; ;
    break;}
case 104:
#line 836 "parser.y"
{ yyval.b = false; ;
    break;}
case 105:
#line 840 "parser.y"
{
	yyval.valueList = new ValueList;
	yyval.valueList->addValue( yyvsp[0].value );
    ;
    break;}
case 106:
#line 844 "parser.y"
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
case 107:
#line 857 "parser.y"
{
        delete yyvsp[-1].valueList;
        yyval.valueList = 0;
    ;
    break;}
case 108:
#line 864 "parser.y"
{
	yyval.tok = '/';
    ;
    break;}
case 109:
#line 867 "parser.y"
{
	yyval.tok = ',';
    ;
    break;}
case 110:
#line 870 "parser.y"
{
        yyval.tok = 0;
  ;
    break;}
case 111:
#line 876 "parser.y"
{ yyval.value = yyvsp[0].value; ;
    break;}
case 112:
#line 877 "parser.y"
{ yyval.value = yyvsp[0].value; yyval.value.fValue *= yyvsp[-1].val; ;
    break;}
case 113:
#line 878 "parser.y"
{ yyval.value.id = 0; yyval.value.string = yyvsp[-1].string; yyval.value.unit = CSSPrimitiveValue::CSS_STRING; ;
    break;}
case 114:
#line 879 "parser.y"
{
      QString str = qString( yyvsp[-1].string );
      yyval.value.id = getValueID( str.lower().latin1(), str.length() );
      yyval.value.unit = CSSPrimitiveValue::CSS_IDENT;
      yyval.value.string = yyvsp[-1].string;
  ;
    break;}
case 115:
#line 886 "parser.y"
{ yyval.value.id = 0; yyval.value.string = yyvsp[-1].string; yyval.value.unit = CSSPrimitiveValue::CSS_DIMENSION ;
    break;}
case 116:
#line 887 "parser.y"
{ yyval.value.id = 0; yyval.value.string = yyvsp[-1].string; yyval.value.unit = CSSPrimitiveValue::CSS_DIMENSION ;
    break;}
case 117:
#line 888 "parser.y"
{ yyval.value.id = 0; yyval.value.string = yyvsp[-1].string; yyval.value.unit = CSSPrimitiveValue::CSS_URI; ;
    break;}
case 118:
#line 889 "parser.y"
{ yyval.value.id = 0; yyval.value.iValue = 0; yyval.value.unit = CSSPrimitiveValue::CSS_UNKNOWN;/* ### */ ;
    break;}
case 119:
#line 890 "parser.y"
{ yyval.value.id = 0; yyval.value.string = yyvsp[0].string; yyval.value.unit = CSSPrimitiveValue::CSS_RGBCOLOR; ;
    break;}
case 120:
#line 891 "parser.y"
{ yyval.value.id = 0; yyval.value.string = ParseString(); yyval.value.unit = CSSPrimitiveValue::CSS_RGBCOLOR; ;
    break;}
case 121:
#line 893 "parser.y"
{
      yyval.value = yyvsp[0].value;
  ;
    break;}
case 122:
#line 899 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_NUMBER; ;
    break;}
case 123:
#line 900 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_PERCENTAGE; ;
    break;}
case 124:
#line 901 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_PX; ;
    break;}
case 125:
#line 902 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_CM; ;
    break;}
case 126:
#line 903 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_MM; ;
    break;}
case 127:
#line 904 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_IN; ;
    break;}
case 128:
#line 905 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_PT; ;
    break;}
case 129:
#line 906 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_PC; ;
    break;}
case 130:
#line 907 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_DEG; ;
    break;}
case 131:
#line 908 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_RAD; ;
    break;}
case 132:
#line 909 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_GRAD; ;
    break;}
case 133:
#line 910 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_MS; ;
    break;}
case 134:
#line 911 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_S; ;
    break;}
case 135:
#line 912 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_HZ; ;
    break;}
case 136:
#line 913 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_KHZ; ;
    break;}
case 137:
#line 914 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_EMS; ;
    break;}
case 138:
#line 915 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = Value::Q_EMS; ;
    break;}
case 139:
#line 916 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_EXS; ;
    break;}
case 140:
#line 921 "parser.y"
{
      Function *f = new Function;
      f->name = yyvsp[-4].string;
      f->args = yyvsp[-2].valueList;
      yyval.value.id = 0;
      yyval.value.unit = Value::Function;
      yyval.value.function = f;
  ;
    break;}
case 141:
#line 929 "parser.y"
{
      Function *f = new Function;
      f->name = yyvsp[-2].string;
      f->args = 0;
      yyval.value.id = 0;
      yyval.value.unit = Value::Function;
      yyval.value.function = f;
  ;
    break;}
case 142:
#line 944 "parser.y"
{ yyval.string = yyvsp[-1].string; ;
    break;}
case 143:
#line 951 "parser.y"
{
	yyval.rule = 0;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipped invalid @-rule" << endl;
#endif
    ;
    break;}
case 144:
#line 957 "parser.y"
{
	yyval.rule = 0;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipped invalid @-rule" << endl;
#endif
    ;
    break;}
case 145:
#line 966 "parser.y"
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
#line 1001 "parser.y"


