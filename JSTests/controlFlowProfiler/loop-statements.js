load("./driver/driver.js");

function forRegular(limit) {
    var sum = 0;
    for (var i = 0; i < limit; i++) {
        sum += i;
    }

    return sum;
}

function forIn(o) {
    var s = "";
    var p;
    for (p in o) {
        s += p;
    }
}

function forOf(a) {
    var s = "";
    var p;
    for (p of a) {
        s += p;
    }
}

function whileLoop(limit) {
    var i = 0;
    var sum = 0;
    while (i < limit) {
        sum += i;
        i++;
    }

    return sum;
}

assert(!hasBasicBlockExecuted(forRegular, "var sum"), "should not have executed yet.");

forRegular(0);
assert(hasBasicBlockExecuted(forRegular, "var sum"), "should have executed.");
assert(!hasBasicBlockExecuted(forRegular, "sum += i"), "should not have executed yet.");

forRegular(1);
assert(hasBasicBlockExecuted(forRegular, "sum += i"), "should have executed.");


assert(!hasBasicBlockExecuted(forIn, "var s"), "should not have executed yet.");

forIn({});
assert(hasBasicBlockExecuted(forIn, "var s"), "should have executed.");
assert(!hasBasicBlockExecuted(forIn, "s += p"), "should not have executed yet.");

forIn({foo: "bar"});
assert(hasBasicBlockExecuted(forIn, "s += p"), "should have executed.");


assert(!hasBasicBlockExecuted(forOf, "var s"), "should not have executed yet.");

forOf([]);
assert(hasBasicBlockExecuted(forOf, "var s"), "should have executed.");
assert(!hasBasicBlockExecuted(forOf, "s += p"), "should not have executed yet.");

forOf(["a"]);
assert(hasBasicBlockExecuted(forOf, "s += p"), "should have executed.");


assert(!hasBasicBlockExecuted(whileLoop, "var sum"), "should not have executed yet.");

whileLoop(0);
assert(hasBasicBlockExecuted(whileLoop, "var sum"), "should have executed.");
assert(!hasBasicBlockExecuted(whileLoop, "sum += i"), "should not have executed yet.");

whileLoop(1);
assert(hasBasicBlockExecuted(whileLoop, "sum += i"), "should have executed.");
