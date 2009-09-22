description(
    'This test checks a few Number.toFixed cases, including ' +
    '<a href="http://bugzilla.opendarwin.org/show_bug.cgi?id=5307">5307: Number.toFixed does not round 0.5 up</a>' +
    ' and ' +
    '<a href="http://bugzilla.opendarwin.org/show_bug.cgi?id=5308">5308: Number.toFixed does not include leading zero</a>' +
    '.');

shouldBe("(0).toFixed(0)", "'0'");

shouldBe("(0.49).toFixed(0)", "'0'");
shouldBe("(0.5).toFixed(0)", "'1'");
shouldBe("(0.51).toFixed(0)", "'1'");

shouldBe("(-0.49).toFixed(0)", "'-0'");
shouldBe("(-0.5).toFixed(0)", "'-1'");
shouldBe("(-0.51).toFixed(0)", "'-1'");

shouldBe("(0).toFixed(1)", "'0.0'");

shouldBe("(0.449).toFixed(1)", "'0.4'");
shouldBe("(0.45).toFixed(1)", "'0.5'");
shouldBe("(0.451).toFixed(1)", "'0.5'");
shouldBe("(0.5).toFixed(1)", "'0.5'");
shouldBe("(0.549).toFixed(1)", "'0.5'");
shouldBe("(0.55).toFixed(1)", "'0.6'");
shouldBe("(0.551).toFixed(1)", "'0.6'");

shouldBe("(-0.449).toFixed(1)", "'-0.4'");
shouldBe("(-0.45).toFixed(1)", "'-0.5'");
shouldBe("(-0.451).toFixed(1)", "'-0.5'");
shouldBe("(-0.5).toFixed(1)", "'-0.5'");
shouldBe("(-0.549).toFixed(1)", "'-0.5'");
shouldBe("(-0.55).toFixed(1)", "'-0.6'");
shouldBe("(-0.551).toFixed(1)", "'-0.6'");

var posInf = 1/0;
var negInf = -1/0;
var nan = 0/0;

// From Acid3, http://bugs.webkit.org/show_bug.cgi?id=16640
shouldBeEqualToString("(0.0).toFixed(4)", "0.0000");
shouldBeEqualToString("(-0.0).toFixed(4)", "0.0000");
shouldBeEqualToString("(0.0).toFixed()", "0");
shouldBeEqualToString("(-0.0).toFixed()", "0");

// From http://bugs.webkit.org/show_bug.cgi?id=5258
shouldBeEqualToString("(1234.567).toFixed()", "1235");
shouldBeEqualToString("(1234.567).toFixed(0)", "1235");
// 0 equivilents
shouldBeEqualToString("(1234.567).toFixed(null)", "1235");
shouldBeEqualToString("(1234.567).toFixed(false)", "1235");
shouldBeEqualToString("(1234.567).toFixed('foo')", "1235");
shouldBeEqualToString("(1234.567).toFixed(nan)", "1235"); // nan is treated like 0

shouldBeEqualToString("(1234.567).toFixed(1)", "1234.6");
shouldBeEqualToString("(1234.567).toFixed(true)", "1234.6"); // just like 1
shouldBeEqualToString("(1234.567).toFixed('1')", "1234.6"); // just like 1

shouldBeEqualToString("(1234.567).toFixed(2)", "1234.57");
shouldBeEqualToString("(1234.567).toFixed(2.9)", "1234.57");
shouldBeEqualToString("(1234.567).toFixed(5)", "1234.56700");
shouldBeEqualToString("(1234.567).toFixed(20)", "1234.56700000000000727596");

// SpiderMonkey allows precision values -20 to 100, we only allow 0 to 20 currently
shouldBeEqualToString("(1234.567).toFixed(21)", "1234.567000000000007275958");
shouldBeEqualToString("(1234.567).toFixed(100)", "1234.5670000000000072759576141834259033203125000000000000000000000000000000000000000000000000000000000000");
shouldThrow("(1234.567).toFixed(101)");
shouldBeEqualToString("(1234.567).toFixed(-1)", "1230");
shouldBeEqualToString("(1234.567).toFixed(-4)", "0");
shouldBeEqualToString("(1234.567).toFixed(-5)", "0");
shouldBeEqualToString("(1234.567).toFixed(-20)", "0");
shouldThrow("(1234.567).toFixed(-21)");

shouldThrow("(1234.567).toFixed(posInf)");
shouldThrow("(1234.567).toFixed(negInf)");

shouldBeEqualToString("posInf.toFixed()", "Infinity");
shouldBeEqualToString("negInf.toFixed()", "-Infinity");
shouldBeEqualToString("nan.toFixed()", "NaN");

var successfullyParsed = true;
