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

function exitAndStore(key, value) { OSRExit(); this[key] = value; log = this[key]; return "ignored"; }
function exitAndDescribe() { OSRExit(); log = arguments.length + ":" + Array.prototype.join.call(arguments, ","); return "ignored"; }
function exitAndDescribeParameters(a, b, c, d) { OSRExit(); log = a + "," + b + "," + c + "," + d; return "ignored"; }

const target = { data: 0, set accessor(value) { this.data = value + 1; }, get accessor() { return this.data; } };

test(exitAndStore.bind(target, "data"), (i) => i);
test(exitAndStore.bind(target, "accessor"), (i) => i + 1);
test(exitAndDescribe.bind(null), (i) => "1:" + i);
test(exitAndDescribe.bind(null, "a", "b", "c"), (i) => "4:a,b,c," + i);
test(exitAndDescribeParameters.bind(null, "a"), (i) => "a," + i + ",undefined,undefined");
test(exitAndDescribeParameters.bind(null, "a", "b", "c", "d"), () => "a,b,c,d");
test(exitAndDescribe.bind(null, "a").bind(null, "b"), (i) => "3:a,b," + i);

test(exitAndDescribe.bind(null, "a"), (i) => "2:a," + i, "return object[key] = value;");
test(exitAndDescribe.bind(null, "a"), (i) => "2:a," + i, "for (let name in object) return object[name] = value;");

(function () {
    const objects = [makeObject(exitAndDescribe.bind(null, "a")), makeObject(function (value) { log = "plain" + value; })];
    const writer = makeWriter("return object.property = value;");
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(writer(objects[i & 1], i), i);
        shouldBe(log, i & 1 ? "plain" + i : "2:a," + i);
    }
}());
