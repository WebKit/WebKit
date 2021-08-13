function assert(b) {
    if (!b)
        throw new Error;
}

function test1() {
    function count(o) {
        let c = 0;
        for (let p in o) {
            if (o.hasOwnProperty(p))
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

    for (let i = 0; i < 1000; ++i) {
        assert(count(o) === 4);
    }
}
test1();

function test2() {
    function count(o) {
        let c = 0;
        for (let p in o) {
            if (p === "a")
                delete o.a;
            if (o.hasOwnProperty(p))
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

    for (let i = 0; i < 1000; ++i) {
        assert(count(o) === 3);
    }
}
test2();

function test3() {
    function count(o) {
        let c = 0;
        for (let p in o) {
            p = "a";
            if (o.hasOwnProperty(p))
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

    for (let i = 0; i < 1000; ++i) {
        assert(count(o) === 4);
    }
}
test3();

function test4() {
    function count(o) {
        let c = 0;
        for (let p in o) {
            p = "f";
            if (o.hasOwnProperty(p))
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

    for (let i = 0; i < 1000; ++i) {
        assert(count(o) === 0);
    }
}
test4();

function test5() {
    function count(o) {
        let c = 0;
        for (let p in o) {
            delete o[p];
            if (o.hasOwnProperty(p))
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

    let p = {
        a:20,
        b:30,
        c:40,
        d:50
    };

    o.__proto__ = p;

    for (let i = 0; i < 1000; ++i) {
        assert(count(o) === 0);
    }
}
test5();

function test6() {
    function count(o) {
        let c = 0;
        for (let p in o) {
            delete o[p];
            if (o.hasOwnProperty(p))
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

    let p = { };

    o.__proto__ = p;

    for (let i = 0; i < 1000; ++i) {
        assert(count(o) === 0);
    }
}
test6();

function test7() {
    function count(o, o2) {
        let c = 0;
        for (let p in o) {
            if (o2.hasOwnProperty(p))
                ++c;
        }
        return c;
    }
    noInline(count);

    for (let i = 0; i < 1000; ++i) {
        let o = {a: 20};
        if (!!(i % 2)) {
            let ok = false;
            try {
                count(o, null);
            } catch(e) {
                assert(e instanceof TypeError);
                ok = e.toString().indexOf("null is not an object") >= 0;
            }
            assert(ok);
        } else {
            assert(count(o, {}) === 0);
        }
    }
}
test7();

function test8() {
    function count(o) {
        let c = 0;
        for (let p in o) {
            if (o.hasOwnProperty(p))
                ++c;
        }
        return c;
    }
    noInline(count);

    let hasOwnPropertyCalled = false;
    let p = new Proxy({a:20}, {
        getOwnPropertyDescriptor(...args) {
            hasOwnPropertyCalled = true;
            return Reflect.getOwnPropertyDescriptor(...args);
        }
    });

    for (let i = 0; i < 1000; ++i) {
        assert(count(p) === 1);
        assert(hasOwnPropertyCalled === true);
        hasOwnPropertyCalled = false;
    }
}
test8();

function test9() {
    function count(o) {
        let c = 0;
        for (let p in o) {
            if (o.hasOwnProperty(p))
                ++c;
        }
        return c;
    }
    noInline(count);

    let hasOwnPropertyCalled = false;
    let o = {
        hasOwnProperty(p) {
            hasOwnPropertyCalled = p === "a" || p === "hasOwnProperty";
            return true;
        },
        a: 42
    };

    for (let i = 0; i < 1000; ++i) {
        assert(count(o) === 2);
        assert(hasOwnPropertyCalled === true);
        hasOwnPropertyCalled = false;
    }
}
test9();

function test10() {
    function test(o, p) {
        return o?.hasOwnProperty(p)
    }
    noInline(test);

    let o = {
        a: 42
    };

    for (let i = 0; i < 1000; ++i) {
        assert(test(null, "a") === undefined);
        assert(test(o, "a") === true);
    }
}
test10();

function test11() {
    function test(o, p) {
        return o.hasOwnProperty?.(p);
    }
    noInline(test);

    let o = {
        a: 42,
        hasOwnProperty: undefined
    };

    for (let i = 0; i < 1000; ++i) {
        assert(test(o, "a") === undefined);
    }
}
test11();
