function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + ", expected: " + expected);
}

var symbol = Symbol("symbol");

class Point {
    #hidden = 50;
    x;
    #alsoHidden = 51;
    y = 2;
    constructor(x)
    {
        this.x = x;
    }
    #method() { }
}

function Base() { }
Base.prototype.inherited = 10;

function get(object) {
    var result = "";
    for (var key in object)
        result += key + ":" + object[key] + ",";
    return result;
}
noInline(get);

function has(object) {
    var result = "";
    for (var key in object)
        result += (key in object) + ":" + object.hasOwnProperty(key) + ",";
    return result;
}
noInline(has);

function increment(object) {
    for (var key in object)
        object[key] = object[key] + 1;
}
noInline(increment);

function describe(object) {
    var result = "";
    for (var key of Reflect.ownKeys(object))
        result += String(key) + ":" + object[key] + ",";
    return result;
}

function check(object, expected, expectedAfterIncrement) {
    shouldBe(get(object), expected);
    shouldBe(has(object), "true:true,".repeat(Object.keys(object).length) + (object instanceof Base ? "true:false," : ""));
    increment(object);
    shouldBe(describe(object), expectedAfterIncrement);
}

function createWithOutOfLineProperties(i) {
    var object = { p0: i };
    for (var j = 1; j < 12; ++j)
        object["p" + j] = j;
    return object;
}

for (var i = 0; i < testLoopCount / 4; ++i) {
    check({ a: i, b: 2, c: 3 }, "a:" + i + ",b:2,c:3,", "a:" + (i + 1) + ",b:3,c:4,");

    check({ a: i, [symbol]: 50, b: 2, c: 3 }, "a:" + i + ",b:2,c:3,", "a:" + (i + 1) + ",b:3,c:4,Symbol(symbol):50,");

    var hidden = { a: i };
    Object.defineProperty(hidden, "hidden", { value: 51, writable: true, configurable: true, enumerable: false });
    hidden.b = 2;
    check(hidden, "a:" + i + ",b:2,", "a:" + (i + 1) + ",hidden:51,b:3,");

    var deleted = { a: i, parent: 0, b: 2, c: 3 };
    delete deleted.parent;
    check(deleted, "a:" + i + ",b:2,c:3,", "a:" + (i + 1) + ",b:3,c:4,");

    var reused = { p: 0, a: i, q: 0, b: 2 };
    delete reused.p;
    delete reused.q;
    reused.c = 3;
    reused.d = 4;
    reused.e = 5;
    check(reused, "a:" + i + ",b:2,c:3,d:4,e:5,", "a:" + (i + 1) + ",b:3,c:4,d:5,e:6,");

    var stored = 0;
    check({ a: i, get b() { return 2; }, set b(value) { stored = value; }, c: 3 }, "a:" + i + ",b:2,c:3,", "a:" + (i + 1) + ",b:2,c:4,");
    shouldBe(stored, 3);

    var readOnly = { a: i, [symbol]: 50 };
    Object.defineProperty(readOnly, "b", { value: 2, writable: false, configurable: true, enumerable: true });
    readOnly.c = 3;
    check(readOnly, "a:" + i + ",b:2,c:3,", "a:" + (i + 1) + ",b:2,c:4,Symbol(symbol):50,");

    check(Object.freeze({ a: i, [symbol]: 50, b: 2 }), "a:" + i + ",b:2,", "a:" + i + ",b:2,Symbol(symbol):50,");

    check(new Point(i), "x:" + i + ",y:2,", "x:" + (i + 1) + ",y:3,");

    var outOfLine = createWithOutOfLineProperties(i);
    delete outOfLine.p2;
    delete outOfLine.p8;
    delete outOfLine.p11;
    Object.defineProperty(outOfLine, "p9", { enumerable: false });
    check(outOfLine, "p0:" + i + ",p1:1,p3:3,p4:4,p5:5,p6:6,p7:7,p10:10,", "p0:" + (i + 1) + ",p1:2,p3:4,p4:5,p5:6,p6:7,p7:8,p9:9,p10:11,");

    var derived = new Base;
    derived.parent = 0;
    derived.a = i;
    delete derived.parent;
    check(derived, "a:" + i + ",inherited:10,", "a:" + (i + 1) + ",inherited:11,");
}

function enterInLoop(object, count) {
    var sum = 0;
    var has = 0;
    for (var i = 0; i < count; ++i) {
        for (var key in object) {
            sum += object[key];
            if (key in object)
                ++has;
            if (object.hasOwnProperty(key))
                ++has;
            object[key] = object[key] + 1;
        }
    }
    return sum + ":" + has;
}
noInline(enterInLoop);

var object = createWithOutOfLineProperties(0);
object[symbol] = -1000000;
delete object.p5;
object.p12 = 5;
shouldBe(enterInLoop(object, testLoopCount), (66 * testLoopCount + 12 * testLoopCount * (testLoopCount - 1) / 2) + ":" + 24 * testLoopCount);
shouldBe(object[symbol], -1000000);
shouldBe(object.p12, 5 + testLoopCount);
