description(
    'This test checks a few Number.toPrecision cases, including ' +
    '<a href="http://bugs.webkit.org/show_bug.cgi?id=15145">15145: (0.999).toPrecision(1) returns incorrect result</a>' +
    '.');

shouldBe("(0.999).toPrecision(1)", "'1'");
shouldBe("(0.999).toPrecision(2)", "'1.0'");
shouldBe("(0.999).toPrecision(3)", "'0.999'");


var successfullyParsed = true;
