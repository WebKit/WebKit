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
    SAMPLEREXTERNAL2DY2YEXT   = 346,
    IMAGE2D                   = 347,
    IIMAGE2D                  = 348,
    UIMAGE2D                  = 349,
    IMAGE3D                   = 350,
    IIMAGE3D                  = 351,
    UIMAGE3D                  = 352,
    IMAGE2DARRAY              = 353,
    IIMAGE2DARRAY             = 354,
    UIMAGE2DARRAY             = 355,
    IMAGECUBE                 = 356,
    IIMAGECUBE                = 357,
    UIMAGECUBE                = 358,
    ATOMICUINT                = 359,
    LAYOUT                    = 360,
    YUVCSCSTANDARDEXT         = 361,
    YUVCSCSTANDARDEXTCONSTANT = 362,
    IDENTIFIER                = 363,
    TYPE_NAME                 = 364,
    FLOATCONSTANT             = 365,
    INTCONSTANT               = 366,
    UINTCONSTANT              = 367,
    BOOLCONSTANT              = 368,
    FIELD_SELECTION           = 369,
    LEFT_OP                   = 370,
    RIGHT_OP                  = 371,
    INC_OP                    = 372,
    DEC_OP                    = 373,
    LE_OP                     = 374,
    GE_OP                     = 375,
    EQ_OP                     = 376,
    NE_OP                     = 377,
    AND_OP                    = 378,
    OR_OP                     = 379,
    XOR_OP                    = 380,
    MUL_ASSIGN                = 381,
    DIV_ASSIGN                = 382,
    ADD_ASSIGN                = 383,
    MOD_ASSIGN                = 384,
    LEFT_ASSIGN               = 385,
    RIGHT_ASSIGN              = 386,
    AND_ASSIGN                = 387,
    XOR_ASSIGN                = 388,
    OR_ASSIGN                 = 389,
    SUB_ASSIGN                = 390,
    LEFT_PAREN                = 391,
    RIGHT_PAREN               = 392,
    LEFT_BRACKET              = 393,
    RIGHT_BRACKET             = 394,
    LEFT_BRACE                = 395,
    RIGHT_BRACE               = 396,
    DOT                       = 397,
    COMMA                     = 398,
    COLON                     = 399,
    EQUAL                     = 400,
    SEMICOLON                 = 401,
    BANG                      = 402,
    DASH                      = 403,
    TILDE                     = 404,
    PLUS                      = 405,
    STAR                      = 406,
    SLASH                     = 407,
    PERCENT                   = 408,
    LEFT_ANGLE                = 409,
    RIGHT_ANGLE               = 410,
    VERTICAL_BAR              = 411,
    CARET                     = 412,
    AMPERSAND                 = 413,
    QUESTION                  = 414
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
