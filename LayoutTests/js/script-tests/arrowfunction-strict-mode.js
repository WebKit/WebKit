"use strict";

description('Tests for ES6 arrow function, make sure parsing is OK in strict mode.');

var f1 = x => x;
shouldBe("f1(10)", "10");

var f2 = (x) => x;
shouldBe("f2(20)", "20");

var f3 = (x, y) => x + y;
shouldBe("f3(10, 20)", "30");

var f4 = (x, y) => { return x + y; };
shouldBe("f4(20, 20)", "40");

function foo(f) {
    return f(10);
}

shouldBe("foo(x => x + 1)", "11");
shouldBe("foo((x) => x + 1)", "11");
shouldBe("foo(x => { return x + 1; })", "11");
shouldBe("foo((x) => { return x + 1; })", "11");
