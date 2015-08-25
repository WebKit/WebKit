// Inspired by mozilla tests
description('Tests for ES6 arrow function toString() method');

debug('var simpleArrowFunction = () => {}');
var simpleArrowFunction = () => {};
shouldBe("simpleArrowFunction.toString()", "'() => {}'");
shouldBe("((x) => { x + 1 }).toString()", "'(x) => { x + 1 }'");
shouldBe("(x => x + 1).toString()", "'x => x + 1'");

debug('var f0 = x => x');
var f0 = x => x;
shouldBe("f0.toString()", "'x => x'");

debug('var f1 = () => this');
var f1 = () => this;
shouldBe("f1.toString()", "'() => this'");

debug('var f2 = x => { return x; };');
var f2 = (x) => { return x; };
shouldBe("f2.toString()", "'(x) => { return x; }'");

debug('var f3 = (x, y) => { return x + y; };');
var f3 = (x, y) => { return x + y; };
shouldBe("f3.toString()", "'(x, y) => { return x + y; }'");

function foo(x) { return x.toString()};
debug('function foo(x) { return x.toString()};');
shouldBe("foo((x)=>x)", "'(x)=>x'");

var a = z => z*2, b = () => ({});
debug('var a = z => z*2, b = () => ({});');
shouldBe("a.toString()", "'z => z*2'");
shouldBe("b.toString()", "'() => ({})'");

var arrExpr = [y=>y + 1, x=>x];
debug('var arrExpr = [y=>y + 1, x=>x];');
shouldBe("arrExpr[0].toString()", "'y=>y + 1'");
shouldBe("arrExpr[1].toString()", "'x=>x'");

var arrBody  = [y=>{ y + 1 }, x=>{ x }];
debug('var arrBody  = [y=>{ y + 1 }, x=>{ x }];');
shouldBe("arrBody[0].toString()", "'y=>{ y + 1 }'");
shouldBe("arrBody[1].toString()", "'x=>{ x }'");

var successfullyParsed = true;
