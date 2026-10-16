function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${String(expected)} but got ${String(actual)}`);
}

const symbol = Symbol("tag");

const makers = {
    empty: {
        tagger: "symbol",
        hot: true,
        make: `return [];`,
        length: 0,
    },
    int32: {
        tagger: "nine",
        make: `return [i, i + 1, i + 2];`,
        length: 3,
        element: (i, index) => i + index,
    },
    double: {
        tagger: "one",
        make: `return [i + 0.5, i + 1.5];`,
        length: 2,
        element: (i, index) => i + index + 0.5,
    },
    contiguous: {
        tagger: "nine",
        hot: true,
        make: `return [i, "b", "c", "d", i + 4];`,
        length: 5,
        element: (i, index) => index % 4 ? "abcd"[index] : i + index,
    },
    holes: {
        tagger: "one",
        hot: true,
        make: `const array = new Array(9); array[4] = i + 4; array[8] = i + 8; return array;`,
        length: 9,
        element: (i, index) => index % 4 || !index ? undefined : i + index,
        hole: (index) => index % 4 || !index,
        push: false,
    },
    large: {
        tagger: "symbol",
        make: `const array = new Array(1200); array[0] = i; array[1199] = i + 1; return array;`,
        length: 1200,
        element: (i, index) => !index ? i : index === 1199 ? i + 1 : undefined,
        hole: (index) => index && index !== 1199,
        indices: [0, 1, 600, 1198, 1199],
    },
    copyOnWrite: {
        tagger: "nine",
        make: `return [1, 2, 3];`,
        length: 3,
        element: (i, index) => index + 1,
    },
    arrayStorage: {
        tagger: "checked",
        hot: true,
        make: `const array = []; ensureArrayStorage(array); for (let k = 8; k--;) array.unshift(i + k); return array;`,
        length: 8,
        element: (i, index) => i + index,
    },
    object: {
        tagger: "nine",
        make: `const object = {}; object[0] = i; object[1] = "x"; object[2] = i + 2; return object;`,
        element: (i, index) => index === 1 ? "x" : i + index,
        indices: [0, 1, 2],
    },
    typedArray: {
        tagger: "symbol",
        make: `const array = new Uint16Array(4); array[1] = i; return array;`,
        length: 4,
        element: (i, index) => index === 1 ? i & 0xffff : 0,
    },
};

const taggers = {
    one: {
        tag: `target.tag = value;`,
        check(target, value) {
            shouldBe(target.tag, value);
        },
    },
    checked: {
        tag: `if (target.tag !== undefined) return null; target.tag = value;`,
        check(target, value) {
            shouldBe(target.tag, value);
        },
    },
    symbol: {
        tag: `target[symbol] = value;`,
        check(target, value) {
            shouldBe(target[symbol], value);
        },
    },
    nine: {
        tag: `target.p0 = value; target.p1 = value + 1; target.p2 = value + 2; target.p3 = value + 3; target.p4 = value + 4; target.p5 = value + 5; target.p6 = value + 6; target.p7 = value + 7; target.p8 = value + 8;`,
        check(target, value) {
            shouldBe(target.p0, value);
            shouldBe(target.p1, value + 1);
            shouldBe(target.p2, value + 2);
            shouldBe(target.p3, value + 3);
            shouldBe(target.p4, value + 4);
            shouldBe(target.p5, value + 5);
            shouldBe(target.p6, value + 6);
            shouldBe(target.p7, value + 7);
            shouldBe(target.p8, value + 8);
        },
    },
};

let counter = 0;
function compile(parameters, body) {
    const result = new Function("symbol", `return function (${parameters}) { ${body} /* ${counter++} */ };`)(symbol);
    noInline(result);
    return result;
}

function checkElements(maker, target, i) {
    shouldBe(target.length, maker.length);
    const indices = maker.indices;
    const count = indices ? indices.length : maker.length;
    for (let k = 0; k < count; ++k) {
        const index = indices ? indices[k] : k;
        shouldBe(target[index], maker.element(i, index));
        if (maker.hole)
            shouldBe(index in target, !maker.hole(index));
    }
}

function run(maker, tagger, make, tag, iterations) {
    const push = maker.push !== false && Array.isArray(make(0));
    const kept = [];
    for (let i = 0; i < iterations; ++i) {
        const target = tag(make(i), i);
        if (!(i & 63))
            kept.push(target, i);
        tagger.check(target, i);
        if (i < 64 || !(i & 15))
            checkElements(maker, target, i);
        if (push) {
            target.push(i);
            shouldBe(target.length, maker.length + 1);
            shouldBe(target[maker.length], i);
        }
    }
    for (let k = 0; k < kept.length; k += 2) {
        const target = kept[k];
        const i = kept[k + 1];
        tagger.check(target, i);
        if (push)
            target.pop();
        checkElements(maker, target, i);
    }
}
noInline(run);

const few = Math.max(Math.ceil(testLoopCount / 16), 60);

const shared = {};
for (const tagName of ["one", "symbol", "nine"])
    shared[tagName] = compile("target, value", `${taggers[tagName].tag} return target;`);

for (const makeName in makers) {
    const maker = makers[makeName];
    const make = compile("i", maker.make);
    const tagger = taggers[maker.tagger];
    run(maker, tagger, make, compile("target, value", `${tagger.tag} return target;`), maker.hot ? testLoopCount : few);
    if (makeName === "copyOnWrite")
        continue;
    for (const tagName in shared)
        run(maker, taggers[tagName], make, shared[tagName], few);
}
