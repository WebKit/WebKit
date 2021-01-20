function assert(b) {
    if (!b)
        throw new Error;
}

function test1() {
    function count(o, o2) {
        let c = 0;
        for (let p in o) {
            let o = o2;
            if (o2.hasOwnProperty(p))
                ++c;
        }
        return c;
    }
    noInline(count);

    let o = {
        a:20,
        b:30,
        c:40,
        d:50
    };

    let o2 = {
        a:20
    };

    for (let i = 0; i < 1e4; ++i) {
        assert(count(o, o2) === 1);
    }
}
test1();
