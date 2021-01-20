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


# Context Manager for saving the value of an object's attribute, setting it to
# a new value (None, by default), and then restoring the original value.
#
# Used as:
#
#   myObject.fooAttribute = 1
#   print(myObject.fooAttribute)
#   with AttributeSaver(myObject, "fooAttribute", 5):
#       print(myObject.fooAttribute)
#   print(myObject.fooAttribute)
#
# Prints: 1, 5, 1

class AttributeSaver:
    def __init__(self, obj, attribute, value=None):
        self.obj = obj
        self.attribute = attribute
        self.old_value = getattr(self.obj, self.attribute)
        self.new_value = value

    def __enter__(self):
        setattr(self.obj, self.attribute, self.new_value)

    def __exit__(self, exc_type, exc_value, traceback):
        setattr(self.obj, self.attribute, self.old_value)
        return None
