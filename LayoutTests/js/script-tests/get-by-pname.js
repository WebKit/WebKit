description(
"This tests that op_get_by_pname is compiled correctly."
);

function foo(o) {
    var result = 0;
    for (var n in o)
        result += o[n];
    return result;
}

var o = {a:1, b:3, c:7};
var p = {a:1, b:2, c:3, d:4};
var q = {a:1, b:2, c:3, d:4, e:3457};
var r = {a:1, b:2, c:3, d:4, e:91, f:12};
var s = {a:1, b:2, c:3, d:4, e:91, f:12, g:69};

var testCases = [
    [ "foo(o)", "11" ],
    [ "foo(p)", "10" ],
    [ "foo(q)", "3467" ],
    [ "foo(r)", "113" ],
    [ "foo(s)", "182" ],
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

