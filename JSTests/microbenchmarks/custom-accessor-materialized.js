function assert(b) {
    if (!b)
        throw new Error;
}

function test4() {
    function get(o) {
        return o.testStaticAccessor;
    }
    noInline(get);

    const proto = $vm.createStaticCustomAccessor();
    const o = {__proto__: proto};
    const d = Object.getOwnPropertyDescriptor(proto, "testStaticAccessor");
    assert(!!d.get);
    o.testField = 1337;

    for (let i = 0; i < 500000; ++i) {
        assert(get(o) === 1337);
    }
}
test4();
