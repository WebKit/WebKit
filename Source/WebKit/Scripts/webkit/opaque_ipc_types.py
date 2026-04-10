# Copyright (C) 2025-2026 Apple Inc. All rights reserved.
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

from .serialization_parser import parse_serialized_types


# Leaf opaque types — no serialization definition that decomposes further.
# Types that are transitively opaque through their members are detected
# automatically via the serialized type map.
OPAQUE_TYPES = {
    "NotDispatchableFromWebContent",  # For testing message generation
    "MachSendRight",
    "WTF::MachSendRightAnnotated",
    "CFDataRef",
    "CFArrayRef",
    "CFDictionaryRef",
    "NSArray",
    "NSData",
    "NSDictionary",
}

# If these types are in a 'opaque container' we are concerned
ODT_CONCERN = {
    "char", "signed char", "unsigned char", "int8_t", "uint8_t", "char16_t"
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


def _is_verifiably_opaque(type_str):
    """Check if a type is verifiably opaque without full type-map recursion.

    Used by is_safe_wrapper_type() to filter stale entries without risking
    infinite recursion. Returns True if the type is opaque by string analysis
    OR has its own tracking entries (meaning it was verified as opaque when tracked).
    """
    clean = _remove_const_and_whitespace(type_str)
    if _find_opaque_data_type(clean, use_type_map=False) is not None:
        return True
    # If the type itself has structure_param entries, it was tracked as containing
    # opaque data. This catches types like WebCore::SharedMemoryHandle that aren't
    # leaf types but are known to be opaque from their own tracking entries.
    if opaque_ipc_types is not None:
        if clean in opaque_ipc_types.structure_param_data_types:
            return True
    return False


def _find_opaque_data_type(type_str, visited=None, from_opaque_container=False, use_type_map=True):
    """Find the opaque data type within a type string, if any.

    Args:
        type_str: The type string to check
        visited: Set of already visited types (cycle detection)
        from_opaque_container: True if we're in an opaque container's context
        use_type_map: Whether to resolve types via the serialized type map.
                      Set to False to avoid recursion from SafeWrapper checks.

    Returns:
        The opaque data type name if found, None otherwise
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

    if container_name and parameters:
        params_to_check = _get_parameters_to_check(container_name, parameters)

        if is_opaque_container:
            next_context = True
        else:
            config = ALL_CONTAINER_CONFIGS.get(container_name, {})
            propagates = config.get("propagate_context", False)
            next_context = from_opaque_container if propagates else False

        for param in params_to_check:
            clean_param = _remove_const_and_whitespace(param)

            if _is_odt_concern(clean_param) and next_context:
                return clean_param

            result = _find_opaque_data_type(param, visited.copy(), next_context, use_type_map)
            if result is not None:
                return result

    # Resolve via serialized type map — check if the type's members are transitively opaque
    if use_type_map and opaque_ipc_types is not None and opaque_ipc_types.type_map is not None:
        # SafeWrapper types terminate the recursion chain
        if opaque_ipc_types.is_safe_wrapper_type(clean_type):
            return None

        member_types = opaque_ipc_types.type_map['members'].get(clean_type)
        if member_types is not None:
            for member_type in member_types:
                result = _find_opaque_data_type(member_type, visited.copy())
                if result is not None:
                    return result

        subclass_types = opaque_ipc_types.type_map['subclasses'].get(clean_type)
        if subclass_types is not None:
            for subclass_type in subclass_types:
                result = _find_opaque_data_type(subclass_type, visited.copy())
                if result is not None:
                    return result

        alias_target = opaque_ipc_types.type_map['aliases'].get(clean_type)
        if alias_target is not None:
            return _find_opaque_data_type(alias_target, visited.copy())

        # WTF:: types are stored without the prefix in the type map.
        # Intentionally don't pass from_opaque_container — this is a name
        # resolution step, not a container context propagation.
        if clean_type.startswith('WTF::'):
            return _find_opaque_data_type(clean_type[5:], visited.copy(), use_type_map=use_type_map)

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


def unwrap_to_candidate_types(type_str):
    """Unwrap container types to produce candidate types for tracking lookups.

    When a parameter type is e.g. Vector<Foo>, the tracking entry may be for
    either the full type or the inner type Foo (since the Vector wrapper doesn't
    change the security justification). Returns a list starting with the original
    type, followed by any unwrapped inner types.
    """
    clean = _remove_const_and_whitespace(type_str)
    candidates = [clean]
    container_name, parameters, _ = _get_container_info(clean)
    if container_name and parameters:
        params_to_check = _get_parameters_to_check(container_name, parameters)
        for param in params_to_check:
            candidates.extend(unwrap_to_candidate_types(param))
    return candidates


class OpaqueIPCTypes(object):
    def __init__(self, tracking_file_path=None):
        if tracking_file_path is None:
            tracking_file_path = os.path.join(os.path.dirname(__file__), 'opaque_ipc_types.tracking.in')

        self.message_params = {}
        self.message_param_replies = {}
        self.alias_params = {}
        self.structure_params = {}
        self.type_map = self._build_type_map()

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

        # Precomputed index: set of data_type names that have structure_param entries.
        # Used by _is_verifiably_opaque() to avoid O(n) scans.
        self.structure_param_data_types = {dt for (dt, _) in self.structure_params}

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

    def is_safe_wrapper_type(self, type_name):
        """Check if a type is tracked as SafeWrapper for all its opaque members.

        Only considers entries whose member type is verifiably opaque:
        either via string analysis (leaf types, container patterns) or by
        being a known type in the serialized type map. Entries for types
        that cannot be verified as opaque are skipped, making this robust
        to stale entries.
        """
        opaque_entries = []
        for (data_type, name_or_method), entry_list in self.structure_params.items():
            if data_type == type_name:
                for e in entry_list:
                    if _is_verifiably_opaque(e.type):
                        opaque_entries.append(e)
        if not opaque_entries:
            return False
        return all(e.safe_wrapper for e in opaque_entries)

    @staticmethod
    def _build_type_map():
        """Build the serialized type map by parsing all .serialization.in files."""
        scripts_webkit_dir = os.path.dirname(os.path.abspath(__file__))
        webkit_dir = os.path.dirname(os.path.dirname(scripts_webkit_dir))

        type_members = {}
        aliases = {}
        subclass_variants = {}

        for root, dirs, files in os.walk(webkit_dir):
            if os.sep + 'tests' in root:
                continue
            for f in sorted(files):
                if not f.endswith('.serialization.in'):
                    continue
                path = os.path.join(root, f)
                with open(path) as fh:
                    types, enums, headers, usings, fwds, objc = parse_serialized_types(fh)

                    for st in types:
                        name = f'{st.cf_type}Ref' if st.cf_type is not None else st.namespace_if_not_wtf_and_name()
                        if st.members_are_subclasses:
                            subclass_variants[name] = [
                                f'{m.namespace}::{m.name}' if m.namespace else m.name
                                for m in st.members
                            ]
                        else:
                            members = [m.type for m in st.serialized_members()]
                            parent = st.parent_class
                            while parent is not None:
                                members = [m.type for m in parent.serialized_members()] + members
                                parent = parent.parent_class
                            existing = type_members.get(name)
                            if existing is not None:
                                seen = set(existing)
                                for m in members:
                                    if m not in seen:
                                        existing.append(m)
                                        seen.add(m)
                            else:
                                type_members[name] = members

                    for us in usings:
                        parts = [line.strip() for line in us.alias_lines if not line.strip().startswith('#')]
                        target = ' '.join(parts).strip().rstrip(';').strip()
                        if target:
                            existing = aliases.get(us.name)
                            if existing is None or len(target) > len(existing):
                                aliases[us.name] = target

        return {'members': type_members, 'aliases': aliases, 'subclasses': subclass_variants}


try:
    opaque_ipc_types = OpaqueIPCTypes()
except FileNotFoundError as e:
    raise Exception(f"opaque_ipc_types.tracking.in file not found: {e}")


def is_opaque_type(type):
    """Check if a type represents opaque data transport."""
    return _find_opaque_data_type(type) is not None
