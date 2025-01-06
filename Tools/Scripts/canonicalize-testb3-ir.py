#!/usr/bin/env python3

# canonicalize-testb3-ir
#
# This is a simple program that will rewrite a B3 IR dump to quotient away
# some details that don't affect the program behavior.
#
# Notably it serves to make the ordering of effect-free instructions irrelevant:
#
# E.g. both this program:
#
# BB#0:
#   Int64 b@0 = ArgumentReg(%rdi)
#   Int64 b@1 = ArgumentReg(%rsi)
#   Int64 b@2 = Add(b@0, b@1)
#   Void b@3 = Return(b@2, Terminal)
#
# ... and this program ...:
#
# BB#0:
#   Int64 b@0 = ArgumentReg(%rsi)
#   Int64 b@1 = ArgumentReg(%rdi)
#   Int64 b@2 = Add(b@1, b@0)
#   Void b@3 = Return(b@2, Terminal)
#
# ... will canonicalize to the same result:
#
# BB#0:
#   Successors: None
#     Void %0 = Return(Int64(Add(Int64(ArgumentReg(%rdi)), Int64(ArgumentReg(%rsi))), Terminal))

import argparse
import sys
import re
import typing
import io
import traceback
from dataclasses import dataclass
from pprint import pprint

# sometimes we need to substitue large expression trees, and I'm too lazy to
# add the actual call stack in python
# ... hopefully you have enough stack space :)
# sys.setrecursionlimit(1000000)

if __name__ != "__main__":
    sys.exit(1)

# First we need to lex the B3 IR:


class TestBegin(typing.NamedTuple):
    name: str


class TestEnd(typing.NamedTuple):
    name: str


class BasicBlockBegin(typing.NamedTuple):
    id: str
    detail: str


class BasicBlockEnd(typing.NamedTuple):
    detail: str


class Type(typing.NamedTuple):
    name: str


class Var(typing.NamedTuple):
    name: str


class PhiVar(typing.NamedTuple):
    name: str


class Operand(typing.NamedTuple):
    content: str


class LeftParen(typing.NamedTuple):
    pass


class RightParen(typing.NamedTuple):
    pass


class Comma(typing.NamedTuple):
    pass


def lex_ssa(line):
    match = re.match(r'^([^ ]+) b@([0-9]+) = (.*)$', line)
    if match is None:
        raise RuntimeError("lex_ssa: expected an SSA assignment here")
    yield Type(match.group(1))
    yield Var(match.group(2))

    operands = match.group(3)
    for tok in re.findall(r'([^,()]+|,|\(|\)| +)', operands):
        tok = tok.strip()
        if tok == '':
            continue
        match = re.match('b@([0-9]+)', tok)
        if match is not None:
            yield Var(match.group(1))
            continue
        match = re.match('\^([0-9]+)', tok)
        if match is not None:
            yield PhiVar(match.group(1))
            continue
        match = re.match(',', tok)
        if match is not None:
            yield Comma()
            continue
        match = re.match('\(', tok)
        if match is not None:
            yield LeftParen()
            continue
        match = re.match('\)', tok)
        if match is not None:
            yield RightParen()
            continue
        yield Operand(tok)


def lex(f):
    line_num = 0
    discard = False
    for line in f.readlines():
        line_num += 1
        try:
            match = re.match(r'^O[0-2]: (.*)\.\.\.$', line)
            if match is not None:
                yield TestBegin(match.group(1))
                discard = False
                continue
            match = re.match(r'^O[0-2]: (.*): OK!$', line)
            if match is not None:
                yield TestEnd(match.group(1))
                discard = False
                continue
            if discard:
                continue
            if "Initial B3" in line:
                discard = False
                continue
            if "Predecessors" in line:
                continue
            if "Variables:" in line:
                discard = True
                continue
            if "Stack slots:" in line:  # FIXME(jgriego)
                discard = True
                continue
            if not line.startswith('b3'):
                continue
            line = re.sub(r'^b3 *', '', line)
            match = re.match(r'BB#([0-9]+): (.*)$', line)
            if match is not None:
                yield BasicBlockBegin(match.group(1), match.group(2))
                continue
            match = re.match(r'^Successors: (.*)$', line)
            if match is not None:
                yield BasicBlockEnd(match.group(1))
                continue
            yield from lex_ssa(line)
        except:
            print(f"lexing failed at line {line_num}\n")
            raise


# Now we parse into something a little easier to work with:


@dataclass
class Test:
    """A test is a named collection of basic blocks, corresponding to one or more B3 procedures"""

    name: str
    blocks: typing.List["Block"]


@dataclass
class Block:
    id: str
    detail: str
    insns: typing.List["Instr"]
    successor_detail: str


@dataclass
class Instr:
    dest: Var
    typ: Type
    rhs: list


def parse_test_list(suite, toks):
    while parse_test(suite, toks):
        pass


def parse_test(suite, toks):
    if len(toks) == 0:
        return None
    if isinstance(toks[-1], TestBegin):
        test = Test(toks[-1].name, [])

        suite.append(test)

        toks.pop()
        while parse_block(test.blocks, toks):
            pass

        if isinstance(toks[-1], TestEnd):
            toks.pop()
            return True

    return False


def parse_block(prgm, toks):
    if isinstance(toks[-1], BasicBlockBegin):
        block = Block(toks[-1].id, toks[-1].detail, [], None)
        prgm.append(block)
        toks.pop()
        parse_insn_list(block, toks)
        if isinstance(toks[-1], BasicBlockEnd):
            block.successor_detail = toks[-1].detail
            toks.pop()
        return True

    return False


def parse_insn_list(block, toks):
    while parse_insn(block, toks):
        pass


def parse_insn(block, toks):
    if isinstance(toks[-1], Type) and isinstance(toks[-2], Var):
        i = Instr(toks[-2], toks[-1], [])
        block.insns.append(i)
        toks.pop()
        toks.pop()
        parse_expr(i.rhs, toks)
        i.rhs = i.rhs[0]
        return True

    return False


def parse_expr_list(out, toks):
    while parse_expr(out, toks):
        if isinstance(toks[-1], Comma):
            toks.pop()


def parse_expr(out, toks):
    if isinstance(toks[-1], Operand) and isinstance(toks[-2], LeftParen):
        e = [toks[-1]]
        out.append(e)
        toks.pop()
        toks.pop()
        parse_expr_list(e, toks)
        if isinstance(toks[-1], RightParen):
            toks.pop()
            return True
    elif (
        isinstance(toks[-1], Operand)
        or isinstance(toks[-1], Var)
        or isinstance(toks[-1], PhiVar)
    ):
        out.append(toks[-1])
        toks.pop()
        return True
    return False


def dump_test(test):
    # We need to deal with the tests that emit multiple B3 procedures during
    # their run However, these are not delineated by the parser, so, we do it
    # here--when we encounter a second basic block with index 0, we flush the
    # renaming context and finish writing out the current procedure before
    # continuing with the next

    unit_count = 0
    e = {}
    walked_blocks = []

    def flush():
        # Clear the renaming context, `e', and write out all walked blocks
        nonlocal unit_count
        nonlocal e
        nonlocal walked_blocks
        if len(walked_blocks) == 0:
            return
        print(f"*** *** *** *** {test.name} [unit {unit_count}] *** *** *** ***")
        for block in walked_blocks:
            dump_block(e, block)
        e = {}
        unit_count += 1
        walked_blocks = []
        print("")

    for block in test.blocks:
        if int(block.id) == 0:
            flush()
        walked_blocks.append(walk_block(e, block))
    flush()


# Finally, renumber so we only name SSA nodes with effects and print the result
# out as we go along


@dataclass
class VisitedBlock:
    id: str
    insns: list
    detail: str
    successor_detail: str


@dataclass
class VisitedInstr:
    dest: int
    typ: Type
    rhs: list


def walk_block(e, block):
    out = []
    for insn in block.insns:
        i = walk_instr(e, insn)
        if i is not None:
            out.append(i)
    return VisitedBlock(block.id, out, block.detail, block.successor_detail)


def top_expr_has_effects(exp):
    last_operand = exp[-1]

    # this is over-eager but will catch everything that looks vaguely effect-like
    if isinstance(last_operand, Operand):
        if re.match(r'^[A-Za-z:|]+$', last_operand.content):
            return True

    return False


def walk_instr(e, ins):
    if top_expr_has_effects(ins.rhs):
        vid = e.get("next_id", 0)
        e["next_id"] = vid + 1
        e[ins.dest.name] = vid
        return VisitedInstr(vid, ins.typ, ins.rhs)
    else:
        # we move the type coercion to the rhs so as to not lose it
        e[ins.dest.name] = [Operand(ins.typ.name), ins.rhs]
        return None


@dataclass
class ListK:
    length: int


def subst(e, exp):
    # explicit stacks because we need to substitute large expression trees and
    # python isn't up to task if we just write the natural (recursive) thing
    stack_out = []
    stack_in = [exp]

    while len(stack_in) > 0:
        exp = stack_in.pop()
        if isinstance(exp, list):
            stack_in.append(ListK(len(exp)))
            stack_in.extend(exp)
        elif isinstance(exp, Operand):
            stack_out.append(exp)
        elif isinstance(exp, Var):
            stack_in.append(e[exp.name])
        elif isinstance(exp, PhiVar):
            stack_in.append(e[exp.name])
        elif isinstance(exp, int):
            stack_out.append(exp)
        elif isinstance(exp, ListK):
            elems = []
            for _ in range(exp.length):
                elems.append(stack_out.pop())
            stack_out.append(elems)
        else:
            raise RuntimeError("subst")
    result = stack_out.pop()
    if len(stack_out) != 0:
        raise RuntimeError("subst: stacks not balanced")
    return result


def dump_block(e, block):
    print(f"B{block.id}: {block.detail}")
    print(f"  Successors: {block.successor_detail}")
    for insn in block.insns:
        print(f"    {insn.typ.name} %{insn.dest} = {fmt_expr(subst(e, insn.rhs))}")


def fmt_expr(exp):
    # explicit stack because we need to write out large expression trees and
    # python isn't up to task if we just write the natural (recursive) thing
    out = ""
    stack = [exp]
    while len(stack) > 0:
        exp = stack.pop()
        if isinstance(exp, list):
            stack.append(")")
            operands = exp[1:]
            operands.reverse()
            for i, e in enumerate(operands):
                if i != 0:
                    stack.append(",")
                stack.append(e)

            stack.append("(")
            stack.append(exp[0])
        elif isinstance(exp, Operand):
            out += exp.content
        elif isinstance(exp, int):
            out += f"%{exp}"
        elif isinstance(exp, str):
            out += exp
        else:
            print(exp)
            raise RuntimeError("fmt_expr: not an expr")
    return out


stdin_contents = sys.stdin.read()
try:
    toks = [tok for tok in lex(io.StringIO(stdin_contents))]
    toks.reverse()
    suite = []
    leftover = parse_test_list(suite, toks)
    for test in suite:
        dump_test(test)
except Exception as e:
    # If something goes wrong, put the error in the file along with the
    # original input
    print(
        f"couldn't parse b3 IR; error:\n{traceback.format_exc()}\n\n *** *** *** \n stdin:"
    )
    for idx, line in enumerate(stdin_contents.split("\n")):
        print(f"{idx + 1:7} | {line}")
