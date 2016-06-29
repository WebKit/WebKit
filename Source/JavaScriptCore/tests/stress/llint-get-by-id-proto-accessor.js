foo = {};
var expected = 6;
Object.defineProperty(Object.prototype, "bar", { get: () => { return 2 * 3; } });

function test() {
    if (foo.bar != expected) {
        throw new Error();
    }
}

for (var i = 0; i < 10; i++) {
    if (i == 9) {
        foo = {bar: 7};
        expected = 7;
    }
    test();
}

