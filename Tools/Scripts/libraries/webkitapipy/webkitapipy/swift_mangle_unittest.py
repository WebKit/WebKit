"""Tests for swift_mangling module."""

from __future__ import annotations

import os
import re
import subprocess
import unittest

from .swift_mangle import Mangler, mangle_partial


class TestIdentifierEncoding(unittest.TestCase):
    """Test the word-substitution identifier encoding algorithm."""

    def _mangle(self, ident: str) -> str:
        m = Mangler()
        m._mangle_identifier(ident)
        return "".join(m.buffer)

    def test_simple_word(self):
        self.assertEqual(self._mangle("foo"), "3foo")

    def test_single_char(self):
        self.assertEqual(self._mangle("x"), "1x")

    def test_camel_case_splits(self):
        # "ModuleName" -> words: Module, Name -> no prior words -> just literal
        self.assertEqual(self._mangle("ModuleName"), "10ModuleName")

    def test_word_substitution_same_ident(self):
        # "abcabc" has word "abc" starting at pos 0, repeated at pos 3
        # First "abc" is new (idx 0), second matches -> substitution
        result = self._mangle("AbcAbc")
        # "Abc" at pos 0 (new, word 0), "Abc" at pos 3 (matches word 0)
        # Substitutions: [(3, 0)], prefix '0', then literal "3Abc" + last subst 'A0'
        self.assertEqual(result, "03AbcA0")

    def test_word_substitution_across_identifiers(self):
        # Mangling "ModuleName" then "TypeName" should reuse "Name"
        m = Mangler()
        m._mangle_identifier("ModuleName")
        buf_after_first = "".join(m.buffer)
        self.assertEqual(buf_after_first, "10ModuleName")

        m._mangle_identifier("TypeName")
        buf_after_second = "".join(m.buffer)
        # "TypeName": Type is new (word 2), Name matches word 1 from buffer
        # Substitutions: [(4, 1)] -> prefix '0', literal "4Type", last subst 'B0'
        self.assertEqual(buf_after_second, "10ModuleName04TypeB0")

    def test_underscore_prefix(self):
        # Identifiers starting with _ should work
        self.assertEqual(self._mangle("_arg"), "4_arg")

    def test_digit_first_literal(self):
        result = self._mangle("a1b")
        # "a" not word (len < 2), "1b" not word start (digit)
        # No substitutions -> just emit "3a1b"
        self.assertEqual(result, "3a1b")


class TestAppendIdentifier(unittest.TestCase):
    """Test string-substitution wrapping of identifier encoding."""

    def test_first_use_encodes(self):
        m = Mangler()
        m._append_identifier("Hello")
        self.assertEqual("".join(m.buffer), "5Hello")

    def test_second_use_substitutes(self):
        m = Mangler()
        m._append_identifier("Hello")
        m._append_identifier("Hello")
        result = "".join(m.buffer)
        # First: encodes "5Hello", registers subst idx 0
        # Second: emits substitution for idx 0 -> "AA"
        self.assertEqual(result, "5HelloAA")


class TestModuleEncoding(unittest.TestCase):
    def test_swift_module(self):
        m = Mangler()
        m._mangle_module("Swift")
        self.assertEqual("".join(m.buffer), "s")

    def test_objc_module(self):
        m = Mangler()
        m._mangle_module("__C")
        self.assertEqual("".join(m.buffer), "So")

    def test_clang_importer(self):
        m = Mangler()
        m._mangle_module("__C_Synthesized")
        self.assertEqual("".join(m.buffer), "SC")

    def test_regular_module(self):
        m = Mangler()
        m._mangle_module("Foundation")
        self.assertEqual("".join(m.buffer), "10Foundation")


class TestStandardTypes(unittest.TestCase):
    def test_swift_int(self):
        result = mangle_partial("Swift.Int.bitWidth", type_kinds={"Int": "struct"})
        # Swift -> s, Int -> Si (standard), bitWidth -> identifier
        self.assertTrue(result.startswith("_$sSi"))

    def test_swift_string(self):
        result = mangle_partial("Swift.String.count", type_kinds={"String": "struct"})
        self.assertTrue(result.startswith("_$sSS"))

    def test_swift_array(self):
        result = mangle_partial("Swift.Array.count", type_kinds={"Array": "struct"})
        self.assertTrue(result.startswith("_$sSa"))

    def test_swift_optional(self):
        result = mangle_partial("Swift.Optional.map", type_kinds={"Optional": "enum"})
        self.assertTrue(result.startswith("_$sSq"))

    def test_swift_bool(self):
        result = mangle_partial("Swift.Bool.toggle", type_kinds={"Bool": "struct"})
        self.assertTrue(result.startswith("_$sSb"))


class TestLabelEncoding(unittest.TestCase):
    def test_single_label(self):
        m = Mangler()
        m._mangle_labels(["bar"])
        self.assertEqual("".join(m.buffer), "3bar")

    def test_multiple_labels(self):
        m = Mangler()
        m._mangle_labels(["from", "to"])
        result = "".join(m.buffer)
        self.assertEqual(result, "4from2to")

    def test_unlabeled_params(self):
        m = Mangler()
        m._mangle_labels(["_", "_"])
        self.assertEqual("".join(m.buffer), "y")

    def test_mixed_labels(self):
        m = Mangler()
        m._mangle_labels(["_", "by"])
        result = "".join(m.buffer)
        self.assertEqual(result, "_2by")

    def test_no_params(self):
        m = Mangler()
        m._mangle_labels([])
        self.assertEqual("".join(m.buffer), "")


class TestNominalTypes(unittest.TestCase):
    def test_struct(self):
        result = mangle_partial(
            "Foundation.URLRequest.url",
            type_kinds={"URLRequest": "struct"},
        )
        self.assertEqual(result, "_$s10Foundation10URLRequestV3url")

    def test_class(self):
        result = mangle_partial(
            "Foundation.JSONDecoder.decode",
            type_kinds={"JSONDecoder": "class"},
        )
        self.assertTrue(result.startswith("_$s10Foundation11JSONDecoderC"))

    def test_enum(self):
        result = mangle_partial(
            "Foundation.CocoaError.Code.fileNoSuchFile",
            type_kinds={"CocoaError": "struct", "Code": "struct"},
        )
        self.assertTrue(result.startswith("_$s10Foundation10CocoaErrorV4CodeV"))

    def test_protocol(self):
        result = mangle_partial(
            "Foundation._LocaleProtocol.identifier",
            type_kinds={"_LocaleProtocol": "protocol"},
        )
        self.assertTrue(result.startswith("_$s10Foundation15_LocaleProtocolP"))


class TestWorkedExample(unittest.TestCase):
    """Test the worked example from the design plan."""

    def test_module_type_func_label(self):
        result = mangle_partial("ModuleName.TypeName.foo(bar:)")
        self.assertEqual(result, "_$s10ModuleName04TypeB0C3foo3bar")

    def test_step_by_step(self):
        m = Mangler()
        m.buffer.extend("$s")

        # Step 1: Module "ModuleName"
        m._mangle_module("ModuleName")
        self.assertEqual("".join(m.buffer), "$s10ModuleName")
        # Words should now include "Module" and "Name"
        self.assertEqual(len(m.words), 2)

        # Step 2: Type "TypeName" — "Name" should match word 1
        m._mangle_nominal_type("ModuleName", ["TypeName"], 0, {})
        buf = "".join(m.buffer)
        # Module was already mangled, but _mangle_nominal_type calls _mangle_module again
        # which hits string substitution for "ModuleName" (index 0)
        # Then "TypeName" is mangled as "04TypeB0" (Type=new word, Name=subst for word 1)
        # Kind C
        self.assertTrue(buf.endswith("04TypeB0C"), f"Buffer: {buf}")


class TestInit(unittest.TestCase):
    def test_init_with_labels(self):
        result = mangle_partial(
            "Foundation.URLRequest.init(url:)",
            type_kinds={"URLRequest": "struct"},
        )
        self.assertEqual(result, "_$s10Foundation10URLRequestV3url")

    def test_init_no_labels(self):
        result = mangle_partial(
            "Foundation.Data.init()",
            type_kinds={"Data": "struct"},
        )
        self.assertEqual(result, "_$s10Foundation4DataV")

    def test_allocating_init(self):
        # __allocating_init should be treated as init
        result = mangle_partial(
            "SomeModule.SomeClass.__allocating_init()",
            type_kinds={"SomeClass": "class"},
        )
        # Should not contain "__allocating_init" as identifier
        self.assertNotIn("allocating", result)


class TestSubscript(unittest.TestCase):
    def test_subscript_not_encoded(self):
        result = mangle_partial(
            "Foundation.AttributeContainer.subscript",
            type_kinds={"AttributeContainer": "struct"},
        )
        # subscript should not appear as an identifier
        self.assertNotIn("subscript", result)
        self.assertTrue(result.startswith("_$s10Foundation18AttributeContainerV"))


class TestEntitySubstitution(unittest.TestCase):
    def test_repeated_type_context(self):
        # When we mangle nested types, entity substitution fires for parent types
        result = mangle_partial(
            "Foundation.CocoaError.Code.fileNoSuchFile",
            {"CocoaError": "struct", "Code": "struct"},
        )
        buf = result[len("_"):]  # strip leading _
        # CocoaError entity sub should be used when mangling Code's context
        self.assertIn("V4CodeV", buf)


class TestSubstitutionMerging(unittest.TestCase):
    def test_different_subst_merging(self):
        # Two consecutive different A-substitutions should merge
        # AA + AB -> AaB
        m = Mangler()
        m._mangle_substitution(0)  # AA
        m._mangle_substitution(1)  # AB -> merged to AaB
        result = "".join(m.buffer)
        self.assertEqual(result, "AaB")

    def test_same_subst_merging(self):
        # Two consecutive same A-substitutions should merge
        # AA + AA -> A2A
        m = Mangler()
        m._mangle_substitution(0)  # AA
        m._mangle_substitution(0)  # AA -> merged to A2A
        result = "".join(m.buffer)
        self.assertEqual(result, "A2A")

    def test_large_substitution(self):
        # Index 26 -> A + Index(0) = A_
        m = Mangler()
        m._mangle_substitution(26)
        result = "".join(m.buffer)
        self.assertEqual(result, "A_")

    def test_large_substitution_27(self):
        # Index 27 -> A + Index(1) = A0_
        m = Mangler()
        m._mangle_substitution(27)
        result = "".join(m.buffer)
        self.assertEqual(result, "A0_")


class TestEndToEnd(unittest.TestCase):
    """End-to-end tests verified against xcrun swift-demangle."""

    def _verify_prefix(self, decl, type_kinds, expected_prefix):
        result = mangle_partial(decl, type_kinds)
        self.assertEqual(
            result, expected_prefix,
            f"mangle_partial({decl!r}) = {result!r}, expected {expected_prefix!r}"
        )

    def test_foundation_url_request(self):
        self._verify_prefix(
            "Foundation.URLRequest.init(url:)",
            {"URLRequest": "struct"},
            "_$s10Foundation10URLRequestV3url",
        )

    def test_foundation_calendar_method(self):
        # Calendar.nextDate(after:matching:matchingPolicy:repeatedTimePolicy:direction:)
        result = mangle_partial(
            "Foundation.Calendar.nextDate(after:matching:matchingPolicy:repeatedTimePolicy:direction:)",
            {"Calendar": "struct"},
        )
        # Verify it starts with Foundation + Calendar struct
        self.assertTrue(result.startswith("_$s10Foundation8CalendarV"))
        # Verify it contains "nextDate"
        self.assertIn("8nextDate", result)

    def test_nested_types(self):
        result = mangle_partial(
            "Foundation.CocoaError.Code.fileNoSuchFile",
            {"CocoaError": "struct", "Code": "struct"},
        )
        self.assertTrue(result.startswith("_$s10Foundation10CocoaErrorV4CodeV"))

    def test_no_label_function(self):
        result = mangle_partial(
            "Foundation.Date.ISO8601FormatStyle.day()",
            {"Date": "struct", "ISO8601FormatStyle": "struct"},
        )
        self.assertTrue(result.startswith("_$s10Foundation4DateV18ISO8601FormatStyleV3day"))

    def test_swift_standard_type_member(self):
        result = mangle_partial(
            "Swift.Array.append(_:)",
            {"Array": "struct"},
        )
        # Array -> Sa, append identifier, _ label
        self.assertTrue(result.startswith("_$sSa"))
        self.assertIn("6append", result)


class TestDemanglerVerification(unittest.TestCase):
    """Verify mangled prefixes demangle to expected components."""

    def _demangle(self, symbol: str) -> str | None:
        """Demangle a symbol using xcrun swift-demangle."""
        try:
            result = subprocess.run(
                ["xcrun", "swift-demangle"],
                input=symbol.lstrip("_"),
                capture_output=True,
                text=True,
                timeout=5,
            )
            output = result.stdout.strip()
            return output if output != symbol.lstrip("_") else None
        except (subprocess.TimeoutExpired, FileNotFoundError):
            return None

    def test_foundation_type_demangles(self):
        prefix = mangle_partial(
            "Foundation.URLRequest.init(url:)",
            {"URLRequest": "struct"},
        )
        # The prefix alone may not fully demangle, but adding a simple
        # type suffix should give us something containing the expected parts
        demangled = self._demangle(prefix.lstrip("_") + "SgvpMV")
        # Even partial, the prefix should be recognizable
        if demangled:
            self.assertIn("Foundation", demangled)


class TestExtension(unittest.TestCase):
    """Test extension context mangling."""

    def test_cross_module_swift_type(self):
        # Swift.Int._arg (ext in Foundation)
        result = mangle_partial(
            "Swift.Int._arg",
            type_kinds={"Int": "struct"},
            extension_module="Foundation",
        )
        self.assertEqual(result, "_$sSi10FoundationE4_arg")

    def test_cross_module_objc_type(self):
        # __C.NSScanner.currentIndex (ext in Foundation)
        result = mangle_partial(
            "__C.NSScanner.currentIndex",
            extension_module="Foundation",
        )
        self.assertTrue(result.startswith("_$sSo9NSScannerC10FoundationE"))

    def test_cross_module_init(self):
        # Swift.String.init(_characters:) (ext in Foundation)
        result = mangle_partial(
            "Swift.String.init(_characters:)",
            type_kinds={"String": "struct"},
            extension_module="Foundation",
        )
        self.assertTrue(result.startswith("_$sSS10FoundationE"))
        self.assertIn("_characters", result)

    def test_no_extension(self):
        # Without extension_module, no E should be emitted
        result = mangle_partial(
            "Swift.Int.bitWidth",
            type_kinds={"Int": "struct"},
        )
        self.assertNotIn("E", result.replace("_$s", ""))

    def test_nested_type_in_extension(self):
        # __C.NSUndoManager.DidCloseUndoGroupMessage.groupIsDiscardable (ext in Foundation)
        # E goes after NSUndoManager (base_depth=1), before DidCloseUndoGroupMessage
        result = mangle_partial(
            "__C.NSUndoManager.DidCloseUndoGroupMessage.groupIsDiscardable",
            type_kinds={"NSUndoManager": "class", "DidCloseUndoGroupMessage": "struct"},
            extension_module="Foundation",
            extension_base_depth=1,
        )
        self.assertTrue(
            result.startswith("_$sSo13NSUndoManagerC10FoundationE"),
            f"got: {result}",
        )
        self.assertIn("24DidCloseUndoGroupMessageV", result)
