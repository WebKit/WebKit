function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(actual + " !== " + expected);
}

function check(op) {
    let log = [];
    try {
        op(log);
        log.push("no-throw");
    } catch (e) {
        log.push(e.name);
    }
    return log.join(",");
}

let rhs = {
    valueOf() {
        return 1;
    }
};

function less(left, right) { return left < right; }
function greater(left, right) { return left > right; }
function lessEq(left, right) { return left <= right; }
function greaterEq(left, right) { return left >= right; }
noInline(less);
noInline(greater);
noInline(lessEq);
noInline(greaterEq);

for (let i = 0; i < 100; ++i) {
    let symbol = Symbol.species;
    shouldBe(check(log => { rhs.valueOf = function() { log.push("valueOf"); return 1; }; less(symbol, rhs); }), "valueOf,TypeError");
    shouldBe(check(log => { rhs.valueOf = function() { log.push("valueOf"); return 1; }; greater(symbol, rhs); }), "valueOf,TypeError");
    shouldBe(check(log => { rhs.valueOf = function() { log.push("valueOf"); return 1; }; lessEq(symbol, rhs); }), "valueOf,TypeError");
    shouldBe(check(log => { rhs.valueOf = function() { log.push("valueOf"); return 1; }; greaterEq(symbol, rhs); }), "valueOf,TypeError");

    shouldBe(check(() => less(symbol, { valueOf() { throw new RangeError("rhs"); } })), "RangeError");
    shouldBe(check(() => greater(symbol, { valueOf() { throw new RangeError("rhs"); } })), "RangeError");
    shouldBe(check(() => lessEq(symbol, { valueOf() { throw new RangeError("rhs"); } })), "RangeError");
    shouldBe(check(() => greaterEq(symbol, { valueOf() { throw new RangeError("rhs"); } })), "RangeError");

    let left = {
        [Symbol.toPrimitive]() {
            return Symbol.iterator;
        }
    };
    shouldBe(check(log => {
        left[Symbol.toPrimitive] = function() { log.push("left"); return Symbol.iterator; };
        rhs.valueOf = function() { log.push("valueOf"); return 1; };
        less(left, rhs);
    }), "left,valueOf,TypeError");
    shouldBe(check(log => {
        left[Symbol.toPrimitive] = function() { log.push("left"); return Symbol.iterator; };
        rhs.valueOf = function() { log.push("valueOf"); return 1; };
        less(rhs, left);
    }), "valueOf,left,TypeError");

    shouldBe(less(1, 2), true);
    shouldBe(greater(2, 1), true);
    shouldBe(lessEq(1, 1), true);
    shouldBe(greaterEq(1, 1), true);
    shouldBe(less("a", "b"), true);
    shouldBe(lessEq("b", "b"), true);
    shouldBe(less(1n, 2n), true);
    shouldBe(greaterEq(2n, 1n), true);
    shouldBe(less("2", 10), true);
}
