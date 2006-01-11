description('This test checks a few Number.toFixed cases, including <a href="http://bugzilla.opendarwin.org/show_bug.cgi?id=5307">5307: Number.toFixed does not round 0.5 up</a>.');

shouldBe("(0).toFixed(0)", "'0'");

shouldBe("0.49.toFixed(0)", "'0'");
shouldBe("0.5.toFixed(0)", "'1'");
shouldBe("0.51.toFixed(0)", "'1'");

shouldBe("(-0.49).toFixed(0)", "'-0'");
shouldBe("(-0.5).toFixed(0)", "'-1'");
shouldBe("(-0.51).toFixed(0)", "'-1'");

var successfullyParsed = true;
