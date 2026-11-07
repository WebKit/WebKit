function shouldBe(actual, expected, index, key) {
    if (actual !== expected)
        throw new Error("bad value: " + String(actual) + ", expected: " + String(expected) + ", object: " + index + ", key: " + String(key));
}

function getString(object, key) {
    return object[key];
}
noInline(getString);

function getFreshString(object, key) {
    return object[key.substring(1)];
}
noInline(getFreshString);

function getSymbol(object, key) {
    return object[key];
}
noInline(getSymbol);

function getAny(object, key) {
    return object[key];
}
noInline(getAny);

let symbol = Symbol("present");
let missingSymbol = Symbol("missing");

function fill(object) {
    object.present = "present";
    object[symbol] = "symbol";
    object[0] = "zero";
    object[7] = "seven";
    object[4294967294] = "max index";
    object[4294967295] = "not index";
    object["-1"] = "minus one";
    object["01"] = "leading zero";
    return object;
}

function small() {
    return fill(Object.create(null));
}

function empty() {
    return Object.create(null);
}

function dictionary() {
    let object = Object.create(null);
    for (let i = 0; i < 200; ++i)
        object["key" + i] = i;
    return fill(object);
}

function uncacheableDictionary() {
    let object = dictionary();
    delete object.key10;
    return object;
}

function withGetter() {
    let object = small();
    Object.defineProperty(object, "getter", { get() { return "getter"; }, configurable: true });
    return object;
}

function withIndexedGetter() {
    let object = small();
    Object.defineProperty(object, 3, { get() { return this; }, configurable: true });
    return object;
}

function ordinary() {
    return fill({ });
}

function inheriting() {
    let object = fill(Object.create({ inherited: "inherited", 3: "three", [missingSymbol]: "inherited symbol" }));
    return object;
}

function inheritingFromNullPrototype() {
    let prototype = Object.create(null);
    prototype.inherited = "inherited";
    prototype[3] = "three";
    return fill(Object.create(prototype));
}

function inheritingFromProxy() {
    return fill(Object.create(new Proxy({ }, {
        get(target, key) {
            return typeof key === "symbol" ? "proxy symbol" : "proxy " + key;
        }
    })));
}

function inheritingGetter() {
    return fill(Object.create({
        get inherited() { return this; },
        get [missingSymbol]() { return this; },
    }));
}

function inheritingFromProxyReturningReceiver() {
    return fill(Object.create(Object.create(new Proxy({ }, {
        get(target, key, receiver) {
            return receiver;
        }
    }))));
}

function polyProto(prototype) {
    function create() {
        function Constructor() {
            this.own = "own";
        }
        Constructor.prototype = { inherited: "inherited" };
        return Constructor;
    }
    let result;
    for (let i = 0; i < 40; ++i)
        result = new (create());
    Object.setPrototypeOf(result, prototype);
    return fill(result);
}

function frozen() {
    return Object.freeze(small());
}

function array() {
    return fill(Object.setPrototypeOf([], null));
}

function func() {
    return fill(Object.setPrototypeOf(function () { }, null));
}

let objects = [
    small(),
    empty(),
    dictionary(),
    uncacheableDictionary(),
    withGetter(),
    withIndexedGetter(),
    ordinary(),
    inheriting(),
    inheritingFromNullPrototype(),
    inheritingFromProxy(),
    inheritingGetter(),
    inheritingFromProxyReturningReceiver(),
    polyProto(null),
    polyProto({ inherited: "replaced" }),
    frozen(),
    array(),
    func(),
    fill(Object.setPrototypeOf(new Map, null)),
    fill(Object.setPrototypeOf(new Date(0), null)),
    fill(Object.setPrototypeOf(new Error("message"), null)),
];
let primitives = [Symbol("description"), 10n, "string", 1.5, true];
let bases = objects.concat(primitives);

let stringKeys = [
    "present", "missing", "inherited", "own", "getter", "key10", "key11", "__proto__", "toString", "constructor", "length", "name", "description", "message", "size", "",
    "0", "3", "7", "8", "4294967294", "4294967295", "4294967296", "-1", "-0", "01", "1.5", "1e3", "Infinity", "NaN", "undefined", "null",
];
let symbolKeys = [symbol, missingSymbol, Symbol.iterator, Symbol.toStringTag];
let otherKeys = [0, 3, 7, 8, -1, -0, 1.5, 4294967294, 4294967295, undefined, null, true, { toString() { return "missing"; } }, { toString() { return "present"; } }];

let expectedValues = bases.map((object) => [stringKeys, symbolKeys, otherKeys].map((keys) => keys.map((key) => Reflect.get(Object(object), key, object))));
let prefixedKeys = stringKeys.map((key) => "x" + key);

function check(index) {
    let base = bases[index];
    let [expectedStrings, expectedSymbols, expectedOthers] = expectedValues[index];
    for (let i = 0; i < stringKeys.length; ++i)
        shouldBe(getAny(base, stringKeys[i]), expectedStrings[i], index, stringKeys[i]);
    for (let i = 0; i < symbolKeys.length; ++i)
        shouldBe(getAny(base, symbolKeys[i]), expectedSymbols[i], index, symbolKeys[i]);
    for (let i = 0; i < otherKeys.length; ++i)
        shouldBe(getAny(base, otherKeys[i]), expectedOthers[i], index, i);
    if (index >= objects.length)
        return;
    for (let i = 0; i < stringKeys.length; ++i) {
        shouldBe(getString(base, stringKeys[i]), expectedStrings[i], index, stringKeys[i]);
        shouldBe(getFreshString(base, prefixedKeys[i]), expectedStrings[i], index, stringKeys[i]);
    }
    for (let i = 0; i < symbolKeys.length; ++i)
        shouldBe(getSymbol(base, symbolKeys[i]), expectedSymbols[i], index, symbolKeys[i]);
}

for (let i = 0; i < testLoopCount; ++i)
    check(i % bases.length);

let changing = small();
let changingDictionary = uncacheableDictionary();
for (let object of [changing, changingDictionary]) {
    for (let i = 0; i < 10; ++i) {
        shouldBe(getString(object, "inherited"), undefined);
        shouldBe(getFreshString(object, "xinherited"), undefined);
        shouldBe(getSymbol(object, missingSymbol), undefined);
        Object.setPrototypeOf(object, { inherited: i, [missingSymbol]: i });
        shouldBe(getString(object, "inherited"), i);
        shouldBe(getFreshString(object, "xinherited"), i);
        shouldBe(getSymbol(object, missingSymbol), i);
        Object.setPrototypeOf(object, null);
        shouldBe(getString(object, "later" + i), undefined);
        shouldBe(getFreshString(object, "xlater" + i), undefined);
        object["later" + i] = i;
        shouldBe(getString(object, "later" + i), i);
        shouldBe(getFreshString(object, "xlater" + i), i);
        shouldBe(getFreshString(object, "x" + (100 + i)), undefined);
        object[100 + i] = i;
        shouldBe(getFreshString(object, "x" + (100 + i)), i);
    }
}

shouldBe(typeof getString(Object.setPrototypeOf(createGlobalObject().JSON, null), "parse"), "function");
shouldBe(typeof getFreshString(Object.setPrototypeOf(createGlobalObject().JSON, null), "xparse"), "function");
shouldBe(typeof getAny(Object.setPrototypeOf(createGlobalObject().JSON, null), "parse"), "function");

let throwing = fill(Object.create({
    get inherited() { throw new RangeError("inherited"); },
    get [missingSymbol]() { throw new RangeError("symbol"); },
}));
let throwingIndex = small();
Object.defineProperty(throwingIndex, 3, { get() { throw new RangeError("index"); } });
for (let get of [
    () => getString(throwing, "inherited"),
    () => getFreshString(throwing, "xinherited"),
    () => getSymbol(throwing, missingSymbol),
    () => getAny(throwing, "inherited"),
    () => getString(throwingIndex, "3"),
    () => getFreshString(throwingIndex, "x3"),
    () => getAny(throwingIndex, "3"),
]) {
    let error = null;
    try {
        get();
    } catch (e) {
        error = e;
    }
    shouldBe(error instanceof RangeError, true);
}
