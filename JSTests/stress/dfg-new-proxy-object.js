function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}, expected: ${expected}`);
}

function shouldThrow(func, message) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof TypeError))
        throw new Error(`Expected TypeError, got: ${error}`);
    shouldBe(error.message, message);
}

const isConstructor = $vm.createBuiltin("(function (c) { return @isConstructor(c); })");

function createFromObject(target, handler) {
    return new Proxy(target, handler);
}
noInline(createFromObject);

function createFromArray(target, handler) {
    return new Proxy(target, handler);
}
noInline(createFromArray);

function createFromFunction(target, handler) {
    return new Proxy(target, handler);
}
noInline(createFromFunction);

function createFromAnything(target, handler) {
    return new Proxy(target, handler);
}
noInline(createFromAnything);

function createAndDrop(target, handler) {
    new Proxy(target, handler);
}
noInline(createAndDrop);

function createWithExtraArgument(target, handler) {
    return new Proxy(target, handler, 42);
}
noInline(createWithExtraArgument);

function createWithoutHandler(target) {
    return new Proxy(target);
}
noInline(createWithoutHandler);

const otherGlobal = createGlobalObject();
const OtherProxy = otherGlobal.Proxy;
function createInOtherRealm(target, handler) {
    return new OtherProxy(target, handler);
}
noInline(createInOtherRealm);

class DerivedFromProxy extends Object {
    constructor(target, handler) {
        super(target, handler);
    }
}
Object.setPrototypeOf(DerivedFromProxy, Proxy);
function createWithOtherNewTarget(target, handler) {
    return new DerivedFromProxy(target, handler);
}
noInline(createWithOtherNewTarget);

const revokedObject = Proxy.revocable({}, {});
revokedObject.revoke();
const revokedFunction = Proxy.revocable(function () { }, {});
revokedFunction.revoke();

const targets = [
    [{}, "object", false],
    [Object.create(null), "object", false],
    [new (class { }), "object", false],
    [[], "object", false],
    [new (class extends Array { }), "object", false],
    [Array.prototype, "object", false],
    [Object.prototype, "object", false],
    [new Map, "object", false],
    [new Date(0), "object", false],
    [new Uint8Array(1), "object", false],
    [new Proxy({}, {}), "object", false],
    [revokedObject.proxy, "object", false],
    [$vm.SimpleObject(), "object", true],
    [function () { }, "function", true],
    [() => { }, "function", false],
    [class { }, "function", true],
    [function () { }.bind(null), "function", true],
    [Math.max, "function", false],
    [Map, "function", true],
    [new Proxy(function () { }, {}), "function", true],
    [new Proxy(() => { }, {}), "function", false],
    [revokedFunction.proxy, "function", true],
];

function checkKinds(create) {
    for (let [target, type, constructible] of targets) {
        let proxy = create(target, {});
        shouldBe(typeof proxy, type);
        shouldBe(isConstructor(proxy), constructible);
    }
    shouldBe(Array.isArray(create([], {})), true);
    shouldBe(Array.isArray(create({}, {})), false);
}

const trapNames = ["has", "get", "getOwnPropertyDescriptor", "ownKeys", "set", "defineProperty", "deleteProperty", "isExtensible", "setPrototypeOf", "getPrototypeOf", "preventExtensions"];

function useTraps(proxy) {
    "x" in proxy;
    proxy.x;
    Object.getOwnPropertyDescriptor(proxy, "x");
    Reflect.ownKeys(proxy);
    proxy.x = 1;
    Object.defineProperty(proxy, "y", { value: 1, configurable: true });
    delete proxy.y;
    Object.isExtensible(proxy);
    Object.setPrototypeOf(proxy, Object.getPrototypeOf(proxy));
    Object.preventExtensions(proxy);
}

function checkTraps() {
    let log = [];
    let handler = {};
    for (let name of trapNames) {
        handler[name] = function (...args) {
            log.push(name);
            return Reflect[name](...args);
        };
    }
    useTraps(createFromObject({}, handler));
    for (let name of trapNames)
        shouldBe(log.includes(name), true);

    let target = {};
    useTraps(createFromObject(target, {}));
    shouldBe(target.x, 1);
    shouldBe(Object.isExtensible(target), false);

    let lateHandler = {};
    let proxy = createFromObject({ x: 1 }, lateHandler);
    shouldBe(proxy.x, 1);
    lateHandler.get = () => 2;
    shouldBe(proxy.x, 2);
}

const anything = [{ x: 0 }, Object.assign([], { x: 1 }), Object.assign(() => { }, { x: 2 }), Object.assign(new Map, { x: 3 })];
const seenByGet = { get(target, key, receiver) { return [this, target, receiver]; } };
const proxies = [];

for (let i = 0; i < testLoopCount; ++i) {
    let target = { x: i };
    let proxy = createFromObject(target, seenByGet);
    let [handler, seenTarget, receiver] = proxy.x;
    shouldBe(handler, seenByGet);
    shouldBe(seenTarget, target);
    shouldBe(receiver, proxy);
    shouldBe(typeof proxy, "object");
    proxies.push(proxy);

    let arrayProxy = createFromArray([i], {});
    shouldBe(arrayProxy[0], i);
    shouldBe(arrayProxy.length, 1);
    shouldBe(Array.isArray(arrayProxy), true);

    shouldBe(createFromFunction(() => i, {})(), i);
    shouldBe(createFromAnything(anything[i % anything.length], {}).x, i % anything.length);
    shouldBe(createWithExtraArgument(target, {}).x, i);
    shouldBe(createWithOtherNewTarget(target, {}).x, i);
    shouldBe($vm.globalObjectForObject(createInOtherRealm(i & 1 ? target : () => i, {})), otherGlobal);
    createAndDrop(target, {});
}

gc();
for (let i = 0; i < proxies.length; ++i)
    shouldBe(proxies[i].x[1].x, i);

checkKinds(createFromObject);
checkKinds(createFromArray);
checkKinds(createFromFunction);
checkKinds(createFromAnything);
checkKinds(createInOtherRealm);
checkKinds(createWithOtherNewTarget);
checkTraps();

for (let value of [undefined, 1, "string"]) {
    for (let i = 0; i < 10; ++i) {
        shouldThrow(() => createFromObject(value, {}), "A Proxy's 'target' should be an Object");
        shouldThrow(() => createFromObject({}, value), "A Proxy's 'handler' should be an Object");
        shouldThrow(() => createAndDrop(value, {}), "A Proxy's 'target' should be an Object");
        shouldThrow(() => createAndDrop({}, value), "A Proxy's 'handler' should be an Object");
        shouldThrow(() => createWithoutHandler({}), "A Proxy's 'handler' should be an Object");
    }
}

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(createFromObject({ x: i }, {}).x, i);
    createAndDrop({}, {});
}
