#!/usr/bin/env python3

# Copyright (C) 2023 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

import os
import re
from pathlib import Path

try:
    import ply.lex as lex
    import ply.yacc as yacc
except ImportError:
    import sys
    sys.exit('Please run `pip3 install ply`');


# --- Parse Tree

class Comment(str):
    pass


class Variable(str):
    pass


class Stmt:
    def __init__(self, type, children=None):
        self.type = type
        if children:
            self.children = children
        else:
            self.children = []

    def __getitem__(self, index):
        out = self.children[index]
        return Stmt(self.type, children=out) if isinstance(index, slice) else out

    def __len__(self):
        return len(self.children)

    def __repr__(self):
        return f'({self.type})'


class Expr:
    def __init__(self, type, *children):
        self.type = type
        self.children = children

    def __getitem__(self, index):
        out = self.children[index]
        return Expr(self.type, children=out) if isinstance(index, slice) else out

    def __len__(self):
        return len(self.children)

    def __repr__(self):
        return f'({self.type}' + ' '.join([f'{child}' for child in self.children]) + ')'


# --- Parser

class Parser:
    '''
    Base class for a lexer/parser that has the rules defined as methods
    '''
    tokens = []
    precedence = ()

    def __init__(self, **kw):
        self.debug = kw.get('debug', None)
        self.names = {}
        try:
            modname = os.path.split(os.path.splitext(__file__)[0])[1]
        except:
            modname = 'parser'
        modname += '_' + self.__class__.__name__
        self.debugfile = modname + '.dbg'

        # Build the lexer and parser
        lex.lex(module=self, debug=self.debug)
        yacc.yacc(module=self, debug=self.debug, debugfile=self.debugfile)

    def parse(self, input):
        return yacc.parse(input)

class GnParser(Parser):
    reserved = { 'if': 'IF', 'else': 'ELSE', 'true': 'TRUE', 'false': 'FALSE' }

    tokens = [ 'PLUS', 'PLUSEQ', 'MINUS', 'MINUSEQ', 'EQ', 'EQEQ', 'NOT', 'NOTEQ',
               'GT', 'GTEQ', 'LT', 'LTEQ', 'ANDAND', 'OROR', 'DOT', 'COMMA',
               'LBRACE', 'RBRACE', 'LBRACK', 'RBRACK', 'LPAREN', 'RPAREN',
               'NAME', 'INTEGER', 'STRING', 'COMMENT' ] + list(reserved.values())

    # Ignored characters
    t_ignore = ' \r\t'

    # The following character sequences represent punctuation:
    t_PLUS = r'\+'
    t_PLUSEQ = r'\+='
    t_MINUS = r'-'
    t_MINUSEQ = r'-='
    t_EQ = r'='
    t_EQEQ = r'=='
    t_NOT = r'!'
    t_NOTEQ = r'!='
    t_GT = r'>'
    t_GTEQ = r'>='
    t_LT = r'<'
    t_LTEQ = r'<='
    t_ANDAND = r'&&'
    t_OROR = r'\|\|'
    t_DOT = r'\.'
    t_COMMA = r','
    t_LPAREN = r'\('
    t_RPAREN = r'\)'
    t_LBRACK = r'\['
    t_RBRACK = r'\]'
    t_LBRACE = r'{'
    t_RBRACE = r'}'

    t_STRING = r'\"([^\\\n]|(\\.))*?\"'
    t_COMMENT = r'\#.*'

    def t_NAME(self, t):
        r'[a-zA-Z_][a-zA-Z0-9_]*'
        t.type = self.reserved.get(t.value, 'NAME') # Check for reserved words
        return t

    def t_INTEGER(self, t):
        r'\d+'
        t.value = int(t.value)
        return t

    # Ignored token with an action associated with it
    def t_ignore_newline(self, t):
        r'\n+'
        t.lexer.lineno += t.value.count('\n')

    # Error handler for illegal characters
    def t_error(self, t):
        print(f'{t.lexer.lineno}: Illegal character {t.value[0]!r}')
        t.lexer.skip(1)

    precedence = (
        ('left', 'OROR'),
        ('left', 'ANDAND'),
        ('left', 'EQEQ', 'NOTEQ'),
        ('left', 'GT', 'GTEQ', 'LT', 'LTEQ'),
        ('left', 'PLUS', 'MINUS'),
        ('right', 'NOT')
    )

    def p_file(self, p):
        '''file : statement_list'''
        p[0] = p[1]

    def p_statement(self, p):
        '''statement : comment
                     | assignment
                     | call
                     | condition'''
        p[0] = p[1]

    def p_comment(self, p):
        '''comment : COMMENT'''
        p[0] = Comment(p[1])

    def p_lvalue(self, p):
        '''lvalue : NAME
                | array_access
                | scope_access'''
        p[0] = p[1]

    def p_assignment(self, p):
        '''assignment : lvalue EQ expr
                    | lvalue PLUSEQ expr
                    | lvalue MINUSEQ expr'''
        p[0] = Stmt(p[2], children=[p[1], p[3]])

    def p_call_0(self, p):
        '''call : NAME LPAREN RPAREN
                | NAME LPAREN RPAREN block'''
        block = p[4] if len(p) == 5 else None
        p[0] = Stmt(p[1], children=[[], block])

    def p_call_1(self, p):
        '''call : NAME LPAREN expr_list RPAREN
                | NAME LPAREN expr_list RPAREN block'''
        block = p[5] if len(p) == 6 else None
        p[0] = Stmt(p[1], [p[3], block])

    def p_condition(self, p):
        '''condition : IF LPAREN expr RPAREN block
                    | IF LPAREN expr RPAREN block ELSE condition
                    | IF LPAREN expr RPAREN block ELSE block'''
        if len(p) == 8:
            p[0] = Stmt('if', [p[3], p[5], p[7]])
        else:
            p[0] = Stmt('if', [p[3], p[5], None])

    def p_block(self, p):
        '''block : LBRACE RBRACE
                | LBRACE statement_list RBRACE'''
        if len(p) == 3:
            p[0] = []
        else:
            p[0] = p[2]

    def p_statement_list(self, p):
        '''statement_list : statement
                        | statement_list statement'''
        if len(p) == 2:
            p[0] = [p[1]]
        else:
            p[0] = p[1] + [p[2]]

    def p_array_access(self, p):
        '''array_access : NAME LBRACK expr RBRACK'''
        p[0] = ('array-access', p[1], p[3])

    def p_scope_access(self, p):
        '''scope_access : NAME DOT NAME'''
        p[0] = ('scope-access', p[1], p[3])

    def p_expr_primary(self, p):
        '''expr : primary_expr'''
        p[0] = p[1]

    def p_expr_binary(self, p):
        '''expr : expr PLUS expr
                | expr MINUS  expr
                | expr GT expr
                | expr GTEQ  expr
                | expr LT  expr
                | expr LTEQ expr
                | expr EQEQ expr
                | expr NOTEQ expr
                | expr ANDAND expr
                | expr OROR expr'''
        p[0] = Expr(p[2], p[1], p[3])

    def p_expr_unary(self, p):
        '''expr : NOT expr'''
        p[0] = Expr(p[1], p[2])

    def p_primary_expr_name(self, p):
        '''primary_expr : NAME'''
        p[0] = Variable(p[1])

    def p_primary_expr_sliteral(self, p):
        '''primary_expr : STRING'''
        p[0] = p[1][1:-1] # Trim leading & trailing "

    def p_primary_expr(self, p):
        '''primary_expr : INTEGER
                        | TRUE
                        | FALSE
                        | call
                        | array_access
                        | scope_access
                        | block
                        | LPAREN expr RPAREN'''
        if len(p) == 2:
            p[0] = p[1]
        elif len(p) == 4:
            p[0] = p[2]

    def p_primary_expr_2(self, p):
        '''primary_expr : LBRACK RBRACK
                        | LBRACK expr_list RBRACK
                        | LBRACK expr_list COMMA RBRACK'''
        if len(p) == 3:
            p[0] = []
        else:
            p[0] = p[2]

    def p_expr_list(self, p):
        '''expr_list : expr
                    | expr_list COMMA expr'''
        if len(p) == 2:
            p[0] = [p[1]]
        else:
            p[0] = p[1] + [p[3]]

    def p_error(self, p):
        print(f'{p.lineno}: Syntax error at {p.value!r}')


# --- Process importing GNI files

class ImportGni:
    def __init__(self, cwd, exclude):
        self.cwd = cwd
        if type(exclude) is list:
            self.exclude = exclude
        else:
            self.exclude = [exclude]

    def _is_excluded(self, s):
        for pat in self.exclude:
            if re.fullmatch(pat, s):
                return True
        return False

    def _import_gni(self, import_stmt):
        assert(import_stmt.type == 'import')
        args = import_stmt[0]
        assert(len(args) == 1)
        path = args[0]
        if self._is_excluded(path):
            return []

        path = self.cwd / path
        return load_gn(path, self.exclude)

    def fold_list(self, list):
        new_list = []
        for child in list:
            if isinstance(child, Stmt) and child.type == 'import':
                imported = self._import_gni(child)
                new_list.extend(imported)
            else:
                new_list.append(self.fold(child))
        return new_list

    def fold(self, ast):
        if type(ast) is list:
            return self.fold_list(ast)
        elif isinstance(ast, Stmt):
            ast.children = self.fold_list(ast.children)

        return ast

def import_gni(ast, cwd, exclude):
    '''
    Import any gni files referenced in the AST.

    Any import paths that match the patterns in `exclude` are skipped.
    '''
    importer = ImportGni(cwd, exclude)
    return importer.fold(ast)


def load_gn(path, exclude):
    '''
    Load a gn/gni file, loading any imports not excluded by the patterns
    in `exclude`.
    '''
    input = path.read_text()
    ast = GnParser().parse(input)
    ast = import_gni(ast, path.parent, exclude)
    return ast


# --- Convert GN expression operators into CMake versions

class CMakeExprOps:
    def fold(self, ast):
        if type(ast) is list:
            return [self.fold(child) for child in ast]
        elif isinstance(ast, Stmt):
            ast.children = [self.fold(child) for child in ast.children]
        elif isinstance(ast, Expr):
            ast.children = [self.fold(child) for child in ast.children]
            # Rename GN binary operators into CMake
            if ast.type == '==':
                ast.type = ' STREQUAL '
            elif ast.type == '&&':
                ast.type = ' AND '
            elif ast.type == '||':
                ast.type = ' OR '
            elif ast.type == '!':
                ast.type = 'NOT '
            # Convert list concatenation into CMake form
            elif ast.type == '+':
                #  var + [ list ] -> [ ${var} list ]
                if isinstance(ast[0], Variable) and type(ast[1]) is list:
                    ast = [ Variable(f'${{{ast[0]}}}') ] + ast[1]
                #  "" + var -> ${var}
                elif len(ast[0]) == 0 and isinstance(ast[1], Variable):
                    ast = Variable(f'${{{ast[1]}}}')

        return ast

def convert_to_cmake_ops(ast):
    visitor = CMakeExprOps()
    return visitor.fold(ast)


# --- Exclude nodes

class ExcludeStmts:
    def __init__(self, exclude):
        if type(exclude) is list:
            self.exclude = exclude
        else:
            self.exclude = [exclude]

    def _excluded(self, ast):
        if isinstance(ast, Stmt):
            return ast.type in self.exclude
        return False

    def fold(self, ast):
        if type(ast) is list:
            return [self.fold(child) for child in ast if not self._excluded(child)]
        elif isinstance(ast, Stmt):
            ast.children = self.fold(ast.children)
        return ast

def exclude_stmts(ast, exclude):
    visitor = ExcludeStmts(exclude)
    return visitor.fold(ast)


# --- Write GN into CMake format

class CMakeWriter:
    def __init__(self, path, prepend):
        if not isinstance(path, Path):
            path = Path(path)
        self.prepend = prepend
        self.output = path.open('w', encoding='utf-8')
        self.writef('# This file was generated with the command:\n')
        self.writef('# %s\n\n', ' '.join(['"' + arg.replace('"', '\\"') + '"' for arg in sys.argv]))

    def __del__(self):
        self.output.close()

    def writef(self, fmt, *args):
        self.output.write(fmt % args)

    def visit(self, ast, indent=''):
        nindent = indent + '    '
        # Lists
        if type(ast) is list:
            [self.visit(child, indent) for child in ast]
        # Comment
        elif isinstance(ast, Comment):
            self.writef('%s\n', ast)
        # Variable
        elif isinstance(ast, Variable):
            self.writef('%s', ast)
        # Strings (representing Paths)
        elif isinstance(ast, str):
            if self.prepend:
                self.writef('"%s%s"', self.prepend, ast)
            else:
                self.writef('"%s"', ast)
        # Expressions
        elif isinstance(ast, Expr):
            if len(ast) == 1:
                self.writef('%s', ast.type)
                self.visit(ast[0], indent)
            else:
                self.visit(ast[0], indent)
                self.writef('%s', ast.type)
                self.visit(ast[1], indent)
        # Statements
        elif isinstance(ast, Stmt):
            # list assignment
            if ast.type == '=':
                if indent == '':
                    self.writef('\n')
                self.writef('%sset(%s', indent, ast[0])

                lst = []
                if type(ast[1]) is list:
                    lst = ast[1]
                    if len(lst) == 1:
                        self.writef(' ')
                        self.visit(lst[0])
                        self.writef(')\n')
                        return

                self.writef('\n')
                for child in lst:
                    self.writef('%s', nindent)
                    self.visit(child, nindent)
                    self.writef('\n')
                self.writef('%s)\n', indent)
            # list concatenation
            elif ast.type == '+=':
                if indent == '':
                    self.writef('\n')
                self.writef('%slist(APPEND %s', indent, ast[0])
                if type(ast[1]) is list:
                    lst = ast[1]
                    if len(lst) == 1:
                        self.writef(' ')
                        self.visit(lst[0])
                        self.writef(')\n')
                        return
                    else:
                        self.writef('\n')
                        for child in lst:
                            self.writef('%s', nindent)
                            self.visit(child, nindent)
                            self.writef('\n')
                        self.writef('%s)\n', indent)
            # if then else
            elif ast.type == 'if':
                if not len(ast[1]):
                    return
                self.writef('\n%sif(', indent)
                self.visit(ast[0], indent)
                self.writef(f')\n')
                self.visit(ast[1], nindent)
                if ast[2]:
                    self.writef('%selse()\n', indent)
                    self.visit(ast[2], nindent)
                self.writef('%sendif()\n', indent)
            # foreach loop
            elif ast.type == 'foreach':
                self.writef('\n%sforeach(', indent)
                self.visit(ast[0][0], indent)
                self.writef(' IN LISTS ')
                self.visit(ast[0][1], indent)
                self.writef(f')\n')
                self.visit(ast[1], nindent)
                self.writef('%sendforeach()\n', indent)
        # Unknown - These should have been removed by previous passes over the AST
        # before writing the output CMake
        else:
            raise RuntimeError

def write_cmake(path, ast, prepend):
    writer = CMakeWriter(path, prepend)
    writer.visit(ast)


if __name__ == '__main__':
    import sys
    from argparse import ArgumentParser

    parser = ArgumentParser(
        prog='gni-to-cmake', description='converts a .gn/.gni file to .cmake', usage='%(prog)s [options')
    parser.add_argument('gni', help='the .gn/gni file to parse')
    parser.add_argument('cmake', help='the .cmake file to output')
    parser.add_argument('--prepend', help='the path to prepend to each file name')

    args = parser.parse_args()

    path = Path(args.gni)
    if not path.exists():
        print(f'{path.fullname}: Not found')

    # TODO:
    ast = load_gn(path, exclude=[r'.*/angle\.gni$', r'.*/pkg_config\.gni$'])
    ast = exclude_stmts(ast, exclude=['assert', 'config', 'angle_source_set', 'pkg_config', 'declare_args'])
    ast = convert_to_cmake_ops(ast)
    write_cmake(args.cmake, ast, prepend=args.prepend)
