// Calls

foo();
foo(1);
foo(1, 2);
foo(1, 2, );
foo(1, 2, 3);
foo(foo(1));
foo(foo(1), foo(2, 3));
foo(...spread);
foo(x, ...spread);
foo(x, ...foo());

foo({
    a: 1,
    b: 2,
    c: 3
});

foo(1, 2, 3, function(e) {
    e.foo()
});

foo(function(e) {
    e.foo()
}, function(e) {
    e.bar()
});

// Basic Functions

function f() {}
(function() {});
(function() {})();
(function f() {});
(function f() {})();

function f() {
    1
}
(function() {
    1
});
(function() {
    1
})();
(function f() {
    1
});
(function f() {
    1
})();

// Parameters

function foo(a, b, c) {
    1
}

function foo(a=1, b, c) {
    1
}

function foo(a=1, b=2, c=3) {
    1
}

function foo(a, ...rest) {
    1
}

function foo(a=1, ...rest) {
    1
}

function foo({a, b}) {
    1
}

function foo([a, b]) {
    1
}

function foo([a, b, ]) {
    1
}

// Methods

o = {
    prop: function() {},
    method() {},
    get getter() {},
    set setter(x) {},
    *gen1() {},
    *gen2() {},
    async a() {}
}

o = {
    prop: function() {
        1
    },
    method() {
        1
    },
    get getter() {
        1
    },
    set setter(x) {
        1
    },
    *gen1() {
        1
    },
    *gen2() {
        1
    },
    async a() {
        1
    }
}
// Multiple / Nested

function f() {}
function f() {}

function outer() {
    1;
    function inner() {
        2
    }
    3
}

// Async / Await

async function foo() {}
async function foo() {
    1
}
async function foo() {
    await foo()
}
o = {
    async async() {},
    async() {},
    async: 1
}
o = {
    async 1() {}
}
o = {
    async "foo"() {}
}
o = {
    async ["foo"]() {}
}
o = {
    foo: async function() {}
}
// Commas

function foo() {
    a(),
    b(),
    c()
}

// Return

function foo() {
    return
}
function foo() {
    return 42
}
function foo() {
    return 42, a(), b()
}
