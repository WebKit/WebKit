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

This applies in both directions of travel: to the parameters of a message web content
sends to a privileged process, and to the parameters of a reply web content sends back
to a privileged process that asked it a question.
"""

import os
import re


# Types that name a web origin, site, or domain. A privileged process must not
# trust one of these when it came from web content.
UNTRUSTED_TYPES = {
    "URL",
    "WTF::URL",
    "WebCore::ClientOrigin",
    "WebCore::RegistrableDomain",
    "WebCore::SecurityOrigin",
    "WebCore::SecurityOriginData",
    "WebCore::Site",
}

# Maps each untrusted type onto the IPC::UntrustedValueKind naming it. A serialized struct
# publishes the kinds it carries so that a validator applied to it need only account for
# those, rather than for every kind any struct might contain.
UNTRUSTED_VALUE_KINDS = {
    "URL": "URL",
    "WTF::URL": "URL",
    "WebCore::ClientOrigin": "ClientOrigin",
    "WebCore::RegistrableDomain": "RegistrableDomain",
    "WebCore::SecurityOrigin": "SecurityOrigin",
    "WebCore::SecurityOriginData": "SecurityOriginData",
    "WebCore::Site": "Site",
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

# Kinds of untrusted value that name an authority a process may or may not hold. A URL is not
# one of them: it is usually a request target rather than a claim, so the structs that carry
# only URLs are held back from enforcement for now.
ORIGIN_VALUE_KINDS = {
    "ClientOrigin",
    "RegistrableDomain",
    "SecurityOrigin",
    "SecurityOriginData",
    "Site",
}


def _load_serializer_generator():
    """Loads generate-serializers.py as a module.

    The set of structs messages.py demands a wrapper for has to be the same set the visitor is
    generated for, or one of the two has a hole. Reusing that script's parser rather than
    writing a second one is what makes them the same set by construction. The import is done
    here rather than at module scope because generate-serializers.py imports this module.
    """
    import importlib.util
    path = os.path.join(os.path.dirname(__file__), '..', 'generate-serializers.py')
    spec = importlib.util.spec_from_file_location('generate_serializers', os.path.normpath(path))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


_struct_carriers = None


def struct_types_carrying_untrusted_origins():
    """Serialized struct types that transitively carry an origin, site or domain.

    Wrapping an origin in IPC::Untrusted<T> is only worth anything if putting the same origin
    in a struct field is not a way around it, so these types are enforced the same way. A type
    reachable only through a URL field is excluded; see ORIGIN_VALUE_KINDS.

    The .serialization.in files are found by walking the tree rather than taken from the build,
    so this can name a type the current configuration does not build. That over-approximates,
    which is safe: a message parameter can only have a type the configuration serializes.
    """
    global _struct_carriers
    if _struct_carriers is not None:
        return _struct_carriers

    generator = _load_serializer_generator()
    source_root = os.path.normpath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
    serialized_types = []
    for directory, _, filenames in os.walk(source_root):
        # Holds deliberately malformed inputs for the generators' own negative tests.
        if directory.endswith(os.path.join('Scripts', 'webkit', 'tests')):
            continue
        for filename in filenames:
            if not filename.endswith('.serialization.in'):
                continue
            path = os.path.join(directory, filename)
            with open(path, 'r', errors='replace') as source_file:
                types, _, _, _, _, _ = generator.parse_serialized_types(source_file)
            serialized_types.extend(types)

    serialized_types = generator.resolve_inheritance(serialized_types)
    context = generator.UntrustedValueContext(serialized_types)
    _struct_carriers = set(context.kinds)
    return _struct_carriers


# Files permitted to declare a designated validation procedure, either by specializing
# IPC::IsValidationProcedureFor or by deriving from IPC::CanValidateUntrusted. Confining these
# keeps the set of ways to recover a trusted value from an Untrusted<T> small and reviewable.
# Enforced by test_validation_procedures_are_confined below.
VALIDATION_PROCEDURE_HEADERS = {
    "Platform/IPC/Untrusted.h",
    "GPUProcess/GPUHostedDomainAuthority.h",
    "NetworkProcess/FirstPartyForCookiesAuthority.h",
    "NetworkProcess/ServiceWorker/ServiceWorkerOriginAuthority.h",
    "NetworkProcess/webrtc/RTCDomainAuthority.h",
    "NetworkProcess/storage/StorageOriginAuthority.h",
    "UIProcess/Extensions/ExtensionHostPermissionAuthority.h",
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


def canonical_type(type_str):
    """Return type_str without leading const or surrounding whitespace."""
    return _strip_const_and_whitespace(type_str)


def split_container(type_str):
    """Return (container_name, parameters) if type_str is a template, else (None, None)."""
    return _split_container(_strip_const_and_whitespace(type_str))


def conveys_untrusted_value(type_str, visited=None, extra_types=None):
    """Return the untrusted type conveyed by type_str, or None.

    An IPC::Untrusted<T> wrapper is transparent here: what matters is whether the
    parameter names an origin or URL at all, not whether it is already wrapped.

    Every container is traversed.

    extra_types names further types to treat as untrusted, used to propagate
    untrustedness out of the structs that carry an origin or URL in a field.
    """
    if visited is None:
        visited = set()

    if type_str in visited:
        return None
    visited.add(type_str)

    clean_type = _strip_const_and_whitespace(type_str)
    if clean_type in UNTRUSTED_TYPES:
        return clean_type
    if extra_types and clean_type in extra_types:
        return clean_type

    container, parameters = _split_container(clean_type)
    if not container or not parameters:
        return None
    for parameter in parameters:
        result = conveys_untrusted_value(parameter, visited.copy(), extra_types)
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


def replies_to_privileged_sender(receiver):
    """True if the receiver's replies travel from web content back into a privileged process.

    The mirror of is_privileged_receiver. A privileged process asking web content a question is
    no better placed to trust the answer than to trust an unsolicited message: the reply is
    whatever the web process chose to put in it.

    A receiver that declines to name its senders may be asked by any of them, so its replies are
    treated as reaching a privileged process. The other end is read strictly: unless web content
    dispatches the receiver, nothing untrustworthy composed the reply.
    """
    if 'WebContent' not in _process_names(receiver.receiver_dispatched_to):
        return False
    if receiver.receiver_dispatched_from_exception:
        return True
    return any(name in PRIVILEGED_PROCESSES for name in _process_names(receiver.receiver_dispatched_from))


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
            self.assertEqual(conveys_untrusted_value('std::optional<URL>'), 'URL')
            self.assertEqual(conveys_untrusted_value('HashSet<WebCore::SecurityOriginData>'), 'WebCore::SecurityOriginData')
            self.assertEqual(conveys_untrusted_value('Vector<std::pair<String, WebCore::RegistrableDomain>>'),
                             'WebCore::RegistrableDomain')
            self.assertEqual(conveys_untrusted_value('HashMap<WebCore::ClientOrigin, uint64_t>'), 'WebCore::ClientOrigin')
            self.assertEqual(conveys_untrusted_value('std::optional<const URL>'), 'URL')

        def test_unknown_containers_are_traversed(self):
            self.assertEqual(conveys_untrusted_value('SomeUnknownTemplate<WebCore::Site>'), 'WebCore::Site')
            self.assertIsNone(conveys_untrusted_value('SomeUnknownTemplate<String>'))

        def test_extra_types(self):
            carrying = {'WebKit::FrameInfoData'}
            self.assertIsNone(conveys_untrusted_value('WebKit::FrameInfoData'))
            self.assertEqual(conveys_untrusted_value('WebKit::FrameInfoData', extra_types=carrying),
                             'WebKit::FrameInfoData')
            self.assertEqual(conveys_untrusted_value('std::optional<WebKit::FrameInfoData>', extra_types=carrying),
                             'WebKit::FrameInfoData')
            self.assertIsNone(conveys_untrusted_value('WebKit::SomethingElse', extra_types=carrying))

        def test_canonical_type_and_split_container(self):
            self.assertEqual(canonical_type('  const URL '), 'URL')
            self.assertEqual(split_container('HashMap<WebCore::ClientOrigin, uint64_t>'),
                             ('HashMap', ['WebCore::ClientOrigin', 'uint64_t']))
            self.assertEqual(split_container('URL'), (None, None))

        def test_wrapper_is_transparent_to_detection(self):
            self.assertEqual(conveys_untrusted_value('IPC::Untrusted<URL>'), 'URL')
            self.assertEqual(conveys_untrusted_value('IPC::Untrusted<std::optional<WebCore::SecurityOriginData>>'),
                             'WebCore::SecurityOriginData')

        def test_unwrap_untrusted(self):
            self.assertEqual(unwrap_untrusted('IPC::Untrusted<URL>'), 'URL')
            self.assertEqual(unwrap_untrusted('IPC::Untrusted<HashSet<WebCore::SecurityOriginData>>'),
                             'HashSet<WebCore::SecurityOriginData>')
            self.assertIsNone(unwrap_untrusted('URL'))
            self.assertIsNone(unwrap_untrusted('std::optional<URL>'))
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

        def test_direction_of_travel(self):
            class FakeReceiver(object):
                def __init__(self, dispatched_from, dispatched_to, dispatched_from_exception=False):
                    self.receiver_dispatched_from = dispatched_from
                    self.receiver_dispatched_to = dispatched_to
                    self.receiver_dispatched_from_exception = dispatched_from_exception

            web_content_to_ui = FakeReceiver('WebContent', 'UI')
            self.assertTrue(is_privileged_receiver(web_content_to_ui))
            self.assertFalse(replies_to_privileged_sender(web_content_to_ui))

            ui_to_web_content = FakeReceiver('UI', 'WebContent')
            self.assertFalse(is_privileged_receiver(ui_to_web_content))
            self.assertTrue(replies_to_privileged_sender(ui_to_web_content))

            # Either end may name several processes, and one privileged end is enough.
            self.assertTrue(replies_to_privileged_sender(FakeReceiver('GPU|WebContent', 'WebContent|Model')))
            self.assertTrue(is_privileged_receiver(FakeReceiver('WebContent|Model', 'Networking')))

            # Neither end is web content, or neither is privileged.
            self.assertFalse(is_privileged_receiver(FakeReceiver('UI', 'Networking')))
            self.assertFalse(replies_to_privileged_sender(FakeReceiver('UI', 'Networking')))
            self.assertFalse(replies_to_privileged_sender(FakeReceiver('WebContent', 'WebContent')))
            self.assertFalse(is_privileged_receiver(FakeReceiver(None, None)))
            self.assertFalse(replies_to_privileged_sender(FakeReceiver(None, None)))

            # A receiver that does not name its senders could be asked by a privileged one, but
            # only a receiver web content dispatches can compose an untrustworthy reply.
            self.assertTrue(replies_to_privileged_sender(FakeReceiver(None, 'WebContent', dispatched_from_exception=True)))
            self.assertFalse(replies_to_privileged_sender(FakeReceiver(None, 'UI', dispatched_from_exception=True)))

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
