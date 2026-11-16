"use strict";

function assert(b) {
    if (!b)
        throw new Error("Bad")
}

function test(f, count = 1000) {
    noInline(f);
    for (let i = 0; i < count; ++i)
        f();
}

test(function() {
    let called = false;
    let target = {
        set prop(x)
        {
            assert(x === 20);
            called = true;
            assert(this === proxy)
        }
    }

    let proxy = new Proxy(target, {})
    proxy.prop = 20;
    assert(called);
});

test(function() {
    let called = false;
    let target = {
        get prop()
        {
            called = true;
            assert(this === proxy)
        }
    }

    let proxy = new Proxy(target, {})
    proxy.prop
    assert(called);
});

test(function() {
    let target = {
        get prop()
        {
            called = true;
            assert(this === proxy)
        }
    }
    let p1 = new Proxy(target, {});

    let called = false;
    let proxy = new Proxy(p1, {});
    proxy.prop
    assert(called);
});

test(function() {
    let t = {};
    let p1 = new Proxy(t, {
        get(target, prop, receiver) {
            called = true;
            assert(target === t);
            assert(receiver === proxy);
            assert(prop === "prop");
        }
    });

    let called = false;
    let proxy = new Proxy(p1, {});
    proxy.prop
    assert(called);
});

test(function() {
    let t = {};
    let callCount = 0;
    let handler = {
        get(target, prop, receiver) {
            if (callCount === 100)
                assert(target === t);
            ++callCount;
            assert(receiver === proxy);
            assert(prop === "prop");
            return Reflect.get(target, prop, receiver);
        }
    };
    let proxy = new Proxy(t, handler);
    for (let i = 0; i < 100; ++i)
        proxy = new Proxy(proxy, handler);
    proxy.prop
    assert(callCount === 101);
}, 10);

test(function() {
    let t = {};
    let callCount = 0;
    let handler = {
        set(target, prop, value, receiver) {
            if (callCount === 100)
                assert(target === t);
            ++callCount;
            assert(receiver === proxy);
            assert(prop === "prop");
            assert(value === 20);
            return Reflect.set(target, prop, value, receiver);
        }
    };
    let proxy = new Proxy(t, handler);
    for (let i = 0; i < 100; ++i)
        proxy = new Proxy(proxy, handler);
    proxy.prop = 20;
    assert(callCount === 101);
}, 10);
