function assert(b) {
    if (!b)
        throw new Error;
}

function test1() {
    function has(o, p) {
        return p in o;
    }

    function count(o) {
        let c = 0;
        for (let p in o) {
            if (has(o, p))
                c += p.length;
        }
        return c;
    }
    noInline(count);

    const keys = ['method', 'url', 'status', 'headers', 'body', 'retries', 'timeout', 'ok'];
    let srcs = [];
    for (let j = 0; j < 16; j++) {
        let o = {};
        for (let k of keys)
            o[k] = k + j;
        srcs.push(o);
    }

    let expected = 0;
    for (let k of keys)
        expected += k.length;

    for (let i = 0; i < 1000000; ++i)
        assert(count(srcs[i & 15]) === expected);
}
test1();
