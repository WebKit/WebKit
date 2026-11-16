function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

const symbols = [];
for (let i = 0; i < 16; ++i)
    symbols.push(Symbol());

const accessors = [];
const plains = [];
for (let i = 0; i < 16; ++i) {
    const object = {};
    const plain = {};
    for (let j = 0; j < i; ++j) {
        object["p" + j] = j;
        plain["p" + j] = j;
    }
    Object.defineProperty(object, symbols[i], { get() { return i; }, set(value) { } });
    for (let j = 0; j < 16; ++j)
        plain[symbols[j]] = j;
    accessors.push(object);
    plains.push(plain);
}

let collect = false;
const handler = {
    get get() {
        if (collect)
            gc();
        return undefined;
    },
    get set() {
        if (collect)
            gc();
        return undefined;
    },
};

function makeKey(index) {
    if (index < 0)
        return Symbol();
    return symbols[index];
}
noInline(makeKey);

function get(object, index) {
    return object[makeKey(index)];
}
noInline(get);

function getMegamorphic(object, index) {
    return object[makeKey(index)];
}
noInline(getMegamorphic);

function put(object, index) {
    object[makeKey(index)] = 1;
}
noInline(put);

const getProxy = new Proxy({}, handler);
const putProxy = new Proxy({}, handler);
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(get(accessors[i & 15], i & 15), i & 15);
    put(accessors[i & 15], i & 15);
    shouldBe(get(getProxy, i & 15), undefined);
    put(putProxy, i & 15);
    shouldBe(getMegamorphic(plains[i & 15], (i >> 4) & 15), (i >> 4) & 15);
}

collect = true;
for (let i = 0; i < 4; ++i) {
    const target = {};
    const proxy = new Proxy(target, handler);
    shouldBe(get(proxy, -1), undefined);
    shouldBe(getMegamorphic(proxy, -1), undefined);
    shouldBe(Object.getOwnPropertySymbols(target).length, 0);

    put(proxy, -1);
    const keys = Object.getOwnPropertySymbols(target);
    shouldBe(keys.length, 1);
    shouldBe(target[keys[0]], 1);
}
