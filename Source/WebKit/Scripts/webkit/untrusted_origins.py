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

"""Enforces that web origins (and in future, other types) crossing into a
privileged process arrive as IPC::Untrusted<T>.

A privileged process cannot trust a web origin supplied by a web content
process. Wrapping such a parameter in IPC::Untrusted<T> makes that untrustedness
part of the type: the receiving handler cannot reach the value without running one
of the designated validation procedures (see Platform/IPC/Untrusted.h), which
confirm that the sending process had authority over the origin or domain.
"""

import os
import re


# Types that name a web origin, site, or domain. A privileged process must not
# trust one of these when it came from web content.
UNTRUSTED_TYPES = {
    "WebCore::ClientOrigin",
    "WebCore::RegistrableDomain",
    "WebCore::SecurityOrigin",
    "WebCore::SecurityOriginData",
    "WebCore::Site",
}

# Processes that hold privilege a web content process does not, and so must not take
# a web-content-supplied origin on trust.
PRIVILEGED_PROCESSES = {
    "UI",
    "Networking",
    "GPU",
    "Model",
}


UNTRUSTED_WRAPPER = "IPC::Untrusted"

# Files permitted to declare a designated validation procedure by specializing
# IPC::IsValidationProcedureFor. Confining these keeps the set of ways to recover a
# trusted value from an Untrusted<T> small and reviewable. Enforced by
# test_validation_procedures_are_confined below.
VALIDATION_PROCEDURE_HEADERS = {
    "Platform/IPC/Untrusted.h",
    "NetworkProcess/FirstPartyForCookiesAuthority.h",
    "NetworkProcess/ServiceWorker/ServiceWorkerOriginAuthority.h",
    "NetworkProcess/storage/StorageOriginAuthority.h",
    "UIProcess/FirstPartyAuthority.h",
}

# Text that declares a validation procedure, and so may only appear in the headers above.
VALIDATION_PROCEDURE_MARKERS = (
    "struct IsValidationProcedureFor",
    "IPC::CanValidateUntrusted<",
)


def _strip_const_and_whitespace(type_str):
    if not type_str:
        return ""
    type_str = type_str.strip()
    if type_str.startswith("const "):
        type_str = type_str[6:].strip()
    return type_str


def _split_template_parameters(parameter_list):
    """Split template parameters, honouring nested angle brackets.

    Example: "HashMap<String, URL>, int" -> ["HashMap<String, URL>", "int"]
    """
    parameters = []
    current = ""
    depth = 0

    for character in parameter_list:
        if character == '<':
            depth += 1
            current += character
        elif character == '>':
            depth -= 1
            current += character
        elif character == ',' and not depth:
            parameter = _strip_const_and_whitespace(current)
            if parameter:
                parameters.append(parameter)
            current = ""
        else:
            current += character

    parameter = _strip_const_and_whitespace(current)
    if parameter:
        parameters.append(parameter)

    return parameters


def _split_container(type_str):
    """Return (container_name, parameters) for a template type, else (None, None)."""
    match = re.match(r'^(?P<name>[A-Za-z_][A-Za-z_0-9:]*)<(?P<parameters>.+)>$', type_str)
    if not match:
        return None, None
    return match.group('name'), _split_template_parameters(match.group('parameters'))


def unwrap_untrusted(type_str):
    """Return the wrapped type if type_str is IPC::Untrusted<T>, else None."""
    container, parameters = _split_container(_strip_const_and_whitespace(type_str))
    if container != UNTRUSTED_WRAPPER or not parameters or len(parameters) != 1:
        return None
    return parameters[0]


def unwrap_if_untrusted(type_str):
    """The type inside IPC::Untrusted<T>, or type_str itself when it is not wrapped."""
    return unwrap_untrusted(type_str) or type_str


def conveys_untrusted_value(type_str, visited=None):
    """Return the untrusted type conveyed by type_str, or None.

    An IPC::Untrusted<T> wrapper is transparent here: what matters is whether the
    parameter names an origin or URL at all, not whether it is already wrapped.

    Every container is traversed.
    """
    if visited is None:
        visited = set()

    if type_str in visited:
        return None
    visited.add(type_str)

    clean_type = _strip_const_and_whitespace(type_str)
    if clean_type in UNTRUSTED_TYPES:
        return clean_type

    container, parameters = _split_container(clean_type)
    if not container or not parameters:
        return None
    for parameter in parameters:
        result = conveys_untrusted_value(parameter, visited.copy())
        if result is not None:
            return result

    return None


def _process_names(attribute_value):
    """DispatchedFrom and DispatchedTo may each name several processes, e.g. "WebContent|Model"."""
    return [name for name in (attribute_value or '').split('|') if name]


def is_privileged_receiver(receiver):
    """True if the receiver takes messages from web content into a privileged process."""
    return ('WebContent' in _process_names(receiver.receiver_dispatched_from)
            and any(name in PRIVILEGED_PROCESSES for name in _process_names(receiver.receiver_dispatched_to)))


if __name__ == '__main__':
    import unittest

    class TestUntrustedOrigins(unittest.TestCase):

        def test_direct_untrusted_types(self):
            for type_string in UNTRUSTED_TYPES:
                self.assertEqual(conveys_untrusted_value(type_string), type_string)

        def test_non_untrusted_types(self):
            for type_string in ['String', 'int', 'bool', 'WTF::UUID', 'WebCore::PageIdentifier',
                                'Vector<String>', 'std::optional<uint64_t>', 'HashMap<String, int>']:
                self.assertIsNone(conveys_untrusted_value(type_string))

        def test_containers(self):
            self.assertEqual(conveys_untrusted_value('std::optional<WebCore::Site>'), 'WebCore::Site')
            self.assertEqual(conveys_untrusted_value('HashSet<WebCore::SecurityOriginData>'), 'WebCore::SecurityOriginData')
            self.assertEqual(conveys_untrusted_value('Vector<std::pair<String, WebCore::RegistrableDomain>>'),
                             'WebCore::RegistrableDomain')
            self.assertEqual(conveys_untrusted_value('HashMap<WebCore::ClientOrigin, uint64_t>'), 'WebCore::ClientOrigin')
            self.assertEqual(conveys_untrusted_value('std::optional<const WebCore::Site>'), 'WebCore::Site')

        def test_unknown_containers_are_traversed(self):
            self.assertEqual(conveys_untrusted_value('SomeUnknownTemplate<WebCore::Site>'), 'WebCore::Site')
            self.assertIsNone(conveys_untrusted_value('SomeUnknownTemplate<String>'))

        def test_wrapper_is_transparent_to_detection(self):
            self.assertEqual(conveys_untrusted_value('IPC::Untrusted<WebCore::Site>'), 'WebCore::Site')
            self.assertEqual(conveys_untrusted_value('IPC::Untrusted<std::optional<WebCore::SecurityOriginData>>'),
                             'WebCore::SecurityOriginData')

        def test_unwrap_untrusted(self):
            self.assertEqual(unwrap_untrusted('IPC::Untrusted<WebCore::Site>'), 'WebCore::Site')
            self.assertEqual(unwrap_untrusted('IPC::Untrusted<HashSet<WebCore::SecurityOriginData>>'),
                             'HashSet<WebCore::SecurityOriginData>')
            self.assertIsNone(unwrap_untrusted('URL'))
            self.assertIsNone(unwrap_untrusted('std::optional<WebCore::Site>'))
            self.assertIsNone(unwrap_untrusted(''))

        def test_unwrap_if_untrusted(self):
            self.assertEqual(unwrap_if_untrusted('IPC::Untrusted<WebCore::Site>'), 'WebCore::Site')
            self.assertEqual(unwrap_if_untrusted('URL'), 'URL')
            self.assertEqual(unwrap_if_untrusted('std::optional<WebCore::Site>'), 'std::optional<WebCore::Site>')
            self.assertEqual(unwrap_if_untrusted(''), '')

        def test_bad_formatting(self):
            for type_string in ['', 'Vector<>', 'Vector', '<URL>', 'IPC::Untrusted<>']:
                self.assertIsNone(conveys_untrusted_value(type_string))
                self.assertIsNone(unwrap_untrusted(type_string))

        def test_split_template_parameters(self):
            self.assertEqual(_split_template_parameters('HashMap<String, URL>, int'), ['HashMap<String, URL>', 'int'])
            self.assertEqual(_split_template_parameters('URL'), ['URL'])
            self.assertEqual(_split_template_parameters(''), [])

        def test_validation_procedures_are_confined(self):
            source_root = os.path.normpath(os.path.join(os.path.dirname(__file__), '..', '..'))
            found = set()
            for directory, _, filenames in os.walk(source_root):
                for filename in filenames:
                    if not filename.endswith(('.h', '.cpp', '.mm')):
                        continue
                    path = os.path.join(directory, filename)
                    with open(path, 'r', errors='replace') as source_file:
                        contents = source_file.read()
                    if any(marker in contents for marker in VALIDATION_PROCEDURE_MARKERS):
                        found.add(os.path.relpath(path, source_root))
            self.assertEqual(found, VALIDATION_PROCEDURE_HEADERS,
                             'A designated validation procedure may only be declared in the headers listed in '
                             'VALIDATION_PROCEDURE_HEADERS. Adding one needs a security review.')

    unittest.main()
