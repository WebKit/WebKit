shouldBe("true ? 1 : 2", "1");
shouldBe("false ? 1 : 2", "2");
shouldBe("'abc' ? 1 : 2", "1");
shouldBe("null ? 1 : 2", "2");
shouldBe("undefined ? 1 : 2", "2");
var a = 1;
if ( undefined )
  a = 2;
shouldBe("/*var a=1;if (undefined) a = 2;*/ a", "1");
successfullyParsed = true
