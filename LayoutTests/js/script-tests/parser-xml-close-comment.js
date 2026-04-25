description("Test to ensure correct handling of --> as a single line comment when at the beginning of a line or preceeded by a multi-line comment");

shouldThrow("'should be a syntax error' -->");
shouldThrow("/**/ 1-->");
shouldThrow("/**/ 1 -->");
shouldThrow("1 /**/-->");
shouldThrow("1 /**/ -->");

shouldBe("1/*\n*/-->", `1`);
shouldBe("1/*\n*/\n-->", `1`);
shouldBe("2/*\n*/ -->", `2`);
shouldBe("2/*\n*/\n -->", `2`);

shouldBeUndefined("-->");
shouldBeUndefined(" -->");
shouldBeUndefined("/**/-->");
shouldBeUndefined("/*\n*/-->");
