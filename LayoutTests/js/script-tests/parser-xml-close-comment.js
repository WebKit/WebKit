description("Test to ensure correct handling of --> as a single line comment when at the beginning of a line");

shouldThrow("'should be a syntax error' -->");
shouldThrow("/**/ 1 -->");
shouldThrow("1 /**/ -->");
shouldThrow("1/*\n*/-->");

shouldBeUndefined("-->");
shouldBeUndefined("/**/-->");
shouldBeUndefined("/*\n*/-->");
