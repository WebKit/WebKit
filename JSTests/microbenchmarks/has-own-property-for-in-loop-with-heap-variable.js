function assert(b) {
    if (!b)
        throw new Error;
}

function test1() {
    let o = {
        a:20,
        b:30,
        c:40,
        d:50
    };

    function count() {
        let c = 0;
        for (let p in o) {
            if (o.hasOwnProperty(p))
                ++c;
        }
        return c;
    }
    noInline(count);

    for (let i = 0; i < 600000; ++i) {
        assert(count() === 4);
    }
}
test1();
