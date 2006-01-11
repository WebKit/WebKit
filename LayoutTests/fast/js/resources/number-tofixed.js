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

var successfullyParsed = true;
