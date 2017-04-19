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
#define YY_YY_GLSLANG_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif
/* "%code requires" blocks.  */

#define YYLTYPE TSourceLoc
#define YYLTYPE_IS_DECLARED 1

/* Token type.  */
#ifndef YYTOKENTYPE
#define YYTOKENTYPE
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
    VARYING                   = 299,
    MATRIX2x3                 = 300,
    MATRIX3x2                 = 301,
    MATRIX2x4                 = 302,
    MATRIX4x2                 = 303,
    MATRIX3x4                 = 304,
    MATRIX4x3                 = 305,
    CENTROID                  = 306,
    FLAT                      = 307,
    SMOOTH                    = 308,
    READONLY                  = 309,
    WRITEONLY                 = 310,
    COHERENT                  = 311,
    RESTRICT                  = 312,
    VOLATILE                  = 313,
    SHARED                    = 314,
    STRUCT                    = 315,
    VOID_TYPE                 = 316,
    WHILE                     = 317,
    SAMPLER2D                 = 318,
    SAMPLERCUBE               = 319,
    SAMPLER_EXTERNAL_OES      = 320,
    SAMPLER2DRECT             = 321,
    SAMPLER2DARRAY            = 322,
    ISAMPLER2D                = 323,
    ISAMPLER3D                = 324,
    ISAMPLERCUBE              = 325,
    ISAMPLER2DARRAY           = 326,
    USAMPLER2D                = 327,
    USAMPLER3D                = 328,
    USAMPLERCUBE              = 329,
    USAMPLER2DARRAY           = 330,
    SAMPLER2DMS               = 331,
    ISAMPLER2DMS              = 332,
    USAMPLER2DMS              = 333,
    SAMPLER3D                 = 334,
    SAMPLER3DRECT             = 335,
    SAMPLER2DSHADOW           = 336,
    SAMPLERCUBESHADOW         = 337,
    SAMPLER2DARRAYSHADOW      = 338,
    SAMPLEREXTERNAL2DY2YEXT   = 339,
    IMAGE2D                   = 340,
    IIMAGE2D                  = 341,
    UIMAGE2D                  = 342,
    IMAGE3D                   = 343,
    IIMAGE3D                  = 344,
    UIMAGE3D                  = 345,
    IMAGE2DARRAY              = 346,
    IIMAGE2DARRAY             = 347,
    UIMAGE2DARRAY             = 348,
    IMAGECUBE                 = 349,
    IIMAGECUBE                = 350,
    UIMAGECUBE                = 351,
    LAYOUT                    = 352,
    YUVCSCSTANDARDEXT         = 353,
    YUVCSCSTANDARDEXTCONSTANT = 354,
    IDENTIFIER                = 355,
    TYPE_NAME                 = 356,
    FLOATCONSTANT             = 357,
    INTCONSTANT               = 358,
    UINTCONSTANT              = 359,
    BOOLCONSTANT              = 360,
    FIELD_SELECTION           = 361,
    LEFT_OP                   = 362,
    RIGHT_OP                  = 363,
    INC_OP                    = 364,
    DEC_OP                    = 365,
    LE_OP                     = 366,
    GE_OP                     = 367,
    EQ_OP                     = 368,
    NE_OP                     = 369,
    AND_OP                    = 370,
    OR_OP                     = 371,
    XOR_OP                    = 372,
    MUL_ASSIGN                = 373,
    DIV_ASSIGN                = 374,
    ADD_ASSIGN                = 375,
    MOD_ASSIGN                = 376,
    LEFT_ASSIGN               = 377,
    RIGHT_ASSIGN              = 378,
    AND_ASSIGN                = 379,
    XOR_ASSIGN                = 380,
    OR_ASSIGN                 = 381,
    SUB_ASSIGN                = 382,
    LEFT_PAREN                = 383,
    RIGHT_PAREN               = 384,
    LEFT_BRACKET              = 385,
    RIGHT_BRACKET             = 386,
    LEFT_BRACE                = 387,
    RIGHT_BRACE               = 388,
    DOT                       = 389,
    COMMA                     = 390,
    COLON                     = 391,
    EQUAL                     = 392,
    SEMICOLON                 = 393,
    BANG                      = 394,
    DASH                      = 395,
    TILDE                     = 396,
    PLUS                      = 397,
    STAR                      = 398,
    SLASH                     = 399,
    PERCENT                   = 400,
    LEFT_ANGLE                = 401,
    RIGHT_ANGLE               = 402,
    VERTICAL_BAR              = 403,
    CARET                     = 404,
    AMPERSAND                 = 405,
    QUESTION                  = 406
};
#endif

/* Value type.  */
#if !defined YYSTYPE && !defined YYSTYPE_IS_DECLARED

union YYSTYPE {

    struct
    {
        union {
            TString *string;
            float f;
            int i;
            unsigned int u;
            bool b;
        };
        TSymbol *symbol;
    } lex;
    struct
    {
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
#define YYSTYPE_IS_TRIVIAL 1
#define YYSTYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if !defined YYLTYPE && !defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE YYLTYPE;
struct YYLTYPE
{
    int first_line;
    int first_column;
    int last_line;
    int last_column;
};
#define YYLTYPE_IS_DECLARED 1
#define YYLTYPE_IS_TRIVIAL 1
#endif

int yyparse(TParseContext *context, void *scanner);

#endif /* !YY_YY_GLSLANG_TAB_H_INCLUDED  */
