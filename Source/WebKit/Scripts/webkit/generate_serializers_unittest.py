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

# Tests the Swift forms of types marked [Swift] that generate-serializers.py generates. The other
# expected outputs in tests/ are checked by regenerating them (see tests/Makefile).
#
# Examples of how to run:
# python3 Source/WebKit/Scripts/webkit/generate_serializers_unittest.py
# cd Source/WebKit/Scripts && python3 -m unittest discover -p '*_unittest.py'

import importlib.util
import os
import re
import sys
import unittest

module_directory = os.path.dirname(os.path.abspath(__file__))
scripts_directory = os.path.abspath(os.path.join(module_directory, os.path.pardir))
sys.path.append(scripts_directory)

_specification = importlib.util.spec_from_file_location('generate_serializers', os.path.join(scripts_directory, 'generate-serializers.py'))
generate_serializers = importlib.util.module_from_spec(_specification)
_specification.loader.exec_module(generate_serializers)

tests_directory = os.path.join(module_directory, 'tests')

_identifiers = '''
template: struct WebCore::FrameIdentifierType
class WTF::ObjectIdentifier {
    uint64_t toUInt64()
}
'''


def generate_swift(source):
    serialized_types, serialized_enums, _, using_statements, _, _ = generate_serializers.parse_serialized_types(source.splitlines())
    serialized_types = generate_serializers.resolve_inheritance(serialized_types)
    return generate_serializers.generate_swift(serialized_types, serialized_enums, using_statements)


def enum_value_assertions(source):
    _, serialized_enums, _, _, _, _ = generate_serializers.parse_serialized_types(source.splitlines())
    return generate_serializers.swift_enum_value_assertions(serialized_enums[0])


class GeneratedSwiftTest(unittest.TestCase):
    def test_generated_file(self):
        with open(os.path.join(tests_directory, 'TestSerializedType.serialization.in')) as file:
            source = file.read()
        with open(os.path.join(tests_directory, 'GeneratedSerializers.swift')) as file:
            expected = file.read()
        self.assertEqual(generate_swift(source), expected)

    def test_lower_camel_case(self):
        for name, expected in [('FirstValue', 'firstValue'), ('HTTPSOnly', 'httpsOnly'), ('URL', 'url'), ('A', 'a'),
                               ('RGBA8', 'rgba8'), ('alreadyLower', 'alreadyLower'), ('IPCTesterInner', 'ipcTesterInner')]:
            self.assertEqual(generate_serializers.swift_lower_camel_case(name), expected)

    def test_variant_case_names(self):
        for cpp_type, expected in [('uint64_t', 'uint64'), ('String', 'string'), ('Vector<String>', 'vectorString'),
                                   ('Vector<uint8_t>', 'vectorUInt8'), ('WebKit::IPCTesterInner', 'ipcTesterInner'),
                                   ('Ref<WebKit::Node>', 'node'), ('RefPtr<WebKit::Node>', 'refPtrNode'),
                                   ('std::optional<WebCore::FrameIdentifier>', 'optionalFrameIdentifier')]:
            self.assertEqual(generate_serializers.swift_variant_case_name(cpp_type), expected)

    def test_no_swift_types(self):
        swift = generate_swift('struct WebKit::NotSwift {\n    uint32_t value;\n}\n')
        self.assertIn('#if ENABLE_BACK_FORWARD_LIST_SWIFT || (ENABLE_IPC_TESTING_API && ENABLE_IPC_TESTING_SWIFT)\n\nimport WebKit_Internal\n\n#endif\n', swift)
        self.assertNotIn('extension', swift)

    def test_same_type_under_exclusive_conditions(self):
        swift = generate_swift('''
#if ENABLE(A)
[Swift] struct WebKit::S {
    uint32_t value;
}
#else
[Swift] struct WebKit::S {
    uint64_t value;
}
#endif
''')
        self.assertIn('#if ENABLE_A\n', swift)
        self.assertIn('#if !(ENABLE_A)\n', swift)
        self.assertIn('        #if ENABLE_A\n        typealias WebKit_S = IPCTypes.S\n        #endif\n'
                      '        #if !(ENABLE_A)\n        typealias WebKit_S = IPCTypes.S\n        #endif\n', swift)

    def test_cxx_names(self):
        swift = generate_swift(_identifiers + '''
[Swift] struct WebKit::S {
    WebCore::FrameIdentifier frameID;
}
#if ENABLE(A)
[Swift, Nested] enum class WebKit::S::Kind : uint8_t {
    Value
}
#endif
''')
        self.assertIn('''    enum CxxName {
        typealias WebCore_FrameIdentifier = IPCTypes.FrameIdentifier
        typealias WebKit_S = IPCTypes.S
        #if ENABLE_A
        typealias WebKit_S_Kind = IPCTypes.Kind
        #endif
    }
''', swift)
        self.assertIn('        var frameID: IPCTypes.FrameIdentifier\n', swift)
        self.assertIn('        IPCTypes.FrameIdentifier.encode(value.frameID, to: encoder)\n', swift)

    def test_conditional_lines_are_indented(self):
        swift = generate_swift('''
[Swift, RefCounted] class WebKit::C {
    uint32_t a;
#if ENABLE(B)
    uint32_t b;
#endif
}
''')
        self.assertIn('        let a: UInt32\n        #if ENABLE_B\n        let b: UInt32\n        #endif\n', swift)
        self.assertIn('            self.a = try UInt32.decode(from: &decoder)\n            #if ENABLE_B\n', swift)
        self.assertNotRegex(swift, r'\n#(?!if ENABLE_BACK_FORWARD_LIST_SWIFT |endif\n$)')

    def test_enum_value_assertions(self):
        self.assertEqual(enum_value_assertions('''
[Swift] enum class WebKit::E : uint8_t {
    A,
#if ENABLE(B)
    B,
#endif
}
'''), [
            'static_assert(sizeof(std::underlying_type_t<WebKit::E>) == sizeof(uint8_t), "IPCTypes.E in GeneratedSerializers.swift has the size of the underlying type in .serialization.in");',
            'static_assert(!static_cast<uint8_t>(WebKit::E::A), "IPCTypes.E in GeneratedSerializers.swift numbers values in .serialization.in order");',
            '#if ENABLE(B)',
            'static_assert(static_cast<uint8_t>(WebKit::E::B) == 1, "IPCTypes.E in GeneratedSerializers.swift numbers values in .serialization.in order");',
            '#endif',
        ])
        self.assertEqual(enum_value_assertions('''
[Swift, OptionSet] enum class WebKit::E : uint32_t {
    A,
    B,
}
'''), [
            'static_assert(sizeof(std::underlying_type_t<WebKit::E>) == sizeof(uint32_t), "IPCTypes.E in GeneratedSerializers.swift has the size of the underlying type in .serialization.in");',
            'static_assert(static_cast<uint32_t>(WebKit::E::A) == (1ull << 0), "IPCTypes.E in GeneratedSerializers.swift numbers values in .serialization.in order");',
            'static_assert(static_cast<uint32_t>(WebKit::E::B) == (1ull << 1), "IPCTypes.E in GeneratedSerializers.swift numbers values in .serialization.in order");',
        ])
        self.assertEqual(enum_value_assertions('''
enum class WebKit::E : uint8_t {
    A,
}
'''), [])

    def test_ref_counted_class_without_members(self):
        swift = generate_swift('[Swift, RefCounted] class WebKit::Empty {\n}\n')
        self.assertIn('    final class Empty: Sendable, Equatable {\n\n        init() {}\n', swift)


class SwiftErrorTest(unittest.TestCase):
    def assertGenerationFails(self, source, message):
        with self.assertRaisesRegex(Exception, message):
            generate_swift(source)

    def test_name_collisions(self):
        self.assertGenerationFails('''
[Swift] struct WebKit::Thing {
    uint32_t value;
}
[Swift] enum class WebCore::Thing : uint8_t {
    Value
}
''', r'WebCore::Thing and WebKit::Thing would both be IPCTypes\.Thing')
        self.assertGenerationFails(_identifiers + '''
[Swift] struct WebKit::FrameIdentifier {
    uint32_t value;
}
''', r'WebCore::FrameIdentifier and WebKit::FrameIdentifier would both be IPCTypes\.FrameIdentifier')
        self.assertGenerationFails(_identifiers + '''
[Swift] using WebKit::FrameIdentifierType = Variant<uint32_t, uint64_t>
''', r'would both be IPCTypes\.FrameIdentifierType')
        self.assertGenerationFails('''
[Swift] enum class WebKit::Optional : bool {
    No,
    Yes
}
''', r'WebKit::Optional cannot have a Swift form, because IPCTypes\.Optional would hide the Swift type Optional')
        self.assertGenerationFails('''
[Swift, Nested] enum class WebCore::PlatformEvent::Type : uint8_t {
    Value
}
''', r'WebCore::PlatformEvent::Type cannot have a Swift form, because Swift does not allow a type named Type')
        self.assertGenerationFails('''
[Swift] struct WebKit::CxxName {
    uint32_t value;
}
''', r'WebKit::CxxName cannot have a Swift form, because IPCTypes\.CxxName is the generated index of Swift forms by C\+\+ name')
        self.assertGenerationFails('''
[Swift] struct WebKit::Inner_Thing {
    uint32_t value;
}
[Swift] struct WebKit_Inner::Thing {
    uint32_t value;
}
''', r'WebKit::Inner_Thing and WebKit_Inner::Thing would both be IPCTypes\.CxxName\.WebKit_Inner_Thing in Swift')

    def test_identifiers_described_as_other_types(self):
        source = '''
template: struct WebCore::FrameIdentifierType
template: struct WebCore::NavigationIdentifierType
template: struct WebCore::PageIdentifierType
class WTF::ObjectIdentifier {
    uint64_t toUInt64()
}
[Alias=class ProcessQualified<NavigationIdentifierID>, CustomHeader] alias WebCore::NavigationIdentifier {
    WebCore::NavigationIdentifierID object();
    WebCore::ProcessIdentifier processIdentifier();
};
using WebCore::PageIdentifier = Variant<uint32_t, uint64_t>;
'''
        swift = generate_swift(source)
        self.assertIn('    typealias FrameIdentifier = IPCObjectIdentifier<FrameIdentifierType>\n', swift)
        self.assertNotIn('NavigationIdentifier', swift)
        self.assertNotIn('PageIdentifier', swift)
        self.assertGenerationFails(source + '''
[Swift] struct WebKit::S {
    WebCore::NavigationIdentifier navigationID;
}
''', r'member navigationID: WebCore::NavigationIdentifier has no Swift form')
        self.assertGenerationFails(source + '''
[Swift] struct WebKit::S {
    std::optional<WebCore::PageIdentifier> pageID;
}
''', r'member pageID: WebCore::PageIdentifier has no Swift form')

    def test_disallowed_member_names(self):
        for name, swift_name in [('m_Type', 'Type'), ('Protocol()', 'Protocol'), ('m_self', 'self')]:
            self.assertGenerationFails(f'''
[Swift] struct WebKit::S {{
    uint32_t {name};
}}
''', rf'member {re.escape(name)}: it would be named {swift_name} in Swift, which cannot be the name of a property')

    def test_unmappable_member_type(self):
        self.assertGenerationFails('''
[Swift] struct WebKit::S {
    int value;
}
''', r"\[Swift\] WebKit::S: member value: 'int' has no Swift form")
        self.assertGenerationFails('''
[Swift] struct WebKit::S {
    HashMap<String, String> map;
}
''', r'member map: .*HashMap<> is not supported')

    def test_member_type_without_swift_form(self):
        self.assertGenerationFails('''
struct WebKit::NotSwift {
    uint32_t value;
}
[Swift] struct WebKit::S {
    Vector<WebKit::NotSwift> values;
}
''', r'member values: WebKit::NotSwift has no Swift form')
        self.assertGenerationFails(_identifiers + '''
[Swift] struct WebKit::S {
    WebCore::FrameIdentifierType tag;
}
''', r'member tag: WebCore::FrameIdentifierType has no Swift form')

    def test_member_type_in_another_namespace(self):
        self.assertGenerationFails('''
[Swift] struct WebKit::Inner {
    uint32_t value;
}
[Swift] struct WebKit::S {
    WebCore::Inner inner;
}
''', r'member inner: WebCore::Inner would be IPCTypes\.Inner in Swift, which is the Swift form of WebKit::Inner')

    def test_ref_counted_members(self):
        self.assertGenerationFails('''
[Swift] struct WebKit::Inner {
    uint32_t value;
}
[Swift] struct WebKit::S {
    Ref<WebKit::Inner> inner;
}
''', r'WebKit::Inner must be held in a Ref or RefPtr if, and only if, it is marked \[RefCounted\]')
        self.assertGenerationFails('''
[Swift, RefCounted] class WebKit::Node {
    uint32_t value;
}
[Swift] struct WebKit::S {
    Vector<WebKit::Node> nodes;
}
''', r'WebKit::Node must be held in a Ref or RefPtr if, and only if, it is marked \[RefCounted\]')

    def test_option_set_members(self):
        self.assertGenerationFails('''
[Swift] enum class WebKit::E : uint8_t {
    Value
}
[Swift] struct WebKit::S {
    OptionSet<WebKit::E> values;
}
''', r'WebKit::E must be held in an OptionSet if, and only if, it is marked \[OptionSet\]')
        self.assertGenerationFails('''
[Swift, OptionSet] enum class WebKit::E : uint8_t {
    Value
}
[Swift] struct WebKit::S {
    WebKit::E value;
}
''', r'WebKit::E must be held in an OptionSet if, and only if, it is marked \[OptionSet\]')

    def test_unsupported_member_attributes(self):
        for attribute in ["Validator='value > 1'", "Precondition='isAvailable()'", 'EncodeRequestBody', 'OptionalTupleBits']:
            self.assertGenerationFails(f'''
[Swift] struct WebKit::S {{
    [{attribute}] uint32_t value;
}}
''', r'member value: types marked \[Swift\] do not support the member attribute ' + attribute.split('=')[0])
        self.assertGenerationFails('''
[Swift] struct WebKit::S {
    [OptionalTupleBit=WebKit::Change::Value] uint32_t value;
}
''', 'do not support the member attribute OptionalTupleBit')

    def test_unsupported_member_names(self):
        for name in ['functionCall().member', 'member.value', 'functionCall(argument)']:
            self.assertGenerationFails(f'''
[Swift] struct WebKit::S {{
    uint32_t {name};
}}
''', r'can only have members that are variables or member functions without arguments')

    def test_duplicate_member_names(self):
        self.assertGenerationFails('''
[Swift] struct WebKit::S {
    uint32_t m_value;
    uint32_t value();
}
''', r'\[Swift\] WebKit::S: more than one of its members would be named value in Swift')

    def test_unsupported_types(self):
        self.assertGenerationFails('''
[Swift, CustomEncoder] struct WebKit::S {
    uint32_t value;
}
''', r'types marked \[Swift\] do not support the attribute CustomEncoder')
        self.assertGenerationFails('''
[Swift, RefCounted] class WebCore::TimingFunction subclasses {
    WebCore::LinearTimingFunction,
}
''', r'types serialized as their subclasses cannot be marked \[Swift\]')
        self.assertGenerationFails('''
[Swift, Alias=class Pair<float,double>] alias WebCore::FloatPair {
    float first()
}
''', r'types marked \[Swift\] do not support the attribute Alias')
        self.assertGenerationFails('''
[Swift] CFFooRef wrapped by WebKit::FooWrapper {
}
''', r'wrapped CF types cannot be marked \[Swift\]')
        self.assertGenerationFails('''
[Swift] webkit_secure_coding NSSomeFoundationType {
    StringKey: String
}
''', r'webkit_secure_coding types cannot be marked \[Swift\]')
        self.assertGenerationFails('''
[Swift] class WebCore::RectEdges<bool> {
    bool top();
}
''', r'templates cannot be marked \[Swift\]')
        self.assertGenerationFails('''
template: class WebKit::Fabulous
[Swift] class WebKit::TemplateTest {
    bool value;
}
''', r'templates cannot be marked \[Swift\]')
        with self.assertRaisesRegex(Exception, r'\[Swift\] is not supported on wrapped types'):
            generate_swift('[Swift] NSNull wrapped by CoreIPCNull\n')
        self.assertGenerationFails('''
[Swift, CreateUsing=fromValue] struct WebKit::S {
    uint32_t value;
}
''', r'types marked \[Swift\] do not support the attribute CreateUsing, because Swift would not make the checks that the C\+\+ function may make')

    def test_parent_class(self):
        self.assertGenerationFails('''
[Swift] class WebKit::Derived : WebKit::Unknown {
    uint32_t value;
}
''', r'its parent class WebKit::Unknown is not described in a .serialization.in file')
        self.assertGenerationFails('''
class WebKit::Base {
    [Validator='value > 1'] uint32_t value;
}
[Swift] class WebKit::Derived : WebKit::Base {
    uint32_t otherValue;
}
''', r'\[Swift\] WebKit::Derived: member value: types marked \[Swift\] do not support the member attribute Validator')

    def test_conditions(self):
        self.assertGenerationFails('''
#if OS(WINDOWS)
[Swift] struct WebKit::S {
    uint32_t value;
}
#endif
''', r"\[Swift\] WebKit::S: 'OS\(WINDOWS\)' cannot be evaluated in Swift")
        self.assertGenerationFails('''
[Swift] struct WebKit::S {
#if defined(SOMETHING)
    uint32_t value;
#endif
}
''', r'member value: .*cannot be evaluated in Swift')
        for condition, name in [('CGFLOAT_IS_DOUBLE', 'CGFLOAT_IS_DOUBLE'), ('!RELEASE_LOG_DISABLED', 'RELEASE_LOG_DISABLED'), ('ENABLE(FOO) && LOG_DISABLED', 'LOG_DISABLED')]:
            self.assertGenerationFails(f'''
[Swift] struct WebKit::S {{
#if {condition}
    uint32_t value;
#endif
}}
''', rf'member value: .*cannot be evaluated in Swift: {name} is not available there')
        self.assertGenerationFails('''
[Swift] enum class WebKit::E : uint8_t {
    A,
#if !RELEASE_LOG_DISABLED
    B,
#endif
}
''', r'value B: .*RELEASE_LOG_DISABLED is not available there')

    def test_enums(self):
        self.assertGenerationFails('''
[Swift] enum class WebKit::E : bool
''', r'an enum marked \[Swift\] must list its values')
        self.assertGenerationFails('''
[Swift] enum class WebKit::E : bool {
    No,
    Yes,
    Maybe
}
''', r'a bool enum marked \[Swift\] must have exactly two values')
        self.assertGenerationFails('''
[Swift, OptionSet] enum class WebKit::E : int8_t {
    A
}
''', r'the underlying type of an OptionSet marked \[Swift\] must be unsigned')
        self.assertGenerationFails('''
[Swift] enum class WebKit::E : uint8_t {
#if ENABLE(A)
    A,
#endif
}
''', r'must have at least one value that is not conditional')
        self.assertGenerationFails('''
[Swift] enum class WebKit::E : uint8_t {
    URL,
    Url
}
''', r'more than one of its values would be named url in Swift')
        self.assertGenerationFails('''
[Swift] enum class WebKit::E : uint8_t {
    RawValue
}
''', r'a value would be named rawValue in Swift')
        self.assertGenerationFails('''
[Swift, OptionSet] enum class WebKit::E : uint8_t {
''' + ''.join(f'    Value{index},\n' for index in range(9)) + '''}
''', r'it has more values than uint8_t has bits')
        self.assertGenerationFails('''
[Swift] enum class WebKit::E : int {
    A
}
''', r"'int' has no Swift form")
        self.assertGenerationFails('''
[Swift, Unknown] enum class WebKit::E : uint8_t {
    A
}
''', r'enums marked \[Swift\] do not support the attribute Unknown')

    def test_variants(self):
        self.assertGenerationFails('''
[Swift] using WebKit::V = Variant<
    uint32_t,
#if ENABLE(A)
    uint64_t,
#endif
    double
>;
''', r'a Variant marked \[Swift\] cannot have conditional alternatives')
        self.assertGenerationFails('''
[Swift] using WebKit::V = uint32_t;
''', r'only using statements of a Variant can be marked \[Swift\]')
        self.assertGenerationFails('''
[Swift] using WebKit::V = Variant<uint8_t, uint8_t>
''', r'more than one of its alternatives would be named uint8 in Swift')
        self.assertGenerationFails('''
[Swift] using WebKit::V = Variant<uint8_t, int>
''', r"alternative int: 'int' has no Swift form")


if __name__ == '__main__':
    unittest.main()
