"use strict";

function assert(b) {
    if (!b)
        throw new Error("Bad assertion")
}

function test(f) {
    for (let i = 0; i < 500; i++)
        f();
}

{
    let called = false;
    function foo() {
        called = true;
        function bar() { return 25; }
        assert(bar() === 25);
        {
            function bar() { return 30; }
            assert(bar() === 30);
        }
        assert(bar() === 25);
    }
    test(foo);
    assert(called);
}

{
    let called = false;
    function foo() {
        called = true;
        assert(bar() === 25);
        {
            assert(bar() === 30);
            function bar() { return 30; }
        }
        assert(bar() === 25);

        function bar() { return 25; }
    }
    test(foo);
    assert(called);
}

{
    let called = false;
    function foo() {
        called = true;
        function foo() { return bar(); }
        function bar() { return 25; }
        assert(bar() === 25);
        assert(foo() === 25);
        {
            function bar() { return 30; }
            function foo() { return 25; }
            assert(bar() === 30);
            assert(foo() === 25);
        }
        assert(bar() === 25);
        assert(foo() === 25);
    }
    test(foo);
    assert(called);
}

{
    let called = false;
    function foo() {
        called = true;
        assert(bar() === 25);
        assert(foo() === 25);
        {
            function bar() { return 30; }
            function foo() { return 25; }
            assert(bar() === 30);
            assert(foo() === 25);
        }
        assert(bar() === 25);
        assert(foo() === 25);

        function foo() { return bar(); }
        function bar() { return 25; }
    }
    test(foo);
    assert(called);
}

{
    let called = false;
    function foo() {
        called = true;
        let isDefault = false;
        switch ('foo') {
        case 1:
            function foo() { return 25; }
            break;
        case 2:
            function bar() { return 30; }
            break;
        default:
            isDefault = true;
            assert(foo() === 25);
            assert(bar() === 30);
            break;
        }
        assert(isDefault);
    }
    test(foo);
    assert(called);
}

{
    let called = false;
    function foo() {
        called = true;
        let is1 = false;
        switch (1) {
        case 1:
            is1 = true;
            function foo() { return 25; }
            assert(foo() === 25);
            assert(bar() === 30);
            break;
        case 2:
            function bar() { return 30; }
            break;
        }
        assert(is1);
    }
    test(foo);
    assert(called);
}

{
    let called = false;
    function foo() {
        called = true;
        function foo() { return 25; }
        function bar() { return "bar"; }
        let is2 = false;
        switch (2) {
        case 1: {
            function foo() { return 30; }
            break;
        }
        case 2:
            is2 = true;
            function bar() { return 30; }
            assert(bar() === 30);
            assert(foo() === 25);
            break;
        }
        assert(is2);
        assert(bar() === "bar");
        assert(foo() === 25);
    }
    test(foo);
    assert(called);
}

{
    let called = false;
    function foo() {
        called = true;
        function foo() { return 25; }
        function bar() { return "bar"; }
        let capture = () => foo + "" + bar;
        let is2 = false;
        switch (2) {
        case 1: {
            function foo() { return 30; }
            break;
        }
        case 2:
            is2 = true;
            function bar() { return 30; }
            let capture = () => bar;
            assert(bar() === 30);
            assert(foo() === 25);
            break;
        }
        assert(is2);
        assert(bar() === "bar");
        assert(foo() === 25);
    }
    test(foo);
    assert(called);
}

{
    let called = false;
    function foo() {
        called = true;
        let f1;
        let f2 = foo;
        function foo() { }
        if (true) {
            f1 = foo;
            function foo() { }
        }
        assert(!!f1 && !!f2);
        assert(f1 !== f2);
    }
    test(foo);
    assert(called);
}

{
    let called = false;
    function foo() {
        called = true;
        let f1;
        let f2 = foo;
        function foo() { }
        let capture = () => foo;
        if (true) {
            f1 = foo;
            function foo() { }
            let capture = () => foo;
        }
        assert(!!f1 && !!f2);
        assert(f1 !== f2);
    }
    test(foo);
    assert(called);
}
