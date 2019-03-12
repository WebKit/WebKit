#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright (C) 2018 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import re
import subprocess
import sys

from webkitpy.common.system.systemhost import SystemHost
sys.path.append(SystemHost().path_to_lldb_python_directory())
import lldb

# lldb Python reference:
# <https://lldb.llvm.org/python_reference/>


class AnsiColors:
    BLUE = '\033[94m'
    WARNING = '\033[93m'
    ENDCOLOR = '\033[0m'


class ClassLayoutBase(object):

    MEMBER_NAME_KEY = 'name'
    MEMBER_TYPE_KEY = 'type'
    MEMBER_TYPE_CLASS = 'type_class'  # lldb.eTypeClassStruct etc. Values here: <https://lldb.llvm.org/python_reference/_lldb%27-module.html#eTypeClassAny>
    MEMBER_CLASS_INSTANCE = 'class_instance'
    MEMBER_BYTE_SIZE = 'byte_size'
    MEMBER_OFFSET = 'offset'  # offset is local to this class.
    MEMBER_OFFSET_IN_BITS = 'offset_in_bits'
    MEMBER_IS_BITFIELD = 'is_bitfield'
    MEMBER_BITFIELD_BIT_SIZE = 'bit_size'
    PADDING_BYTES_TYPE = 'padding'
    PADDING_BITS_TYPE = 'padding_bits'
    PADDING_BITS_SIZE = 'padding_bits_size'
    PADDING_NAME = ''

    def __init__(self, typename):
        self.typename = typename
        self.total_byte_size = 0
        self.total_pad_bytes = 0
        self.data_members = []

    def __ne__(self, other):
        return not self.__eq__(other)

    def __eq__(self, other):
        if not isinstance(other, self.__class__):
            return False

        if self.total_byte_size != other.total_byte_size:
            return False

        if self.total_pad_bytes != other.total_pad_bytes:
            return False

        if len(self.data_members) != other.data_members:
            return False

        for i in range(len(self.data_members)):
            self_member = self.data_members[i]
            other_member = other.data_members[i]
            if self_member != other_member:
                return False

        return True

    def _to_string_recursive(self, str_list, colorize, member_name=None, depth=0, total_offset=0):
        type_start = AnsiColors.BLUE if colorize else ''
        warn_start = AnsiColors.WARNING if colorize else ''
        color_end = AnsiColors.ENDCOLOR if colorize else ''

        if member_name:
            str_list.append('%+4u <%3u> %s%s%s%s %s' % (total_offset, self.total_byte_size, '    ' * depth, type_start, self.typename, color_end, member_name))
        else:
            str_list.append('%+4u <%3u> %s%s%s%s' % (total_offset, self.total_byte_size, '    ' * depth, type_start, self.typename, color_end))

        start_offset = total_offset

        for data_member in self.data_members:
            member_total_offset = start_offset + data_member[self.MEMBER_OFFSET]
            if self.MEMBER_CLASS_INSTANCE in data_member:
                data_member[self.MEMBER_CLASS_INSTANCE]._to_string_recursive(str_list, colorize, data_member[self.MEMBER_NAME_KEY], depth + 1, member_total_offset)
            else:
                byte_size = data_member[self.MEMBER_BYTE_SIZE]

                if self.MEMBER_IS_BITFIELD in data_member:
                    num_bits = data_member[self.MEMBER_BITFIELD_BIT_SIZE]
                    str_list.append('%+4u < :%1u> %s  %s %s : %d' % (member_total_offset, num_bits, '    ' * depth, data_member[self.MEMBER_TYPE_KEY], data_member[self.MEMBER_NAME_KEY], num_bits))
                elif data_member[self.MEMBER_TYPE_KEY] == self.PADDING_BYTES_TYPE:
                    str_list.append('%+4u <%3u> %s  %s<PADDING: %d %s>%s' % (member_total_offset, byte_size, '    ' * depth, warn_start, byte_size, 'bytes' if byte_size > 1 else 'byte', color_end))
                elif data_member[self.MEMBER_TYPE_KEY] == self.PADDING_BITS_TYPE:
                    padding_bits = data_member[self.PADDING_BITS_SIZE]
                    str_list.append('%+4u < :%1u> %s  %s<UNUSED BITS: %d %s>%s' % (member_total_offset, padding_bits, '    ' * depth, warn_start, padding_bits, 'bits' if padding_bits > 1 else 'bit', color_end))
                else:
                    str_list.append('%+4u <%3u> %s  %s %s' % (member_total_offset, byte_size, '    ' * depth, data_member[self.MEMBER_TYPE_KEY], data_member[self.MEMBER_NAME_KEY]))

    def as_string_list(self, colorize=False):
        str_list = []
        self._to_string_recursive(str_list, colorize)
        str_list.append('Total byte size: %d' % (self.total_byte_size))
        str_list.append('Total pad bytes: %d' % (self.total_pad_bytes))
        if self.total_pad_bytes > 0:
            str_list.append('Padding percentage: %2.2f %%' % ((float(self.total_pad_bytes) / float(self.total_byte_size)) * 100.0))
        return str_list

    def as_string(self, colorize=False):
        return '\n'.join(self.as_string_list(colorize))

    def dump(self, colorize=True):
        print self.as_string(colorize)


class ClassLayout(ClassLayoutBase):
    "Stores the layout of a class or struct."

    def __init__(self, target, type, containerClass=None, derivedClass=None):
        super(ClassLayout, self).__init__(type.GetName())

        self.target = target
        self.type = type
        self.total_byte_size = self.type.GetByteSize()
        self.pointer_size = self.target.GetAddressByteSize()
        self.total_pad_bytes = 0
        self.top_level_pad_bytes = 0
        self.data_members = []
        self.virtual_base_classes = self._virtual_base_classes_dictionary()
        self._parse(containerClass, derivedClass)
        if containerClass == None and derivedClass == None:
            self.total_pad_bytes = self._compute_padding()

    def _has_polymorphic_non_virtual_base_class(self):
        num_base_classes = self.type.GetNumberOfDirectBaseClasses()
        for i in range(num_base_classes):
            base_class = self.type.GetDirectBaseClassAtIndex(i)
            if base_class.GetName() in self.virtual_base_classes:
                continue

            if base_class.GetType().IsPolymorphicClass():
                return True

        return False

    def _virtual_base_classes_dictionary(self):
        result = {}
        num_virtual_base_classes = self.type.GetNumberOfVirtualBaseClasses()
        for i in range(num_virtual_base_classes):
            virtual_base = self.type.GetVirtualBaseClassAtIndex(i)
            result[virtual_base.GetName()] = ClassLayout(self.target, virtual_base.GetType(), self)
        return result

    def _parse(self, containerClass=None, derivedClass=None):
        # It's moot where we actually show the vtable pointer, but to match clang -fdump-record-layouts, assign it to the
        # base-most polymorphic class (unless virtual inheritance is involved).
        if self.type.IsPolymorphicClass() and not self._has_polymorphic_non_virtual_base_class():
            data_member = {
                self.MEMBER_NAME_KEY : '__vtbl_ptr_type * _vptr',
                self.MEMBER_TYPE_KEY : '',
                self.MEMBER_BYTE_SIZE : self.pointer_size,
                self.MEMBER_OFFSET : 0
            }
            self.data_members.append(data_member)

        num_direct_base_classes = self.type.GetNumberOfDirectBaseClasses()
        if num_direct_base_classes > 0:
            for i in range(num_direct_base_classes):
                direct_base = self.type.GetDirectBaseClassAtIndex(i)

                # virtual base classes are also considered direct base classes, but we need to skip those here.
                if direct_base.GetName() in self.virtual_base_classes:
                    continue

                member_type = direct_base.GetType()
                member_typename = member_type.GetName()
                member_canonical_type = member_type.GetCanonicalType()
                member_type_class = member_canonical_type.GetTypeClass()

                member_name = direct_base.GetName()
                member_offset = direct_base.GetOffsetInBytes()
                member_byte_size = member_type.GetByteSize()

                base_class = ClassLayout(self.target, member_type, None, self)

                data_member = {
                    self.MEMBER_NAME_KEY : member_name,
                    self.MEMBER_TYPE_KEY : member_typename,
                    self.MEMBER_TYPE_CLASS : member_type_class,
                    self.MEMBER_CLASS_INSTANCE : base_class,
                    self.MEMBER_BYTE_SIZE : member_byte_size,
                    self.MEMBER_OFFSET : member_offset,
                }

                self.data_members.append(data_member)

        num_fields = self.type.GetNumberOfFields()
        for i in range(num_fields):
            field = self.type.GetFieldAtIndex(i)

            member_type = field.GetType()
            member_typename = member_type.GetName()
            member_canonical_type = member_type.GetCanonicalType()
            member_type_class = member_canonical_type.GetTypeClass()

            member_name = field.GetName()
            member_offset = field.GetOffsetInBytes()
            member_byte_size = member_type.GetByteSize()

            data_member = {
                self.MEMBER_NAME_KEY : member_name,
                self.MEMBER_TYPE_KEY : member_typename,
                self.MEMBER_TYPE_CLASS : member_type_class,
                self.MEMBER_BYTE_SIZE : member_byte_size,
                self.MEMBER_OFFSET : member_offset,
            }

            if field.IsBitfield():
                data_member[self.MEMBER_IS_BITFIELD] = True
                data_member[self.MEMBER_BITFIELD_BIT_SIZE] = field.GetBitfieldSizeInBits()
                data_member[self.MEMBER_OFFSET_IN_BITS] = field.GetOffsetInBits()
                # For bitfields, member_byte_size was computed based on the field type without the bitfield modifiers, so compute from the number of bits.
                data_member[self.MEMBER_BYTE_SIZE] = (field.GetBitfieldSizeInBits() + 7) / 8
            elif member_type_class == lldb.eTypeClassStruct or member_type_class == lldb.eTypeClassClass:
                nested_class = ClassLayout(self.target, member_type, self)
                data_member[self.MEMBER_CLASS_INSTANCE] = nested_class

            self.data_members.append(data_member)

        # "For each distinct base class that is specified virtual, the most derived object contains only one base class subobject of that type,
        # even if the class appears many times in the inheritance hierarchy (as long as it is inherited virtual every time)."
        num_virtual_base_classes = self.type.GetNumberOfVirtualBaseClasses()
        if derivedClass == None and num_virtual_base_classes > 0:
            for i in range(num_virtual_base_classes):
                virtual_base = self.type.GetVirtualBaseClassAtIndex(i)
                member_type = virtual_base.GetType()
                member_typename = member_type.GetName()
                member_canonical_type = member_type.GetCanonicalType()
                member_type_class = member_canonical_type.GetTypeClass()

                member_name = virtual_base.GetName()
                member_offset = virtual_base.GetOffsetInBytes()
                member_byte_size = member_type.GetByteSize()

                nested_class = ClassLayout(self.target, member_type, None, self)

                data_member = {
                    self.MEMBER_NAME_KEY : member_name,
                    self.MEMBER_TYPE_KEY : member_typename,
                    self.MEMBER_TYPE_CLASS : member_type_class,
                    self.MEMBER_CLASS_INSTANCE : nested_class,
                    self.MEMBER_BYTE_SIZE : member_byte_size,
                    self.MEMBER_OFFSET : member_offset,
                }

                self.data_members.append(data_member)

    # clang -fdump-record-layouts shows "(empty)" for such classes, but I can't find any way to access this information via lldb.
    def _probably_has_empty_base_class_optimization(self):
        if self.total_byte_size > 1:
            return False

        if len(self.data_members) > 1:
            return False

        if len(self.data_members) == 1:
            data_member = self.data_members[0]
            if self.MEMBER_CLASS_INSTANCE in data_member:
                return data_member[self.MEMBER_CLASS_INSTANCE]._probably_has_empty_base_class_optimization()

        return True

    def _compute_padding_recursive(self, total_offset=0, depth=0, containerClass=None):
        padding_bytes = 0
        start_offset = total_offset
        current_offset = total_offset

        i = 0
        while i < len(self.data_members):
            data_member = self.data_members[i]
            member_offset = data_member[self.MEMBER_OFFSET]

            probably_empty_base_class = False
            if self.MEMBER_CLASS_INSTANCE in data_member:
                probably_empty_base_class = member_offset == 0 and data_member[self.MEMBER_CLASS_INSTANCE]._probably_has_empty_base_class_optimization()

            byte_size = data_member[self.MEMBER_BYTE_SIZE]

            if not probably_empty_base_class:
                padding_size = start_offset + member_offset - current_offset

                if padding_size > 0:
                    padding_member = {
                        self.MEMBER_NAME_KEY : self.PADDING_NAME,
                        self.MEMBER_TYPE_KEY : self.PADDING_BYTES_TYPE,
                        self.MEMBER_BYTE_SIZE : padding_size,
                        self.MEMBER_OFFSET : current_offset - start_offset,
                    }

                    self.data_members.insert(i, padding_member)
                    padding_bytes += padding_size
                    if depth == 0 and padding_size < 8:
                        self.top_level_pad_bytes += padding_size
                    i += 1

                if self.MEMBER_IS_BITFIELD in data_member:
                    next_member_is_bitfield = False
                    if i < len(self.data_members) - 1:
                        next_data_member = self.data_members[i + 1]
                        next_member_is_bitfield = self.MEMBER_IS_BITFIELD in next_data_member

                    if not next_member_is_bitfield:
                        end_bit_offset = data_member[self.MEMBER_OFFSET_IN_BITS] + data_member[self.MEMBER_BITFIELD_BIT_SIZE]
                        unused_bits = (8 - end_bit_offset) % 8
                        if unused_bits:
                            bit_padding_member = {
                                self.MEMBER_NAME_KEY : self.PADDING_NAME,
                                self.MEMBER_TYPE_KEY : self.PADDING_BITS_TYPE,
                                self.MEMBER_BYTE_SIZE : data_member[self.MEMBER_BYTE_SIZE],
                                self.PADDING_BITS_SIZE : unused_bits,
                                self.MEMBER_OFFSET : data_member[self.MEMBER_OFFSET],
                            }
                            self.data_members.insert(i + 1, bit_padding_member)
                            i += 1

                current_offset = start_offset + member_offset

            if self.MEMBER_CLASS_INSTANCE in data_member:
                [padding, offset] = data_member[self.MEMBER_CLASS_INSTANCE]._compute_padding_recursive(current_offset, depth + 1, self)
                padding_bytes += padding
                current_offset = offset
            else:
                current_offset += byte_size

            i += 1

        # Look for padding at the end.
        if containerClass == None:
            padding_size = self.total_byte_size - current_offset
            if padding_size > 0:
                padding_member = {
                    self.MEMBER_NAME_KEY : self.PADDING_NAME,
                    self.MEMBER_TYPE_KEY : self.PADDING_BYTES_TYPE,
                    self.MEMBER_BYTE_SIZE : padding_size,
                    self.MEMBER_OFFSET : current_offset - start_offset,
                }
                self.data_members.append(padding_member)
                padding_bytes += padding_size
                self.top_level_pad_bytes += padding_size

        return [padding_bytes, current_offset]

    def _compute_padding(self):
        [padding, offset] = self._compute_padding_recursive()
        return padding


class LLDBDebuggerInstance:
    "Wraps an instance of lldb.SBDebugger and vends ClassLayouts"

    def __init__(self, binary_path, architecture):
        self.binary_path = binary_path
        self.architecture = architecture

        self.debugger = lldb.SBDebugger.Create()
        self.debugger.SetAsync(False)
        architecture = self.architecture
        if not architecture:
            architecture = self._get_first_file_architecture()

        self.target = self.debugger.CreateTargetWithFileAndArch(str(self.binary_path), architecture)
        if not self.target:
            print "Failed to make target for " + self.binary_path

        self.module = self.target.GetModuleAtIndex(0)
        if not self.module:
            print "Failed to get first module in " + self.binary_path

    def __del__(self):
        if lldb:
            lldb.SBDebugger.Destroy(self.debugger)

    def _get_first_file_architecture(self):
        p = re.compile('shared library +(\w+)$')
        file_result = subprocess.check_output(["file", self.binary_path]).split('\n')
        arches = []
        for line in file_result:
            match = p.search(line)
            if match:
                arches.append(match.group(1))

        if len(arches) > 0:
            return arches[0]

        return lldb.LLDB_ARCH_DEFAULT

    def dump_layout_for_classname(self, classname):
        types = self.module.FindTypes(classname)
        if not types.GetSize():
            print 'error: no type matches "%s" in "%s"' % (classname, self.module.file)
            return None
        for t in types:
            ClassLayout(self.target, t).dump()

    def dump_all_wasteful_layouts(self):
        types = self.module.GetTypes(lldb.eTypeClassClass | lldb.eTypeClassStruct)
        seenTypes = set()
        for t in types:
            if t.GetName() in seenTypes:
                continue
            seenTypes.add(t.GetName())
            classLayout = ClassLayout(self.target, t)
            if classLayout.top_level_pad_bytes >= 8:
                classLayout.dump()
                print ""
