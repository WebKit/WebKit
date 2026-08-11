let testSymbol = Symbol("test");
Object.defineProperty(Object.prototype, "test", {
    set() {
        throw new Error('bad');
    },
    get() { return 42; }
});
Object.defineProperty(Object.prototype, testSymbol, {
    set() {
        throw new Error('bad');
    },
    get() { return 42; }
});

function test1(prop) {
    return { [prop]: prop };
}
noInline(test1);

for (var i = 0; i < 1e4; ++i) {
    test1("test" + i);
    test1("test");
}

function test2(prop) {
    return { [prop]: prop };
}
noInline(test2);

for (var i = 0; i < 1e4; ++i) {
    test2(Symbol("test"));
    test2(testSymbol);
}
