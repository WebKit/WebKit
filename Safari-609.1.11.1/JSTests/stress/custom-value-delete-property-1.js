function assert(b) {
    if (!b)
        throw new Error;
}

function test1() {
    function getMultiline(o) {
        return o.multiline;
    }
    noInline(getMultiline);

    const o = {};
    o.__proto__ = RegExp;
    RegExp.multiline = false;

    for (let i = 0; i < 500; ++i) {
        assert(getMultiline(o) === false);
    }
    delete RegExp.input;
    delete RegExp.multiline;
    assert(getMultiline(o) === undefined);
}
test1();
