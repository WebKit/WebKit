// https://tc39.es/ecma262/#sec-well-known-intrinsic-objects
const wellKnownIntrinsicObjects = [
    AggregateError,
    Array,
    ArrayBuffer,
    Atomics,
    BigInt,
    BigInt64Array,
    BigUint64Array,
    Boolean,
    DataView,
    Date,
    decodeURI,
    decodeURIComponent,
    encodeURI,
    encodeURIComponent,
    Error,
    eval,
    EvalError,
    FinalizationRegistry,
    Float32Array,
    Float64Array,
    Function,
    isFinite,
    isNaN,
    JSON,
    Map,
    Math,
    Number,
    Object,
    parseFloat,
    parseInt,
    Promise,
    Proxy,
    RangeError,
    ReferenceError,
    Reflect,
    RegExp,
    Set,
    SharedArrayBuffer,
    String,
    Symbol,
    SyntaxError,
    TypeError,
    Uint8Array,
    Uint8ClampedArray,
    Uint16Array,
    Uint32Array,
    URIError,
    WeakMap,
    WeakRef,
    WeakSet,
];

function assertNativeGetter(ns) {
    const stringified = Function.prototype.toString.call(ns);
    if (!stringified.startsWith("function get ")) {
        throw new Error("bad value: " + stringified);
    }
}

function assertNativeSetter(ns) {
    const stringified = Function.prototype.toString.call(ns);
    if (!stringified.startsWith("function set ")) {
        throw new Error("bad value: " + stringified);
    }
}

const visited = [];
function visit(ns, isGetter, isSetter) {
    if (visited.includes(ns)) {
        return;
    }
    visited.push(ns);
    if (typeof ns === "function") {
        if (isGetter) {
            assertNativeGetter(ns);
        }
        if (isSetter) {
            assertNativeSetter(ns);
        }
    }
    if (typeof ns !== "function" && (typeof ns !== "object" || ns === null)) {
        return;
    }

    const descriptors = Object.getOwnPropertyDescriptors(ns);
    Reflect.ownKeys(descriptors).forEach((name) => {
        const desc = descriptors[name];
        if ("value" in desc) {
            visit(desc.value, false, false);
        }
        if ("get" in desc) {
            visit(desc.get, true, false);
        }
        if ("set" in desc) {
            visit(desc.set, false, true);
        }
    });
}

wellKnownIntrinsicObjects.forEach((intrinsic) => {
    visit(intrinsic, false, false);
});
