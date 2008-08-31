description(
"This test checks the boundary cases of substr()."
);

shouldBe("'bar'.substr(0)", "'bar'");
shouldBe("'bar'.substr(3)", "''");
shouldBe("'bar'.substr(4)", "''");
shouldBe("'bar'.substr(-1)", "'r'");
shouldBe("'bar'.substr(-3)", "'bar'");
shouldBe("'bar'.substr(-4)", "'bar'");

shouldBe("'bar'.substr(0, 0)", "''");
shouldBe("'bar'.substr(0, 1)", "'b'");
shouldBe("'bar'.substr(0, 3)", "'bar'");
shouldBe("'bar'.substr(0, 4)", "'bar'");

shouldBe("'bar'.substr(1, 0)", "''");
shouldBe("'bar'.substr(1, 1)", "'a'");
shouldBe("'bar'.substr(1, 2)", "'ar'");
shouldBe("'bar'.substr(1, 3)", "'ar'");

shouldBe("'bar'.substr(3, 0)", "''");
shouldBe("'bar'.substr(3, 1)", "''");
shouldBe("'bar'.substr(3, 3)", "''");

shouldBe("'bar'.substr(4, 0)", "''");
shouldBe("'bar'.substr(4, 1)", "''");
shouldBe("'bar'.substr(4, 3)", "''");

shouldBe("'bar'.substr(-1, 0)", "''");
shouldBe("'bar'.substr(-1, 1)", "'r'");

shouldBe("'bar'.substr(-3, 1)", "'b'");
shouldBe("'bar'.substr(-3, 3)", "'bar'");
shouldBe("'bar'.substr(-3, 4)", "'bar'");

shouldBe("'bar'.substr(-4)", "'bar'");
shouldBe("'bar'.substr(-4, 0)", "''");
shouldBe("'bar'.substr(-4, 1)", "'b'");
shouldBe("'bar'.substr(-4, 3)", "'bar'");
shouldBe("'bar'.substr(-4, 4)", "'bar'");

shouldBe("'GMAIL_IMP=bf-i%2Fd-0-0%2Ftl-v'.substr(10)", "'bf-i%2Fd-0-0%2Ftl-v'");

var successfullyParsed = true;
