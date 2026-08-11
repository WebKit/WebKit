function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var globalObject = createGlobalObject();
var constructor = globalObject.Object;
var tests = [
    [ null, globalObject.Object ],
    [ undefined, globalObject.Object ],
    [ "Hello", globalObject.String ],
    [ 42, globalObject.Number ],
    [ false, globalObject.Boolean ],
    [ Symbol("Cocoa"), globalObject.Symbol ],
];

for (var i = 0; i < 1e4; ++i) {
    for (var [target, cls] of tests) {
        var result = constructor(target);
        shouldBe(result instanceof cls, true);
    }
}
