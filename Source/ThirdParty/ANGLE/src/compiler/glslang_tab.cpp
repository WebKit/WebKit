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



/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     INVARIANT = 258,
     HIGH_PRECISION = 259,
     MEDIUM_PRECISION = 260,
     LOW_PRECISION = 261,
     PRECISION = 262,
     ATTRIBUTE = 263,
     CONST_QUAL = 264,
     BOOL_TYPE = 265,
     FLOAT_TYPE = 266,
     INT_TYPE = 267,
     BREAK = 268,
     CONTINUE = 269,
     DO = 270,
     ELSE = 271,
     FOR = 272,
     IF = 273,
     DISCARD = 274,
     RETURN = 275,
     BVEC2 = 276,
     BVEC3 = 277,
     BVEC4 = 278,
     IVEC2 = 279,
     IVEC3 = 280,
     IVEC4 = 281,
     VEC2 = 282,
     VEC3 = 283,
     VEC4 = 284,
     MATRIX2 = 285,
     MATRIX3 = 286,
     MATRIX4 = 287,
     IN_QUAL = 288,
     OUT_QUAL = 289,
     INOUT_QUAL = 290,
     UNIFORM = 291,
     VARYING = 292,
     STRUCT = 293,
     VOID_TYPE = 294,
     WHILE = 295,
     SAMPLER2D = 296,
     SAMPLERCUBE = 297,
     SAMPLER_EXTERNAL_OES = 298,
     SAMPLER2DRECT = 299,
     IDENTIFIER = 300,
     TYPE_NAME = 301,
     FLOATCONSTANT = 302,
     INTCONSTANT = 303,
     BOOLCONSTANT = 304,
     FIELD_SELECTION = 305,
     LEFT_OP = 306,
     RIGHT_OP = 307,
     INC_OP = 308,
     DEC_OP = 309,
     LE_OP = 310,
     GE_OP = 311,
     EQ_OP = 312,
     NE_OP = 313,
     AND_OP = 314,
     OR_OP = 315,
     XOR_OP = 316,
     MUL_ASSIGN = 317,
     DIV_ASSIGN = 318,
     ADD_ASSIGN = 319,
     MOD_ASSIGN = 320,
     LEFT_ASSIGN = 321,
     RIGHT_ASSIGN = 322,
     AND_ASSIGN = 323,
     XOR_ASSIGN = 324,
     OR_ASSIGN = 325,
     SUB_ASSIGN = 326,
     LEFT_PAREN = 327,
     RIGHT_PAREN = 328,
     LEFT_BRACKET = 329,
     RIGHT_BRACKET = 330,
     LEFT_BRACE = 331,
     RIGHT_BRACE = 332,
     DOT = 333,
     COMMA = 334,
     COLON = 335,
     EQUAL = 336,
     SEMICOLON = 337,
     BANG = 338,
     DASH = 339,
     TILDE = 340,
     PLUS = 341,
     STAR = 342,
     SLASH = 343,
     PERCENT = 344,
     LEFT_ANGLE = 345,
     RIGHT_ANGLE = 346,
     VERTICAL_BAR = 347,
     CARET = 348,
     AMPERSAND = 349,
     QUESTION = 350
   };
#endif
/* Tokens.  */
#define INVARIANT 258
#define HIGH_PRECISION 259
#define MEDIUM_PRECISION 260
#define LOW_PRECISION 261
#define PRECISION 262
#define ATTRIBUTE 263
#define CONST_QUAL 264
#define BOOL_TYPE 265
#define FLOAT_TYPE 266
#define INT_TYPE 267
#define BREAK 268
#define CONTINUE 269
#define DO 270
#define ELSE 271
#define FOR 272
#define IF 273
#define DISCARD 274
#define RETURN 275
#define BVEC2 276
#define BVEC3 277
#define BVEC4 278
#define IVEC2 279
#define IVEC3 280
#define IVEC4 281
#define VEC2 282
#define VEC3 283
#define VEC4 284
#define MATRIX2 285
#define MATRIX3 286
#define MATRIX4 287
#define IN_QUAL 288
#define OUT_QUAL 289
#define INOUT_QUAL 290
#define UNIFORM 291
#define VARYING 292
#define STRUCT 293
#define VOID_TYPE 294
#define WHILE 295
#define SAMPLER2D 296
#define SAMPLERCUBE 297
#define SAMPLER_EXTERNAL_OES 298
#define SAMPLER2DRECT 299
#define IDENTIFIER 300
#define TYPE_NAME 301
#define FLOATCONSTANT 302
#define INTCONSTANT 303
#define BOOLCONSTANT 304
#define FIELD_SELECTION 305
#define LEFT_OP 306
#define RIGHT_OP 307
#define INC_OP 308
#define DEC_OP 309
#define LE_OP 310
#define GE_OP 311
#define EQ_OP 312
#define NE_OP 313
#define AND_OP 314
#define OR_OP 315
#define XOR_OP 316
#define MUL_ASSIGN 317
#define DIV_ASSIGN 318
#define ADD_ASSIGN 319
#define MOD_ASSIGN 320
#define LEFT_ASSIGN 321
#define RIGHT_ASSIGN 322
#define AND_ASSIGN 323
#define XOR_ASSIGN 324
#define OR_ASSIGN 325
#define SUB_ASSIGN 326
#define LEFT_PAREN 327
#define RIGHT_PAREN 328
#define LEFT_BRACKET 329
#define RIGHT_BRACKET 330
#define LEFT_BRACE 331
#define RIGHT_BRACE 332
#define DOT 333
#define COMMA 334
#define COLON 335
#define EQUAL 336
#define SEMICOLON 337
#define BANG 338
#define DASH 339
#define TILDE 340
#define PLUS 341
#define STAR 342
#define SLASH 343
#define PERCENT 344
#define LEFT_ANGLE 345
#define RIGHT_ANGLE 346
#define VERTICAL_BAR 347
#define CARET 348
#define AMPERSAND 349
#define QUESTION 350




/* Copy the first part of user declarations.  */


//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// This file is auto-generated by generate_parser.sh. DO NOT EDIT!

// Ignore errors in auto-generated code.
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wswitch-enum"
#elif defined(_MSC_VER)
#pragma warning(disable: 4065)
#pragma warning(disable: 4189)
#pragma warning(disable: 4505)
#pragma warning(disable: 4701)
#endif

#include "compiler/SymbolTable.h"
#include "compiler/ParseHelper.h"
#include "GLSLANG/ShaderLang.h"

#define YYENABLE_NLS 0
#define YYLTYPE_IS_TRIVIAL 1

#define YYLEX_PARAM context->scanner


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

{
    struct {
        TSourceLoc line;
        union {
            TString *string;
            float f;
            int i;
            bool b;
        };
        TSymbol* symbol;
    } lex;
    struct {
        TSourceLoc line;
        TOperator op;
        union {
            TIntermNode* intermNode;
            TIntermNodePair nodePair;
            TIntermTyped* intermTypedNode;
            TIntermAggregate* intermAggregate;
        };
        union {
            TPublicType type;
            TPrecision precision;
            TQualifier qualifier;
            TFunction* function;
            TParameter param;
            TTypeLine typeLine;
            TTypeList* typeList;
        };
    } interm;
}
/* Line 193 of yacc.c.  */

	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


extern int yylex(YYSTYPE* yylval_param, void* yyscanner);
extern void yyerror(TParseContext* context, const char* reason);

#define FRAG_VERT_ONLY(S, L) {  \
    if (context->shaderType != SH_FRAGMENT_SHADER &&  \
        context->shaderType != SH_VERTEX_SHADER) {  \
        context->error(L, " supported in vertex/fragment shaders only ", S);  \
        context->recover();  \
    }  \
}

#define VERTEX_ONLY(S, L) {  \
    if (context->shaderType != SH_VERTEX_SHADER) {  \
        context->error(L, " supported in vertex shaders only ", S);  \
        context->recover();  \
    }  \
}

#define FRAG_ONLY(S, L) {  \
    if (context->shaderType != SH_FRAGMENT_SHADER) {  \
        context->error(L, " supported in fragment shaders only ", S);  \
        context->recover();  \
    }  \
}


/* Line 216 of yacc.c.  */


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
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  71
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   1416

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  96
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  83
/* YYNRULES -- Number of rules.  */
#define YYNRULES  201
/* YYNRULES -- Number of states.  */
#define YYNSTATES  304

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   350

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
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
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     5,     7,     9,    11,    13,    17,    19,
      24,    26,    30,    33,    36,    38,    40,    42,    46,    49,
      52,    55,    57,    60,    64,    67,    69,    71,    73,    75,
      78,    81,    84,    86,    88,    90,    92,    96,   100,   102,
     106,   110,   112,   114,   118,   122,   126,   130,   132,   136,
     140,   142,   144,   146,   148,   152,   154,   158,   160,   164,
     166,   172,   174,   178,   180,   182,   184,   186,   188,   190,
     194,   196,   199,   202,   207,   210,   212,   214,   217,   221,
     225,   228,   234,   238,   241,   245,   248,   249,   251,   253,
     255,   257,   259,   263,   269,   276,   282,   284,   287,   292,
     298,   303,   306,   308,   311,   313,   315,   317,   320,   322,
     324,   327,   329,   331,   333,   335,   340,   342,   344,   346,
     348,   350,   352,   354,   356,   358,   360,   362,   364,   366,
     368,   370,   372,   374,   376,   378,   380,   382,   384,   385,
     392,   393,   399,   401,   404,   408,   410,   414,   416,   421,
     423,   425,   427,   429,   431,   433,   435,   437,   439,   442,
     443,   444,   450,   452,   454,   455,   458,   459,   462,   465,
     469,   471,   474,   476,   479,   485,   489,   491,   493,   498,
     499,   506,   507,   516,   517,   525,   527,   529,   531,   532,
     535,   539,   542,   545,   548,   552,   555,   557,   560,   562,
     564,   565
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     175,     0,    -1,    45,    -1,    97,    -1,    48,    -1,    47,
      -1,    49,    -1,    72,   124,    73,    -1,    98,    -1,    99,
      74,   100,    75,    -1,   101,    -1,    99,    78,    50,    -1,
      99,    53,    -1,    99,    54,    -1,   124,    -1,   102,    -1,
     103,    -1,    99,    78,   103,    -1,   105,    73,    -1,   104,
      73,    -1,   106,    39,    -1,   106,    -1,   106,   122,    -1,
     105,    79,   122,    -1,   107,    72,    -1,   142,    -1,    45,
      -1,    50,    -1,    99,    -1,    53,   108,    -1,    54,   108,
      -1,   109,   108,    -1,    86,    -1,    84,    -1,    83,    -1,
     108,    -1,   110,    87,   108,    -1,   110,    88,   108,    -1,
     110,    -1,   111,    86,   110,    -1,   111,    84,   110,    -1,
     111,    -1,   112,    -1,   113,    90,   112,    -1,   113,    91,
     112,    -1,   113,    55,   112,    -1,   113,    56,   112,    -1,
     113,    -1,   114,    57,   113,    -1,   114,    58,   113,    -1,
     114,    -1,   115,    -1,   116,    -1,   117,    -1,   118,    59,
     117,    -1,   118,    -1,   119,    61,   118,    -1,   119,    -1,
     120,    60,   119,    -1,   120,    -1,   120,    95,   124,    80,
     122,    -1,   121,    -1,   108,   123,   122,    -1,    81,    -1,
      62,    -1,    63,    -1,    64,    -1,    71,    -1,   122,    -1,
     124,    79,   122,    -1,   121,    -1,   127,    82,    -1,   135,
      82,    -1,     7,   140,   141,    82,    -1,   128,    73,    -1,
     130,    -1,   129,    -1,   130,   132,    -1,   129,    79,   132,
      -1,   137,    45,    72,    -1,   139,    45,    -1,   139,    45,
      74,   125,    75,    -1,   138,   133,   131,    -1,   133,   131,
      -1,   138,   133,   134,    -1,   133,   134,    -1,    -1,    33,
      -1,    34,    -1,    35,    -1,   139,    -1,   136,    -1,   135,
      79,    45,    -1,   135,    79,    45,    74,    75,    -1,   135,
      79,    45,    74,   125,    75,    -1,   135,    79,    45,    81,
     150,    -1,   137,    -1,   137,    45,    -1,   137,    45,    74,
      75,    -1,   137,    45,    74,   125,    75,    -1,   137,    45,
      81,   150,    -1,     3,    45,    -1,   139,    -1,   138,   139,
      -1,     9,    -1,     8,    -1,    37,    -1,     3,    37,    -1,
      36,    -1,   141,    -1,   140,   141,    -1,     4,    -1,     5,
      -1,     6,    -1,   142,    -1,   142,    74,   125,    75,    -1,
      39,    -1,    11,    -1,    12,    -1,    10,    -1,    27,    -1,
      28,    -1,    29,    -1,    21,    -1,    22,    -1,    23,    -1,
      24,    -1,    25,    -1,    26,    -1,    30,    -1,    31,    -1,
      32,    -1,    41,    -1,    42,    -1,    43,    -1,    44,    -1,
     143,    -1,    46,    -1,    -1,    38,    45,    76,   144,   146,
      77,    -1,    -1,    38,    76,   145,   146,    77,    -1,   147,
      -1,   146,   147,    -1,   139,   148,    82,    -1,   149,    -1,
     148,    79,   149,    -1,    45,    -1,    45,    74,   125,    75,
      -1,   122,    -1,   126,    -1,   154,    -1,   153,    -1,   151,
      -1,   163,    -1,   164,    -1,   167,    -1,   174,    -1,    76,
      77,    -1,    -1,    -1,    76,   155,   162,   156,    77,    -1,
     161,    -1,   153,    -1,    -1,   159,   161,    -1,    -1,   160,
     153,    -1,    76,    77,    -1,    76,   162,    77,    -1,   152,
      -1,   162,   152,    -1,    82,    -1,   124,    82,    -1,    18,
      72,   124,    73,   165,    -1,   158,    16,   158,    -1,   158,
      -1,   124,    -1,   137,    45,    81,   150,    -1,    -1,    40,
      72,   168,   166,    73,   157,    -1,    -1,    15,   169,   158,
      40,    72,   124,    73,    82,    -1,    -1,    17,    72,   170,
     171,   173,    73,   157,    -1,   163,    -1,   151,    -1,   166,
      -1,    -1,   172,    82,    -1,   172,    82,   124,    -1,    14,
      82,    -1,    13,    82,    -1,    20,    82,    -1,    20,   124,
      82,    -1,    19,    82,    -1,   176,    -1,   175,   176,    -1,
     177,    -1,   126,    -1,    -1,   127,   178,   161,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   168,   168,   203,   206,   219,   224,   229,   235,   238,
     317,   320,   421,   431,   444,   452,   552,   555,   563,   567,
     574,   578,   585,   591,   600,   608,   663,   670,   680,   683,
     693,   703,   724,   725,   726,   731,   732,   741,   753,   754,
     762,   773,   777,   778,   788,   798,   808,   821,   822,   832,
     845,   849,   853,   857,   858,   871,   872,   885,   886,   899,
     900,   917,   918,   931,   932,   933,   934,   935,   939,   942,
     953,   961,   988,   993,  1003,  1041,  1044,  1051,  1059,  1080,
    1101,  1112,  1141,  1146,  1156,  1161,  1171,  1174,  1177,  1180,
    1186,  1193,  1196,  1218,  1236,  1260,  1283,  1287,  1305,  1313,
    1345,  1365,  1454,  1463,  1486,  1489,  1495,  1503,  1511,  1519,
    1529,  1536,  1539,  1542,  1548,  1551,  1566,  1570,  1574,  1578,
    1587,  1592,  1597,  1602,  1607,  1612,  1617,  1622,  1627,  1632,
    1638,  1644,  1650,  1655,  1660,  1669,  1678,  1683,  1696,  1696,
    1710,  1710,  1719,  1722,  1737,  1773,  1777,  1783,  1791,  1807,
    1811,  1815,  1816,  1822,  1823,  1824,  1825,  1826,  1830,  1831,
    1831,  1831,  1841,  1842,  1846,  1846,  1847,  1847,  1852,  1855,
    1865,  1868,  1874,  1875,  1879,  1887,  1891,  1901,  1906,  1923,
    1923,  1928,  1928,  1935,  1935,  1943,  1946,  1952,  1955,  1961,
    1965,  1972,  1979,  1986,  1993,  2004,  2013,  2017,  2024,  2027,
    2033,  2033
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "INVARIANT", "HIGH_PRECISION",
  "MEDIUM_PRECISION", "LOW_PRECISION", "PRECISION", "ATTRIBUTE",
  "CONST_QUAL", "BOOL_TYPE", "FLOAT_TYPE", "INT_TYPE", "BREAK", "CONTINUE",
  "DO", "ELSE", "FOR", "IF", "DISCARD", "RETURN", "BVEC2", "BVEC3",
  "BVEC4", "IVEC2", "IVEC3", "IVEC4", "VEC2", "VEC3", "VEC4", "MATRIX2",
  "MATRIX3", "MATRIX4", "IN_QUAL", "OUT_QUAL", "INOUT_QUAL", "UNIFORM",
  "VARYING", "STRUCT", "VOID_TYPE", "WHILE", "SAMPLER2D", "SAMPLERCUBE",
  "SAMPLER_EXTERNAL_OES", "SAMPLER2DRECT", "IDENTIFIER", "TYPE_NAME",
  "FLOATCONSTANT", "INTCONSTANT", "BOOLCONSTANT", "FIELD_SELECTION",
  "LEFT_OP", "RIGHT_OP", "INC_OP", "DEC_OP", "LE_OP", "GE_OP", "EQ_OP",
  "NE_OP", "AND_OP", "OR_OP", "XOR_OP", "MUL_ASSIGN", "DIV_ASSIGN",
  "ADD_ASSIGN", "MOD_ASSIGN", "LEFT_ASSIGN", "RIGHT_ASSIGN", "AND_ASSIGN",
  "XOR_ASSIGN", "OR_ASSIGN", "SUB_ASSIGN", "LEFT_PAREN", "RIGHT_PAREN",
  "LEFT_BRACKET", "RIGHT_BRACKET", "LEFT_BRACE", "RIGHT_BRACE", "DOT",
  "COMMA", "COLON", "EQUAL", "SEMICOLON", "BANG", "DASH", "TILDE", "PLUS",
  "STAR", "SLASH", "PERCENT", "LEFT_ANGLE", "RIGHT_ANGLE", "VERTICAL_BAR",
  "CARET", "AMPERSAND", "QUESTION", "$accept", "variable_identifier",
  "primary_expression", "postfix_expression", "integer_expression",
  "function_call", "function_call_or_method", "function_call_generic",
  "function_call_header_no_parameters",
  "function_call_header_with_parameters", "function_call_header",
  "function_identifier", "unary_expression", "unary_operator",
  "multiplicative_expression", "additive_expression", "shift_expression",
  "relational_expression", "equality_expression", "and_expression",
  "exclusive_or_expression", "inclusive_or_expression",
  "logical_and_expression", "logical_xor_expression",
  "logical_or_expression", "conditional_expression",
  "assignment_expression", "assignment_operator", "expression",
  "constant_expression", "declaration", "function_prototype",
  "function_declarator", "function_header_with_parameters",
  "function_header", "parameter_declarator", "parameter_declaration",
  "parameter_qualifier", "parameter_type_specifier",
  "init_declarator_list", "single_declaration", "fully_specified_type",
  "type_qualifier", "type_specifier", "precision_qualifier",
  "type_specifier_no_prec", "type_specifier_nonarray", "struct_specifier",
  "@1", "@2", "struct_declaration_list", "struct_declaration",
  "struct_declarator_list", "struct_declarator", "initializer",
  "declaration_statement", "statement", "simple_statement",
  "compound_statement", "@3", "@4", "statement_no_new_scope",
  "statement_with_scope", "@5", "@6", "compound_statement_no_new_scope",
  "statement_list", "expression_statement", "selection_statement",
  "selection_rest_statement", "condition", "iteration_statement", "@7",
  "@8", "@9", "for_init_statement", "conditionopt", "for_rest_statement",
  "jump_statement", "translation_unit", "external_declaration",
  "function_definition", "@10", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    96,    97,    98,    98,    98,    98,    98,    99,    99,
      99,    99,    99,    99,   100,   101,   102,   102,   103,   103,
     104,   104,   105,   105,   106,   107,   107,   107,   108,   108,
     108,   108,   109,   109,   109,   110,   110,   110,   111,   111,
     111,   112,   113,   113,   113,   113,   113,   114,   114,   114,
     115,   116,   117,   118,   118,   119,   119,   120,   120,   121,
     121,   122,   122,   123,   123,   123,   123,   123,   124,   124,
     125,   126,   126,   126,   127,   128,   128,   129,   129,   130,
     131,   131,   132,   132,   132,   132,   133,   133,   133,   133,
     134,   135,   135,   135,   135,   135,   136,   136,   136,   136,
     136,   136,   137,   137,   138,   138,   138,   138,   138,   139,
     139,   140,   140,   140,   141,   141,   142,   142,   142,   142,
     142,   142,   142,   142,   142,   142,   142,   142,   142,   142,
     142,   142,   142,   142,   142,   142,   142,   142,   144,   143,
     145,   143,   146,   146,   147,   148,   148,   149,   149,   150,
     151,   152,   152,   153,   153,   153,   153,   153,   154,   155,
     156,   154,   157,   157,   159,   158,   160,   158,   161,   161,
     162,   162,   163,   163,   164,   165,   165,   166,   166,   168,
     167,   169,   167,   170,   167,   171,   171,   172,   172,   173,
     173,   174,   174,   174,   174,   174,   175,   175,   176,   176,
     178,   177
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     1,     1,     1,     1,     3,     1,     4,
       1,     3,     2,     2,     1,     1,     1,     3,     2,     2,
       2,     1,     2,     3,     2,     1,     1,     1,     1,     2,
       2,     2,     1,     1,     1,     1,     3,     3,     1,     3,
       3,     1,     1,     3,     3,     3,     3,     1,     3,     3,
       1,     1,     1,     1,     3,     1,     3,     1,     3,     1,
       5,     1,     3,     1,     1,     1,     1,     1,     1,     3,
       1,     2,     2,     4,     2,     1,     1,     2,     3,     3,
       2,     5,     3,     2,     3,     2,     0,     1,     1,     1,
       1,     1,     3,     5,     6,     5,     1,     2,     4,     5,
       4,     2,     1,     2,     1,     1,     1,     2,     1,     1,
       2,     1,     1,     1,     1,     4,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     0,     6,
       0,     5,     1,     2,     3,     1,     3,     1,     4,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     2,     0,
       0,     5,     1,     1,     0,     2,     0,     2,     2,     3,
       1,     2,     1,     2,     5,     3,     1,     1,     4,     0,
       6,     0,     8,     0,     7,     1,     1,     1,     0,     2,
       3,     2,     2,     2,     3,     2,     1,     2,     1,     1,
       0,     3
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     0,   111,   112,   113,     0,   105,   104,   119,   117,
     118,   123,   124,   125,   126,   127,   128,   120,   121,   122,
     129,   130,   131,   108,   106,     0,   116,   132,   133,   134,
     135,   137,   199,   200,     0,    76,    86,     0,    91,    96,
       0,   102,     0,   109,   114,   136,     0,   196,   198,   107,
     101,     0,     0,   140,    71,     0,    74,    86,     0,    87,
      88,    89,    77,     0,    86,     0,    72,    97,   103,   110,
       0,     1,   197,     0,   138,     0,     0,   201,    78,    83,
      85,    90,     0,    92,    79,     0,     0,     2,     5,     4,
       6,    27,     0,     0,     0,    34,    33,    32,     3,     8,
      28,    10,    15,    16,     0,     0,    21,     0,    35,     0,
      38,    41,    42,    47,    50,    51,    52,    53,    55,    57,
      59,    70,     0,    25,    73,     0,     0,     0,   142,     0,
       0,   181,     0,     0,     0,     0,     0,   159,   168,   172,
      35,    61,    68,     0,   150,     0,   114,   153,   170,   152,
     151,     0,   154,   155,   156,   157,    80,    82,    84,     0,
       0,    98,     0,   149,   100,    29,    30,     0,    12,    13,
       0,     0,    19,    18,     0,    20,    22,    24,    31,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   115,     0,   147,     0,   145,   141,   143,
     192,   191,   166,   183,     0,   195,   193,     0,   179,   158,
       0,    64,    65,    66,    67,    63,     0,     0,   173,   169,
     171,     0,    93,     0,    95,    99,     7,     0,    14,    26,
      11,    17,    23,    36,    37,    40,    39,    45,    46,    43,
      44,    48,    49,    54,    56,    58,     0,   139,     0,     0,
     144,     0,     0,     0,     0,     0,   194,     0,   160,    62,
      69,     0,    94,     9,     0,     0,   146,     0,   165,   167,
     186,   185,   188,   166,   177,     0,     0,     0,    81,    60,
     148,     0,   187,     0,     0,   176,   174,     0,     0,   161,
       0,   189,     0,   166,     0,   163,   180,   162,     0,   190,
     184,   175,   178,   182
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    98,    99,   100,   227,   101,   102,   103,   104,   105,
     106,   107,   140,   109,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,   120,   141,   142,   216,   143,   122,
     144,   145,    34,    35,    36,    79,    62,    63,    80,    37,
      38,    39,    40,    41,    42,    43,   123,    45,   125,    75,
     127,   128,   196,   197,   164,   147,   148,   149,   150,   210,
     277,   296,   251,   252,   253,   297,   151,   152,   153,   286,
     276,   154,   257,   202,   254,   272,   283,   284,   155,    46,
      47,    48,    55
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -266
static const yytype_int16 yypact[] =
{
    1253,   -20,  -266,  -266,  -266,   148,  -266,  -266,  -266,  -266,
    -266,  -266,  -266,  -266,  -266,  -266,  -266,  -266,  -266,  -266,
    -266,  -266,  -266,  -266,  -266,   -39,  -266,  -266,  -266,  -266,
    -266,  -266,  -266,   -18,    -2,     6,    21,   -61,  -266,    51,
    1296,  -266,  1370,  -266,    25,  -266,  1209,  -266,  -266,  -266,
    -266,  1370,    42,  -266,  -266,    50,  -266,    71,    95,  -266,
    -266,  -266,  -266,  1296,   123,   105,  -266,     9,  -266,  -266,
     974,  -266,  -266,    81,  -266,  1296,   290,  -266,  -266,  -266,
    -266,   125,  1296,   -13,  -266,   776,   974,    99,  -266,  -266,
    -266,  -266,   974,   974,   974,  -266,  -266,  -266,  -266,  -266,
      35,  -266,  -266,  -266,   100,    -6,  1040,   104,  -266,   974,
      36,   -64,  -266,   -21,   102,  -266,  -266,  -266,   113,   117,
     -51,  -266,   108,  -266,  -266,  1296,   129,  1109,  -266,    97,
     103,  -266,   112,   114,   106,   842,   115,   116,  -266,  -266,
      39,  -266,  -266,   -43,  -266,   -18,    47,  -266,  -266,  -266,
    -266,   374,  -266,  -266,  -266,  -266,   118,  -266,  -266,   908,
     974,  -266,   120,  -266,  -266,  -266,  -266,    19,  -266,  -266,
     974,  1333,  -266,  -266,   974,   119,  -266,  -266,  -266,   974,
     974,   974,   974,   974,   974,   974,   974,   974,   974,   974,
     974,   974,   974,  -266,  1152,   122,   -29,  -266,  -266,  -266,
    -266,  -266,   121,  -266,   974,  -266,  -266,     5,  -266,  -266,
     458,  -266,  -266,  -266,  -266,  -266,   974,   974,  -266,  -266,
    -266,   974,  -266,   137,  -266,  -266,  -266,   138,   111,  -266,
     142,  -266,  -266,  -266,  -266,    36,    36,  -266,  -266,  -266,
    -266,   -21,   -21,  -266,   113,   117,    82,  -266,   974,   129,
    -266,   175,    50,   626,   710,    38,  -266,   197,   458,  -266,
    -266,   141,  -266,  -266,   974,   155,  -266,   145,  -266,  -266,
    -266,  -266,   197,   121,   111,   186,   159,   160,  -266,  -266,
    -266,   974,  -266,   166,   176,   236,  -266,   174,   542,  -266,
      43,   974,   542,   121,   974,  -266,  -266,  -266,   177,   111,
    -266,  -266,  -266,  -266
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -266,  -266,  -266,  -266,  -266,  -266,  -266,    85,  -266,  -266,
    -266,  -266,   -44,  -266,   -15,  -266,   -55,   -19,  -266,  -266,
    -266,    72,    70,    73,  -266,   -66,   -83,  -266,   -92,   -73,
      13,    14,  -266,  -266,  -266,   180,   206,   201,   184,  -266,
    -266,  -241,   -25,   -30,   262,    -4,     0,  -266,  -266,  -266,
     143,  -122,  -266,    22,  -145,    16,  -144,  -226,  -266,  -266,
    -266,   -17,  -265,  -266,  -266,   -54,    63,    20,  -266,  -266,
       4,  -266,  -266,  -266,  -266,  -266,  -266,  -266,  -266,  -266,
     231,  -266,  -266
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -165
static const yytype_int16 yytable[] =
{
      44,    77,   167,   163,   121,   199,    52,   220,   285,   191,
      68,    64,   162,    32,    33,   224,   275,    49,    65,   121,
     181,    66,   182,   176,    58,    50,   108,   269,   301,     6,
       7,   275,    64,    81,   183,   184,   217,    53,    69,   218,
      44,   108,    44,   207,   192,   126,    44,    73,   165,   166,
     249,    44,    81,   250,    59,    60,    61,    23,    24,    32,
      33,   159,   295,    44,    54,   178,   295,   173,   160,   185,
     186,    56,   199,   174,    58,    44,   146,   163,   228,     6,
       7,    84,    44,    85,   217,    57,   223,   256,   168,   169,
      86,   232,   226,   121,   -75,   126,    67,   126,   217,    70,
     246,   211,   212,   213,    59,    60,    61,    23,    24,   170,
     214,   273,   255,   171,   220,   108,   298,   217,    74,   -25,
     215,    70,   217,   179,   180,    44,    76,    44,   237,   238,
     239,   240,    49,   259,   260,   233,   234,   108,   108,   108,
     108,   108,   108,   108,   108,   108,   108,   108,   261,   302,
      83,   146,     2,     3,     4,   121,    59,    60,    61,   187,
     188,   217,   264,   124,   126,   274,   235,   236,   241,   242,
     156,   -26,   189,   172,   195,   265,   177,   108,   190,   200,
     274,   279,   121,   193,   203,   201,   204,   208,   205,   290,
     217,  -116,   221,   209,    44,   225,   248,  -164,   268,   299,
      58,     2,     3,     4,   108,     6,     7,     8,     9,    10,
     146,   163,   262,   263,   -27,   267,   278,   281,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
     280,   287,   288,    23,    24,    25,    26,   289,    27,    28,
      29,    30,    87,    31,    88,    89,    90,    91,   291,   292,
      92,    93,   293,   146,   146,   294,   231,   146,   146,   303,
     244,   243,   157,    78,   245,    82,   158,    51,   194,    94,
     270,   266,   146,   258,   271,   300,   282,    72,     0,     0,
      95,    96,     0,    97,     0,     0,     0,     0,   146,     0,
       0,     0,   146,     1,     2,     3,     4,     5,     6,     7,
       8,     9,    10,   129,   130,   131,     0,   132,   133,   134,
     135,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,     0,     0,     0,    23,    24,    25,    26,
     136,    27,    28,    29,    30,    87,    31,    88,    89,    90,
      91,     0,     0,    92,    93,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    94,     0,     0,     0,   137,   138,     0,     0,
       0,     0,   139,    95,    96,     0,    97,     1,     2,     3,
       4,     5,     6,     7,     8,     9,    10,   129,   130,   131,
       0,   132,   133,   134,   135,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,     0,     0,     0,
      23,    24,    25,    26,   136,    27,    28,    29,    30,    87,
      31,    88,    89,    90,    91,     0,     0,    92,    93,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    94,     0,     0,     0,
     137,   219,     0,     0,     0,     0,   139,    95,    96,     0,
      97,     1,     2,     3,     4,     5,     6,     7,     8,     9,
      10,   129,   130,   131,     0,   132,   133,   134,   135,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,     0,     0,     0,    23,    24,    25,    26,   136,    27,
      28,    29,    30,    87,    31,    88,    89,    90,    91,     0,
       0,    92,    93,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      94,     0,     0,     0,   137,     0,     0,     0,     0,     0,
     139,    95,    96,     0,    97,     1,     2,     3,     4,     5,
       6,     7,     8,     9,    10,   129,   130,   131,     0,   132,
     133,   134,   135,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,     0,     0,     0,    23,    24,
      25,    26,   136,    27,    28,    29,    30,    87,    31,    88,
      89,    90,    91,     0,     0,    92,    93,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    94,     0,     0,     0,    76,     0,
       0,     0,     0,     0,   139,    95,    96,     0,    97,     1,
       2,     3,     4,     5,     6,     7,     8,     9,    10,   129,
     130,   131,     0,   132,   133,   134,   135,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,     0,
       0,     0,    23,    24,    25,    26,   136,    27,    28,    29,
      30,    87,    31,    88,    89,    90,    91,     0,     0,    92,
      93,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    94,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   139,    95,
      96,     0,    97,     1,     2,     3,     4,     5,     6,     7,
       8,     9,    10,     0,     0,     0,     0,     0,     0,     0,
       0,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,     0,     0,     0,    23,    24,    25,    26,
       0,    27,    28,    29,    30,    87,    31,    88,    89,    90,
      91,     0,     0,    92,    93,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    94,     0,     0,     0,     8,     9,    10,     0,
       0,     0,   139,    95,    96,     0,    97,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,     0,
       0,     0,     0,     0,    25,    26,     0,    27,    28,    29,
      30,    87,    31,    88,    89,    90,    91,     0,     0,    92,
      93,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    94,     0,
       0,   161,     8,     9,    10,     0,     0,     0,     0,    95,
      96,     0,    97,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,     0,     0,     0,     0,     0,
      25,    26,     0,    27,    28,    29,    30,    87,    31,    88,
      89,    90,    91,     0,     0,    92,    93,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    94,     0,     0,     0,     8,     9,
      10,     0,     0,     0,   206,    95,    96,     0,    97,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,     0,     0,     0,     0,     0,    25,    26,     0,    27,
      28,    29,    30,    87,    31,    88,    89,    90,    91,     0,
       0,    92,    93,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      94,     0,     0,   222,     8,     9,    10,     0,     0,     0,
       0,    95,    96,     0,    97,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,     0,     0,     0,
       0,     0,    25,    26,     0,    27,    28,    29,    30,    87,
      31,    88,    89,    90,    91,     0,     0,    92,    93,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    94,     0,     0,     0,
       8,     9,    10,     0,     0,     0,     0,    95,    96,     0,
      97,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,     0,     0,     0,     0,     0,    25,   175,
       0,    27,    28,    29,    30,    87,    31,    88,    89,    90,
      91,     0,     0,    92,    93,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    94,     2,     3,     4,     0,     0,     0,     8,
       9,    10,     0,    95,    96,     0,    97,     0,     0,     0,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,     0,     0,     0,     0,     0,    25,    26,     0,
      27,    28,    29,    30,     0,    31,     2,     3,     4,     0,
       0,     0,     8,     9,    10,     0,     0,     0,     0,     0,
       0,     0,     0,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,     0,   198,     0,     0,     0,
      25,    26,     0,    27,    28,    29,    30,     0,    31,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    71,
       0,     0,     1,     2,     3,     4,     5,     6,     7,     8,
       9,    10,     0,     0,     0,     0,     0,     0,     0,   247,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,     0,     0,     0,    23,    24,    25,    26,     0,
      27,    28,    29,    30,     0,    31,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,     0,     0,     0,     0,
       0,     0,     0,     0,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,     0,     0,     0,    23,
      24,    25,    26,     0,    27,    28,    29,    30,     0,    31,
       2,     3,     4,     0,     0,     0,     8,     9,    10,     0,
       0,     0,     0,     0,     0,     0,     0,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,     0,
       0,     0,     0,     0,    25,    26,     0,    27,    28,    29,
      30,     0,    31,     8,     9,    10,     0,     0,     0,     0,
       0,     0,     0,     0,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,     0,     0,     0,     0,
       0,    25,    26,     0,    27,    28,    29,    30,   229,    31,
       8,     9,    10,   230,     0,     0,     0,     0,     0,     0,
       0,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,     0,     0,     0,     0,     0,    25,    26,
       0,    27,    28,    29,    30,     0,    31
};

static const yytype_int16 yycheck[] =
{
       0,    55,    94,    86,    70,   127,    45,   151,   273,    60,
      40,    36,    85,     0,     0,   160,   257,    37,    79,    85,
      84,    82,    86,   106,     3,    45,    70,   253,   293,     8,
       9,   272,    57,    63,    55,    56,    79,    76,    42,    82,
      40,    85,    42,   135,    95,    75,    46,    51,    92,    93,
      79,    51,    82,    82,    33,    34,    35,    36,    37,    46,
      46,    74,   288,    63,    82,   109,   292,    73,    81,    90,
      91,    73,   194,    79,     3,    75,    76,   160,   170,     8,
       9,    72,    82,    74,    79,    79,   159,    82,    53,    54,
      81,   174,    73,   159,    73,   125,    45,   127,    79,    74,
     192,    62,    63,    64,    33,    34,    35,    36,    37,    74,
      71,    73,   204,    78,   258,   159,    73,    79,    76,    72,
      81,    74,    79,    87,    88,   125,    76,   127,   183,   184,
     185,   186,    37,   216,   217,   179,   180,   181,   182,   183,
     184,   185,   186,   187,   188,   189,   190,   191,   221,   294,
      45,   151,     4,     5,     6,   221,    33,    34,    35,    57,
      58,    79,    80,    82,   194,   257,   181,   182,   187,   188,
      45,    72,    59,    73,    45,   248,    72,   221,    61,    82,
     272,   264,   248,    75,    72,    82,    72,    72,    82,   281,
      79,    72,    74,    77,   194,    75,    74,    76,   252,   291,
       3,     4,     5,     6,   248,     8,     9,    10,    11,    12,
     210,   294,    75,    75,    72,    40,    75,    72,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      75,    45,    73,    36,    37,    38,    39,    77,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    82,    73,
      53,    54,    16,   253,   254,    81,   171,   257,   258,    82,
     190,   189,    82,    57,   191,    64,    82,     5,   125,    72,
     254,   249,   272,   210,   254,   292,   272,    46,    -1,    -1,
      83,    84,    -1,    86,    -1,    -1,    -1,    -1,   288,    -1,
      -1,    -1,   292,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    -1,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    -1,    -1,    -1,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    -1,    -1,    53,    54,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    72,    -1,    -1,    -1,    76,    77,    -1,    -1,
      -1,    -1,    82,    83,    84,    -1,    86,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      -1,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    -1,    -1,    -1,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    -1,    -1,    53,    54,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    72,    -1,    -1,    -1,
      76,    77,    -1,    -1,    -1,    -1,    82,    83,    84,    -1,
      86,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    -1,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    -1,    -1,    -1,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    -1,
      -1,    53,    54,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      72,    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,
      82,    83,    84,    -1,    86,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    14,    15,    -1,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    -1,    -1,    -1,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    -1,    -1,    53,    54,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    72,    -1,    -1,    -1,    76,    -1,
      -1,    -1,    -1,    -1,    82,    83,    84,    -1,    86,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    12,    13,
      14,    15,    -1,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    -1,
      -1,    -1,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    -1,    -1,    53,
      54,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    72,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,    83,
      84,    -1,    86,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    -1,    -1,    -1,    36,    37,    38,    39,
      -1,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    -1,    -1,    53,    54,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    72,    -1,    -1,    -1,    10,    11,    12,    -1,
      -1,    -1,    82,    83,    84,    -1,    86,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    -1,
      -1,    -1,    -1,    -1,    38,    39,    -1,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    -1,    -1,    53,
      54,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    72,    -1,
      -1,    75,    10,    11,    12,    -1,    -1,    -1,    -1,    83,
      84,    -1,    86,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    -1,    -1,    -1,    -1,    -1,
      38,    39,    -1,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    -1,    -1,    53,    54,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    72,    -1,    -1,    -1,    10,    11,
      12,    -1,    -1,    -1,    82,    83,    84,    -1,    86,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    -1,    -1,    -1,    -1,    -1,    38,    39,    -1,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    -1,
      -1,    53,    54,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      72,    -1,    -1,    75,    10,    11,    12,    -1,    -1,    -1,
      -1,    83,    84,    -1,    86,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    -1,    -1,    -1,
      -1,    -1,    38,    39,    -1,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    -1,    -1,    53,    54,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    72,    -1,    -1,    -1,
      10,    11,    12,    -1,    -1,    -1,    -1,    83,    84,    -1,
      86,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    -1,    -1,    -1,    -1,    -1,    38,    39,
      -1,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    -1,    -1,    53,    54,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    72,     4,     5,     6,    -1,    -1,    -1,    10,
      11,    12,    -1,    83,    84,    -1,    86,    -1,    -1,    -1,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    -1,    -1,    -1,    -1,    -1,    38,    39,    -1,
      41,    42,    43,    44,    -1,    46,     4,     5,     6,    -1,
      -1,    -1,    10,    11,    12,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    -1,    77,    -1,    -1,    -1,
      38,    39,    -1,    41,    42,    43,    44,    -1,    46,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     0,
      -1,    -1,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    12,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    77,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    -1,    -1,    -1,    36,    37,    38,    39,    -1,
      41,    42,    43,    44,    -1,    46,     3,     4,     5,     6,
       7,     8,     9,    10,    11,    12,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    -1,    -1,    -1,    36,
      37,    38,    39,    -1,    41,    42,    43,    44,    -1,    46,
       4,     5,     6,    -1,    -1,    -1,    10,    11,    12,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    -1,
      -1,    -1,    -1,    -1,    38,    39,    -1,    41,    42,    43,
      44,    -1,    46,    10,    11,    12,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    -1,    -1,    -1,    -1,
      -1,    38,    39,    -1,    41,    42,    43,    44,    45,    46,
      10,    11,    12,    50,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    -1,    -1,    -1,    -1,    -1,    38,    39,
      -1,    41,    42,    43,    44,    -1,    46
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    36,    37,    38,    39,    41,    42,    43,
      44,    46,   126,   127,   128,   129,   130,   135,   136,   137,
     138,   139,   140,   141,   142,   143,   175,   176,   177,    37,
      45,   140,    45,    76,    82,   178,    73,    79,     3,    33,
      34,    35,   132,   133,   138,    79,    82,    45,   139,   141,
      74,     0,   176,   141,    76,   145,    76,   161,   132,   131,
     134,   139,   133,    45,    72,    74,    81,    45,    47,    48,
      49,    50,    53,    54,    72,    83,    84,    86,    97,    98,
      99,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   125,   142,    82,   144,   139,   146,   147,    13,
      14,    15,    17,    18,    19,    20,    40,    76,    77,    82,
     108,   121,   122,   124,   126,   127,   142,   151,   152,   153,
     154,   162,   163,   164,   167,   174,    45,   131,   134,    74,
      81,    75,   125,   122,   150,   108,   108,   124,    53,    54,
      74,    78,    73,    73,    79,    39,   122,    72,   108,    87,
      88,    84,    86,    55,    56,    90,    91,    57,    58,    59,
      61,    60,    95,    75,   146,    45,   148,   149,    77,   147,
      82,    82,   169,    72,    72,    82,    82,   124,    72,    77,
     155,    62,    63,    64,    71,    81,   123,    79,    82,    77,
     152,    74,    75,   125,   150,    75,    73,   100,   124,    45,
      50,   103,   122,   108,   108,   110,   110,   112,   112,   112,
     112,   113,   113,   117,   118,   119,   124,    77,    74,    79,
      82,   158,   159,   160,   170,   124,    82,   168,   162,   122,
     122,   125,    75,    75,    80,   125,   149,    40,   161,   153,
     151,   163,   171,    73,   124,   137,   166,   156,    75,   122,
      75,    72,   166,   172,   173,   158,   165,    45,    73,    77,
     124,    82,    73,    16,    81,   153,   157,   161,    73,   124,
     157,   158,   150,    82
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (context, YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (&yylval, YYLEX_PARAM)
#else
# define YYLEX yylex (&yylval)
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value, context); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, TParseContext* context)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, context)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    TParseContext* context;
#endif
{
  if (!yyvaluep)
    return;
  YYUSE (context);
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
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, TParseContext* context)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, context)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    TParseContext* context;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, context);
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

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule, TParseContext* context)
#else
static void
yy_reduce_print (yyvsp, yyrule, context)
    YYSTYPE *yyvsp;
    int yyrule;
    TParseContext* context;
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
		       		       , context);
      fprintf (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule, context); \
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
#ifndef	YYINITDEPTH
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
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, TParseContext* context)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, context)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    TParseContext* context;
#endif
{
  YYUSE (yyvaluep);
  YYUSE (context);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

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
int yyparse (TParseContext* context);
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
yyparse (TParseContext* context)
#else
int
yyparse (context)
    TParseContext* context;
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
  yychar = YYEMPTY;		/* Cause a token to be read.  */

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

    {
        // The symbol table search was done in the lexical phase
        const TSymbol* symbol = (yyvsp[(1) - (1)].lex).symbol;
        const TVariable* variable;
        if (symbol == 0) {
            context->error((yyvsp[(1) - (1)].lex).line, "undeclared identifier", (yyvsp[(1) - (1)].lex).string->c_str());
            context->recover();
            TType type(EbtFloat, EbpUndefined);
            TVariable* fakeVariable = new TVariable((yyvsp[(1) - (1)].lex).string, type);
            context->symbolTable.insert(*fakeVariable);
            variable = fakeVariable;
        } else {
            // This identifier can only be a variable type symbol
            if (! symbol->isVariable()) {
                context->error((yyvsp[(1) - (1)].lex).line, "variable expected", (yyvsp[(1) - (1)].lex).string->c_str());
                context->recover();
            }
            variable = static_cast<const TVariable*>(symbol);
        }

        // don't delete $1.string, it's used by error recovery, and the pool
        // pop will reclaim the memory

        if (variable->getType().getQualifier() == EvqConst ) {
            ConstantUnion* constArray = variable->getConstPointer();
            TType t(variable->getType());
            (yyval.interm.intermTypedNode) = context->intermediate.addConstantUnion(constArray, t, (yyvsp[(1) - (1)].lex).line);
        } else
            (yyval.interm.intermTypedNode) = context->intermediate.addSymbol(variable->getUniqueId(),
                                                     variable->getName(),
                                                     variable->getType(), (yyvsp[(1) - (1)].lex).line);
    ;}
    break;

  case 3:

    {
        (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode);
    ;}
    break;

  case 4:

    {
        //
        // INT_TYPE is only 16-bit plus sign bit for vertex/fragment shaders,
        // check for overflow for constants
        //
        if (abs((yyvsp[(1) - (1)].lex).i) >= (1 << 16)) {
            context->error((yyvsp[(1) - (1)].lex).line, " integer constant overflow", "");
            context->recover();
        }
        ConstantUnion *unionArray = new ConstantUnion[1];
        unionArray->setIConst((yyvsp[(1) - (1)].lex).i);
        (yyval.interm.intermTypedNode) = context->intermediate.addConstantUnion(unionArray, TType(EbtInt, EbpUndefined, EvqConst), (yyvsp[(1) - (1)].lex).line);
    ;}
    break;

  case 5:

    {
        ConstantUnion *unionArray = new ConstantUnion[1];
        unionArray->setFConst((yyvsp[(1) - (1)].lex).f);
        (yyval.interm.intermTypedNode) = context->intermediate.addConstantUnion(unionArray, TType(EbtFloat, EbpUndefined, EvqConst), (yyvsp[(1) - (1)].lex).line);
    ;}
    break;

  case 6:

    {
        ConstantUnion *unionArray = new ConstantUnion[1];
        unionArray->setBConst((yyvsp[(1) - (1)].lex).b);
        (yyval.interm.intermTypedNode) = context->intermediate.addConstantUnion(unionArray, TType(EbtBool, EbpUndefined, EvqConst), (yyvsp[(1) - (1)].lex).line);
    ;}
    break;

  case 7:

    {
        (yyval.interm.intermTypedNode) = (yyvsp[(2) - (3)].interm.intermTypedNode);
    ;}
    break;

  case 8:

    {
        (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode);
    ;}
    break;

  case 9:

    {
        if (!(yyvsp[(1) - (4)].interm.intermTypedNode)->isArray() && !(yyvsp[(1) - (4)].interm.intermTypedNode)->isMatrix() && !(yyvsp[(1) - (4)].interm.intermTypedNode)->isVector()) {
            if ((yyvsp[(1) - (4)].interm.intermTypedNode)->getAsSymbolNode())
                context->error((yyvsp[(2) - (4)].lex).line, " left of '[' is not of type array, matrix, or vector ", (yyvsp[(1) - (4)].interm.intermTypedNode)->getAsSymbolNode()->getSymbol().c_str());
            else
                context->error((yyvsp[(2) - (4)].lex).line, " left of '[' is not of type array, matrix, or vector ", "expression");
            context->recover();
        }
        if ((yyvsp[(1) - (4)].interm.intermTypedNode)->getType().getQualifier() == EvqConst && (yyvsp[(3) - (4)].interm.intermTypedNode)->getQualifier() == EvqConst) {
            if ((yyvsp[(1) - (4)].interm.intermTypedNode)->isArray()) { // constant folding for arrays
                (yyval.interm.intermTypedNode) = context->addConstArrayNode((yyvsp[(3) - (4)].interm.intermTypedNode)->getAsConstantUnion()->getUnionArrayPointer()->getIConst(), (yyvsp[(1) - (4)].interm.intermTypedNode), (yyvsp[(2) - (4)].lex).line);
            } else if ((yyvsp[(1) - (4)].interm.intermTypedNode)->isVector()) {  // constant folding for vectors
                TVectorFields fields;
                fields.num = 1;
                fields.offsets[0] = (yyvsp[(3) - (4)].interm.intermTypedNode)->getAsConstantUnion()->getUnionArrayPointer()->getIConst(); // need to do it this way because v.xy sends fields integer array
                (yyval.interm.intermTypedNode) = context->addConstVectorNode(fields, (yyvsp[(1) - (4)].interm.intermTypedNode), (yyvsp[(2) - (4)].lex).line);
            } else if ((yyvsp[(1) - (4)].interm.intermTypedNode)->isMatrix()) { // constant folding for matrices
                (yyval.interm.intermTypedNode) = context->addConstMatrixNode((yyvsp[(3) - (4)].interm.intermTypedNode)->getAsConstantUnion()->getUnionArrayPointer()->getIConst(), (yyvsp[(1) - (4)].interm.intermTypedNode), (yyvsp[(2) - (4)].lex).line);
            }
        } else {
            if ((yyvsp[(3) - (4)].interm.intermTypedNode)->getQualifier() == EvqConst) {
                if (((yyvsp[(1) - (4)].interm.intermTypedNode)->isVector() || (yyvsp[(1) - (4)].interm.intermTypedNode)->isMatrix()) && (yyvsp[(1) - (4)].interm.intermTypedNode)->getType().getNominalSize() <= (yyvsp[(3) - (4)].interm.intermTypedNode)->getAsConstantUnion()->getUnionArrayPointer()->getIConst() && !(yyvsp[(1) - (4)].interm.intermTypedNode)->isArray() ) {
                    std::stringstream extraInfoStream;
                    extraInfoStream << "field selection out of range '" << (yyvsp[(3) - (4)].interm.intermTypedNode)->getAsConstantUnion()->getUnionArrayPointer()->getIConst() << "'";
                    std::string extraInfo = extraInfoStream.str();
                    context->error((yyvsp[(2) - (4)].lex).line, "", "[", extraInfo.c_str());
                    context->recover();
                } else {
                    if ((yyvsp[(1) - (4)].interm.intermTypedNode)->isArray()) {
                        if ((yyvsp[(1) - (4)].interm.intermTypedNode)->getType().getArraySize() == 0) {
                            if ((yyvsp[(1) - (4)].interm.intermTypedNode)->getType().getMaxArraySize() <= (yyvsp[(3) - (4)].interm.intermTypedNode)->getAsConstantUnion()->getUnionArrayPointer()->getIConst()) {
                                if (context->arraySetMaxSize((yyvsp[(1) - (4)].interm.intermTypedNode)->getAsSymbolNode(), (yyvsp[(1) - (4)].interm.intermTypedNode)->getTypePointer(), (yyvsp[(3) - (4)].interm.intermTypedNode)->getAsConstantUnion()->getUnionArrayPointer()->getIConst(), true, (yyvsp[(2) - (4)].lex).line))
                                    context->recover();
                            } else {
                                if (context->arraySetMaxSize((yyvsp[(1) - (4)].interm.intermTypedNode)->getAsSymbolNode(), (yyvsp[(1) - (4)].interm.intermTypedNode)->getTypePointer(), 0, false, (yyvsp[(2) - (4)].lex).line))
                                    context->recover();
                            }
                        } else if ( (yyvsp[(3) - (4)].interm.intermTypedNode)->getAsConstantUnion()->getUnionArrayPointer()->getIConst() >= (yyvsp[(1) - (4)].interm.intermTypedNode)->getType().getArraySize()) {
                            std::stringstream extraInfoStream;
                            extraInfoStream << "array index out of range '" << (yyvsp[(3) - (4)].interm.intermTypedNode)->getAsConstantUnion()->getUnionArrayPointer()->getIConst() << "'";
                            std::string extraInfo = extraInfoStream.str();
                            context->error((yyvsp[(2) - (4)].lex).line, "", "[", extraInfo.c_str());
                            context->recover();
                        }
                    }
                    (yyval.interm.intermTypedNode) = context->intermediate.addIndex(EOpIndexDirect, (yyvsp[(1) - (4)].interm.intermTypedNode), (yyvsp[(3) - (4)].interm.intermTypedNode), (yyvsp[(2) - (4)].lex).line);
                }
            } else {
                if ((yyvsp[(1) - (4)].interm.intermTypedNode)->isArray() && (yyvsp[(1) - (4)].interm.intermTypedNode)->getType().getArraySize() == 0) {
                    context->error((yyvsp[(2) - (4)].lex).line, "", "[", "array must be redeclared with a size before being indexed with a variable");
                    context->recover();
                }

                (yyval.interm.intermTypedNode) = context->intermediate.addIndex(EOpIndexIndirect, (yyvsp[(1) - (4)].interm.intermTypedNode), (yyvsp[(3) - (4)].interm.intermTypedNode), (yyvsp[(2) - (4)].lex).line);
            }
        }
        if ((yyval.interm.intermTypedNode) == 0) {
            ConstantUnion *unionArray = new ConstantUnion[1];
            unionArray->setFConst(0.0f);
            (yyval.interm.intermTypedNode) = context->intermediate.addConstantUnion(unionArray, TType(EbtFloat, EbpHigh, EvqConst), (yyvsp[(2) - (4)].lex).line);
        } else if ((yyvsp[(1) - (4)].interm.intermTypedNode)->isArray()) {
            if ((yyvsp[(1) - (4)].interm.intermTypedNode)->getType().getStruct())
                (yyval.interm.intermTypedNode)->setType(TType((yyvsp[(1) - (4)].interm.intermTypedNode)->getType().getStruct(), (yyvsp[(1) - (4)].interm.intermTypedNode)->getType().getTypeName()));
            else
                (yyval.interm.intermTypedNode)->setType(TType((yyvsp[(1) - (4)].interm.intermTypedNode)->getBasicType(), (yyvsp[(1) - (4)].interm.intermTypedNode)->getPrecision(), EvqTemporary, (yyvsp[(1) - (4)].interm.intermTypedNode)->getNominalSize(), (yyvsp[(1) - (4)].interm.intermTypedNode)->isMatrix()));

            if ((yyvsp[(1) - (4)].interm.intermTypedNode)->getType().getQualifier() == EvqConst)
                (yyval.interm.intermTypedNode)->getTypePointer()->setQualifier(EvqConst);
        } else if ((yyvsp[(1) - (4)].interm.intermTypedNode)->isMatrix() && (yyvsp[(1) - (4)].interm.intermTypedNode)->getType().getQualifier() == EvqConst)
            (yyval.interm.intermTypedNode)->setType(TType((yyvsp[(1) - (4)].interm.intermTypedNode)->getBasicType(), (yyvsp[(1) - (4)].interm.intermTypedNode)->getPrecision(), EvqConst, (yyvsp[(1) - (4)].interm.intermTypedNode)->getNominalSize()));
        else if ((yyvsp[(1) - (4)].interm.intermTypedNode)->isMatrix())
            (yyval.interm.intermTypedNode)->setType(TType((yyvsp[(1) - (4)].interm.intermTypedNode)->getBasicType(), (yyvsp[(1) - (4)].interm.intermTypedNode)->getPrecision(), EvqTemporary, (yyvsp[(1) - (4)].interm.intermTypedNode)->getNominalSize()));
        else if ((yyvsp[(1) - (4)].interm.intermTypedNode)->isVector() && (yyvsp[(1) - (4)].interm.intermTypedNode)->getType().getQualifier() == EvqConst)
            (yyval.interm.intermTypedNode)->setType(TType((yyvsp[(1) - (4)].interm.intermTypedNode)->getBasicType(), (yyvsp[(1) - (4)].interm.intermTypedNode)->getPrecision(), EvqConst));
        else if ((yyvsp[(1) - (4)].interm.intermTypedNode)->isVector())
            (yyval.interm.intermTypedNode)->setType(TType((yyvsp[(1) - (4)].interm.intermTypedNode)->getBasicType(), (yyvsp[(1) - (4)].interm.intermTypedNode)->getPrecision(), EvqTemporary));
        else
            (yyval.interm.intermTypedNode)->setType((yyvsp[(1) - (4)].interm.intermTypedNode)->getType());
    ;}
    break;

  case 10:

    {
        (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode);
    ;}
    break;

  case 11:

    {
        if ((yyvsp[(1) - (3)].interm.intermTypedNode)->isArray()) {
            context->error((yyvsp[(3) - (3)].lex).line, "cannot apply dot operator to an array", ".");
            context->recover();
        }

        if ((yyvsp[(1) - (3)].interm.intermTypedNode)->isVector()) {
            TVectorFields fields;
            if (! context->parseVectorFields(*(yyvsp[(3) - (3)].lex).string, (yyvsp[(1) - (3)].interm.intermTypedNode)->getNominalSize(), fields, (yyvsp[(3) - (3)].lex).line)) {
                fields.num = 1;
                fields.offsets[0] = 0;
                context->recover();
            }

            if ((yyvsp[(1) - (3)].interm.intermTypedNode)->getType().getQualifier() == EvqConst) { // constant folding for vector fields
                (yyval.interm.intermTypedNode) = context->addConstVectorNode(fields, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].lex).line);
                if ((yyval.interm.intermTypedNode) == 0) {
                    context->recover();
                    (yyval.interm.intermTypedNode) = (yyvsp[(1) - (3)].interm.intermTypedNode);
                }
                else
                    (yyval.interm.intermTypedNode)->setType(TType((yyvsp[(1) - (3)].interm.intermTypedNode)->getBasicType(), (yyvsp[(1) - (3)].interm.intermTypedNode)->getPrecision(), EvqConst, (int) (*(yyvsp[(3) - (3)].lex).string).size()));
            } else {
                TString vectorString = *(yyvsp[(3) - (3)].lex).string;
                TIntermTyped* index = context->intermediate.addSwizzle(fields, (yyvsp[(3) - (3)].lex).line);
                (yyval.interm.intermTypedNode) = context->intermediate.addIndex(EOpVectorSwizzle, (yyvsp[(1) - (3)].interm.intermTypedNode), index, (yyvsp[(2) - (3)].lex).line);
                (yyval.interm.intermTypedNode)->setType(TType((yyvsp[(1) - (3)].interm.intermTypedNode)->getBasicType(), (yyvsp[(1) - (3)].interm.intermTypedNode)->getPrecision(), EvqTemporary, (int) vectorString.size()));
            }
        } else if ((yyvsp[(1) - (3)].interm.intermTypedNode)->isMatrix()) {
            TMatrixFields fields;
            if (! context->parseMatrixFields(*(yyvsp[(3) - (3)].lex).string, (yyvsp[(1) - (3)].interm.intermTypedNode)->getNominalSize(), fields, (yyvsp[(3) - (3)].lex).line)) {
                fields.wholeRow = false;
                fields.wholeCol = false;
                fields.row = 0;
                fields.col = 0;
                context->recover();
            }

            if (fields.wholeRow || fields.wholeCol) {
                context->error((yyvsp[(2) - (3)].lex).line, " non-scalar fields not implemented yet", ".");
                context->recover();
                ConstantUnion *unionArray = new ConstantUnion[1];
                unionArray->setIConst(0);
                TIntermTyped* index = context->intermediate.addConstantUnion(unionArray, TType(EbtInt, EbpUndefined, EvqConst), (yyvsp[(3) - (3)].lex).line);
                (yyval.interm.intermTypedNode) = context->intermediate.addIndex(EOpIndexDirect, (yyvsp[(1) - (3)].interm.intermTypedNode), index, (yyvsp[(2) - (3)].lex).line);
                (yyval.interm.intermTypedNode)->setType(TType((yyvsp[(1) - (3)].interm.intermTypedNode)->getBasicType(), (yyvsp[(1) - (3)].interm.intermTypedNode)->getPrecision(),EvqTemporary, (yyvsp[(1) - (3)].interm.intermTypedNode)->getNominalSize()));
            } else {
                ConstantUnion *unionArray = new ConstantUnion[1];
                unionArray->setIConst(fields.col * (yyvsp[(1) - (3)].interm.intermTypedNode)->getNominalSize() + fields.row);
                TIntermTyped* index = context->intermediate.addConstantUnion(unionArray, TType(EbtInt, EbpUndefined, EvqConst), (yyvsp[(3) - (3)].lex).line);
                (yyval.interm.intermTypedNode) = context->intermediate.addIndex(EOpIndexDirect, (yyvsp[(1) - (3)].interm.intermTypedNode), index, (yyvsp[(2) - (3)].lex).line);
                (yyval.interm.intermTypedNode)->setType(TType((yyvsp[(1) - (3)].interm.intermTypedNode)->getBasicType(), (yyvsp[(1) - (3)].interm.intermTypedNode)->getPrecision()));
            }
        } else if ((yyvsp[(1) - (3)].interm.intermTypedNode)->getBasicType() == EbtStruct) {
            bool fieldFound = false;
            const TTypeList* fields = (yyvsp[(1) - (3)].interm.intermTypedNode)->getType().getStruct();
            if (fields == 0) {
                context->error((yyvsp[(2) - (3)].lex).line, "structure has no fields", "Internal Error");
                context->recover();
                (yyval.interm.intermTypedNode) = (yyvsp[(1) - (3)].interm.intermTypedNode);
            } else {
                unsigned int i;
                for (i = 0; i < fields->size(); ++i) {
                    if ((*fields)[i].type->getFieldName() == *(yyvsp[(3) - (3)].lex).string) {
                        fieldFound = true;
                        break;
                    }
                }
                if (fieldFound) {
                    if ((yyvsp[(1) - (3)].interm.intermTypedNode)->getType().getQualifier() == EvqConst) {
                        (yyval.interm.intermTypedNode) = context->addConstStruct(*(yyvsp[(3) - (3)].lex).string, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(2) - (3)].lex).line);
                        if ((yyval.interm.intermTypedNode) == 0) {
                            context->recover();
                            (yyval.interm.intermTypedNode) = (yyvsp[(1) - (3)].interm.intermTypedNode);
                        }
                        else {
                            (yyval.interm.intermTypedNode)->setType(*(*fields)[i].type);
                            // change the qualifier of the return type, not of the structure field
                            // as the structure definition is shared between various structures.
                            (yyval.interm.intermTypedNode)->getTypePointer()->setQualifier(EvqConst);
                        }
                    } else {
                        ConstantUnion *unionArray = new ConstantUnion[1];
                        unionArray->setIConst(i);
                        TIntermTyped* index = context->intermediate.addConstantUnion(unionArray, *(*fields)[i].type, (yyvsp[(3) - (3)].lex).line);
                        (yyval.interm.intermTypedNode) = context->intermediate.addIndex(EOpIndexDirectStruct, (yyvsp[(1) - (3)].interm.intermTypedNode), index, (yyvsp[(2) - (3)].lex).line);
                        (yyval.interm.intermTypedNode)->setType(*(*fields)[i].type);
                    }
                } else {
                    context->error((yyvsp[(2) - (3)].lex).line, " no such field in structure", (yyvsp[(3) - (3)].lex).string->c_str());
                    context->recover();
                    (yyval.interm.intermTypedNode) = (yyvsp[(1) - (3)].interm.intermTypedNode);
                }
            }
        } else {
            context->error((yyvsp[(2) - (3)].lex).line, " field selection requires structure, vector, or matrix on left hand side", (yyvsp[(3) - (3)].lex).string->c_str());
            context->recover();
            (yyval.interm.intermTypedNode) = (yyvsp[(1) - (3)].interm.intermTypedNode);
        }
        // don't delete $3.string, it's from the pool
    ;}
    break;

  case 12:

    {
        if (context->lValueErrorCheck((yyvsp[(2) - (2)].lex).line, "++", (yyvsp[(1) - (2)].interm.intermTypedNode)))
            context->recover();
        (yyval.interm.intermTypedNode) = context->intermediate.addUnaryMath(EOpPostIncrement, (yyvsp[(1) - (2)].interm.intermTypedNode), (yyvsp[(2) - (2)].lex).line, context->symbolTable);
        if ((yyval.interm.intermTypedNode) == 0) {
            context->unaryOpError((yyvsp[(2) - (2)].lex).line, "++", (yyvsp[(1) - (2)].interm.intermTypedNode)->getCompleteString());
            context->recover();
            (yyval.interm.intermTypedNode) = (yyvsp[(1) - (2)].interm.intermTypedNode);
        }
    ;}
    break;

  case 13:

    {
        if (context->lValueErrorCheck((yyvsp[(2) - (2)].lex).line, "--", (yyvsp[(1) - (2)].interm.intermTypedNode)))
            context->recover();
        (yyval.interm.intermTypedNode) = context->intermediate.addUnaryMath(EOpPostDecrement, (yyvsp[(1) - (2)].interm.intermTypedNode), (yyvsp[(2) - (2)].lex).line, context->symbolTable);
        if ((yyval.interm.intermTypedNode) == 0) {
            context->unaryOpError((yyvsp[(2) - (2)].lex).line, "--", (yyvsp[(1) - (2)].interm.intermTypedNode)->getCompleteString());
            context->recover();
            (yyval.interm.intermTypedNode) = (yyvsp[(1) - (2)].interm.intermTypedNode);
        }
    ;}
    break;

  case 14:

    {
        if (context->integerErrorCheck((yyvsp[(1) - (1)].interm.intermTypedNode), "[]"))
            context->recover();
        (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode);
    ;}
    break;

  case 15:

    {
        TFunction* fnCall = (yyvsp[(1) - (1)].interm).function;
        TOperator op = fnCall->getBuiltInOp();

        if (op != EOpNull)
        {
            //
            // Then this should be a constructor.
            // Don't go through the symbol table for constructors.
            // Their parameters will be verified algorithmically.
            //
            TType type(EbtVoid, EbpUndefined);  // use this to get the type back
            if (context->constructorErrorCheck((yyvsp[(1) - (1)].interm).line, (yyvsp[(1) - (1)].interm).intermNode, *fnCall, op, &type)) {
                (yyval.interm.intermTypedNode) = 0;
            } else {
                //
                // It's a constructor, of type 'type'.
                //
                (yyval.interm.intermTypedNode) = context->addConstructor((yyvsp[(1) - (1)].interm).intermNode, &type, op, fnCall, (yyvsp[(1) - (1)].interm).line);
            }

            if ((yyval.interm.intermTypedNode) == 0) {
                context->recover();
                (yyval.interm.intermTypedNode) = context->intermediate.setAggregateOperator(0, op, (yyvsp[(1) - (1)].interm).line);
            }
            (yyval.interm.intermTypedNode)->setType(type);
        } else {
            //
            // Not a constructor.  Find it in the symbol table.
            //
            const TFunction* fnCandidate;
            bool builtIn;
            fnCandidate = context->findFunction((yyvsp[(1) - (1)].interm).line, fnCall, &builtIn);
            if (fnCandidate) {
                //
                // A declared function.
                //
                if (builtIn && !fnCandidate->getExtension().empty() &&
                    context->extensionErrorCheck((yyvsp[(1) - (1)].interm).line, fnCandidate->getExtension())) {
                    context->recover();
                }
                op = fnCandidate->getBuiltInOp();
                if (builtIn && op != EOpNull) {
                    //
                    // A function call mapped to a built-in operation.
                    //
                    if (fnCandidate->getParamCount() == 1) {
                        //
                        // Treat it like a built-in unary operator.
                        //
                        (yyval.interm.intermTypedNode) = context->intermediate.addUnaryMath(op, (yyvsp[(1) - (1)].interm).intermNode, 0, context->symbolTable);
                        if ((yyval.interm.intermTypedNode) == 0)  {
                            std::stringstream extraInfoStream;
                            extraInfoStream << "built in unary operator function.  Type: " << static_cast<TIntermTyped*>((yyvsp[(1) - (1)].interm).intermNode)->getCompleteString();
                            std::string extraInfo = extraInfoStream.str();
                            context->error((yyvsp[(1) - (1)].interm).intermNode->getLine(), " wrong operand type", "Internal Error", extraInfo.c_str());
                            YYERROR;
                        }
                    } else {
                        (yyval.interm.intermTypedNode) = context->intermediate.setAggregateOperator((yyvsp[(1) - (1)].interm).intermAggregate, op, (yyvsp[(1) - (1)].interm).line);
                    }
                } else {
                    // This is a real function call

                    (yyval.interm.intermTypedNode) = context->intermediate.setAggregateOperator((yyvsp[(1) - (1)].interm).intermAggregate, EOpFunctionCall, (yyvsp[(1) - (1)].interm).line);
                    (yyval.interm.intermTypedNode)->setType(fnCandidate->getReturnType());

                    // this is how we know whether the given function is a builtIn function or a user defined function
                    // if builtIn == false, it's a userDefined -> could be an overloaded builtIn function also
                    // if builtIn == true, it's definitely a builtIn function with EOpNull
                    if (!builtIn)
                        (yyval.interm.intermTypedNode)->getAsAggregate()->setUserDefined();
                    (yyval.interm.intermTypedNode)->getAsAggregate()->setName(fnCandidate->getMangledName());

                    TQualifier qual;
                    for (size_t i = 0; i < fnCandidate->getParamCount(); ++i) {
                        qual = fnCandidate->getParam(i).type->getQualifier();
                        if (qual == EvqOut || qual == EvqInOut) {
                            if (context->lValueErrorCheck((yyval.interm.intermTypedNode)->getLine(), "assign", (yyval.interm.intermTypedNode)->getAsAggregate()->getSequence()[i]->getAsTyped())) {
                                context->error((yyvsp[(1) - (1)].interm).intermNode->getLine(), "Constant value cannot be passed for 'out' or 'inout' parameters.", "Error");
                                context->recover();
                            }
                        }
                    }
                }
                (yyval.interm.intermTypedNode)->setType(fnCandidate->getReturnType());
            } else {
                // error message was put out by PaFindFunction()
                // Put on a dummy node for error recovery
                ConstantUnion *unionArray = new ConstantUnion[1];
                unionArray->setFConst(0.0f);
                (yyval.interm.intermTypedNode) = context->intermediate.addConstantUnion(unionArray, TType(EbtFloat, EbpUndefined, EvqConst), (yyvsp[(1) - (1)].interm).line);
                context->recover();
            }
        }
        delete fnCall;
    ;}
    break;

  case 16:

    {
        (yyval.interm) = (yyvsp[(1) - (1)].interm);
    ;}
    break;

  case 17:

    {
        context->error((yyvsp[(3) - (3)].interm).line, "methods are not supported", "");
        context->recover();
        (yyval.interm) = (yyvsp[(3) - (3)].interm);
    ;}
    break;

  case 18:

    {
        (yyval.interm) = (yyvsp[(1) - (2)].interm);
        (yyval.interm).line = (yyvsp[(2) - (2)].lex).line;
    ;}
    break;

  case 19:

    {
        (yyval.interm) = (yyvsp[(1) - (2)].interm);
        (yyval.interm).line = (yyvsp[(2) - (2)].lex).line;
    ;}
    break;

  case 20:

    {
        (yyval.interm).function = (yyvsp[(1) - (2)].interm.function);
        (yyval.interm).intermNode = 0;
    ;}
    break;

  case 21:

    {
        (yyval.interm).function = (yyvsp[(1) - (1)].interm.function);
        (yyval.interm).intermNode = 0;
    ;}
    break;

  case 22:

    {
        TParameter param = { 0, new TType((yyvsp[(2) - (2)].interm.intermTypedNode)->getType()) };
        (yyvsp[(1) - (2)].interm.function)->addParameter(param);
        (yyval.interm).function = (yyvsp[(1) - (2)].interm.function);
        (yyval.interm).intermNode = (yyvsp[(2) - (2)].interm.intermTypedNode);
    ;}
    break;

  case 23:

    {
        TParameter param = { 0, new TType((yyvsp[(3) - (3)].interm.intermTypedNode)->getType()) };
        (yyvsp[(1) - (3)].interm).function->addParameter(param);
        (yyval.interm).function = (yyvsp[(1) - (3)].interm).function;
        (yyval.interm).intermNode = context->intermediate.growAggregate((yyvsp[(1) - (3)].interm).intermNode, (yyvsp[(3) - (3)].interm.intermTypedNode), (yyvsp[(2) - (3)].lex).line);
    ;}
    break;

  case 24:

    {
        (yyval.interm.function) = (yyvsp[(1) - (2)].interm.function);
    ;}
    break;

  case 25:

    {
        //
        // Constructor
        //
        TOperator op = EOpNull;
        if ((yyvsp[(1) - (1)].interm.type).userDef) {
            op = EOpConstructStruct;
        } else {
            switch ((yyvsp[(1) - (1)].interm.type).type) {
            case EbtFloat:
                if ((yyvsp[(1) - (1)].interm.type).matrix) {
                    switch((yyvsp[(1) - (1)].interm.type).size) {
                    case 2:                                     op = EOpConstructMat2;  break;
                    case 3:                                     op = EOpConstructMat3;  break;
                    case 4:                                     op = EOpConstructMat4;  break;
                    }
                } else {
                    switch((yyvsp[(1) - (1)].interm.type).size) {
                    case 1:                                     op = EOpConstructFloat; break;
                    case 2:                                     op = EOpConstructVec2;  break;
                    case 3:                                     op = EOpConstructVec3;  break;
                    case 4:                                     op = EOpConstructVec4;  break;
                    }
                }
                break;
            case EbtInt:
                switch((yyvsp[(1) - (1)].interm.type).size) {
                case 1:                                         op = EOpConstructInt;   break;
                case 2:       FRAG_VERT_ONLY("ivec2", (yyvsp[(1) - (1)].interm.type).line); op = EOpConstructIVec2; break;
                case 3:       FRAG_VERT_ONLY("ivec3", (yyvsp[(1) - (1)].interm.type).line); op = EOpConstructIVec3; break;
                case 4:       FRAG_VERT_ONLY("ivec4", (yyvsp[(1) - (1)].interm.type).line); op = EOpConstructIVec4; break;
                }
                break;
            case EbtBool:
                switch((yyvsp[(1) - (1)].interm.type).size) {
                case 1:                                         op = EOpConstructBool;  break;
                case 2:       FRAG_VERT_ONLY("bvec2", (yyvsp[(1) - (1)].interm.type).line); op = EOpConstructBVec2; break;
                case 3:       FRAG_VERT_ONLY("bvec3", (yyvsp[(1) - (1)].interm.type).line); op = EOpConstructBVec3; break;
                case 4:       FRAG_VERT_ONLY("bvec4", (yyvsp[(1) - (1)].interm.type).line); op = EOpConstructBVec4; break;
                }
                break;
            default: break;
            }
            if (op == EOpNull) {
                context->error((yyvsp[(1) - (1)].interm.type).line, "cannot construct this type", getBasicString((yyvsp[(1) - (1)].interm.type).type));
                context->recover();
                (yyvsp[(1) - (1)].interm.type).type = EbtFloat;
                op = EOpConstructFloat;
            }
        }
        TString tempString;
        TType type((yyvsp[(1) - (1)].interm.type));
        TFunction *function = new TFunction(&tempString, type, op);
        (yyval.interm.function) = function;
    ;}
    break;

  case 26:

    {
        if (context->reservedErrorCheck((yyvsp[(1) - (1)].lex).line, *(yyvsp[(1) - (1)].lex).string))
            context->recover();
        TType type(EbtVoid, EbpUndefined);
        TFunction *function = new TFunction((yyvsp[(1) - (1)].lex).string, type);
        (yyval.interm.function) = function;
    ;}
    break;

  case 27:

    {
        if (context->reservedErrorCheck((yyvsp[(1) - (1)].lex).line, *(yyvsp[(1) - (1)].lex).string))
            context->recover();
        TType type(EbtVoid, EbpUndefined);
        TFunction *function = new TFunction((yyvsp[(1) - (1)].lex).string, type);
        (yyval.interm.function) = function;
    ;}
    break;

  case 28:

    {
        (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode);
    ;}
    break;

  case 29:

    {
        if (context->lValueErrorCheck((yyvsp[(1) - (2)].lex).line, "++", (yyvsp[(2) - (2)].interm.intermTypedNode)))
            context->recover();
        (yyval.interm.intermTypedNode) = context->intermediate.addUnaryMath(EOpPreIncrement, (yyvsp[(2) - (2)].interm.intermTypedNode), (yyvsp[(1) - (2)].lex).line, context->symbolTable);
        if ((yyval.interm.intermTypedNode) == 0) {
            context->unaryOpError((yyvsp[(1) - (2)].lex).line, "++", (yyvsp[(2) - (2)].interm.intermTypedNode)->getCompleteString());
            context->recover();
            (yyval.interm.intermTypedNode) = (yyvsp[(2) - (2)].interm.intermTypedNode);
        }
    ;}
    break;

  case 30:

    {
        if (context->lValueErrorCheck((yyvsp[(1) - (2)].lex).line, "--", (yyvsp[(2) - (2)].interm.intermTypedNode)))
            context->recover();
        (yyval.interm.intermTypedNode) = context->intermediate.addUnaryMath(EOpPreDecrement, (yyvsp[(2) - (2)].interm.intermTypedNode), (yyvsp[(1) - (2)].lex).line, context->symbolTable);
        if ((yyval.interm.intermTypedNode) == 0) {
            context->unaryOpError((yyvsp[(1) - (2)].lex).line, "--", (yyvsp[(2) - (2)].interm.intermTypedNode)->getCompleteString());
            context->recover();
            (yyval.interm.intermTypedNode) = (yyvsp[(2) - (2)].interm.intermTypedNode);
        }
    ;}
    break;

  case 31:

    {
        if ((yyvsp[(1) - (2)].interm).op != EOpNull) {
            (yyval.interm.intermTypedNode) = context->intermediate.addUnaryMath((yyvsp[(1) - (2)].interm).op, (yyvsp[(2) - (2)].interm.intermTypedNode), (yyvsp[(1) - (2)].interm).line, context->symbolTable);
            if ((yyval.interm.intermTypedNode) == 0) {
                const char* errorOp = "";
                switch((yyvsp[(1) - (2)].interm).op) {
                case EOpNegative:   errorOp = "-"; break;
                case EOpLogicalNot: errorOp = "!"; break;
                default: break;
                }
                context->unaryOpError((yyvsp[(1) - (2)].interm).line, errorOp, (yyvsp[(2) - (2)].interm.intermTypedNode)->getCompleteString());
                context->recover();
                (yyval.interm.intermTypedNode) = (yyvsp[(2) - (2)].interm.intermTypedNode);
            }
        } else
            (yyval.interm.intermTypedNode) = (yyvsp[(2) - (2)].interm.intermTypedNode);
    ;}
    break;

  case 32:

    { (yyval.interm).line = (yyvsp[(1) - (1)].lex).line; (yyval.interm).op = EOpNull; ;}
    break;

  case 33:

    { (yyval.interm).line = (yyvsp[(1) - (1)].lex).line; (yyval.interm).op = EOpNegative; ;}
    break;

  case 34:

    { (yyval.interm).line = (yyvsp[(1) - (1)].lex).line; (yyval.interm).op = EOpLogicalNot; ;}
    break;

  case 35:

    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); ;}
    break;

  case 36:

    {
        FRAG_VERT_ONLY("*", (yyvsp[(2) - (3)].lex).line);
        (yyval.interm.intermTypedNode) = context->intermediate.addBinaryMath(EOpMul, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode), (yyvsp[(2) - (3)].lex).line, context->symbolTable);
        if ((yyval.interm.intermTypedNode) == 0) {
            context->binaryOpError((yyvsp[(2) - (3)].lex).line, "*", (yyvsp[(1) - (3)].interm.intermTypedNode)->getCompleteString(), (yyvsp[(3) - (3)].interm.intermTypedNode)->getCompleteString());
            context->recover();
            (yyval.interm.intermTypedNode) = (yyvsp[(1) - (3)].interm.intermTypedNode);
        }
    ;}
    break;

  case 37:

    {
        FRAG_VERT_ONLY("/", (yyvsp[(2) - (3)].lex).line);
        (yyval.interm.intermTypedNode) = context->intermediate.addBinaryMath(EOpDiv, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode), (yyvsp[(2) - (3)].lex).line, context->symbolTable);
        if ((yyval.interm.intermTypedNode) == 0) {
            context->binaryOpError((yyvsp[(2) - (3)].lex).line, "/", (yyvsp[(1) - (3)].interm.intermTypedNode)->getCompleteString(), (yyvsp[(3) - (3)].interm.intermTypedNode)->getCompleteString());
            context->recover();
            (yyval.interm.intermTypedNode) = (yyvsp[(1) - (3)].interm.intermTypedNode);
        }
    ;}
    break;

  case 38:

    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); ;}
    break;

  case 39:

    {
        (yyval.interm.intermTypedNode) = context->intermediate.addBinaryMath(EOpAdd, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode), (yyvsp[(2) - (3)].lex).line, context->symbolTable);
        if ((yyval.interm.intermTypedNode) == 0) {
            context->binaryOpError((yyvsp[(2) - (3)].lex).line, "+", (yyvsp[(1) - (3)].interm.intermTypedNode)->getCompleteString(), (yyvsp[(3) - (3)].interm.intermTypedNode)->getCompleteString());
            context->recover();
            (yyval.interm.intermTypedNode) = (yyvsp[(1) - (3)].interm.intermTypedNode);
        }
    ;}
    break;

  case 40:

    {
        (yyval.interm.intermTypedNode) = context->intermediate.addBinaryMath(EOpSub, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode), (yyvsp[(2) - (3)].lex).line, context->symbolTable);
        if ((yyval.interm.intermTypedNode) == 0) {
            context->binaryOpError((yyvsp[(2) - (3)].lex).line, "-", (yyvsp[(1) - (3)].interm.intermTypedNode)->getCompleteString(), (yyvsp[(3) - (3)].interm.intermTypedNode)->getCompleteString());
            context->recover();
            (yyval.interm.intermTypedNode) = (yyvsp[(1) - (3)].interm.intermTypedNode);
        }
    ;}
    break;

  case 41:

    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); ;}
    break;

  case 42:

    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); ;}
    break;

  case 43:

    {
        (yyval.interm.intermTypedNode) = context->intermediate.addBinaryMath(EOpLessThan, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode), (yyvsp[(2) - (3)].lex).line, context->symbolTable);
        if ((yyval.interm.intermTypedNode) == 0) {
            context->binaryOpError((yyvsp[(2) - (3)].lex).line, "<", (yyvsp[(1) - (3)].interm.intermTypedNode)->getCompleteString(), (yyvsp[(3) - (3)].interm.intermTypedNode)->getCompleteString());
            context->recover();
            ConstantUnion *unionArray = new ConstantUnion[1];
            unionArray->setBConst(false);
            (yyval.interm.intermTypedNode) = context->intermediate.addConstantUnion(unionArray, TType(EbtBool, EbpUndefined, EvqConst), (yyvsp[(2) - (3)].lex).line);
        }
    ;}
    break;

  case 44:

    {
        (yyval.interm.intermTypedNode) = context->intermediate.addBinaryMath(EOpGreaterThan, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode), (yyvsp[(2) - (3)].lex).line, context->symbolTable);
        if ((yyval.interm.intermTypedNode) == 0) {
            context->binaryOpError((yyvsp[(2) - (3)].lex).line, ">", (yyvsp[(1) - (3)].interm.intermTypedNode)->getCompleteString(), (yyvsp[(3) - (3)].interm.intermTypedNode)->getCompleteString());
            context->recover();
            ConstantUnion *unionArray = new ConstantUnion[1];
            unionArray->setBConst(false);
            (yyval.interm.intermTypedNode) = context->intermediate.addConstantUnion(unionArray, TType(EbtBool, EbpUndefined, EvqConst), (yyvsp[(2) - (3)].lex).line);
        }
    ;}
    break;

  case 45:

    {
        (yyval.interm.intermTypedNode) = context->intermediate.addBinaryMath(EOpLessThanEqual, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode), (yyvsp[(2) - (3)].lex).line, context->symbolTable);
        if ((yyval.interm.intermTypedNode) == 0) {
            context->binaryOpError((yyvsp[(2) - (3)].lex).line, "<=", (yyvsp[(1) - (3)].interm.intermTypedNode)->getCompleteString(), (yyvsp[(3) - (3)].interm.intermTypedNode)->getCompleteString());
            context->recover();
            ConstantUnion *unionArray = new ConstantUnion[1];
            unionArray->setBConst(false);
            (yyval.interm.intermTypedNode) = context->intermediate.addConstantUnion(unionArray, TType(EbtBool, EbpUndefined, EvqConst), (yyvsp[(2) - (3)].lex).line);
        }
    ;}
    break;

  case 46:

    {
        (yyval.interm.intermTypedNode) = context->intermediate.addBinaryMath(EOpGreaterThanEqual, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode), (yyvsp[(2) - (3)].lex).line, context->symbolTable);
        if ((yyval.interm.intermTypedNode) == 0) {
            context->binaryOpError((yyvsp[(2) - (3)].lex).line, ">=", (yyvsp[(1) - (3)].interm.intermTypedNode)->getCompleteString(), (yyvsp[(3) - (3)].interm.intermTypedNode)->getCompleteString());
            context->recover();
            ConstantUnion *unionArray = new ConstantUnion[1];
            unionArray->setBConst(false);
            (yyval.interm.intermTypedNode) = context->intermediate.addConstantUnion(unionArray, TType(EbtBool, EbpUndefined, EvqConst), (yyvsp[(2) - (3)].lex).line);
        }
    ;}
    break;

  case 47:

    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); ;}
    break;

  case 48:

    {
        (yyval.interm.intermTypedNode) = context->intermediate.addBinaryMath(EOpEqual, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode), (yyvsp[(2) - (3)].lex).line, context->symbolTable);
        if ((yyval.interm.intermTypedNode) == 0) {
            context->binaryOpError((yyvsp[(2) - (3)].lex).line, "==", (yyvsp[(1) - (3)].interm.intermTypedNode)->getCompleteString(), (yyvsp[(3) - (3)].interm.intermTypedNode)->getCompleteString());
            context->recover();
            ConstantUnion *unionArray = new ConstantUnion[1];
            unionArray->setBConst(false);
            (yyval.interm.intermTypedNode) = context->intermediate.addConstantUnion(unionArray, TType(EbtBool, EbpUndefined, EvqConst), (yyvsp[(2) - (3)].lex).line);
        }
    ;}
    break;

  case 49:

    {
        (yyval.interm.intermTypedNode) = context->intermediate.addBinaryMath(EOpNotEqual, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode), (yyvsp[(2) - (3)].lex).line, context->symbolTable);
        if ((yyval.interm.intermTypedNode) == 0) {
            context->binaryOpError((yyvsp[(2) - (3)].lex).line, "!=", (yyvsp[(1) - (3)].interm.intermTypedNode)->getCompleteString(), (yyvsp[(3) - (3)].interm.intermTypedNode)->getCompleteString());
            context->recover();
            ConstantUnion *unionArray = new ConstantUnion[1];
            unionArray->setBConst(false);
            (yyval.interm.intermTypedNode) = context->intermediate.addConstantUnion(unionArray, TType(EbtBool, EbpUndefined, EvqConst), (yyvsp[(2) - (3)].lex).line);
        }
    ;}
    break;

  case 50:

    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); ;}
    break;

  case 51:

    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); ;}
    break;

  case 52:

    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); ;}
    break;

  case 53:

    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); ;}
    break;

  case 54:

    {
        (yyval.interm.intermTypedNode) = context->intermediate.addBinaryMath(EOpLogicalAnd, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode), (yyvsp[(2) - (3)].lex).line, context->symbolTable);
        if ((yyval.interm.intermTypedNode) == 0) {
            context->binaryOpError((yyvsp[(2) - (3)].lex).line, "&&", (yyvsp[(1) - (3)].interm.intermTypedNode)->getCompleteString(), (yyvsp[(3) - (3)].interm.intermTypedNode)->getCompleteString());
            context->recover();
            ConstantUnion *unionArray = new ConstantUnion[1];
            unionArray->setBConst(false);
            (yyval.interm.intermTypedNode) = context->intermediate.addConstantUnion(unionArray, TType(EbtBool, EbpUndefined, EvqConst), (yyvsp[(2) - (3)].lex).line);
        }
    ;}
    break;

  case 55:

    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); ;}
    break;

  case 56:

    {
        (yyval.interm.intermTypedNode) = context->intermediate.addBinaryMath(EOpLogicalXor, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode), (yyvsp[(2) - (3)].lex).line, context->symbolTable);
        if ((yyval.interm.intermTypedNode) == 0) {
            context->binaryOpError((yyvsp[(2) - (3)].lex).line, "^^", (yyvsp[(1) - (3)].interm.intermTypedNode)->getCompleteString(), (yyvsp[(3) - (3)].interm.intermTypedNode)->getCompleteString());
            context->recover();
            ConstantUnion *unionArray = new ConstantUnion[1];
            unionArray->setBConst(false);
            (yyval.interm.intermTypedNode) = context->intermediate.addConstantUnion(unionArray, TType(EbtBool, EbpUndefined, EvqConst), (yyvsp[(2) - (3)].lex).line);
        }
    ;}
    break;

  case 57:

    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); ;}
    break;

  case 58:

    {
        (yyval.interm.intermTypedNode) = context->intermediate.addBinaryMath(EOpLogicalOr, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode), (yyvsp[(2) - (3)].lex).line, context->symbolTable);
        if ((yyval.interm.intermTypedNode) == 0) {
            context->binaryOpError((yyvsp[(2) - (3)].lex).line, "||", (yyvsp[(1) - (3)].interm.intermTypedNode)->getCompleteString(), (yyvsp[(3) - (3)].interm.intermTypedNode)->getCompleteString());
            context->recover();
            ConstantUnion *unionArray = new ConstantUnion[1];
            unionArray->setBConst(false);
            (yyval.interm.intermTypedNode) = context->intermediate.addConstantUnion(unionArray, TType(EbtBool, EbpUndefined, EvqConst), (yyvsp[(2) - (3)].lex).line);
        }
    ;}
    break;

  case 59:

    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); ;}
    break;

  case 60:

    {
       if (context->boolErrorCheck((yyvsp[(2) - (5)].lex).line, (yyvsp[(1) - (5)].interm.intermTypedNode)))
            context->recover();

        (yyval.interm.intermTypedNode) = context->intermediate.addSelection((yyvsp[(1) - (5)].interm.intermTypedNode), (yyvsp[(3) - (5)].interm.intermTypedNode), (yyvsp[(5) - (5)].interm.intermTypedNode), (yyvsp[(2) - (5)].lex).line);
        if ((yyvsp[(3) - (5)].interm.intermTypedNode)->getType() != (yyvsp[(5) - (5)].interm.intermTypedNode)->getType())
            (yyval.interm.intermTypedNode) = 0;

        if ((yyval.interm.intermTypedNode) == 0) {
            context->binaryOpError((yyvsp[(2) - (5)].lex).line, ":", (yyvsp[(3) - (5)].interm.intermTypedNode)->getCompleteString(), (yyvsp[(5) - (5)].interm.intermTypedNode)->getCompleteString());
            context->recover();
            (yyval.interm.intermTypedNode) = (yyvsp[(5) - (5)].interm.intermTypedNode);
        }
    ;}
    break;

  case 61:

    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); ;}
    break;

  case 62:

    {
        if (context->lValueErrorCheck((yyvsp[(2) - (3)].interm).line, "assign", (yyvsp[(1) - (3)].interm.intermTypedNode)))
            context->recover();
        (yyval.interm.intermTypedNode) = context->intermediate.addAssign((yyvsp[(2) - (3)].interm).op, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode), (yyvsp[(2) - (3)].interm).line);
        if ((yyval.interm.intermTypedNode) == 0) {
            context->assignError((yyvsp[(2) - (3)].interm).line, "assign", (yyvsp[(1) - (3)].interm.intermTypedNode)->getCompleteString(), (yyvsp[(3) - (3)].interm.intermTypedNode)->getCompleteString());
            context->recover();
            (yyval.interm.intermTypedNode) = (yyvsp[(1) - (3)].interm.intermTypedNode);
        }
    ;}
    break;

  case 63:

    {                                    (yyval.interm).line = (yyvsp[(1) - (1)].lex).line; (yyval.interm).op = EOpAssign; ;}
    break;

  case 64:

    { FRAG_VERT_ONLY("*=", (yyvsp[(1) - (1)].lex).line);     (yyval.interm).line = (yyvsp[(1) - (1)].lex).line; (yyval.interm).op = EOpMulAssign; ;}
    break;

  case 65:

    { FRAG_VERT_ONLY("/=", (yyvsp[(1) - (1)].lex).line);     (yyval.interm).line = (yyvsp[(1) - (1)].lex).line; (yyval.interm).op = EOpDivAssign; ;}
    break;

  case 66:

    {                                    (yyval.interm).line = (yyvsp[(1) - (1)].lex).line; (yyval.interm).op = EOpAddAssign; ;}
    break;

  case 67:

    {                                    (yyval.interm).line = (yyvsp[(1) - (1)].lex).line; (yyval.interm).op = EOpSubAssign; ;}
    break;

  case 68:

    {
        (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode);
    ;}
    break;

  case 69:

    {
        (yyval.interm.intermTypedNode) = context->intermediate.addComma((yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode), (yyvsp[(2) - (3)].lex).line);
        if ((yyval.interm.intermTypedNode) == 0) {
            context->binaryOpError((yyvsp[(2) - (3)].lex).line, ",", (yyvsp[(1) - (3)].interm.intermTypedNode)->getCompleteString(), (yyvsp[(3) - (3)].interm.intermTypedNode)->getCompleteString());
            context->recover();
            (yyval.interm.intermTypedNode) = (yyvsp[(3) - (3)].interm.intermTypedNode);
        }
    ;}
    break;

  case 70:

    {
        if (context->constErrorCheck((yyvsp[(1) - (1)].interm.intermTypedNode)))
            context->recover();
        (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode);
    ;}
    break;

  case 71:

    {
        TFunction &function = *((yyvsp[(1) - (2)].interm).function);
        
        TIntermAggregate *prototype = new TIntermAggregate;
        prototype->setType(function.getReturnType());
        prototype->setName(function.getName());
        
        for (size_t i = 0; i < function.getParamCount(); i++)
        {
            const TParameter &param = function.getParam(i);
            if (param.name != 0)
            {
                TVariable *variable = new TVariable(param.name, *param.type);
                
                prototype = context->intermediate.growAggregate(prototype, context->intermediate.addSymbol(variable->getUniqueId(), variable->getName(), variable->getType(), (yyvsp[(1) - (2)].interm).line), (yyvsp[(1) - (2)].interm).line);
            }
            else
            {
                prototype = context->intermediate.growAggregate(prototype, context->intermediate.addSymbol(0, "", *param.type, (yyvsp[(1) - (2)].interm).line), (yyvsp[(1) - (2)].interm).line);
            }
        }
        
        prototype->setOp(EOpPrototype);
        (yyval.interm.intermNode) = prototype;

        context->symbolTable.pop();
    ;}
    break;

  case 72:

    {
        if ((yyvsp[(1) - (2)].interm).intermAggregate)
            (yyvsp[(1) - (2)].interm).intermAggregate->setOp(EOpDeclaration);
        (yyval.interm.intermNode) = (yyvsp[(1) - (2)].interm).intermAggregate;
    ;}
    break;

  case 73:

    {
        if (!context->symbolTable.setDefaultPrecision( (yyvsp[(3) - (4)].interm.type), (yyvsp[(2) - (4)].interm.precision) )) {
            context->error((yyvsp[(1) - (4)].lex).line, "illegal type argument for default precision qualifier", getBasicString((yyvsp[(3) - (4)].interm.type).type));
            context->recover();
        }
        (yyval.interm.intermNode) = 0;
    ;}
    break;

  case 74:

    {
        //
        // Multiple declarations of the same function are allowed.
        //
        // If this is a definition, the definition production code will check for redefinitions
        // (we don't know at this point if it's a definition or not).
        //
        // Redeclarations are allowed.  But, return types and parameter qualifiers must match.
        //
        TFunction* prevDec = static_cast<TFunction*>(context->symbolTable.find((yyvsp[(1) - (2)].interm.function)->getMangledName()));
        if (prevDec) {
            if (prevDec->getReturnType() != (yyvsp[(1) - (2)].interm.function)->getReturnType()) {
                context->error((yyvsp[(2) - (2)].lex).line, "overloaded functions must have the same return type", (yyvsp[(1) - (2)].interm.function)->getReturnType().getBasicString());
                context->recover();
            }
            for (size_t i = 0; i < prevDec->getParamCount(); ++i) {
                if (prevDec->getParam(i).type->getQualifier() != (yyvsp[(1) - (2)].interm.function)->getParam(i).type->getQualifier()) {
                    context->error((yyvsp[(2) - (2)].lex).line, "overloaded functions must have the same parameter qualifiers", (yyvsp[(1) - (2)].interm.function)->getParam(i).type->getQualifierString());
                    context->recover();
                }
            }
        }

        //
        // If this is a redeclaration, it could also be a definition,
        // in which case, we want to use the variable names from this one, and not the one that's
        // being redeclared.  So, pass back up this declaration, not the one in the symbol table.
        //
        (yyval.interm).function = (yyvsp[(1) - (2)].interm.function);
        (yyval.interm).line = (yyvsp[(2) - (2)].lex).line;

        // We're at the inner scope level of the function's arguments and body statement.
        // Add the function prototype to the surrounding scope instead.
        context->symbolTable.getOuterLevel()->insert(*(yyval.interm).function);
    ;}
    break;

  case 75:

    {
        (yyval.interm.function) = (yyvsp[(1) - (1)].interm.function);
    ;}
    break;

  case 76:

    {
        (yyval.interm.function) = (yyvsp[(1) - (1)].interm.function);
    ;}
    break;

  case 77:

    {
        // Add the parameter
        (yyval.interm.function) = (yyvsp[(1) - (2)].interm.function);
        if ((yyvsp[(2) - (2)].interm).param.type->getBasicType() != EbtVoid)
            (yyvsp[(1) - (2)].interm.function)->addParameter((yyvsp[(2) - (2)].interm).param);
        else
            delete (yyvsp[(2) - (2)].interm).param.type;
    ;}
    break;

  case 78:

    {
        //
        // Only first parameter of one-parameter functions can be void
        // The check for named parameters not being void is done in parameter_declarator
        //
        if ((yyvsp[(3) - (3)].interm).param.type->getBasicType() == EbtVoid) {
            //
            // This parameter > first is void
            //
            context->error((yyvsp[(2) - (3)].lex).line, "cannot be an argument type except for '(void)'", "void");
            context->recover();
            delete (yyvsp[(3) - (3)].interm).param.type;
        } else {
            // Add the parameter
            (yyval.interm.function) = (yyvsp[(1) - (3)].interm.function);
            (yyvsp[(1) - (3)].interm.function)->addParameter((yyvsp[(3) - (3)].interm).param);
        }
    ;}
    break;

  case 79:

    {
        if ((yyvsp[(1) - (3)].interm.type).qualifier != EvqGlobal && (yyvsp[(1) - (3)].interm.type).qualifier != EvqTemporary) {
            context->error((yyvsp[(2) - (3)].lex).line, "no qualifiers allowed for function return", getQualifierString((yyvsp[(1) - (3)].interm.type).qualifier));
            context->recover();
        }
        // make sure a sampler is not involved as well...
        if (context->structQualifierErrorCheck((yyvsp[(2) - (3)].lex).line, (yyvsp[(1) - (3)].interm.type)))
            context->recover();

        // Add the function as a prototype after parsing it (we do not support recursion)
        TFunction *function;
        TType type((yyvsp[(1) - (3)].interm.type));
        function = new TFunction((yyvsp[(2) - (3)].lex).string, type);
        (yyval.interm.function) = function;
        
        context->symbolTable.push();
    ;}
    break;

  case 80:

    {
        if ((yyvsp[(1) - (2)].interm.type).type == EbtVoid) {
            context->error((yyvsp[(2) - (2)].lex).line, "illegal use of type 'void'", (yyvsp[(2) - (2)].lex).string->c_str());
            context->recover();
        }
        if (context->reservedErrorCheck((yyvsp[(2) - (2)].lex).line, *(yyvsp[(2) - (2)].lex).string))
            context->recover();
        TParameter param = {(yyvsp[(2) - (2)].lex).string, new TType((yyvsp[(1) - (2)].interm.type))};
        (yyval.interm).line = (yyvsp[(2) - (2)].lex).line;
        (yyval.interm).param = param;
    ;}
    break;

  case 81:

    {
        // Check that we can make an array out of this type
        if (context->arrayTypeErrorCheck((yyvsp[(3) - (5)].lex).line, (yyvsp[(1) - (5)].interm.type)))
            context->recover();

        if (context->reservedErrorCheck((yyvsp[(2) - (5)].lex).line, *(yyvsp[(2) - (5)].lex).string))
            context->recover();

        int size;
        if (context->arraySizeErrorCheck((yyvsp[(3) - (5)].lex).line, (yyvsp[(4) - (5)].interm.intermTypedNode), size))
            context->recover();
        (yyvsp[(1) - (5)].interm.type).setArray(true, size);

        TType* type = new TType((yyvsp[(1) - (5)].interm.type));
        TParameter param = { (yyvsp[(2) - (5)].lex).string, type };
        (yyval.interm).line = (yyvsp[(2) - (5)].lex).line;
        (yyval.interm).param = param;
    ;}
    break;

  case 82:

    {
        (yyval.interm) = (yyvsp[(3) - (3)].interm);
        if (context->paramErrorCheck((yyvsp[(3) - (3)].interm).line, (yyvsp[(1) - (3)].interm.type).qualifier, (yyvsp[(2) - (3)].interm.qualifier), (yyval.interm).param.type))
            context->recover();
    ;}
    break;

  case 83:

    {
        (yyval.interm) = (yyvsp[(2) - (2)].interm);
        if (context->parameterSamplerErrorCheck((yyvsp[(2) - (2)].interm).line, (yyvsp[(1) - (2)].interm.qualifier), *(yyvsp[(2) - (2)].interm).param.type))
            context->recover();
        if (context->paramErrorCheck((yyvsp[(2) - (2)].interm).line, EvqTemporary, (yyvsp[(1) - (2)].interm.qualifier), (yyval.interm).param.type))
            context->recover();
    ;}
    break;

  case 84:

    {
        (yyval.interm) = (yyvsp[(3) - (3)].interm);
        if (context->paramErrorCheck((yyvsp[(3) - (3)].interm).line, (yyvsp[(1) - (3)].interm.type).qualifier, (yyvsp[(2) - (3)].interm.qualifier), (yyval.interm).param.type))
            context->recover();
    ;}
    break;

  case 85:

    {
        (yyval.interm) = (yyvsp[(2) - (2)].interm);
        if (context->parameterSamplerErrorCheck((yyvsp[(2) - (2)].interm).line, (yyvsp[(1) - (2)].interm.qualifier), *(yyvsp[(2) - (2)].interm).param.type))
            context->recover();
        if (context->paramErrorCheck((yyvsp[(2) - (2)].interm).line, EvqTemporary, (yyvsp[(1) - (2)].interm.qualifier), (yyval.interm).param.type))
            context->recover();
    ;}
    break;

  case 86:

    {
        (yyval.interm.qualifier) = EvqIn;
    ;}
    break;

  case 87:

    {
        (yyval.interm.qualifier) = EvqIn;
    ;}
    break;

  case 88:

    {
        (yyval.interm.qualifier) = EvqOut;
    ;}
    break;

  case 89:

    {
        (yyval.interm.qualifier) = EvqInOut;
    ;}
    break;

  case 90:

    {
        TParameter param = { 0, new TType((yyvsp[(1) - (1)].interm.type)) };
        (yyval.interm).param = param;
    ;}
    break;

  case 91:

    {
        (yyval.interm) = (yyvsp[(1) - (1)].interm);
    ;}
    break;

  case 92:

    {
        if ((yyvsp[(1) - (3)].interm).type.type == EbtInvariant && !(yyvsp[(3) - (3)].lex).symbol)
        {
            context->error((yyvsp[(3) - (3)].lex).line, "undeclared identifier declared as invariant", (yyvsp[(3) - (3)].lex).string->c_str());
            context->recover();
        }

        TIntermSymbol* symbol = context->intermediate.addSymbol(0, *(yyvsp[(3) - (3)].lex).string, TType((yyvsp[(1) - (3)].interm).type), (yyvsp[(3) - (3)].lex).line);
        (yyval.interm).intermAggregate = context->intermediate.growAggregate((yyvsp[(1) - (3)].interm).intermNode, symbol, (yyvsp[(3) - (3)].lex).line);
        
        if (context->structQualifierErrorCheck((yyvsp[(3) - (3)].lex).line, (yyval.interm).type))
            context->recover();

        if (context->nonInitConstErrorCheck((yyvsp[(3) - (3)].lex).line, *(yyvsp[(3) - (3)].lex).string, (yyval.interm).type, false))
            context->recover();

        TVariable* variable = 0;
        if (context->nonInitErrorCheck((yyvsp[(3) - (3)].lex).line, *(yyvsp[(3) - (3)].lex).string, (yyval.interm).type, variable))
            context->recover();
        if (symbol && variable)
            symbol->setId(variable->getUniqueId());
    ;}
    break;

  case 93:

    {
        if (context->structQualifierErrorCheck((yyvsp[(3) - (5)].lex).line, (yyvsp[(1) - (5)].interm).type))
            context->recover();

        if (context->nonInitConstErrorCheck((yyvsp[(3) - (5)].lex).line, *(yyvsp[(3) - (5)].lex).string, (yyvsp[(1) - (5)].interm).type, true))
            context->recover();

        (yyval.interm) = (yyvsp[(1) - (5)].interm);

        if (context->arrayTypeErrorCheck((yyvsp[(4) - (5)].lex).line, (yyvsp[(1) - (5)].interm).type) || context->arrayQualifierErrorCheck((yyvsp[(4) - (5)].lex).line, (yyvsp[(1) - (5)].interm).type))
            context->recover();
        else {
            (yyvsp[(1) - (5)].interm).type.setArray(true);
            TVariable* variable;
            if (context->arrayErrorCheck((yyvsp[(4) - (5)].lex).line, *(yyvsp[(3) - (5)].lex).string, (yyvsp[(1) - (5)].interm).type, variable))
                context->recover();
        }
    ;}
    break;

  case 94:

    {
        if (context->structQualifierErrorCheck((yyvsp[(3) - (6)].lex).line, (yyvsp[(1) - (6)].interm).type))
            context->recover();

        if (context->nonInitConstErrorCheck((yyvsp[(3) - (6)].lex).line, *(yyvsp[(3) - (6)].lex).string, (yyvsp[(1) - (6)].interm).type, true))
            context->recover();

        (yyval.interm) = (yyvsp[(1) - (6)].interm);

        if (context->arrayTypeErrorCheck((yyvsp[(4) - (6)].lex).line, (yyvsp[(1) - (6)].interm).type) || context->arrayQualifierErrorCheck((yyvsp[(4) - (6)].lex).line, (yyvsp[(1) - (6)].interm).type))
            context->recover();
        else {
            int size;
            if (context->arraySizeErrorCheck((yyvsp[(4) - (6)].lex).line, (yyvsp[(5) - (6)].interm.intermTypedNode), size))
                context->recover();
            (yyvsp[(1) - (6)].interm).type.setArray(true, size);
            TVariable* variable = 0;
            if (context->arrayErrorCheck((yyvsp[(4) - (6)].lex).line, *(yyvsp[(3) - (6)].lex).string, (yyvsp[(1) - (6)].interm).type, variable))
                context->recover();
            TType type = TType((yyvsp[(1) - (6)].interm).type);
            type.setArraySize(size);
            (yyval.interm).intermAggregate = context->intermediate.growAggregate((yyvsp[(1) - (6)].interm).intermNode, context->intermediate.addSymbol(variable ? variable->getUniqueId() : 0, *(yyvsp[(3) - (6)].lex).string, type, (yyvsp[(3) - (6)].lex).line), (yyvsp[(3) - (6)].lex).line);
        }
    ;}
    break;

  case 95:

    {
        if (context->structQualifierErrorCheck((yyvsp[(3) - (5)].lex).line, (yyvsp[(1) - (5)].interm).type))
            context->recover();

        (yyval.interm) = (yyvsp[(1) - (5)].interm);

        TIntermNode* intermNode;
        if (!context->executeInitializer((yyvsp[(3) - (5)].lex).line, *(yyvsp[(3) - (5)].lex).string, (yyvsp[(1) - (5)].interm).type, (yyvsp[(5) - (5)].interm.intermTypedNode), intermNode)) {
            //
            // build the intermediate representation
            //
            if (intermNode)
        (yyval.interm).intermAggregate = context->intermediate.growAggregate((yyvsp[(1) - (5)].interm).intermNode, intermNode, (yyvsp[(4) - (5)].lex).line);
            else
                (yyval.interm).intermAggregate = (yyvsp[(1) - (5)].interm).intermAggregate;
        } else {
            context->recover();
            (yyval.interm).intermAggregate = 0;
        }
    ;}
    break;

  case 96:

    {
        (yyval.interm).type = (yyvsp[(1) - (1)].interm.type);
        (yyval.interm).intermAggregate = context->intermediate.makeAggregate(context->intermediate.addSymbol(0, "", TType((yyvsp[(1) - (1)].interm.type)), (yyvsp[(1) - (1)].interm.type).line), (yyvsp[(1) - (1)].interm.type).line);
    ;}
    break;

  case 97:

    {
        TIntermSymbol* symbol = context->intermediate.addSymbol(0, *(yyvsp[(2) - (2)].lex).string, TType((yyvsp[(1) - (2)].interm.type)), (yyvsp[(2) - (2)].lex).line);
        (yyval.interm).intermAggregate = context->intermediate.makeAggregate(symbol, (yyvsp[(2) - (2)].lex).line);
        
        if (context->structQualifierErrorCheck((yyvsp[(2) - (2)].lex).line, (yyval.interm).type))
            context->recover();

        if (context->nonInitConstErrorCheck((yyvsp[(2) - (2)].lex).line, *(yyvsp[(2) - (2)].lex).string, (yyval.interm).type, false))
            context->recover();
            
            (yyval.interm).type = (yyvsp[(1) - (2)].interm.type);

        TVariable* variable = 0;
        if (context->nonInitErrorCheck((yyvsp[(2) - (2)].lex).line, *(yyvsp[(2) - (2)].lex).string, (yyval.interm).type, variable))
            context->recover();
        if (variable && symbol)
            symbol->setId(variable->getUniqueId());
    ;}
    break;

  case 98:

    {
        context->error((yyvsp[(2) - (4)].lex).line, "unsized array declarations not supported", (yyvsp[(2) - (4)].lex).string->c_str());
        context->recover();

        TIntermSymbol* symbol = context->intermediate.addSymbol(0, *(yyvsp[(2) - (4)].lex).string, TType((yyvsp[(1) - (4)].interm.type)), (yyvsp[(2) - (4)].lex).line);
        (yyval.interm).intermAggregate = context->intermediate.makeAggregate(symbol, (yyvsp[(2) - (4)].lex).line);
        (yyval.interm).type = (yyvsp[(1) - (4)].interm.type);
    ;}
    break;

  case 99:

    {
        TType type = TType((yyvsp[(1) - (5)].interm.type));
        int size;
        if (context->arraySizeErrorCheck((yyvsp[(2) - (5)].lex).line, (yyvsp[(4) - (5)].interm.intermTypedNode), size))
            context->recover();
        type.setArraySize(size);
        TIntermSymbol* symbol = context->intermediate.addSymbol(0, *(yyvsp[(2) - (5)].lex).string, type, (yyvsp[(2) - (5)].lex).line);
        (yyval.interm).intermAggregate = context->intermediate.makeAggregate(symbol, (yyvsp[(2) - (5)].lex).line);
        
        if (context->structQualifierErrorCheck((yyvsp[(2) - (5)].lex).line, (yyvsp[(1) - (5)].interm.type)))
            context->recover();

        if (context->nonInitConstErrorCheck((yyvsp[(2) - (5)].lex).line, *(yyvsp[(2) - (5)].lex).string, (yyvsp[(1) - (5)].interm.type), true))
            context->recover();

        (yyval.interm).type = (yyvsp[(1) - (5)].interm.type);

        if (context->arrayTypeErrorCheck((yyvsp[(3) - (5)].lex).line, (yyvsp[(1) - (5)].interm.type)) || context->arrayQualifierErrorCheck((yyvsp[(3) - (5)].lex).line, (yyvsp[(1) - (5)].interm.type)))
            context->recover();
        else {
            int size;
            if (context->arraySizeErrorCheck((yyvsp[(3) - (5)].lex).line, (yyvsp[(4) - (5)].interm.intermTypedNode), size))
                context->recover();

            (yyvsp[(1) - (5)].interm.type).setArray(true, size);
            TVariable* variable = 0;
            if (context->arrayErrorCheck((yyvsp[(3) - (5)].lex).line, *(yyvsp[(2) - (5)].lex).string, (yyvsp[(1) - (5)].interm.type), variable))
                context->recover();
            if (variable && symbol)
                symbol->setId(variable->getUniqueId());
        }
    ;}
    break;

  case 100:

    {
        if (context->structQualifierErrorCheck((yyvsp[(2) - (4)].lex).line, (yyvsp[(1) - (4)].interm.type)))
            context->recover();

        (yyval.interm).type = (yyvsp[(1) - (4)].interm.type);

        TIntermNode* intermNode;
        if (!context->executeInitializer((yyvsp[(2) - (4)].lex).line, *(yyvsp[(2) - (4)].lex).string, (yyvsp[(1) - (4)].interm.type), (yyvsp[(4) - (4)].interm.intermTypedNode), intermNode)) {
        //
        // Build intermediate representation
        //
            if(intermNode)
                (yyval.interm).intermAggregate = context->intermediate.makeAggregate(intermNode, (yyvsp[(3) - (4)].lex).line);
            else
                (yyval.interm).intermAggregate = 0;
        } else {
            context->recover();
            (yyval.interm).intermAggregate = 0;
        }
    ;}
    break;

  case 101:

    {
        VERTEX_ONLY("invariant declaration", (yyvsp[(1) - (2)].lex).line);
        if (context->globalErrorCheck((yyvsp[(1) - (2)].lex).line, context->symbolTable.atGlobalLevel(), "invariant varying"))
            context->recover();
        (yyval.interm).type.setBasic(EbtInvariant, EvqInvariantVaryingOut, (yyvsp[(2) - (2)].lex).line);
        if (!(yyvsp[(2) - (2)].lex).symbol)
        {
            context->error((yyvsp[(2) - (2)].lex).line, "undeclared identifier declared as invariant", (yyvsp[(2) - (2)].lex).string->c_str());
            context->recover();
            
            (yyval.interm).intermAggregate = 0;
        }
        else
        {
            TIntermSymbol *symbol = context->intermediate.addSymbol(0, *(yyvsp[(2) - (2)].lex).string, TType((yyval.interm).type), (yyvsp[(2) - (2)].lex).line);
            (yyval.interm).intermAggregate = context->intermediate.makeAggregate(symbol, (yyvsp[(2) - (2)].lex).line);
        }
    ;}
    break;

  case 102:

    {
        (yyval.interm.type) = (yyvsp[(1) - (1)].interm.type);

        if ((yyvsp[(1) - (1)].interm.type).array) {
            context->error((yyvsp[(1) - (1)].interm.type).line, "not supported", "first-class array");
            context->recover();
            (yyvsp[(1) - (1)].interm.type).setArray(false);
        }
    ;}
    break;

  case 103:

    {
        if ((yyvsp[(2) - (2)].interm.type).array) {
            context->error((yyvsp[(2) - (2)].interm.type).line, "not supported", "first-class array");
            context->recover();
            (yyvsp[(2) - (2)].interm.type).setArray(false);
        }

        if ((yyvsp[(1) - (2)].interm.type).qualifier == EvqAttribute &&
            ((yyvsp[(2) - (2)].interm.type).type == EbtBool || (yyvsp[(2) - (2)].interm.type).type == EbtInt)) {
            context->error((yyvsp[(2) - (2)].interm.type).line, "cannot be bool or int", getQualifierString((yyvsp[(1) - (2)].interm.type).qualifier));
            context->recover();
        }
        if (((yyvsp[(1) - (2)].interm.type).qualifier == EvqVaryingIn || (yyvsp[(1) - (2)].interm.type).qualifier == EvqVaryingOut) &&
            ((yyvsp[(2) - (2)].interm.type).type == EbtBool || (yyvsp[(2) - (2)].interm.type).type == EbtInt)) {
            context->error((yyvsp[(2) - (2)].interm.type).line, "cannot be bool or int", getQualifierString((yyvsp[(1) - (2)].interm.type).qualifier));
            context->recover();
        }
        (yyval.interm.type) = (yyvsp[(2) - (2)].interm.type);
        (yyval.interm.type).qualifier = (yyvsp[(1) - (2)].interm.type).qualifier;
    ;}
    break;

  case 104:

    {
        (yyval.interm.type).setBasic(EbtVoid, EvqConst, (yyvsp[(1) - (1)].lex).line);
    ;}
    break;

  case 105:

    {
        VERTEX_ONLY("attribute", (yyvsp[(1) - (1)].lex).line);
        if (context->globalErrorCheck((yyvsp[(1) - (1)].lex).line, context->symbolTable.atGlobalLevel(), "attribute"))
            context->recover();
        (yyval.interm.type).setBasic(EbtVoid, EvqAttribute, (yyvsp[(1) - (1)].lex).line);
    ;}
    break;

  case 106:

    {
        if (context->globalErrorCheck((yyvsp[(1) - (1)].lex).line, context->symbolTable.atGlobalLevel(), "varying"))
            context->recover();
        if (context->shaderType == SH_VERTEX_SHADER)
            (yyval.interm.type).setBasic(EbtVoid, EvqVaryingOut, (yyvsp[(1) - (1)].lex).line);
        else
            (yyval.interm.type).setBasic(EbtVoid, EvqVaryingIn, (yyvsp[(1) - (1)].lex).line);
    ;}
    break;

  case 107:

    {
        if (context->globalErrorCheck((yyvsp[(1) - (2)].lex).line, context->symbolTable.atGlobalLevel(), "invariant varying"))
            context->recover();
        if (context->shaderType == SH_VERTEX_SHADER)
            (yyval.interm.type).setBasic(EbtVoid, EvqInvariantVaryingOut, (yyvsp[(1) - (2)].lex).line);
        else
            (yyval.interm.type).setBasic(EbtVoid, EvqInvariantVaryingIn, (yyvsp[(1) - (2)].lex).line);
    ;}
    break;

  case 108:

    {
        if (context->globalErrorCheck((yyvsp[(1) - (1)].lex).line, context->symbolTable.atGlobalLevel(), "uniform"))
            context->recover();
        (yyval.interm.type).setBasic(EbtVoid, EvqUniform, (yyvsp[(1) - (1)].lex).line);
    ;}
    break;

  case 109:

    {
        (yyval.interm.type) = (yyvsp[(1) - (1)].interm.type);

        if ((yyval.interm.type).precision == EbpUndefined) {
            (yyval.interm.type).precision = context->symbolTable.getDefaultPrecision((yyvsp[(1) - (1)].interm.type).type);
            if (context->precisionErrorCheck((yyvsp[(1) - (1)].interm.type).line, (yyval.interm.type).precision, (yyvsp[(1) - (1)].interm.type).type)) {
                context->recover();
            }
        }
    ;}
    break;

  case 110:

    {
        (yyval.interm.type) = (yyvsp[(2) - (2)].interm.type);
        (yyval.interm.type).precision = (yyvsp[(1) - (2)].interm.precision);
    ;}
    break;

  case 111:

    {
        (yyval.interm.precision) = EbpHigh;
    ;}
    break;

  case 112:

    {
        (yyval.interm.precision) = EbpMedium;
    ;}
    break;

  case 113:

    {
        (yyval.interm.precision) = EbpLow;
    ;}
    break;

  case 114:

    {
        (yyval.interm.type) = (yyvsp[(1) - (1)].interm.type);
    ;}
    break;

  case 115:

    {
        (yyval.interm.type) = (yyvsp[(1) - (4)].interm.type);

        if (context->arrayTypeErrorCheck((yyvsp[(2) - (4)].lex).line, (yyvsp[(1) - (4)].interm.type)))
            context->recover();
        else {
            int size;
            if (context->arraySizeErrorCheck((yyvsp[(2) - (4)].lex).line, (yyvsp[(3) - (4)].interm.intermTypedNode), size))
                context->recover();
            (yyval.interm.type).setArray(true, size);
        }
    ;}
    break;

  case 116:

    {
        TQualifier qual = context->symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary;
        (yyval.interm.type).setBasic(EbtVoid, qual, (yyvsp[(1) - (1)].lex).line);
    ;}
    break;

  case 117:

    {
        TQualifier qual = context->symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary;
        (yyval.interm.type).setBasic(EbtFloat, qual, (yyvsp[(1) - (1)].lex).line);
    ;}
    break;

  case 118:

    {
        TQualifier qual = context->symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary;
        (yyval.interm.type).setBasic(EbtInt, qual, (yyvsp[(1) - (1)].lex).line);
    ;}
    break;

  case 119:

    {
        TQualifier qual = context->symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary;
        (yyval.interm.type).setBasic(EbtBool, qual, (yyvsp[(1) - (1)].lex).line);
    ;}
    break;

  case 120:

    {
        TQualifier qual = context->symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary;
        (yyval.interm.type).setBasic(EbtFloat, qual, (yyvsp[(1) - (1)].lex).line);
        (yyval.interm.type).setAggregate(2);
    ;}
    break;

  case 121:

    {
        TQualifier qual = context->symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary;
        (yyval.interm.type).setBasic(EbtFloat, qual, (yyvsp[(1) - (1)].lex).line);
        (yyval.interm.type).setAggregate(3);
    ;}
    break;

  case 122:

    {
        TQualifier qual = context->symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary;
        (yyval.interm.type).setBasic(EbtFloat, qual, (yyvsp[(1) - (1)].lex).line);
        (yyval.interm.type).setAggregate(4);
    ;}
    break;

  case 123:

    {
        TQualifier qual = context->symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary;
        (yyval.interm.type).setBasic(EbtBool, qual, (yyvsp[(1) - (1)].lex).line);
        (yyval.interm.type).setAggregate(2);
    ;}
    break;

  case 124:

    {
        TQualifier qual = context->symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary;
        (yyval.interm.type).setBasic(EbtBool, qual, (yyvsp[(1) - (1)].lex).line);
        (yyval.interm.type).setAggregate(3);
    ;}
    break;

  case 125:

    {
        TQualifier qual = context->symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary;
        (yyval.interm.type).setBasic(EbtBool, qual, (yyvsp[(1) - (1)].lex).line);
        (yyval.interm.type).setAggregate(4);
    ;}
    break;

  case 126:

    {
        TQualifier qual = context->symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary;
        (yyval.interm.type).setBasic(EbtInt, qual, (yyvsp[(1) - (1)].lex).line);
        (yyval.interm.type).setAggregate(2);
    ;}
    break;

  case 127:

    {
        TQualifier qual = context->symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary;
        (yyval.interm.type).setBasic(EbtInt, qual, (yyvsp[(1) - (1)].lex).line);
        (yyval.interm.type).setAggregate(3);
    ;}
    break;

  case 128:

    {
        TQualifier qual = context->symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary;
        (yyval.interm.type).setBasic(EbtInt, qual, (yyvsp[(1) - (1)].lex).line);
        (yyval.interm.type).setAggregate(4);
    ;}
    break;

  case 129:

    {
        FRAG_VERT_ONLY("mat2", (yyvsp[(1) - (1)].lex).line);
        TQualifier qual = context->symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary;
        (yyval.interm.type).setBasic(EbtFloat, qual, (yyvsp[(1) - (1)].lex).line);
        (yyval.interm.type).setAggregate(2, true);
    ;}
    break;

  case 130:

    {
        FRAG_VERT_ONLY("mat3", (yyvsp[(1) - (1)].lex).line);
        TQualifier qual = context->symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary;
        (yyval.interm.type).setBasic(EbtFloat, qual, (yyvsp[(1) - (1)].lex).line);
        (yyval.interm.type).setAggregate(3, true);
    ;}
    break;

  case 131:

    {
        FRAG_VERT_ONLY("mat4", (yyvsp[(1) - (1)].lex).line);
        TQualifier qual = context->symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary;
        (yyval.interm.type).setBasic(EbtFloat, qual, (yyvsp[(1) - (1)].lex).line);
        (yyval.interm.type).setAggregate(4, true);
    ;}
    break;

  case 132:

    {
        FRAG_VERT_ONLY("sampler2D", (yyvsp[(1) - (1)].lex).line);
        TQualifier qual = context->symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary;
        (yyval.interm.type).setBasic(EbtSampler2D, qual, (yyvsp[(1) - (1)].lex).line);
    ;}
    break;

  case 133:

    {
        FRAG_VERT_ONLY("samplerCube", (yyvsp[(1) - (1)].lex).line);
        TQualifier qual = context->symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary;
        (yyval.interm.type).setBasic(EbtSamplerCube, qual, (yyvsp[(1) - (1)].lex).line);
    ;}
    break;

  case 134:

    {
        if (!context->supportsExtension("GL_OES_EGL_image_external")) {
            context->error((yyvsp[(1) - (1)].lex).line, "unsupported type", "samplerExternalOES");
            context->recover();
        }
        FRAG_VERT_ONLY("samplerExternalOES", (yyvsp[(1) - (1)].lex).line);
        TQualifier qual = context->symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary;
        (yyval.interm.type).setBasic(EbtSamplerExternalOES, qual, (yyvsp[(1) - (1)].lex).line);
    ;}
    break;

  case 135:

    {
        if (!context->supportsExtension("GL_ARB_texture_rectangle")) {
            context->error((yyvsp[(1) - (1)].lex).line, "unsupported type", "sampler2DRect");
            context->recover();
        }
        FRAG_VERT_ONLY("sampler2DRect", (yyvsp[(1) - (1)].lex).line);
        TQualifier qual = context->symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary;
        (yyval.interm.type).setBasic(EbtSampler2DRect, qual, (yyvsp[(1) - (1)].lex).line);
    ;}
    break;

  case 136:

    {
        FRAG_VERT_ONLY("struct", (yyvsp[(1) - (1)].interm.type).line);
        (yyval.interm.type) = (yyvsp[(1) - (1)].interm.type);
        (yyval.interm.type).qualifier = context->symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary;
    ;}
    break;

  case 137:

    {
        //
        // This is for user defined type names.  The lexical phase looked up the
        // type.
        //
        TType& structure = static_cast<TVariable*>((yyvsp[(1) - (1)].lex).symbol)->getType();
        TQualifier qual = context->symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary;
        (yyval.interm.type).setBasic(EbtStruct, qual, (yyvsp[(1) - (1)].lex).line);
        (yyval.interm.type).userDef = &structure;
    ;}
    break;

  case 138:

    { if (context->enterStructDeclaration((yyvsp[(2) - (3)].lex).line, *(yyvsp[(2) - (3)].lex).string)) context->recover(); ;}
    break;

  case 139:

    {
        if (context->reservedErrorCheck((yyvsp[(2) - (6)].lex).line, *(yyvsp[(2) - (6)].lex).string))
            context->recover();

        TType* structure = new TType((yyvsp[(5) - (6)].interm.typeList), *(yyvsp[(2) - (6)].lex).string);
        TVariable* userTypeDef = new TVariable((yyvsp[(2) - (6)].lex).string, *structure, true);
        if (! context->symbolTable.insert(*userTypeDef)) {
            context->error((yyvsp[(2) - (6)].lex).line, "redefinition", (yyvsp[(2) - (6)].lex).string->c_str(), "struct");
            context->recover();
        }
        (yyval.interm.type).setBasic(EbtStruct, EvqTemporary, (yyvsp[(1) - (6)].lex).line);
        (yyval.interm.type).userDef = structure;
        context->exitStructDeclaration();
    ;}
    break;

  case 140:

    { if (context->enterStructDeclaration((yyvsp[(2) - (2)].lex).line, *(yyvsp[(2) - (2)].lex).string)) context->recover(); ;}
    break;

  case 141:

    {
        TType* structure = new TType((yyvsp[(4) - (5)].interm.typeList), TString(""));
        (yyval.interm.type).setBasic(EbtStruct, EvqTemporary, (yyvsp[(1) - (5)].lex).line);
        (yyval.interm.type).userDef = structure;
        context->exitStructDeclaration();
    ;}
    break;

  case 142:

    {
        (yyval.interm.typeList) = (yyvsp[(1) - (1)].interm.typeList);
    ;}
    break;

  case 143:

    {
        (yyval.interm.typeList) = (yyvsp[(1) - (2)].interm.typeList);
        for (unsigned int i = 0; i < (yyvsp[(2) - (2)].interm.typeList)->size(); ++i) {
            for (unsigned int j = 0; j < (yyval.interm.typeList)->size(); ++j) {
                if ((*(yyval.interm.typeList))[j].type->getFieldName() == (*(yyvsp[(2) - (2)].interm.typeList))[i].type->getFieldName()) {
                    context->error((*(yyvsp[(2) - (2)].interm.typeList))[i].line, "duplicate field name in structure:", "struct", (*(yyvsp[(2) - (2)].interm.typeList))[i].type->getFieldName().c_str());
                    context->recover();
                }
            }
            (yyval.interm.typeList)->push_back((*(yyvsp[(2) - (2)].interm.typeList))[i]);
        }
    ;}
    break;

  case 144:

    {
        (yyval.interm.typeList) = (yyvsp[(2) - (3)].interm.typeList);

        if (context->voidErrorCheck((yyvsp[(1) - (3)].interm.type).line, (*(yyvsp[(2) - (3)].interm.typeList))[0].type->getFieldName(), (yyvsp[(1) - (3)].interm.type))) {
            context->recover();
        }
        for (unsigned int i = 0; i < (yyval.interm.typeList)->size(); ++i) {
            //
            // Careful not to replace already known aspects of type, like array-ness
            //
            TType* type = (*(yyval.interm.typeList))[i].type;
            type->setBasicType((yyvsp[(1) - (3)].interm.type).type);
            type->setNominalSize((yyvsp[(1) - (3)].interm.type).size);
            type->setMatrix((yyvsp[(1) - (3)].interm.type).matrix);
            type->setPrecision((yyvsp[(1) - (3)].interm.type).precision);

            // don't allow arrays of arrays
            if (type->isArray()) {
                if (context->arrayTypeErrorCheck((yyvsp[(1) - (3)].interm.type).line, (yyvsp[(1) - (3)].interm.type)))
                    context->recover();
            }
            if ((yyvsp[(1) - (3)].interm.type).array)
                type->setArraySize((yyvsp[(1) - (3)].interm.type).arraySize);
            if ((yyvsp[(1) - (3)].interm.type).userDef) {
                type->setStruct((yyvsp[(1) - (3)].interm.type).userDef->getStruct());
                type->setTypeName((yyvsp[(1) - (3)].interm.type).userDef->getTypeName());
            }

            if (context->structNestingErrorCheck((yyvsp[(1) - (3)].interm.type).line, *type)) {
                context->recover();
            }
        }
    ;}
    break;

  case 145:

    {
        (yyval.interm.typeList) = NewPoolTTypeList();
        (yyval.interm.typeList)->push_back((yyvsp[(1) - (1)].interm.typeLine));
    ;}
    break;

  case 146:

    {
        (yyval.interm.typeList)->push_back((yyvsp[(3) - (3)].interm.typeLine));
    ;}
    break;

  case 147:

    {
        if (context->reservedErrorCheck((yyvsp[(1) - (1)].lex).line, *(yyvsp[(1) - (1)].lex).string))
            context->recover();

        (yyval.interm.typeLine).type = new TType(EbtVoid, EbpUndefined);
        (yyval.interm.typeLine).line = (yyvsp[(1) - (1)].lex).line;
        (yyval.interm.typeLine).type->setFieldName(*(yyvsp[(1) - (1)].lex).string);
    ;}
    break;

  case 148:

    {
        if (context->reservedErrorCheck((yyvsp[(1) - (4)].lex).line, *(yyvsp[(1) - (4)].lex).string))
            context->recover();

        (yyval.interm.typeLine).type = new TType(EbtVoid, EbpUndefined);
        (yyval.interm.typeLine).line = (yyvsp[(1) - (4)].lex).line;
        (yyval.interm.typeLine).type->setFieldName(*(yyvsp[(1) - (4)].lex).string);

        int size;
        if (context->arraySizeErrorCheck((yyvsp[(2) - (4)].lex).line, (yyvsp[(3) - (4)].interm.intermTypedNode), size))
            context->recover();
        (yyval.interm.typeLine).type->setArraySize(size);
    ;}
    break;

  case 149:

    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); ;}
    break;

  case 150:

    { (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode); ;}
    break;

  case 151:

    { (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermAggregate); ;}
    break;

  case 152:

    { (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode); ;}
    break;

  case 153:

    { (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode); ;}
    break;

  case 154:

    { (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode); ;}
    break;

  case 155:

    { (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode); ;}
    break;

  case 156:

    { (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode); ;}
    break;

  case 157:

    { (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode); ;}
    break;

  case 158:

    { (yyval.interm.intermAggregate) = 0; ;}
    break;

  case 159:

    { context->symbolTable.push(); ;}
    break;

  case 160:

    { context->symbolTable.pop(); ;}
    break;

  case 161:

    {
        if ((yyvsp[(3) - (5)].interm.intermAggregate) != 0) {
            (yyvsp[(3) - (5)].interm.intermAggregate)->setOp(EOpSequence);
            (yyvsp[(3) - (5)].interm.intermAggregate)->setEndLine((yyvsp[(5) - (5)].lex).line);
        }
        (yyval.interm.intermAggregate) = (yyvsp[(3) - (5)].interm.intermAggregate);
    ;}
    break;

  case 162:

    { (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode); ;}
    break;

  case 163:

    { (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode); ;}
    break;

  case 164:

    { context->symbolTable.push(); ;}
    break;

  case 165:

    { context->symbolTable.pop(); (yyval.interm.intermNode) = (yyvsp[(2) - (2)].interm.intermNode); ;}
    break;

  case 166:

    { context->symbolTable.push(); ;}
    break;

  case 167:

    { context->symbolTable.pop(); (yyval.interm.intermNode) = (yyvsp[(2) - (2)].interm.intermNode); ;}
    break;

  case 168:

    {
        (yyval.interm.intermNode) = 0;
    ;}
    break;

  case 169:

    {
        if ((yyvsp[(2) - (3)].interm.intermAggregate)) {
            (yyvsp[(2) - (3)].interm.intermAggregate)->setOp(EOpSequence);
            (yyvsp[(2) - (3)].interm.intermAggregate)->setEndLine((yyvsp[(3) - (3)].lex).line);
        }
        (yyval.interm.intermNode) = (yyvsp[(2) - (3)].interm.intermAggregate);
    ;}
    break;

  case 170:

    {
        (yyval.interm.intermAggregate) = context->intermediate.makeAggregate((yyvsp[(1) - (1)].interm.intermNode), 0);
    ;}
    break;

  case 171:

    {
        (yyval.interm.intermAggregate) = context->intermediate.growAggregate((yyvsp[(1) - (2)].interm.intermAggregate), (yyvsp[(2) - (2)].interm.intermNode), 0);
    ;}
    break;

  case 172:

    { (yyval.interm.intermNode) = 0; ;}
    break;

  case 173:

    { (yyval.interm.intermNode) = static_cast<TIntermNode*>((yyvsp[(1) - (2)].interm.intermTypedNode)); ;}
    break;

  case 174:

    {
        if (context->boolErrorCheck((yyvsp[(1) - (5)].lex).line, (yyvsp[(3) - (5)].interm.intermTypedNode)))
            context->recover();
        (yyval.interm.intermNode) = context->intermediate.addSelection((yyvsp[(3) - (5)].interm.intermTypedNode), (yyvsp[(5) - (5)].interm.nodePair), (yyvsp[(1) - (5)].lex).line);
    ;}
    break;

  case 175:

    {
        (yyval.interm.nodePair).node1 = (yyvsp[(1) - (3)].interm.intermNode);
        (yyval.interm.nodePair).node2 = (yyvsp[(3) - (3)].interm.intermNode);
    ;}
    break;

  case 176:

    {
        (yyval.interm.nodePair).node1 = (yyvsp[(1) - (1)].interm.intermNode);
        (yyval.interm.nodePair).node2 = 0;
    ;}
    break;

  case 177:

    {
        (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode);
        if (context->boolErrorCheck((yyvsp[(1) - (1)].interm.intermTypedNode)->getLine(), (yyvsp[(1) - (1)].interm.intermTypedNode)))
            context->recover();
    ;}
    break;

  case 178:

    {
        TIntermNode* intermNode;
        if (context->structQualifierErrorCheck((yyvsp[(2) - (4)].lex).line, (yyvsp[(1) - (4)].interm.type)))
            context->recover();
        if (context->boolErrorCheck((yyvsp[(2) - (4)].lex).line, (yyvsp[(1) - (4)].interm.type)))
            context->recover();

        if (!context->executeInitializer((yyvsp[(2) - (4)].lex).line, *(yyvsp[(2) - (4)].lex).string, (yyvsp[(1) - (4)].interm.type), (yyvsp[(4) - (4)].interm.intermTypedNode), intermNode))
            (yyval.interm.intermTypedNode) = (yyvsp[(4) - (4)].interm.intermTypedNode);
        else {
            context->recover();
            (yyval.interm.intermTypedNode) = 0;
        }
    ;}
    break;

  case 179:

    { context->symbolTable.push(); ++context->loopNestingLevel; ;}
    break;

  case 180:

    {
        context->symbolTable.pop();
        (yyval.interm.intermNode) = context->intermediate.addLoop(ELoopWhile, 0, (yyvsp[(4) - (6)].interm.intermTypedNode), 0, (yyvsp[(6) - (6)].interm.intermNode), (yyvsp[(1) - (6)].lex).line);
        --context->loopNestingLevel;
    ;}
    break;

  case 181:

    { ++context->loopNestingLevel; ;}
    break;

  case 182:

    {
        if (context->boolErrorCheck((yyvsp[(8) - (8)].lex).line, (yyvsp[(6) - (8)].interm.intermTypedNode)))
            context->recover();

        (yyval.interm.intermNode) = context->intermediate.addLoop(ELoopDoWhile, 0, (yyvsp[(6) - (8)].interm.intermTypedNode), 0, (yyvsp[(3) - (8)].interm.intermNode), (yyvsp[(4) - (8)].lex).line);
        --context->loopNestingLevel;
    ;}
    break;

  case 183:

    { context->symbolTable.push(); ++context->loopNestingLevel; ;}
    break;

  case 184:

    {
        context->symbolTable.pop();
        (yyval.interm.intermNode) = context->intermediate.addLoop(ELoopFor, (yyvsp[(4) - (7)].interm.intermNode), reinterpret_cast<TIntermTyped*>((yyvsp[(5) - (7)].interm.nodePair).node1), reinterpret_cast<TIntermTyped*>((yyvsp[(5) - (7)].interm.nodePair).node2), (yyvsp[(7) - (7)].interm.intermNode), (yyvsp[(1) - (7)].lex).line);
        --context->loopNestingLevel;
    ;}
    break;

  case 185:

    {
        (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode);
    ;}
    break;

  case 186:

    {
        (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode);
    ;}
    break;

  case 187:

    {
        (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode);
    ;}
    break;

  case 188:

    {
        (yyval.interm.intermTypedNode) = 0;
    ;}
    break;

  case 189:

    {
        (yyval.interm.nodePair).node1 = (yyvsp[(1) - (2)].interm.intermTypedNode);
        (yyval.interm.nodePair).node2 = 0;
    ;}
    break;

  case 190:

    {
        (yyval.interm.nodePair).node1 = (yyvsp[(1) - (3)].interm.intermTypedNode);
        (yyval.interm.nodePair).node2 = (yyvsp[(3) - (3)].interm.intermTypedNode);
    ;}
    break;

  case 191:

    {
        if (context->loopNestingLevel <= 0) {
            context->error((yyvsp[(1) - (2)].lex).line, "continue statement only allowed in loops", "");
            context->recover();
        }
        (yyval.interm.intermNode) = context->intermediate.addBranch(EOpContinue, (yyvsp[(1) - (2)].lex).line);
    ;}
    break;

  case 192:

    {
        if (context->loopNestingLevel <= 0) {
            context->error((yyvsp[(1) - (2)].lex).line, "break statement only allowed in loops", "");
            context->recover();
        }
        (yyval.interm.intermNode) = context->intermediate.addBranch(EOpBreak, (yyvsp[(1) - (2)].lex).line);
    ;}
    break;

  case 193:

    {
        (yyval.interm.intermNode) = context->intermediate.addBranch(EOpReturn, (yyvsp[(1) - (2)].lex).line);
        if (context->currentFunctionType->getBasicType() != EbtVoid) {
            context->error((yyvsp[(1) - (2)].lex).line, "non-void function must return a value", "return");
            context->recover();
        }
    ;}
    break;

  case 194:

    {
        (yyval.interm.intermNode) = context->intermediate.addBranch(EOpReturn, (yyvsp[(2) - (3)].interm.intermTypedNode), (yyvsp[(1) - (3)].lex).line);
        context->functionReturnsValue = true;
        if (context->currentFunctionType->getBasicType() == EbtVoid) {
            context->error((yyvsp[(1) - (3)].lex).line, "void function cannot return a value", "return");
            context->recover();
        } else if (*(context->currentFunctionType) != (yyvsp[(2) - (3)].interm.intermTypedNode)->getType()) {
            context->error((yyvsp[(1) - (3)].lex).line, "function return is not matching type:", "return");
            context->recover();
        }
    ;}
    break;

  case 195:

    {
        FRAG_ONLY("discard", (yyvsp[(1) - (2)].lex).line);
        (yyval.interm.intermNode) = context->intermediate.addBranch(EOpKill, (yyvsp[(1) - (2)].lex).line);
    ;}
    break;

  case 196:

    {
        (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode);
        context->treeRoot = (yyval.interm.intermNode);
    ;}
    break;

  case 197:

    {
        (yyval.interm.intermNode) = context->intermediate.growAggregate((yyvsp[(1) - (2)].interm.intermNode), (yyvsp[(2) - (2)].interm.intermNode), 0);
        context->treeRoot = (yyval.interm.intermNode);
    ;}
    break;

  case 198:

    {
        (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode);
    ;}
    break;

  case 199:

    {
        (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode);
    ;}
    break;

  case 200:

    {
        TFunction* function = (yyvsp[(1) - (1)].interm).function;
        
        const TSymbol *builtIn = context->symbolTable.findBuiltIn(function->getMangledName());
        
        if (builtIn)
        {
            context->error((yyvsp[(1) - (1)].interm).line, "built-in functions cannot be redefined", function->getName().c_str());
            context->recover();
        }
        
        TFunction* prevDec = static_cast<TFunction*>(context->symbolTable.find(function->getMangledName()));
        //
        // Note:  'prevDec' could be 'function' if this is the first time we've seen function
        // as it would have just been put in the symbol table.  Otherwise, we're looking up
        // an earlier occurance.
        //
        if (prevDec->isDefined()) {
            //
            // Then this function already has a body.
            //
            context->error((yyvsp[(1) - (1)].interm).line, "function already has a body", function->getName().c_str());
            context->recover();
        }
        prevDec->setDefined();

        //
        // Raise error message if main function takes any parameters or return anything other than void
        //
        if (function->getName() == "main") {
            if (function->getParamCount() > 0) {
                context->error((yyvsp[(1) - (1)].interm).line, "function cannot take any parameter(s)", function->getName().c_str());
                context->recover();
            }
            if (function->getReturnType().getBasicType() != EbtVoid) {
                context->error((yyvsp[(1) - (1)].interm).line, "", function->getReturnType().getBasicString(), "main function cannot return a value");
                context->recover();
            }
        }

        //
        // Remember the return type for later checking for RETURN statements.
        //
        context->currentFunctionType = &(prevDec->getReturnType());
        context->functionReturnsValue = false;

        //
        // Insert parameters into the symbol table.
        // If the parameter has no name, it's not an error, just don't insert it
        // (could be used for unused args).
        //
        // Also, accumulate the list of parameters into the HIL, so lower level code
        // knows where to find parameters.
        //
        TIntermAggregate* paramNodes = new TIntermAggregate;
        for (size_t i = 0; i < function->getParamCount(); i++) {
            const TParameter& param = function->getParam(i);
            if (param.name != 0) {
                TVariable *variable = new TVariable(param.name, *param.type);
                //
                // Insert the parameters with name in the symbol table.
                //
                if (! context->symbolTable.insert(*variable)) {
                    context->error((yyvsp[(1) - (1)].interm).line, "redefinition", variable->getName().c_str());
                    context->recover();
                    delete variable;
                }

                //
                // Add the parameter to the HIL
                //
                paramNodes = context->intermediate.growAggregate(
                                               paramNodes,
                                               context->intermediate.addSymbol(variable->getUniqueId(),
                                                                       variable->getName(),
                                                                       variable->getType(), (yyvsp[(1) - (1)].interm).line),
                                               (yyvsp[(1) - (1)].interm).line);
            } else {
                paramNodes = context->intermediate.growAggregate(paramNodes, context->intermediate.addSymbol(0, "", *param.type, (yyvsp[(1) - (1)].interm).line), (yyvsp[(1) - (1)].interm).line);
            }
        }
        context->intermediate.setAggregateOperator(paramNodes, EOpParameters, (yyvsp[(1) - (1)].interm).line);
        (yyvsp[(1) - (1)].interm).intermAggregate = paramNodes;
        context->loopNestingLevel = 0;
    ;}
    break;

  case 201:

    {
        //?? Check that all paths return a value if return type != void ?
        //   May be best done as post process phase on intermediate code
        if (context->currentFunctionType->getBasicType() != EbtVoid && ! context->functionReturnsValue) {
            context->error((yyvsp[(1) - (3)].interm).line, "function does not return a value:", "", (yyvsp[(1) - (3)].interm).function->getName().c_str());
            context->recover();
        }
        
        (yyval.interm.intermNode) = context->intermediate.growAggregate((yyvsp[(1) - (3)].interm).intermAggregate, (yyvsp[(3) - (3)].interm.intermNode), 0);
        context->intermediate.setAggregateOperator((yyval.interm.intermNode), EOpFunction, (yyvsp[(1) - (3)].interm).line);
        (yyval.interm.intermNode)->getAsAggregate()->setName((yyvsp[(1) - (3)].interm).function->getMangledName().c_str());
        (yyval.interm.intermNode)->getAsAggregate()->setType((yyvsp[(1) - (3)].interm).function->getReturnType());

        // store the pragma information for debug and optimize and other vendor specific
        // information. This information can be queried from the parse tree
        (yyval.interm.intermNode)->getAsAggregate()->setOptimize(context->pragma().optimize);
        (yyval.interm.intermNode)->getAsAggregate()->setDebug(context->pragma().debug);

        if ((yyvsp[(3) - (3)].interm.intermNode) && (yyvsp[(3) - (3)].interm.intermNode)->getAsAggregate())
            (yyval.interm.intermNode)->getAsAggregate()->setEndLine((yyvsp[(3) - (3)].interm.intermNode)->getAsAggregate()->getEndLine());

        context->symbolTable.pop();
    ;}
    break;


/* Line 1267 of yacc.c.  */

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
      yyerror (context, YY_("syntax error"));
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
	    yyerror (context, yymsg);
	  }
	else
	  {
	    yyerror (context, YY_("syntax error"));
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
		      yytoken, &yylval, context);
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
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

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
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

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
		  yystos[yystate], yyvsp, context);
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
  yyerror (context, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEOF && yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval, context);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp, context);
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





int glslang_parse(TParseContext* context) {
    return yyparse(context);
}


