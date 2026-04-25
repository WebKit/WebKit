function assert(b) {
    if (!b)
        throw new Error;
}

let o = {
    a:20,
    b:30,
    c:40,
    d:50
};

function test1() {
    let count = () => {
        let c = 0;
        for (let p in this) {
            if (this.hasOwnProperty(p))
                ++c;
        }
        return c;
    }
    noInline(count);

    for (let i = 0; i < 1e4; ++i) {
        assert(count() === 4);
    }
}
test1.call(o);
