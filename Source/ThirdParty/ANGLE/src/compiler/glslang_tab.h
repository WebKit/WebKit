/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C

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
/* Line 1529 of yacc.c.  */

	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



