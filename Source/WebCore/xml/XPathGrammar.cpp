/* This file is generated using the following command:
   bison -d -p xpathyy XPathGrammar.y -o XPathGrammar.cpp 
 */

/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.3"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Using locations.  */
#define YYLSP_NEEDED 0

/* Substitute the variable and function names.  */
#define yyparse xpathyyparse
#define yylex   xpathyylex
#define yyerror xpathyyerror
#define yylval  xpathyylval
#define yychar  xpathyychar
#define yydebug xpathyydebug
#define yynerrs xpathyynerrs


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     MULOP = 258,
     RELOP = 259,
     EQOP = 260,
     MINUS = 261,
     PLUS = 262,
     AND = 263,
     OR = 264,
     FUNCTIONNAME = 265,
     LITERAL = 266,
     NAMETEST = 267,
     NUMBER = 268,
     NODETYPE = 269,
     VARIABLEREFERENCE = 270,
     AXISNAME = 271,
     COMMENT = 272,
     DOTDOT = 273,
     PI = 274,
     NODE = 275,
     SLASHSLASH = 276,
     TEXT_ = 277,
     XPATH_ERROR = 278
   };
#endif
/* Tokens.  */
#define MULOP 258
#define RELOP 259
#define EQOP 260
#define MINUS 261
#define PLUS 262
#define AND 263
#define OR 264
#define FUNCTIONNAME 265
#define LITERAL 266
#define NAMETEST 267
#define NUMBER 268
#define NODETYPE 269
#define VARIABLEREFERENCE 270
#define AXISNAME 271
#define COMMENT 272
#define DOTDOT 273
#define PI 274
#define NODE 275
#define SLASHSLASH 276
#define TEXT_ 277
#define XPATH_ERROR 278



/* Copy the first part of user declarations.  */
#line 28 "WebCore/xml/XPathGrammar.y"


#include "config.h"

#include "XPathFunctions.h"
#include "XPathParser.h"
#include "XPathPath.h"
#include "XPathStep.h"
#include "XPathVariableReference.h"


#if COMPILER(MSVC)
// See https://msdn.microsoft.com/en-us/library/1wea5zwe.aspx
#pragma warning(disable: 4701)
#endif

#define YYMALLOC fastMalloc
#define YYFREE fastFree

#define YYENABLE_NLS 0
#define YYLTYPE_IS_TRIVIAL 1
#define YYDEBUG 0
#define YYMAXDEPTH 10000

using namespace WebCore;
using namespace XPath;


/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif

#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 60 "WebCore/xml/XPathGrammar.y"
{ 
    NumericOp::Opcode numericOpcode; 
    EqTestOp::Opcode equalityTestOpcode;
    StringImpl* string;
    Step::Axis axis;
    LocationPath* locationPath;
    Step::NodeTest* nodeTest;
    Vector<std::unique_ptr<Expression>>* expressionVector;
    Step* step;
    Expression* expression; 
}
/* Line 193 of yacc.c.  */
#line 190 "./XPathGrammar.cpp"
    YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */
#line 104 "WebCore/xml/XPathGrammar.y"


static int xpathyylex(YYSTYPE* yylval, Parser& parser) { return parser.lex(*yylval); }
static void xpathyyerror(Parser&, const char*) { }



/* Line 216 of yacc.c.  */
#line 209 "./XPathGrammar.cpp"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int i)
#else
static int
YYID (i)
    int i;
#endif
{
  return i;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
         && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
     || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)        \
      do                    \
    {                    \
      YYSIZE_T yyi;                \
      for (yyi = 0; yyi < (Count); yyi++)    \
        (To)[yyi] = (From)[yyi];        \
    }                    \
      while (YYID (0))
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)                    \
    do                                    \
      {                                    \
    YYSIZE_T yynewbytes;                        \
    YYCOPY (&yyptr->Stack, Stack, yysize);                \
    Stack = &yyptr->Stack;                        \
    yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
    yyptr += yynewbytes / sizeof (*yyptr);                \
      }                                    \
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  52
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   132

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  33
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  28
/* YYNRULES -- Number of rules.  */
#define YYNRULES  64
/* YYNRULES -- Number of states.  */
#define YYNSTATES  101

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   278

#define YYTRANSLATE(YYX)                        \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      26,    27,     2,     2,    31,     2,    30,    24,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    25,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    28,     2,    29,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    32,     2,     2,     2,     2,     2,
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
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint8 yyprhs[] =
{
       0,     0,     3,     5,     7,     9,    11,    13,    16,    19,
      21,    25,    29,    32,    35,    39,    43,    45,    47,    49,
      53,    57,    61,    65,    70,    71,    73,    75,    78,    82,
      84,    86,    88,    90,    94,    96,    98,   100,   104,   109,
     111,   115,   117,   119,   123,   125,   127,   131,   135,   137,
     140,   142,   146,   148,   152,   154,   158,   160,   164,   166,
     170,   174,   176,   180,   182
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      34,     0,    -1,    35,    -1,    54,    -1,    37,    -1,    38,
      -1,    24,    -1,    24,    38,    -1,    45,    38,    -1,    39,
      -1,    38,    24,    39,    -1,    38,    45,    39,    -1,    41,
      42,    -1,    12,    42,    -1,    40,    41,    42,    -1,    40,
      12,    42,    -1,    46,    -1,    16,    -1,    25,    -1,    20,
      26,    27,    -1,    22,    26,    27,    -1,    17,    26,    27,
      -1,    19,    26,    27,    -1,    19,    26,    11,    27,    -1,
      -1,    43,    -1,    44,    -1,    43,    44,    -1,    28,    35,
      29,    -1,    21,    -1,    30,    -1,    18,    -1,    15,    -1,
      26,    35,    27,    -1,    11,    -1,    13,    -1,    48,    -1,
      10,    26,    27,    -1,    10,    26,    49,    27,    -1,    50,
      -1,    49,    31,    50,    -1,    35,    -1,    52,    -1,    51,
      32,    52,    -1,    36,    -1,    53,    -1,    53,    24,    38,
      -1,    53,    45,    38,    -1,    47,    -1,    47,    43,    -1,
      55,    -1,    54,     9,    55,    -1,    56,    -1,    55,     8,
      56,    -1,    57,    -1,    56,     5,    57,    -1,    58,    -1,
      57,     4,    58,    -1,    59,    -1,    58,     7,    59,    -1,
      58,     6,    59,    -1,    60,    -1,    59,     3,    60,    -1,
      51,    -1,     6,    60,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   114,   114,   121,   125,   131,   135,   140,   145,   153,
     159,   165,   174,   184,   202,   213,   231,   235,   237,   244,
     249,   254,   259,   264,   273,   277,   281,   287,   295,   302,
     309,   314,   321,   327,   332,   338,   344,   348,   356,   367,
     373,   381,   385,   387,   394,   399,   401,   407,   416,   418,
     426,   428,   435,   437,   444,   446,   453,   455,   462,   464,
     469,   476,   478,   485,   487
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "MULOP", "RELOP", "EQOP", "MINUS",
  "PLUS", "AND", "OR", "FUNCTIONNAME", "LITERAL", "NAMETEST", "NUMBER",
  "NODETYPE", "VARIABLEREFERENCE", "AXISNAME", "COMMENT", "DOTDOT", "PI",
  "NODE", "SLASHSLASH", "TEXT", "XPATH_ERROR", "'/'", "'@'", "'('", "')'",
  "'['", "']'", "'.'", "','", "'|'", "$accept", "Top", "Expr",
  "LocationPath", "AbsoluteLocationPath", "RelativeLocationPath", "Step",
  "AxisSpecifier", "NodeTest", "OptionalPredicateList", "PredicateList",
  "Predicate", "DescendantOrSelf", "AbbreviatedStep", "PrimaryExpr",
  "FunctionCall", "ArgumentList", "Argument", "UnionExpr", "PathExpr",
  "FilterExpr", "OrExpr", "AndExpr", "EqualityExpr", "RelationalExpr",
  "AdditiveExpr", "MultiplicativeExpr", "UnaryExpr", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,    47,    64,    40,    41,    91,    93,
      46,    44,   124
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    33,    34,    35,    36,    36,    37,    37,    37,    38,
      38,    38,    39,    39,    39,    39,    39,    40,    40,    41,
      41,    41,    41,    41,    42,    42,    43,    43,    44,    45,
      46,    46,    47,    47,    47,    47,    47,    48,    48,    49,
      49,    50,    51,    51,    52,    52,    52,    52,    53,    53,
      54,    54,    55,    55,    56,    56,    57,    57,    58,    58,
      58,    59,    59,    60,    60
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     1,     1,     1,     1,     2,     2,     1,
       3,     3,     2,     2,     3,     3,     1,     1,     1,     3,
       3,     3,     3,     4,     0,     1,     1,     2,     3,     1,
       1,     1,     1,     3,     1,     1,     1,     3,     4,     1,
       3,     1,     1,     3,     1,     1,     3,     3,     1,     2,
       1,     3,     1,     3,     1,     3,     1,     3,     1,     3,
       3,     1,     3,     1,     2
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     0,     0,    34,    24,    35,    32,    17,     0,    31,
       0,     0,    29,     0,     6,    18,     0,    30,     0,     2,
      44,     4,     5,     9,     0,    24,     0,    16,    48,    36,
      63,    42,    45,     3,    50,    52,    54,    56,    58,    61,
      64,     0,     0,    13,    25,    26,     0,     0,     0,     0,
       7,     0,     1,     0,     0,    24,    24,    12,     8,    49,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      37,    41,     0,    39,     0,    27,    21,     0,    22,    19,
      20,    33,    10,    11,    15,    14,    43,    46,    47,    51,
      53,    55,    57,    60,    59,    62,    38,     0,    28,    23,
      40
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
      -1,    18,    71,    20,    21,    22,    23,    24,    25,    43,
      44,    45,    26,    27,    28,    29,    72,    73,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -37
static const yytype_int8 yypact[] =
{
      81,    81,    -8,   -37,    -7,   -37,   -37,   -37,    14,   -37,
      17,    20,   -37,    21,     8,   -37,    81,   -37,    48,   -37,
     -37,   -37,   -17,   -37,    22,    -7,     8,   -37,    -7,   -37,
      23,   -37,    -9,     1,    45,    49,    52,    10,    54,   -37,
     -37,    59,    81,   -37,    -7,   -37,    31,    -5,    32,    33,
     -17,    34,   -37,     8,     8,    -7,    -7,   -37,   -17,    -7,
     102,     8,     8,    81,    81,    81,    81,    81,    81,    81,
     -37,   -37,   -18,   -37,    35,   -37,   -37,    36,   -37,   -37,
     -37,   -37,   -37,   -37,   -37,   -37,   -37,   -17,   -17,    45,
      49,    52,    10,    54,    54,   -37,   -37,    81,   -37,   -37,
     -37
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -37,   -37,     3,   -37,   -37,   -12,   -22,   -37,    38,   -20,
      60,   -36,   -21,   -37,   -37,   -37,   -37,   -24,   -37,    30,
     -37,   -37,    19,    40,    43,    29,   -16,    -1
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const yytype_uint8 yytable[] =
{
      40,    54,    50,    19,    12,    57,    77,    53,    75,    96,
      63,    62,    12,    97,    58,    61,    67,    68,    41,    51,
       4,    42,    78,    75,     7,     8,     9,    10,    11,    54,
      13,    82,    83,    15,    55,    84,    85,    54,    17,     8,
      46,    10,    11,    47,    13,    74,    48,    49,    52,    87,
      88,    93,    94,    64,    65,    60,    66,    69,    76,    79,
      80,    81,    56,    99,    98,     1,    54,    54,    95,     2,
       3,     4,     5,   100,     6,     7,     8,     9,    10,    11,
      12,    13,    89,    14,    15,    16,    70,     1,    59,    17,
      86,     2,     3,     4,     5,    92,     6,     7,     8,     9,
      10,    11,    12,    13,    90,    14,    15,    16,    91,     0,
       0,    17,     2,     3,     4,     5,     0,     6,     7,     8,
       9,    10,    11,    12,    13,     0,    14,    15,    16,     0,
       0,     0,    17
};

static const yytype_int8 yycheck[] =
{
       1,    22,    14,     0,    21,    25,    11,    24,    44,    27,
       9,    32,    21,    31,    26,    24,     6,     7,    26,    16,
      12,    28,    27,    59,    16,    17,    18,    19,    20,    50,
      22,    53,    54,    25,    12,    55,    56,    58,    30,    17,
      26,    19,    20,    26,    22,    42,    26,    26,     0,    61,
      62,    67,    68,     8,     5,    32,     4,     3,    27,    27,
      27,    27,    24,    27,    29,     6,    87,    88,    69,    10,
      11,    12,    13,    97,    15,    16,    17,    18,    19,    20,
      21,    22,    63,    24,    25,    26,    27,     6,    28,    30,
      60,    10,    11,    12,    13,    66,    15,    16,    17,    18,
      19,    20,    21,    22,    64,    24,    25,    26,    65,    -1,
      -1,    30,    10,    11,    12,    13,    -1,    15,    16,    17,
      18,    19,    20,    21,    22,    -1,    24,    25,    26,    -1,
      -1,    -1,    30
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     6,    10,    11,    12,    13,    15,    16,    17,    18,
      19,    20,    21,    22,    24,    25,    26,    30,    34,    35,
      36,    37,    38,    39,    40,    41,    45,    46,    47,    48,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      60,    26,    28,    42,    43,    44,    26,    26,    26,    26,
      38,    35,     0,    24,    45,    12,    41,    42,    38,    43,
      32,    24,    45,     9,     8,     5,     4,     6,     7,     3,
      27,    35,    49,    50,    35,    44,    27,    11,    27,    27,
      27,    27,    39,    39,    42,    42,    52,    38,    38,    55,
      56,    57,    58,    59,    59,    60,    27,    31,    29,    27,
      50
};

#define yyerrok        (yyerrstatus = 0)
#define yyclearin    (yychar = YYEMPTY)
#define YYEMPTY        (-2)
#define YYEOF        0

#define YYACCEPT    goto yyacceptlab
#define YYABORT        goto yyabortlab
#define YYERROR        goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL        goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                    \
do                                \
  if (yychar == YYEMPTY && yylen == 1)                \
    {                                \
      yychar = (Token);                        \
      yylval = (Value);                        \
      yytoken = YYTRANSLATE (yychar);                \
      YYPOPSTACK (1);                        \
      goto yybackup;                        \
    }                                \
  else                                \
    {                                \
      yyerror (parser, YY_("syntax error: cannot back up")); \
      YYERROR;                            \
    }                                \
while (YYID (0))


#define YYTERROR    1
#define YYERRCODE    256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                \
    do                                    \
      if (YYID (N))                                                    \
    {                                \
      (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;    \
      (Current).first_column = YYRHSLOC (Rhs, 1).first_column;    \
      (Current).last_line    = YYRHSLOC (Rhs, N).last_line;        \
      (Current).last_column  = YYRHSLOC (Rhs, N).last_column;    \
    }                                \
      else                                \
    {                                \
      (Current).first_line   = (Current).last_line   =        \
        YYRHSLOC (Rhs, 0).last_line;                \
      (Current).first_column = (Current).last_column =        \
        YYRHSLOC (Rhs, 0).last_column;                \
    }                                \
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)            \
     fprintf (File, "%d.%d-%d.%d",            \
          (Loc).first_line, (Loc).first_column,    \
          (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (&yylval, YYLEX_PARAM)
#else
# define YYLEX yylex (&yylval, parser)
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)            \
do {                        \
  if (yydebug)                    \
    YYFPRINTF Args;                \
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)              \
do {                                      \
  if (yydebug)                                  \
    {                                      \
      YYFPRINTF (stderr, "%s ", Title);                      \
      yy_symbol_print (stderr,                          \
          Type, Value, parser); \
      YYFPRINTF (stderr, "\n");                          \
    }                                      \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, Parser& parser)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, parser)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    Parser& parser;
#endif
{
  if (!yyvaluep)
    return;
  YYUSE (parser);
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
    break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, Parser& parser)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, parser)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    Parser& parser;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, parser);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *bottom, yytype_int16 *top)
#else
static void
yy_stack_print (bottom, top)
    yytype_int16 *bottom;
    yytype_int16 *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                \
do {                                \
  if (yydebug)                            \
    yy_stack_print ((Bottom), (Top));                \
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule, Parser& parser)
#else
static void
yy_reduce_print (yyvsp, yyrule, parser)
    YYSTYPE *yyvsp;
    int yyrule;
    Parser& parser;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
         yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      fprintf (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
               &(yyvsp[(yyi + 1) - (yynrhs)])
                              , parser);
      fprintf (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)        \
do {                    \
  if (yydebug)                \
    yy_reduce_print (yyvsp, Rule, parser); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef    YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
    switch (*++yyp)
      {
      case '\'':
      case ',':
        goto do_not_strip_quotes;

      case '\\':
        if (*++yyp != '\\')
          goto do_not_strip_quotes;
        /* Fall through.  */
      default:
        if (yyres)
          yyres[yyn] = *yyp;
        yyn++;
        break;

      case '"':
        if (yyres)
          yyres[yyn] = '\0';
        return yyn;
      }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
     constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
            + sizeof yyexpecting - 1
            + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
               * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
     YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
      {
        if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
          {
        yycount = 1;
        yysize = yysize0;
        yyformat[sizeof yyunexpected - 1] = '\0';
        break;
          }
        yyarg[yycount++] = yytname[yyx];
        yysize1 = yysize + yytnamerr (0, yytname[yyx]);
        yysize_overflow |= (yysize1 < yysize);
        yysize = yysize1;
        yyfmt = yystpcpy (yyfmt, yyprefix);
        yyprefix = yyor;
      }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
    return YYSIZE_MAXIMUM;

      if (yyresult)
    {
      /* Avoid sprintf, as that infringes on the user's name space.
         Don't have undefined behavior even if the translation
         produced a string with the wrong number of "%s"s.  */
      char *yyp = yyresult;
      int yyi = 0;
      while ((*yyp = *yyf) != '\0')
        {
          if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyf += 2;
        }
          else
        {
          yyp++;
          yyf++;
        }
        }
    }
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, Parser& parser)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, parser)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    Parser& parser;
#endif
{
  YYUSE (yyvaluep);
  YYUSE (parser);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {
      case 10: /* "FUNCTIONNAME" */
#line 80 "WebCore/xml/XPathGrammar.y"
    { if ((yyvaluep->string)) (yyvaluep->string)->deref(); };
#line 1205 "./XPathGrammar.cpp"
    break;
      case 11: /* "LITERAL" */
#line 80 "WebCore/xml/XPathGrammar.y"
    { if ((yyvaluep->string)) (yyvaluep->string)->deref(); };
#line 1210 "./XPathGrammar.cpp"
    break;
      case 12: /* "NAMETEST" */
#line 80 "WebCore/xml/XPathGrammar.y"
    { if ((yyvaluep->string)) (yyvaluep->string)->deref(); };
#line 1215 "./XPathGrammar.cpp"
    break;
      case 13: /* "NUMBER" */
#line 80 "WebCore/xml/XPathGrammar.y"
    { if ((yyvaluep->string)) (yyvaluep->string)->deref(); };
#line 1220 "./XPathGrammar.cpp"
    break;
      case 14: /* "NODETYPE" */
#line 80 "WebCore/xml/XPathGrammar.y"
    { if ((yyvaluep->string)) (yyvaluep->string)->deref(); };
#line 1225 "./XPathGrammar.cpp"
    break;
      case 15: /* "VARIABLEREFERENCE" */
#line 80 "WebCore/xml/XPathGrammar.y"
    { if ((yyvaluep->string)) (yyvaluep->string)->deref(); };
#line 1230 "./XPathGrammar.cpp"
    break;
      case 35: /* "Expr" */
#line 100 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->expression); };
#line 1235 "./XPathGrammar.cpp"
    break;
      case 36: /* "LocationPath" */
#line 88 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->locationPath); };
#line 1240 "./XPathGrammar.cpp"
    break;
      case 37: /* "AbsoluteLocationPath" */
#line 88 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->locationPath); };
#line 1245 "./XPathGrammar.cpp"
    break;
      case 38: /* "RelativeLocationPath" */
#line 88 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->locationPath); };
#line 1250 "./XPathGrammar.cpp"
    break;
      case 39: /* "Step" */
#line 97 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->step); };
#line 1255 "./XPathGrammar.cpp"
    break;
      case 41: /* "NodeTest" */
#line 91 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->nodeTest); };
#line 1260 "./XPathGrammar.cpp"
    break;
      case 42: /* "OptionalPredicateList" */
#line 94 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->expressionVector); };
#line 1265 "./XPathGrammar.cpp"
    break;
      case 43: /* "PredicateList" */
#line 94 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->expressionVector); };
#line 1270 "./XPathGrammar.cpp"
    break;
      case 44: /* "Predicate" */
#line 100 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->expression); };
#line 1275 "./XPathGrammar.cpp"
    break;
      case 45: /* "DescendantOrSelf" */
#line 97 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->step); };
#line 1280 "./XPathGrammar.cpp"
    break;
      case 46: /* "AbbreviatedStep" */
#line 97 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->step); };
#line 1285 "./XPathGrammar.cpp"
    break;
      case 47: /* "PrimaryExpr" */
#line 100 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->expression); };
#line 1290 "./XPathGrammar.cpp"
    break;
      case 48: /* "FunctionCall" */
#line 100 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->expression); };
#line 1295 "./XPathGrammar.cpp"
    break;
      case 49: /* "ArgumentList" */
#line 94 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->expressionVector); };
#line 1300 "./XPathGrammar.cpp"
    break;
      case 50: /* "Argument" */
#line 100 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->expression); };
#line 1305 "./XPathGrammar.cpp"
    break;
      case 51: /* "UnionExpr" */
#line 100 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->expression); };
#line 1310 "./XPathGrammar.cpp"
    break;
      case 52: /* "PathExpr" */
#line 100 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->expression); };
#line 1315 "./XPathGrammar.cpp"
    break;
      case 53: /* "FilterExpr" */
#line 100 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->expression); };
#line 1320 "./XPathGrammar.cpp"
    break;
      case 54: /* "OrExpr" */
#line 100 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->expression); };
#line 1325 "./XPathGrammar.cpp"
    break;
      case 55: /* "AndExpr" */
#line 100 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->expression); };
#line 1330 "./XPathGrammar.cpp"
    break;
      case 56: /* "EqualityExpr" */
#line 100 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->expression); };
#line 1335 "./XPathGrammar.cpp"
    break;
      case 57: /* "RelationalExpr" */
#line 100 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->expression); };
#line 1340 "./XPathGrammar.cpp"
    break;
      case 58: /* "AdditiveExpr" */
#line 100 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->expression); };
#line 1345 "./XPathGrammar.cpp"
    break;
      case 59: /* "MultiplicativeExpr" */
#line 100 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->expression); };
#line 1350 "./XPathGrammar.cpp"
    break;
      case 60: /* "UnaryExpr" */
#line 100 "WebCore/xml/XPathGrammar.y"
    { delete (yyvaluep->expression); };
#line 1355 "./XPathGrammar.cpp"
    break;

      default:
    break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (Parser& parser);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */






/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (Parser& parser)
#else
int
yyparse (parser)
    Parser& parser;
#endif
#endif
{
  /* The look-ahead symbol.  */
int yychar;

/* The semantic value of the look-ahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;

  int yystate;
  int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Look-ahead token as an internal (translated) token number.  */
  int yytoken = 0;
#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  yytype_int16 yyssa[YYINITDEPTH];
  yytype_int16 *yyss = yyssa;
  yytype_int16 *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;        /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
    /* Give user a chance to reallocate the stack.  Use copies of
       these so that the &'s don't force the real ones into
       memory.  */
    YYSTYPE *yyvs1 = yyvs;
    yytype_int16 *yyss1 = yyss;


    /* Each stack pointer address is followed by the size of the
       data in use in that stack, in bytes.  This used to be a
       conditional around just the two extra args, but that might
       be undefined if yyoverflow is a macro.  */
    yyoverflow (YY_("memory exhausted"),
            &yyss1, yysize * sizeof (*yyssp),
            &yyvs1, yysize * sizeof (*yyvsp),

            &yystacksize);

    yyss = yyss1;
    yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
    goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
    yystacksize = YYMAXDEPTH;

      {
    yytype_int16 *yyss1 = yyss;
    union yyalloc *yyptr =
      (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
    if (! yyptr)
      goto yyexhaustedlab;
    YYSTACK_RELOCATE (yyss);
    YYSTACK_RELOCATE (yyvs);

#  undef YYSTACK_RELOCATE
    if (yyss1 != yyssa)
      YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;


      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
          (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
    YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     look-ahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to look-ahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a look-ahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid look-ahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
    goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
#line 115 "WebCore/xml/XPathGrammar.y"
    {
        parser.setParseResult(std::unique_ptr<Expression>((yyvsp[(1) - (1)].expression)));
    ;}
    break;

  case 4:
#line 126 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.locationPath) = (yyvsp[(1) - (1)].locationPath);
        (yyval.locationPath)->setAbsolute();
    ;}
    break;

  case 6:
#line 136 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.locationPath) = new LocationPath;
    ;}
    break;

  case 7:
#line 141 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.locationPath) = (yyvsp[(2) - (2)].locationPath);
    ;}
    break;

  case 8:
#line 146 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.locationPath) = (yyvsp[(2) - (2)].locationPath);
        (yyval.locationPath)->prependStep(std::unique_ptr<Step>((yyvsp[(1) - (2)].step)));
    ;}
    break;

  case 9:
#line 154 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.locationPath) = new LocationPath;
        (yyval.locationPath)->appendStep(std::unique_ptr<Step>((yyvsp[(1) - (1)].step)));
    ;}
    break;

  case 10:
#line 160 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.locationPath) = (yyvsp[(1) - (3)].locationPath);
        (yyval.locationPath)->appendStep(std::unique_ptr<Step>((yyvsp[(3) - (3)].step)));
    ;}
    break;

  case 11:
#line 166 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.locationPath) = (yyvsp[(1) - (3)].locationPath);
        (yyval.locationPath)->appendStep(std::unique_ptr<Step>((yyvsp[(2) - (3)].step)));
        (yyval.locationPath)->appendStep(std::unique_ptr<Step>((yyvsp[(3) - (3)].step)));
    ;}
    break;

  case 12:
#line 175 "WebCore/xml/XPathGrammar.y"
    {
        std::unique_ptr<Step::NodeTest> nodeTest((yyvsp[(1) - (2)].nodeTest));
        std::unique_ptr<Vector<std::unique_ptr<Expression>>> predicateList((yyvsp[(2) - (2)].expressionVector));
        if (predicateList)
            (yyval.step) = new Step(Step::ChildAxis, WTFMove(*nodeTest), WTFMove(*predicateList));
        else
            (yyval.step) = new Step(Step::ChildAxis, WTFMove(*nodeTest));
    ;}
    break;

  case 13:
#line 185 "WebCore/xml/XPathGrammar.y"
    {
        String nametest = adoptRef((yyvsp[(1) - (2)].string));
        std::unique_ptr<Vector<std::unique_ptr<Expression>>> predicateList((yyvsp[(2) - (2)].expressionVector));

        String localName;
        String namespaceURI;
        if (!parser.expandQualifiedName(nametest, localName, namespaceURI)) {
            (yyval.step) = nullptr;
            YYABORT;
        }

        if (predicateList)
            (yyval.step) = new Step(Step::ChildAxis, Step::NodeTest(Step::NodeTest::NameTest, localName, namespaceURI), WTFMove(*predicateList));
        else
            (yyval.step) = new Step(Step::ChildAxis, Step::NodeTest(Step::NodeTest::NameTest, localName, namespaceURI));
    ;}
    break;

  case 14:
#line 203 "WebCore/xml/XPathGrammar.y"
    {
        std::unique_ptr<Step::NodeTest> nodeTest((yyvsp[(2) - (3)].nodeTest));
        std::unique_ptr<Vector<std::unique_ptr<Expression>>> predicateList((yyvsp[(3) - (3)].expressionVector));

        if (predicateList)
            (yyval.step) = new Step((yyvsp[(1) - (3)].axis), WTFMove(*nodeTest), WTFMove(*predicateList));
        else
            (yyval.step) = new Step((yyvsp[(1) - (3)].axis), WTFMove(*nodeTest));
    ;}
    break;

  case 15:
#line 214 "WebCore/xml/XPathGrammar.y"
    {
        String nametest = adoptRef((yyvsp[(2) - (3)].string));
        std::unique_ptr<Vector<std::unique_ptr<Expression>>> predicateList((yyvsp[(3) - (3)].expressionVector));

        String localName;
        String namespaceURI;
        if (!parser.expandQualifiedName(nametest, localName, namespaceURI)) {
            (yyval.step) = nullptr;
            YYABORT;
        }

        if (predicateList)
            (yyval.step) = new Step((yyvsp[(1) - (3)].axis), Step::NodeTest(Step::NodeTest::NameTest, localName, namespaceURI), WTFMove(*predicateList));
        else
            (yyval.step) = new Step((yyvsp[(1) - (3)].axis), Step::NodeTest(Step::NodeTest::NameTest, localName, namespaceURI));
    ;}
    break;

  case 18:
#line 238 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.axis) = Step::AttributeAxis;
    ;}
    break;

  case 19:
#line 245 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.nodeTest) = new Step::NodeTest(Step::NodeTest::AnyNodeTest);
    ;}
    break;

  case 20:
#line 250 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.nodeTest) = new Step::NodeTest(Step::NodeTest::TextNodeTest);
    ;}
    break;

  case 21:
#line 255 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.nodeTest) = new Step::NodeTest(Step::NodeTest::CommentNodeTest);
    ;}
    break;

  case 22:
#line 260 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.nodeTest) = new Step::NodeTest(Step::NodeTest::ProcessingInstructionNodeTest);
    ;}
    break;

  case 23:
#line 265 "WebCore/xml/XPathGrammar.y"
    {
        String literal = adoptRef((yyvsp[(3) - (4)].string));
        (yyval.nodeTest) = new Step::NodeTest(Step::NodeTest::ProcessingInstructionNodeTest, literal.stripWhiteSpace());
    ;}
    break;

  case 24:
#line 273 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.expressionVector) = nullptr;
    ;}
    break;

  case 26:
#line 282 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.expressionVector) = new Vector<std::unique_ptr<Expression>>;
        (yyval.expressionVector)->append(std::unique_ptr<Expression>((yyvsp[(1) - (1)].expression)));
    ;}
    break;

  case 27:
#line 288 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.expressionVector) = (yyvsp[(1) - (2)].expressionVector);
        (yyval.expressionVector)->append(std::unique_ptr<Expression>((yyvsp[(2) - (2)].expression)));
    ;}
    break;

  case 28:
#line 296 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.expression) = (yyvsp[(2) - (3)].expression);
    ;}
    break;

  case 29:
#line 303 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.step) = new Step(Step::DescendantOrSelfAxis, Step::NodeTest(Step::NodeTest::AnyNodeTest));
    ;}
    break;

  case 30:
#line 310 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.step) = new Step(Step::SelfAxis, Step::NodeTest(Step::NodeTest::AnyNodeTest));
    ;}
    break;

  case 31:
#line 315 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.step) = new Step(Step::ParentAxis, Step::NodeTest(Step::NodeTest::AnyNodeTest));
    ;}
    break;

  case 32:
#line 322 "WebCore/xml/XPathGrammar.y"
    {
        String name = adoptRef((yyvsp[(1) - (1)].string));
        (yyval.expression) = new VariableReference(name);
    ;}
    break;

  case 33:
#line 328 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.expression) = (yyvsp[(2) - (3)].expression);
    ;}
    break;

  case 34:
#line 333 "WebCore/xml/XPathGrammar.y"
    {
        String literal = adoptRef((yyvsp[(1) - (1)].string));
        (yyval.expression) = new StringExpression(WTFMove(literal));
    ;}
    break;

  case 35:
#line 339 "WebCore/xml/XPathGrammar.y"
    {
        String numeral = adoptRef((yyvsp[(1) - (1)].string));
        (yyval.expression) = new Number(numeral.toDouble());
    ;}
    break;

  case 37:
#line 349 "WebCore/xml/XPathGrammar.y"
    {
        String name = adoptRef((yyvsp[(1) - (3)].string));
        (yyval.expression) = XPath::Function::create(name).release();
        if (!(yyval.expression))
            YYABORT;
    ;}
    break;

  case 38:
#line 357 "WebCore/xml/XPathGrammar.y"
    {
        String name = adoptRef((yyvsp[(1) - (4)].string));
        std::unique_ptr<Vector<std::unique_ptr<Expression>>> argumentList((yyvsp[(3) - (4)].expressionVector));
        (yyval.expression) = XPath::Function::create(name, WTFMove(*argumentList)).release();
        if (!(yyval.expression))
            YYABORT;
    ;}
    break;

  case 39:
#line 368 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.expressionVector) = new Vector<std::unique_ptr<Expression>>;
        (yyval.expressionVector)->append(std::unique_ptr<Expression>((yyvsp[(1) - (1)].expression)));
    ;}
    break;

  case 40:
#line 374 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.expressionVector) = (yyvsp[(1) - (3)].expressionVector);
        (yyval.expressionVector)->append(std::unique_ptr<Expression>((yyvsp[(3) - (3)].expression)));
    ;}
    break;

  case 43:
#line 388 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.expression) = new Union(std::unique_ptr<Expression>((yyvsp[(1) - (3)].expression)), std::unique_ptr<Expression>((yyvsp[(3) - (3)].expression)));
    ;}
    break;

  case 44:
#line 395 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.expression) = (yyvsp[(1) - (1)].locationPath);
    ;}
    break;

  case 46:
#line 402 "WebCore/xml/XPathGrammar.y"
    {
        (yyvsp[(3) - (3)].locationPath)->setAbsolute();
        (yyval.expression) = new XPath::Path(std::unique_ptr<Expression>((yyvsp[(1) - (3)].expression)), std::unique_ptr<LocationPath>((yyvsp[(3) - (3)].locationPath)));
    ;}
    break;

  case 47:
#line 408 "WebCore/xml/XPathGrammar.y"
    {
        (yyvsp[(3) - (3)].locationPath)->prependStep(std::unique_ptr<Step>((yyvsp[(2) - (3)].step)));
        (yyvsp[(3) - (3)].locationPath)->setAbsolute();
        (yyval.expression) = new XPath::Path(std::unique_ptr<Expression>((yyvsp[(1) - (3)].expression)), std::unique_ptr<LocationPath>((yyvsp[(3) - (3)].locationPath)));
    ;}
    break;

  case 49:
#line 419 "WebCore/xml/XPathGrammar.y"
    {
        std::unique_ptr<Vector<std::unique_ptr<Expression>>> predicateList((yyvsp[(2) - (2)].expressionVector));
        (yyval.expression) = new Filter(std::unique_ptr<Expression>((yyvsp[(1) - (2)].expression)), WTFMove(*predicateList));
    ;}
    break;

  case 51:
#line 429 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.expression) = new LogicalOp(LogicalOp::OP_Or, std::unique_ptr<Expression>((yyvsp[(1) - (3)].expression)), std::unique_ptr<Expression>((yyvsp[(3) - (3)].expression)));
    ;}
    break;

  case 53:
#line 438 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.expression) = new LogicalOp(LogicalOp::OP_And, std::unique_ptr<Expression>((yyvsp[(1) - (3)].expression)), std::unique_ptr<Expression>((yyvsp[(3) - (3)].expression)));
    ;}
    break;

  case 55:
#line 447 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.expression) = new EqTestOp((yyvsp[(2) - (3)].equalityTestOpcode), std::unique_ptr<Expression>((yyvsp[(1) - (3)].expression)), std::unique_ptr<Expression>((yyvsp[(3) - (3)].expression)));
    ;}
    break;

  case 57:
#line 456 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.expression) = new EqTestOp((yyvsp[(2) - (3)].equalityTestOpcode), std::unique_ptr<Expression>((yyvsp[(1) - (3)].expression)), std::unique_ptr<Expression>((yyvsp[(3) - (3)].expression)));
    ;}
    break;

  case 59:
#line 465 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.expression) = new NumericOp(NumericOp::OP_Add, std::unique_ptr<Expression>((yyvsp[(1) - (3)].expression)), std::unique_ptr<Expression>((yyvsp[(3) - (3)].expression)));
    ;}
    break;

  case 60:
#line 470 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.expression) = new NumericOp(NumericOp::OP_Sub, std::unique_ptr<Expression>((yyvsp[(1) - (3)].expression)), std::unique_ptr<Expression>((yyvsp[(3) - (3)].expression)));
    ;}
    break;

  case 62:
#line 479 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.expression) = new NumericOp((yyvsp[(2) - (3)].numericOpcode), std::unique_ptr<Expression>((yyvsp[(1) - (3)].expression)), std::unique_ptr<Expression>((yyvsp[(3) - (3)].expression)));
    ;}
    break;

  case 64:
#line 488 "WebCore/xml/XPathGrammar.y"
    {
        (yyval.expression) = new Negative(std::unique_ptr<Expression>((yyvsp[(2) - (2)].expression)));
    ;}
    break;


/* Line 1267 of yacc.c.  */
#line 2049 "./XPathGrammar.cpp"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;


  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (parser, YY_("syntax error"));
#else
      {
    YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
    if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
      {
        YYSIZE_T yyalloc = 2 * yysize;
        if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
          yyalloc = YYSTACK_ALLOC_MAXIMUM;
        if (yymsg != yymsgbuf)
          YYSTACK_FREE (yymsg);
        yymsg = (char *) YYSTACK_ALLOC (yyalloc);
        if (yymsg)
          yymsg_alloc = yyalloc;
        else
          {
        yymsg = yymsgbuf;
        yymsg_alloc = sizeof yymsgbuf;
          }
      }

    if (0 < yysize && yysize <= yymsg_alloc)
      {
        (void) yysyntax_error (yymsg, yystate, yychar);
        yyerror (parser, yymsg);
      }
    else
      {
        yyerror (parser, YY_("syntax error"));
        if (yysize != 0)
          goto yyexhaustedlab;
      }
      }
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse look-ahead token after an
     error, discard it.  */

      if (yychar <= YYEOF)
    {
      /* Return failure if at end of input.  */
      if (yychar == YYEOF)
        YYABORT;
    }
      else
    {
      yydestruct ("Error: discarding",
              yytoken, &yylval, parser);
      yychar = YYEMPTY;
    }
    }

  /* Else will try to reuse look-ahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  IGNORE_CLANG_WARNINGS_BEGIN("unreachable-code")
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;
  IGNORE_CLANG_WARNINGS_END

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;    /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
    {
      yyn += YYTERROR;
      if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
        {
          yyn = yytable[yyn];
          if (0 < yyn)
        break;
        }
    }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
    YYABORT;


      yydestruct ("Error: popping",
          yystos[yystate], yyvsp, parser);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#ifndef yyoverflow
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (parser, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEOF && yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
         yytoken, &yylval, parser);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
          yystos[*yyssp], yyvsp, parser);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}


#line 493 "WebCore/xml/XPathGrammar.y"


