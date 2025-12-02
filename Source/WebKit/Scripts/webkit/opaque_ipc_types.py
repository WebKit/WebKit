# Copyright (C) 2025 Apple Inc. All rights reserved.
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
import re


# These are by themselves opaque without a container
OPAQUE_TYPES = {
    "NotDispatchableFromWebContent",  # For testing message generation
    "MachSendRight",
    "MachSendRightAnnotated",
    "WTF::MachSendRightAnnotated",
    "CFDataRef",
    "CFArrayRef",
    "CFDictionaryRef",
    "NSArray",
    "NSData",
    "NSDictionary",
    "WebCore::SharedMemoryHandle",
    "WebCore::SharedMemory::Handle",
    "IPC::SharedBufferReference",
    "WebKit::CoreIPCData",
    "WebKit::CoreIPCPlistDictionary",
    "WebKit::CoreIPCPlistArray"
}

# If these types are in a 'opaque container' we are concerned
ODT_CONCERN = {
    "char", "signed char", "unsigned char", "int8_t", "uint8_t",
}.union(OPAQUE_TYPES)

# Containers that are an ODT when they contain a type from ODT_CONCERN or OPAQUE_TYPES
#
# Configuration options:
#   check_params: Which template parameters to check for opaque data
#     - "first": Check only first parameter (e.g., Vector<T>)
#     - "first_two": Check first two parameters (e.g., HashMap<K,V>)
#     - "all": Check all parameters
#     - "selective": Check specific indices (use with selective_indices)
#
#   special_parsing: Custom parameter extraction logic
#     - "array_parsing": For std::array<T, N>, extract only T (ignore N)
#
OPAQUE_CONTAINERS = {
    "std::span": {"check_params": "first"},
    "std::array": {"check_params": "first", "special_parsing": "array_parsing"},
    "Vector": {"check_params": "first"},
    "FixedVector": {"check_params": "first"},
}

# Containers that should be decomposed to find opaque inner containers.
#
# These containers are "transparent" - they don't make types opaque by themselves,
# but we traverse through them to find opaque types within.
#
# Configuration options:
#   check_params: Which template parameters to check (same options as OPAQUE_CONTAINERS)
#
#   propagate_context: Whether opaque container context is preserved through this container
#     - True: Simple wrappers (std::optional, RetainPtr) - preserve parent opaque context
#              Example: Vector<std::optional<uint8_t>> is opaque because Vector creates
#              opaque context and std::optional preserves it to uint8_t
#     - False: Structural containers (std::pair, Variant, Expected) - reset opaque context
#              Example: std::pair<uint8_t, String> is NOT opaque because std::pair resets
#              context, so uint8_t is checked outside of opaque container context
#
TRANSPARENT_CONTAINERS = {
    # Simple wrappers - preserve opaque context from parent
    "std::optional": {"check_params": "first", "propagate_context": True},
    "RetainPtr": {"check_params": "first", "propagate_context": True},

    "Expected": {"check_params": "selective", "selective_indices": [0], "propagate_context": False},
    "Variant": {"check_params": "all", "propagate_context": False},

    "std::pair": {"check_params": "all", "propagate_context": False},
    "std::tuple": {"check_params": "all", "propagate_context": False},
    "KeyValuePair": {"check_params": "all", "propagate_context": False},
    "OptionalTuple": {"check_params": "all", "propagate_context": False},
    "IPC::ArrayReferenceTuple": {"check_params": "all", "propagate_context": False},

    "std::unique_ptr": {"check_params": "first", "propagate_context": False},
    "UniqueRef": {"check_params": "first", "propagate_context": False},
    "Ref": {"check_params": "first", "propagate_context": False},
    "RefPtr": {"check_params": "first", "propagate_context": False},

    "HashMap": {"check_params": "first_two", "propagate_context": False},
    "MemoryCompactRobinHoodHashMap": {"check_params": "first_two", "propagate_context": False},
    "MemoryCompactLookupOnlyRobinHoodHashSet": {"check_params": "first", "propagate_context": False},
    "HashSet": {"check_params": "first", "propagate_context": False},
    "OptionSet": {"check_params": "first", "propagate_context": False},
    "Markable": {"check_params": "first", "propagate_context": False},
    "HashCountedSet": {"check_params": "first", "propagate_context": False},
}

ALL_CONTAINER_CONFIGS = {**OPAQUE_CONTAINERS, **TRANSPARENT_CONTAINERS}


def _is_odt_concern(type_str):
    return type_str.strip() in ODT_CONCERN


def _remove_const_and_whitespace(type_str):
    if not type_str:
        return ""
    type_str = type_str.strip()
    if type_str.startswith("const "):
        type_str = type_str[6:].strip()
    return type_str


def _split_template_parameters(param_list):
    """Split template parameters handling nested brackets properly

    Example: "Vector<uint8_t>, String, int" -> ["Vector<uint8_t>", "String", "int"]
    """
    if not param_list:
        return []

    parameters = []
    current_param = ""
    bracket_depth = 0

    for char in param_list:
        if char == '<':
            bracket_depth += 1
            current_param += char
        elif char == '>':
            bracket_depth -= 1
            current_param += char
        elif char == ',' and bracket_depth == 0:
            # Found a top-level comma - end of current parameter
            param = _remove_const_and_whitespace(current_param)
            if param:
                parameters.append(param)
            current_param = ""
        else:
            current_param += char

    # Add the last parameter
    param = _remove_const_and_whitespace(current_param)
    if param:
        parameters.append(param)

    return parameters


def _array_special_parsing(param_list):
    """Special parsing for std::array<T, N> - only return T, ignore N"""
    params = _split_template_parameters(param_list)
    return params[:1] if params else []


def _get_container_info(type_str):
    """Get container name and parameters from a type string
    Returns: (container_name, parameters_list, is_opaque_container) or (None, None, False) if not a container
    """
    for container_name in ALL_CONTAINER_CONFIGS:
        prefix = container_name + "<"
        if type_str.startswith(prefix) and type_str.endswith(">"):
            param_list = type_str[len(prefix):-1]

            # Handle special parsing cases
            config = ALL_CONTAINER_CONFIGS[container_name]
            if config.get("special_parsing") == "array_parsing":
                parameters = _array_special_parsing(param_list)
            else:
                parameters = _split_template_parameters(param_list)

            is_opaque_container = container_name in OPAQUE_CONTAINERS
            return container_name, parameters, is_opaque_container

    return None, None, False


def _get_parameters_to_check(container_name, parameters):
    """Get the list of parameters to check based on container config"""
    if not parameters:
        return []

    config = ALL_CONTAINER_CONFIGS.get(container_name, {})
    check_params = config.get("check_params", "all")

    if check_params == "first":
        return parameters[:1]
    elif check_params == "first_two":
        return parameters[:2]
    elif check_params == "all":
        return parameters
    elif check_params == "selective":
        indices = config.get("selective_indices", [])
        return [parameters[i] for i in indices if i < len(parameters)]
    else:
        return []


def _contains_opaque_data(type_str, visited=None, from_opaque_container=False):
    """Check if a type contains opaque data.

    Args:
        type_str: The type string to check
        visited: Set of already visited types (cycle detection)
        from_opaque_container: True if we're in an opaque container's context

    Returns:
        The ODT concern type if found, None otherwise
    """
    if visited is None:
        visited = set()

    # Avoid infinite recursion
    if type_str in visited:
        return None
    visited.add(type_str)

    clean_type = _remove_const_and_whitespace(type_str)

    if clean_type in OPAQUE_TYPES:
        return clean_type

    # ODT concerns are only opaque in opaque container context
    if _is_odt_concern(clean_type) and from_opaque_container:
        return clean_type

    # Try to parse as container (e.g., "Vector<uint8_t>" → container_name="Vector", parameters=["uint8_t"])
    container_name, parameters, is_opaque_container = _get_container_info(clean_type)

    # If not a container template, or container has no parameters, type is not opaque
    # Examples that return None here: "String", "int", "Vector" (no <>), "Vector<>" (empty)
    if not container_name or not parameters:
        return None

    # Get which parameters to check based on container type
    # E.g., HashMap<K,V> checks both K and V, Expected<T,E> only checks T
    params_to_check = _get_parameters_to_check(container_name, parameters)

    # Determine opaque context for checking child parameters
    #
    # ODT concerns (uint8_t, char, etc.) are ONLY opaque when inside
    # an opaque container like Vector or std::span. The context flag tracks this.
    #
    # Three cases:
    #   1. Opaque containers (Vector, std::span) → always create opaque context for children
    #   2. Simple wrappers (std::optional, RetainPtr) → preserve parent's context
    #   3. Structural containers (std::pair, Variant) → reset context to non-opaque
    #
    # Examples:
    #   Vector<uint8_t>: Vector is opaque container → next_context=True → uint8_t is opaque ✓
    #   std::optional<uint8_t>: std::optional preserves context, parent=False → next_context=False → not opaque ✓
    #   Vector<std::optional<uint8_t>>: Vector sets context=True, std::optional preserves it → uint8_t is opaque ✓
    #   std::pair<uint8_t, String>: std::pair resets context → next_context=False → uint8_t not opaque ✓
    #
    if is_opaque_container:
        # Opaque containers always create opaque context for their contents
        next_context = True
    else:
        # Transparent containers may preserve or reset context based on configuration
        config = ALL_CONTAINER_CONFIGS.get(container_name, {})
        propagates = config.get("propagate_context", False)
        next_context = from_opaque_container if propagates else False

    # Check each template parameter for opaque data
    for param in params_to_check:
        clean_param = _remove_const_and_whitespace(param)

        # Fast path: Check if parameter is an ODT concern (uint8_t, char, etc.) in opaque context
        # This catches cases like Vector<uint8_t> without needing to recurse
        if _is_odt_concern(clean_param) and next_context:
            return clean_param

        # Recurse to check if parameter contains opaque data
        # Use visited.copy() to allow same type in different branches (e.g., std::pair<Foo, Foo>)
        # while still preventing infinite loops within a single branch
        result = _contains_opaque_data(param, visited.copy(), next_context)
        if result is not None:
            return result

    return None


ATTRIBUTE_FLAG_HANDLERS = {
    'NotSentFromWebContent': ('can_webcontent_dispatch', False),
    'SecurityGatedReply': ('security_gated_reply', True),
    'NeedsReview': ('needs_review', True),
    'DebugOnly': ('debug_only', True),
    'Legacy': ('legacy', True),
    'UnsafeWrapper': ('unsafe_wrapper', True),
    'SafeWrapper': ('safe_wrapper', True),
}

ATTRIBUTE_KEY_VALUE_HANDLERS = {
    'SerializationPolicyViolation': 'serialization_policy_violation',
    'MemorySafety': 'memory_safety',
    'Docs': 'docs',
}


class OpaqueIPCTypeEntry(object):
    """Represents a single tracked opaque transport type entry"""

    def __init__(self, entry_type, attributes=None,
                 receiver=None, message=None, parameter_name=None, parameter_type=None,
                 alias_name=None, alias_type=None,
                 data_type=None, name_or_method=None, type=None):
        self.entry_type = entry_type
        self.receiver = receiver
        self.message = message
        self.parameter_name = parameter_name
        self.parameter_type = parameter_type
        self.alias_name = alias_name
        self.alias_type = alias_type
        self.data_type = data_type
        self.name_or_method = name_or_method
        self.type = type

        self._parse_attributes(attributes)

    def _parse_attributes(self, attributes):
        """Parse and set attribute flags from attribute string."""
        self.serialization_policy_violation = None
        self.can_webcontent_dispatch = True
        self.legacy = False
        self.security_gated_reply = False
        self.needs_review = False
        self.debug_only = False
        self.unsafe_wrapper = False
        self.safe_wrapper = False
        self.memory_safety = None
        self.docs = None

        if attributes is None:
            return

        for attribute in attributes.split(', '):
            attribute = attribute.strip()
            if '=' in attribute:
                key, value = attribute.split('=', 1)
                key = key.strip()
                value = value.strip()

                if key not in ATTRIBUTE_KEY_VALUE_HANDLERS:
                    valid_attrs = ', '.join(sorted(ATTRIBUTE_KEY_VALUE_HANDLERS.keys()))
                    raise Exception(f"Unknown attribute '{key}' in: [{attributes}]. Valid key=value attributes are: {valid_attrs}")

                attr_name = ATTRIBUTE_KEY_VALUE_HANDLERS[key]
                setattr(self, attr_name, value.strip("'\""))
            else:
                if attribute not in ATTRIBUTE_FLAG_HANDLERS:
                    valid_attrs = ', '.join(sorted(ATTRIBUTE_FLAG_HANDLERS.keys()))
                    raise Exception(f"Unknown attribute '{attribute}' in: [{attributes}]. Valid flag attributes are: {valid_attrs}")

                attr_name, attr_value = ATTRIBUTE_FLAG_HANDLERS[attribute]
                setattr(self, attr_name, attr_value)


class OpaqueIPCTypes(object):
    def __init__(self, tracking_file_path=None):
        if tracking_file_path is None:
            tracking_file_path = os.path.join(os.path.dirname(__file__), 'opaque_ipc_types.tracking.in')

        self.message_params = {}
        self.message_param_replies = {}
        self.alias_params = {}
        self.structure_params = {}

        # State for tracking groups
        current_group_type = None
        current_group_attributes = None

        with open(tracking_file_path, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue

                if line == '}':
                    current_group_type = None
                    current_group_attributes = None
                    continue

                group_header = self._parse_group_header(line)
                if group_header:
                    current_group_attributes, current_group_type = group_header
                    continue

                entry = self._parse_line(line, current_group_type, current_group_attributes)
                if entry:
                    self._add_entry(entry)

    def _parse_group_header(self, line):
        """Parse a group header like: [UnsafeWrapper] Vector<uint8_t> {
        Returns: (attributes, type_name) or None if not a group header
        """
        match = re.match(r'^\[([^\]]*)\]\s+(.+?)\s*\{\s*$', line)
        if match:
            attributes = match.group(1).strip()
            type_name = match.group(2).strip()
            return (attributes, type_name)
        return None

    def _parse_alias_param(self, rest, group_type, attributes):
        """Parse an AliasParam entry."""
        if group_type:
            alias_name = rest.strip()
            alias_type = group_type
        else:
            parts = rest.split(None, 1)
            if len(parts) < 2:
                raise Exception(f'opaque_ipc_types.tracking.in ungrouped AliasParam missing type. Expected format: [attr] AliasParam AliasName TypeString')
            alias_name = parts[0]
            alias_type = parts[1]

        return OpaqueIPCTypeEntry(
            entry_type='AliasParam',
            attributes=attributes,
            alias_name=alias_name,
            alias_type=alias_type
        )

    def _parse_structure_param(self, rest, group_type, attributes):
        """Parse a StructureParam entry."""
        if group_type:
            parts = [rest.strip()]
            param_type = group_type
        else:
            parts = rest.split(None, 1)
            if len(parts) < 2:
                raise Exception(f'opaque_ipc_types.tracking.in ungrouped StructureParam missing type. Expected format: [attr] StructureParam DataType.member TypeString')
            param_type = parts[1]

        if '.' in parts[0]:
            data_type, name_or_method = parts[0].split('.', 1)
        else:
            data_type = parts[0]
            name_or_method = parts[0]

        return OpaqueIPCTypeEntry(
            entry_type='StructureParam',
            attributes=attributes,
            data_type=data_type,
            name_or_method=name_or_method,
            type=param_type
        )

    def _parse_message_param(self, record_type, rest, group_type, attributes):
        """Parse a MessageParam or MessageParamReply entry."""
        if group_type:
            parts = rest.split(None, 1)
            if len(parts) < 2:
                raise Exception(f'opaque_ipc_types.tracking.in grouped {record_type} missing parameter name. Expected format: [attr] {record_type} Receiver.Message parameterName')
            param_type = group_type
        else:
            parts = rest.split(None, 2)
            if len(parts) < 3:
                raise Exception(f'opaque_ipc_types.tracking.in ungrouped {record_type} incomplete. Expected format: [attr] {record_type} Receiver.Message parameterName TypeString')
            param_type = parts[2]

        if '.' in parts[0]:
            receiver, message = parts[0].split('.', 1)
        else:
            receiver = parts[0]
            message = parts[0]

        return OpaqueIPCTypeEntry(
            entry_type=record_type,
            attributes=attributes,
            receiver=receiver,
            message=message,
            parameter_name=parts[1],
            parameter_type=param_type
        )

    def _parse_line(self, line, group_type=None, group_attributes=None):
        match = re.match(r'^\[([^\]]*)\]\s+(\S+)\s+(.+)$', line)
        if not match:
            if group_type:
                raise Exception(f'opaque_ipc_types.tracking.in grouped entry malformed. Line: {line}. Expected format: [attr] RecordType ...')
            raise Exception(f'opaque_ipc_types.tracking.in item missing attributes: {line}')

        attributes, record_type, rest = match.groups()

        combined_attributes = f"{group_attributes}, {attributes}" if group_attributes and attributes else (group_attributes or attributes)

        if record_type == 'AliasParam':
            return self._parse_alias_param(rest, group_type, combined_attributes)
        elif record_type == 'StructureParam':
            return self._parse_structure_param(rest, group_type, combined_attributes)
        elif record_type in ('MessageParam', 'MessageParamReply'):
            return self._parse_message_param(record_type, rest, group_type, combined_attributes)
        else:
            raise Exception(f'Unknown record type: {record_type}')

    def _add_entry(self, entry):
        if entry.entry_type == 'MessageParam':
            key = (f'{entry.receiver}.{entry.message}', entry.parameter_name)
            if key not in self.message_params:
                self.message_params[key] = []
            self.message_params[key].append(entry)
        elif entry.entry_type == 'MessageParamReply':
            key = (f'{entry.receiver}.{entry.message}', entry.parameter_name)
            if key not in self.message_param_replies:
                self.message_param_replies[key] = []
            self.message_param_replies[key].append(entry)
        elif entry.entry_type == 'AliasParam':
            if entry.alias_name not in self.alias_params:
                self.alias_params[entry.alias_name] = []
            self.alias_params[entry.alias_name].append(entry)
        elif entry.entry_type == 'StructureParam':
            key = (entry.data_type, entry.name_or_method)
            if key not in self.structure_params:
                self.structure_params[key] = []
            self.structure_params[key].append(entry)

    def _query_entries(self, entry_dict, key, type_field, type_string):
        """Generic query method for checking if an entry is tracked."""
        if key not in entry_dict:
            return False
        if type_string is None:
            return True
        return any(getattr(e, type_field) == type_string for e in entry_dict[key])

    def _is_webcontent_dispatchable(self, entry_dict, key, type_field, type_string):
        """Generic method to check if entry allows WebContent dispatch."""
        if key not in entry_dict:
            return True
        return not any(
            getattr(e, type_field) == type_string and not e.can_webcontent_dispatch
            for e in entry_dict[key]
        )

    def message_param_tracked(self, receiver, message, parameter_name, type_string=None):
        key = (f'{receiver}.{message}', parameter_name)
        return self._query_entries(self.message_params, key, 'parameter_type', type_string)

    def message_param_reply_tracked(self, receiver, message, parameter_name, type_string=None):
        key = (f'{receiver}.{message}', parameter_name)
        return self._query_entries(self.message_param_replies, key, 'parameter_type', type_string)

    def alias_param_tracked(self, alias_name, type_string=None):
        return self._query_entries(self.alias_params, alias_name, 'alias_type', type_string)

    def structure_param_tracked(self, data_type, name_or_method, type_string=None):
        key = (data_type, name_or_method)
        return self._query_entries(self.structure_params, key, 'type', type_string)

    def webcontent_dispatchable(self, receiver, message, parameter_name, parameter_type):
        key = (f'{receiver}.{message}', parameter_name)
        return self._is_webcontent_dispatchable(self.message_params, key, 'parameter_type', parameter_type)

    def reply_webcontent_dispatchable(self, receiver, message, parameter_name, parameter_type):
        key = (f'{receiver}.{message}', parameter_name)
        return self._is_webcontent_dispatchable(self.message_param_replies, key, 'parameter_type', parameter_type)

    def structure_webcontent_dispatchable(self, data_type, name_or_method, type_string):
        key = (data_type, name_or_method)
        return self._is_webcontent_dispatchable(self.structure_params, key, 'type', type_string)


try:
    opaque_ipc_types = OpaqueIPCTypes()
except FileNotFoundError as e:
    raise Exception(f"opaque_ipc_types.tracking.in file not found: {e}")


def is_opaque_type(type):
    """Check if a type represents opaque data transport.

    Returns True if the type is opaque:
    1. Direct opaque types (MachSendRight, CFDataRef, etc.)
    2. Opaque containers with ODT concerns (Vector<uint8_t>, std::span<char>, etc.)
    3. Transparent containers containing opaque data (std::pair<Vector<uint8_t>, String>)

    Returns False otherwise:
    4. Transparent containers without opaque data (std::pair<uint8_t, String>)
    5. Non-opaque types (String, int, etc.)
    6. Non-containers (uint8_t, WTF::UUID, etc.)
    """
    return _contains_opaque_data(type) is not None


if __name__ == '__main__':
    import unittest

    class TestOpaqueTypes(unittest.TestCase):

        def test_is_odt_concern_function(self):
            for odt_type in ODT_CONCERN:
                self.assertTrue(_is_odt_concern(odt_type), f"Expected {odt_type} to be ODT concern")

            non_odt_types = ['String', 'int', 'float', 'bool', 'WTF::UUID']
            for non_odt_type in non_odt_types:
                self.assertFalse(_is_odt_concern(non_odt_type), f"Expected {non_odt_type} to not be ODT concern")

        def test_contains_opaque_data_function(self):
            # Test opaque containers with ODT concerns
            self.assertEqual(_contains_opaque_data("std::span<uint8_t>"), "uint8_t")
            self.assertEqual(_contains_opaque_data("std::span<const uint8_t>"), "uint8_t")
            self.assertEqual(_contains_opaque_data("Vector<char>"), "char")
            self.assertEqual(_contains_opaque_data("Vector<const char>"), "char")
            self.assertEqual(_contains_opaque_data("std::array<uint8_t, 24>"), "uint8_t")
            self.assertEqual(_contains_opaque_data("std::array<const uint8_t, 16>"), "uint8_t")
            self.assertEqual(_contains_opaque_data("RetainPtr<CFDataRef>"), "CFDataRef")
            self.assertEqual(_contains_opaque_data("Vector<MachSendRight>"), "MachSendRight")

            # Test nested containers
            self.assertEqual(_contains_opaque_data("std::optional<Vector<uint8_t>>"), "uint8_t")
            self.assertEqual(_contains_opaque_data("Vector<std::optional<uint8_t>>"), "uint8_t")
            self.assertEqual(_contains_opaque_data("Vector<std::pair<Vector<uint8_t>, std::optional<WTF::UUID>>>"), "uint8_t")
            self.assertEqual(_contains_opaque_data("Variant<Vector<uint8_t>, String>"), "uint8_t")

            # Test transparent containers without opaque content
            self.assertIsNone(_contains_opaque_data("Expected<uint8_t, String>"))
            self.assertIsNone(_contains_opaque_data("Variant<uint8_t, String>"))
            self.assertIsNone(_contains_opaque_data("std::pair<uint8_t, String>"))
            self.assertIsNone(_contains_opaque_data("std::optional<uint8_t>"))
            self.assertIsNone(_contains_opaque_data("uint8_t"))
            self.assertIsNone(_contains_opaque_data("String"))

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
            self.assertTrue(is_opaque_type("Vector<std::pair<Vector<uint8_t>, std::optional<WTF::UUID>>>"))
            self.assertTrue(is_opaque_type("std::optional<Vector<std::pair<Vector<uint8_t>, String>>>"))
            self.assertTrue(is_opaque_type("Variant<Vector<uint8_t>, WebKit::HTTPBody::Element::FileData, String>"))
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
            # Create a visited set that simulates a circular reference
            visited = {"TestType"}

            # This should return None due to the visited check, not cause infinite recursion
            result = _contains_opaque_data("TestType", visited=visited)
            self.assertIsNone(result)

            # Test with a type that would normally be opaque but is already visited
            visited_opaque = {"Vector<uint8_t>"}
            result = _contains_opaque_data("Vector<uint8_t>", visited=visited_opaque)
            self.assertIsNone(result)

            # Verify normal operation still works when not visited
            result = _contains_opaque_data("Vector<uint8_t>")
            self.assertEqual(result, "uint8_t")

        def test_bad_formatting(self):
            self.assertFalse(is_opaque_type(""))
            self.assertFalse(is_opaque_type("Vector<>"))
            self.assertFalse(is_opaque_type("std::optional<>"))
            self.assertFalse(is_opaque_type("Vector"))
            self.assertFalse(is_opaque_type("std::optional"))
            self.assertFalse(is_opaque_type("<uint8_t>"))

        def test_context_propagation_through_simple_wrappers(self):
            # This is opaque because Vector creates opaque context
            self.assertTrue(is_opaque_type("Vector<std::optional<uint8_t>>"))

            # This is NOT opaque because std::optional alone doesn't create context
            self.assertFalse(is_opaque_type("std::optional<uint8_t>"))

        def test_context_reset_in_structural_containers(self):
            # uint8_t alone in pair is not opaque
            self.assertFalse(is_opaque_type("std::pair<uint8_t, String>"))

            # But Vector<uint8_t> is opaque even in pair
            self.assertTrue(is_opaque_type("std::pair<Vector<uint8_t>, String>"))

        def test_retainptr_with_direct_opaque_types(self):
            self.assertTrue(is_opaque_type("RetainPtr<CFDataRef>"))
            self.assertTrue(is_opaque_type("RetainPtr<NSData>"))
            self.assertTrue(is_opaque_type("RetainPtr<MachSendRight>"))

        def test_deeply_nested_types(self):
            deep_type = "Vector<HashMap<String, std::pair<std::optional<Vector<uint8_t>>, int>>>"
            self.assertTrue(is_opaque_type(deep_type))

        def test_opaque_ipc_types_parsing(self):
            test_file = os.path.join(os.path.dirname(__file__), 'tests', 'test_opaque_ipc_types.tracking.in')
            if not os.path.exists(test_file):
                self.fail(f"Test tracking file not found: {test_file}")

            ot = OpaqueIPCTypes(test_file)

            total_entries = sum(len(entries) for entries in ot.message_params.values())
            total_entries += sum(len(entries) for entries in ot.message_param_replies.values())
            total_entries += sum(len(entries) for entries in ot.alias_params.values())
            total_entries += sum(len(entries) for entries in ot.structure_params.values())

            print(f"\ntest_opaque_ipc_types.tracking.in:")
            print(f"  Total entries: {total_entries}")
            print(f"    MessageParam: {sum(len(e) for e in ot.message_params.values())}")
            print(f"    MessageParamReply: {sum(len(e) for e in ot.message_param_replies.values())}")
            print(f"    AliasParam: {sum(len(e) for e in ot.alias_params.values())}")
            print(f"    StructureParam: {sum(len(e) for e in ot.structure_params.values())}")

            self.assertGreater(total_entries, 0, "Test file should have entries")

            # Specific entries which are in the test file
            self.assertTrue(ot.message_param_tracked('TestWithLegacyReceiver', 'DidCreateWebProcessConnection', 'connectionIdentifier', 'MachSendRight'))
            self.assertTrue(ot.message_param_tracked('TestWithStream', 'SendMachSendRight', 'a1', 'MachSendRight'))
            self.assertTrue(ot.message_param_reply_tracked('TestWithStream', 'ReceiveMachSendRight', 'r1', 'MachSendRight'))
            self.assertTrue(ot.alias_param_tracked('TestNamespace::TestSalt', 'std::array<uint8_t, 8>'))
            self.assertTrue(ot.structure_param_tracked('WebKit::TestStruct', 'buffer', 'Vector<uint8_t>'))

            # Check the lookup logic for non existant items
            self.assertFalse(ot.message_param_tracked('NonExistantReceiver', 'NonExistantMessage', 'NonExistantParameterName', 'NonExistantType'))
            self.assertFalse(ot.message_param_reply_tracked('NonExistantReceiver', 'NonExistantMessage', 'NonExistantParameterName', 'NonExistantType'))
            self.assertFalse(ot.alias_param_tracked('NonExistantAlias', 'NonExistantType'))
            self.assertFalse(ot.structure_param_tracked('NonExistantStruct', 'NonExistantMmember', 'NonExistantType'))

            # Things WebContent can send
            self.assertTrue(ot.structure_webcontent_dispatchable('WebKit::TestStruct', 'buffer', 'Vector<uint8_t>'))
            self.assertFalse(ot.structure_webcontent_dispatchable('WebKit::SecureData', 'encrypted', 'Vector<uint8_t>'))

            self.assertTrue(ot.webcontent_dispatchable('TestWithLegacyReceiver', 'DidCreateWebProcessConnection', 'connectionIdentifier', 'MachSendRight'))
            self.assertFalse(ot.webcontent_dispatchable('TestWithLegacyReceiver', 'OpaqueTypeSecurityAssertion', 'param', 'NotDispatchableFromWebContentType'))

            self.assertTrue(ot.reply_webcontent_dispatchable('UserInterface', 'GetUserData', 'data', 'std::span<const uint8_t>'))
            self.assertFalse(ot.reply_webcontent_dispatchable('TestInterface', 'GetData', 'result', 'std::span<const uint8_t>'))

        def test_production_tracking_file_parses(self):
            ot = OpaqueIPCTypes()

            total_entries = sum(len(e) for e in ot.message_params.values())
            total_entries += sum(len(e) for e in ot.message_param_replies.values())
            total_entries += sum(len(e) for e in ot.alias_params.values())
            total_entries += sum(len(e) for e in ot.structure_params.values())

            print(f"\nopaque_ipc_types.tracking.in")
            print(f"  Total entries: {total_entries}")
            print(f"    MessageParam: {sum(len(e) for e in ot.message_params.values())}")
            print(f"    MessageParamReply: {sum(len(e) for e in ot.message_param_replies.values())}")
            print(f"    AliasParam: {sum(len(e) for e in ot.alias_params.values())}")
            print(f"    StructureParam: {sum(len(e) for e in ot.structure_params.values())}")

            self.assertGreater(total_entries, 10, "opaque_ipc_types.tracking.in seems to have too few entries")

    unittest.main()
