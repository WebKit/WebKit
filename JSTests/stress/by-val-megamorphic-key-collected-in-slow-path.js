function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

const symbols = [];
const strings = [];
for (let i = 0; i < 16; ++i) {
    symbols.push(Symbol());
    strings.push("s" + i);
}

const plains = [];
for (let i = 0; i < 16; ++i) {
    const plain = {};
    for (let j = 0; j < i; ++j)
        plain["p" + j] = j;
    for (let j = 0; j < 16; ++j) {
        plain[symbols[j]] = j;
        plain[strings[j]] = j;
    }
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
    get has() {
        if (collect)
            gc();
        return undefined;
    },
};

let count = 0;
function makeKey(index, isSymbol) {
    if (index < 0) {
        if (isSymbol)
            return Symbol();
        const string = "fresh" + count++;
        ({})[string];
        return string;
    }
    return isSymbol ? symbols[index] : strings[index];
}
noInline(makeKey);

function get(object, index, isSymbol) {
    return object[makeKey(index, isSymbol)];
}
noInline(get);

function put(object, index, isSymbol) {
    object[makeKey(index, isSymbol)] = 1;
}
noInline(put);

function has(object, index, isSymbol) {
    return makeKey(index, isSymbol) in object;
}
noInline(has);

for (let i = 0; i < testLoopCount; ++i) {
    put(plains[i & 15], (i >> 4) & 15, !!(i & 256));
    shouldBe(get(plains[i & 15], (i >> 4) & 15, !!(i & 256)), 1);
    shouldBe(has(plains[i & 15], (i >> 4) & 15, !!(i & 256)), true);
}

collect = true;
for (let i = 0; i < 4; ++i) {
    for (const isSymbol of [false, true]) {
        const target = {};
        const proxy = new Proxy(target, handler);
        shouldBe(get(proxy, -1, isSymbol), undefined);
        shouldBe(has(proxy, -1, isSymbol), false);
        shouldBe(Reflect.ownKeys(target).length, 0);

        put(proxy, -1, isSymbol);
        const keys = Reflect.ownKeys(target);
        shouldBe(keys.length, 1);
        shouldBe(typeof keys[0], isSymbol ? "symbol" : "string");
        if (!isSymbol)
            shouldBe(keys[0], "fresh" + (count - 1));
        shouldBe(target[keys[0]], 1);
    }
}
