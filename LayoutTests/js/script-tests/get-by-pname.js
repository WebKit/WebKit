description(
"This tests that op_get_by_pname is compiled correctly."
);

function foo(o) {
    var result = 0;
    for (var n in o)
        result += o[n];
    return result;
}

function getByPnameOnConstant(o) {
    var result = 0;
    for (var n in o)
        result += 0[n] ? 0[n] : 0;
    return result;
}

function getByPnameOnVar(o, v) {
    var result = 0;
    for (var n in o)
        result += v[n] ? v[n] : 0;
    return result;
}

var o = {a:1, b:3, c:7};
var p = {a:1, b:2, c:3, d:4};
var q = {a:1, b:2, c:3, d:4, e:3457};
var r = {a:1, b:2, c:3, d:4, e:91, f:12};
var s = {a:1, b:2, c:3, d:4, e:91, f:12, g:69};

var a = [1, 2, 3];
var o1 = {"1":1, "2":3, "3":7};

var testCases = [
    { test: "foo(o)", result: "11" },
    { test: "foo(p)", result: "10" },
    { test: "foo(q)", result: "3467" },
    { test: "foo(r)", result: "113" },
    { test: "foo(s)", result: "182" },

    { test: "getByPnameOnConstant(a)", result: "0" },
    { test: "getByPnameOnVar(a, 100)", result: "0" },
    { test: "getByPnameOnVar(a, 'abc')", result: "'0abc'" },
    { test: "getByPnameOnVar(a, o)", result: "0" },
    { test: "getByPnameOnVar(a, o1)", result: "4" },
    { test: "getByPnameOnVar(a, a)", result: "6" },

    { test: "getByPnameOnConstant(o1)", result: "0" },
    { test: "getByPnameOnVar(o1, 100)", result: "0" },
    { test: "getByPnameOnVar(o1, 'abc')", result: "'0bc0'" },
    { test: "getByPnameOnVar(o1, o)", result: "0" },
    { test: "getByPnameOnVar(o1, o1)", result: "11" },
    { test: "getByPnameOnVar(o1, a)", result: "5" },

    { test: "getByPnameOnConstant(o)", result: "0" },
    { test: "getByPnameOnVar(o, 100)", result: "0" },
    { test: "getByPnameOnVar(o, 'abc')", result: "0" },
    { test: "getByPnameOnVar(o, o)", result: "11" },
    { test: "getByPnameOnVar(o, o1)", result: "0" },
    { test: "getByPnameOnVar(o, a)", result: "0" },

    { test: "getByPnameOnConstant(0)", result: "0" },
    { test: "getByPnameOnVar(0, 100)", result: "0" },
    { test: "getByPnameOnVar(0, 'abc')", result: "0" },
    { test: "getByPnameOnVar(0, o)", result: "0" },
    { test: "getByPnameOnVar(0, o1)", result: "0" },
    { test: "getByPnameOnVar(0, a)", result: "0" },

    { test: "getByPnameOnConstant('abc')", result: "0" },
    { test: "getByPnameOnVar('abc', 100)", result: "0" },
    { test: "getByPnameOnVar('abc', 'abc')", result: "'0abc'" },
    { test: "getByPnameOnVar('abc', o)", result: "0" },
    { test: "getByPnameOnVar('abc', o1)", result: "4" },
    { test: "getByPnameOnVar('abc', a)", result: "6" },
    { test: "getByPnameOnVar('def', 'abc')", result: "'0abc'" },
];

// The tiers should be sorted from lowest iterations to highest.
var tiers = [
    // name, iterations
    { name: "cold", iterations: 0 },
    { name: "llint", iterations: 10 },
    { name: "baseline", iterations: 500 },
    { name: "dfg", iterations: 1000 },
    // { name: "ftl", iterations: 100000 },
];
var highestIteration = tiers[tiers.length - 1].iterations;

function isTierIteration(iteration) {
    for (var i = 0; i < tiers.length; i++) {
        var tierIteration = tiers[i].iterations;
        if (iteration < tierIteration)
            return false;
        if (iteration == tierIteration)
            return true;
    }
    return false;
}
function tierName(iteration) {
    for (var i = 0; i < tiers.length; i++) {
        if (iteration == tiers[i].iterations)
            return tiers[i].name;
    }
}

for (var i = 0; i <= highestIteration; ++i) {
    if (isTierIteration(i)) {
        debug("Test tier: " + tierName(i));
        for (var j = 0; j < testCases.length; j++)
            shouldBe(testCases[j].test, testCases[j].result);
        debug("");
    } else {
        for (var j = 0; j < testCases.length; j++)
            eval(testCases[j].test);
    }
}

