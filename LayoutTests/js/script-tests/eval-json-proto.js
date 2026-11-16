description("This test case checks different JSON literal constructions and ensures they handle __proto__ as expected.");

shouldBeTrue("({__proto__:[]}) instanceof Array");

evalResult = eval("({__proto__:[]})");
shouldBeTrue("evalResult instanceof Array");

jsonParseResult = JSON.parse('{"__proto__":[]}');
shouldBeFalse("jsonParseResult instanceof Array");
