function assert(b) {
    if (!b)
        throw new Error;
}

function test1() {
    function getMultiline(o) {
        return o.multiline;
    }
    noInline(getMultiline);

    RegExp.foo = 42;

    const o = {};
    o.__proto__ = RegExp;
    RegExp.multiline = false;

    for (let i = 0; i < 5000000; ++i) {
        assert(getMultiline(o) === false);
    }
    delete RegExp.foo;
    for (let i = 0; i < 5000000; ++i) {
        assert(getMultiline(o) === false);
    }
}
test1();
