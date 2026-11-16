function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function f(a, b) { return a + b; };
let count = 0;
for (var i = 0; i < 10_000; i++) {
    var g = f.bind(null);
    var h = g.bind(null);
    Object.defineProperty(g, Symbol.hasInstance, {get: function() { count++; }});
    var res = {} instanceof h;
}
shouldBe(count, 10000);
