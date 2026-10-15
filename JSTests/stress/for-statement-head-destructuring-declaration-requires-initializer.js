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

shouldBeSyntaxError(`for (const [z]; ;) break;`);
shouldBeSyntaxError(`for (let [z]; ;) break;`);
shouldBeSyntaxError(`for (var [z]; ;) break;`);
shouldBeSyntaxError(`for (const {z}; ;) break;`);
shouldBeSyntaxError(`for (let {z}; ;) break;`);
shouldBeSyntaxError(`for (var {z}; ;) break;`);
shouldBeSyntaxError(`for (let x = 0, []; ;) break;`);
shouldBeSyntaxError(`for (let x, [a]; ;) break;`);
shouldBeSyntaxError(`for (let [a], b = 1; ;) break;`);
shouldBeSyntaxError(`for (let [a] = [], {b}; ;) break;`);
shouldBeSyntaxError(`for (let [a], b of []) ;`);
shouldBeSyntaxError(`for (let x = 1, [a] of []) ;`);
shouldBeSyntaxError(`for (let [a] o\\u0066 []) ;`);

for (let [a] = [1]; ;) {
    shouldBe(a, 1);
    break;
}
for (var [b] = [2], c = 3, {d} = { d: 4 }; ;) {
    shouldBe(b + c + d, 9);
    break;
}
for (const [e] = [5]; ;) {
    shouldBe(e, 5);
    break;
}
for (let [f] of [[6]])
    shouldBe(f, 6);
for (var {g} of [{ g: 7 }])
    shouldBe(g, 7);
for (const {length} in { abc: 1 })
    shouldBe(length, 3);
for (let [h]
of [[8]])
    shouldBe(h, 8);
for (var [i]
in { j: 1 })
    shouldBe(i, "j");
