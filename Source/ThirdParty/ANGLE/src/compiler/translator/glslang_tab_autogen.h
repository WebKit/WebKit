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
    SAMPLE                    = 308,
    CENTROID                  = 309,
    FLAT                      = 310,
    SMOOTH                    = 311,
    NOPERSPECTIVE             = 312,
    READONLY                  = 313,
    WRITEONLY                 = 314,
    COHERENT                  = 315,
    RESTRICT                  = 316,
    VOLATILE                  = 317,
    SHARED                    = 318,
    STRUCT                    = 319,
    VOID_TYPE                 = 320,
    WHILE                     = 321,
    SAMPLER2D                 = 322,
    SAMPLERCUBE               = 323,
    SAMPLER_EXTERNAL_OES      = 324,
    SAMPLER2DRECT             = 325,
    SAMPLER2DARRAY            = 326,
    ISAMPLER2D                = 327,
    ISAMPLER3D                = 328,
    ISAMPLERCUBE              = 329,
    ISAMPLER2DARRAY           = 330,
    USAMPLER2D                = 331,
    USAMPLER3D                = 332,
    USAMPLERCUBE              = 333,
    USAMPLER2DARRAY           = 334,
    SAMPLER2DMS               = 335,
    ISAMPLER2DMS              = 336,
    USAMPLER2DMS              = 337,
    SAMPLER2DMSARRAY          = 338,
    ISAMPLER2DMSARRAY         = 339,
    USAMPLER2DMSARRAY         = 340,
    SAMPLER3D                 = 341,
    SAMPLER3DRECT             = 342,
    SAMPLER2DSHADOW           = 343,
    SAMPLERCUBESHADOW         = 344,
    SAMPLER2DARRAYSHADOW      = 345,
    SAMPLERVIDEOWEBGL         = 346,
    SAMPLERCUBEARRAYOES       = 347,
    SAMPLERCUBEARRAYSHADOWOES = 348,
    ISAMPLERCUBEARRAYOES      = 349,
    USAMPLERCUBEARRAYOES      = 350,
    SAMPLERCUBEARRAYEXT       = 351,
    SAMPLERCUBEARRAYSHADOWEXT = 352,
    ISAMPLERCUBEARRAYEXT      = 353,
    USAMPLERCUBEARRAYEXT      = 354,
    SAMPLERBUFFER             = 355,
    ISAMPLERBUFFER            = 356,
    USAMPLERBUFFER            = 357,
    SAMPLEREXTERNAL2DY2YEXT   = 358,
    IMAGE2D                   = 359,
    IIMAGE2D                  = 360,
    UIMAGE2D                  = 361,
    IMAGE3D                   = 362,
    IIMAGE3D                  = 363,
    UIMAGE3D                  = 364,
    IMAGE2DARRAY              = 365,
    IIMAGE2DARRAY             = 366,
    UIMAGE2DARRAY             = 367,
    IMAGECUBE                 = 368,
    IIMAGECUBE                = 369,
    UIMAGECUBE                = 370,
    IMAGECUBEARRAYOES         = 371,
    IIMAGECUBEARRAYOES        = 372,
    UIMAGECUBEARRAYOES        = 373,
    IMAGECUBEARRAYEXT         = 374,
    IIMAGECUBEARRAYEXT        = 375,
    UIMAGECUBEARRAYEXT        = 376,
    IMAGEBUFFER               = 377,
    IIMAGEBUFFER              = 378,
    UIMAGEBUFFER              = 379,
    ATOMICUINT                = 380,
    LAYOUT                    = 381,
    YUVCSCSTANDARDEXT         = 382,
    YUVCSCSTANDARDEXTCONSTANT = 383,
    IDENTIFIER                = 384,
    TYPE_NAME                 = 385,
    FLOATCONSTANT             = 386,
    INTCONSTANT               = 387,
    UINTCONSTANT              = 388,
    BOOLCONSTANT              = 389,
    FIELD_SELECTION           = 390,
    LEFT_OP                   = 391,
    RIGHT_OP                  = 392,
    INC_OP                    = 393,
    DEC_OP                    = 394,
    LE_OP                     = 395,
    GE_OP                     = 396,
    EQ_OP                     = 397,
    NE_OP                     = 398,
    AND_OP                    = 399,
    OR_OP                     = 400,
    XOR_OP                    = 401,
    MUL_ASSIGN                = 402,
    DIV_ASSIGN                = 403,
    ADD_ASSIGN                = 404,
    MOD_ASSIGN                = 405,
    LEFT_ASSIGN               = 406,
    RIGHT_ASSIGN              = 407,
    AND_ASSIGN                = 408,
    XOR_ASSIGN                = 409,
    OR_ASSIGN                 = 410,
    SUB_ASSIGN                = 411,
    LEFT_PAREN                = 412,
    RIGHT_PAREN               = 413,
    LEFT_BRACKET              = 414,
    RIGHT_BRACKET             = 415,
    LEFT_BRACE                = 416,
    RIGHT_BRACE               = 417,
    DOT                       = 418,
    COMMA                     = 419,
    COLON                     = 420,
    EQUAL                     = 421,
    SEMICOLON                 = 422,
    BANG                      = 423,
    DASH                      = 424,
    TILDE                     = 425,
    PLUS                      = 426,
    STAR                      = 427,
    SLASH                     = 428,
    PERCENT                   = 429,
    LEFT_ANGLE                = 430,
    RIGHT_ANGLE               = 431,
    VERTICAL_BAR              = 432,
    CARET                     = 433,
    AMPERSAND                 = 434,
    QUESTION                  = 435
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
