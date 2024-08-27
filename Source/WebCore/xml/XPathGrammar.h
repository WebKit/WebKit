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

#ifndef YY_XPATHYY_XPATHGRAMMAR_HPP_INCLUDED
# define YY_XPATHYY_XPATHGRAMMAR_HPP_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int xpathyydebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    MULOP = 258,                   /* MULOP  */
    EQOP = 259,                    /* EQOP  */
    RELOP = 260,                   /* RELOP  */
    PLUS = 261,                    /* PLUS  */
    MINUS = 262,                   /* MINUS  */
    OR = 263,                      /* OR  */
    AND = 264,                     /* AND  */
    FUNCTIONNAME = 265,            /* FUNCTIONNAME  */
    LITERAL = 266,                 /* LITERAL  */
    NAMETEST = 267,                /* NAMETEST  */
    NUMBER = 268,                  /* NUMBER  */
    NODETYPE = 269,                /* NODETYPE  */
    VARIABLEREFERENCE = 270,       /* VARIABLEREFERENCE  */
    AXISNAME = 271,                /* AXISNAME  */
    COMMENT = 272,                 /* COMMENT  */
    DOTDOT = 273,                  /* DOTDOT  */
    PI = 274,                      /* PI  */
    NODE = 275,                    /* NODE  */
    SLASHSLASH = 276,              /* SLASHSLASH  */
    TEXT_ = 277,                   /* TEXT_  */
    XPATH_ERROR = 278              /* XPATH_ERROR  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 57 "XPathGrammar.y"
 
    WebCore::XPath::NumericOp::Opcode numericOpcode;
    WebCore::XPath::EqTestOp::Opcode equalityTestOpcode;
    StringImpl* string;
    WebCore::XPath::Step::Axis axis;
    WebCore::XPath::LocationPath* locationPath;
    WebCore::XPath::Step::NodeTest* nodeTest;
    Vector<std::unique_ptr<WebCore::XPath::Expression>>* expressionVector;
    WebCore::XPath::Step* step;
    WebCore::XPath::Expression* expression;

#line 99 "XPathGrammar.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif




int xpathyyparse (WebCore::XPath::Parser& parser);


#endif /* !YY_XPATHYY_XPATHGRAMMAR_HPP_INCLUDED  */
