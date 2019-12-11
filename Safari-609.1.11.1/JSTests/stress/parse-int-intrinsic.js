"use strict";

function assert(b) {
    if (!b)
        throw new Error("Bad")
}

function testIntrinsic(radix) {
    let s = `
        {
            function foo(n) {
                n = n|0;
                return parseInt(n, ${radix});
            }
            noInline(foo);
            for (let i = 0; i < 10000; i++)
                assert(foo(i) === i);
            assert(foo("20") === 20);
        }
    `;

    eval(s);
}

testIntrinsic(10);
testIntrinsic(0);

function testIntrinsic2() {
    function baz(n) {
        n = n | 0;
        return parseInt(n, 16);
    }
    noInline(baz);

    for (let i = 0; i < 100000; i++)
        assert(baz(i) === parseInt("0x" + i));
}
noDFG(testIntrinsic2);
testIntrinsic2();

function testIntrinsic3() {
    function foo(s) {
        return parseInt(s) + 1;
    }
    noInline(foo);

    for (let i = 0; i < 100000; i++)
        assert(foo(i + "") === i + 1);
}
noDFG(testIntrinsic3);
testIntrinsic3();

function testIntrinsic4() {
    function foo(s) {
        return parseInt(s, 0) + 1;
    }
    noInline(foo);

    for (let i = 0; i < 100000; i++)
        assert(foo(i + "") === i + 1);
}
testIntrinsic4();

function testIntrinsic5() {
    function foo(s) {
        return parseInt(s, 10) + 1;
    }
    noInline(foo);

    for (let i = 0; i < 100000; i++)
        assert(foo(i + "") === i + 1);
}
testIntrinsic5();

function testIntrinsic6() {
    function foo(s) {
        return parseInt(s, 16) + 1;
    }
    noInline(foo);

    for (let i = 0; i < 100000; i++)
        assert(foo(i + "") === (parseInt("0x" + i) + 1));
}
noDFG(testIntrinsic6);
testIntrinsic6();

function testIntrinsic7() {
    function foo(s) {
        return parseInt(s, 16) + parseInt(s, 16);
    }
    noInline(foo);

    for (let i = 0; i < 100000; i++)
        assert(foo(i + "") === (parseInt("0x" + i) * 2));
}
noDFG(testIntrinsic7);
testIntrinsic7();
