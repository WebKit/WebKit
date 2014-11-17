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
    [ "foo(o)", "11" ],
    [ "foo(p)", "10" ],
    [ "foo(q)", "3467" ],
    [ "foo(r)", "113" ],
    [ "foo(s)", "182" ],

    [ "getByPnameOnConstant(a)", "0" ],
    [ "getByPnameOnVar(a, 100)", "0" ],
    [ "getByPnameOnVar(a, 'abc')", "'0abc'" ],
    [ "getByPnameOnVar(a, o)", "0" ],
    [ "getByPnameOnVar(a, o1)", "4" ],
    [ "getByPnameOnVar(a, a)", "6" ],

    [ "getByPnameOnConstant(o1)", "0" ],
    [ "getByPnameOnVar(o1, 100)", "0" ],
    [ "getByPnameOnVar(o1, 'abc')", "'0bc0'" ],
    [ "getByPnameOnVar(o1, o)", "0" ],
    [ "getByPnameOnVar(o1, o1)", "11" ],
    [ "getByPnameOnVar(o1, a)", "5" ],

    [ "getByPnameOnConstant(o)", "0" ],
    [ "getByPnameOnVar(o, 100)", "0" ],
    [ "getByPnameOnVar(o, 'abc')", "0" ],
    [ "getByPnameOnVar(o, o)", "11" ],
    [ "getByPnameOnVar(o, o1)", "0" ],
    [ "getByPnameOnVar(o, a)", "0" ],

    [ "getByPnameOnConstant(0)", "0" ],
    [ "getByPnameOnVar(0, 100)", "0" ],
    [ "getByPnameOnVar(0, 'abc')", "0" ],
    [ "getByPnameOnVar(0, o)", "0" ],
    [ "getByPnameOnVar(0, o1)", "0" ],
    [ "getByPnameOnVar(0, a)", "0" ],

    [ "getByPnameOnConstant('abc')", "0" ],
    [ "getByPnameOnVar('abc', 100)", "0" ],
    [ "getByPnameOnVar('abc', 'abc')", "'0abc'" ],
    [ "getByPnameOnVar('abc', o)", "0" ],
    [ "getByPnameOnVar('abc', o1)", "4" ],
    [ "getByPnameOnVar('abc', a)", "6" ],
    [ "getByPnameOnVar('def', 'abc')", "'0abc'" ],
];

function testExpr(index) {
    return testCases[index][0];
}
function testExpectedResult(index) {
    return testCases[index][1];
}

// The tiers should be sorted from lowest iterations to highest.
var tiers = [
    // name, iterations
    [ "cold", 0 ],
    [ "llint", 10 ],
    [ "baseline", 500 ],
    [ "dfg", 1000 ],
    // [ "ftl", 100000 ],
];
var highestIteration = tiers[tiers.length - 1][1];

function isTierIteration(iteration) {
    for (var i = 0; i < tiers.length; i++) {
        var tierIteration = tiers[i][1];
        if (iteration < tierIteration)
            return false;
        if (iteration == tierIteration)
            return true;
    }
    return false;
}
function tierName(iteration) {
    for (var i = 0; i < tiers.length; i++) {
        if (iteration == tiers[i][1])
            return tiers[i][0];
    }
}

for (var i = 0; i <= highestIteration; ++i) {
    if (isTierIteration(i)) {
        debug("Test tier: " + tierName(i));
        for (var j = 0; j < testCases.length; j++)
            shouldBe(testExpr(j), testExpectedResult(j));
        debug("");
    } else {
        for (var j = 0; j < testCases.length; j++)
            eval(testExpr(j));
    }
}

