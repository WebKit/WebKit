/* Apple Note: For the avoidance of doubt, Apple elects to distribute this file under the terms of
 * the BSD license. */

/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

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

/* Token kinds.  */
#ifndef YYTOKENTYPE
#    define YYTOKENTYPE
enum yytokentype
{
    YYEMPTY                   = -2,
    YYEOF                     = 0,   /* "end of file"  */
    YYerror                   = 256, /* error  */
    YYUNDEF                   = 257, /* "invalid token"  */
    INVARIANT                 = 258, /* INVARIANT  */
    PRECISE                   = 259, /* PRECISE  */
    HIGH_PRECISION            = 260, /* HIGH_PRECISION  */
    MEDIUM_PRECISION          = 261, /* MEDIUM_PRECISION  */
    LOW_PRECISION             = 262, /* LOW_PRECISION  */
    PRECISION                 = 263, /* PRECISION  */
    ATTRIBUTE                 = 264, /* ATTRIBUTE  */
    CONST_QUAL                = 265, /* CONST_QUAL  */
    BOOL_TYPE                 = 266, /* BOOL_TYPE  */
    FLOAT_TYPE                = 267, /* FLOAT_TYPE  */
    INT_TYPE                  = 268, /* INT_TYPE  */
    UINT_TYPE                 = 269, /* UINT_TYPE  */
    BREAK                     = 270, /* BREAK  */
    CONTINUE                  = 271, /* CONTINUE  */
    DO                        = 272, /* DO  */
    ELSE                      = 273, /* ELSE  */
    FOR                       = 274, /* FOR  */
    IF                        = 275, /* IF  */
    DISCARD                   = 276, /* DISCARD  */
    RETURN                    = 277, /* RETURN  */
    SWITCH                    = 278, /* SWITCH  */
    CASE                      = 279, /* CASE  */
    DEFAULT                   = 280, /* DEFAULT  */
    BVEC2                     = 281, /* BVEC2  */
    BVEC3                     = 282, /* BVEC3  */
    BVEC4                     = 283, /* BVEC4  */
    IVEC2                     = 284, /* IVEC2  */
    IVEC3                     = 285, /* IVEC3  */
    IVEC4                     = 286, /* IVEC4  */
    VEC2                      = 287, /* VEC2  */
    VEC3                      = 288, /* VEC3  */
    VEC4                      = 289, /* VEC4  */
    UVEC2                     = 290, /* UVEC2  */
    UVEC3                     = 291, /* UVEC3  */
    UVEC4                     = 292, /* UVEC4  */
    MATRIX2                   = 293, /* MATRIX2  */
    MATRIX3                   = 294, /* MATRIX3  */
    MATRIX4                   = 295, /* MATRIX4  */
    IN_QUAL                   = 296, /* IN_QUAL  */
    OUT_QUAL                  = 297, /* OUT_QUAL  */
    INOUT_QUAL                = 298, /* INOUT_QUAL  */
    UNIFORM                   = 299, /* UNIFORM  */
    BUFFER                    = 300, /* BUFFER  */
    VARYING                   = 301, /* VARYING  */
    MATRIX2x3                 = 302, /* MATRIX2x3  */
    MATRIX3x2                 = 303, /* MATRIX3x2  */
    MATRIX2x4                 = 304, /* MATRIX2x4  */
    MATRIX4x2                 = 305, /* MATRIX4x2  */
    MATRIX3x4                 = 306, /* MATRIX3x4  */
    MATRIX4x3                 = 307, /* MATRIX4x3  */
    SAMPLE                    = 308, /* SAMPLE  */
    CENTROID                  = 309, /* CENTROID  */
    FLAT                      = 310, /* FLAT  */
    SMOOTH                    = 311, /* SMOOTH  */
    NOPERSPECTIVE             = 312, /* NOPERSPECTIVE  */
    PATCH                     = 313, /* PATCH  */
    READONLY                  = 314, /* READONLY  */
    WRITEONLY                 = 315, /* WRITEONLY  */
    COHERENT                  = 316, /* COHERENT  */
    RESTRICT                  = 317, /* RESTRICT  */
    VOLATILE                  = 318, /* VOLATILE  */
    SHARED                    = 319, /* SHARED  */
    STRUCT                    = 320, /* STRUCT  */
    VOID_TYPE                 = 321, /* VOID_TYPE  */
    WHILE                     = 322, /* WHILE  */
    SAMPLER2D                 = 323, /* SAMPLER2D  */
    SAMPLERCUBE               = 324, /* SAMPLERCUBE  */
    SAMPLER_EXTERNAL_OES      = 325, /* SAMPLER_EXTERNAL_OES  */
    SAMPLER2DRECT             = 326, /* SAMPLER2DRECT  */
    SAMPLER2DARRAY            = 327, /* SAMPLER2DARRAY  */
    ISAMPLER2D                = 328, /* ISAMPLER2D  */
    ISAMPLER3D                = 329, /* ISAMPLER3D  */
    ISAMPLERCUBE              = 330, /* ISAMPLERCUBE  */
    ISAMPLER2DARRAY           = 331, /* ISAMPLER2DARRAY  */
    USAMPLER2D                = 332, /* USAMPLER2D  */
    USAMPLER3D                = 333, /* USAMPLER3D  */
    USAMPLERCUBE              = 334, /* USAMPLERCUBE  */
    USAMPLER2DARRAY           = 335, /* USAMPLER2DARRAY  */
    SAMPLER2DMS               = 336, /* SAMPLER2DMS  */
    ISAMPLER2DMS              = 337, /* ISAMPLER2DMS  */
    USAMPLER2DMS              = 338, /* USAMPLER2DMS  */
    SAMPLER2DMSARRAY          = 339, /* SAMPLER2DMSARRAY  */
    ISAMPLER2DMSARRAY         = 340, /* ISAMPLER2DMSARRAY  */
    USAMPLER2DMSARRAY         = 341, /* USAMPLER2DMSARRAY  */
    SAMPLER3D                 = 342, /* SAMPLER3D  */
    SAMPLER3DRECT             = 343, /* SAMPLER3DRECT  */
    SAMPLER2DSHADOW           = 344, /* SAMPLER2DSHADOW  */
    SAMPLERCUBESHADOW         = 345, /* SAMPLERCUBESHADOW  */
    SAMPLER2DARRAYSHADOW      = 346, /* SAMPLER2DARRAYSHADOW  */
    SAMPLERCUBEARRAYOES       = 347, /* SAMPLERCUBEARRAYOES  */
    SAMPLERCUBEARRAYSHADOWOES = 348, /* SAMPLERCUBEARRAYSHADOWOES  */
    ISAMPLERCUBEARRAYOES      = 349, /* ISAMPLERCUBEARRAYOES  */
    USAMPLERCUBEARRAYOES      = 350, /* USAMPLERCUBEARRAYOES  */
    SAMPLERCUBEARRAYEXT       = 351, /* SAMPLERCUBEARRAYEXT  */
    SAMPLERCUBEARRAYSHADOWEXT = 352, /* SAMPLERCUBEARRAYSHADOWEXT  */
    ISAMPLERCUBEARRAYEXT      = 353, /* ISAMPLERCUBEARRAYEXT  */
    USAMPLERCUBEARRAYEXT      = 354, /* USAMPLERCUBEARRAYEXT  */
    SAMPLERBUFFER             = 355, /* SAMPLERBUFFER  */
    ISAMPLERBUFFER            = 356, /* ISAMPLERBUFFER  */
    USAMPLERBUFFER            = 357, /* USAMPLERBUFFER  */
    SAMPLEREXTERNAL2DY2YEXT   = 358, /* SAMPLEREXTERNAL2DY2YEXT  */
    IMAGE2D                   = 359, /* IMAGE2D  */
    IIMAGE2D                  = 360, /* IIMAGE2D  */
    UIMAGE2D                  = 361, /* UIMAGE2D  */
    IMAGE3D                   = 362, /* IMAGE3D  */
    IIMAGE3D                  = 363, /* IIMAGE3D  */
    UIMAGE3D                  = 364, /* UIMAGE3D  */
    IMAGE2DARRAY              = 365, /* IMAGE2DARRAY  */
    IIMAGE2DARRAY             = 366, /* IIMAGE2DARRAY  */
    UIMAGE2DARRAY             = 367, /* UIMAGE2DARRAY  */
    IMAGECUBE                 = 368, /* IMAGECUBE  */
    IIMAGECUBE                = 369, /* IIMAGECUBE  */
    UIMAGECUBE                = 370, /* UIMAGECUBE  */
    IMAGECUBEARRAYOES         = 371, /* IMAGECUBEARRAYOES  */
    IIMAGECUBEARRAYOES        = 372, /* IIMAGECUBEARRAYOES  */
    UIMAGECUBEARRAYOES        = 373, /* UIMAGECUBEARRAYOES  */
    IMAGECUBEARRAYEXT         = 374, /* IMAGECUBEARRAYEXT  */
    IIMAGECUBEARRAYEXT        = 375, /* IIMAGECUBEARRAYEXT  */
    UIMAGECUBEARRAYEXT        = 376, /* UIMAGECUBEARRAYEXT  */
    IMAGEBUFFER               = 377, /* IMAGEBUFFER  */
    IIMAGEBUFFER              = 378, /* IIMAGEBUFFER  */
    UIMAGEBUFFER              = 379, /* UIMAGEBUFFER  */
    ATOMICUINT                = 380, /* ATOMICUINT  */
    PIXELLOCALANGLE           = 381, /* PIXELLOCALANGLE  */
    IPIXELLOCALANGLE          = 382, /* IPIXELLOCALANGLE  */
    UPIXELLOCALANGLE          = 383, /* UPIXELLOCALANGLE  */
    LAYOUT                    = 384, /* LAYOUT  */
    YUVCSCSTANDARDEXT         = 385, /* YUVCSCSTANDARDEXT  */
    YUVCSCSTANDARDEXTCONSTANT = 386, /* YUVCSCSTANDARDEXTCONSTANT  */
    IDENTIFIER                = 387, /* IDENTIFIER  */
    TYPE_NAME                 = 388, /* TYPE_NAME  */
    FLOATCONSTANT             = 389, /* FLOATCONSTANT  */
    INTCONSTANT               = 390, /* INTCONSTANT  */
    UINTCONSTANT              = 391, /* UINTCONSTANT  */
    BOOLCONSTANT              = 392, /* BOOLCONSTANT  */
    FIELD_SELECTION           = 393, /* FIELD_SELECTION  */
    LEFT_OP                   = 394, /* LEFT_OP  */
    RIGHT_OP                  = 395, /* RIGHT_OP  */
    INC_OP                    = 396, /* INC_OP  */
    DEC_OP                    = 397, /* DEC_OP  */
    LE_OP                     = 398, /* LE_OP  */
    GE_OP                     = 399, /* GE_OP  */
    EQ_OP                     = 400, /* EQ_OP  */
    NE_OP                     = 401, /* NE_OP  */
    AND_OP                    = 402, /* AND_OP  */
    OR_OP                     = 403, /* OR_OP  */
    XOR_OP                    = 404, /* XOR_OP  */
    MUL_ASSIGN                = 405, /* MUL_ASSIGN  */
    DIV_ASSIGN                = 406, /* DIV_ASSIGN  */
    ADD_ASSIGN                = 407, /* ADD_ASSIGN  */
    MOD_ASSIGN                = 408, /* MOD_ASSIGN  */
    LEFT_ASSIGN               = 409, /* LEFT_ASSIGN  */
    RIGHT_ASSIGN              = 410, /* RIGHT_ASSIGN  */
    AND_ASSIGN                = 411, /* AND_ASSIGN  */
    XOR_ASSIGN                = 412, /* XOR_ASSIGN  */
    OR_ASSIGN                 = 413, /* OR_ASSIGN  */
    SUB_ASSIGN                = 414, /* SUB_ASSIGN  */
    LEFT_PAREN                = 415, /* LEFT_PAREN  */
    RIGHT_PAREN               = 416, /* RIGHT_PAREN  */
    LEFT_BRACKET              = 417, /* LEFT_BRACKET  */
    RIGHT_BRACKET             = 418, /* RIGHT_BRACKET  */
    LEFT_BRACE                = 419, /* LEFT_BRACE  */
    RIGHT_BRACE               = 420, /* RIGHT_BRACE  */
    DOT                       = 421, /* DOT  */
    COMMA                     = 422, /* COMMA  */
    COLON                     = 423, /* COLON  */
    EQUAL                     = 424, /* EQUAL  */
    SEMICOLON                 = 425, /* SEMICOLON  */
    BANG                      = 426, /* BANG  */
    DASH                      = 427, /* DASH  */
    TILDE                     = 428, /* TILDE  */
    PLUS                      = 429, /* PLUS  */
    STAR                      = 430, /* STAR  */
    SLASH                     = 431, /* SLASH  */
    PERCENT                   = 432, /* PERCENT  */
    LEFT_ANGLE                = 433, /* LEFT_ANGLE  */
    RIGHT_ANGLE               = 434, /* RIGHT_ANGLE  */
    VERTICAL_BAR              = 435, /* VERTICAL_BAR  */
    CARET                     = 436, /* CARET  */
    AMPERSAND                 = 437, /* AMPERSAND  */
    QUESTION                  = 438  /* QUESTION  */
};
typedef enum yytokentype yytoken_kind_t;
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
