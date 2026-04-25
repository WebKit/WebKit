function assert(b, m = "") {
    if (!b)
        throw new Error("Bad assertion: " + m);
}
noInline(assert);

const iterations = 20000;

function test1() {
    function bar(a, b, ...args) {
        return args.length;
    }
    noInline(bar);
    for (let i = 0; i < iterations; i++) {
        assert(bar() === 0, bar());
        assert(bar(i) === 0);
        assert(bar(i, i) === 0);
        assert(bar(i, i, i) === 1);
        assert(bar(i, i, i, i, i) === 3);
    }
}

function shallowEq(a, b) {
    if (a.length !== b.length)
        return false;
    for (let i = 0; i < a.length; i++) {
        if (a.length !== b.length)
            return false;
    }
    return true;
}
noInline(shallowEq);

function test2() {
    function jaz(a, b, ...args) {
        let result = [];
        for (let i = 0; i < args.length; i++) {
            result.push(args[i]);
        }
        return result;
    }
    noInline(jaz);

    function jaz2(...args) {
        function kaz(a, b, ...args) {
            let result = [];
            for (let i = 0; i < args.length; i++) {
                result.push(args[i]);
            }
            return result;
        }
        return kaz.apply(null, args);
    }
    noInline(jaz2);

    for (let f of [jaz, jaz2]) {
        for (let i = 0; i < iterations; i++) {
            let r = f();
            assert(!r.length);

            r = f(i);
            assert(!r.length);

            r = f(i, i)
            assert(!r.length);

            r = f(i, i, i)
            assert(r.length === 1);
            assert(shallowEq(r, [i]));

            r = f(i, i, i)
            assert(r.length === 1);
            assert(shallowEq(r, [i]));

            r = f(i, i, i, i*2, i*4)
            assert(r.length === 3);
            assert(shallowEq(r, [i, i*2, i*4]));
        }
    }
}

function test3() {
    function foo(...args) {
        return args;
    }
    function baz(a, b, c, ...args) {
        return foo.apply(null, args);
    }
    function jaz(a, b, c, d, e, f) {
        return baz(a, b, c, d, e, f);
    }
    noInline(jaz);

    for (let i = 0; i < iterations; i++) {
        let r = jaz();
        assert(r.length === 3);
        assert(shallowEq(r, [undefined, undefined, undefined]));

        r = jaz(i, i);
        assert(r.length === 3);
        assert(shallowEq(r, [undefined, undefined, undefined]));

        r = jaz(i, i, i);
        assert(r.length === 3);
        assert(shallowEq(r, [undefined, undefined, undefined]));

        r = jaz(i, i, i, i);
        assert(r.length === 3);
        assert(shallowEq(r, [i, undefined, undefined]));

        r = jaz(i, i, i, i, i, i);
        assert(r.length === 3);
        assert(shallowEq(r, [i, i, i]));
    }
}

function test4() {
    function baz(...args) {
        return args;
    }
    function jaz(a, b, ...args) {
        return baz.apply(null, args);
    }
    noInline(jaz);

    for (let i = 0; i < iterations; i++) {
        let r = jaz();
        assert(r.length === 0);

        r = jaz(i);
        assert(r.length === 0);

        r = jaz(i, i);
        assert(r.length === 0);

        r = jaz(i, i, i);
        assert(r.length === 1);
        assert(shallowEq(r, [i]));

        r = jaz(i, i, i, i*10);
        assert(r.length === 2);
        assert(shallowEq(r, [i, i*10]));

        let o = {};
        r = jaz(i, i, i, i*10, o);
        assert(r.length === 3);
        assert(shallowEq(r, [i, i*10, o]));
    }
}

function test5() {
    function baz(...args) {
        return args;
    }
    noInline(baz);
    function jaz(a, b, ...args) {
        return baz.apply(null, args);
    }
    noInline(jaz);

    for (let i = 0; i < iterations; i++) {
        let r = jaz();
        assert(r.length === 0);

        r = jaz(i);
        assert(r.length === 0);

        r = jaz(i, i);
        assert(r.length === 0);

        r = jaz(i, i, i);
        assert(r.length === 1);
        assert(shallowEq(r, [i]));

        r = jaz(i, i, i, i*10);
        assert(r.length === 2);
        assert(shallowEq(r, [i, i*10]));

        let o = {};
        r = jaz(i, i, i, i*10, o);
        assert(r.length === 3);
        assert(shallowEq(r, [i, i*10, o]));
    }
}

function test6() {
    "use strict";
    function baz(...args) {
        return args;
    }
    noInline(baz);
    function jaz(a, b, ...args) {
        return baz.apply(null, args);
    }
    noInline(jaz);

    for (let i = 0; i < iterations; i++) {
        let r = jaz();
        assert(r.length === 0);

        r = jaz(i);
        assert(r.length === 0);

        r = jaz(i, i);
        assert(r.length === 0);

        r = jaz(i, i, i);
        assert(r.length === 1);
        assert(shallowEq(r, [i]));

        r = jaz(i, i, i, i*10);
        assert(r.length === 2);
        assert(shallowEq(r, [i, i*10]));

        let o = {};
        r = jaz(i, i, i, i*10, o);
        assert(r.length === 3);
        assert(shallowEq(r, [i, i*10, o]));
    }
}

function test7() {
    let shouldExit = false;
    function baz(...args) {
        if (shouldExit) {
            OSRExit();
            return [args.length, args[0], args[1], args[2]];
        }
        return [args.length, args[0], args[1], args[2]];
    }
    function jaz(a, b, ...args) {
        return baz.apply(null, args);
    }
    noInline(jaz);

    function check(i) {
        let [length, a, b, c] = jaz();
        assert(length === 0);
        assert(a === undefined);
        assert(b === undefined);
        assert(c === undefined);

        [length, a, b, c] = jaz(i);
        assert(length === 0);
        assert(a === undefined);
        assert(b === undefined);
        assert(c === undefined);

        [length, a, b, c] = jaz(i, i);
        assert(length === 0);
        assert(a === undefined);
        assert(b === undefined);
        assert(c === undefined);

        [length, a, b, c] = jaz(i, i, i);
        assert(length === 1);
        assert(a === i, JSON.stringify(a));
        assert(b === undefined);
        assert(c === undefined);

        [length, a, b, c] = jaz(i, i, i, i*10);
        assert(length === 2);
        assert(a === i);
        assert(b === i*10);
        assert(c === undefined);

        let o = {oooo:20};
        [length, a, b, c] = jaz(i, i, i, i*10, o);
        assert(length === 3);
        assert(a === i);
        assert(b === i*10);
        assert(c === o);
    }

    shouldExit = true;
    for (let i = 0; i < 400; i++) {
        check(i);
    }

    shouldExit = false;
    for (let i = 0; i < iterations; i++) {
        check(i);
    }

    shouldExit = false;
    check(10);
}

function test8() {
    let shouldExit = false;
    function baz(...args) {
        if (shouldExit) {
            OSRExit();
            return args;
        }
        return args;
    }
    function jaz(a, b, ...args) {
        return baz.apply(null, args);
    }
    noInline(jaz);

    function check(i) {
        let args = jaz();
        assert(args.length === 0);

        args = jaz(i);
        assert(args.length === 0);

        args = jaz(i, i);
        assert(args.length === 0);

        args = jaz(i, i, i);
        assert(args.length === 1);
        assert(args[0] === i);

        args = jaz(i, i, i, i*10);
        assert(args.length === 2);
        assert(args[0] === i);
        assert(args[1] === i*10);

        let o = {oooo:20};
        args = jaz(i, i, i, i*10, o);
        assert(args.length === 3);
        assert(args[0] === i);
        assert(args[1] === i*10);
        assert(args[2] === o);
    }

    shouldExit = true;
    for (let i = 0; i < 400; i++) {
        check(i);
    }

    shouldExit = false;
    for (let i = 0; i < iterations; i++) {
        check(i);
    }

    shouldExit = false;
    check(10);
}

function test9() {
    let shouldExit = false;
    function baz(a, ...args) {
        if (shouldExit) {
            OSRExit();
            return [args.length, args[0], args[1]];
        }
        return [args.length, args[0], args[1]];
    }
    function jaz(...args) {
        return baz.apply(null, args);
    }
    noInline(jaz);

    function check(i) {
        let [length, a, b] = jaz();
        assert(length === 0);

        [length, a, b] = jaz(i);
        assert(length === 0);
        assert(a === undefined);
        assert(b === undefined);

        [length, a, b] = jaz(i, i + 1);
        assert(length === 1);
        assert(a === i+1);
        assert(b === undefined);

        [length, a, b] = jaz(i, i+1, i+2);
        assert(length === 2);
        assert(a === i+1);
        assert(b === i+2);

        let o = {oooo:20};
        [length, a, b] = jaz(i, i+1, o);
        assert(length === 2);
        assert(a === i+1);
        assert(b === o);
    }

    shouldExit = true;
    for (let i = 0; i < 400; i++) {
        check(i);
    }

    shouldExit = false;
    for (let i = 0; i < iterations; i++) {
        check(i);
    }

    shouldExit = false;
    check(10);
}

function test10() {
    function baz(a, b, c, ...args) {
        return [args.length, args[0], args[1], args[2], args[3]];
    }
    function jaz(a, b, c, d, e, f) {
        return baz(a, b, c, d, e, f);
    }
    noInline(jaz);

    for (let i = 0; i < iterations; i++) {
        let [length, a, b, c, d] = jaz(1, 2, 3, 4, 5, 6);
        assert(length === 3);
        assert(a === 4);
        assert(b === 5);
        assert(c === 6);
        assert(d === undefined);
    }
}

function test11() {
    function bar(...args) {
        return args;
    }
    noInline(bar);

    function foo(a, b, c, d, ...args) {
        return bar.apply(null, args);
    }
    noInline(foo);

    function makeArguments(args) {
        return [1,2,3,4, ...args];
    }
    for (let i = 0; i < iterations; i++) {
        function test() { assert(shallowEq(a, foo.apply(null, makeArguments(a)))); }
        let a = [{}, 25, 50];
        test();

        a = [];
        test();

        a = [{foo: 20}];
        test();

        a = [10, 20, 30, 40, 50, 60, 70, 80];
        test();
    }
}

function test12() {
    "use strict";
    let thisValue = {};
    function getThisValue() { return thisValue; }
    noInline(getThisValue);

    function bar(...args) {
        assert(this === thisValue);
        return args;
    }
    noInline(bar);

    function foo(a, b, c, d, ...args) {
        return bar.apply(getThisValue(), args);
    }
    noInline(foo);

    function makeArguments(args) {
        return [1,2,3,4, ...args];
    }
    for (let i = 0; i < iterations; i++) {
        function test() { assert(shallowEq(a, foo.apply(null, makeArguments(a)))); }
        let a = [{}, 25, 50];
        test();

        a = [];
        test();

        a = [{foo: 20}];
        test();

        a = [10, 20, 30, 40, 50, 60, 70, 80];
        test();
    }
}

function test13() {
    "use strict";
    function bar(...args) {
        return args;
    }
    noInline(bar);

    function top(a, b, c, d, ...args) {
        return bar.apply(null, args);
    }
    function foo(...args) {
        let r = top.apply(null, args);
        return r;
    }
    noInline(foo);

    function makeArguments(args) {
        return [1,2,3,4, ...args];
    }
    for (let i = 0; i < iterations; i++) {
        function test() { assert(shallowEq(a, foo.apply(null, makeArguments(a)))); }
        let a = [{}, 25, 50];
        test();

        a = [];
        test();

        a = [10, 20, 30, 40, 50, 60, 70, 80];
        test();
    }
}

function test14() {
    "use strict";
    function bar(...args) {
        return args;
    }
    noInline(bar);

    function top(a, b, c, d, ...args) {
        return bar.apply(null, args);
    }
    function foo(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17) {
        return top(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17);
    }
    noInline(foo);

    function makeArguments(args) {
        let r = [1,2,3,4, ...args];
        while (r.length < foo.length)
            r.push(undefined);
        return r;
    }
    for (let i = 0; i < iterations; i++) {
        function test()
        {
            let args = makeArguments(a);
            assert(shallowEq(args.slice(4), foo.apply(null, args)));
        }

        let a = [{}, 25, 50];
        test();

        a = [];
        test();

        a = [10, 20, 30, 40, 50, 60, 70, 80];
        test();
    }
}

function test15() {
    "use strict";
    function bar(...args) {
        return args;
    }
    noInline(bar);

    function top(a, b, c, d, ...args) {
        return bar.apply(null, args);
    }
    function foo(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17) {
        let r = top(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17);
        return r;
    }
    noInline(foo);

    function makeArguments(args) {
        let r = [1,2,3,4, ...args];
        while (r.length < foo.length)
            r.push(undefined);
        return r;
    }
    for (let i = 0; i < iterations; i++) {
        function test()
        {
            let args = makeArguments(a);
            assert(shallowEq(args.slice(4), foo.apply(null, args)));
        }

        let a = [{}, 25, 50];
        test();

        a = [];
        test();

        a = [10, 20, 30, 40, 50, 60, 70, 80];
        test();
    }
}

let start = Date.now();
test1();
test2();
test3();
test4();
test5();
test6();
test7();
test8();
test9();
test10();
test11();
test12();
test13();
test14();
test15();
const verbose = false;
if (verbose)
    print(Date.now() - start);

