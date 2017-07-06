function assert(b) {
    if (!b)
        throw new Error("Bad");
}

function test(f) {
    noInline(f);
    for (let i = 0; i < 1000; ++i)
        f();
}

test(function() {
    let o = {xx: 0};
    for (let i in o) {
        for (i in [0, 1, 2]) { }
        assert(typeof i === "string");
        assert(o[i] === undefined);
    }
});

test(function() {
    let o = {xx: 0};
    for (let i in o) {
        for (var i of [0]) { }
        assert(typeof i === "number");
        assert(o[i] === undefined);
    }
});

test(function() {
    let o = {xx: 0};
    for (let i in o) {
        for ({i} of [{i: 0}]) { }
        assert(typeof i === "number");
        assert(o[i] === undefined);
    }
});

test(function() {
    let o = {xx: 0};
    for (let i in o) {
        ;({i} = {i: 0});
        assert(typeof i === "number");
        assert(o[i] === undefined);
    }
});

test(function() {
    let o = {xx: 0};
    for (let i in o) {
        ;([i] = [0]);
        assert(typeof i === "number");
        assert(o[i] === undefined);
    }
});

test(function() {
    let o = {xx: 0};
    for (let i in o) {
        ;({...i} = {a:20, b:30});
        assert(typeof i === "object");
        assert(o[i] === undefined);
    }
});

test(function() {
    let o = {xx: 0};
    for (let i in o) {
        eval("i = 0;");
        assert(typeof i === "number");
        assert(o[i] === undefined);
    }
});

test(function() {
    let o = {xx: 0};
    for (let i in o) {
        var i = 0;
        assert(typeof i === "number");
        assert(o[i] === undefined);
    }
});
