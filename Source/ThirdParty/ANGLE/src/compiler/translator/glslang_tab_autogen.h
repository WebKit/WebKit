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
    NOPERSPECTIVE             = 311,
    READONLY                  = 312,
    WRITEONLY                 = 313,
    COHERENT                  = 314,
    RESTRICT                  = 315,
    VOLATILE                  = 316,
    SHARED                    = 317,
    STRUCT                    = 318,
    VOID_TYPE                 = 319,
    WHILE                     = 320,
    SAMPLER2D                 = 321,
    SAMPLERCUBE               = 322,
    SAMPLER_EXTERNAL_OES      = 323,
    SAMPLER2DRECT             = 324,
    SAMPLER2DARRAY            = 325,
    ISAMPLER2D                = 326,
    ISAMPLER3D                = 327,
    ISAMPLERCUBE              = 328,
    ISAMPLER2DARRAY           = 329,
    USAMPLER2D                = 330,
    USAMPLER3D                = 331,
    USAMPLERCUBE              = 332,
    USAMPLER2DARRAY           = 333,
    SAMPLER2DMS               = 334,
    ISAMPLER2DMS              = 335,
    USAMPLER2DMS              = 336,
    SAMPLER2DMSARRAY          = 337,
    ISAMPLER2DMSARRAY         = 338,
    USAMPLER2DMSARRAY         = 339,
    SAMPLER3D                 = 340,
    SAMPLER3DRECT             = 341,
    SAMPLER2DSHADOW           = 342,
    SAMPLERCUBESHADOW         = 343,
    SAMPLER2DARRAYSHADOW      = 344,
    SAMPLERVIDEOWEBGL         = 345,
    SAMPLERCUBEARRAYOES       = 346,
    SAMPLERCUBEARRAYSHADOWOES = 347,
    ISAMPLERCUBEARRAYOES      = 348,
    USAMPLERCUBEARRAYOES      = 349,
    SAMPLERCUBEARRAYEXT       = 350,
    SAMPLERCUBEARRAYSHADOWEXT = 351,
    ISAMPLERCUBEARRAYEXT      = 352,
    USAMPLERCUBEARRAYEXT      = 353,
    SAMPLEREXTERNAL2DY2YEXT   = 354,
    IMAGE2D                   = 355,
    IIMAGE2D                  = 356,
    UIMAGE2D                  = 357,
    IMAGE3D                   = 358,
    IIMAGE3D                  = 359,
    UIMAGE3D                  = 360,
    IMAGE2DARRAY              = 361,
    IIMAGE2DARRAY             = 362,
    UIMAGE2DARRAY             = 363,
    IMAGECUBE                 = 364,
    IIMAGECUBE                = 365,
    UIMAGECUBE                = 366,
    IMAGECUBEARRAYOES         = 367,
    IIMAGECUBEARRAYOES        = 368,
    UIMAGECUBEARRAYOES        = 369,
    IMAGECUBEARRAYEXT         = 370,
    IIMAGECUBEARRAYEXT        = 371,
    UIMAGECUBEARRAYEXT        = 372,
    ATOMICUINT                = 373,
    LAYOUT                    = 374,
    YUVCSCSTANDARDEXT         = 375,
    YUVCSCSTANDARDEXTCONSTANT = 376,
    IDENTIFIER                = 377,
    TYPE_NAME                 = 378,
    FLOATCONSTANT             = 379,
    INTCONSTANT               = 380,
    UINTCONSTANT              = 381,
    BOOLCONSTANT              = 382,
    FIELD_SELECTION           = 383,
    LEFT_OP                   = 384,
    RIGHT_OP                  = 385,
    INC_OP                    = 386,
    DEC_OP                    = 387,
    LE_OP                     = 388,
    GE_OP                     = 389,
    EQ_OP                     = 390,
    NE_OP                     = 391,
    AND_OP                    = 392,
    OR_OP                     = 393,
    XOR_OP                    = 394,
    MUL_ASSIGN                = 395,
    DIV_ASSIGN                = 396,
    ADD_ASSIGN                = 397,
    MOD_ASSIGN                = 398,
    LEFT_ASSIGN               = 399,
    RIGHT_ASSIGN              = 400,
    AND_ASSIGN                = 401,
    XOR_ASSIGN                = 402,
    OR_ASSIGN                 = 403,
    SUB_ASSIGN                = 404,
    LEFT_PAREN                = 405,
    RIGHT_PAREN               = 406,
    LEFT_BRACKET              = 407,
    RIGHT_BRACKET             = 408,
    LEFT_BRACE                = 409,
    RIGHT_BRACE               = 410,
    DOT                       = 411,
    COMMA                     = 412,
    COLON                     = 413,
    EQUAL                     = 414,
    SEMICOLON                 = 415,
    BANG                      = 416,
    DASH                      = 417,
    TILDE                     = 418,
    PLUS                      = 419,
    STAR                      = 420,
    SLASH                     = 421,
    PERCENT                   = 422,
    LEFT_ANGLE                = 423,
    RIGHT_ANGLE               = 424,
    VERTICAL_BAR              = 425,
    CARET                     = 426,
    AMPERSAND                 = 427,
    QUESTION                  = 428
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
