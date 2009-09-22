description("Test to ensure correct behaviour of Object.keys");

shouldBe("Object.keys({})", "[]");
shouldBe("Object.keys({a:null})", "['a']");
shouldBe("Object.keys({a:null, b:null})", "['a', 'b']");
shouldBe("Object.keys({b:null, a:null})", "['b', 'a']");
shouldBe("Object.keys([])", "[]");
shouldBe("Object.keys([null])", "['0']");
shouldBe("Object.keys([null,null])", "['0','1']");
shouldBe("Object.keys([null,null,,,,null])", "['0','1','5']");
shouldBe("Object.keys({__proto__:{a:null}})", "[]");
shouldBe("Object.keys({__proto__:[1,2,3]})", "[]");
shouldBe("x=[];x.__proto__=[1,2,3];Object.keys(x)", "[]");

successfullyParsed = true;
