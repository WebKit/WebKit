description('Tests for various method names');

shouldBe("class A { 0.1() { return 1; } }; (new A)[0.1]()", "1");
shouldBe("class A { get() { return 2; } }; (new A).get()", "2");
shouldBe("class A { set() { return 3; } }; (new A).set()", "3");
shouldBe("class A { get get() { return 4; } }; (new A).get", "4");
shouldBe("class A { get set() { return 5; } }; (new A).set", "5");
shouldBe("setterValue = undefined; class A { set get(x) { setterValue = x; } }; (new A).get = 6; setterValue", "6");
shouldBe("setterValue = undefined; class A { set set(x) { setterValue = x; } }; (new A).set = 7; setterValue", "7");

shouldBe("class A { static 0.1() { return 101; } }; A[0.1]()", "101");
shouldBe("class A { static get() { return 102; } }; A.get()", "102");
shouldBe("class A { static set() { return 103; } }; A.set()", "103");
shouldBe("class A { static get get() { return 104; } }; A.get", "104");
shouldBe("class A { static get set() { return 105; } }; A.set", "105");
shouldBe("setterValue = undefined; class A { static set get(x) { setterValue = x; } }; A.get = 106; setterValue", "106");
shouldBe("setterValue = undefined; class A { static set set(x) { setterValue = x; } }; A.set = 107; setterValue", "107");
