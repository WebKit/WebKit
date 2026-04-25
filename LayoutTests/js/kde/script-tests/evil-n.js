shouldBe("(new Error()).message", "''");

// the empty match isn't taken in account
shouldBe("''.split(/.*/).length", "0");
