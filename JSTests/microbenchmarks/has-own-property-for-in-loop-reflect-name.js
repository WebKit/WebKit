function assert(b) {
    if (!b)
        throw new Error;
}

function test1() {
    function count(Reflect) {
        let c = 0;
        for (let p in Reflect) {
            if (Reflect.hasOwnProperty(p))
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

    for (let i = 0; i < 600000; ++i) {
        assert(count(o) === 4);
    }
}
test1();
