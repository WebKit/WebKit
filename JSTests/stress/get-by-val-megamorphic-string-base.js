function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function get(base, key) {
    return base[key];
}
noInline(get);

function getCharAt(base) {
    return base.charAt;
}
noInline(getCharAt);

function* gen() { yield 1; }

var bases = [[1], ["a"], [1.5], [{}], new Set, new Map, gen(), { a: 1 }, { b: 1 }, { c: 1 }, { d: 1 }, { e: 1 }, { f: 1 }, { g: 1 }];
var hidden = Symbol("hidden");
var stringIterator = String.prototype[Symbol.iterator];

for (var i = 0; i < testLoopCount; ++i) {
    var base = bases[i % bases.length];
    shouldBe(get(base, Symbol.iterator), base[Symbol.iterator]);
    shouldBe(get("abc", Symbol.iterator), stringIterator);
    shouldBe(get("abc", "charAt"), String.prototype.charAt);
    shouldBe(get("abc", "hasOwnProperty"), Object.prototype.hasOwnProperty);
    shouldBe(get("abc", hidden), undefined);
    shouldBe(get(String.prototype, "charAt"), String.prototype.charAt);
    shouldBe(get(String.prototype, Symbol.iterator), stringIterator);
    shouldBe(getCharAt(base), base.charAt);
    shouldBe(getCharAt("abc"), String.prototype.charAt);
}

Object.prototype[hidden] = 42;
shouldBe(get("abc", hidden), 42);
shouldBe(get(String.prototype, hidden), 42);
shouldBe(get({ a: 1 }, hidden), 42);
delete Object.prototype[hidden];
shouldBe(get("abc", hidden), undefined);
shouldBe(get(String.prototype, hidden), undefined);

String.prototype[hidden] = "own";
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(get(String.prototype, hidden), "own");
    shouldBe(get("abc", hidden), "own");
}
String.prototype[hidden] = "replaced";
shouldBe(get("abc", hidden), "replaced");
delete String.prototype[hidden];
shouldBe(get("abc", hidden), undefined);

Object.defineProperty(String.prototype, hidden, { get() { "use strict"; return typeof this; }, configurable: true });
shouldBe(get("abc", hidden), "string");
delete String.prototype[hidden];

shouldBe(get("abc", "length"), 3);
shouldBe(get("abc", "1"), "b");
shouldBe(get("abc", 1), "b");
shouldBe(get(new String("xy"), Symbol.iterator), stringIterator);
shouldBe(get(new String("xy"), "length"), 2);

class MyString extends String { }
var myString = new MyString("hello");
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(get(myString, Symbol.iterator), stringIterator);
    shouldBe(get(myString, "length"), 5);
    shouldBe(get(myString, 0), "h");
}
