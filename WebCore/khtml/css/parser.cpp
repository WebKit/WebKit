
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
    char tok;
    Value value;
    ValueList *valueList;
} YYSTYPE;
#line 105 "parser.y"


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



#define	YYFINAL		310
#define	YYFLAG		-32768
#define	YYNTBASE	63

#define YYTRANSLATE(x) ((unsigned)(x) <= 298 ? yytranslate[x] : 116)

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
    70,    71,    75,    77,    79,    81,    83,    85,    87,    94,
    98,   102,   109,   113,   117,   118,   121,   123,   125,   126,
   128,   129,   131,   136,   139,   147,   148,   152,   155,   159,
   163,   167,   171,   174,   177,   178,   180,   182,   188,   190,
   195,   198,   200,   204,   207,   208,   210,   212,   215,   219,
   222,   227,   233,   238,   240,   242,   244,   247,   250,   252,
   254,   256,   258,   261,   264,   269,   278,   285,   296,   298,
   300,   302,   304,   306,   308,   310,   312,   315,   319,   326,
   328,   331,   333,   337,   339,   343,   348,   352,   358,   363,
   368,   375,   381,   384,   391,   393,   397,   400,   403,   404,
   406,   410,   413,   416,   419,   420,   422,   425,   428,   431,
   434,   438,   441,   444,   446,   449,   451,   454,   457,   460,
   463,   466,   469,   472,   475,   478,   481,   484,   487,   490,
   493,   496,   499,   502,   505,   511,   515,   518,   522,   526,
   529,   535,   539,   541
};

static const short yyrhs[] = {    69,
    68,    70,    71,    72,     0,    64,    67,     0,    65,    67,
     0,    66,    67,     0,    22,    48,    67,    87,    67,    49,
     0,    23,    48,    67,   101,    49,     0,    24,    48,    67,
   106,    49,     0,     0,    67,     3,     0,     0,    68,     4,
     0,    68,     3,     0,     0,    20,    67,    10,    67,    50,
     0,    20,     1,   114,     0,    20,     1,    50,     0,     0,
    70,    74,    68,     0,     0,    71,    75,    68,     0,     0,
    72,    73,    68,     0,    87,     0,    80,     0,    83,     0,
    84,     0,   113,     0,   112,     0,    16,    67,    77,    67,
    78,    50,     0,    16,     1,   114,     0,    16,     1,    50,
     0,    21,    67,    76,    77,    67,    50,     0,    21,     1,
   114,     0,    21,     1,    50,     0,     0,    11,     3,     0,
    10,     0,    45,     0,     0,    79,     0,     0,    82,     0,
    79,    51,    67,    82,     0,    79,     1,     0,    18,    67,
    79,    48,    67,    81,    49,     0,     0,    81,    87,    67,
     0,    11,    67,     0,    17,     1,   114,     0,    17,     1,
    50,     0,    19,     1,   114,     0,    19,     1,    50,     0,
    52,    67,     0,    53,    67,     0,     0,    54,     0,    52,
     0,    88,    48,    67,   101,    49,     0,    89,     0,    88,
    51,    67,    89,     0,    88,     1,     0,    91,     0,    89,
    85,    91,     0,    89,     1,     0,     0,    55,     0,    11,
     0,    92,    67,     0,    92,    93,    67,     0,    93,    67,
     0,    90,    56,    92,    67,     0,    90,    56,    92,    93,
    67,     0,    90,    56,    93,    67,     0,    11,     0,    55,
     0,    94,     0,    93,    94,     0,    93,     1,     0,    12,
     0,    95,     0,    97,     0,   100,     0,    14,    11,     0,
    11,    67,     0,    15,    67,    96,    57,     0,    15,    67,
    96,    98,    67,    99,    67,    57,     0,    15,    67,    90,
    56,    96,    57,     0,    15,    67,    90,    56,    96,    98,
    67,    99,    67,    57,     0,    58,     0,     5,     0,     6,
     0,     7,     0,     8,     0,     9,     0,    11,     0,    10,
     0,    13,    11,     0,    13,    13,    11,     0,    13,    46,
    67,    91,    67,    59,     0,   103,     0,   102,   103,     0,
   102,     0,     1,   115,     1,     0,     1,     0,   103,    50,
    67,     0,   103,   115,    50,    67,     0,     1,    50,    67,
     0,     1,   115,     1,    50,    67,     0,   102,   103,    50,
    67,     0,   102,     1,    50,    67,     0,   102,     1,   115,
     1,    50,    67,     0,   104,    13,    67,   106,   105,     0,
   104,     1,     0,   104,    13,    67,     1,   106,   105,     0,
   105,     0,   104,    13,    67,     0,    11,    67,     0,    25,
    67,     0,     0,   108,     0,   106,   107,   108,     0,   106,
     1,     0,    60,    67,     0,    51,    67,     0,     0,   109,
     0,    86,   109,     0,    10,    67,     0,    11,    67,     0,
    42,    67,     0,    86,    42,    67,     0,    45,    67,     0,
    47,    67,     0,   111,     0,    61,    67,     0,   110,     0,
    44,    67,     0,    43,    67,     0,    29,    67,     0,    30,
    67,     0,    31,    67,     0,    32,    67,     0,    33,    67,
     0,    34,    67,     0,    35,    67,     0,    36,    67,     0,
    37,    67,     0,    38,    67,     0,    39,    67,     0,    40,
    67,     0,    41,    67,     0,    27,    67,     0,    26,    67,
     0,    28,    67,     0,    46,    67,   106,    59,    67,     0,
    46,    67,     1,     0,    12,    67,     0,    62,     1,   114,
     0,    62,     1,    50,     0,     1,   114,     0,    48,     1,
   115,     1,    49,     0,    48,     1,    49,     0,   114,     0,
   115,     1,   114,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   236,   238,   239,   240,   243,   250,   256,   281,   283,   286,
   288,   289,   292,   294,   299,   300,   303,   305,   315,   317,
   320,   322,   332,   334,   335,   336,   337,   338,   341,   354,
   357,   362,   371,   372,   375,   377,   380,   382,   385,   389,
   393,   397,   401,   406,   412,   426,   428,   437,   459,   463,
   468,   472,   477,   479,   480,   483,   485,   488,   507,   519,
   533,   539,   543,   572,   578,   580,   581,   584,   589,   594,
   599,   606,   615,   626,   643,   648,   652,   662,   668,   678,
   679,   680,   683,   695,   715,   721,   727,   735,   746,   750,
   753,   756,   759,   762,   767,   769,   772,   786,   793,   802,
   806,   811,   814,   820,   828,   832,   835,   841,   847,   852,
   858,   866,   889,   893,   901,   906,   913,   920,   922,   925,
   930,   943,   949,   953,   956,   961,   963,   964,   965,   972,
   973,   974,   975,   976,   977,   979,   984,   986,   987,   988,
   989,   990,   991,   992,   993,   994,   995,   996,   997,   998,
   999,  1000,  1001,  1002,  1006,  1014,  1029,  1036,  1043,  1051,
  1077,  1079,  1082,  1084
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
"invalid_rule","invalid_block","invalid_block_list", NULL
};
#endif

static const short yyr1[] = {     0,
    63,    63,    63,    63,    64,    65,    66,    67,    67,    68,
    68,    68,    69,    69,    69,    69,    70,    70,    71,    71,
    72,    72,    73,    73,    73,    73,    73,    73,    74,    74,
    74,    75,    75,    75,    76,    76,    77,    77,    78,    78,
    79,    79,    79,    79,    80,    81,    81,    82,    83,    83,
    84,    84,    85,    85,    85,    86,    86,    87,    88,    88,
    88,    89,    89,    89,    90,    90,    90,    91,    91,    91,
    91,    91,    91,    92,    92,    93,    93,    93,    94,    94,
    94,    94,    95,    96,    97,    97,    97,    97,    98,    98,
    98,    98,    98,    98,    99,    99,   100,   100,   100,   101,
   101,   101,   101,   101,   102,   102,   102,   102,   102,   102,
   102,   103,   103,   103,   103,   103,   104,   105,   105,   106,
   106,   106,   107,   107,   107,   108,   108,   108,   108,   108,
   108,   108,   108,   108,   108,   108,   109,   109,   109,   109,
   109,   109,   109,   109,   109,   109,   109,   109,   109,   109,
   109,   109,   109,   109,   110,   110,   111,   112,   112,   113,
   114,   114,   115,   115
};

static const short yyr2[] = {     0,
     5,     2,     2,     2,     6,     5,     5,     0,     2,     0,
     2,     2,     0,     5,     3,     3,     0,     3,     0,     3,
     0,     3,     1,     1,     1,     1,     1,     1,     6,     3,
     3,     6,     3,     3,     0,     2,     1,     1,     0,     1,
     0,     1,     4,     2,     7,     0,     3,     2,     3,     3,
     3,     3,     2,     2,     0,     1,     1,     5,     1,     4,
     2,     1,     3,     2,     0,     1,     1,     2,     3,     2,
     4,     5,     4,     1,     1,     1,     2,     2,     1,     1,
     1,     1,     2,     2,     4,     8,     6,    10,     1,     1,
     1,     1,     1,     1,     1,     1,     2,     3,     6,     1,
     2,     1,     3,     1,     3,     4,     3,     5,     4,     4,
     6,     5,     2,     6,     1,     3,     2,     2,     0,     1,
     3,     2,     2,     2,     0,     1,     2,     2,     2,     2,
     3,     2,     2,     1,     2,     1,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     5,     3,     2,     3,     3,     2,
     5,     3,     1,     3
};

static const short yydefact[] = {    13,
     0,     0,     0,     0,     8,     8,     8,    10,     0,     0,
     8,     8,     8,     2,     3,     4,    17,     0,    16,    15,
     9,     8,    65,     0,     0,    12,    11,    19,     0,     0,
    74,    79,     0,     0,     8,    75,     8,     0,     0,     0,
    62,     8,     0,    76,    80,    81,    82,   104,     8,     8,
     0,     0,   100,     0,   115,     8,     8,     8,     8,     8,
     8,     8,     8,     8,     8,     8,     8,     8,     8,     8,
     8,     8,     8,     8,     8,     8,     8,     8,     8,     8,
    57,    56,     8,     0,     0,   120,   126,   136,   134,     0,
    21,    10,   162,   163,     0,    14,    97,     0,     8,    83,
    65,     0,    61,     8,     8,    64,     8,     8,    65,     0,
    68,     0,    78,    70,    77,     8,     0,   117,   118,     6,
     0,   101,     8,     0,   113,     8,   128,   129,   157,   153,
   152,   154,   139,   140,   141,   142,   143,   144,   145,   146,
   147,   148,   149,   150,   151,   130,   138,   137,   132,     0,
   133,   135,     8,   127,   122,     7,     8,     8,     0,     0,
     0,     0,     0,    10,    18,     0,    98,    65,     8,    66,
     0,     0,     5,     0,    65,    53,    54,    63,    74,    75,
     8,     0,    69,   107,   103,     8,     0,     8,   105,     0,
     8,     0,   156,     0,   131,   124,   123,   121,    31,    30,
    37,    38,     8,     0,    35,     0,     0,     8,     0,     0,
    10,    24,    25,    26,    23,    28,    27,    20,   161,   164,
     8,    84,     0,    90,    91,    92,    93,    94,    85,    89,
     8,     0,     0,    71,     0,    73,     8,   110,     0,   109,
   106,     0,     0,     8,    41,    34,    33,     0,     0,   160,
     0,    41,     0,     0,    22,     0,     8,     0,     0,    58,
    72,   108,     8,     0,   112,   155,     8,     0,     0,    42,
    36,     8,    50,    49,     0,    52,    51,   159,   158,    99,
    87,     8,    96,    95,     8,   111,   114,    48,    29,    44,
     8,     0,     8,     0,     0,     0,    32,    46,     8,    86,
    43,    65,     0,    45,     8,    88,    47,     0,     0,     0
};

static const short yydefgoto[] = {   308,
     5,     6,     7,   222,    17,     8,    28,    91,   163,   211,
    92,   164,   249,   203,   268,   269,   212,   302,   270,   213,
   214,   109,    84,    37,    38,    39,    40,    41,    42,    43,
    44,    45,   172,    46,   231,   285,    47,    51,    52,    53,
    54,    55,    85,   159,    86,    87,    88,    89,   216,   217,
    94,    95
};

static const short yypact[] = {   372,
    22,   -39,   -22,   -15,-32768,-32768,-32768,-32768,     3,    21,
-32768,-32768,-32768,    37,    37,    37,   185,    43,-32768,-32768,
-32768,-32768,   288,   173,   532,-32768,-32768,   104,   143,   110,
    58,-32768,   108,   112,-32768,    70,-32768,    46,   227,    77,
-32768,   274,   200,-32768,-32768,-32768,-32768,    89,-32768,-32768,
   101,    99,   121,    16,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,   606,   388,-32768,-32768,-32768,-32768,   223,
   149,-32768,-32768,-32768,   161,-32768,-32768,   171,-32768,-32768,
    35,    47,-32768,-32768,-32768,-32768,-32768,-32768,   346,   395,
    37,   200,-32768,    37,-32768,-32768,   199,    37,    37,-32768,
   127,   155,-32768,    38,-32768,-32768,    37,    37,    37,    37,
    37,    37,    37,    37,    37,    37,    37,    37,    37,    37,
    37,    37,    37,    37,    37,    37,    37,    37,    37,   487,
    37,    37,-32768,-32768,-32768,-32768,-32768,-32768,   570,   156,
   102,   262,   117,-32768,   185,   147,-32768,   288,    58,-32768,
   153,   136,-32768,   173,   288,    37,    37,-32768,-32768,-32768,
   274,   200,    37,    37,   177,-32768,   209,-32768,    37,   170,
-32768,   336,-32768,   435,    37,    37,    37,-32768,-32768,-32768,
-32768,-32768,-32768,   181,    17,   170,   218,-32768,   244,   248,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,   185,-32768,-32768,
-32768,    37,   239,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,   205,   539,    37,   200,    37,-32768,    37,   187,    37,
    37,   570,   284,-32768,     4,-32768,-32768,   254,   141,-32768,
   210,   135,   219,   226,   185,    24,-32768,   159,    98,-32768,
    37,    37,-32768,   284,-32768,    37,-32768,   211,    42,-32768,
-32768,-32768,-32768,-32768,    51,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,    37,-32768,    37,-32768,-32768,
-32768,   113,-32768,    98,    27,   135,-32768,    37,-32768,-32768,
-32768,   338,    34,-32768,-32768,-32768,    37,   264,   270,-32768
};

static const short yypgoto[] = {-32768,
-32768,-32768,-32768,    -1,   -84,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,    28,-32768,    41,-32768,-32768,     1,-32768,
-32768,-32768,-32768,  -162,-32768,   130,   207,   -87,   196,   -23,
   -27,-32768,   118,-32768,    82,    48,-32768,   180,-32768,   303,
-32768,  -229,  -147,-32768,   197,   307,-32768,-32768,-32768,-32768,
    -7,   -35
};


#define	YYLAST		650


static const short yytable[] = {    10,
   215,    20,   194,    14,    15,    16,    21,   165,    11,    23,
    24,    25,   117,   265,   267,   115,   125,   124,   112,    21,
    30,   178,     9,    21,    -8,    12,    21,   248,   126,    21,
    22,    -8,    13,   101,   287,   102,    21,    21,   190,    21,
   111,   114,   290,    29,   243,   169,   103,   118,   119,    21,
    18,   290,    19,   -39,   127,   128,   129,   130,   131,   132,
   133,   134,   135,   136,   137,   138,   139,   140,   141,   142,
   143,   144,   145,   146,   147,   148,   149,   150,   151,   218,
   221,   152,   280,   300,   115,   187,   182,   191,   161,   170,
   306,   -40,   291,   104,   264,   173,   105,   168,   293,   121,
    21,   291,   174,   175,    21,   176,   177,   283,   284,    49,
   183,   201,    21,   -67,   184,    21,    -1,   206,    97,    90,
    98,   189,   100,    50,   192,   -66,   255,    31,    32,    33,
    34,    35,   110,   207,   208,   209,    18,    21,   116,   305,
   224,   225,   226,   227,   228,   267,   202,  -102,  -119,   120,
   201,   195,   200,    99,   115,   196,   197,   235,   220,    96,
   205,   166,   297,   224,   225,   226,   227,   228,    18,   162,
   123,    36,   -65,    48,    18,    21,   186,   220,   210,   234,
   236,   167,   220,    49,   238,   202,   240,    26,    27,   241,
    18,    93,   229,   230,    18,   219,   247,    50,   250,   185,
   113,   245,    -8,    18,   188,   199,   252,   115,   223,   239,
    -8,    32,    33,    34,    35,   281,   230,    18,   251,   256,
  -119,  -119,  -119,   160,    18,    -8,   237,   106,    18,   259,
   246,   220,    -8,   261,    18,   262,   263,   -55,   -55,   -55,
   -55,   -55,   266,   274,   253,   277,   279,    -8,   254,   257,
    -8,    -8,    -8,   260,    -8,    -8,   271,    18,    -8,   273,
   289,   286,   204,   309,    -8,   288,    18,    -8,   276,   310,
   292,    -8,    -8,    18,   -59,   278,   272,   -59,   107,   108,
   294,   -55,   -55,   295,   155,    32,    33,    34,    35,   296,
    21,   298,   275,  -125,  -125,  -125,   301,   303,    31,    32,
    33,    34,    35,   307,   233,   181,    -8,   171,    50,  -125,
  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,
  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,
  -125,  -119,  -119,  -119,   157,  -125,   242,  -125,    21,   282,
   258,   299,    36,   158,  -125,    56,    57,    58,    31,    32,
    33,    34,    35,   232,   122,   198,    31,    32,    33,    34,
    35,    59,    60,    61,    62,    63,    64,    65,    66,    67,
    68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
    78,    79,    80,  -116,  -116,  -116,   304,    81,   155,    82,
   154,     1,    36,     2,     3,     4,    83,  -125,  -125,  -125,
    36,     0,     0,     0,     0,   179,    32,    33,    34,    35,
     0,     0,     0,  -125,  -125,  -125,  -125,  -125,  -125,  -125,
  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,
  -125,  -125,  -125,  -125,  -125,   155,   156,     0,   157,  -125,
     0,  -125,     0,     0,  -125,  -125,  -125,   158,  -125,   180,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,
  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,  -125,
  -125,  -125,     0,     0,     0,   157,  -125,   193,  -125,    21,
     0,     0,     0,   244,   158,  -125,    56,    57,    58,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,    59,    60,    61,    62,    63,    64,    65,    66,
    67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
    77,    78,    79,    80,    21,     0,     0,     0,    81,   106,
    82,    56,    57,    58,     0,     0,     0,    83,     0,   -55,
   -55,   -55,   -55,   -55,     0,     0,     0,    59,    60,    61,
    62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
    72,    73,    74,    75,    76,    77,    78,    79,    80,    56,
    57,    58,     0,    81,     0,    82,   -60,     0,     0,   -60,
   107,   108,    83,   -55,   -55,    59,    60,    61,    62,    63,
    64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
    74,    75,    76,    77,    78,    79,    80,     0,     0,     0,
     0,    81,     0,    82,     0,     0,     0,     0,     0,     0,
    83,    59,    60,    61,    62,    63,    64,    65,    66,    67,
    68,    69,    70,    71,    72,    73,    74,   153,    76,    77
};

static const short yycheck[] = {     1,
   163,     9,   150,     5,     6,     7,     3,    92,    48,    11,
    12,    13,    48,   243,    11,    43,     1,    53,    42,     3,
    22,   109,     1,     3,     3,    48,     3,    11,    13,     3,
    10,    10,    48,    35,   264,    37,     3,     3,     1,     3,
    42,    43,     1,     1,   192,    11,     1,    49,    50,     3,
    48,     1,    50,    50,    56,    57,    58,    59,    60,    61,
    62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
    72,    73,    74,    75,    76,    77,    78,    79,    80,   164,
   168,    83,    59,    57,   112,   121,   110,    50,    90,    55,
    57,    50,    51,    48,   242,    49,    51,    99,    48,     1,
     3,    51,   104,   105,     3,   107,   108,    10,    11,    11,
   112,    10,     3,    56,   116,     3,     0,     1,    11,    16,
    13,   123,    11,    25,   126,    56,   211,    11,    12,    13,
    14,    15,    56,    17,    18,    19,    48,     3,    50,   302,
     5,     6,     7,     8,     9,    11,    45,    49,    50,    49,
    10,   153,   160,    46,   182,   157,   158,   181,   166,    50,
   162,     1,    50,     5,     6,     7,     8,     9,    48,    21,
    50,    55,    56,     1,    48,     3,    50,   185,    62,   181,
   182,    11,   190,    11,   186,    45,   188,     3,     4,   191,
    48,    49,    57,    58,    48,    49,   204,    25,   206,     1,
     1,   203,     3,    48,    50,    50,   208,   235,    56,     1,
    11,    12,    13,    14,    15,    57,    58,    48,     1,   221,
    48,    49,    50,     1,    48,     3,    50,     1,    48,   231,
    50,   239,    10,   235,    48,   237,    50,    11,    12,    13,
    14,    15,   244,   251,     1,   253,   254,    48,     1,    11,
    51,    52,    53,    49,    55,    56,     3,    48,    59,    50,
    50,   263,     1,     0,     3,   267,    48,    45,    50,     0,
   272,    10,    11,    48,    48,    50,   249,    51,    52,    53,
   282,    55,    56,   285,     1,    12,    13,    14,    15,   291,
     3,   293,   252,    10,    11,    12,   296,   299,    11,    12,
    13,    14,    15,   305,   175,   110,    45,   101,    25,    26,
    27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
    37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
    47,    48,    49,    50,    51,    52,     1,    54,     3,   258,
   223,   294,    55,    60,    61,    10,    11,    12,    11,    12,
    13,    14,    15,   174,    52,   159,    11,    12,    13,    14,
    15,    26,    27,    28,    29,    30,    31,    32,    33,    34,
    35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
    45,    46,    47,    48,    49,    50,    49,    52,     1,    54,
    84,    20,    55,    22,    23,    24,    61,    10,    11,    12,
    55,    -1,    -1,    -1,    -1,    11,    12,    13,    14,    15,
    -1,    -1,    -1,    26,    27,    28,    29,    30,    31,    32,
    33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
    43,    44,    45,    46,    47,     1,    49,    -1,    51,    52,
    -1,    54,    -1,    -1,    10,    11,    12,    60,    61,    55,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
    36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
    46,    47,    -1,    -1,    -1,    51,    52,     1,    54,     3,
    -1,    -1,    -1,    59,    60,    61,    10,    11,    12,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    26,    27,    28,    29,    30,    31,    32,    33,
    34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
    44,    45,    46,    47,     3,    -1,    -1,    -1,    52,     1,
    54,    10,    11,    12,    -1,    -1,    -1,    61,    -1,    11,
    12,    13,    14,    15,    -1,    -1,    -1,    26,    27,    28,
    29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
    39,    40,    41,    42,    43,    44,    45,    46,    47,    10,
    11,    12,    -1,    52,    -1,    54,    48,    -1,    -1,    51,
    52,    53,    61,    55,    56,    26,    27,    28,    29,    30,
    31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
    41,    42,    43,    44,    45,    46,    47,    -1,    -1,    -1,
    -1,    52,    -1,    54,    -1,    -1,    -1,    -1,    -1,    -1,
    61,    26,    27,    28,    29,    30,    31,    32,    33,    34,
    35,    36,    37,    38,    39,    40,    41,    42,    43,    44
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
#line 244 "parser.y"
{
        CSSParser *p = static_cast<CSSParser *>(parser);
        p->rule = yyvsp[-2].rule;
    ;
    break;}
case 6:
#line 251 "parser.y"
{
	/* can be empty */
    ;
    break;}
case 7:
#line 257 "parser.y"
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
#line 294 "parser.y"
{
#ifdef CSS_DEBUG
     kdDebug( 6080 ) << "charset rule: " << qString(yyvsp[-2].string) << endl;
#endif
 ;
    break;}
case 18:
#line 305 "parser.y"
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
#line 322 "parser.y"
{
     CSSParser *p = static_cast<CSSParser *>(parser);
     if ( yyvsp[-1].rule && p->styleElement && p->styleElement->isCSSStyleSheet() ) {
	 p->styleElement->append( yyvsp[-1].rule );
     } else {
	 delete yyvsp[-1].rule;
     }
 ;
    break;}
case 29:
#line 342 "parser.y"
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
case 30:
#line 354 "parser.y"
{
        yyval.rule = 0;
    ;
    break;}
case 31:
#line 357 "parser.y"
{
        yyval.rule = 0;
    ;
    break;}
case 32:
#line 363 "parser.y"
{
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "@namespace: " << qString(yyvsp[-2].string) << endl;
#endif
    CSSParser *p = static_cast<CSSParser *>(parser);
    if (p->styleElement && p->styleElement->isCSSStyleSheet())
        static_cast<CSSStyleSheetImpl*>(p->styleElement)->addNamespace(p, domString(yyvsp[-3].string), domString(yyvsp[-2].string));
;
    break;}
case 35:
#line 376 "parser.y"
{ yyval.string.string = 0; ;
    break;}
case 36:
#line 377 "parser.y"
{ yyval.string = yyvsp[-1].string; ;
    break;}
case 39:
#line 386 "parser.y"
{
        yyval.mediaList = new MediaListImpl();
     ;
    break;}
case 41:
#line 394 "parser.y"
{
	yyval.mediaList = 0;
    ;
    break;}
case 42:
#line 397 "parser.y"
{
	yyval.mediaList = new MediaListImpl();
	yyval.mediaList->appendMedium( domString(yyvsp[0].string).lower() );
    ;
    break;}
case 43:
#line 401 "parser.y"
{
	yyval.mediaList = yyvsp[-3].mediaList;
        if (yyval.mediaList)
	    yyval.mediaList->appendMedium( domString(yyvsp[0].string) );
    ;
    break;}
case 44:
#line 406 "parser.y"
{
        delete yyvsp[-1].mediaList;
        yyval.mediaList = 0;
    ;
    break;}
case 45:
#line 413 "parser.y"
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
case 46:
#line 427 "parser.y"
{ yyval.ruleList = 0; ;
    break;}
case 47:
#line 428 "parser.y"
{
      yyval.ruleList = yyvsp[-2].ruleList;
      if ( yyvsp[-1].rule ) {
	  if ( !yyval.ruleList ) yyval.ruleList = new CSSRuleListImpl();
	  yyval.ruleList->append( yyvsp[-1].rule );
      }
  ;
    break;}
case 48:
#line 438 "parser.y"
{
      yyval.string = yyvsp[-1].string;
  ;
    break;}
case 49:
#line 460 "parser.y"
{
      yyval.rule = 0;
    ;
    break;}
case 50:
#line 463 "parser.y"
{
      yyval.rule = 0;
    ;
    break;}
case 51:
#line 469 "parser.y"
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
#line 478 "parser.y"
{ yyval.relation = CSSSelector::Sibling; ;
    break;}
case 54:
#line 479 "parser.y"
{ yyval.relation = CSSSelector::Child; ;
    break;}
case 55:
#line 480 "parser.y"
{ yyval.relation = CSSSelector::Descendant; ;
    break;}
case 56:
#line 484 "parser.y"
{ yyval.val = -1; ;
    break;}
case 57:
#line 485 "parser.y"
{ yyval.val = 1; ;
    break;}
case 58:
#line 489 "parser.y"
{
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "got ruleset" << endl << "  selector:" << endl;
#endif
	CSSParser *p = static_cast<CSSParser *>(parser);
	if ( yyvsp[-4].selector ) {
            CSSStyleRuleImpl *rule = new CSSStyleRuleImpl( p->styleElement );
            CSSStyleDeclarationImpl *decl = p->createStyleDeclaration( rule );
            rule->setSelector( yyvsp[-4].selector );
            rule->setDeclaration(decl);
            yyval.rule = rule;
	} else {
	    yyval.rule = 0;
	    p->clearProperties();
	}
    ;
    break;}
case 59:
#line 508 "parser.y"
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
case 60:
#line 519 "parser.y"
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
case 61:
#line 533 "parser.y"
{
        delete yyvsp[-1].selector;
        yyval.selector = 0;
    ;
    break;}
case 62:
#line 540 "parser.y"
{
	yyval.selector = yyvsp[0].selector;
    ;
    break;}
case 63:
#line 543 "parser.y"
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
case 64:
#line 572 "parser.y"
{
        delete yyvsp[-1].selector;
        yyval.selector = 0;
    ;
    break;}
case 65:
#line 579 "parser.y"
{ yyval.string.string = 0; yyval.string.length = 0; ;
    break;}
case 66:
#line 580 "parser.y"
{ static unsigned short star = '*'; yyval.string.string = &star; yyval.string.length = 1; ;
    break;}
case 67:
#line 581 "parser.y"
{ yyval.string = yyvsp[0].string; ;
    break;}
case 68:
#line 585 "parser.y"
{
	yyval.selector = new CSSSelector();
	yyval.selector->tag = yyvsp[-1].element;
    ;
    break;}
case 69:
#line 589 "parser.y"
{
	yyval.selector = yyvsp[-1].selector;
	if ( yyval.selector )
            yyval.selector->tag = yyvsp[-2].element;
    ;
    break;}
case 70:
#line 594 "parser.y"
{
	yyval.selector = yyvsp[-1].selector;
        if (yyval.selector)
            yyval.selector->tag = makeId(static_cast<CSSParser*>(parser)->defaultNamespace, anyLocalName);;
    ;
    break;}
case 71:
#line 599 "parser.y"
{
        yyval.selector = new CSSSelector();
        yyval.selector->tag = yyvsp[-1].element;
        CSSParser *p = static_cast<CSSParser *>(parser);
        if (p->styleElement && p->styleElement->isCSSStyleSheet())
            static_cast<CSSStyleSheetImpl*>(p->styleElement)->determineNamespace(yyval.selector->tag, domString(yyvsp[-3].string));
    ;
    break;}
case 72:
#line 606 "parser.y"
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
case 73:
#line 615 "parser.y"
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
case 74:
#line 627 "parser.y"
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
case 75:
#line 643 "parser.y"
{
	yyval.element = makeId(static_cast<CSSParser*>(parser)->defaultNamespace, anyLocalName);
    ;
    break;}
case 76:
#line 649 "parser.y"
{
	yyval.selector = yyvsp[0].selector;
    ;
    break;}
case 77:
#line 652 "parser.y"
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
case 78:
#line 662 "parser.y"
{
        delete yyvsp[-1].selector;
        yyval.selector = 0;
    ;
    break;}
case 79:
#line 669 "parser.y"
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
case 83:
#line 684 "parser.y"
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
case 84:
#line 696 "parser.y"
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
case 85:
#line 716 "parser.y"
{
	yyval.selector = new CSSSelector();
	yyval.selector->attr = yyvsp[-1].attribute;
	yyval.selector->match = CSSSelector::Set;
    ;
    break;}
case 86:
#line 721 "parser.y"
{
	yyval.selector = new CSSSelector();
	yyval.selector->attr = yyvsp[-5].attribute;
	yyval.selector->match = (CSSSelector::Match)yyvsp[-4].val;
	yyval.selector->value = atomicString(yyvsp[-2].string);
    ;
    break;}
case 87:
#line 727 "parser.y"
{
        yyval.selector = new CSSSelector();
        yyval.selector->attr = yyvsp[-1].attribute;
        yyval.selector->match = CSSSelector::Set;
        CSSParser *p = static_cast<CSSParser *>(parser);
        if (p->styleElement && p->styleElement->isCSSStyleSheet())
            static_cast<CSSStyleSheetImpl*>(p->styleElement)->determineNamespace(yyval.selector->attr, domString(yyvsp[-3].string));
    ;
    break;}
case 88:
#line 735 "parser.y"
{
        yyval.selector = new CSSSelector();
        yyval.selector->attr = yyvsp[-5].attribute;
        yyval.selector->match = (CSSSelector::Match)yyvsp[-4].val;
        yyval.selector->value = atomicString(yyvsp[-2].string);
        CSSParser *p = static_cast<CSSParser *>(parser);
        if (p->styleElement && p->styleElement->isCSSStyleSheet())
            static_cast<CSSStyleSheetImpl*>(p->styleElement)->determineNamespace(yyval.selector->attr, domString(yyvsp[-7].string));
    ;
    break;}
case 89:
#line 747 "parser.y"
{
	yyval.val = CSSSelector::Exact;
    ;
    break;}
case 90:
#line 750 "parser.y"
{
	yyval.val = CSSSelector::List;
    ;
    break;}
case 91:
#line 753 "parser.y"
{
	yyval.val = CSSSelector::Hyphen;
    ;
    break;}
case 92:
#line 756 "parser.y"
{
	yyval.val = CSSSelector::Begin;
    ;
    break;}
case 93:
#line 759 "parser.y"
{
	yyval.val = CSSSelector::End;
    ;
    break;}
case 94:
#line 762 "parser.y"
{
	yyval.val = CSSSelector::Contain;
    ;
    break;}
case 97:
#line 773 "parser.y"
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
case 98:
#line 787 "parser.y"
{
        yyval.selector = new CSSSelector();
        yyval.selector->match = CSSSelector::Pseudo;
        yyvsp[0].string.lower();
        yyval.selector->value = atomicString(yyvsp[0].string);
    ;
    break;}
case 99:
#line 793 "parser.y"
{
        yyval.selector = new CSSSelector();
        yyval.selector->match = CSSSelector::Pseudo;
        yyval.selector->simpleSelector = yyvsp[-2].selector;
        yyvsp[-4].string.lower();
        yyval.selector->value = atomicString(yyvsp[-4].string);
    ;
    break;}
case 100:
#line 803 "parser.y"
{
	yyval.ok = yyvsp[0].ok;
    ;
    break;}
case 101:
#line 806 "parser.y"
{
	yyval.ok = yyvsp[-1].ok;
	if ( yyvsp[0].ok )
	    yyval.ok = yyvsp[0].ok;
    ;
    break;}
case 102:
#line 811 "parser.y"
{
	yyval.ok = yyvsp[0].ok;
    ;
    break;}
case 103:
#line 814 "parser.y"
{
	yyval.ok = false;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    ;
    break;}
case 104:
#line 820 "parser.y"
{
	yyval.ok = false;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping all declarations" << endl;
#endif
    ;
    break;}
case 105:
#line 829 "parser.y"
{
	yyval.ok = yyvsp[-2].ok;
    ;
    break;}
case 106:
#line 832 "parser.y"
{
        yyval.ok = false;
    ;
    break;}
case 107:
#line 835 "parser.y"
{
	yyval.ok = false;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    ;
    break;}
case 108:
#line 841 "parser.y"
{
	yyval.ok = false;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    ;
    break;}
case 109:
#line 847 "parser.y"
{
	yyval.ok = yyvsp[-3].ok;
	if ( yyvsp[-2].ok )
	    yyval.ok = yyvsp[-2].ok;
    ;
    break;}
case 110:
#line 852 "parser.y"
{
	yyval.ok = yyvsp[-3].ok;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    ;
    break;}
case 111:
#line 858 "parser.y"
{
	yyval.ok = yyvsp[-5].ok;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipping bogus declaration" << endl;
#endif
    ;
    break;}
case 112:
#line 867 "parser.y"
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
case 113:
#line 890 "parser.y"
{
        yyval.ok = false;
    ;
    break;}
case 114:
#line 894 "parser.y"
{
        /* The default movable type template has letter-spacing: .none;  Handle this by looking for
        error tokens at the start of an expr, recover the expr and then treat as an error, cleaning
        up and deleting the shifted expr.  */
        delete yyvsp[-1].valueList;
        yyval.ok = false;
    ;
    break;}
case 115:
#line 902 "parser.y"
{
        /* Handle this case: div { text-align: center; !important } Just reduce away the stray !important. */
        yyval.ok = false;
    ;
    break;}
case 116:
#line 907 "parser.y"
{
        /* div { font-family: } Just reduce away this property with no value. */
        yyval.ok = false;
    ;
    break;}
case 117:
#line 914 "parser.y"
{
	QString str = qString(yyvsp[-1].string);
	yyval.prop_id = getPropertyID( str.lower().latin1(), str.length() );
    ;
    break;}
case 118:
#line 921 "parser.y"
{ yyval.b = true; ;
    break;}
case 119:
#line 922 "parser.y"
{ yyval.b = false; ;
    break;}
case 120:
#line 926 "parser.y"
{
	yyval.valueList = new ValueList;
	yyval.valueList->addValue( yyvsp[0].value );
    ;
    break;}
case 121:
#line 930 "parser.y"
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
case 122:
#line 943 "parser.y"
{
        delete yyvsp[-1].valueList;
        yyval.valueList = 0;
    ;
    break;}
case 123:
#line 950 "parser.y"
{
	yyval.tok = '/';
    ;
    break;}
case 124:
#line 953 "parser.y"
{
	yyval.tok = ',';
    ;
    break;}
case 125:
#line 956 "parser.y"
{
        yyval.tok = 0;
  ;
    break;}
case 126:
#line 962 "parser.y"
{ yyval.value = yyvsp[0].value; ;
    break;}
case 127:
#line 963 "parser.y"
{ yyval.value = yyvsp[0].value; yyval.value.fValue *= yyvsp[-1].val; ;
    break;}
case 128:
#line 964 "parser.y"
{ yyval.value.id = 0; yyval.value.string = yyvsp[-1].string; yyval.value.unit = CSSPrimitiveValue::CSS_STRING; ;
    break;}
case 129:
#line 965 "parser.y"
{
      QString str = qString( yyvsp[-1].string );
      yyval.value.id = getValueID( str.lower().latin1(), str.length() );
      yyval.value.unit = CSSPrimitiveValue::CSS_IDENT;
      yyval.value.string = yyvsp[-1].string;
  ;
    break;}
case 130:
#line 972 "parser.y"
{ yyval.value.id = 0; yyval.value.string = yyvsp[-1].string; yyval.value.unit = CSSPrimitiveValue::CSS_DIMENSION ;
    break;}
case 131:
#line 973 "parser.y"
{ yyval.value.id = 0; yyval.value.string = yyvsp[-1].string; yyval.value.unit = CSSPrimitiveValue::CSS_DIMENSION ;
    break;}
case 132:
#line 974 "parser.y"
{ yyval.value.id = 0; yyval.value.string = yyvsp[-1].string; yyval.value.unit = CSSPrimitiveValue::CSS_URI; ;
    break;}
case 133:
#line 975 "parser.y"
{ yyval.value.id = 0; yyval.value.iValue = 0; yyval.value.unit = CSSPrimitiveValue::CSS_UNKNOWN;/* ### */ ;
    break;}
case 134:
#line 976 "parser.y"
{ yyval.value.id = 0; yyval.value.string = yyvsp[0].string; yyval.value.unit = CSSPrimitiveValue::CSS_RGBCOLOR; ;
    break;}
case 135:
#line 977 "parser.y"
{ yyval.value.id = 0; yyval.value.string = ParseString(); yyval.value.unit = CSSPrimitiveValue::CSS_RGBCOLOR; ;
    break;}
case 136:
#line 979 "parser.y"
{
      yyval.value = yyvsp[0].value;
  ;
    break;}
case 137:
#line 985 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_NUMBER; ;
    break;}
case 138:
#line 986 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_PERCENTAGE; ;
    break;}
case 139:
#line 987 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_PX; ;
    break;}
case 140:
#line 988 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_CM; ;
    break;}
case 141:
#line 989 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_MM; ;
    break;}
case 142:
#line 990 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_IN; ;
    break;}
case 143:
#line 991 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_PT; ;
    break;}
case 144:
#line 992 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_PC; ;
    break;}
case 145:
#line 993 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_DEG; ;
    break;}
case 146:
#line 994 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_RAD; ;
    break;}
case 147:
#line 995 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_GRAD; ;
    break;}
case 148:
#line 996 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_MS; ;
    break;}
case 149:
#line 997 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_S; ;
    break;}
case 150:
#line 998 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_HZ; ;
    break;}
case 151:
#line 999 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_KHZ; ;
    break;}
case 152:
#line 1000 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_EMS; ;
    break;}
case 153:
#line 1001 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = Value::Q_EMS; ;
    break;}
case 154:
#line 1002 "parser.y"
{ yyval.value.id = 0; yyval.value.fValue = yyvsp[-1].val; yyval.value.unit = CSSPrimitiveValue::CSS_EXS; ;
    break;}
case 155:
#line 1007 "parser.y"
{
      Function *f = new Function;
      f->name = yyvsp[-4].string;
      f->args = yyvsp[-2].valueList;
      yyval.value.id = 0;
      yyval.value.unit = Value::Function;
      yyval.value.function = f;
  ;
    break;}
case 156:
#line 1015 "parser.y"
{
      Function *f = new Function;
      f->name = yyvsp[-2].string;
      f->args = 0;
      yyval.value.id = 0;
      yyval.value.unit = Value::Function;
      yyval.value.function = f;
  ;
    break;}
case 157:
#line 1030 "parser.y"
{ yyval.string = yyvsp[-1].string; ;
    break;}
case 158:
#line 1037 "parser.y"
{
	yyval.rule = 0;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipped invalid @-rule" << endl;
#endif
    ;
    break;}
case 159:
#line 1043 "parser.y"
{
	yyval.rule = 0;
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "skipped invalid @-rule" << endl;
#endif
    ;
    break;}
case 160:
#line 1052 "parser.y"
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
#line 1087 "parser.y"


