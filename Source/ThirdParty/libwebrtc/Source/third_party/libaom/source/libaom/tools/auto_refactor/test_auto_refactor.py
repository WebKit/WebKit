#!/usr/bin/env python
# Copyright (c) 2021, Alliance for Open Media. All rights reserved.
#
# This source code is subject to the terms of the BSD 2 Clause License and
# the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
# was not distributed with this source code in the LICENSE file, you can
# obtain it at www.aomedia.org/license/software. If the Alliance for Open
# Media Patent License 1.0 was not distributed with this source code in the
# PATENTS file, you can obtain it at www.aomedia.org/license/patent.
#

import pprint
import re
import os, sys
import io
import unittest as googletest

sys.path[0:0] = ['.', '..']

from pycparser import c_parser, parse_file
from pycparser.c_ast import *
from pycparser.c_parser import CParser, Coord, ParseError

from auto_refactor import *


def get_c_file_path(filename):
  return os.path.join('c_files', filename)


class TestStructInfo(googletest.TestCase):

  def setUp(self):
    filename = get_c_file_path('struct_code.c')
    self.ast = parse_file(filename)

  def test_build_struct_info(self):
    struct_info = build_struct_info(self.ast)
    typedef_name_dic = struct_info.typedef_name_dic
    self.assertEqual('T1' in typedef_name_dic, True)
    self.assertEqual('T4' in typedef_name_dic, True)
    self.assertEqual('T5' in typedef_name_dic, True)

    struct_name_dic = struct_info.struct_name_dic
    struct_name = 'S1'
    self.assertEqual(struct_name in struct_name_dic, True)
    struct_item = struct_name_dic[struct_name]
    self.assertEqual(struct_item.is_union, False)

    struct_name = 'S3'
    self.assertEqual(struct_name in struct_name_dic, True)
    struct_item = struct_name_dic[struct_name]
    self.assertEqual(struct_item.is_union, False)

    struct_name = 'U5'
    self.assertEqual(struct_name in struct_name_dic, True)
    struct_item = struct_name_dic[struct_name]
    self.assertEqual(struct_item.is_union, True)

    self.assertEqual(len(struct_info.struct_item_list), 6)

  def test_get_child_decl_status(self):
    struct_info = build_struct_info(self.ast)
    struct_item = struct_info.typedef_name_dic['T4']

    decl_status = struct_item.child_decl_map['x']
    self.assertEqual(decl_status.struct_item, None)
    self.assertEqual(decl_status.is_ptr_decl, False)

    decl_status = struct_item.child_decl_map['s3']
    self.assertEqual(decl_status.struct_item.struct_name, 'S3')
    self.assertEqual(decl_status.is_ptr_decl, False)

    struct_item = struct_info.typedef_name_dic['T6']
    decl_status = struct_item.child_decl_map['x']
    self.assertEqual(decl_status.struct_item, None)
    self.assertEqual(decl_status.is_ptr_decl, False)

    decl_status = struct_item.child_decl_map['y']
    self.assertEqual(decl_status.struct_item, None)
    self.assertEqual(decl_status.is_ptr_decl, False)

    decl_status = struct_item.child_decl_map['z']
    self.assertEqual(decl_status.struct_item, None)
    self.assertEqual(decl_status.is_ptr_decl, False)

    struct_item = struct_info.typedef_name_dic['T7']
    decl_status = struct_item.child_decl_map['y']
    self.assertEqual('x' in decl_status.struct_item.child_decl_map, True)

    struct_item = struct_info.typedef_name_dic['T7']
    decl_status = struct_item.child_decl_map['z']
    self.assertEqual('w' in decl_status.struct_item.child_decl_map, True)


class TestParseLvalue(googletest.TestCase):

  def setUp(self):
    filename = get_c_file_path('parse_lvalue.c')
    self.ast = parse_file(filename)
    self.func_dictionary = build_func_dictionary(self.ast)

  def test_parse_lvalue(self):
    func_node = self.func_dictionary['func']
    func_body_items = func_node.body.block_items
    id_list = parse_lvalue(func_body_items[0].lvalue)
    ref_id_list = ['cpi', 'rd', 'u']
    self.assertEqual(id_list, ref_id_list)

    id_list = parse_lvalue(func_body_items[2].lvalue)
    ref_id_list = ['y']
    self.assertEqual(id_list, ref_id_list)

    id_list = parse_lvalue(func_body_items[3].lvalue)
    ref_id_list = ['cpi', 'rd2', 'v']
    self.assertEqual(id_list, ref_id_list)

    id_list = parse_lvalue(func_body_items[4].lvalue)
    ref_id_list = ['cpi', 'rd', 'arr']
    self.assertEqual(id_list, ref_id_list)

    id_list = parse_lvalue(func_body_items[5].lvalue)
    ref_id_list = ['cpi', 'rd3', 'arr']
    self.assertEqual(id_list, ref_id_list)

  def test_parse_lvalue_2(self):
    func_node = self.func_dictionary['parse_lvalue_2']
    func_body_items = func_node.body.block_items
    id_list = parse_lvalue(func_body_items[0].init)
    ref_id_list = ['cpi', 'rd2']
    self.assertEqual(id_list, ref_id_list)


class TestIDStatusNode(googletest.TestCase):

  def test_add_descendant(self):
    root = IDStatusNode('root')
    id_chain1 = ['cpi', 'rd', 'u']
    id_chain2 = ['cpi', 'rd', 'v']
    root.add_descendant(id_chain1)
    root.add_descendant(id_chain2)

    ref_children_list1 = ['cpi']
    children_list1 = list(root.children.keys())
    self.assertEqual(children_list1, ref_children_list1)

    ref_children_list2 = ['rd']
    children_list2 = list(root.children['cpi'].children.keys())
    self.assertEqual(children_list2, ref_children_list2)

    ref_children_list3 = ['u', 'v']
    children_list3 = list(root.children['cpi'].children['rd'].children.keys())
    self.assertEqual(children_list3, ref_children_list3)

  def test_get_descendant(self):
    root = IDStatusNode('root')
    id_chain1 = ['cpi', 'rd', 'u']
    id_chain2 = ['cpi', 'rd', 'v']
    ref_descendant_1 = root.add_descendant(id_chain1)
    ref_descendant_2 = root.add_descendant(id_chain2)

    descendant_1 = root.get_descendant(id_chain1)
    self.assertEqual(descendant_1 is ref_descendant_1, True)

    descendant_2 = root.get_descendant(id_chain2)
    self.assertEqual(descendant_2 is ref_descendant_2, True)

    id_chain3 = ['cpi', 'rd', 'h']
    descendant_3 = root.get_descendant(id_chain3)
    self.assertEqual(descendant_3, None)


class TestFuncInOut(googletest.TestCase):

  def setUp(self):
    c_filename = get_c_file_path('func_in_out.c')
    self.ast = parse_file(c_filename)
    self.func_dictionary = build_func_dictionary(self.ast)
    self.struct_info = build_struct_info(self.ast)

  def test_get_func_param_id_map(self):
    func_def_node = self.func_dictionary['func']
    param_id_map = get_func_param_id_map(func_def_node)
    ref_param_id_map_keys = ['cpi', 'x']
    self.assertEqual(list(param_id_map.keys()), ref_param_id_map_keys)

  def test_assign_refer_status_1(self):
    func_def_node = self.func_dictionary['func_assign_refer_status_1']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()
    body_id_tree = visitor.body_id_tree

    id_chain = ['rd']
    descendant = body_id_tree.get_descendant(id_chain)
    self.assertEqual(descendant.get_assign(), False)
    self.assertEqual(descendant.get_refer(), False)
    ref_link_id_chain = ['cpi', 'rd']
    self.assertEqual(ref_link_id_chain, descendant.get_link_id_chain())

    id_chain = ['cpi', 'rd']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), False)
    self.assertEqual(descendant.get_refer(), False)
    self.assertEqual(None, descendant.get_link_id_chain())

  def test_assign_refer_status_2(self):
    func_def_node = self.func_dictionary['func_assign_refer_status_2']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()
    body_id_tree = visitor.body_id_tree

    id_chain = ['rd2']
    descendant = body_id_tree.get_descendant(id_chain)
    self.assertEqual(descendant.get_assign(), False)
    self.assertEqual(descendant.get_refer(), False)

    ref_link_id_chain = ['cpi', 'rd']
    self.assertEqual(ref_link_id_chain, descendant.get_link_id_chain())

    id_chain = ['cpi', 'rd']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), False)
    self.assertEqual(descendant.get_refer(), False)
    self.assertEqual(None, descendant.get_link_id_chain())

  def test_assign_refer_status_3(self):
    func_def_node = self.func_dictionary['func_assign_refer_status_3']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()
    body_id_tree = visitor.body_id_tree

    id_chain = ['a']
    descendant = body_id_tree.get_descendant(id_chain)
    self.assertEqual(descendant.get_assign(), True)
    self.assertEqual(descendant.get_refer(), False)
    self.assertEqual(None, descendant.get_link_id_chain())

    id_chain = ['cpi', 'y']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), False)
    self.assertEqual(descendant.get_refer(), True)
    self.assertEqual(None, descendant.get_link_id_chain())

  def test_assign_refer_status_4(self):
    func_def_node = self.func_dictionary['func_assign_refer_status_4']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()
    body_id_tree = visitor.body_id_tree

    id_chain = ['b']
    descendant = body_id_tree.get_descendant(id_chain)
    self.assertEqual(descendant.get_assign(), True)
    self.assertEqual(descendant.get_refer(), False)
    self.assertEqual(None, descendant.get_link_id_chain())

    id_chain = ['cpi', 'y']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), True)
    self.assertEqual(descendant.get_refer(), True)
    self.assertEqual(None, descendant.get_link_id_chain())

  def test_assign_refer_status_5(self):
    func_def_node = self.func_dictionary['func_assign_refer_status_5']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()
    body_id_tree = visitor.body_id_tree

    id_chain = ['rd5']
    descendant = body_id_tree.get_descendant(id_chain)
    self.assertEqual(descendant.get_assign(), False)
    self.assertEqual(descendant.get_refer(), False)

    id_chain = ['cpi', 'rd2']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), False)
    self.assertEqual(descendant.get_refer(), False)
    self.assertEqual(None, descendant.get_link_id_chain())

  def test_assign_refer_status_6(self):
    func_def_node = self.func_dictionary['func_assign_refer_status_6']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()

    id_chain = ['cpi', 'rd']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), True)
    self.assertEqual(descendant.get_refer(), False)
    self.assertEqual(None, descendant.get_link_id_chain())

    id_chain = ['cpi2', 'rd']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), False)
    self.assertEqual(descendant.get_refer(), True)
    self.assertEqual(None, descendant.get_link_id_chain())

  def test_assign_refer_status_7(self):
    func_def_node = self.func_dictionary['func_assign_refer_status_7']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()
    id_chain = ['cpi', 'arr']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), True)
    self.assertEqual(descendant.get_refer(), False)

  def test_assign_refer_status_8(self):
    func_def_node = self.func_dictionary['func_assign_refer_status_8']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()
    id_chain = ['cpi', 'arr']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), False)
    self.assertEqual(descendant.get_refer(), True)

  def test_assign_refer_status_9(self):
    func_def_node = self.func_dictionary['func_assign_refer_status_9']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()
    id_chain = ['cpi', 'rd', 'u']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), True)
    self.assertEqual(descendant.get_refer(), False)

  def test_assign_refer_status_10(self):
    func_def_node = self.func_dictionary['func_assign_refer_status_10']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()
    id_chain = ['cpi', 'rd', 'u']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), False)
    self.assertEqual(descendant.get_refer(), True)

    id_chain = ['cpi', 'arr']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), True)
    self.assertEqual(descendant.get_refer(), False)

  def test_assign_refer_status_11(self):
    func_def_node = self.func_dictionary['func_assign_refer_status_11']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()
    id_chain = ['cpi', 'rd2', 'v']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), True)
    self.assertEqual(descendant.get_refer(), False)

  def test_assign_refer_status_12(self):
    func_def_node = self.func_dictionary['func_assign_refer_status_12']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()
    id_chain = ['cpi', 'rd']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), True)
    self.assertEqual(descendant.get_refer(), False)

    id_chain = ['cpi2', 'rd']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), False)
    self.assertEqual(descendant.get_refer(), True)

  def test_assign_refer_status_13(self):
    func_def_node = self.func_dictionary['func_assign_refer_status_13']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()
    id_chain = ['cpi', 'z']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), True)
    self.assertEqual(descendant.get_refer(), False)

    id_chain = ['cpi', 'w']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), True)
    self.assertEqual(descendant.get_refer(), False)

  def test_id_status_forrest_1(self):
    func_def_node = self.func_dictionary['func']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack().top()
    children_names = set(root.get_children().keys())
    ref_children_names = set(['cpi', 'x'])
    self.assertEqual(children_names, ref_children_names)

    root = visitor.body_id_tree
    children_names = set(root.get_children().keys())
    ref_children_names = set(['a', 'ref_rd', 'ref_rd2', 'ref_rd3', 'b'])
    self.assertEqual(children_names, ref_children_names)

  def test_id_status_forrest_show(self):
    func_def_node = self.func_dictionary['func_id_forrest_show']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    visitor.get_id_tree_stack().top().show()

  def test_id_status_forrest_2(self):
    func_def_node = self.func_dictionary['func_id_forrest_show']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack().top()
    self.assertEqual(root, root.root)

    id_chain = ['cpi', 'rd']
    descendant = root.get_descendant(id_chain)
    self.assertEqual(root, descendant.root)

    id_chain = ['b']
    descendant = root.get_descendant(id_chain)
    self.assertEqual(root, descendant.root)

  def test_link_id_chain_1(self):
    func_def_node = self.func_dictionary['func_link_id_chain_1']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()
    id_chain = ['cpi', 'rd', 'u']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), True)

  def test_link_id_chain_2(self):
    func_def_node = self.func_dictionary['func_link_id_chain_2']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()
    id_chain = ['cpi', 'rd', 'xd', 'u']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), True)

  def test_func_call_1(self):
    func_def_node = self.func_dictionary['func_call_1']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()
    id_chain = ['cpi', 'y']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), True)
    self.assertEqual(descendant.get_refer(), False)

    id_chain = ['y']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), False)
    self.assertEqual(descendant.get_refer(), True)

  def test_func_call_2(self):
    func_def_node = self.func_dictionary['func_call_2']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()
    id_chain = ['cpi', 'rd', 'u']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), True)
    self.assertEqual(descendant.get_refer(), False)

    id_chain = ['cpi', 'rd']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), False)
    self.assertEqual(descendant.get_refer(), False)

  def test_func_call_3(self):
    func_def_node = self.func_dictionary['func_call_3']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()
    id_chain = ['cpi', 'y']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), True)
    self.assertEqual(descendant.get_refer(), True)

  def test_func_call_4(self):
    func_def_node = self.func_dictionary['func_call_4']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()

    id_chain = ['cpi', 'rd', 'u']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), True)
    self.assertEqual(descendant.get_refer(), False)

    id_chain = ['cpi', 'rd', 'xd', 'u']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), True)
    self.assertEqual(descendant.get_refer(), False)

  def test_func_call_5(self):
    func_def_node = self.func_dictionary['func_call_5']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()

    id_chain = ['cpi', 'y']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), True)
    self.assertEqual(descendant.get_refer(), False)

  def test_func_compound_1(self):
    func_def_node = self.func_dictionary['func_compound_1']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()
    id_chain = ['cpi', 'y']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), True)
    self.assertEqual(descendant.get_refer(), True)

  def test_func_compound_2(self):
    func_def_node = self.func_dictionary['func_compound_2']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()
    id_chain = ['cpi', 'y']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), False)
    self.assertEqual(descendant.get_refer(), True)

    id_chain = ['cpi', 'rd', 'u']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), True)
    self.assertEqual(descendant.get_refer(), False)

  def test_func_compound_3(self):
    func_def_node = self.func_dictionary['func_compound_3']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()

    id_chain = ['cpi', 'rd', 'u']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), True)
    self.assertEqual(descendant.get_refer(), False)

  def test_func_compound_4(self):
    func_def_node = self.func_dictionary['func_compound_4']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()
    id_chain = ['cpi', 'y']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), True)
    self.assertEqual(descendant.get_refer(), True)

  def test_func_compound_5(self):
    func_def_node = self.func_dictionary['func_compound_5']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()
    id_chain = ['cpi', 'y']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), True)
    self.assertEqual(descendant.get_refer(), True)

  def test_func_compound_6(self):
    func_def_node = self.func_dictionary['func_compound_6']
    visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                               self.func_dictionary)
    visitor.visit(func_def_node.body)
    root = visitor.get_id_tree_stack()
    id_chain = ['cpi', 'y']
    descendant = root.get_id_node(id_chain)
    self.assertEqual(descendant.get_assign(), True)
    self.assertEqual(descendant.get_refer(), True)


class TestDeclStatus(googletest.TestCase):

  def setUp(self):
    filename = get_c_file_path('decl_status_code.c')
    self.ast = parse_file(filename)
    self.func_dictionary = build_func_dictionary(self.ast)
    self.struct_info = build_struct_info(self.ast)

  def test_parse_decl_node(self):
    func_def_node = self.func_dictionary['main']
    decl_list = func_def_node.body.block_items
    decl_status = parse_decl_node(self.struct_info, decl_list[0])
    self.assertEqual(decl_status.name, 'a')
    self.assertEqual(decl_status.is_ptr_decl, False)

    decl_status = parse_decl_node(self.struct_info, decl_list[1])
    self.assertEqual(decl_status.name, 't1')
    self.assertEqual(decl_status.is_ptr_decl, False)

    decl_status = parse_decl_node(self.struct_info, decl_list[2])
    self.assertEqual(decl_status.name, 's1')
    self.assertEqual(decl_status.is_ptr_decl, False)

    decl_status = parse_decl_node(self.struct_info, decl_list[3])
    self.assertEqual(decl_status.name, 't2')
    self.assertEqual(decl_status.is_ptr_decl, True)

  def test_parse_decl_node_2(self):
    func_def_node = self.func_dictionary['parse_decl_node_2']
    decl_list = func_def_node.body.block_items
    decl_status = parse_decl_node(self.struct_info, decl_list[0])
    self.assertEqual(decl_status.name, 'arr')
    self.assertEqual(decl_status.is_ptr_decl, True)
    self.assertEqual(decl_status.struct_item, None)

  def test_parse_decl_node_3(self):
    func_def_node = self.func_dictionary['parse_decl_node_3']
    decl_list = func_def_node.body.block_items
    decl_status = parse_decl_node(self.struct_info, decl_list[0])
    self.assertEqual(decl_status.name, 'a')
    self.assertEqual(decl_status.is_ptr_decl, True)
    self.assertEqual(decl_status.struct_item, None)

  def test_parse_decl_node_4(self):
    func_def_node = self.func_dictionary['parse_decl_node_4']
    decl_list = func_def_node.body.block_items
    decl_status = parse_decl_node(self.struct_info, decl_list[0])
    self.assertEqual(decl_status.name, 't1')
    self.assertEqual(decl_status.is_ptr_decl, True)
    self.assertEqual(decl_status.struct_item.typedef_name, 'T1')
    self.assertEqual(decl_status.struct_item.struct_name, 'S1')

  def test_parse_decl_node_5(self):
    func_def_node = self.func_dictionary['parse_decl_node_5']
    decl_list = func_def_node.body.block_items
    decl_status = parse_decl_node(self.struct_info, decl_list[0])
    self.assertEqual(decl_status.name, 't2')
    self.assertEqual(decl_status.is_ptr_decl, True)
    self.assertEqual(decl_status.struct_item.typedef_name, 'T1')
    self.assertEqual(decl_status.struct_item.struct_name, 'S1')

  def test_parse_decl_node_6(self):
    func_def_node = self.func_dictionary['parse_decl_node_6']
    decl_list = func_def_node.body.block_items
    decl_status = parse_decl_node(self.struct_info, decl_list[0])
    self.assertEqual(decl_status.name, 't3')
    self.assertEqual(decl_status.is_ptr_decl, True)
    self.assertEqual(decl_status.struct_item.typedef_name, 'T1')
    self.assertEqual(decl_status.struct_item.struct_name, 'S1')


if __name__ == '__main__':
  googletest.main()
