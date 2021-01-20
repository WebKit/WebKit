#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright (C) 2019 Apple Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import unittest

from webkitpy.common.attribute_saver import AttributeSaver


class AttributeSaverTest(unittest.TestCase):

    class SimpleTestClass:
        def __init__(self):
            self.value = 0

    def test_class(self):
        obj = self.SimpleTestClass()
        self.assertEquals(obj.value, 0)

    def test_normal_default(self):
        obj = self.SimpleTestClass()
        self.assertEquals(obj.value, 0)
        with AttributeSaver(obj, "value"):
            self.assertEquals(obj.value, None)
        self.assertEquals(obj.value, 0)

    def test_normal_value(self):
        obj = self.SimpleTestClass()
        self.assertEquals(obj.value, 0)
        with AttributeSaver(obj, "value", 1):
            self.assertEquals(obj.value, 1)
        self.assertEquals(obj.value, 0)

    def test_normal_value_on_exception(self):
        with self.assertRaises(RuntimeError):
            obj = self.SimpleTestClass()
            self.assertEquals(obj.value, 0)
            try:
                with AttributeSaver(obj, "value", 1):
                    self.assertEquals(obj.value, 1)
                    raise RuntimeError()
            except:
                self.assertEquals(obj.value, 0)
                raise
            self.assertEquals(obj.value, 0)

    def test_normal_value_on_normal_exit(self):
        obj = self.SimpleTestClass()
        self.assertEquals(obj.value, 0)
        try:
            with AttributeSaver(obj, "value", 1):
                self.assertEquals(obj.value, 1)
        except:
            self.assertEquals(obj.value, 0)
            raise
        self.assertEquals(obj.value, 0)

    def test_normal_value_with_finally_on_exception(self):
        with self.assertRaises(RuntimeError):
            obj = self.SimpleTestClass()
            self.assertEquals(obj.value, 0)
            try:
                with AttributeSaver(obj, "value", 1):
                    self.assertEquals(obj.value, 1)
                    raise RuntimeError()
            finally:
                self.assertEquals(obj.value, 0)
            self.assertEquals(obj.value, 0)

    def test_normal_value_with_finally_on_normal_exit(self):
        obj = self.SimpleTestClass()
        self.assertEquals(obj.value, 0)
        try:
            with AttributeSaver(obj, "value", 1):
                self.assertEquals(obj.value, 1)
        finally:
            self.assertEquals(obj.value, 0)
        self.assertEquals(obj.value, 0)

    def test_normal_value_with_else_on_exception(self):
        with self.assertRaises(RuntimeError):
            obj = self.SimpleTestClass()
            self.assertEquals(obj.value, 0)
            try:
                with AttributeSaver(obj, "value", 1):
                    self.assertEquals(obj.value, 1)
                    raise RuntimeError()
            except IOError:
                self.assertFalse(True)
            else:
                self.assertEquals(obj.value, 0)
            self.assertEquals(obj.value, 0)

    def test_normal_value_with_else_on_normal_exit(self):
        obj = self.SimpleTestClass()
        self.assertEquals(obj.value, 0)
        try:
            with AttributeSaver(obj, "value", 1):
                self.assertEquals(obj.value, 1)
        except IOError:
            self.assertFalse(True)
        else:
            self.assertEquals(obj.value, 0)
        self.assertEquals(obj.value, 0)
