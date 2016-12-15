#ifndef CSSGRAMMAR_H
#define CSSGRAMMAR_H
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
     MULOP = 258,
     RELOP = 259,
     EQOP = 260,
     MINUS = 261,
     PLUS = 262,
     AND = 263,
     OR = 264,
     FUNCTIONNAME = 265,
     LITERAL = 266,
     NAMETEST = 267,
     NUMBER = 268,
     NODETYPE = 269,
     VARIABLEREFERENCE = 270,
     AXISNAME = 271,
     COMMENT = 272,
     DOTDOT = 273,
     PI = 274,
     NODE = 275,
     SLASHSLASH = 276,
     TEXT_ = 277,
     XPATH_ERROR = 278
   };
#endif
/* Tokens.  */
#define MULOP 258
#define RELOP 259
#define EQOP 260
#define MINUS 261
#define PLUS 262
#define AND 263
#define OR 264
#define FUNCTIONNAME 265
#define LITERAL 266
#define NAMETEST 267
#define NUMBER 268
#define NODETYPE 269
#define VARIABLEREFERENCE 270
#define AXISNAME 271
#define COMMENT 272
#define DOTDOT 273
#define PI 274
#define NODE 275
#define SLASHSLASH 276
#define TEXT_ 277
#define XPATH_ERROR 278




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 60 "WebCore/xml/XPathGrammar.y"
{ 
    NumericOp::Opcode numericOpcode; 
    EqTestOp::Opcode equalityTestOpcode;
    StringImpl* string;
    Step::Axis axis;
    LocationPath* locationPath;
    Step::NodeTest* nodeTest;
    Vector<std::unique_ptr<Expression>>* expressionVector;
    Step* step;
    Expression* expression; 
}
/* Line 1529 of yacc.c.  */
#line 107 "./XPathGrammar.hpp"
    YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



#endif
