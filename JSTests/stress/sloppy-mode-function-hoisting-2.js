function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

function test(fn) {
    for (var i = 0; i < 1000; i++)
        fn();
}

test(function() {
    assert(foo === undefined);

    {
        assert(foo() === 1);
        function foo() { return 1; }
        assert(foo() === 1);
    }

    assert(foo() === 1);

    {
        let foo = 1;

        {
            assert(foo() === 2);
            function foo() { return 2; }
            assert(foo() === 2);
        }
    }

    assert(foo() === 1);
});

test(function() {
    assert(foo === undefined);

    {
        assert(foo() === 1);
        function foo() { return 1; }
        assert(foo() === 1);
    }

    assert(foo() === 1);

    {
        {{{
            assert(foo() === 2);
            function foo() { return 2; }
            assert(foo() === 2);
        }}}

        let foo = 1;
    }

    assert(foo() === 1);
});

test(function() {
    assert(foo === undefined);
    const err = new Error();

    try {
        assert(foo() === 1);
        function foo() { return 1; }

        throw err;
    } catch (foo) {
        assert(foo === err);

        {
            assert(foo() === 2);
            function foo() { return 2; }
            assert(foo() === 2);
        }

        assert(foo === err);
    }

    assert(foo() === 2);
});

test(function() {
    assert(foo === undefined);
    const err = new Error();
    err.foo = err;

    try {
        assert(foo() === 1);
        function foo() { return 1; }

        throw err;
    } catch ({foo}) {
        assert(foo === err);

        {
            assert(foo() === 2);
            function foo() { return 2; }
            assert(foo() === 2);
        }

        assert(foo === err);
    }

    assert(foo() === 1);
});

test(function() {
    assert(foo === undefined);
    const err = new Error();
    err.foo = err;

    try {
        assert(foo() === 1);
        function foo() { return 1; }

        throw err;
    } catch (foo) {
        assert(foo === err);

        {
            {{
                assert(foo() === 2);
                function foo() { return 2; }
                assert(foo() === 2);
            }}

            const foo = 1;
        }

        assert(foo === err);
    }

    assert(foo() === 1);
});

eval("if (true) { { function foo1() {} } } let foo1;");
eval("if (true) { function foo2() {} } let foo2;");
eval("{ if (true) function foo3() {} } let foo3;");

assert(typeof foo1 === "undefined");
assert(typeof foo2 === "undefined");
assert(typeof foo3 === "undefined");

test(function() {
    assert(foo === undefined);

    {
        assert(foo() === 1);
        function foo() { return 1; }
        assert(foo() === 1);

        {
            assert(foo() === 2);
            function foo() { return 2; }
            assert(foo() === 2);
        }
    }

    assert(foo() === 1);
});

try {
    eval(`try {} catch (foo) { function foo() {} }`);
    throw new Error("eval() didn't throw()!");
} catch (err) {
    assert(err.toString() === "SyntaxError: Cannot declare a function that shadows a let/const/class/function variable 'foo'.");
}

try {
    eval(`"use strict"; { function foo() {} function foo() {} }`);
    throw new Error("eval() didn't throw()!");
} catch (err) {
    assert(err.toString() === "SyntaxError: Cannot declare a function that shadows a let/const/class/function variable 'foo'.");
}
