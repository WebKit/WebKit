//@ requireOptions("--useDollarVM=1")

// Parsing and running code must never build a source's line-start table, only asking for a line or
// column may. The last section forces the table and checks that it flips, because otherwise a build
// whose accessor always answered false would pass every check above it.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected} but got ${actual}`);
}

function declared(a, b) { return a + b; }

const expression = function (a) { return a * 2; };

const arrow = (a) => a + 1;

class Klass {
    constructor(x) { this.x = x; }
    method() { return this.x; }
    get accessor() { return this.x + 1; }
    static staticMethod() { return 1; }
}

function* generator() { yield 1; }
async function asyncFunction() { return 1; }

// Tier up, so that the baseline, DFG and FTL paths all get exercised: each consults expression info.
// testLoopCount is set per configuration by the jsc CLI.
let sum = 0;
for (let i = 0; i < testLoopCount; ++i) {
    sum += declared(i, 1) + expression(i) + arrow(i);
    sum += new Klass(i).method() + new Klass(i).accessor;
    sum += Klass.staticMethod();
}
for (const v of generator())
    sum += v;
asyncFunction();

shouldBe(sum > 0, true, "the functions should have run");

const cases = [
    ["a function declaration", declared],
    ["a function expression", expression],
    ["an arrow function", arrow],
    ["a class method", Klass.prototype.method],
    ["a class getter", Object.getOwnPropertyDescriptor(Klass.prototype, "accessor").get],
    ["a static method", Klass.staticMethod],
    ["a generator", generator],
    ["an async function", asyncFunction],
];

for (const [name, f] of cases)
    shouldBe($vm.lineStartTableIsBuilt(f), false, `parsing and running ${name} should not build the table`);

// A function from the Function constructor gets its own provider, which neither constructing it,
// calling it, nor reading its source text may build a table for.
//
// Two constructions are needed, with the check on the first. The cache is empty on the first call so
// it validates nothing; only the second call examines a candidate, and that is what would build the
// first function's table. A single construction cannot reach that path at all.
const constructedFirst = new Function("a", "return a + 1");
shouldBe(constructedFirst(1), 2, "the first constructed function should work");
shouldBe(typeof constructedFirst.toString(), "string", "toString should work");

const constructedSecond = new Function("a", "return a + 2");
shouldBe(constructedSecond(1), 3, "the second constructed function should work");

shouldBe($vm.lineStartTableIsBuilt(constructedFirst), false,
    "a later new Function should not build the table of an earlier one while checking the cache");
shouldBe($vm.lineStartTableIsBuilt(constructedSecond), false,
    "new Function and toString should not build the table");

// Positive control: a stack trace does need a line and column, so it must build the table.
shouldBe($vm.lineStartTableIsBuilt(declared), false, "still not built, just before forcing it");
try {
    null.x;
} catch (e) {
    if (typeof e.stack !== "string")
        throw new Error("expected a stack string");
}
shouldBe($vm.lineStartTableIsBuilt(declared), true, "reading a stack trace should build the table");
