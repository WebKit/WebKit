/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Apple Note: For the avoidance of doubt, Apple elects to distribute this file under the terms of the BSD license. */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

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

#ifndef YY_YY_GLSLANG_TAB_H_INCLUDED
# define YY_YY_GLSLANG_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif
/* "%code requires" blocks.  */


#define YYLTYPE TSourceLoc
#define YYLTYPE_IS_DECLARED 1



/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
enum yytokentype
{
    INVARIANT                 = 258,
    HIGH_PRECISION            = 259,
    MEDIUM_PRECISION          = 260,
    LOW_PRECISION             = 261,
    PRECISION                 = 262,
    ATTRIBUTE                 = 263,
    CONST_QUAL                = 264,
    BOOL_TYPE                 = 265,
    FLOAT_TYPE                = 266,
    INT_TYPE                  = 267,
    UINT_TYPE                 = 268,
    BREAK                     = 269,
    CONTINUE                  = 270,
    DO                        = 271,
    ELSE                      = 272,
    FOR                       = 273,
    IF                        = 274,
    DISCARD                   = 275,
    RETURN                    = 276,
    SWITCH                    = 277,
    CASE                      = 278,
    DEFAULT                   = 279,
    BVEC2                     = 280,
    BVEC3                     = 281,
    BVEC4                     = 282,
    IVEC2                     = 283,
    IVEC3                     = 284,
    IVEC4                     = 285,
    VEC2                      = 286,
    VEC3                      = 287,
    VEC4                      = 288,
    UVEC2                     = 289,
    UVEC3                     = 290,
    UVEC4                     = 291,
    MATRIX2                   = 292,
    MATRIX3                   = 293,
    MATRIX4                   = 294,
    IN_QUAL                   = 295,
    OUT_QUAL                  = 296,
    INOUT_QUAL                = 297,
    UNIFORM                   = 298,
    BUFFER                    = 299,
    VARYING                   = 300,
    MATRIX2x3                 = 301,
    MATRIX3x2                 = 302,
    MATRIX2x4                 = 303,
    MATRIX4x2                 = 304,
    MATRIX3x4                 = 305,
    MATRIX4x3                 = 306,
    CENTROID                  = 307,
    FLAT                      = 308,
    SMOOTH                    = 309,
    READONLY                  = 310,
    WRITEONLY                 = 311,
    COHERENT                  = 312,
    RESTRICT                  = 313,
    VOLATILE                  = 314,
    SHARED                    = 315,
    STRUCT                    = 316,
    VOID_TYPE                 = 317,
    WHILE                     = 318,
    SAMPLER2D                 = 319,
    SAMPLERCUBE               = 320,
    SAMPLER_EXTERNAL_OES      = 321,
    SAMPLER2DRECT             = 322,
    SAMPLER2DARRAY            = 323,
    ISAMPLER2D                = 324,
    ISAMPLER3D                = 325,
    ISAMPLERCUBE              = 326,
    ISAMPLER2DARRAY           = 327,
    USAMPLER2D                = 328,
    USAMPLER3D                = 329,
    USAMPLERCUBE              = 330,
    USAMPLER2DARRAY           = 331,
    SAMPLER2DMS               = 332,
    ISAMPLER2DMS              = 333,
    USAMPLER2DMS              = 334,
    SAMPLER3D                 = 335,
    SAMPLER3DRECT             = 336,
    SAMPLER2DSHADOW           = 337,
    SAMPLERCUBESHADOW         = 338,
    SAMPLER2DARRAYSHADOW      = 339,
    SAMPLEREXTERNAL2DY2YEXT   = 340,
    IMAGE2D                   = 341,
    IIMAGE2D                  = 342,
    UIMAGE2D                  = 343,
    IMAGE3D                   = 344,
    IIMAGE3D                  = 345,
    UIMAGE3D                  = 346,
    IMAGE2DARRAY              = 347,
    IIMAGE2DARRAY             = 348,
    UIMAGE2DARRAY             = 349,
    IMAGECUBE                 = 350,
    IIMAGECUBE                = 351,
    UIMAGECUBE                = 352,
    ATOMICUINT                = 353,
    LAYOUT                    = 354,
    YUVCSCSTANDARDEXT         = 355,
    YUVCSCSTANDARDEXTCONSTANT = 356,
    IDENTIFIER                = 357,
    TYPE_NAME                 = 358,
    FLOATCONSTANT             = 359,
    INTCONSTANT               = 360,
    UINTCONSTANT              = 361,
    BOOLCONSTANT              = 362,
    FIELD_SELECTION           = 363,
    LEFT_OP                   = 364,
    RIGHT_OP                  = 365,
    INC_OP                    = 366,
    DEC_OP                    = 367,
    LE_OP                     = 368,
    GE_OP                     = 369,
    EQ_OP                     = 370,
    NE_OP                     = 371,
    AND_OP                    = 372,
    OR_OP                     = 373,
    XOR_OP                    = 374,
    MUL_ASSIGN                = 375,
    DIV_ASSIGN                = 376,
    ADD_ASSIGN                = 377,
    MOD_ASSIGN                = 378,
    LEFT_ASSIGN               = 379,
    RIGHT_ASSIGN              = 380,
    AND_ASSIGN                = 381,
    XOR_ASSIGN                = 382,
    OR_ASSIGN                 = 383,
    SUB_ASSIGN                = 384,
    LEFT_PAREN                = 385,
    RIGHT_PAREN               = 386,
    LEFT_BRACKET              = 387,
    RIGHT_BRACKET             = 388,
    LEFT_BRACE                = 389,
    RIGHT_BRACE               = 390,
    DOT                       = 391,
    COMMA                     = 392,
    COLON                     = 393,
    EQUAL                     = 394,
    SEMICOLON                 = 395,
    BANG                      = 396,
    DASH                      = 397,
    TILDE                     = 398,
    PLUS                      = 399,
    STAR                      = 400,
    SLASH                     = 401,
    PERCENT                   = 402,
    LEFT_ANGLE                = 403,
    RIGHT_ANGLE               = 404,
    VERTICAL_BAR              = 405,
    CARET                     = 406,
    AMPERSAND                 = 407,
    QUESTION                  = 408
};
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{


    struct {
        union {
            TString *string;
            float f;
            int i;
            unsigned int u;
            bool b;
        };
        TSymbol* symbol;
    } lex;
    struct {
        TOperator op;
        union {
            TIntermNode *intermNode;
            TIntermNodePair nodePair;
            TIntermFunctionCallOrMethod callOrMethodPair;
            TIntermTyped *intermTypedNode;
            TIntermAggregate *intermAggregate;
            TIntermBlock *intermBlock;
            TIntermDeclaration *intermDeclaration;
            TIntermFunctionPrototype *intermFunctionPrototype;
            TIntermSwitch *intermSwitch;
            TIntermCase *intermCase;
        };
        union {
            TVector<unsigned int> *arraySizes;
            TTypeSpecifierNonArray typeSpecifierNonArray;
            TPublicType type;
            TPrecision precision;
            TLayoutQualifier layoutQualifier;
            TQualifier qualifier;
            TFunction *function;
            TParameter param;
            TField *field;
            TFieldList *fieldList;
            TQualifierWrapperBase *qualifierWrapper;
            TTypeQualifierBuilder *typeQualifierBuilder;
        };
    } interm;


};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE YYLTYPE;
struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif



int yyparse (TParseContext* context, void *scanner);

#endif /* !YY_YY_GLSLANG_TAB_H_INCLUDED  */
