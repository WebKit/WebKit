/* A Bison parser, made by GNU Bison 3.3.2.  */

/* Apple Note: For the avoidance of doubt, Apple elects to distribute this file under the terms of the BSD license. */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2019 Free Software Foundation,
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

/* Undocumented macros, especially those whose name start with YY_,
   are private implementation details.  Do not rely on them.  */

#ifndef YY_YY_GLSLANG_TAB_AUTOGEN_H_INCLUDED
#define YY_YY_GLSLANG_TAB_AUTOGEN_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
#    define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif
/* "%code requires" blocks.  */

#define YYLTYPE TSourceLoc
#define YYLTYPE_IS_DECLARED 1
#define YYLTYPE_IS_TRIVIAL 1

/* Token type.  */
#ifndef YYTOKENTYPE
#    define YYTOKENTYPE
enum yytokentype
{
    INVARIANT                 = 258,
    PRECISE                   = 259,
    HIGH_PRECISION            = 260,
    MEDIUM_PRECISION          = 261,
    LOW_PRECISION             = 262,
    PRECISION                 = 263,
    ATTRIBUTE                 = 264,
    CONST_QUAL                = 265,
    BOOL_TYPE                 = 266,
    FLOAT_TYPE                = 267,
    INT_TYPE                  = 268,
    UINT_TYPE                 = 269,
    BREAK                     = 270,
    CONTINUE                  = 271,
    DO                        = 272,
    ELSE                      = 273,
    FOR                       = 274,
    IF                        = 275,
    DISCARD                   = 276,
    RETURN                    = 277,
    SWITCH                    = 278,
    CASE                      = 279,
    DEFAULT                   = 280,
    BVEC2                     = 281,
    BVEC3                     = 282,
    BVEC4                     = 283,
    IVEC2                     = 284,
    IVEC3                     = 285,
    IVEC4                     = 286,
    VEC2                      = 287,
    VEC3                      = 288,
    VEC4                      = 289,
    UVEC2                     = 290,
    UVEC3                     = 291,
    UVEC4                     = 292,
    MATRIX2                   = 293,
    MATRIX3                   = 294,
    MATRIX4                   = 295,
    IN_QUAL                   = 296,
    OUT_QUAL                  = 297,
    INOUT_QUAL                = 298,
    UNIFORM                   = 299,
    BUFFER                    = 300,
    VARYING                   = 301,
    MATRIX2x3                 = 302,
    MATRIX3x2                 = 303,
    MATRIX2x4                 = 304,
    MATRIX4x2                 = 305,
    MATRIX3x4                 = 306,
    MATRIX4x3                 = 307,
    CENTROID                  = 308,
    FLAT                      = 309,
    SMOOTH                    = 310,
    READONLY                  = 311,
    WRITEONLY                 = 312,
    COHERENT                  = 313,
    RESTRICT                  = 314,
    VOLATILE                  = 315,
    SHARED                    = 316,
    STRUCT                    = 317,
    VOID_TYPE                 = 318,
    WHILE                     = 319,
    SAMPLER2D                 = 320,
    SAMPLERCUBE               = 321,
    SAMPLER_EXTERNAL_OES      = 322,
    SAMPLER2DRECT             = 323,
    SAMPLER2DARRAY            = 324,
    ISAMPLER2D                = 325,
    ISAMPLER3D                = 326,
    ISAMPLERCUBE              = 327,
    ISAMPLER2DARRAY           = 328,
    USAMPLER2D                = 329,
    USAMPLER3D                = 330,
    USAMPLERCUBE              = 331,
    USAMPLER2DARRAY           = 332,
    SAMPLER2DMS               = 333,
    ISAMPLER2DMS              = 334,
    USAMPLER2DMS              = 335,
    SAMPLER2DMSARRAY          = 336,
    ISAMPLER2DMSARRAY         = 337,
    USAMPLER2DMSARRAY         = 338,
    SAMPLER3D                 = 339,
    SAMPLER3DRECT             = 340,
    SAMPLER2DSHADOW           = 341,
    SAMPLERCUBESHADOW         = 342,
    SAMPLER2DARRAYSHADOW      = 343,
    SAMPLERVIDEOWEBGL         = 344,
    SAMPLEREXTERNAL2DY2YEXT   = 345,
    IMAGE2D                   = 346,
    IIMAGE2D                  = 347,
    UIMAGE2D                  = 348,
    IMAGE3D                   = 349,
    IIMAGE3D                  = 350,
    UIMAGE3D                  = 351,
    IMAGE2DARRAY              = 352,
    IIMAGE2DARRAY             = 353,
    UIMAGE2DARRAY             = 354,
    IMAGECUBE                 = 355,
    IIMAGECUBE                = 356,
    UIMAGECUBE                = 357,
    ATOMICUINT                = 358,
    LAYOUT                    = 359,
    YUVCSCSTANDARDEXT         = 360,
    YUVCSCSTANDARDEXTCONSTANT = 361,
    IDENTIFIER                = 362,
    TYPE_NAME                 = 363,
    FLOATCONSTANT             = 364,
    INTCONSTANT               = 365,
    UINTCONSTANT              = 366,
    BOOLCONSTANT              = 367,
    FIELD_SELECTION           = 368,
    LEFT_OP                   = 369,
    RIGHT_OP                  = 370,
    INC_OP                    = 371,
    DEC_OP                    = 372,
    LE_OP                     = 373,
    GE_OP                     = 374,
    EQ_OP                     = 375,
    NE_OP                     = 376,
    AND_OP                    = 377,
    OR_OP                     = 378,
    XOR_OP                    = 379,
    MUL_ASSIGN                = 380,
    DIV_ASSIGN                = 381,
    ADD_ASSIGN                = 382,
    MOD_ASSIGN                = 383,
    LEFT_ASSIGN               = 384,
    RIGHT_ASSIGN              = 385,
    AND_ASSIGN                = 386,
    XOR_ASSIGN                = 387,
    OR_ASSIGN                 = 388,
    SUB_ASSIGN                = 389,
    LEFT_PAREN                = 390,
    RIGHT_PAREN               = 391,
    LEFT_BRACKET              = 392,
    RIGHT_BRACKET             = 393,
    LEFT_BRACE                = 394,
    RIGHT_BRACE               = 395,
    DOT                       = 396,
    COMMA                     = 397,
    COLON                     = 398,
    EQUAL                     = 399,
    SEMICOLON                 = 400,
    BANG                      = 401,
    DASH                      = 402,
    TILDE                     = 403,
    PLUS                      = 404,
    STAR                      = 405,
    SLASH                     = 406,
    PERCENT                   = 407,
    LEFT_ANGLE                = 408,
    RIGHT_ANGLE               = 409,
    VERTICAL_BAR              = 410,
    CARET                     = 411,
    AMPERSAND                 = 412,
    QUESTION                  = 413
};
#endif

/* Value type.  */
#if !defined YYSTYPE && !defined YYSTYPE_IS_DECLARED

union YYSTYPE
{

    struct
    {
        union
        {
            const char *string;  // pool allocated.
            float f;
            int i;
            unsigned int u;
            bool b;
        };
        const TSymbol *symbol;
    } lex;
    struct
    {
        TOperator op;
        union
        {
            TIntermNode *intermNode;
            TIntermNodePair nodePair;
            TIntermTyped *intermTypedNode;
            TIntermAggregate *intermAggregate;
            TIntermBlock *intermBlock;
            TIntermDeclaration *intermDeclaration;
            TIntermFunctionPrototype *intermFunctionPrototype;
            TIntermSwitch *intermSwitch;
            TIntermCase *intermCase;
        };
        union
        {
            TVector<unsigned int> *arraySizes;
            TTypeSpecifierNonArray typeSpecifierNonArray;
            TPublicType type;
            TPrecision precision;
            TLayoutQualifier layoutQualifier;
            TQualifier qualifier;
            TFunction *function;
            TFunctionLookup *functionLookup;
            TParameter param;
            TDeclarator *declarator;
            TDeclaratorList *declaratorList;
            TFieldList *fieldList;
            TQualifierWrapperBase *qualifierWrapper;
            TTypeQualifierBuilder *typeQualifierBuilder;
        };
    } interm;
};

typedef union YYSTYPE YYSTYPE;
#    define YYSTYPE_IS_TRIVIAL 1
#    define YYSTYPE_IS_DECLARED 1
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
#    define YYLTYPE_IS_DECLARED 1
#    define YYLTYPE_IS_TRIVIAL 1
#endif

int yyparse(TParseContext *context, void *scanner);

#endif /* !YY_YY_GLSLANG_TAB_AUTOGEN_H_INCLUDED  */
