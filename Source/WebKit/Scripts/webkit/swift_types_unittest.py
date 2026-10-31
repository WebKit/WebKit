# Copyright (C) 2026 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import sys
import unittest

module_directory = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.abspath(os.path.join(module_directory, os.path.pardir)))
from webkit import swift_types  # noqa: E402


class SwiftTypeAndCoderTest(unittest.TestCase):
    def test_types(self):
        for cpp_type, expected in [
            ('uint32_t', ('UInt32', 'UInt32')),
            ('const String&', ('String?', 'IPCCoding.WTFString')),
            ('WebKit::IPCTesterInner', ('IPCTypes.IPCTesterInner', 'IPCTypes.CxxName.WebKit_IPCTesterInner')),
            ('struct WebKit::IPCTesterInner', ('IPCTypes.IPCTesterInner', 'IPCTypes.CxxName.WebKit_IPCTesterInner')),
            ('WebCore::PlatformEvent::Kind', ('IPCTypes.Kind', 'IPCTypes.CxxName.WebCore_PlatformEvent_Kind')),
            ('Vector<WebKit::IPCTesterInner>', ('[IPCTypes.IPCTesterInner]', 'IPCCoding.Vector<IPCTypes.CxxName.WebKit_IPCTesterInner>')),
            ('Vector<Ref<WebKit::Node>>', ('[IPCTypes.Node]', 'IPCCoding.Vector<IPCTypes.CxxName.WebKit_Node>')),
            ('RefPtr<WebKit::Node>', ('IPCTypes.Node?', 'IPCCoding.StdOptional<IPCTypes.CxxName.WebKit_Node>')),
            ('Markable<WebCore::FrameIdentifier>', ('IPCTypes.FrameIdentifier?', 'IPCCoding.Markable<IPCTypes.CxxName.WebCore_FrameIdentifier>')),
            ('OptionSet<WebKit::Flag>', ('IPCTypes.Flag', 'IPCTypes.CxxName.WebKit_Flag')),
            ('std::optional<Vector<String>>', ('[String?]?', 'IPCCoding.StdOptional<IPCCoding.Vector<IPCCoding.WTFString>>')),
        ]:
            self.assertEqual(swift_types.swift_type_and_coder(cpp_type), expected)

    def test_coders_without_cxx_names(self):
        self.assertEqual(swift_types.swift_type_and_coder('Vector<Ref<WebKit::Node>>', cxx_name_coders=False), ('[IPCTypes.Node]', 'IPCCoding.Vector<IPCTypes.Node>'))
        self.assertEqual(swift_types.swift_type_and_coder('WebCore::FrameIdentifier', cxx_name_coders=False), ('IPCTypes.FrameIdentifier', 'IPCTypes.FrameIdentifier'))

    def test_unmappable_types(self):
        for cpp_type in ['int', 'URL', 'WTF::URL', 'std::monostate', 'HashMap<String, String>', 'std::pair<uint8_t, uint8_t>']:
            with self.assertRaises(swift_types.UnmappableSwiftType):
                swift_types.swift_type_and_coder(cpp_type)

    def test_cxx_name(self):
        self.assertEqual(swift_types.swift_cxx_name('WebCore::FrameIdentifier'), 'WebCore_FrameIdentifier')
        self.assertEqual(swift_types.swift_cxx_name('::WebCore::PlatformEvent::Kind'), 'WebCore_PlatformEvent_Kind')

    def test_helpers(self):
        self.assertEqual(swift_types.strip_cpp_type_decorations(' const struct WebKit::S& '), 'WebKit::S')
        self.assertEqual(swift_types.split_template_arguments('Vector<uint8_t>, HashMap<String, uint32_t>, int'), ['Vector<uint8_t>', 'HashMap<String, uint32_t>', 'int'])


class SwiftConditionTest(unittest.TestCase):
    def test_conditions(self):
        for condition, expected in [
            ('ENABLE(FOO)', 'ENABLE_FOO'),
            ('ENABLE(FOO) && (PLATFORM(COCOA) || !HAVE(BAR))', 'ENABLE_FOO && (WTF_PLATFORM_COCOA || !HAVE_BAR)'),
            ('USE(GLIB) || ASSERT_ENABLED', 'USE_GLIB || ASSERT_ENABLED'),
            ('!(ENABLE_FOO)', '!(ENABLE_FOO)'),
        ]:
            self.assertEqual(swift_types.swift_condition(condition), expected)

    def test_unavailable_macros(self):
        for condition, name in [
            ('CGFLOAT_IS_DOUBLE', 'CGFLOAT_IS_DOUBLE'),
            ('!RELEASE_LOG_DISABLED', 'RELEASE_LOG_DISABLED'),
            ('ENABLE(FOO) && LOG_DISABLED', 'LOG_DISABLED'),
            ('PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED', '__MAC_OS_X_VERSION_MIN_REQUIRED'),
            ('OS(WINDOWS)', r'OS\(\)'),
            ('defined(FOO)', r'defined\(\)'),
        ]:
            with self.assertRaisesRegex(swift_types.UnmappableSwiftCondition, r'cannot be evaluated in Swift: %s is not available there' % name):
                swift_types.swift_condition(condition)

    def test_unsupported_expressions(self):
        for condition in ['0', 'ENABLE(FOO) || 1', 'ENABLE(FOO) == 1', 'USE(FOO) && HAVE_BAR >= 2', 'ENABLE(FOO, BAR)', 'ENABLE_FOO(BAR)']:
            with self.assertRaisesRegex(swift_types.UnmappableSwiftCondition, r'cannot be evaluated in Swift'):
                swift_types.swift_condition(condition)


if __name__ == '__main__':
    unittest.main()
