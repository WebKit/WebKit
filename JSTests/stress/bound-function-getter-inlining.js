function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

let counter = 0;
function makeReader(source) {
    const reader = new Function("object", "key", `${source} // ${counter++}`);
    noInline(reader);
    return reader;
}

function makeObject(getter, key = "property") {
    return Object.defineProperty({}, key, { get: getter, enumerable: true, configurable: true });
}

function test(getter, expected, source = "return object.property;", key = "property") {
    const object = makeObject(getter, key);
    const reader = makeReader(source);
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(reader(object, key), expected);
}

function access(key) { return this[key]; }
function accessStrict(key) { "use strict"; return this[key]; }
function typeofThis() { return typeof this; }
function typeofThisStrict() { "use strict"; return typeof this; }
function describe() { return arguments.length + ":" + Array.prototype.join.call(arguments, ","); }
function describeRest(first, ...rest) { return first + ":" + rest.join(","); }
function describeParameters(a, b, c, d) { return a + "," + b + "," + c + "," + d; }
function forward() { return describe.apply(this, arguments); }
function tailCall(key) { "use strict"; return accessStrict.call(this, key); }
function accessAndThrow(key) {
    if (this.shouldThrow)
        throw new Error(key);
    return this[key];
}

const target = { data: 42, get accessor() { return this.data + 1; } };

test(access.bind(target, "data"), 42);
test(access.bind(target, "accessor"), 43);
test(access.bind(target, "missing"), undefined);
test(accessStrict.bind(target, "data"), 42);
test(access.bind("string", "length"), 6);
test(accessStrict.bind("string", "length"), 6);
test(typeofThis.bind(1), "object");
test(typeofThisStrict.bind(1), "number");
test(typeofThis.bind(undefined), "object");
test(typeofThisStrict.bind(undefined), "undefined");

test(describe.bind(null), "0:");
test(describe.bind(null, 1), "1:1");
test(describe.bind(null, 1, 2, 3, 4, 5, 6, 7), "7:1,2,3,4,5,6,7");
test(describeRest.bind(null), "undefined:");
test(describeRest.bind(null, 1, 2, 3), "1:2,3");
test(describeParameters.bind(null), "undefined,undefined,undefined,undefined");
test(describeParameters.bind(null, 1), "1,undefined,undefined,undefined");
test(describeParameters.bind(null, 1, 2, 3, 4, 5), "1,2,3,4");
test(forward.bind(null, 1, 2, 3), "3:1,2,3");
test(tailCall.bind(target, "data"), 42);
test(describe.bind(null, 1).bind(null, 2).bind(null, 3), "3:1,2,3");
test(Math.max.bind(null, 3, 9), 9);

test(access.bind(target, "data"), 42, "return object[key];");
test(access.bind(target, "data"), 42, "return object.length;", "length");
test(access.bind(target, "data"), 42, "for (let name in object) return object[name];");
test(access.bind(target, "data"), 42, "return Object.create(object).property;");

(function () {
    const constant = makeObject(access.bind(target, "accessor"));
    function reader() { return constant.property; }
    noInline(reader);
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(reader(), 43);
    target.data = 1.5;
    shouldBe(reader(), 2.5);
    target.data = "1";
    shouldBe(reader(), "11");
    Object.defineProperty(target, "accessor", { get() { return "redefined"; } });
    shouldBe(reader(), "redefined");
    Object.defineProperty(target, "accessor", { value: "data" });
    shouldBe(reader(), "data");
    delete target.accessor;
    shouldBe(reader(), undefined);
    Object.defineProperty(constant, "property", { get: access.bind(target, "data") });
    shouldBe(reader(), "1");
    Object.defineProperty(constant, "property", { value: 0 });
    shouldBe(reader(), 0);
}());

(function () {
    const state = { shouldThrow: false, data: 42 };
    const object = makeObject(accessAndThrow.bind(state, "data"));
    const reader = makeReader("try { return object.property; } catch (error) { return error.message; }");
    for (let i = 0; i < testLoopCount; ++i) {
        state.shouldThrow = !(i % 100);
        shouldBe(reader(object), state.shouldThrow ? "data" : 42);
    }
}());

(function () {
    const object = makeObject((class { }).bind(null));
    const other = makeObject(access.bind(target, "missing"));
    const reader = makeReader("try { return object.property; } catch (error) { return error instanceof TypeError; }");
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(reader(i % 100 ? other : object), i % 100 ? undefined : true);
}());

(function () {
    const objects = [];
    for (let i = 0; i < 4; ++i)
        objects.push(makeObject(describe.bind(null, i)));
    const reader = makeReader("return object.property;");
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(reader(objects[0]), "1:0");
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(reader(objects[i & 3]), "1:" + (i & 3));
}());

(function () {
    const object = {};
    function recurse(depth) { return depth ? this["level" + (depth - 1)] + 1 : 0; }
    for (let depth = 0; depth < 8; ++depth)
        Object.defineProperty(object, "level" + depth, { get: recurse.bind(object, depth) });
    const reader = makeReader("return object.level7;");
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(reader(object), 7);
}());

(function () {
    const other = createGlobalObject();
    other.marker = "other";
    const otherAccess = other.Function("key", "return this[key];");
    test(otherAccess.bind(target, "data"), "1");
    test(otherAccess.bind(undefined, "marker"), "other");
}());
