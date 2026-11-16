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

function exitAndAccess(key) { OSRExit(); return this[key]; }
function exitAndDescribe() { OSRExit(); return arguments.length + ":" + Array.prototype.join.call(arguments, ","); }
function exitAndDescribeParameters(a, b, c, d) { OSRExit(); return a + "," + b + "," + c + "," + d; }

const target = { data: 42, get accessor() { return this.data + 1; } };

test(exitAndAccess.bind(target, "data"), 43, "return object.property + 1;");
test(exitAndAccess.bind(target, "accessor"), 44, "return object.property + 1;");
test(exitAndDescribe.bind(null), "0:!", "return object.property + '!';");
test(exitAndDescribe.bind(null, 1, 2, 3), "3:1,2,3!", "return object.property + '!';");
test(exitAndDescribeParameters.bind(null, 1), "1,undefined,undefined,undefined!", "return object.property + '!';");
test(exitAndDescribeParameters.bind(null, 1, 2, 3, 4, 5), "1,2,3,4!", "return object.property + '!';");
test(exitAndDescribe.bind(null, 1).bind(null, 2), "2:1,2!", "return object.property + '!';");

test(exitAndDescribe.bind(null, 1, 2), "2:1,2!", "return object[key] + '!';");
test(exitAndDescribe.bind(null, 1, 2), "2:1,2!", "return object.length + '!';", "length");
test(exitAndDescribe.bind(null, 1, 2), "2:1,2!", "for (let name in object) return object[name] + '!';");

(function () {
    const objects = [makeObject(exitAndDescribe.bind(null, 1, 2)), makeObject(function () { return "plain"; })];
    const reader = makeReader("return object.property + '!';");
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(reader(objects[i & 1]), i & 1 ? "plain!" : "2:1,2!");
}());
