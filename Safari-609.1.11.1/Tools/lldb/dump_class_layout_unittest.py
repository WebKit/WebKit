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

import atexit
import difflib
import lldb
import os
import sys
import unittest

from lldb_dump_class_layout import LLDBDebuggerInstance, ClassLayoutBase
from webkitpy.common.system.systemhost import SystemHost

# Run these tests with ./Tools/Scripts/test-webkitpy dump_class_layout_unittest
# Run a single test with e.g. ./Tools/Scripts/test-webkitpy dump_class_layout_unittest.TestDumpClassLayout.serial_test_ClassWithUniquePtrs
# Compare with clang's output: clang++ -Xclang -fdump-record-layouts DumpClassLayoutTesting.cpp

debugger_instance = None


@atexit.register
def destroy_cached_debug_session():
    debugger_instance = None


class TestDumpClassLayout(unittest.TestCase):
    @classmethod
    def shouldSkip(cls):
        return not SystemHost().platform.is_mac()

    @classmethod
    def setUpClass(cls):
        global debugger_instance
        if not debugger_instance:
            lldbWebKitTesterExecutable = str(os.environ['LLDB_WEBKIT_TESTER_EXECUTABLE'])

            architecture = 'x86_64'
            debugger_instance = LLDBDebuggerInstance(lldbWebKitTesterExecutable, architecture)
            if not debugger_instance:
                print 'Failed to create lldb debugger instance for %s' % (lldbWebKitTesterExecutable)

    def setUp(self):
        super(TestDumpClassLayout, self).setUp()
        self.maxDiff = None
        self.addTypeEqualityFunc(str, self.assertMultiLineEqual)

    def serial_test_BasicClassLayout(self):
        EXPECTED_RESULT = """  +0 <  8> BasicClassLayout
  +0 <  4>   int intMember
  +4 <  1>   bool boolMember
  +5 <  3>   <PADDING: 3 bytes>
Total byte size: 8
Total pad bytes: 3
Padding percentage: 37.50 %"""
        actual_layout = debugger_instance.layout_for_classname('BasicClassLayout')
        self.assertEqual(EXPECTED_RESULT, actual_layout.as_string())

    def serial_test_PaddingBetweenClassMembers(self):
        EXPECTED_RESULT = """  +0 < 16> PaddingBetweenClassMembers
  +0 <  8>     BasicClassLayout basic1
  +0 <  4>       int intMember
  +4 <  1>       bool boolMember
  +5 <  3>   <PADDING: 3 bytes>
  +8 <  8>     BasicClassLayout basic2
  +8 <  4>       int intMember
 +12 <  1>       bool boolMember
 +13 <  3>   <PADDING: 3 bytes>
Total byte size: 16
Total pad bytes: 6
Padding percentage: 37.50 %"""
        actual_layout = debugger_instance.layout_for_classname('PaddingBetweenClassMembers')
        self.assertEqual(EXPECTED_RESULT, actual_layout.as_string())

    def serial_test_BoolPaddingClass(self):
        EXPECTED_RESULT = """  +0 < 12> BoolPaddingClass
  +0 <  1>   bool bool1
  +1 <  1>   bool bool2
  +2 <  1>   bool bool3
  +3 <  1>   <PADDING: 1 byte>
  +4 <  8>     BoolMemberFirst memberClass
  +4 <  1>       bool boolMember
  +5 <  3>       <PADDING: 3 bytes>
  +8 <  4>       int intMember
Total byte size: 12
Total pad bytes: 4
Padding percentage: 33.33 %"""
        actual_layout = debugger_instance.layout_for_classname('BoolPaddingClass')
        self.assertEqual(EXPECTED_RESULT, actual_layout.as_string())

    def serial_test_ClassWithEmptyClassMembers(self):
        EXPECTED_RESULT = """  +0 < 12> ClassWithEmptyClassMembers
  +0 <  4>   int intMember
  +4 <  1>     EmptyClass empty1
  +4 <  1>   <PADDING: 1 byte>
  +5 <  1>   bool boolMember
  +6 <  1>     EmptyClass empty2
  +6 <  2>   <PADDING: 2 bytes>
  +8 <  4>   int intMember2
Total byte size: 12
Total pad bytes: 3
Padding percentage: 25.00 %"""
        actual_layout = debugger_instance.layout_for_classname('ClassWithEmptyClassMembers')
        self.assertEqual(EXPECTED_RESULT, actual_layout.as_string())

    def serial_test_SimpleVirtualClass(self):
        EXPECTED_RESULT = """  +0 < 24> SimpleVirtualClass
  +0 <  8>    __vtbl_ptr_type * _vptr
  +8 <  4>   int intMember
 +12 <  4>   <PADDING: 4 bytes>
 +16 <  8>   double doubleMember
Total byte size: 24
Total pad bytes: 4
Padding percentage: 16.67 %"""
        actual_layout = debugger_instance.layout_for_classname('SimpleVirtualClass')
        self.assertEqual(EXPECTED_RESULT, actual_layout.as_string())

    def serial_test_VirtualClassWithNonVirtualBase(self):
        EXPECTED_RESULT = """  +0 < 24> VirtualClassWithNonVirtualBase
  +0 <  8>    __vtbl_ptr_type * _vptr
  +8 <  8>     BasicClassLayout BasicClassLayout
  +8 <  4>       int intMember
 +12 <  1>       bool boolMember
 +13 <  3>   <PADDING: 3 bytes>
 +16 <  8>   double doubleMember
Total byte size: 24
Total pad bytes: 3
Padding percentage: 12.50 %"""
        actual_layout = debugger_instance.layout_for_classname('VirtualClassWithNonVirtualBase')
        self.assertEqual(EXPECTED_RESULT, actual_layout.as_string())

    def serial_test_InterleavedVirtualNonVirtual(self):
        EXPECTED_RESULT = """  +0 < 16> InterleavedVirtualNonVirtual
  +0 < 16>     ClassWithVirtualBase ClassWithVirtualBase
  +0 <  8>         VirtualBaseClass VirtualBaseClass
  +0 <  8>            __vtbl_ptr_type * _vptr
  +8 <  1>       bool boolMember
  +9 <  1>   bool boolMember
 +10 <  6>   <PADDING: 6 bytes>
Total byte size: 16
Total pad bytes: 6
Padding percentage: 37.50 %"""
        actual_layout = debugger_instance.layout_for_classname('InterleavedVirtualNonVirtual')
        self.assertEqual(EXPECTED_RESULT, actual_layout.as_string())

    def serial_test_ClassWithTwoVirtualBaseClasses(self):
        EXPECTED_RESULT = """  +0 < 24> ClassWithTwoVirtualBaseClasses
  +0 <  8>     VirtualBaseClass VirtualBaseClass
  +0 <  8>        __vtbl_ptr_type * _vptr
  +8 <  8>     VirtualBaseClass2 VirtualBaseClass2
  +8 <  8>        __vtbl_ptr_type * _vptr
 +16 <  1>   bool boolMember
 +17 <  7>   <PADDING: 7 bytes>
Total byte size: 24
Total pad bytes: 7
Padding percentage: 29.17 %"""
        actual_layout = debugger_instance.layout_for_classname('ClassWithTwoVirtualBaseClasses')
        self.assertEqual(EXPECTED_RESULT, actual_layout.as_string())

    def serial_test_ClassWithVirtualInheritance(self):
        EXPECTED_RESULT = """  +0 < 64> ClassWithVirtualInheritance
  +0 < 32>     VirtualInheritingA VirtualInheritingA
  +0 <  8>        __vtbl_ptr_type * _vptr
  +8 <  4>       int intMemberA
 +12 <  4>   <PADDING: 4 bytes>
 +16 < 40>     VirtualInheritingB VirtualInheritingB
 +16 <  8>        __vtbl_ptr_type * _vptr
 +24 <  8>         BasicClassLayout BasicClassLayout
 +24 <  4>           int intMember
 +28 <  1>           bool boolMember
 +29 <  3>       <PADDING: 3 bytes>
 +32 <  4>       int intMemberB
 +36 <  4>   <PADDING: 4 bytes>
 +40 <  8>   double derivedMember
 +48 < 16>     VirtualBase VirtualBase
 +48 <  8>        __vtbl_ptr_type * _vptr
 +56 <  1>       bool baseMember
 +57 <  7>   <PADDING: 7 bytes>
Total byte size: 64
Total pad bytes: 18
Padding percentage: 28.12 %"""
        actual_layout = debugger_instance.layout_for_classname('ClassWithVirtualInheritance')
        self.assertEqual(EXPECTED_RESULT, actual_layout.as_string())

    def serial_test_ClassWithInheritanceAndClassMember(self):
        EXPECTED_RESULT = """  +0 < 80> ClassWithInheritanceAndClassMember
  +0 < 32>     VirtualInheritingA VirtualInheritingA
  +0 <  8>        __vtbl_ptr_type * _vptr
  +8 <  4>       int intMemberA
 +12 <  4>   <PADDING: 4 bytes>
 +16 < 40>     VirtualInheritingB dataMember
 +16 <  8>        __vtbl_ptr_type * _vptr
 +24 <  8>         BasicClassLayout BasicClassLayout
 +24 <  4>           int intMember
 +28 <  1>           bool boolMember
 +29 <  3>       <PADDING: 3 bytes>
 +32 <  4>       int intMemberB
 +36 <  4>       <PADDING: 4 bytes>
 +40 < 16>         VirtualBase VirtualBase
 +40 <  8>            __vtbl_ptr_type * _vptr
 +48 <  1>           bool baseMember
 +49 <  7>   <PADDING: 7 bytes>
 +56 <  8>   double derivedMember
 +64 < 16>     VirtualBase VirtualBase
 +64 <  8>        __vtbl_ptr_type * _vptr
 +72 <  1>       bool baseMember
 +73 <  7>   <PADDING: 7 bytes>
Total byte size: 80
Total pad bytes: 25
Padding percentage: 31.25 %"""
        actual_layout = debugger_instance.layout_for_classname('ClassWithInheritanceAndClassMember')
        self.assertEqual(EXPECTED_RESULT, actual_layout.as_string())

    def serial_test_DerivedClassWithIndirectVirtualInheritance(self):
        EXPECTED_RESULT = """  +0 < 72> DerivedClassWithIndirectVirtualInheritance
  +0 < 64>     ClassWithVirtualInheritance ClassWithVirtualInheritance
  +0 < 32>         VirtualInheritingA VirtualInheritingA
  +0 <  8>            __vtbl_ptr_type * _vptr
  +8 <  4>           int intMemberA
 +12 <  4>       <PADDING: 4 bytes>
 +16 < 40>         VirtualInheritingB VirtualInheritingB
 +16 <  8>            __vtbl_ptr_type * _vptr
 +24 <  8>             BasicClassLayout BasicClassLayout
 +24 <  4>               int intMember
 +28 <  1>               bool boolMember
 +29 <  3>           <PADDING: 3 bytes>
 +32 <  4>           int intMemberB
 +36 <  4>       <PADDING: 4 bytes>
 +40 <  8>       double derivedMember
 +48 <  8>   long mostDerivedMember
 +56 < 16>     VirtualBase VirtualBase
 +56 <  8>        __vtbl_ptr_type * _vptr
 +64 <  1>       bool baseMember
 +65 <  7>   <PADDING: 7 bytes>
Total byte size: 72
Total pad bytes: 18
Padding percentage: 25.00 %"""
        actual_layout = debugger_instance.layout_for_classname('DerivedClassWithIndirectVirtualInheritance')
        self.assertEqual(EXPECTED_RESULT, actual_layout.as_string())

    def serial_test_ClassWithClassMembers(self):
        EXPECTED_RESULT = """  +0 < 72> ClassWithClassMembers
  +0 <  1>   bool boolMember
  +1 <  3>   <PADDING: 3 bytes>
  +4 <  8>     BasicClassLayout classMember
  +4 <  4>       int intMember
  +8 <  1>       bool boolMember
  +9 <  7>   <PADDING: 7 bytes>
 +16 < 24>     ClassWithTwoVirtualBaseClasses virtualClassesMember
 +16 <  8>         VirtualBaseClass VirtualBaseClass
 +16 <  8>            __vtbl_ptr_type * _vptr
 +24 <  8>         VirtualBaseClass2 VirtualBaseClass2
 +24 <  8>            __vtbl_ptr_type * _vptr
 +32 <  1>       bool boolMember
 +33 <  7>   <PADDING: 7 bytes>
 +40 <  8>   double doubleMember
 +48 < 16>     ClassWithVirtualBase virtualClassMember
 +48 <  8>         VirtualBaseClass VirtualBaseClass
 +48 <  8>            __vtbl_ptr_type * _vptr
 +56 <  1>       bool boolMember
 +57 <  7>   <PADDING: 7 bytes>
 +64 <  4>   int intMember
 +68 <  4>   <PADDING: 4 bytes>
Total byte size: 72
Total pad bytes: 28
Padding percentage: 38.89 %"""
        actual_layout = debugger_instance.layout_for_classname('ClassWithClassMembers')
        self.assertEqual(EXPECTED_RESULT, actual_layout.as_string())

    def serial_test_ClassWithBitfields(self):
        EXPECTED_RESULT = """  +0 < 12> ClassWithBitfields
  +0 <  1>   bool boolMember
  +1 < :1>   unsigned int bitfield1 : 1
  +1 < :2>   unsigned int bitfield2 : 2
  +1 < :1>   unsigned int bitfield3 : 1
  +1 < :1>   bool bitfield4 : 1
  +1 < :2>   bool bitfield5 : 2
  +1 < :1>   bool bitfield6 : 1
  +2 <  2>   <PADDING: 2 bytes>
  +4 <  4>   int intMember
  +8 < :1>   unsigned int bitfield7 : 1
  +8 < :1>   bool bitfield8 : 1
  +8 < :6>   <UNUSED BITS: 6 bits>
  +9 <  3>   <PADDING: 3 bytes>
Total byte size: 12
Total pad bytes: 5
Padding percentage: 41.67 %"""
        actual_layout = debugger_instance.layout_for_classname('ClassWithBitfields')
        self.assertEqual(EXPECTED_RESULT, actual_layout.as_string())

    def serial_test_ClassWithPaddedBitfields(self):
        EXPECTED_RESULT = """  +0 < 16> ClassWithPaddedBitfields
  +0 <  1>   bool boolMember
  +1 < :1>   unsigned int bitfield1 : 1
  +1 < :1>   bool bitfield2 : 1
  +1 < :2>   unsigned int bitfield3 : 2
  +1 < :1>   unsigned int bitfield4 : 1
  +1 < :2>   unsigned long bitfield5 : 2
  +1 < :1>   <UNUSED BITS: 1 bit>
  +2 <  2>   <PADDING: 2 bytes>
  +4 <  4>   int intMember
  +8 < :1>   unsigned int bitfield7 : 1
  +8 < :9>   unsigned int bitfield8 : 9
  +9 < :1>   bool bitfield9 : 1
  +9 < :5>   <UNUSED BITS: 5 bits>
 +10 <  6>   <PADDING: 6 bytes>
Total byte size: 16
Total pad bytes: 8
Padding percentage: 50.00 %"""
        actual_layout = debugger_instance.layout_for_classname('ClassWithPaddedBitfields')
        self.assertEqual(EXPECTED_RESULT, actual_layout.as_string())

    def serial_test_MemberHasBitfieldPadding(self):
        EXPECTED_RESULT = """  +0 < 24> MemberHasBitfieldPadding
  +0 < 16>     ClassWithPaddedBitfields bitfieldMember
  +0 <  1>       bool boolMember
  +1 < :1>       unsigned int bitfield1 : 1
  +1 < :1>       bool bitfield2 : 1
  +1 < :2>       unsigned int bitfield3 : 2
  +1 < :1>       unsigned int bitfield4 : 1
  +1 < :2>       unsigned long bitfield5 : 2
  +1 < :1>       <UNUSED BITS: 1 bit>
  +2 <  2>       <PADDING: 2 bytes>
  +4 <  4>       int intMember
  +8 < :1>       unsigned int bitfield7 : 1
  +8 < :9>       unsigned int bitfield8 : 9
  +9 < :1>       bool bitfield9 : 1
  +9 < :5>       <UNUSED BITS: 5 bits>
 +10 <  6>   <PADDING: 6 bytes>
 +16 < :1>   bool bitfield1 : 1
 +16 < :7>   <UNUSED BITS: 7 bits>
 +17 <  7>   <PADDING: 7 bytes>
Total byte size: 24
Total pad bytes: 15
Padding percentage: 62.50 %"""
        actual_layout = debugger_instance.layout_for_classname('MemberHasBitfieldPadding')
        self.assertEqual(EXPECTED_RESULT, actual_layout.as_string())

    def serial_test_InheritsFromClassWithPaddedBitfields(self):
        EXPECTED_RESULT = """  +0 < 16> InheritsFromClassWithPaddedBitfields
  +0 < 16>     ClassWithPaddedBitfields ClassWithPaddedBitfields
  +0 <  1>       bool boolMember
  +1 < :1>       unsigned int bitfield1 : 1
  +1 < :1>       bool bitfield2 : 1
  +1 < :2>       unsigned int bitfield3 : 2
  +1 < :1>       unsigned int bitfield4 : 1
  +1 < :2>       unsigned long bitfield5 : 2
  +1 < :1>       <UNUSED BITS: 1 bit>
  +2 <  2>       <PADDING: 2 bytes>
  +4 <  4>       int intMember
  +8 < :1>       unsigned int bitfield7 : 1
  +8 < :9>       unsigned int bitfield8 : 9
  +9 < :1>       bool bitfield9 : 1
  +9 < :5>       <UNUSED BITS: 5 bits>
 +10 < :1>   bool derivedBitfield : 1
 +10 < :7>   <UNUSED BITS: 7 bits>
 +11 <  5>   <PADDING: 5 bytes>
Total byte size: 16
Total pad bytes: 7
Padding percentage: 43.75 %"""
        actual_layout = debugger_instance.layout_for_classname('InheritsFromClassWithPaddedBitfields')
        self.assertEqual(EXPECTED_RESULT, actual_layout.as_string())
