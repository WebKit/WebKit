function assert(b) {
    if (!b)
        throw new Error;
}

function test1() {
    function func(b, o) {
        if (b)
            return 0 in o;
        return true;
    }
    noInline(func);

    let o = {__proto__:[0, 1]};
    o[2] = 4;
    for (let i = 0; i < 100; ++i) {
        func(true, o);
        func(false, o);
    }

    for (let i = 0; i < 10000; ++i) {
        assert(func(false, o));
    }
    assert(func(true, o));
}
test1();

function test2() {
    function func(b, o) {
        if (b)
            return 0 in o;
        return true;
    }
    noInline(func);

    let o = {__proto__:[0, 1]};
    o[2] = 13.333;
    for (let i = 0; i < 100; ++i) {
        func(true, o);
        func(false, o);
    }

    for (let i = 0; i < 10000; ++i) {
        assert(func(false, o));
    }
    assert(func(true, o));
}
test2();

function test3() {
    function func(b, o) {
        if (b)
            return 0 in o;
        return true;
    }
    noInline(func);

    let o = {__proto__:[0, 1]};
    o[2] = {};
    for (let i = 0; i < 100; ++i) {
        func(true, o);
        func(false, o);
    }

    for (let i = 0; i < 10000; ++i) {
        assert(func(false, o));
    }
    assert(func(true, o));
}
test3();

function test4() {
    function func(b, o) {
        if (b)
            return 0 in o;
        return true;
    }
    noInline(func);

    let o = {__proto__:[0, 1]};
    o[2] = {};
    $vm.ensureArrayStorage(o);

    for (let i = 0; i < 100; ++i) {
        func(true, o);
        func(false, o);
    }

    for (let i = 0; i < 10000; ++i) {
        assert(func(false, o));
    }
    assert(func(true, o));
}
test4();
