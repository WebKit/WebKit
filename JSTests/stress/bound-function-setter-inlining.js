function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

let counter = 0;
function makeWriter(source) {
    const writer = new Function("object", "value", "key", `${source} // ${counter++}`);
    noInline(writer);
    return writer;
}

function makeObject(setter, key = "property") {
    return Object.defineProperty({}, key, { set: setter, enumerable: true, configurable: true });
}

let log;
function test(setter, expected, source = "return object.property = value;", key = "property") {
    const object = makeObject(setter, key);
    const writer = makeWriter(source);
    for (let i = 0; i < testLoopCount; ++i) {
        log = undefined;
        shouldBe(writer(object, i, key), i);
        shouldBe(log, expected(i));
    }
}

function store(key, value) { this[key] = value; log = this[key]; return "ignored"; }
function storeStrict(key, value) { "use strict"; this[key] = value; log = this[key]; return "ignored"; }
function typeofThis() { log = typeof this; }
function typeofThisStrict() { "use strict"; log = typeof this; }
function describe() { log = arguments.length + ":" + Array.prototype.join.call(arguments, ","); }
function describeRest(first, ...rest) { log = first + ":" + rest.join(","); }
function describeParameters(a, b, c, d) { log = a + "," + b + "," + c + "," + d; }
function forward() { describe.apply(this, arguments); }
function tailCall(key, value) { "use strict"; return storeStrict.call(this, key, value); }
function storeAndThrow(key, value) {
    if (this.shouldThrow)
        throw new Error(key);
    this[key] = value;
}

const target = { data: 0, set accessor(value) { this.data = value + 1; }, get accessor() { return this.data; } };

test(store.bind(target, "data"), (i) => i);
test(store.bind(target, "accessor"), (i) => i + 1);
test(storeStrict.bind(target, "data"), (i) => i);
test(typeofThis.bind(1), () => "object");
test(typeofThisStrict.bind(1), () => "number");
test(typeofThis.bind(undefined), () => "object");
test(typeofThisStrict.bind(undefined), () => "undefined");

test(describe.bind(null), (i) => "1:" + i);
test(describe.bind(null, "a"), (i) => "2:a," + i);
test(describe.bind(null, "a", "b", "c", "d", "e", "f"), (i) => "7:a,b,c,d,e,f," + i);
test(describeRest.bind(null), (i) => i + ":");
test(describeRest.bind(null, "a", "b"), (i) => "a:b," + i);
test(describeParameters.bind(null), (i) => i + ",undefined,undefined,undefined");
test(describeParameters.bind(null, "a"), (i) => "a," + i + ",undefined,undefined");
test(describeParameters.bind(null, "a", "b", "c", "d"), () => "a,b,c,d");
test(forward.bind(null, "a", "b"), (i) => "3:a,b," + i);
test(tailCall.bind(target, "data"), (i) => i);
test(describe.bind(null, "a").bind(null, "b"), (i) => "3:a,b," + i);

test(store.bind(target, "data"), (i) => i, "'use strict'; return object.property = value;");
test(store.bind(target, "data"), (i) => i, "return object[key] = value;");
test(store.bind(target, "data"), (i) => i, "for (let name in object) return object[name] = value;");
test(store.bind(target, "data"), (i) => i, "return Object.create(object).property = value;");

(function () {
    const constant = makeObject(store.bind(target, "accessor"));
    function writer(value) { constant.property = value; }
    noInline(writer);
    for (let i = 0; i < testLoopCount; ++i) {
        writer(i);
        shouldBe(target.data, i + 1);
    }
    writer(0.5);
    shouldBe(target.data, 1.5);
    writer("1");
    shouldBe(target.data, "11");
    Object.defineProperty(target, "accessor", { set(value) { this.data = "redefined"; } });
    writer(0);
    shouldBe(target.data, "redefined");
    Object.defineProperty(constant, "property", { set: store.bind(target, "data") });
    writer(1);
    shouldBe(target.data, 1);
    Object.defineProperty(constant, "property", { value: 0, writable: true });
    writer(2);
    shouldBe(target.data, 1);
    shouldBe(constant.property, 2);
}());

(function () {
    const state = { shouldThrow: false, data: -1 };
    const object = makeObject(storeAndThrow.bind(state, "data"));
    const writer = makeWriter("try { return object.property = value; } catch (error) { return error.message; }");
    for (let i = 0; i < testLoopCount; ++i) {
        state.shouldThrow = !(i % 100);
        shouldBe(writer(object, i), state.shouldThrow ? "data" : i);
        shouldBe(state.data, state.shouldThrow ? i - 1 : i);
    }
}());
