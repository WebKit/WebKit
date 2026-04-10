import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))

import unittest  # noqa: E402

from webkit.opaque_ipc_types import (  # noqa: E402
    _find_opaque_data_type, _is_odt_concern, is_opaque_type,
    ODT_CONCERN, OpaqueIPCTypes, opaque_ipc_types,
)


class TestOpaqueTypes(unittest.TestCase):

    def test_is_odt_concern_known_types(self):
        # Verify specific known ODT concern types by name, rather than
        # tautologically iterating the set that backs the function.
        known_odt = ['uint8_t', 'char', 'int8_t', 'MachSendRight', 'CFDataRef', 'char16_t']
        for odt_type in known_odt:
            self.assertTrue(_is_odt_concern(odt_type), f"Expected {odt_type} to be ODT concern")

        non_odt_types = ['String', 'int', 'float', 'bool', 'WTF::UUID']
        for non_odt_type in non_odt_types:
            self.assertFalse(_is_odt_concern(non_odt_type), f"Expected {non_odt_type} to not be ODT concern")

    def test_find_opaque_data_type_function(self):
        self.assertEqual(_find_opaque_data_type("std::span<uint8_t>"), "uint8_t")
        self.assertEqual(_find_opaque_data_type("std::span<const uint8_t>"), "uint8_t")
        self.assertEqual(_find_opaque_data_type("Vector<char>"), "char")
        self.assertEqual(_find_opaque_data_type("Vector<const char>"), "char")
        self.assertEqual(_find_opaque_data_type("std::array<uint8_t, 24>"), "uint8_t")
        self.assertEqual(_find_opaque_data_type("std::array<const uint8_t, 16>"), "uint8_t")
        self.assertEqual(_find_opaque_data_type("RetainPtr<CFDataRef>"), "CFDataRef")
        self.assertEqual(_find_opaque_data_type("Vector<MachSendRight>"), "MachSendRight")

        self.assertEqual(_find_opaque_data_type("std::optional<Vector<uint8_t>>"), "uint8_t")
        self.assertEqual(_find_opaque_data_type("Vector<std::optional<uint8_t>>"), "uint8_t")
        self.assertEqual(_find_opaque_data_type("Vector<std::pair<Vector<uint8_t>, std::optional<WTF::UUID>>>"), "uint8_t")
        self.assertEqual(_find_opaque_data_type("Variant<Vector<uint8_t>, String>"), "uint8_t")

        self.assertIsNone(_find_opaque_data_type("Expected<uint8_t, String>"))
        self.assertIsNone(_find_opaque_data_type("Variant<uint8_t, String>"))
        self.assertIsNone(_find_opaque_data_type("std::pair<uint8_t, String>"))
        self.assertIsNone(_find_opaque_data_type("std::optional<uint8_t>"))
        self.assertIsNone(_find_opaque_data_type("uint8_t"))
        self.assertIsNone(_find_opaque_data_type("String"))

    def test_direct_opaque_types(self):
        self.assertTrue(is_opaque_type("MachSendRight"))
        self.assertTrue(is_opaque_type("MachSendRightAnnotated"))
        self.assertFalse(is_opaque_type("String"))
        self.assertFalse(is_opaque_type("int"))

    def test_container_types_with_odt_concerns(self):
        self.assertTrue(is_opaque_type("std::optional<Vector<uint8_t>>"))
        self.assertTrue(is_opaque_type("Vector<std::optional<uint8_t>>"))
        self.assertTrue(is_opaque_type("std::span<uint8_t>"))
        self.assertTrue(is_opaque_type("std::span<const uint8_t>"))
        self.assertTrue(is_opaque_type("std::span<char>"))
        self.assertTrue(is_opaque_type("std::span<const char>"))
        self.assertTrue(is_opaque_type("std::array<uint8_t, 24>"))
        self.assertTrue(is_opaque_type("std::array<const uint8_t, 16>"))
        self.assertTrue(is_opaque_type("Vector<uint8_t>"))
        self.assertTrue(is_opaque_type("Vector<const uint8_t>"))
        self.assertTrue(is_opaque_type("Vector<char>"))
        self.assertTrue(is_opaque_type("RetainPtr<CFDataRef>"))
        self.assertTrue(is_opaque_type("RetainPtr<NSData>"))
        self.assertTrue(is_opaque_type("std::optional<Vector<uint8_t>>"))
        self.assertTrue(is_opaque_type("Expected<Vector<uint8_t>, String>"))
        self.assertTrue(is_opaque_type("Variant<Vector<uint8_t>, String>"))
        self.assertTrue(is_opaque_type("std::pair<Vector<uint8_t>, String>"))
        self.assertTrue(is_opaque_type("std::pair<String, Vector<uint8_t>>"))
        self.assertTrue(is_opaque_type("Vector<std::pair<Vector<uint8_t>, std::optional<WTF::UUID>>>"))
        self.assertTrue(is_opaque_type("std::optional<Vector<std::pair<Vector<uint8_t>, String>>>"))
        self.assertTrue(is_opaque_type("HashMap<String, FixedVector<uint8_t>>"))
        self.assertTrue(is_opaque_type("HashSet<Vector<uint8_t>>"))
        self.assertTrue(is_opaque_type("std::unique_ptr<Vector<uint8_t>>"))
        self.assertTrue(is_opaque_type("KeyValuePair<Vector<uint8_t>, String>"))
        self.assertTrue(is_opaque_type("Vector<HashMap<String, std::pair<Vector<uint8_t>, int>>>"))
        self.assertTrue(is_opaque_type("Variant<Vector<uint8_t>, WebKit::HTTPBody::Element::FileData, String>"))
        self.assertTrue(is_opaque_type("Expected<std::pair<Vector<uint8_t>, String>, String>"))
        self.assertTrue(is_opaque_type("Variant<Vector<uint8_t>, Ref<WebCore::SharedBuffer>, URL>"))

    def test_container_types_without_odt_concerns(self):
        self.assertFalse(is_opaque_type("std::span<String>"))
        self.assertFalse(is_opaque_type("std::array<int, 5>"))
        self.assertFalse(is_opaque_type("Vector<String>"))
        self.assertFalse(is_opaque_type("std::optional<String>"))
        self.assertFalse(is_opaque_type("std::optional<uint8_t>"))
        self.assertFalse(is_opaque_type("Expected<uint8_t, String>"))
        self.assertFalse(is_opaque_type("Expected<String, uint8_t>"))
        self.assertFalse(is_opaque_type("Expected<String, int>"))
        self.assertFalse(is_opaque_type("Variant<uint8_t, int>"))
        self.assertFalse(is_opaque_type("Variant<String, int>"))
        self.assertFalse(is_opaque_type("std::pair<uint8_t, String>"))
        self.assertFalse(is_opaque_type("std::optional<std::pair<uint8_t, String>>"))
        self.assertFalse(is_opaque_type("Vector<std::pair<uint8_t, String>>"))

    def test_infinite_recursion_protection(self):
        visited = {"TestType"}
        result = _find_opaque_data_type("TestType", visited=visited)
        self.assertIsNone(result)

        visited_opaque = {"Vector<uint8_t>"}
        result = _find_opaque_data_type("Vector<uint8_t>", visited=visited_opaque)
        self.assertIsNone(result)

        result = _find_opaque_data_type("Vector<uint8_t>")
        self.assertEqual(result, "uint8_t")

    def test_bad_formatting(self):
        self.assertFalse(is_opaque_type(""))
        self.assertFalse(is_opaque_type("Vector<>"))
        self.assertFalse(is_opaque_type("std::optional<>"))
        self.assertFalse(is_opaque_type("Vector"))
        self.assertFalse(is_opaque_type("std::optional"))
        self.assertFalse(is_opaque_type("<uint8_t>"))

    def test_context_propagation_through_simple_wrappers(self):
        self.assertTrue(is_opaque_type("Vector<std::optional<uint8_t>>"))
        self.assertFalse(is_opaque_type("std::optional<uint8_t>"))

    def test_context_reset_in_structural_containers(self):
        self.assertFalse(is_opaque_type("std::pair<uint8_t, String>"))
        self.assertTrue(is_opaque_type("std::pair<Vector<uint8_t>, String>"))

    def test_retainptr_with_direct_opaque_types(self):
        self.assertTrue(is_opaque_type("RetainPtr<CFDataRef>"))
        self.assertTrue(is_opaque_type("RetainPtr<NSData>"))
        self.assertTrue(is_opaque_type("RetainPtr<MachSendRight>"))

    def test_deeply_nested_types(self):
        deep_type = "Vector<HashMap<String, std::pair<std::optional<Vector<uint8_t>>, int>>>"
        self.assertTrue(is_opaque_type(deep_type))

    def test_opaque_ipc_types_parsing(self):
        test_file = os.path.join(os.path.dirname(__file__), 'test_opaque_ipc_types.tracking.in')
        if not os.path.exists(test_file):
            self.fail(f"Test tracking file not found: {test_file}")

        ot = OpaqueIPCTypes(test_file)

        total_entries = sum(len(entries) for entries in ot.message_params.values())
        total_entries += sum(len(entries) for entries in ot.message_param_replies.values())
        total_entries += sum(len(entries) for entries in ot.alias_params.values())
        total_entries += sum(len(entries) for entries in ot.structure_params.values())

        self.assertGreater(total_entries, 0, "Test file should have entries")

        self.assertTrue(ot.message_param_tracked('TestWithLegacyReceiver', 'DidCreateWebProcessConnection', 'connectionIdentifier', 'MachSendRight'))
        self.assertTrue(ot.message_param_tracked('TestWithStream', 'SendMachSendRight', 'a1', 'MachSendRight'))
        self.assertTrue(ot.message_param_reply_tracked('TestWithStream', 'ReceiveMachSendRight', 'r1', 'MachSendRight'))
        self.assertTrue(ot.alias_param_tracked('TestNamespace::TestSalt', 'std::array<uint8_t, 8>'))
        self.assertTrue(ot.structure_param_tracked('WebKit::TestStruct', 'buffer', 'Vector<uint8_t>'))

        self.assertFalse(ot.message_param_tracked('NonExistentReceiver', 'NonExistentMessage', 'NonExistentParameterName', 'NonExistentType'))
        self.assertFalse(ot.message_param_reply_tracked('NonExistentReceiver', 'NonExistentMessage', 'NonExistentParameterName', 'NonExistentType'))
        self.assertFalse(ot.alias_param_tracked('NonExistentAlias', 'NonExistentType'))
        self.assertFalse(ot.structure_param_tracked('NonExistentStruct', 'NonExistentMember', 'NonExistentType'))

        self.assertTrue(ot.structure_webcontent_dispatchable('WebKit::TestStruct', 'buffer', 'Vector<uint8_t>'))
        self.assertFalse(ot.structure_webcontent_dispatchable('WebKit::SecureData', 'encrypted', 'Vector<uint8_t>'))

        self.assertTrue(ot.webcontent_dispatchable('TestWithLegacyReceiver', 'DidCreateWebProcessConnection', 'connectionIdentifier', 'MachSendRight'))
        self.assertFalse(ot.webcontent_dispatchable('TestWithLegacyReceiver', 'OpaqueTypeSecurityAssertion', 'param', 'NotDispatchableFromWebContentType'))

        self.assertTrue(ot.reply_webcontent_dispatchable('UserInterface', 'GetUserData', 'data', 'std::span<const uint8_t>'))
        self.assertFalse(ot.reply_webcontent_dispatchable('TestInterface', 'GetData', 'result', 'std::span<const uint8_t>'))

    def test_production_tracking_file_parses(self):
        self.assertTrue(is_opaque_type("IPC::TransferString::IPCData"))
        self.assertTrue(is_opaque_type("WebKit::CFObjectValue"))
        self.assertTrue(is_opaque_type("UniqueRef<WebKit::CFObjectValue>"))
        self.assertTrue(is_opaque_type("std::optional<IPC::TransferString::IPCData>"))
        self.assertTrue(is_opaque_type("Vector<WebKit::CFObjectValue>"))

        total_entries = sum(len(e) for e in opaque_ipc_types.message_params.values())
        total_entries += sum(len(e) for e in opaque_ipc_types.message_param_replies.values())
        total_entries += sum(len(e) for e in opaque_ipc_types.alias_params.values())
        total_entries += sum(len(e) for e in opaque_ipc_types.structure_params.values())

        self.assertGreater(total_entries, 10, "opaque_ipc_types.tracking.in seems to have too few entries")

    def test_type_map_built(self):
        self.assertIsNotNone(opaque_ipc_types.type_map, "Type map should be built at init time")
        self.assertIn('WebCore::SharedBuffer', opaque_ipc_types.type_map['members'])
        self.assertIn('WebCore::FragmentedSharedBuffer', opaque_ipc_types.type_map['members'])
        self.assertIn('JSC::ArrayBuffer', opaque_ipc_types.type_map['members'])
        self.assertIn('API::Object', opaque_ipc_types.type_map['subclasses'])

    def test_transitive_opaque_via_type_map(self):
        # Types that are opaque through their serialized members
        self.assertTrue(is_opaque_type("WebCore::SharedBuffer"))
        self.assertTrue(is_opaque_type("Ref<WebCore::SharedBuffer>"))
        self.assertTrue(is_opaque_type("RefPtr<WebCore::SharedBuffer>"))
        self.assertTrue(is_opaque_type("WebCore::FragmentedSharedBuffer"))
        self.assertTrue(is_opaque_type("JSC::ArrayBuffer"))
        self.assertTrue(is_opaque_type("WTF::CString"))
        self.assertTrue(is_opaque_type("CString"))
        self.assertTrue(is_opaque_type("IPC::Semaphore"))

    def test_safe_wrapper_terminates_recursion(self):
        # SafeWrapper types should NOT be detected as opaque
        self.assertFalse(is_opaque_type("IPC::ConnectionHandle"))

    def test_subclass_variant_resolution(self):
        # API::Object has subclasses including API::Data (opaque)
        self.assertTrue(is_opaque_type("API::Object"))
        self.assertTrue(is_opaque_type("WebKit::UserData"))

    def test_non_opaque_types_with_type_map(self):
        self.assertFalse(is_opaque_type("String"))
        self.assertFalse(is_opaque_type("WebCore::URL"))
        self.assertFalse(is_opaque_type("bool"))
        self.assertFalse(is_opaque_type("uint32_t"))

    def test_alias_resolution_via_type_map(self):
        # FileSystem::Salt is a using alias for std::array<uint8_t, 8>
        self.assertTrue(is_opaque_type("FileSystem::Salt"))

    def test_deep_transitive_chain(self):
        # ScriptBuffer -> FragmentedSharedBuffer -> SharedMemoryHandle -> MachSendRight
        self.assertTrue(is_opaque_type("WebCore::ScriptBuffer"))

    def test_non_opaque_subclass_variant(self):
        # API::Object is opaque (has opaque subclass API::Data)
        # but API::String is not itself opaque
        self.assertTrue(is_opaque_type("API::Data"))
        self.assertFalse(is_opaque_type("API::String"))

    def test_container_wrapping_transitive_type(self):
        # SharedBuffer is transitively opaque; containers of it should be too
        self.assertTrue(is_opaque_type("Vector<std::pair<Ref<WebCore::SharedBuffer>, WebCore::CDMKeyStatus>>"))

    def test_non_opaque_struct_in_type_map(self):
        # Types with serialization definitions but no opaque members
        self.assertFalse(is_opaque_type("WebCore::IntSize"))
        self.assertFalse(is_opaque_type("WebCore::FloatRect"))

    def test_char16_t_odt_concern(self):
        # char16_t in an opaque container should be detected
        self.assertTrue(is_opaque_type("std::span<const char16_t>"))
        self.assertTrue(is_opaque_type("Vector<char16_t>"))
        # But bare char16_t is not opaque
        self.assertFalse(is_opaque_type("char16_t"))

    def test_safe_wrapper_with_mixed_entries(self):
        # ShareableBitmapHandle has SafeWrapper for m_handle but Legacy for
        # m_configuration (which is transitively opaque). It should NOT be
        # considered a SafeWrapper because not all opaque entries are safe.
        self.assertTrue(is_opaque_type("WebCore::ShareableBitmapHandle"))


if __name__ == '__main__':
    unittest.main()
