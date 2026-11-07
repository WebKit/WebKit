function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + ", expected: " + expected);
}

var symbol = Symbol("symbol");

function create(i) {
    return { a: i, [symbol]: 50, b: 2, c: 3, d: 4 };
}

function names(object, kind) {
    var result = "";
    for (var key in object) {
        if (key === "b")
            change(object, kind);
        result += key + ",";
    }
    return result;
}

function get(object, kind) {
    var result = "";
    for (var key in object) {
        if (key === "b")
            change(object, kind);
        result += key + ":" + object[key] + ",";
    }
    return result;
}

function has(object, kind) {
    var result = "";
    for (var key in object) {
        if (key === "b")
            change(object, kind);
        result += key + ":" + (key in object) + ":" + object.hasOwnProperty(key) + ",";
    }
    return result;
}

function put(object, kind) {
    for (var key in object) {
        if (key === "b")
            change(object, kind);
        object[key] = 7;
    }
    return Reflect.ownKeys(object).map((key) => String(key) + ":" + object[key]).join();
}

function change(object, kind) {
    if (kind >= 0)
        tests[kind][0](object);
}
noInline(change);

function select(expected, ...indices) {
    return expected.split(",").slice(0, -1).map((entry) => indices.map((index) => entry.split(":")[index]).join(":") + ",").join("");
}

var stored;
var tests = [
    [(object) => { delete object.b; }, "a:0:true:true,b:undefined:false:false,c:3:true:true,d:4:true:true,", "a:7,c:7,d:7,b:7,Symbol(symbol):50"],
    [(object) => { delete object.c; }, "a:0:true:true,b:2:true:true,d:4:true:true,", "a:7,b:7,d:7,Symbol(symbol):50"],
    [(object) => { delete object[symbol]; }, "a:0:true:true,b:2:true:true,c:3:true:true,d:4:true:true,", "a:7,b:7,c:7,d:7"],
    [(object) => { delete object.c; object.c = 30; }, "a:0:true:true,b:2:true:true,c:30:true:true,d:4:true:true,", "a:7,b:7,d:7,c:7,Symbol(symbol):50"],
    [(object) => { delete object.d; object.d = 40; }, "a:0:true:true,b:2:true:true,c:3:true:true,d:40:true:true,", "a:7,b:7,c:7,d:7,Symbol(symbol):50"],
    [(object) => { delete object.c; object.e = 30; }, "a:0:true:true,b:2:true:true,d:4:true:true,", "a:7,b:7,d:7,e:30,Symbol(symbol):50"],
    [(object) => { Object.defineProperty(object, "c", { enumerable: false }); }, "a:0:true:true,b:2:true:true,d:4:true:true,", "a:7,b:7,c:3,d:7,Symbol(symbol):50"],
    [(object) => { Object.defineProperty(object, "c", { get() { return 30; }, set(value) { stored = value; } }); }, "a:0:true:true,b:2:true:true,c:30:true:true,d:4:true:true,", "a:7,b:7,c:30,d:7,Symbol(symbol):50"],
    [(object) => { Object.freeze(object); }, "a:0:true:true,b:2:true:true,c:3:true:true,d:4:true:true,", "a:7,b:2,c:3,d:4,Symbol(symbol):50"],
    [(object) => { Object.setPrototypeOf(object, { c: 30, e: 31, hasOwnProperty: Object.prototype.hasOwnProperty }); delete object.c; }, "a:0:true:true,b:2:true:true,c:30:true:false,d:4:true:true,", "a:7,b:7,d:7,c:7,Symbol(symbol):50"],
    [(object) => { object[0] = 60; delete object.c; }, "a:0:true:true,b:2:true:true,d:4:true:true,", "0:60,a:7,b:7,d:7,Symbol(symbol):50"],
    [(object) => { $vm.toUncacheableDictionary(object); delete object.c; object.e = 30; }, "a:0:true:true,b:2:true:true,d:4:true:true,", "a:7,b:7,d:7,e:30,Symbol(symbol):50"],
];

noInline(get);
noInline(has);
noInline(put);

function test() {
    for (var kind = 0; kind < tests.length; ++kind)
        change(create(0), kind);

    var count = Math.min(testLoopCount, 1000);
    for (var i = 0; i < count; ++i) {
        shouldBe(get(create(i), -1), "a:" + i + ",b:2,c:3,d:4,");
        shouldBe(has(create(i), -1), "a:true:true,b:true:true,c:true:true,d:true:true,");
        shouldBe(put(create(i), -1), "a:7,b:7,c:7,d:7,Symbol(symbol):50");
    }

    for (var kind = 0; kind < tests.length; ++kind) {
        var namesForKind = (0, eval)("(" + names + ") // " + kind);
        noInline(namesForKind);
        for (var i = 0; i < count; ++i) {
            shouldBe(namesForKind(create(i), -1), "a,b,c,d,");
            shouldBe(has(create(i), -1), "a:true:true,b:true:true,c:true:true,d:true:true,");
        }
        for (var i = 0; i < 3; ++i) {
            shouldBe(namesForKind(create(0), kind), select(tests[kind][1], 0));
            shouldBe(get(create(0), kind), select(tests[kind][1], 0, 1));
            shouldBe(has(create(0), kind), select(tests[kind][1], 0, 2, 3));
            shouldBe(put(create(0), kind), tests[kind][2]);
            shouldBe(namesForKind(create(0), -1), "a,b,c,d,");
            shouldBe(get(create(0), -1), "a:0,b:2,c:3,d:4,");
            shouldBe(has(create(0), -1), "a:true:true,b:true:true,c:true:true,d:true:true,");
            shouldBe(put(create(0), -1), "a:7,b:7,c:7,d:7,Symbol(symbol):50");
        }
    }
}
noDFG(test);

test();
shouldBe(stored, 7);
