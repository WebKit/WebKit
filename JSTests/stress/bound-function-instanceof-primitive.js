function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual);
}

function primitiveInstanceof(value, ctor) {
    return value instanceof ctor;
}
noInline(primitiveInstanceof);

function hasInstanceCall(ctor, value) {
    return Function.prototype[Symbol.hasInstance].call(ctor, value);
}
noInline(hasInstanceCall);

function objectInstanceof(value, ctor) {
    return value instanceof ctor;
}
noInline(objectInstanceof);

let primitives = [1, 1.5, "x", null, undefined, true, false, Symbol("s"), 1n];

let arrow = () => {};
Object.defineProperty(arrow, "prototype", {
    get() { throw new Error("prototype read"); },
});
let boundArrow = arrow.bind();
let nestedArrow = boundArrow.bind();

shouldBe(primitiveInstanceof(1, arrow), false);
for (let value of primitives) {
    shouldBe(primitiveInstanceof(value, boundArrow), false);
    shouldBe(hasInstanceCall(boundArrow, value), false);
    shouldBe(primitiveInstanceof(value, nestedArrow), false);
    shouldBe(hasInstanceCall(nestedArrow, value), false);
}

for (let i = 0; i < 1e4; ++i) {
    let value = primitives[i % primitives.length];
    shouldBe(primitiveInstanceof(value, boundArrow), false);
    shouldBe(hasInstanceCall(boundArrow, value), false);
    shouldBe(primitiveInstanceof(value, nestedArrow), false);
}

let proto = {};
let reads = 0;
let C = () => {};
Object.defineProperty(C, "prototype", {
    get() {
        reads++;
        return proto;
    },
});
let boundC = C.bind();
let child = Object.create(proto);

reads = 0;
shouldBe(objectInstanceof(child, boundC), true);
shouldBe(reads, 1);
reads = 0;
shouldBe(objectInstanceof({}, boundC), false);
shouldBe(reads, 1);
reads = 0;
shouldBe(objectInstanceof(1, boundC), false);
shouldBe(reads, 0);

reads = 0;
for (let i = 0; i < 1e4; ++i) {
    shouldBe(objectInstanceof(child, boundC), true);
    shouldBe(objectInstanceof(1, boundC), false);
    shouldBe(objectInstanceof({}, boundC), false);
}
shouldBe(reads, 2e4);

let customCalls = 0;
let D = () => {};
Object.defineProperty(D, Symbol.hasInstance, {
    value(value) {
        customCalls++;
        return value === 1;
    },
});
Object.defineProperty(D, "prototype", {
    get() { throw new Error("prototype read"); },
});
let boundD = D.bind();
shouldBe(primitiveInstanceof(1, boundD), true);
shouldBe(primitiveInstanceof(2, boundD), false);
shouldBe(customCalls, 2);

let boundOwn = arrow.bind();
let ownCalls = 0;
Object.defineProperty(boundOwn, Symbol.hasInstance, {
    value() {
        ownCalls++;
        return true;
    },
});
shouldBe(primitiveInstanceof(1, boundOwn), true);
shouldBe(ownCalls, 1);

let proxyGets = [];
let proxy = new Proxy(arrow, {
    get(target, key, receiver) {
        proxyGets.push(key);
        return Reflect.get(target, key, receiver);
    },
});
let boundProxy = proxy.bind();
proxyGets = [];
shouldBe(primitiveInstanceof(1, boundProxy), false);
shouldBe(hasInstanceCall(boundProxy, "x"), false);
if (proxyGets.includes("prototype"))
    throw new Error("prototype read");
shouldBe(proxyGets.includes(Symbol.hasInstance), true);

let proxyCustomCalls = 0;
let proxyCustom = new Proxy(arrow, {
    get(target, key, receiver) {
        if (key === Symbol.hasInstance) {
            return function(value) {
                proxyCustomCalls++;
                return value === 1;
            };
        }
        if (key === "prototype")
            throw new Error("prototype read");
        return Reflect.get(target, key, receiver);
    },
});
let boundProxyCustom = proxyCustom.bind();
shouldBe(primitiveInstanceof(1, boundProxyCustom), true);
shouldBe(primitiveInstanceof(0, boundProxyCustom), false);
shouldBe(proxyCustomCalls, 2);
