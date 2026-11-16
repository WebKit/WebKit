function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldBeSyntaxError(source) {
    var error;
    try {
        new Function(source);
    } catch (e) {
        error = e;
    }
    if (!(error instanceof SyntaxError))
        throw new Error(`expected SyntaxError for ${source}`);
}

var async;

async function test() {
    for await (async of [1, 2]) ;
    shouldBe(async, 2);

    for await (async
    of [3]) ;
    shouldBe(async, 3);

    var of = [4];
    for await (async of of) ;
    shouldBe(async, 4);

    for await (async of async = [5]) ;
    shouldBe(async, 5);

    async = { };
    for await (async.x of [6]) ;
    shouldBe(async.x, 6);

    for await ((async) of [7]) ;
    shouldBe(async, 7);

    for await (\u0061sync of [8]) ;
    shouldBe(async, 8);
}

async function testLocal() {
    var async;
    var functions = [];
    for await (async of [1, 2])
        functions.push(() => async);
    shouldBe(functions[0](), 2);
    return async;
}

async function testStrict() {
    "use strict";
    let async;
    for await (async of [9]) ;
    return async;
}

var finished = false;
var error;
(async function () {
    await test();
    shouldBe(await testLocal(), 2);
    shouldBe(await testStrict(), 9);
    finished = true;
})().catch(e => { error = e; });
drainMicrotasks();
if (error)
    throw error;
shouldBe(finished, true);

for (async of => { }; ;)
    break;

for ((async) of [10]) ;
shouldBe(async, 10);

for (\u0061sync of [11]) ;
shouldBe(async, 11);

shouldBeSyntaxError(`for (async of []) ;`);
shouldBeSyntaxError(`for (async\nof []) ;`);
shouldBeSyntaxError(`for (async of => { } of []) ;`);
shouldBeSyntaxError(`async function f() { for (async of []) ; }`);
shouldBeSyntaxError(`async function f() { for (async\nof []) ; }`);
shouldBeSyntaxError(`async function f() { for await (async of => { } of []) ; }`);
shouldBeSyntaxError(`async function f() { for await (async of => { }; ;) break; }`);
shouldBeSyntaxError(`async function f() { for await (async x of []) ; }`);
shouldBeSyntaxError(`async function f() { for await (async of) ; }`);
shouldBeSyntaxError(`async function f() { for await (async of [], []) ; }`);
