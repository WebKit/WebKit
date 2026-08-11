"""Partial Swift name mangling.

Produces mangled symbol *prefixes* for Swift declarations.
Given a simplified declaration string like ``ModuleName.TypeName.foo(bar:)``,
produces ``_$s10ModuleName04TypeB0C3foo3bar`` — containing module, nominal
types, function name, and argument labels, but **not** type information.

This enables prefix-matching against real Swift symbols.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import Mapping

# Portions of this code are based the Swift compiler:
# https://github.com/swiftlang/swift, retrieved January 2026.
#
# Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
# Licensed under Apache License v2.0 with Runtime Library Exception
#
# See https://swift.org/LICENSE.txt for license information
# See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors


# ---------------------------------------------------------------------------
# Standard-type substitution tables (from StandardTypesMangling.def)
# ---------------------------------------------------------------------------

# Swift module standard types: name -> mangling char
# Emitted as "S<char>"
STANDARD_TYPES: Mapping[str, str] = {
    # Structures
    "AutoreleasingUnsafeMutablePointer": "A",
    "Array": "a",
    "Bool": "b",
    "Dictionary": "D",
    "Double": "d",
    "Float": "f",
    "Set": "h",
    "DefaultIndices": "I",
    "Int": "i",
    "Character": "J",
    "ClosedRange": "N",
    "Range": "n",
    "ObjectIdentifier": "O",
    "UnsafePointer": "P",
    "UnsafeMutablePointer": "p",
    "UnsafeBufferPointer": "R",
    "UnsafeMutableBufferPointer": "r",
    "String": "S",
    "Substring": "s",
    "UInt": "u",
    "UnsafeRawPointer": "V",
    "UnsafeMutableRawPointer": "v",
    "UnsafeRawBufferPointer": "W",
    "UnsafeMutableRawBufferPointer": "w",
    # Enums
    "Optional": "q",
    # Protocols
    "BinaryFloatingPoint": "B",
    "Encodable": "E",
    "Decodable": "e",
    "FloatingPoint": "F",
    "RandomNumberGenerator": "G",
    "Hashable": "H",
    "Numeric": "j",
    "BidirectionalCollection": "K",
    "RandomAccessCollection": "k",
    "Comparable": "L",
    "Collection": "l",
    "MutableCollection": "M",
    "RangeReplaceableCollection": "m",
    "Equatable": "Q",
    "Sequence": "T",
    "IteratorProtocol": "t",
    "UnsignedInteger": "U",
    "RangeExpression": "X",
    "Strideable": "x",
    "RawRepresentable": "Y",
    "StringProtocol": "y",
    "SignedInteger": "Z",
    "BinaryInteger": "z",
}

# Concurrency standard types: name -> mangling char
# Emitted as "Sc<char>"
STANDARD_TYPES_CONCURRENCY: Mapping[str, str] = {
    "Actor": "A",
    "CheckedContinuation": "C",
    "UnsafeContinuation": "c",
    "CancellationError": "E",
    "UnownedSerialExecutor": "e",
    "Executor": "F",
    "SerialExecutor": "f",
    "TaskGroup": "G",
    "ThrowingTaskGroup": "g",
    "TaskExecutor": "h",
    "AsyncIteratorProtocol": "I",
    "AsyncSequence": "i",
    "UnownedJob": "J",
    "MainActor": "M",
    "TaskPriority": "P",
    "AsyncStream": "S",
    "AsyncThrowingStream": "s",
    "Task": "T",
    "UnsafeCurrentTask": "t",
}

# Kind operators for nominal types
KIND_OPERATORS: dict[str, str] = {
    "class": "C",
    "struct": "V",
    "enum": "O",
    "protocol": "P",
}

# ---------------------------------------------------------------------------
# Word-boundary helpers (ports isWordStart / isWordEnd from ManglingUtils.h)
# ---------------------------------------------------------------------------


def _is_word_start(ch: str) -> bool:
    """A word starts at a non-digit, non-underscore, non-NUL character."""
    return ch != "\0" and ch != "_" and not ch.isdigit()


def _is_word_end(ch: str, prev: str) -> bool:
    """A word ends when we hit underscore, NUL, or a lowercase->uppercase transition."""
    if ch == "_" or ch == "\0":
        return True
    if not prev.isupper() and ch.isupper():
        return True
    return False


# ---------------------------------------------------------------------------
# Substitution merging (ports SubstitutionMerging from ManglingUtils.h)
# ---------------------------------------------------------------------------


@dataclass
class SubstitutionMerging:
    last_subst_position: int = 0
    last_subst_size: int = 0
    last_num_substs: int = 0
    last_subst_is_standard: bool = False

    MAX_REPEAT_COUNT: int = 2048

    def try_merge_subst(
        self, buffer: list[str], subst: str, is_standard_subst: bool
    ) -> bool:
        buf_len = len(buffer)
        if (
            self.last_num_substs > 0
            and self.last_num_substs < self.MAX_REPEAT_COUNT
            and buf_len == self.last_subst_position + self.last_subst_size
            and self.last_subst_is_standard == is_standard_subst
        ):
            # Get the last substitution mangling (drop leading digits)
            last_raw = "".join(buffer[-self.last_subst_size:])
            last_subst = last_raw.lstrip("0123456789")

            if last_subst != subst and not is_standard_subst:
                # Merge with a different 'A' substitution: AB -> AbC
                self.last_subst_position = buf_len
                self.last_num_substs = 1
                # Replace last char (uppercase) with lowercase version
                old_char = buffer[-1]
                buffer[-1] = old_char.lower()
                buffer.extend(subst)
                self.last_subst_size = len(subst)
                return True

            if last_subst == subst:
                # Merge with the same substitution: AB -> A2B, S3i -> S4i
                self.last_num_substs += 1
                del buffer[self.last_subst_position:]
                count_str = str(self.last_num_substs)
                buffer.extend(count_str)
                buffer.extend(subst)
                self.last_subst_size = len(buffer) - self.last_subst_position
                return True

        # Can't merge; caller will mangle it. Record for future merging.
        self.last_subst_position = buf_len + 1  # +1 for the 'A' or 'S' prefix
        self.last_subst_size = len(subst)
        self.last_num_substs = 1
        self.last_subst_is_standard = is_standard_subst
        return False


# ---------------------------------------------------------------------------
# WordReplacement helper
# ---------------------------------------------------------------------------


@dataclass
class WordReplacement:
    string_pos: int
    word_idx: int


@dataclass
class SubstitutionWord:
    start: int
    length: int


# ---------------------------------------------------------------------------
# Mangler
# ---------------------------------------------------------------------------


@dataclass
class Mangler:
    """Stateful Swift name mangler for producing symbol prefixes."""

    buffer: list[str] = field(default_factory=list)

    # Word substitutions (up to 26 words shared across identifiers in one symbol)
    words: list[SubstitutionWord] = field(default_factory=list)

    # Entity/string substitutions share a single index counter
    string_substitutions: dict[str, int] = field(default_factory=dict)
    entity_substitutions: dict[str, int] = field(default_factory=dict)
    next_substitution_index: int = 0

    subst_merging: SubstitutionMerging = field(default_factory=SubstitutionMerging)

    def _get_buffer_str(self) -> str:
        return "".join(self.buffer)

    def _reset_buffer(self, pos: int) -> None:
        del self.buffer[pos:]

    # -------------------------------------------------------------------
    # Index encoding (from Mangler.h)
    # -------------------------------------------------------------------

    @staticmethod
    def _encode_index(n: int) -> str:
        """Encode an index: Index(0) = '_', Index(1) = '0_', Index(k) = '{k-1}_'."""
        if n == 0:
            return "_"
        return f"{n - 1}_"

    # -------------------------------------------------------------------
    # Entity/string substitution management
    # -------------------------------------------------------------------

    def _add_substitution(self, key: str, is_entity: bool = False) -> None:
        value = self.next_substitution_index
        self.next_substitution_index += 1
        if is_entity:
            self.entity_substitutions[key] = value
        else:
            self.string_substitutions[key] = value

    def _try_substitution(self, key: str, is_entity: bool = False) -> bool:
        subs = self.entity_substitutions if is_entity else self.string_substitutions
        if key in subs:
            self._mangle_substitution(subs[key])
            return True
        return False

    def _mangle_substitution(self, idx: int) -> None:
        """Emit a substitution encoding for the given index (ports mangleSubstitution)."""
        if idx >= 26:
            # Large substitution: A + Index(idx - 26)
            self.buffer.extend("A")
            self.buffer.extend(self._encode_index(idx - 26))
            return

        subst_char = chr(idx + ord("A"))
        subst = subst_char
        if self.subst_merging.try_merge_subst(self.buffer, subst, False):
            pass  # merged
        else:
            self.buffer.extend("A")
            self.buffer.append(subst_char)

    # -------------------------------------------------------------------
    # Identifier encoding (word substitution algorithm)
    # Faithful port of ManglingUtils.h mangleIdentifier()
    # -------------------------------------------------------------------

    def _mangle_identifier(self, ident: str) -> None:
        """Core word-substitution encoding of an identifier."""
        words_in_buffer = len(self.words)
        subst_words: list[WordReplacement] = []

        NOT_INSIDE_WORD = -1
        word_start_pos = NOT_INSIDE_WORD

        for pos in range(len(ident) + 1):
            ch = ident[pos] if pos < len(ident) else "\0"

            if word_start_pos != NOT_INSIDE_WORD and _is_word_end(
                ch, ident[pos - 1]
            ):
                word_len = pos - word_start_pos
                word = ident[word_start_pos:word_start_pos + word_len]

                # Look up word in buffer-sourced words first
                word_idx = -1
                buf_str = self._get_buffer_str()
                for i in range(0, words_in_buffer):
                    w = self.words[i]
                    existing = buf_str[w.start:w.start + w.length]
                    if word == existing:
                        word_idx = i
                        break

                # Then look in identifier-sourced words
                if word_idx < 0:
                    for i in range(words_in_buffer, len(self.words)):
                        w = self.words[i]
                        existing = ident[w.start:w.start + w.length]
                        if word == existing:
                            word_idx = i
                            break

                if word_idx >= 0:
                    assert word_idx < 26
                    subst_words.append(WordReplacement(word_start_pos, word_idx))
                elif word_len >= 2 and len(self.words) < 26:
                    # New word — position relative to identifier for now
                    self.words.append(SubstitutionWord(word_start_pos, word_len))

                word_start_pos = NOT_INSIDE_WORD

            if word_start_pos == NOT_INSIDE_WORD and _is_word_start(ch):
                word_start_pos = pos

        # If we have word substitutions, prefix with '0'
        if subst_words:
            self.buffer.append("0")

        pos = 0
        # Add sentinel
        subst_words.append(WordReplacement(len(ident), -1))

        for idx in range(len(subst_words)):
            repl = subst_words[idx]
            if pos < repl.string_pos:
                # Emit literal substring length
                first = True
                self.buffer.extend(str(repl.string_pos - pos))
                while pos < repl.string_pos:
                    # Update start positions of new words
                    if (
                        words_in_buffer < len(self.words)
                        and self.words[words_in_buffer].start == pos
                    ):
                        self.words[words_in_buffer].start = len(self.buffer)
                        words_in_buffer += 1

                    if first and ident[pos].isdigit():
                        self.buffer.append("X")
                    else:
                        self.buffer.append(ident[pos])
                    pos += 1
                    first = False

            if repl.word_idx >= 0:
                assert repl.word_idx <= words_in_buffer
                pos += self.words[repl.word_idx].length
                if idx < len(subst_words) - 2:
                    # Non-last: lowercase
                    self.buffer.append(chr(repl.word_idx + ord("a")))
                else:
                    # Last: uppercase
                    self.buffer.append(chr(repl.word_idx + ord("A")))
                    if pos == len(ident):
                        self.buffer.append("0")

    # -------------------------------------------------------------------
    # appendIdentifier — string-substitution wrapper
    # -------------------------------------------------------------------

    def _append_identifier(self, ident: str) -> None:
        """Append an identifier with string-substitution check."""
        if ident in self.string_substitutions:
            self._mangle_substitution(self.string_substitutions[ident])
            return

        self._add_substitution(ident, is_entity=False)
        self._mangle_identifier(ident)

    # -------------------------------------------------------------------
    # Module encoding
    # -------------------------------------------------------------------

    def _mangle_module(self, name: str) -> None:
        if name == "Swift":
            self.buffer.append("s")
            return
        if name == "__C" or name == "ObjectiveC":
            self.buffer.extend("So")
            return
        if name == "__C_Synthesized":
            self.buffer.extend("SC")
            return
        self._append_identifier(name)

    # -------------------------------------------------------------------
    # Standard type substitution
    # -------------------------------------------------------------------

    def _try_standard_type_subst(self, module: str, name: str) -> bool:
        """Try emitting a standard type substitution. Returns True if emitted."""
        if module != "Swift":
            return False

        if name in STANDARD_TYPES:
            ch = STANDARD_TYPES[name]
            subst = ch
            if self.subst_merging.try_merge_subst(self.buffer, subst, True):
                pass
            else:
                self.buffer.append("S")
                self.buffer.append(ch)
            return True

        if name in STANDARD_TYPES_CONCURRENCY:
            ch = STANDARD_TYPES_CONCURRENCY[name]
            self.buffer.extend("Sc")
            self.buffer.append(ch)
            return True

        return False

    # -------------------------------------------------------------------
    # Nominal type encoding
    # -------------------------------------------------------------------

    def _mangle_nominal_type(
        self,
        module: str,
        type_chain: list[str],
        type_idx: int,
        type_kinds: Mapping[str, str],
        extension_module: str | None = None,
        extension_base_depth: int | None = None,
    ) -> None:
        """Mangle a nominal type at position type_idx in the chain."""
        name = type_chain[type_idx]
        kind = type_kinds.get(name, "class")
        kind_op = KIND_OPERATORS[kind]

        # Build entity key
        if type_idx == 0:
            entity_key = f"{module}.{name}"
        else:
            parts = [module] + [type_chain[i] for i in range(type_idx + 1)]
            entity_key = ".".join(parts)

        # Check standard type substitution
        if type_idx == 0 and self._try_standard_type_subst(module, name):
            self.entity_substitutions[entity_key] = self.next_substitution_index
            self.next_substitution_index += 1
            return

        # Check entity substitution
        if entity_key in self.entity_substitutions:
            self._mangle_substitution(self.entity_substitutions[entity_key])
            return

        # Mangle context (module or parent type)
        if type_idx == 0:
            self._mangle_module(module)
        else:
            self._mangle_nominal_type(
                module, type_chain, type_idx - 1, type_kinds,
                extension_module, extension_base_depth,
            )

        # Extension boundary: emit extension context between base types
        # and types defined within the extension.
        if (
            extension_module is not None
            and extension_base_depth is not None
            and type_idx == extension_base_depth
        ):
            self._mangle_extension_context(extension_module)

        # Mangle the type name
        self._append_identifier(name)

        # Kind operator
        self.buffer.append(kind_op)

        # Register entity substitution
        self.entity_substitutions[entity_key] = self.next_substitution_index
        self.next_substitution_index += 1

    # -------------------------------------------------------------------
    # Label encoding (from appendFunction)
    # -------------------------------------------------------------------

    def _mangle_labels(self, labels: list[str]) -> None:
        """Encode parameter labels."""
        if not labels:
            # No parameters at all — emit nothing
            return

        has_any_label = any(n != "_" and n != "" for n in labels)
        if has_any_label:
            for label in labels:
                if label == "_" or label == "":
                    self.buffer.append("_")
                else:
                    self._append_identifier(label)
        else:
            # All unlabeled
            self.buffer.append("y")

    # -------------------------------------------------------------------
    # Input parsing
    # -------------------------------------------------------------------

    @staticmethod
    def _parse_decl(decl: str) -> tuple[str, list[str], str | None, list[str] | None]:
        """Parse a declaration string.

        Returns (module, type_chain, member_name, labels).
        - member_name is None for bare types
        - labels is None for properties, list of strings for functions
        """
        components = decl.split(".")
        module = components[0]
        if len(components) == 1:
            return (module, [], None, None)

        last = components[-1]
        type_chain = list(components[1:-1])

        # Check if last component has parentheses (function/init)
        paren_match = re.match(r"^(\w+)\((.*)\)$", last)
        if paren_match:
            member_name = paren_match.group(1)
            label_str = paren_match.group(2)
            # Normalize __allocating_init -> init
            if member_name == "__allocating_init":
                member_name = "init"
            if label_str:
                # "bar:baz:" -> ["bar", "baz"]
                # "_:foo:" -> ["_", "foo"]
                raw_labels = label_str.rstrip(":").split(":")
                labels = [n.strip() if n.strip() else "_" for n in raw_labels]
            else:
                labels = []
            return (module, type_chain, member_name, labels)
        elif last == "subscript":
            # subscript is special — not mangled as an identifier
            return (module, type_chain, "subscript", None)
        else:
            return (module, type_chain, last, None)

    # -------------------------------------------------------------------
    # Extension context
    # -------------------------------------------------------------------

    def _mangle_extension_context(self, module: str) -> None:
        """Emit extension module + E operator."""
        self._mangle_module(module)
        self.buffer.append("E")

    # -------------------------------------------------------------------
    # Top-level API
    # -------------------------------------------------------------------

    def mangle(
        self,
        decl: str,
        type_kinds: Mapping[str, str] | None = None,
        extension_module: str | None = None,
        extension_base_depth: int | None = None,
    ) -> str:
        """Mangle a declaration to its prefix form.

        Args:
            extension_module: Module where the extension is defined.
            extension_base_depth: Number of types in the base type chain
                (before the extension). Types at index >= this value are
                defined within the extension. Defaults to len(type_chain)
                (all types are base types, extension adds only members).
        """
        if type_kinds is None:
            type_kinds = {}

        module, type_chain, member_name, labels = self._parse_decl(decl)

        self.buffer.extend("$s")

        # Resolve extension_base_depth: default = all types are base types.
        if extension_module and extension_base_depth is None:
            extension_base_depth = len(type_chain)

        # Determine whether extension context is threaded into the type chain
        # (nested types defined in extension) or appended after it (members only).
        ext_in_chain = (
            extension_module is not None
            and extension_base_depth is not None
            and extension_base_depth < len(type_chain)
        )

        # 1. Module + type chain
        if type_chain:
            self._mangle_nominal_type(
                module, type_chain, len(type_chain) - 1, type_kinds,
                extension_module=extension_module if ext_in_chain else None,
                extension_base_depth=extension_base_depth if ext_in_chain else None,
            )
        else:
            self._mangle_module(module)

        # 2. Extension context after full type chain (simple case: no nested
        #    types in extension, only members/properties).
        if extension_module and not ext_in_chain:
            self._mangle_extension_context(extension_module)

        # 3. Member
        if member_name is not None:
            if member_name == "init":
                # Constructor — labels only, no member name identifier
                if labels is not None:
                    self._mangle_labels(labels)
            elif member_name == "subscript":
                # Subscript — not encoded as an identifier name
                # Labels would follow if provided, but subscript labels
                # are part of the type signature we don't emit
                pass
            elif labels is not None:
                # Function/method
                self._append_identifier(member_name)
                self._mangle_labels(labels)
            else:
                # Property
                self._append_identifier(member_name)

        return "_" + "".join(self.buffer)


def mangle_partial(
    decl: str,
    type_kinds: Mapping[str, str] | None = None,
    extension_module: str | None = None,
    extension_base_depth: int | None = None,
) -> str:
    """Compute the mangled symbol prefix for a Swift declaration.

    Args:
        decl: Simplified declaration like ``"Module.Type.func(label:)"``
        type_kinds: Optional map from type name to kind
                    (``"class"``, ``"struct"``, ``"enum"``, ``"protocol"``).
                    Defaults to ``"class"`` for unknown types.
        extension_module: If the declaration is in an extension defined in
                    a different module, the name of that module (e.g.
                    ``"Foundation"``).  Emits the extension context
                    (``<module>E``) between the type chain and the member.
        extension_base_depth: Number of types in the base type chain
                    (before the extension). Types at index >= this value
                    are defined within the extension. Defaults to
                    ``len(type_chain)`` (all types are base, extension
                    adds only members).

    Returns:
        Mangled prefix string like ``"_$s10ModuleName..."``
    """
    m = Mangler()
    return m.mangle(decl, type_kinds, extension_module, extension_base_depth)
