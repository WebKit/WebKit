function shouldBe(actual, expected) {
    if (actual !== expected) {
        throw new Error(`Actual: ${actual}, Expected: ${expected}`);
    }
}

var log = [];
function LoggingProxy() {
    return new Proxy(
        {},
        {
            defineProperty: (t, key, desc) => {
                log.push(key);
                return true;
            },
        }
    );
}

var keys = [1, "before", Symbol(), "during", Symbol.for("during"), Symbol.iterator, 2, "after", 3, true, {}, false];
var expectedValues = [
    "1",
    "2",
    "3",
    "before",
    "during",
    "after",
    "true",
    "[object Object]",
    "false",
    "Symbol()",
    "Symbol(during)",
    "Symbol(Symbol.iterator)",
];
var expectedTypes = [
    "string",
    "string",
    "string",
    "string",
    "string",
    "string",
    "string",
    "string",
    "string",
    "symbol",
    "symbol",
    "symbol",
];

var descs = {};
for (var k of keys) {
    descs[k] = { configurable: true, value: 0 };
}

function test(descsObj) {
    log = [];
    Object.defineProperties(LoggingProxy(), descs);
    shouldBe(log.length, keys.length);
    for (let i = 0; i < keys.length; ++i) {
        shouldBe(log[i].toString(), expectedValues[i]);
        shouldBe(typeof log[i], expectedTypes[i]);
    }
}

for (let i = 0; i < testLoopCount; ++i) {
    test(descs);
    test(new Proxy(descs, {}));
}
