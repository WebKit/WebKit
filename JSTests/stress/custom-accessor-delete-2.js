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

    for (let i = 0; i < 500; ++i) {
        assert(get(o) === 1337);
    }

    proto.xyz = 42;

    assert(delete proto.testStaticAccessor);
    assert(get(o) === undefined);
}
test4();
