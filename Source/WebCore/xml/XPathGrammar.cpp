/* This file is generated using the following command:
   bison -d -p xpathyy XPathGrammar.y -o XPathGrammar.cpp
 */

/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 2

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1


/* Substitute the variable and function names.  */
#define yyparse         xpathyyparse
#define yylex           xpathyylex
#define yyerror         xpathyyerror
#define yydebug         xpathyydebug

/* First part of user prologue.  */
#line 28 "XPathGrammar.y"


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


#line 101 "XPathGrammar.cpp"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "XPathGrammar.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_MULOP = 3,                      /* MULOP  */
  YYSYMBOL_EQOP = 4,                       /* EQOP  */
  YYSYMBOL_RELOP = 5,                      /* RELOP  */
  YYSYMBOL_PLUS = 6,                       /* PLUS  */
  YYSYMBOL_MINUS = 7,                      /* MINUS  */
  YYSYMBOL_OR = 8,                         /* OR  */
  YYSYMBOL_AND = 9,                        /* AND  */
  YYSYMBOL_FUNCTIONNAME = 10,              /* FUNCTIONNAME  */
  YYSYMBOL_LITERAL = 11,                   /* LITERAL  */
  YYSYMBOL_NAMETEST = 12,                  /* NAMETEST  */
  YYSYMBOL_NUMBER = 13,                    /* NUMBER  */
  YYSYMBOL_NODETYPE = 14,                  /* NODETYPE  */
  YYSYMBOL_VARIABLEREFERENCE = 15,         /* VARIABLEREFERENCE  */
  YYSYMBOL_AXISNAME = 16,                  /* AXISNAME  */
  YYSYMBOL_COMMENT = 17,                   /* COMMENT  */
  YYSYMBOL_DOTDOT = 18,                    /* DOTDOT  */
  YYSYMBOL_PI = 19,                        /* PI  */
  YYSYMBOL_NODE = 20,                      /* NODE  */
  YYSYMBOL_SLASHSLASH = 21,                /* SLASHSLASH  */
  YYSYMBOL_TEXT_ = 22,                     /* TEXT_  */
  YYSYMBOL_XPATH_ERROR = 23,               /* XPATH_ERROR  */
  YYSYMBOL_24_ = 24,                       /* '/'  */
  YYSYMBOL_25_ = 25,                       /* '@'  */
  YYSYMBOL_26_ = 26,                       /* '('  */
  YYSYMBOL_27_ = 27,                       /* ')'  */
  YYSYMBOL_28_ = 28,                       /* '['  */
  YYSYMBOL_29_ = 29,                       /* ']'  */
  YYSYMBOL_30_ = 30,                       /* '.'  */
  YYSYMBOL_31_ = 31,                       /* ','  */
  YYSYMBOL_32_ = 32,                       /* '|'  */
  YYSYMBOL_YYACCEPT = 33,                  /* $accept  */
  YYSYMBOL_Top = 34,                       /* Top  */
  YYSYMBOL_Expr = 35,                      /* Expr  */
  YYSYMBOL_LocationPath = 36,              /* LocationPath  */
  YYSYMBOL_AbsoluteLocationPath = 37,      /* AbsoluteLocationPath  */
  YYSYMBOL_RelativeLocationPath = 38,      /* RelativeLocationPath  */
  YYSYMBOL_Step = 39,                      /* Step  */
  YYSYMBOL_AxisSpecifier = 40,             /* AxisSpecifier  */
  YYSYMBOL_NodeTest = 41,                  /* NodeTest  */
  YYSYMBOL_OptionalPredicateList = 42,     /* OptionalPredicateList  */
  YYSYMBOL_PredicateList = 43,             /* PredicateList  */
  YYSYMBOL_Predicate = 44,                 /* Predicate  */
  YYSYMBOL_DescendantOrSelf = 45,          /* DescendantOrSelf  */
  YYSYMBOL_AbbreviatedStep = 46,           /* AbbreviatedStep  */
  YYSYMBOL_PrimaryExpr = 47,               /* PrimaryExpr  */
  YYSYMBOL_FunctionCall = 48,              /* FunctionCall  */
  YYSYMBOL_ArgumentList = 49,              /* ArgumentList  */
  YYSYMBOL_Argument = 50,                  /* Argument  */
  YYSYMBOL_UnionExpr = 51,                 /* UnionExpr  */
  YYSYMBOL_PathExpr = 52,                  /* PathExpr  */
  YYSYMBOL_FilterExpr = 53,                /* FilterExpr  */
  YYSYMBOL_OrExpr = 54,                    /* OrExpr  */
  YYSYMBOL_AndExpr = 55,                   /* AndExpr  */
  YYSYMBOL_EqualityExpr = 56,              /* EqualityExpr  */
  YYSYMBOL_RelationalExpr = 57,            /* RelationalExpr  */
  YYSYMBOL_AdditiveExpr = 58,              /* AdditiveExpr  */
  YYSYMBOL_MultiplicativeExpr = 59,        /* MultiplicativeExpr  */
  YYSYMBOL_UnaryExpr = 60                  /* UnaryExpr  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;


/* Second part of user prologue.  */
#line 101 "XPathGrammar.y"


static int xpathyylex(YYSTYPE* yylval, WebCore::XPath::Parser& parser) { return parser.lex(*yylval); }
static void xpathyyerror(WebCore::XPath::Parser&, const char*) { }


#line 202 "XPathGrammar.cpp"


#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

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
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
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
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  52
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   134

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  33
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  28
/* YYNRULES -- Number of rules.  */
#define YYNRULES  64
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  101

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   278


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
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
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   111,   111,   118,   122,   128,   132,   137,   142,   150,
     156,   162,   171,   181,   199,   210,   228,   232,   234,   241,
     246,   251,   256,   261,   272,   276,   280,   286,   294,   301,
     308,   313,   320,   326,   331,   337,   343,   347,   355,   366,
     372,   380,   384,   386,   393,   398,   400,   406,   415,   417,
     425,   427,   434,   436,   443,   445,   452,   454,   461,   463,
     468,   475,   477,   484,   486
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "MULOP", "EQOP",
  "RELOP", "PLUS", "MINUS", "OR", "AND", "FUNCTIONNAME", "LITERAL",
  "NAMETEST", "NUMBER", "NODETYPE", "VARIABLEREFERENCE", "AXISNAME",
  "COMMENT", "DOTDOT", "PI", "NODE", "SLASHSLASH", "TEXT_", "XPATH_ERROR",
  "'/'", "'@'", "'('", "')'", "'['", "']'", "'.'", "','", "'|'", "$accept",
  "Top", "Expr", "LocationPath", "AbsoluteLocationPath",
  "RelativeLocationPath", "Step", "AxisSpecifier", "NodeTest",
  "OptionalPredicateList", "PredicateList", "Predicate",
  "DescendantOrSelf", "AbbreviatedStep", "PrimaryExpr", "FunctionCall",
  "ArgumentList", "Argument", "UnionExpr", "PathExpr", "FilterExpr",
  "OrExpr", "AndExpr", "EqualityExpr", "RelationalExpr", "AdditiveExpr",
  "MultiplicativeExpr", "UnaryExpr", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-37)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int8 yypact[] =
{
      83,    83,    -8,   -37,    -7,   -37,   -37,   -37,    14,   -37,
      17,    20,   -37,    21,     8,   -37,    83,   -37,    48,   -37,
     -37,   -37,   -17,   -37,    22,    -7,     8,   -37,    -7,   -37,
      23,   -37,    -9,     2,    44,    50,    51,    10,    54,   -37,
     -37,    62,    83,   -37,    -7,   -37,    31,    -5,    32,    33,
     -17,    34,   -37,     8,     8,    -7,    -7,   -37,   -17,    -7,
     104,     8,     8,    83,    83,    83,    83,    83,    83,    83,
     -37,   -37,   -18,   -37,    35,   -37,   -37,    36,   -37,   -37,
     -37,   -37,   -37,   -37,   -37,   -37,   -37,   -17,   -17,    44,
      50,    51,    10,    54,    54,   -37,   -37,    83,   -37,   -37,
     -37
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] =
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
      53,    55,    57,    59,    60,    62,    38,     0,    28,    23,
      40
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -37,   -37,     3,   -37,   -37,   -12,   -22,   -37,    38,   -20,
      37,   -36,   -21,   -37,   -37,   -37,   -37,   -27,   -37,    11,
     -37,   -37,    13,    27,    41,    19,   -16,    -1
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
       0,    18,    71,    20,    21,    22,    23,    24,    25,    43,
      44,    45,    26,    27,    28,    29,    72,    73,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int8 yytable[] =
{
      40,    54,    50,    19,    12,    57,    77,    53,    75,    96,
      63,    62,    12,    97,    58,    61,    67,    68,    41,    51,
       4,    42,    78,    75,     7,     8,     9,    10,    11,    54,
      13,    82,    83,    15,    55,    84,    85,    54,    17,     8,
      46,    10,    11,    47,    13,    74,    48,    49,    52,    87,
      88,    93,    94,    64,    65,    60,    66,    69,    76,    79,
      80,    81,    56,    99,    98,    59,    54,    54,    95,     1,
     100,    86,     2,     3,     4,     5,    89,     6,     7,     8,
       9,    10,    11,    12,    13,    92,    14,    15,    16,    70,
       1,    90,    17,     2,     3,     4,     5,     0,     6,     7,
       8,     9,    10,    11,    12,    13,    91,    14,    15,    16,
       0,     0,     0,    17,     2,     3,     4,     5,     0,     6,
       7,     8,     9,    10,    11,    12,    13,     0,    14,    15,
      16,     0,     0,     0,    17
};

static const yytype_int8 yycheck[] =
{
       1,    22,    14,     0,    21,    25,    11,    24,    44,    27,
       8,    32,    21,    31,    26,    24,     6,     7,    26,    16,
      12,    28,    27,    59,    16,    17,    18,    19,    20,    50,
      22,    53,    54,    25,    12,    55,    56,    58,    30,    17,
      26,    19,    20,    26,    22,    42,    26,    26,     0,    61,
      62,    67,    68,     9,     4,    32,     5,     3,    27,    27,
      27,    27,    24,    27,    29,    28,    87,    88,    69,     7,
      97,    60,    10,    11,    12,    13,    63,    15,    16,    17,
      18,    19,    20,    21,    22,    66,    24,    25,    26,    27,
       7,    64,    30,    10,    11,    12,    13,    -1,    15,    16,
      17,    18,    19,    20,    21,    22,    65,    24,    25,    26,
      -1,    -1,    -1,    30,    10,    11,    12,    13,    -1,    15,
      16,    17,    18,    19,    20,    21,    22,    -1,    24,    25,
      26,    -1,    -1,    -1,    30
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,     7,    10,    11,    12,    13,    15,    16,    17,    18,
      19,    20,    21,    22,    24,    25,    26,    30,    34,    35,
      36,    37,    38,    39,    40,    41,    45,    46,    47,    48,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      60,    26,    28,    42,    43,    44,    26,    26,    26,    26,
      38,    35,     0,    24,    45,    12,    41,    42,    38,    43,
      32,    24,    45,     8,     9,     4,     5,     6,     7,     3,
      27,    35,    49,    50,    35,    44,    27,    11,    27,    27,
      27,    27,    39,    39,    42,    42,    52,    38,    38,    55,
      56,    57,    58,    59,    59,    60,    27,    31,    29,    27,
      50
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    33,    34,    35,    36,    36,    37,    37,    37,    38,
      38,    38,    39,    39,    39,    39,    39,    40,    40,    41,
      41,    41,    41,    41,    42,    42,    43,    43,    44,    45,
      46,    46,    47,    47,    47,    47,    47,    48,    48,    49,
      49,    50,    51,    51,    52,    52,    52,    52,    53,    53,
      54,    54,    55,    55,    56,    56,    57,    57,    58,    58,
      58,    59,    59,    60,    60
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     1,     1,     1,     1,     2,     2,     1,
       3,     3,     2,     2,     3,     3,     1,     1,     1,     3,
       3,     3,     3,     4,     0,     1,     1,     2,     3,     1,
       1,     1,     1,     3,     1,     1,     1,     3,     4,     1,
       3,     1,     1,     3,     1,     1,     3,     3,     1,     2,
       1,     3,     1,     3,     1,     3,     1,     3,     1,     3,
       3,     1,     3,     1,     2
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (parser, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, parser); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, WebCore::XPath::Parser& parser)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (parser);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, WebCore::XPath::Parser& parser)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep, parser);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule, WebCore::XPath::Parser& parser)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)], parser);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, parser); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
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






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, WebCore::XPath::Parser& parser)
{
  YY_USE (yyvaluep);
  YY_USE (parser);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  switch (yykind)
    {
    case YYSYMBOL_FUNCTIONNAME: /* FUNCTIONNAME  */
#line 77 "XPathGrammar.y"
            { if (((*yyvaluep).string)) ((*yyvaluep).string)->deref(); }
#line 975 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_LITERAL: /* LITERAL  */
#line 77 "XPathGrammar.y"
            { if (((*yyvaluep).string)) ((*yyvaluep).string)->deref(); }
#line 981 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_NAMETEST: /* NAMETEST  */
#line 77 "XPathGrammar.y"
            { if (((*yyvaluep).string)) ((*yyvaluep).string)->deref(); }
#line 987 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_NUMBER: /* NUMBER  */
#line 77 "XPathGrammar.y"
            { if (((*yyvaluep).string)) ((*yyvaluep).string)->deref(); }
#line 993 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_NODETYPE: /* NODETYPE  */
#line 77 "XPathGrammar.y"
            { if (((*yyvaluep).string)) ((*yyvaluep).string)->deref(); }
#line 999 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_VARIABLEREFERENCE: /* VARIABLEREFERENCE  */
#line 77 "XPathGrammar.y"
            { if (((*yyvaluep).string)) ((*yyvaluep).string)->deref(); }
#line 1005 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_Expr: /* Expr  */
#line 97 "XPathGrammar.y"
            { delete ((*yyvaluep).expression); }
#line 1011 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_LocationPath: /* LocationPath  */
#line 85 "XPathGrammar.y"
            { delete ((*yyvaluep).locationPath); }
#line 1017 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_AbsoluteLocationPath: /* AbsoluteLocationPath  */
#line 85 "XPathGrammar.y"
            { delete ((*yyvaluep).locationPath); }
#line 1023 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_RelativeLocationPath: /* RelativeLocationPath  */
#line 85 "XPathGrammar.y"
            { delete ((*yyvaluep).locationPath); }
#line 1029 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_Step: /* Step  */
#line 94 "XPathGrammar.y"
            { delete ((*yyvaluep).step); }
#line 1035 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_NodeTest: /* NodeTest  */
#line 88 "XPathGrammar.y"
            { delete ((*yyvaluep).nodeTest); }
#line 1041 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_OptionalPredicateList: /* OptionalPredicateList  */
#line 91 "XPathGrammar.y"
            { delete ((*yyvaluep).expressionVector); }
#line 1047 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_PredicateList: /* PredicateList  */
#line 91 "XPathGrammar.y"
            { delete ((*yyvaluep).expressionVector); }
#line 1053 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_Predicate: /* Predicate  */
#line 97 "XPathGrammar.y"
            { delete ((*yyvaluep).expression); }
#line 1059 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_DescendantOrSelf: /* DescendantOrSelf  */
#line 94 "XPathGrammar.y"
            { delete ((*yyvaluep).step); }
#line 1065 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_AbbreviatedStep: /* AbbreviatedStep  */
#line 94 "XPathGrammar.y"
            { delete ((*yyvaluep).step); }
#line 1071 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_PrimaryExpr: /* PrimaryExpr  */
#line 97 "XPathGrammar.y"
            { delete ((*yyvaluep).expression); }
#line 1077 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_FunctionCall: /* FunctionCall  */
#line 97 "XPathGrammar.y"
            { delete ((*yyvaluep).expression); }
#line 1083 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_ArgumentList: /* ArgumentList  */
#line 91 "XPathGrammar.y"
            { delete ((*yyvaluep).expressionVector); }
#line 1089 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_Argument: /* Argument  */
#line 97 "XPathGrammar.y"
            { delete ((*yyvaluep).expression); }
#line 1095 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_UnionExpr: /* UnionExpr  */
#line 97 "XPathGrammar.y"
            { delete ((*yyvaluep).expression); }
#line 1101 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_PathExpr: /* PathExpr  */
#line 97 "XPathGrammar.y"
            { delete ((*yyvaluep).expression); }
#line 1107 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_FilterExpr: /* FilterExpr  */
#line 97 "XPathGrammar.y"
            { delete ((*yyvaluep).expression); }
#line 1113 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_OrExpr: /* OrExpr  */
#line 97 "XPathGrammar.y"
            { delete ((*yyvaluep).expression); }
#line 1119 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_AndExpr: /* AndExpr  */
#line 97 "XPathGrammar.y"
            { delete ((*yyvaluep).expression); }
#line 1125 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_EqualityExpr: /* EqualityExpr  */
#line 97 "XPathGrammar.y"
            { delete ((*yyvaluep).expression); }
#line 1131 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_RelationalExpr: /* RelationalExpr  */
#line 97 "XPathGrammar.y"
            { delete ((*yyvaluep).expression); }
#line 1137 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_AdditiveExpr: /* AdditiveExpr  */
#line 97 "XPathGrammar.y"
            { delete ((*yyvaluep).expression); }
#line 1143 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_MultiplicativeExpr: /* MultiplicativeExpr  */
#line 97 "XPathGrammar.y"
            { delete ((*yyvaluep).expression); }
#line 1149 "XPathGrammar.cpp"
        break;

    case YYSYMBOL_UnaryExpr: /* UnaryExpr  */
#line 97 "XPathGrammar.y"
            { delete ((*yyvaluep).expression); }
#line 1155 "XPathGrammar.cpp"
        break;

      default:
        break;
    }
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}






/*----------.
| yyparse.  |
`----------*/

int
yyparse (WebCore::XPath::Parser& parser)
{
/* Lookahead token kind.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex (&yylval, parser);
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
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
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
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
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* Top: Expr  */
#line 112 "XPathGrammar.y"
    {
        parser.setParseResult(std::unique_ptr<WebCore::XPath::Expression>((yyvsp[0].expression)));
    }
#line 1433 "XPathGrammar.cpp"
    break;

  case 4: /* LocationPath: AbsoluteLocationPath  */
#line 123 "XPathGrammar.y"
    {
        (yyval.locationPath) = (yyvsp[0].locationPath);
        (yyval.locationPath)->setAbsolute();
    }
#line 1442 "XPathGrammar.cpp"
    break;

  case 6: /* AbsoluteLocationPath: '/'  */
#line 133 "XPathGrammar.y"
    {
        (yyval.locationPath) = new WebCore::XPath::LocationPath;
    }
#line 1450 "XPathGrammar.cpp"
    break;

  case 7: /* AbsoluteLocationPath: '/' RelativeLocationPath  */
#line 138 "XPathGrammar.y"
    {
        (yyval.locationPath) = (yyvsp[0].locationPath);
    }
#line 1458 "XPathGrammar.cpp"
    break;

  case 8: /* AbsoluteLocationPath: DescendantOrSelf RelativeLocationPath  */
#line 143 "XPathGrammar.y"
    {
        (yyval.locationPath) = (yyvsp[0].locationPath);
        (yyval.locationPath)->prependStep(std::unique_ptr<WebCore::XPath::Step>((yyvsp[-1].step)));
    }
#line 1467 "XPathGrammar.cpp"
    break;

  case 9: /* RelativeLocationPath: Step  */
#line 151 "XPathGrammar.y"
    {
        (yyval.locationPath) = new WebCore::XPath::LocationPath;
        (yyval.locationPath)->appendStep(std::unique_ptr<WebCore::XPath::Step>((yyvsp[0].step)));
    }
#line 1476 "XPathGrammar.cpp"
    break;

  case 10: /* RelativeLocationPath: RelativeLocationPath '/' Step  */
#line 157 "XPathGrammar.y"
    {
        (yyval.locationPath) = (yyvsp[-2].locationPath);
        (yyval.locationPath)->appendStep(std::unique_ptr<WebCore::XPath::Step>((yyvsp[0].step)));
    }
#line 1485 "XPathGrammar.cpp"
    break;

  case 11: /* RelativeLocationPath: RelativeLocationPath DescendantOrSelf Step  */
#line 163 "XPathGrammar.y"
    {
        (yyval.locationPath) = (yyvsp[-2].locationPath);
        (yyval.locationPath)->appendStep(std::unique_ptr<WebCore::XPath::Step>((yyvsp[-1].step)));
        (yyval.locationPath)->appendStep(std::unique_ptr<WebCore::XPath::Step>((yyvsp[0].step)));
    }
#line 1495 "XPathGrammar.cpp"
    break;

  case 12: /* Step: NodeTest OptionalPredicateList  */
#line 172 "XPathGrammar.y"
    {
        std::unique_ptr<WebCore::XPath::Step::NodeTest> nodeTest((yyvsp[-1].nodeTest));
        std::unique_ptr<Vector<std::unique_ptr<WebCore::XPath::Expression>>> predicateList((yyvsp[0].expressionVector));
        if (predicateList)
            (yyval.step) = new WebCore::XPath::Step(WebCore::XPath::Step::ChildAxis, WTFMove(*nodeTest), WTFMove(*predicateList));
        else
            (yyval.step) = new WebCore::XPath::Step(WebCore::XPath::Step::ChildAxis, WTFMove(*nodeTest));
    }
#line 1508 "XPathGrammar.cpp"
    break;

  case 13: /* Step: NAMETEST OptionalPredicateList  */
#line 182 "XPathGrammar.y"
    {
        String nametest = adoptRef((yyvsp[-1].string));
        std::unique_ptr<Vector<std::unique_ptr<WebCore::XPath::Expression>>> predicateList((yyvsp[0].expressionVector));

        AtomString localName;
        AtomString namespaceURI;
        if (!parser.expandQualifiedName(nametest, localName, namespaceURI)) {
            (yyval.step) = nullptr;
            YYABORT;
        }

        if (predicateList)
            (yyval.step) = new WebCore::XPath::Step(WebCore::XPath::Step::ChildAxis, WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::NameTest, localName, namespaceURI), WTFMove(*predicateList));
        else
            (yyval.step) = new WebCore::XPath::Step(WebCore::XPath::Step::ChildAxis, WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::NameTest, localName, namespaceURI));
    }
#line 1529 "XPathGrammar.cpp"
    break;

  case 14: /* Step: AxisSpecifier NodeTest OptionalPredicateList  */
#line 200 "XPathGrammar.y"
    {
        std::unique_ptr<WebCore::XPath::Step::NodeTest> nodeTest((yyvsp[-1].nodeTest));
        std::unique_ptr<Vector<std::unique_ptr<WebCore::XPath::Expression>>> predicateList((yyvsp[0].expressionVector));

        if (predicateList)
            (yyval.step) = new WebCore::XPath::Step((yyvsp[-2].axis), WTFMove(*nodeTest), WTFMove(*predicateList));
        else
            (yyval.step) = new WebCore::XPath::Step((yyvsp[-2].axis), WTFMove(*nodeTest));
    }
#line 1543 "XPathGrammar.cpp"
    break;

  case 15: /* Step: AxisSpecifier NAMETEST OptionalPredicateList  */
#line 211 "XPathGrammar.y"
    {
        String nametest = adoptRef((yyvsp[-1].string));
        std::unique_ptr<Vector<std::unique_ptr<WebCore::XPath::Expression>>> predicateList((yyvsp[0].expressionVector));

        AtomString localName;
        AtomString namespaceURI;
        if (!parser.expandQualifiedName(nametest, localName, namespaceURI)) {
            (yyval.step) = nullptr;
            YYABORT;
        }

        if (predicateList)
            (yyval.step) = new WebCore::XPath::Step((yyvsp[-2].axis), WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::NameTest, localName, namespaceURI), WTFMove(*predicateList));
        else
            (yyval.step) = new WebCore::XPath::Step((yyvsp[-2].axis), WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::NameTest, localName, namespaceURI));
    }
#line 1564 "XPathGrammar.cpp"
    break;

  case 18: /* AxisSpecifier: '@'  */
#line 235 "XPathGrammar.y"
    {
        (yyval.axis) = WebCore::XPath::Step::AttributeAxis;
    }
#line 1572 "XPathGrammar.cpp"
    break;

  case 19: /* NodeTest: NODE '(' ')'  */
#line 242 "XPathGrammar.y"
    {
        (yyval.nodeTest) = new WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::AnyNodeTest);
    }
#line 1580 "XPathGrammar.cpp"
    break;

  case 20: /* NodeTest: TEXT_ '(' ')'  */
#line 247 "XPathGrammar.y"
    {
        (yyval.nodeTest) = new WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::TextNodeTest);
    }
#line 1588 "XPathGrammar.cpp"
    break;

  case 21: /* NodeTest: COMMENT '(' ')'  */
#line 252 "XPathGrammar.y"
    {
        (yyval.nodeTest) = new WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::CommentNodeTest);
    }
#line 1596 "XPathGrammar.cpp"
    break;

  case 22: /* NodeTest: PI '(' ')'  */
#line 257 "XPathGrammar.y"
    {
        (yyval.nodeTest) = new WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::ProcessingInstructionNodeTest);
    }
#line 1604 "XPathGrammar.cpp"
    break;

  case 23: /* NodeTest: PI '(' LITERAL ')'  */
#line 262 "XPathGrammar.y"
    {
        auto stringImpl = adoptRef((yyvsp[-1].string));
        if (stringImpl)
            stringImpl = stringImpl->trim(deprecatedIsSpaceOrNewline);
        (yyval.nodeTest) = new WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::ProcessingInstructionNodeTest, stringImpl.get());
    }
#line 1615 "XPathGrammar.cpp"
    break;

  case 24: /* OptionalPredicateList: %empty  */
#line 272 "XPathGrammar.y"
    {
        (yyval.expressionVector) = nullptr;
    }
#line 1623 "XPathGrammar.cpp"
    break;

  case 26: /* PredicateList: Predicate  */
#line 281 "XPathGrammar.y"
    {
        (yyval.expressionVector) = new Vector<std::unique_ptr<WebCore::XPath::Expression>>;
        (yyval.expressionVector)->append(std::unique_ptr<WebCore::XPath::Expression>((yyvsp[0].expression)));
    }
#line 1632 "XPathGrammar.cpp"
    break;

  case 27: /* PredicateList: PredicateList Predicate  */
#line 287 "XPathGrammar.y"
    {
        (yyval.expressionVector) = (yyvsp[-1].expressionVector);
        (yyval.expressionVector)->append(std::unique_ptr<WebCore::XPath::Expression>((yyvsp[0].expression)));
    }
#line 1641 "XPathGrammar.cpp"
    break;

  case 28: /* Predicate: '[' Expr ']'  */
#line 295 "XPathGrammar.y"
    {
        (yyval.expression) = (yyvsp[-1].expression);
    }
#line 1649 "XPathGrammar.cpp"
    break;

  case 29: /* DescendantOrSelf: SLASHSLASH  */
#line 302 "XPathGrammar.y"
    {
        (yyval.step) = new WebCore::XPath::Step(WebCore::XPath::Step::DescendantOrSelfAxis, WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::AnyNodeTest));
    }
#line 1657 "XPathGrammar.cpp"
    break;

  case 30: /* AbbreviatedStep: '.'  */
#line 309 "XPathGrammar.y"
    {
        (yyval.step) = new WebCore::XPath::Step(WebCore::XPath::Step::SelfAxis, WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::AnyNodeTest));
    }
#line 1665 "XPathGrammar.cpp"
    break;

  case 31: /* AbbreviatedStep: DOTDOT  */
#line 314 "XPathGrammar.y"
    {
        (yyval.step) = new WebCore::XPath::Step(WebCore::XPath::Step::ParentAxis, WebCore::XPath::Step::NodeTest(WebCore::XPath::Step::NodeTest::AnyNodeTest));
    }
#line 1673 "XPathGrammar.cpp"
    break;

  case 32: /* PrimaryExpr: VARIABLEREFERENCE  */
#line 321 "XPathGrammar.y"
    {
        String name = adoptRef((yyvsp[0].string));
        (yyval.expression) = new WebCore::XPath::VariableReference(name);
    }
#line 1682 "XPathGrammar.cpp"
    break;

  case 33: /* PrimaryExpr: '(' Expr ')'  */
#line 327 "XPathGrammar.y"
    {
        (yyval.expression) = (yyvsp[-1].expression);
    }
#line 1690 "XPathGrammar.cpp"
    break;

  case 34: /* PrimaryExpr: LITERAL  */
#line 332 "XPathGrammar.y"
    {
        String literal = adoptRef((yyvsp[0].string));
        (yyval.expression) = new WebCore::XPath::StringExpression(WTFMove(literal));
    }
#line 1699 "XPathGrammar.cpp"
    break;

  case 35: /* PrimaryExpr: NUMBER  */
#line 338 "XPathGrammar.y"
    {
        String numeral = adoptRef((yyvsp[0].string));
        (yyval.expression) = new WebCore::XPath::Number(numeral.toDouble());
    }
#line 1708 "XPathGrammar.cpp"
    break;

  case 37: /* FunctionCall: FUNCTIONNAME '(' ')'  */
#line 348 "XPathGrammar.y"
    {
        String name = adoptRef((yyvsp[-2].string));
        (yyval.expression) = WebCore::XPath::Function::create(name).release();
        if (!(yyval.expression))
            YYABORT;
    }
#line 1719 "XPathGrammar.cpp"
    break;

  case 38: /* FunctionCall: FUNCTIONNAME '(' ArgumentList ')'  */
#line 356 "XPathGrammar.y"
    {
        String name = adoptRef((yyvsp[-3].string));
        std::unique_ptr<Vector<std::unique_ptr<WebCore::XPath::Expression>>> argumentList((yyvsp[-1].expressionVector));
        (yyval.expression) = WebCore::XPath::Function::create(name, WTFMove(*argumentList)).release();
        if (!(yyval.expression))
            YYABORT;
    }
#line 1731 "XPathGrammar.cpp"
    break;

  case 39: /* ArgumentList: Argument  */
#line 367 "XPathGrammar.y"
    {
        (yyval.expressionVector) = new Vector<std::unique_ptr<WebCore::XPath::Expression>>;
        (yyval.expressionVector)->append(std::unique_ptr<WebCore::XPath::Expression>((yyvsp[0].expression)));
    }
#line 1740 "XPathGrammar.cpp"
    break;

  case 40: /* ArgumentList: ArgumentList ',' Argument  */
#line 373 "XPathGrammar.y"
    {
        (yyval.expressionVector) = (yyvsp[-2].expressionVector);
        (yyval.expressionVector)->append(std::unique_ptr<WebCore::XPath::Expression>((yyvsp[0].expression)));
    }
#line 1749 "XPathGrammar.cpp"
    break;

  case 43: /* UnionExpr: UnionExpr '|' PathExpr  */
#line 387 "XPathGrammar.y"
    {
        (yyval.expression) = new WebCore::XPath::Union(std::unique_ptr<WebCore::XPath::Expression>((yyvsp[-2].expression)), std::unique_ptr<WebCore::XPath::Expression>((yyvsp[0].expression)));
    }
#line 1757 "XPathGrammar.cpp"
    break;

  case 44: /* PathExpr: LocationPath  */
#line 394 "XPathGrammar.y"
    {
        (yyval.expression) = (yyvsp[0].locationPath);
    }
#line 1765 "XPathGrammar.cpp"
    break;

  case 46: /* PathExpr: FilterExpr '/' RelativeLocationPath  */
#line 401 "XPathGrammar.y"
    {
        (yyvsp[0].locationPath)->setAbsolute();
        (yyval.expression) = new WebCore::XPath::Path(std::unique_ptr<WebCore::XPath::Expression>((yyvsp[-2].expression)), std::unique_ptr<WebCore::XPath::LocationPath>((yyvsp[0].locationPath)));
    }
#line 1774 "XPathGrammar.cpp"
    break;

  case 47: /* PathExpr: FilterExpr DescendantOrSelf RelativeLocationPath  */
#line 407 "XPathGrammar.y"
    {
        (yyvsp[0].locationPath)->prependStep(std::unique_ptr<WebCore::XPath::Step>((yyvsp[-1].step)));
        (yyvsp[0].locationPath)->setAbsolute();
        (yyval.expression) = new WebCore::XPath::Path(std::unique_ptr<WebCore::XPath::Expression>((yyvsp[-2].expression)), std::unique_ptr<WebCore::XPath::LocationPath>((yyvsp[0].locationPath)));
    }
#line 1784 "XPathGrammar.cpp"
    break;

  case 49: /* FilterExpr: PrimaryExpr PredicateList  */
#line 418 "XPathGrammar.y"
    {
        std::unique_ptr<Vector<std::unique_ptr<WebCore::XPath::Expression>>> predicateList((yyvsp[0].expressionVector));
        (yyval.expression) = new WebCore::XPath::Filter(std::unique_ptr<WebCore::XPath::Expression>((yyvsp[-1].expression)), WTFMove(*predicateList));
    }
#line 1793 "XPathGrammar.cpp"
    break;

  case 51: /* OrExpr: OrExpr OR AndExpr  */
#line 428 "XPathGrammar.y"
    {
        (yyval.expression) = new WebCore::XPath::LogicalOp(WebCore::XPath::LogicalOp::OP_Or, std::unique_ptr<WebCore::XPath::Expression>((yyvsp[-2].expression)), std::unique_ptr<WebCore::XPath::Expression>((yyvsp[0].expression)));
    }
#line 1801 "XPathGrammar.cpp"
    break;

  case 53: /* AndExpr: AndExpr AND EqualityExpr  */
#line 437 "XPathGrammar.y"
    {
        (yyval.expression) = new WebCore::XPath::LogicalOp(WebCore::XPath::LogicalOp::OP_And, std::unique_ptr<WebCore::XPath::Expression>((yyvsp[-2].expression)), std::unique_ptr<WebCore::XPath::Expression>((yyvsp[0].expression)));
    }
#line 1809 "XPathGrammar.cpp"
    break;

  case 55: /* EqualityExpr: EqualityExpr EQOP RelationalExpr  */
#line 446 "XPathGrammar.y"
    {
        (yyval.expression) = new WebCore::XPath::EqTestOp((yyvsp[-1].equalityTestOpcode), std::unique_ptr<WebCore::XPath::Expression>((yyvsp[-2].expression)), std::unique_ptr<WebCore::XPath::Expression>((yyvsp[0].expression)));
    }
#line 1817 "XPathGrammar.cpp"
    break;

  case 57: /* RelationalExpr: RelationalExpr RELOP AdditiveExpr  */
#line 455 "XPathGrammar.y"
    {
        (yyval.expression) = new WebCore::XPath::EqTestOp((yyvsp[-1].equalityTestOpcode), std::unique_ptr<WebCore::XPath::Expression>((yyvsp[-2].expression)), std::unique_ptr<WebCore::XPath::Expression>((yyvsp[0].expression)));
    }
#line 1825 "XPathGrammar.cpp"
    break;

  case 59: /* AdditiveExpr: AdditiveExpr PLUS MultiplicativeExpr  */
#line 464 "XPathGrammar.y"
    {
        (yyval.expression) = new WebCore::XPath::NumericOp(WebCore::XPath::NumericOp::OP_Add, std::unique_ptr<WebCore::XPath::Expression>((yyvsp[-2].expression)), std::unique_ptr<WebCore::XPath::Expression>((yyvsp[0].expression)));
    }
#line 1833 "XPathGrammar.cpp"
    break;

  case 60: /* AdditiveExpr: AdditiveExpr MINUS MultiplicativeExpr  */
#line 469 "XPathGrammar.y"
    {
        (yyval.expression) = new WebCore::XPath::NumericOp(WebCore::XPath::NumericOp::OP_Sub, std::unique_ptr<WebCore::XPath::Expression>((yyvsp[-2].expression)), std::unique_ptr<WebCore::XPath::Expression>((yyvsp[0].expression)));
    }
#line 1841 "XPathGrammar.cpp"
    break;

  case 62: /* MultiplicativeExpr: MultiplicativeExpr MULOP UnaryExpr  */
#line 478 "XPathGrammar.y"
    {
        (yyval.expression) = new WebCore::XPath::NumericOp((yyvsp[-1].numericOpcode), std::unique_ptr<WebCore::XPath::Expression>((yyvsp[-2].expression)), std::unique_ptr<WebCore::XPath::Expression>((yyvsp[0].expression)));
    }
#line 1849 "XPathGrammar.cpp"
    break;

  case 64: /* UnaryExpr: MINUS UnaryExpr  */
#line 487 "XPathGrammar.y"
    {
        (yyval.expression) = new WebCore::XPath::Negative(std::unique_ptr<WebCore::XPath::Expression>((yyvsp[0].expression)));
    }
#line 1857 "XPathGrammar.cpp"
    break;


#line 1861 "XPathGrammar.cpp"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      yyerror (parser, YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
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

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;

  /* Do not reclaim the symbols of the rule whose action triggered
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
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
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
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, parser);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (parser, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, parser);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, parser);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 492 "XPathGrammar.y"

