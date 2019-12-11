description('Tests for string and numeric method names for ES6 class syntax');

shouldBe("constructorCallCount = 0; new (class { \"constructor\"() { constructorCallCount++; } }); constructorCallCount", "1");
shouldBe("class A { 'constructor'() { constructorCallCount++; } }; new A; constructorCallCount", "2");
shouldThrow("new (class { constructor() {} \"constructor\"() {} });", "'SyntaxError: Cannot declare multiple constructors in a single class.'");
shouldThrow("new (class { constructor() {} static \"prototype\"() {} });", "'SyntaxError: Cannot declare a static method named \\'prototype\\'.'");

shouldBe("class A { 'foo'() { return 3; } }; (new A).foo()", "3");

shouldBe("class A { get 'foo'() { return 4; } }; (new A).foo", "4");
shouldBe("class A { get 'foo'() { return 4; } }; A.foo", "undefined");
shouldBe("class A { static get 'foo'() { return 5; } }; A.foo", "5");
shouldBe("class A { static get 'foo'() { return 5; } }; (new A).foo", "undefined");

shouldBe("fooValue = 0; X = class { set 'foo'(value) { fooValue = value; } }; (new X).foo = 6; fooValue", "6");
shouldBe("X.foo = 7; fooValue", "6");
shouldBe("fooValue = 0; X = class { static set 'foo'(value) { fooValue = value; } }; X.foo = 8; fooValue", "8");
shouldBe("(new X).foo = 7; fooValue", "8");

shouldBe("X = class { get 'foo'() { return 9 } set 'foo'(x) { } }; x = new X; x.foo", "9");
shouldBe("X.foo", "undefined");
shouldBe("fooValue = 0; X = class { get 'foo'() { } set 'foo'(x) { fooValue = x } }; (new X).foo = 9; fooValue", "9");
shouldBe("X.foo = 10; fooValue", "9");

shouldBe("X = class { static set 'foo'(x) { } static get 'foo'() { return 10 } }; X.foo", "10");
shouldBe("(new X).foo", "undefined");
shouldBe("fooValue = 0; X = class { static set 'foo'(x) { fooValue = x } static get 'foo'() { } }; X.foo = 11; fooValue", "11");
shouldBe("(new X).foo = 12; fooValue", "11");


shouldBe("class A { get 0() { return 20; } }; (new A)[0]", "20");
shouldBe("class A { get 0() { return 20; } }; A[0]", "undefined");
shouldBe("class A { static get 1() { return 21; } }; A[1]", "21");
shouldBe("class A { static get 1() { return 21; } }; (new A)[1]", "undefined");

shouldBe("setterValue = 0; X = class { set 2(x) { setterValue = x; } }; (new X)[2] = 22; setterValue", "22");
shouldBe("X[2] = 23; setterValue", "22");

shouldBe("setterValue = 0; X = class { static set 3(x) { setterValue = x; } }; X[3] = 23; setterValue", "23");
shouldBe("(new X)[3] = 23; setterValue", "23");

shouldBe("X = class { get 4() { return 24 } set 4(x) { } }; x = new X; x[4]", "24");
shouldBe("X[4]", "undefined");

shouldBe("setterValue = 0; X = class { get 5() { } set 5(x) { setterValue = x; } }; (new X)[5] = 25; setterValue", "25");
shouldBe("X[5] = 26; setterValue", "25");

shouldBe("X = class { static set 6(x) { } static get 6() { return 26 } }; X[6]", "26");
shouldBe("(new X)[6]", "undefined");
shouldBe("setterValue = 0; X = class { static set 7(x) { setterValue = x } static get 7() { } }; X[7] = 27; setterValue", "27");
shouldBe("(new X)[7] = 28; setterValue", "27");

shouldBe("function x() { return class { 'foo bar'() { return 29; } } }; (new (x()))['foo bar']()", "29");
shouldBe("function x() { return class { 30() { return 30; } } }; (new (x()))[30]()", "30");
