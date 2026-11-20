function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected " + expected);
}

let log = [];
let target = new Proxy(function f(a, b) {}, {
    getPrototypeOf(target) {
        log.push("getPrototypeOf");
        return Reflect.getPrototypeOf(target);
    },
    getOwnPropertyDescriptor(target, key) {
        log.push("getOwnPropertyDescriptor " + String(key));
        return Reflect.getOwnPropertyDescriptor(target, key);
    },
    get(target, key, receiver) {
        log.push("get " + String(key));
        return Reflect.get(target, key, receiver);
    },
});

function bindTarget(fn) {
    return fn.bind();
}
noInline(bindTarget);

let expected = "get bind, getPrototypeOf, getOwnPropertyDescriptor length, get length, get name";
log = [];
let bound = bindTarget(target);
shouldBe(log.join(", "), expected);
shouldBe(bound.length, 2);
shouldBe(bound.name, "bound f");

for (let i = 0; i < 1e4; ++i) {
    log = [];
    let bound = bindTarget(target);
    shouldBe(log.join(", "), expected);
    shouldBe(bound.length, 2);
    shouldBe(bound.name, "bound f");
}

function plain(a, b, c) {}
function bindPlain(fn) {
    return fn.bind(null, 1);
}
noInline(bindPlain);
for (let i = 0; i < 1e4; ++i) {
    let bound = bindPlain(plain);
    shouldBe(bound.length, 2);
    shouldBe(bound.name, "bound plain");
}

let protoPhase = [];
let protoTarget = new Proxy(function g() {}, {
    getPrototypeOf() {
        protoPhase.push("getPrototypeOf");
        throw new Error("proto");
    },
    getOwnPropertyDescriptor() {
        protoPhase.push("getOwnPropertyDescriptor");
        return undefined;
    },
    get(target, key, receiver) {
        protoPhase.push("get " + String(key));
        return Reflect.get(target, key, receiver);
    },
});
protoPhase = [];
try {
    protoTarget.bind();
    throw new Error("expected throw");
} catch (e) {
    shouldBe(e.message, "proto");
}
shouldBe(protoPhase.join(", "), "get bind, getPrototypeOf");

let lengthPhase = [];
let lengthTarget = new Proxy(function h(a) {}, {
    getPrototypeOf(target) {
        lengthPhase.push("getPrototypeOf");
        return Reflect.getPrototypeOf(target);
    },
    getOwnPropertyDescriptor(target, key) {
        lengthPhase.push("getOwnPropertyDescriptor " + String(key));
        if (key === "length")
            throw new Error("length");
        return Reflect.getOwnPropertyDescriptor(target, key);
    },
    get(target, key, receiver) {
        lengthPhase.push("get " + String(key));
        return Reflect.get(target, key, receiver);
    },
});
lengthPhase = [];
try {
    lengthTarget.bind();
    throw new Error("expected throw");
} catch (e) {
    shouldBe(e.message, "length");
}
shouldBe(lengthPhase.join(", "), "get bind, getPrototypeOf, getOwnPropertyDescriptor length");

let nullProtoLog = [];
let nullProtoTarget = new Proxy(function n() {}, {
    getPrototypeOf() {
        nullProtoLog.push("getPrototypeOf");
        return null;
    },
    getOwnPropertyDescriptor(target, key) {
        nullProtoLog.push("getOwnPropertyDescriptor " + String(key));
        return Reflect.getOwnPropertyDescriptor(target, key);
    },
    get(target, key, receiver) {
        nullProtoLog.push("get " + String(key));
        return Reflect.get(target, key, receiver);
    },
});
nullProtoLog = [];
let nullBound = nullProtoTarget.bind();
shouldBe(nullProtoLog.join(", "), "get bind, getPrototypeOf, getOwnPropertyDescriptor length, get length, get name");
shouldBe(nullBound.length, 0);
shouldBe(nullBound.name, "bound n");
shouldBe(Object.getPrototypeOf(nullBound), null);

let valueOfCalls = 0;
let nonNumberLength = new Proxy(function q(a, b, c) {}, {
    get(target, key, receiver) {
        if (key === "length") {
            return {
                valueOf() {
                    valueOfCalls++;
                    return 9;
                },
            };
        }
        if (key === "name")
            return "q";
        return Reflect.get(target, key, receiver);
    },
});
function bindNonNumberLength(fn) {
    return fn.bind(null, 1);
}
noInline(bindNonNumberLength);
for (let i = 0; i < 1e4; ++i) {
    let bound = bindNonNumberLength(nonNumberLength);
    shouldBe(bound.length, 0);
    shouldBe(bound.name, "bound q");
}
shouldBe(valueOfCalls, 0);
