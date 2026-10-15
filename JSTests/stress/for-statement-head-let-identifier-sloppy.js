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

var let;

for (let in { key: 1 }) ;
shouldBe(let, "key");

for (let
in { other: 1 }) ;
shouldBe(let, "other");

for (let = 3; ;)
    break;
shouldBe(let, 3);

var count = 0;
for (let; count < 2; ++count) ;
shouldBe(count, 2);
shouldBe(let, 3);

for (let++; ;)
    break;
shouldBe(let, 4);

for (let, count = 5; ;)
    break;
shouldBe(count, 5);

let = { };
for (let.x in { key: 1 }) ;
shouldBe(let.x, "key");

for (let.y = 6; ;)
    break;
shouldBe(let.y, 6);

for (let?.z; ;)
    break;

let = () => ({ });
for (let().x in { key: 1 }) ;
for (let(); ;)
    break;

function inFunction() {
    var let;
    var keys = [];
    var functions = [];
    for (let in { a: 1, b: 2 }) {
        keys.push(let);
        functions.push(() => let);
    }
    shouldBe(keys.join(), "a,b");
    shouldBe(functions[0](), "b");
    for (let = 0; let < 3; ++let) ;
    return let;
}
shouldBe(inFunction(), 3);

for (let of of [7])
    shouldBe(of, 7);
for (let of in { key: 1 })
    shouldBe(of, "key");
for (let of = 8; ;) {
    shouldBe(of, 8);
    break;
}
for (let async of [9])
    shouldBe(async, 9);
for (let
x of [10])
    shouldBe(x, 10);
for (let
[y] of [[11]])
    shouldBe(y, 11);
for (let { z } of [{ z: 12 }])
    shouldBe(z, 12);

shouldBeSyntaxError(`for (let of []) ;`);
shouldBeSyntaxError(`for (let.x of []) ;`);
shouldBeSyntaxError(`for (let().x of []) ;`);
shouldBeSyntaxError(`for (let?.x of []) ;`);
shouldBeSyntaxError(`for (let[0] of []) ;`);
shouldBeSyntaxError(`for (let[0] in {}) ;`);
shouldBeSyntaxError(`for (let[0]; ;) break;`);
shouldBeSyntaxError(`for (let let in {}) ;`);
shouldBeSyntaxError(`for (let let; ;) break;`);
shouldBeSyntaxError(`for (let in in {}) ;`);
shouldBeSyntaxError(`for (let = 1 in {}) ;`);
shouldBeSyntaxError(`for (let, x in {}) ;`);
shouldBeSyntaxError(`for (let) ;`);
shouldBeSyntaxError(`async function f() { for await (let.x of []) ; }`);
shouldBeSyntaxError(`async function f() { for await (let in {}) ; }`);
shouldBeSyntaxError(`async function f() { for await (let; ;) break; }`);
shouldBeSyntaxError(`"use strict"; for (let in {}) ;`);
shouldBeSyntaxError(`"use strict"; for (let; ;) break;`);
shouldBeSyntaxError(`"use strict"; for (let = 3; ;) break;`);
shouldBeSyntaxError(`"use strict"; for (let.x in {}) ;`);
