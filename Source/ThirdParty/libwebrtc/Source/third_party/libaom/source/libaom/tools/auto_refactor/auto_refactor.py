# Copyright (c) 2021, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 2 Clause License and
# the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
# was not distributed with this source code in the LICENSE file, you can
# obtain it at www.aomedia.org/license/software. If the Alliance for Open
# Media Patent License 1.0 was not distributed with this source code in the
# PATENTS file, you can obtain it at www.aomedia.org/license/patent.
#

from __future__ import print_function
import sys
import os
import operator
from pycparser import c_parser, c_ast, parse_file
from math import *

from inspect import currentframe, getframeinfo
from collections import deque


def debug_print(frameinfo):
  print('******** ERROR:', frameinfo.filename, frameinfo.lineno, '********')


class StructItem():

  def __init__(self,
               typedef_name=None,
               struct_name=None,
               struct_node=None,
               is_union=False):
    self.typedef_name = typedef_name
    self.struct_name = struct_name
    self.struct_node = struct_node
    self.is_union = is_union
    self.child_decl_map = None

  def __str__(self):
    return str(self.typedef_name) + ' ' + str(self.struct_name) + ' ' + str(
        self.is_union)

  def compute_child_decl_map(self, struct_info):
    self.child_decl_map = {}
    if self.struct_node != None and self.struct_node.decls != None:
      for decl_node in self.struct_node.decls:
        if decl_node.name == None:
          for sub_decl_node in decl_node.type.decls:
            sub_decl_status = parse_decl_node(struct_info, sub_decl_node)
            self.child_decl_map[sub_decl_node.name] = sub_decl_status
        else:
          decl_status = parse_decl_node(struct_info, decl_node)
          self.child_decl_map[decl_status.name] = decl_status

  def get_child_decl_status(self, decl_name):
    if self.child_decl_map == None:
      debug_print(getframeinfo(currentframe()))
      print('child_decl_map is None')
      return None
    if decl_name not in self.child_decl_map:
      debug_print(getframeinfo(currentframe()))
      print(decl_name, 'does not exist ')
      return None
    return self.child_decl_map[decl_name]


class StructInfo():

  def __init__(self):
    self.struct_name_dic = {}
    self.typedef_name_dic = {}
    self.enum_value_dic = {}  # enum value -> enum_node
    self.enum_name_dic = {}  # enum name -> enum_node
    self.struct_item_list = []

  def get_struct_by_typedef_name(self, typedef_name):
    if typedef_name in self.typedef_name_dic:
      return self.typedef_name_dic[typedef_name]
    else:
      return None

  def get_struct_by_struct_name(self, struct_name):
    if struct_name in self.struct_name_dic:
      return self.struct_name_dic[struct_name]
    else:
      debug_print(getframeinfo(currentframe()))
      print('Cant find', struct_name)
      return None

  def update_struct_item_list(self):
    # Collect all struct_items from struct_name_dic and typedef_name_dic
    # Compute child_decl_map for each struct item.
    for struct_name in self.struct_name_dic.keys():
      struct_item = self.struct_name_dic[struct_name]
      struct_item.compute_child_decl_map(self)
      self.struct_item_list.append(struct_item)

    for typedef_name in self.typedef_name_dic.keys():
      struct_item = self.typedef_name_dic[typedef_name]
      if struct_item.struct_name not in self.struct_name_dic:
        struct_item.compute_child_decl_map(self)
        self.struct_item_list.append(struct_item)

  def update_enum(self, enum_node):
    if enum_node.name != None:
      self.enum_name_dic[enum_node.name] = enum_node

    if enum_node.values != None:
      enumerator_list = enum_node.values.enumerators
      for enumerator in enumerator_list:
        self.enum_value_dic[enumerator.name] = enum_node

  def update(self,
             typedef_name=None,
             struct_name=None,
             struct_node=None,
             is_union=False):
    """T: typedef_name S: struct_name N: struct_node

           T S N
   case 1: o o o
   typedef struct P {
    int u;
   } K;
           T S N
   case 2: o o x
   typedef struct P K;

           T S N
   case 3: x o o
   struct P {
    int u;
   };

           T S N
   case 4: o x o
   typedef struct {
    int u;
   } K;
    """
    struct_item = None

    # Check whether struct_name or typedef_name is already in the dictionary
    if struct_name in self.struct_name_dic:
      struct_item = self.struct_name_dic[struct_name]

    if typedef_name in self.typedef_name_dic:
      struct_item = self.typedef_name_dic[typedef_name]

    if struct_item == None:
      struct_item = StructItem(typedef_name, struct_name, struct_node, is_union)

    if struct_node.decls != None:
      struct_item.struct_node = struct_node

    if struct_name != None:
      self.struct_name_dic[struct_name] = struct_item

    if typedef_name != None:
      self.typedef_name_dic[typedef_name] = struct_item


class StructDefVisitor(c_ast.NodeVisitor):

  def __init__(self):
    self.struct_info = StructInfo()

  def visit_Struct(self, node):
    if node.decls != None:
      self.struct_info.update(None, node.name, node)
    self.generic_visit(node)

  def visit_Union(self, node):
    if node.decls != None:
      self.struct_info.update(None, node.name, node, True)
    self.generic_visit(node)

  def visit_Enum(self, node):
    self.struct_info.update_enum(node)
    self.generic_visit(node)

  def visit_Typedef(self, node):
    if node.type.__class__.__name__ == 'TypeDecl':
      typedecl = node.type
      if typedecl.type.__class__.__name__ == 'Struct':
        struct_node = typedecl.type
        typedef_name = node.name
        struct_name = struct_node.name
        self.struct_info.update(typedef_name, struct_name, struct_node)
      elif typedecl.type.__class__.__name__ == 'Union':
        union_node = typedecl.type
        typedef_name = node.name
        union_name = union_node.name
        self.struct_info.update(typedef_name, union_name, union_node, True)
      # TODO(angiebird): Do we need to deal with enum here?
    self.generic_visit(node)


def build_struct_info(ast):
  v = StructDefVisitor()
  v.visit(ast)
  struct_info = v.struct_info
  struct_info.update_struct_item_list()
  return v.struct_info


class DeclStatus():

  def __init__(self, name, struct_item=None, is_ptr_decl=False):
    self.name = name
    self.struct_item = struct_item
    self.is_ptr_decl = is_ptr_decl

  def get_child_decl_status(self, decl_name):
    if self.struct_item != None:
      return self.struct_item.get_child_decl_status(decl_name)
    else:
      #TODO(angiebird): 2. Investigage the situation when a struct's definition can't be found.
      return None

  def __str__(self):
    return str(self.struct_item) + ' ' + str(self.name) + ' ' + str(
        self.is_ptr_decl)


def peel_ptr_decl(decl_type_node):
  """ Remove PtrDecl and ArrayDecl layer """
  is_ptr_decl = False
  peeled_decl_type_node = decl_type_node
  while peeled_decl_type_node.__class__.__name__ == 'PtrDecl' or peeled_decl_type_node.__class__.__name__ == 'ArrayDecl':
    is_ptr_decl = True
    peeled_decl_type_node = peeled_decl_type_node.type
  return is_ptr_decl, peeled_decl_type_node


def parse_peeled_decl_type_node(struct_info, node):
  struct_item = None
  if node.__class__.__name__ == 'TypeDecl':
    if node.type.__class__.__name__ == 'IdentifierType':
      identifier_type_node = node.type
      typedef_name = identifier_type_node.names[0]
      struct_item = struct_info.get_struct_by_typedef_name(typedef_name)
    elif node.type.__class__.__name__ == 'Struct':
      struct_node = node.type
      if struct_node.name != None:
        struct_item = struct_info.get_struct_by_struct_name(struct_node.name)
      else:
        struct_item = StructItem(None, None, struct_node, False)
        struct_item.compute_child_decl_map(struct_info)
    elif node.type.__class__.__name__ == 'Union':
      # TODO(angiebird): Special treatment for Union?
      struct_node = node.type
      if struct_node.name != None:
        struct_item = struct_info.get_struct_by_struct_name(struct_node.name)
      else:
        struct_item = StructItem(None, None, struct_node, True)
        struct_item.compute_child_decl_map(struct_info)
    elif node.type.__class__.__name__ == 'Enum':
      # TODO(angiebird): Special treatment for Union?
      struct_node = node.type
      struct_item = None
    else:
      print('Unrecognized peeled_decl_type_node.type',
            node.type.__class__.__name__)
  else:
    # debug_print(getframeinfo(currentframe()))
    # print(node.__class__.__name__)
    #TODO(angiebird): Do we need to take care of this part?
    pass

  return struct_item


def parse_decl_node(struct_info, decl_node):
  # struct_item is None if this decl_node is not a struct_item
  decl_node_name = decl_node.name
  decl_type_node = decl_node.type
  is_ptr_decl, peeled_decl_type_node = peel_ptr_decl(decl_type_node)
  struct_item = parse_peeled_decl_type_node(struct_info, peeled_decl_type_node)
  return DeclStatus(decl_node_name, struct_item, is_ptr_decl)


def get_lvalue_lead(lvalue_node):
  """return '&' or '*' of lvalue if available"""
  if lvalue_node.__class__.__name__ == 'UnaryOp' and lvalue_node.op == '&':
    return '&'
  elif lvalue_node.__class__.__name__ == 'UnaryOp' and lvalue_node.op == '*':
    return '*'
  return None


def parse_lvalue(lvalue_node):
  """get id_chain from lvalue"""
  id_chain = parse_lvalue_recursive(lvalue_node, [])
  return id_chain


def parse_lvalue_recursive(lvalue_node, id_chain):
  """cpi->rd->u -> (cpi->rd)->u"""
  if lvalue_node.__class__.__name__ == 'ID':
    id_chain.append(lvalue_node.name)
    id_chain.reverse()
    return id_chain
  elif lvalue_node.__class__.__name__ == 'StructRef':
    id_chain.append(lvalue_node.field.name)
    return parse_lvalue_recursive(lvalue_node.name, id_chain)
  elif lvalue_node.__class__.__name__ == 'ArrayRef':
    return parse_lvalue_recursive(lvalue_node.name, id_chain)
  elif lvalue_node.__class__.__name__ == 'UnaryOp' and lvalue_node.op == '&':
    return parse_lvalue_recursive(lvalue_node.expr, id_chain)
  elif lvalue_node.__class__.__name__ == 'UnaryOp' and lvalue_node.op == '*':
    return parse_lvalue_recursive(lvalue_node.expr, id_chain)
  else:
    return None


class FuncDefVisitor(c_ast.NodeVisitor):
  func_dictionary = {}

  def visit_FuncDef(self, node):
    func_name = node.decl.name
    self.func_dictionary[func_name] = node


def build_func_dictionary(ast):
  v = FuncDefVisitor()
  v.visit(ast)
  return v.func_dictionary


def get_func_start_coord(func_node):
  return func_node.coord


def find_end_node(node):
  node_list = []
  for c in node:
    node_list.append(c)
  if len(node_list) == 0:
    return node
  else:
    return find_end_node(node_list[-1])


def get_func_end_coord(func_node):
  return find_end_node(func_node).coord


def get_func_size(func_node):
  start_coord = get_func_start_coord(func_node)
  end_coord = get_func_end_coord(func_node)
  if start_coord.file == end_coord.file:
    return end_coord.line - start_coord.line + 1
  else:
    return None


def save_object(obj, filename):
  with open(filename, 'wb') as obj_fp:
    pickle.dump(obj, obj_fp, protocol=-1)


def load_object(filename):
  obj = None
  with open(filename, 'rb') as obj_fp:
    obj = pickle.load(obj_fp)
  return obj


def get_av1_ast(gen_ast=False):
  # TODO(angiebird): Generalize this path
  c_filename = './av1_pp.c'
  print('generate ast')
  ast = parse_file(c_filename)
  #save_object(ast, ast_file)
  print('finished generate ast')
  return ast


def get_func_param_id_map(func_def_node):
  param_id_map = {}
  func_decl = func_def_node.decl.type
  param_list = func_decl.args.params
  for decl in param_list:
    param_id_map[decl.name] = decl
  return param_id_map


class IDTreeStack():

  def __init__(self, global_id_tree):
    self.stack = deque()
    self.global_id_tree = global_id_tree

  def add_link_node(self, node, link_id_chain):
    link_node = self.add_id_node(link_id_chain)
    node.link_node = link_node
    node.link_id_chain = link_id_chain

  def push_id_tree(self, id_tree=None):
    if id_tree == None:
      id_tree = IDStatusNode()
    self.stack.append(id_tree)
    return id_tree

  def pop_id_tree(self):
    return self.stack.pop()

  def add_id_seed_node(self, id_seed, decl_status):
    return self.stack[-1].add_child(id_seed, decl_status)

  def get_id_seed_node(self, id_seed):
    idx = len(self.stack) - 1
    while idx >= 0:
      id_node = self.stack[idx].get_child(id_seed)
      if id_node != None:
        return id_node
      idx -= 1

    id_node = self.global_id_tree.get_child(id_seed)
    if id_node != None:
      return id_node
    return None

  def add_id_node(self, id_chain):
    id_seed = id_chain[0]
    id_seed_node = self.get_id_seed_node(id_seed)
    if id_seed_node == None:
      return None
    if len(id_chain) == 1:
      return id_seed_node
    return id_seed_node.add_descendant(id_chain[1:])

  def get_id_node(self, id_chain):
    id_seed = id_chain[0]
    id_seed_node = self.get_id_seed_node(id_seed)
    if id_seed_node == None:
      return None
    if len(id_chain) == 1:
      return id_seed_node
    return id_seed_node.get_descendant(id_chain[1:])

  def top(self):
    return self.stack[-1]


class IDStatusNode():

  def __init__(self, name=None, root=None):
    if root is None:
      self.root = self
    else:
      self.root = root

    self.name = name

    self.parent = None
    self.children = {}

    self.assign = False
    self.last_assign_coord = None
    self.refer = False
    self.last_refer_coord = None

    self.decl_status = None

    self.link_id_chain = None
    self.link_node = None

    self.visit = False

  def set_link_id_chain(self, link_id_chain):
    self.set_assign(False)
    self.link_id_chain = link_id_chain
    self.link_node = self.root.get_descendant(link_id_chain)

  def set_link_node(self, link_node):
    self.set_assign(False)
    self.link_id_chain = ['*']
    self.link_node = link_node

  def get_link_id_chain(self):
    return self.link_id_chain

  def get_concrete_node(self):
    if self.visit == True:
      # return None when there is a loop
      return None
    self.visit = True
    if self.link_node == None:
      self.visit = False
      return self
    else:
      concrete_node = self.link_node.get_concrete_node()
      self.visit = False
      if concrete_node == None:
        return self
      return concrete_node

  def set_assign(self, assign, coord=None):
    concrete_node = self.get_concrete_node()
    concrete_node.assign = assign
    concrete_node.last_assign_coord = coord

  def get_assign(self):
    concrete_node = self.get_concrete_node()
    return concrete_node.assign

  def set_refer(self, refer, coord=None):
    concrete_node = self.get_concrete_node()
    concrete_node.refer = refer
    concrete_node.last_refer_coord = coord

  def get_refer(self):
    concrete_node = self.get_concrete_node()
    return concrete_node.refer

  def set_parent(self, parent):
    concrete_node = self.get_concrete_node()
    concrete_node.parent = parent

  def add_child(self, name, decl_status=None):
    concrete_node = self.get_concrete_node()
    if name not in concrete_node.children:
      child_id_node = IDStatusNode(name, concrete_node.root)
      concrete_node.children[name] = child_id_node
      if decl_status == None:
        # Check if the child decl_status can be inferred from its parent's
        # decl_status
        if self.decl_status != None:
          decl_status = self.decl_status.get_child_decl_status(name)
      child_id_node.set_decl_status(decl_status)
    return concrete_node.children[name]

  def get_child(self, name):
    concrete_node = self.get_concrete_node()
    if name in concrete_node.children:
      return concrete_node.children[name]
    else:
      return None

  def add_descendant(self, id_chain):
    current_node = self.get_concrete_node()
    for name in id_chain:
      current_node.add_child(name)
      parent_node = current_node
      current_node = current_node.get_child(name)
      current_node.set_parent(parent_node)
    return current_node

  def get_descendant(self, id_chain):
    current_node = self.get_concrete_node()
    for name in id_chain:
      current_node = current_node.get_child(name)
      if current_node == None:
        return None
    return current_node

  def get_children(self):
    current_node = self.get_concrete_node()
    return current_node.children

  def set_decl_status(self, decl_status):
    current_node = self.get_concrete_node()
    current_node.decl_status = decl_status

  def get_decl_status(self):
    current_node = self.get_concrete_node()
    return current_node.decl_status

  def __str__(self):
    if self.link_id_chain is None:
      return str(self.name) + ' a: ' + str(int(self.assign)) + ' r: ' + str(
          int(self.refer))
    else:
      return str(self.name) + ' -> ' + ' '.join(self.link_id_chain)

  def collect_assign_refer_status(self,
                                  id_chain=None,
                                  assign_ls=None,
                                  refer_ls=None):
    if id_chain == None:
      id_chain = []
    if assign_ls == None:
      assign_ls = []
    if refer_ls == None:
      refer_ls = []
    id_chain.append(self.name)
    if self.assign:
      info_str = ' '.join([
          ' '.join(id_chain[1:]), 'a:',
          str(int(self.assign)), 'r:',
          str(int(self.refer)),
          str(self.last_assign_coord)
      ])
      assign_ls.append(info_str)
    if self.refer:
      info_str = ' '.join([
          ' '.join(id_chain[1:]), 'a:',
          str(int(self.assign)), 'r:',
          str(int(self.refer)),
          str(self.last_refer_coord)
      ])
      refer_ls.append(info_str)
    for c in self.children:
      self.children[c].collect_assign_refer_status(id_chain, assign_ls,
                                                   refer_ls)
    id_chain.pop()
    return assign_ls, refer_ls

  def show(self):
    assign_ls, refer_ls = self.collect_assign_refer_status()
    print('---- assign ----')
    for item in assign_ls:
      print(item)
    print('---- refer ----')
    for item in refer_ls:
      print(item)


class FuncInOutVisitor(c_ast.NodeVisitor):

  def __init__(self,
               func_def_node,
               struct_info,
               func_dictionary,
               keep_body_id_tree=True,
               call_param_map=None,
               global_id_tree=None,
               func_history=None,
               unknown=None):
    self.func_dictionary = func_dictionary
    self.struct_info = struct_info
    self.param_id_map = get_func_param_id_map(func_def_node)
    self.parent_node = None
    self.global_id_tree = global_id_tree
    self.body_id_tree = None
    self.keep_body_id_tree = keep_body_id_tree
    if func_history == None:
      self.func_history = {}
    else:
      self.func_history = func_history

    if unknown == None:
      self.unknown = []
    else:
      self.unknown = unknown

    self.id_tree_stack = IDTreeStack(global_id_tree)
    self.id_tree_stack.push_id_tree()

    #TODO move this part into a function
    for param in self.param_id_map:
      decl_node = self.param_id_map[param]
      decl_status = parse_decl_node(self.struct_info, decl_node)
      descendant = self.id_tree_stack.add_id_seed_node(decl_status.name,
                                                       decl_status)
      if call_param_map is not None and param in call_param_map:
        # This is a function call.
        # Map the input parameter to the caller's nodes
        # TODO(angiebird): Can we use add_link_node here?
        descendant.set_link_node(call_param_map[param])

  def get_id_tree_stack(self):
    return self.id_tree_stack

  def generic_visit(self, node):
    prev_parent = self.parent_node
    self.parent_node = node
    for c in node:
      self.visit(c)
    self.parent_node = prev_parent

  # TODO rename
  def add_new_id_tree(self, node):
    self.id_tree_stack.push_id_tree()
    self.generic_visit(node)
    id_tree = self.id_tree_stack.pop_id_tree()
    if self.parent_node == None and self.keep_body_id_tree == True:
      # this is function body
      self.body_id_tree = id_tree

  def visit_For(self, node):
    self.add_new_id_tree(node)

  def visit_Compound(self, node):
    self.add_new_id_tree(node)

  def visit_Decl(self, node):
    if node.type.__class__.__name__ != 'FuncDecl':
      decl_status = parse_decl_node(self.struct_info, node)
      descendant = self.id_tree_stack.add_id_seed_node(decl_status.name,
                                                       decl_status)
      if node.init is not None:
        init_id_chain = self.process_lvalue(node.init)
        if init_id_chain != None:
          if decl_status.struct_item is None:
            init_descendant = self.id_tree_stack.add_id_node(init_id_chain)
            if init_descendant != None:
              init_descendant.set_refer(True, node.coord)
            else:
              self.unknown.append(node)
            descendant.set_assign(True, node.coord)
          else:
            self.id_tree_stack.add_link_node(descendant, init_id_chain)
        else:
          self.unknown.append(node)
      else:
        descendant.set_assign(True, node.coord)
      self.generic_visit(node)

  def is_lvalue(self, node):
    if self.parent_node is None:
      # TODO(angiebird): Do every lvalue has parent_node != None?
      return False
    if self.parent_node.__class__.__name__ == 'StructRef':
      return False
    if self.parent_node.__class__.__name__ == 'ArrayRef' and node == self.parent_node.name:
      # if node == self.parent_node.subscript, the node could be lvalue
      return False
    if self.parent_node.__class__.__name__ == 'UnaryOp' and self.parent_node.op == '&':
      return False
    if self.parent_node.__class__.__name__ == 'UnaryOp' and self.parent_node.op == '*':
      return False
    return True

  def process_lvalue(self, node):
    id_chain = parse_lvalue(node)
    if id_chain == None:
      return id_chain
    elif id_chain[0] in self.struct_info.enum_value_dic:
      return None
    else:
      return id_chain

  def process_possible_lvalue(self, node):
    if self.is_lvalue(node):
      id_chain = self.process_lvalue(node)
      lead_char = get_lvalue_lead(node)
      # make sure the id is not an enum value
      if id_chain == None:
        self.unknown.append(node)
        return
      descendant = self.id_tree_stack.add_id_node(id_chain)
      if descendant == None:
        self.unknown.append(node)
        return
      decl_status = descendant.get_decl_status()
      if decl_status == None:
        descendant.set_assign(True, node.coord)
        descendant.set_refer(True, node.coord)
        self.unknown.append(node)
        return
      if self.parent_node.__class__.__name__ == 'Assignment':
        if node is self.parent_node.lvalue:
          if decl_status.struct_item != None:
            if len(id_chain) > 1:
              descendant.set_assign(True, node.coord)
            elif len(id_chain) == 1:
              if lead_char == '*':
                descendant.set_assign(True, node.coord)
              else:
                right_id_chain = self.process_lvalue(self.parent_node.rvalue)
                if right_id_chain != None:
                  self.id_tree_stack.add_link_node(descendant, right_id_chain)
                else:
                  #TODO(angiebird): 1.Find a better way to deal with this case.
                  descendant.set_assign(True, node.coord)
            else:
              debug_print(getframeinfo(currentframe()))
          else:
            descendant.set_assign(True, node.coord)
        elif node is self.parent_node.rvalue:
          if decl_status.struct_item is None:
            descendant.set_refer(True, node.coord)
            if lead_char == '&':
              descendant.set_assign(True, node.coord)
          else:
            left_id_chain = self.process_lvalue(self.parent_node.lvalue)
            left_lead_char = get_lvalue_lead(self.parent_node.lvalue)
            if left_id_chain != None:
              if len(left_id_chain) > 1:
                descendant.set_refer(True, node.coord)
              elif len(left_id_chain) == 1:
                if left_lead_char == '*':
                  descendant.set_refer(True, node.coord)
                else:
                  #TODO(angiebird): Check whether the other node is linked to this node.
                  pass
              else:
                self.unknown.append(self.parent_node.lvalue)
                debug_print(getframeinfo(currentframe()))
            else:
              self.unknown.append(self.parent_node.lvalue)
              debug_print(getframeinfo(currentframe()))
        else:
          debug_print(getframeinfo(currentframe()))
      elif self.parent_node.__class__.__name__ == 'UnaryOp':
        # TODO(angiebird): Consider +=, *=, -=, /= etc
        if self.parent_node.op == '--' or self.parent_node.op == '++' or\
        self.parent_node.op == 'p--' or self.parent_node.op == 'p++':
          descendant.set_assign(True, node.coord)
          descendant.set_refer(True, node.coord)
        else:
          descendant.set_refer(True, node.coord)
      elif self.parent_node.__class__.__name__ == 'Decl':
        #The logic is at visit_Decl
        pass
      elif self.parent_node.__class__.__name__ == 'ExprList':
        #The logic is at visit_FuncCall
        pass
      else:
        descendant.set_refer(True, node.coord)

  def visit_ID(self, node):
    # If the parent is a FuncCall, this ID is a function name.
    if self.parent_node.__class__.__name__ != 'FuncCall':
      self.process_possible_lvalue(node)
    self.generic_visit(node)

  def visit_StructRef(self, node):
    self.process_possible_lvalue(node)
    self.generic_visit(node)

  def visit_ArrayRef(self, node):
    self.process_possible_lvalue(node)
    self.generic_visit(node)

  def visit_UnaryOp(self, node):
    if node.op == '&' or node.op == '*':
      self.process_possible_lvalue(node)
    self.generic_visit(node)

  def visit_FuncCall(self, node):
    if node.name.__class__.__name__ == 'ID':
      if node.name.name in self.func_dictionary:
        if node.name.name not in self.func_history:
          self.func_history[node.name.name] = True
          func_def_node = self.func_dictionary[node.name.name]
          call_param_map = self.process_func_call(node, func_def_node)

          visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                                     self.func_dictionary, False,
                                     call_param_map, self.global_id_tree,
                                     self.func_history, self.unknown)
          visitor.visit(func_def_node.body)
    else:
      self.unknown.append(node)
    self.generic_visit(node)

  def process_func_call(self, func_call_node, func_def_node):
    # set up a refer/assign for func parameters
    # return call_param_map
    call_param_ls = func_call_node.args.exprs
    call_param_map = {}

    func_decl = func_def_node.decl.type
    decl_param_ls = func_decl.args.params
    for param_node, decl_node in zip(call_param_ls, decl_param_ls):
      id_chain = self.process_lvalue(param_node)
      if id_chain != None:
        descendant = self.id_tree_stack.add_id_node(id_chain)
        if descendant == None:
          self.unknown.append(param_node)
        else:
          decl_status = descendant.get_decl_status()
          if decl_status != None:
            if decl_status.struct_item == None:
              if decl_status.is_ptr_decl == True:
                descendant.set_assign(True, param_node.coord)
                descendant.set_refer(True, param_node.coord)
              else:
                descendant.set_refer(True, param_node.coord)
            else:
              call_param_map[decl_node.name] = descendant
          else:
            self.unknown.append(param_node)
      else:
        self.unknown.append(param_node)
    return call_param_map


def build_global_id_tree(ast, struct_info):
  global_id_tree = IDStatusNode()
  for node in ast.ext:
    if node.__class__.__name__ == 'Decl':
      # id tree is for tracking assign/refer status
      # we don't care about function id because they can't be changed
      if node.type.__class__.__name__ != 'FuncDecl':
        decl_status = parse_decl_node(struct_info, node)
        descendant = global_id_tree.add_child(decl_status.name, decl_status)
  return global_id_tree


class FuncAnalyzer():

  def __init__(self):
    self.ast = get_av1_ast()
    self.struct_info = build_struct_info(self.ast)
    self.func_dictionary = build_func_dictionary(self.ast)
    self.global_id_tree = build_global_id_tree(self.ast, self.struct_info)

  def analyze(self, func_name):
    if func_name in self.func_dictionary:
      func_def_node = self.func_dictionary[func_name]
      visitor = FuncInOutVisitor(func_def_node, self.struct_info,
                                 self.func_dictionary, True, None,
                                 self.global_id_tree)
      visitor.visit(func_def_node.body)
      root = visitor.get_id_tree_stack()
      root.top().show()
    else:
      print(func_name, "doesn't exist")


if __name__ == '__main__':
  fa = FuncAnalyzer()
  fa.analyze('tpl_get_satd_cost')
  pass
