description(
"This tests that op_get_by_pname is compiled correctly."
);

function makeFoo() {
    return function(o) {
        var result = 0;
        for (var n in o)
            result += o[n];
        return result;
    }
}
var foo1 = makeFoo();
var foo2 = makeFoo();
var foo3 = makeFoo();
var foo4 = makeFoo();
var foo5 = makeFoo();

function makeGetByPnameOnConstant() {
    return function(o) {
        var result = 0;
        for (var n in o)
            result += 0[n] ? 0[n] : 0;
        return result;
    }
}
var getByPnameOnConstant1 = makeGetByPnameOnConstant();
var getByPnameOnConstant2 = makeGetByPnameOnConstant();
var getByPnameOnConstant3 = makeGetByPnameOnConstant();
var getByPnameOnConstant4 = makeGetByPnameOnConstant();
var getByPnameOnConstant5 = makeGetByPnameOnConstant();

function makeGetByPnameOnVar() {
    return function(o, v) {
        var result = 0;
        for (var n in o)
            result += v[n] ? v[n] : 0;
        return result;
    }
}
var getByPnameOnVar11 = makeGetByPnameOnVar();
var getByPnameOnVar12 = makeGetByPnameOnVar();
var getByPnameOnVar13 = makeGetByPnameOnVar();
var getByPnameOnVar14 = makeGetByPnameOnVar();
var getByPnameOnVar15 = makeGetByPnameOnVar();
var getByPnameOnVar21 = makeGetByPnameOnVar();
var getByPnameOnVar22 = makeGetByPnameOnVar();
var getByPnameOnVar23 = makeGetByPnameOnVar();
var getByPnameOnVar24 = makeGetByPnameOnVar();
var getByPnameOnVar25 = makeGetByPnameOnVar();
var getByPnameOnVar31 = makeGetByPnameOnVar();
var getByPnameOnVar32 = makeGetByPnameOnVar();
var getByPnameOnVar33 = makeGetByPnameOnVar();
var getByPnameOnVar34 = makeGetByPnameOnVar();
var getByPnameOnVar35 = makeGetByPnameOnVar();
var getByPnameOnVar41 = makeGetByPnameOnVar();
var getByPnameOnVar42 = makeGetByPnameOnVar();
var getByPnameOnVar43 = makeGetByPnameOnVar();
var getByPnameOnVar44 = makeGetByPnameOnVar();
var getByPnameOnVar45 = makeGetByPnameOnVar();
var getByPnameOnVar51 = makeGetByPnameOnVar();
var getByPnameOnVar52 = makeGetByPnameOnVar();
var getByPnameOnVar53 = makeGetByPnameOnVar();
var getByPnameOnVar54 = makeGetByPnameOnVar();
var getByPnameOnVar55 = makeGetByPnameOnVar();
var getByPnameOnVar56 = makeGetByPnameOnVar();

var o = {a:1, b:3, c:7};
var p = {a:1, b:2, c:3, d:4};
var q = {a:1, b:2, c:3, d:4, e:3457};
var r = {a:1, b:2, c:3, d:4, e:91, f:12};
var s = {a:1, b:2, c:3, d:4, e:91, f:12, g:69};

var a = [1, 2, 3];
var o1 = {"1":1, "2":3, "3":7};

var testCases = [
    { test: "foo1(o)", result: "11" },
    { test: "foo2(p)", result: "10" },
    { test: "foo3(q)", result: "3467" },
    { test: "foo4(r)", result: "113" },
    { test: "foo5(s)", result: "182" },

    { test: "getByPnameOnConstant1(a)", result: "0" },
    { test: "getByPnameOnVar11(a, 100)", result: "0" },
    { test: "getByPnameOnVar12(a, 'abc')", result: "'0abc'" },
    { test: "getByPnameOnVar13(a, o)", result: "0" },
    { test: "getByPnameOnVar14(a, o1)", result: "4" },
    { test: "getByPnameOnVar15(a, a)", result: "6" },

    { test: "getByPnameOnConstant2(o1)", result: "0" },
    { test: "getByPnameOnVar21(o1, 100)", result: "0" },
    { test: "getByPnameOnVar22(o1, 'abc')", result: "'0bc0'" },
    { test: "getByPnameOnVar23(o1, o)", result: "0" },
    { test: "getByPnameOnVar24(o1, o1)", result: "11" },
    { test: "getByPnameOnVar25(o1, a)", result: "5" },

    { test: "getByPnameOnConstant3(o)", result: "0" },
    { test: "getByPnameOnVar31(o, 100)", result: "0" },
    { test: "getByPnameOnVar32(o, 'abc')", result: "0" },
    { test: "getByPnameOnVar33(o, o)", result: "11" },
    { test: "getByPnameOnVar34(o, o1)", result: "0" },
    { test: "getByPnameOnVar35(o, a)", result: "0" },

    { test: "getByPnameOnConstant4(0)", result: "0" },
    { test: "getByPnameOnVar41(0, 100)", result: "0" },
    { test: "getByPnameOnVar42(0, 'abc')", result: "0" },
    { test: "getByPnameOnVar43(0, o)", result: "0" },
    { test: "getByPnameOnVar44(0, o1)", result: "0" },
    { test: "getByPnameOnVar45(0, a)", result: "0" },

    { test: "getByPnameOnConstant5('abc')", result: "0" },
    { test: "getByPnameOnVar51('abc', 100)", result: "0" },
    { test: "getByPnameOnVar52('abc', 'abc')", result: "'0abc'" },
    { test: "getByPnameOnVar53('abc', o)", result: "0" },
    { test: "getByPnameOnVar54('abc', o1)", result: "4" },
    { test: "getByPnameOnVar55('abc', a)", result: "6" },
    { test: "getByPnameOnVar56('def', 'abc')", result: "'0abc'" },
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

