function assert(b) {
    if (!b)
        throw new Error("Bad assertion.");
}

function test(f, ...args) {
    for (let i = 0; i < 500; i++)
        f(...args);
}

function falsey() { return false; }
noInline(falsey);
function truthy() { return true; }
noInline(truthy);

test(function() {
    var a;
    assert(a === undefined);
    {
        function a() { return 20; }
    }
    assert(a() === 20);
});

test(function(a) {
    var a;
    assert(a === undefined);
    {
        function a() { return 20; }
    }
    assert(a === undefined);
});

test(function({a}) {
    var a;
    assert(a === undefined);
    {
        function a() { return 20; }
    }
    assert(a === undefined);
}, {});

test(function() {
    let a;
    assert(a === undefined);
    {
        function a() { return 20; }
    }
    assert(a === undefined);
});

test(function() {
    assert(a === undefined);
    function foo() { return a(); }
    {
        function a() { return 20; }
    }
    assert(a() === 20);
    assert(foo() === 20);
});

test(function(a = 30) {
    assert(a === 30);
    function foo() { return a; }
    assert(foo() === 30);
    {
        function a() { return 20; }
        assert(a() === 20);
        assert(foo() === 30);
    }
    assert(a === 30);
    assert(foo() === 30);
});

test(function() {
    let x = 15;
    assert(x === 15);
    assert(a === undefined);
    {
        let x = {x: 20};
        function a() { return x; }
        assert(a() === x);
        assert(a().x === 20);
    }
    assert(a().x === 20);
    assert(x === 15);
});

test(function() {
    let x = 15;
    assert(x === 15);
    assert(a === undefined);
    let f;
    {
        let x = {x: 20};
        assert(a() === x);
        assert(a().x === 20);

        function a() { throw new Error; }
        function a() { return x; }
        f = a;
    }
    assert(a().x === 20);
    assert(x === 15);
    assert(f().x === 20);
});

test(function() {
    let x = 15;
    let f;
    assert(x === 15);
    assert(a === undefined);
    assert(f === undefined);
    {
        function a() { return f; }
        f = a;
    }
    assert(x === 15);
    assert(f() === f);
});

test(function() {
    function a() { return 20; }
    let f = a;
    assert(a() === 20);
    {
        function a() { return 25; }
        assert(a() === 25);
    }
    assert(f() === 20);
    assert(a() === 25);
});

test(function() {
    assert(f === undefined);
    for (let i = 0; i < 10; i++) {
        function f() { return i; }
        assert(f() === i);
    }
    assert(f() === 9);
});

test(function() {
    assert(f === undefined);
    let nums = [0, 1, 2, 3];
    for (let i of nums) {
        function f() { return i; }
        assert(f() === i);
    }
    assert(f() === 3);
});

test(function() {
    assert(f === undefined);
    let obj = {0:0, 1:1, 2:2, 3:3};
    for (let i in obj) {
        function f() { return i; }
        assert(f() === i);
    }
    assert(f() === "3");
});

test(function() {
    assert(f === undefined);
    let nums = [0, 1, 2, 3];
    let funcs = []
    for (let i of nums) {
        function f() { return i; }
        funcs.push(f);
        assert(f() === i);
    }
    assert(f() === 3);
    assert(funcs.length === nums.length);
    for (let i = 0; i < funcs.length; i++) {
        assert(funcs[i]() === nums[i]);
    }
});

test(function() {
    assert(f === undefined);
    try {
        throw new Error("foo");
    } catch(e) {
        function f() { return 20; }
    }
    assert(f() === 20);
});

test(function() {
    assert(f === undefined);
    try {
        ;
    } catch(e) {
        function f() { return 20; }
    }
    assert(f === undefined);
});

test(function() {
    assert(foo === undefined);
    if (falsey()) {
        function foo() { return 20; }
    }
    assert(foo === undefined);
});

test(function() {
    assert(foo === undefined);
    if (falsey())
        function foo() { return 20; }
    assert(foo === undefined);
});

test(function() {
    assert(foo === undefined);
    if (truthy()) {
        function foo() { return 20; }
    }
    assert(foo() === 20);
});

test(function() {
    assert(foo === undefined);
    while (truthy()) {
        break;

        function foo() { return 20; }
    }
    assert(foo() === 20);
});

test(function() {
    function bar() { return foo; }
    assert(foo === undefined);
    assert(bar() === undefined);
    while (truthy()) {
        break;

        function foo() { return 20; }
    }
    assert(foo() === 20);
    assert(bar()() === 20);
});

test(function() {
    function bar() { return foo; }
    assert(foo === undefined);
    assert(bar() === undefined);
    while (falsey()) {
        function foo() { return 20; }
    }
    assert(foo === undefined);
    assert(bar() === undefined);
});

test(function() {
    var a = "a";
    assert(a === "a");
    {
        let b = 1;
        assert(a === "a");
        {
            let c = 2;
            assert(a === "a");
            {
                let d = 3;
                function a() { return b + c+ d; }
                assert(a() === 6);
            }
            assert(a() === 6);
        }
        assert(a() === 6);
    }
    assert(a() === 6);
});

test(function() {
    assert(foo === undefined);
    switch(1) {
    case 0:
        function foo() { return 20; }
        break;
    }
    assert(foo() === 20);
});

test(function() {
    assert(foo === undefined);
    switch(1) {
    case 0:{
        function foo() { return 20; }
        break;
    }
    }
    assert(foo === undefined);
});

test(function() {
    assert(foo === undefined);
    switch(0) {
    case 0:{
        function foo() { return 20; }
        break;
    }
    }
    assert(foo() === 20);
});

test(function() {
    assert(foo === undefined);
    switch(0) {
    case 0:
        function foo() { return bar; }
        break;
    case 1:
        let bar = 20;
        break;
    }

    let threw = false;
    try {
        foo();
    } catch (e) {
        assert(e instanceof ReferenceError);
        threw = true;
    }
    assert(threw);
});

test(function() {
    assert(foo === undefined);
    switch(0) {
    case 0:
        function foo() { return bar; }
    case 1:
        let bar = 20;
        break;
    }

    assert(foo() === 20);
});

test(function() {
    assert(foo === undefined);
    switch(1) {
    case 0:
        function foo() { return bar; }
    case 1:
        let bar = 20;
        assert(foo() === 20);
        break;
    }

    assert(foo() === 20);
});

test(function(a) {
    assert(a === 25);
    switch(1) {
    case 0:
        function a() { return bar; }
    case 1:
        let bar = 20;
        assert(a() === 20);
        break;
    }

    assert(a === 25);
}, 25);

test(function() {
    let a = 25;
    assert(a === 25);
    switch(1) {
    case 0:
        function a() { return bar; }
    case 1:
        let bar = 20;
        assert(a() === 20);
        break;
    }

    assert(a === 25);
});

test(function() {
    const a = 25;
    assert(a === 25);
    switch(1) {
    case 0:
        function a() { return bar; }
    case 1:
        let bar = 20;
        assert(a() === 20);
        break;
    }

    assert(a === 25);
});

test(function() {
    let foo = {};
    class a { constructor() { return foo; } }
    assert(new a === foo);
    switch(1) {
    case 0:
        function a() { return bar; }
    case 1:
        let bar = 20;
        assert(a() === 20);
        break;
    }

    assert(new a === foo);
});

test(function() {
    assert(f === undefined);
    {
        if (true)
            function f() { return 20; }
        assert(f() === 20);
    }
    assert(f() === 20);
});

test(function() {
    assert(f === undefined);
    {
        if (false)
            function f() { return 20; }
        assert(f === undefined);
    }
    assert(f === undefined);
});

test(function() {
    var x;
    assert(f === undefined);
    if (true)
        if (true)
            if (true)
                function f() { return 20; }
    assert(f() === 20);
});

test(function() {
    var x;
    assert(f === undefined);
    {
        if (true)
            if (false)
                if (true)
                    function f() { return 20; }
    }
    assert(f === undefined);
});

test(function() {
    var x;
    assert(f === undefined);
    {
        while (false)
            while (false)
                if (true)
                    function f() { return 20; }
    }
    assert(f === undefined);
});

test(function() {
    assert(f === undefined);
    var f = 20;
    assert(f === 20);
    while (false)
        while (false)
            if (true)
                function f() { return 20; }
    assert(f === 20);
});

test(function() {
    assert(f === undefined);
    var f = 20;
    assert(f === 20);
    var i = 2;
    {
        while (i-- > 0)
            while (i-- > 0)
                if (true)
                    function f() { return 20; }
    }
    assert(f() === 20);
});

test(function() {
    assert(f === undefined);
    var f = 20;
    assert(f === 20);
    var i = 2;
    {
        while (i-- > 0)
            while (i-- > 0)
                if (false)
                    function f() { return 20; }
    }
    assert(f === 20);
});

test(function() {
    assert(f === undefined);
    var f = 20;
    assert(f === 20);
    var i = 2;
    {
        while (i-- > 0)
            while (i-- > 0)
                if (false)
                    function f() { return 20; }
                else
                    function f() { return 30; }
    }
    assert(f() === 30);
});

test(function() {
    assert(f === undefined);
    if (true) {
        label: function f() { return 20; }
    }
    assert(f() === 20);
});

test(function() {
    assert(f === undefined);
    if (true) {
        label: label2: label3: function f() { return 20; }
    }
    assert(f() === 20);
});

test(function() {
    assert(a === undefined);
    assert(b === undefined);
    assert(c === undefined);
    assert(d === undefined);
    assert(e === undefined);
    assert(f === undefined);
    assert(g === undefined);
    assert(h === undefined);
    assert(i === undefined);
    assert(j === undefined);
    assert(k === undefined);
    assert(l === undefined);
    assert(m === undefined);
    assert(n === undefined);
    assert(o === undefined);
    assert(p === undefined);
    assert(q === undefined);
    assert(r === undefined);
    assert(s === undefined);
    assert(t === undefined);
    assert(u === undefined);
    assert(v === undefined);
    assert(w === undefined);
    assert(x === undefined);
    assert(y === undefined);
    assert(z === undefined);
    {
        function a() { } 
        function b() { } 
        function c() { } 
        function d() { } 
        function e() { } 
        function f() { } 
        function g() { } 
        function h() { } 
        function i() { } 
        function j() { } 
        function k() { } 
        function l() { } 
        function m() { } 
        function n() { } 
        function o() { } 
        function p() { } 
        function q() { } 
        function r() { } 
        function s() { } 
        function t() { } 
        function u() { } 
        function v() { } 
        function w() { } 
        function x() { } 
        function y() { } 
        function z() { } 
    }
    assert(typeof a === "function");
    assert(typeof b === "function");
    assert(typeof c === "function");
    assert(typeof d === "function");
    assert(typeof e === "function");
    assert(typeof f === "function");
    assert(typeof g === "function");
    assert(typeof h === "function");
    assert(typeof i === "function");
    assert(typeof j === "function");
    assert(typeof k === "function");
    assert(typeof l === "function");
    assert(typeof m === "function");
    assert(typeof n === "function");
    assert(typeof o === "function");
    assert(typeof p === "function");
    assert(typeof q === "function");
    assert(typeof r === "function");
    assert(typeof s === "function");
    assert(typeof t === "function");
    assert(typeof u === "function");
    assert(typeof v === "function");
    assert(typeof w === "function");
    assert(typeof x === "function");
    assert(typeof y === "function");
    assert(typeof z === "function");
});

for (let i = 0; i < 500; i++)
    assert(foo() === 25);
function foo() { return 20; }

{
    function foo() { return 25; }
    assert(foo() === 25);
}
assert(foo() === 25);

for (let i = 0; i < 500; i++)
    assert(bar() === "bar2");
function bar() { return "bar1"; }
if (falsey()) {
    {
        if (falsey()) {
            function bar() { return "bar2"; }
        }
    }
}
assert(bar() === "bar2");

for (let i = 0; i < 500; i++)
    assert(baz() === "baz2");
function baz() { return "baz1"; }
while (falsey()) {
    if (falsey()) {
        function baz() { return "baz2"; }
    }
}
assert(baz() === "baz2");
