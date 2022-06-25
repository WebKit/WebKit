description(
'Test RegExp.compile method.'
);

re = new RegExp("a", "i");
shouldBe("re.toString()", "'/a/i'");

re.compile("a");
shouldBe("re.multiline", "false");
shouldBe("re.ignoreCase", "false");
shouldBe("re.global", "false");
shouldBe("re.test('A')", "false");
shouldBe("re.toString()", "'/a/'");

re.compile("b", "g");
shouldBe("re.toString()", "'/b/g'");

re.compile(new RegExp("c"));
shouldBe("re.toString()", "'/c/'");

re.compile(new RegExp("c", "i"));
shouldBe("re.ignoreCase", "true");
shouldBe("re.test('C')", "true");
shouldBe("re.toString()", "'/c/i'");

shouldThrow("re.compile(new RegExp('c'), 'i');");

// It's OK to supply a second argument, as long as the argument is "undefined".
re.compile(re, undefined);
shouldBe("re.toString()", "'/c/i'");

shouldThrow("re.compile(new RegExp('+'));");

re.compile();
shouldBe("re.toString()", "'/(?:)/'");
re.compile(undefined);
shouldBe("re.toString()", "'/(?:)/'");
re.compile("");
shouldBe("re.toString()", "'/(?:)/'");

re.compile(null);
shouldBe("re.toString()", "'/null/'");

re.compile("z", undefined);
shouldBe("re.toString()", "'/z/'");

// Compiling should reset lastIndex.
re.lastIndex = 100;
re.compile(/a/g);
shouldBe("re.lastIndex", "0");
re.exec("aaa");
shouldBe("re.lastIndex", "1");

// Compile returns the regexp itself.
shouldBe("regexpWithUndefinedCompiledToValid = new RegExp(undefined), regexpWithUndefinedCompiledToValid.compile('abc')", "regexpWithUndefinedCompiledToValid");
shouldBe("regexpValidPatternCompiledToValid = new RegExp('zyx'), regexpValidPatternCompiledToValid.compile('abc')", "regexpValidPatternCompiledToValid");
shouldBe("regexpWithValidCompiledToUndefined = new RegExp('abc'), regexpWithValidCompiledToUndefined.compile(undefined)", "regexpWithValidCompiledToUndefined");
