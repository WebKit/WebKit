//@ runFTLNoCJIT

// This test module aims to test the subtraction operator by comparing its runtime
// behavior (using the different tiers) with expected values computed at initialization
// time using the LLINT / bytecode generator.
//
// It works by generating test scenarios from permutations of value pairs to exercise
// the subtraction operator. It computes the expected results by evaluating an expression
// to subtract the values in an initialization pass. The scenarios are later applied to
// a set of test functions of the forms:
//
//     variable - variable
//     constant - variable
//     variable - constant
//
// See generateScenarios() and initializeTestCases() for details on how the test
// cases are generated.
//
// If all goes well, this test module will terminate silently. If not, it will print
// errors.

var o1 = {
    valueOf: function() { return 10; }
};

var set1 = [
    o1,
    null,
    undefined,
    NaN,
    "abc",
];

var set2 = [
    10, -10,
    2147483647, -2147483647,
    4294967296, -4294967296,
    100.2, -100.2,
    true, false
];

// Assemble the values that we'll be testing with:
var values = [];
for (var i = 0; i < set1.length; i++)
    values.push(set1[i]);
for (var i = 0; i < set2.length; i++)
    values.push(set2[i]);
for (var i = 0; i < set2.length; i++)
    values.push("" + set2[i]);

function stringify(value) {
    if (typeof value == "string")
        return '"' + value + '"';
    return "" + value;
}

function generateScenarios(xvalues, yvalues) {
    var scenarios = [];
    for (var i = 0; i < xvalues.length; i++) {
        for (var j = 0; j < yvalues.length; j++) {
            var name = "(" + xvalues[i] + " - " + yvalues[j] + ")";
            var x = xvalues[i];
            var y = yvalues[j];
            var expected = eval(stringify(x) + " - " + stringify(y));
            var scenario = { name: name, x: x, y: y, expected: expected };

            scenarios.push(scenario);
        }
    }
    return scenarios;
}

function printScenarios(scenarios) {
    for (var i = 0; i < scenarios.length; i++) {
        var scenario = scenarios[i];
        print("scenario[" + i + "]: { name: " + scenario.name + ", x: " + scenario.x, ", y: " + scenario.y + ", expected: " + scenario.expected + " }");
    }
}

var testCases = [
    {
        name: "sub",
        func: function(x, y) { return x - y; },
        xvalues: values,
        yvalues: values
    },
    {
        name: "subI32V",
        func: function(x, y) { return 10 - y; },
        xvalues: [ 10 ],
        yvalues: values
    },
    {
        name: "subVI32",
        func: function(x, y) { return x - 10; },
        xvalues: values,
        yvalues: [ 10 ]
    },
    {
        name: "subI32oV",
        func: function(x, y) { return -2147483647 - y; },
        xvalues: [ -2147483647 ],
        yvalues: values
    },
    {
        name: "subVI32o",
        func: function(x, y) { return x - 2147483647; },
        xvalues: values,
        yvalues: [ 2147483647 ]
    },
    {
        name: "subI52V",
        func: function(x, y) { return 4294967296 - y; },
        xvalues: [ 4294967296 ],
        yvalues: values
    },
    {
        name: "subVI52",
        func: function(x, y) { return x - 4294967296; },
        xvalues: values,
        yvalues: [ 4294967296 ]
    },
    {
        name: "subDV",
        func: function(x, y) { return 100.2 - y; },
        xvalues: [ 100.2 ],
        yvalues: values
    },
    {
        name: "subVD",
        func: function(x, y) { return x - 100.2; },
        xvalues: values,
        yvalues: [ 100.2 ]
    },
    {
        name: "subBV",
        func: function(x, y) { return true - y; },
        xvalues: [ true ],
        yvalues: values
    },
    {
        name: "subVB",
        func: function(x, y) { return x - true; },
        xvalues: values,
        yvalues: [ true ]
    },
    {
        name: "subSi32V",
        func: function(x, y) { return "10" - y; },
        xvalues: [ "10" ],
        yvalues: values
    },
    {
        name: "subVSi32",
        func: function(x, y) { return x - "10"; },
        xvalues: values,
        yvalues: [ "10" ]
    },

    {
        name: "subSi32oV",
        func: function(x, y) { return "-2147483647" - y; },
        xvalues: [ "-2147483647" ],
        yvalues: values
    },
    {
        name: "subVSi32o",
        func: function(x, y) { return x - "2147483647"; },
        xvalues: values,
        yvalues: [ "2147483647" ]
    },
    {
        name: "subSi52V",
        func: function(x, y) { return "4294967296" - y; },
        xvalues: [ "4294967296" ],
        yvalues: values
    },
    {
        name: "subVSi52",
        func: function(x, y) { return x - "4294967296"; },
        xvalues: values,
        yvalues: [ "4294967296" ]
    },
    {
        name: "subSdV",
        func: function(x, y) { return "100.2" - y; },
        xvalues: [ "100.2" ],
        yvalues: values
    },
    {
        name: "subVSd",
        func: function(x, y) { return x - "100.2"; },
        xvalues: values,
        yvalues: [ "100.2" ]
    },
    {
        name: "subSbV",
        func: function(x, y) { return "true" - y; },
        xvalues: [ "true" ],
        yvalues: values
    },
    {
        name: "subVSb",
        func: function(x, y) { return x - "true"; },
        xvalues: values,
        yvalues: [ "true" ]
    },

    {
        name: "subSV",
        func: function(x, y) { return "abc" - y; },
        xvalues: [ "abc" ],
        yvalues: values
    },
    {
        name: "subVS",
        func: function(x, y) { return x - "abc"; },
        xvalues: values,
        yvalues: [ "abc" ]
    },
    {
        name: "subNV",
        func: function(x, y) { return null - y; },
        xvalues: [ null ],
        yvalues: values
    },
    {
        name: "subVN",
        func: function(x, y) { return x - null; },
        xvalues: values,
        yvalues: [ null ]
    },
    {
        name: "subOV",
        func: function(x, y) { return o1 - y; },
        xvalues: [ o1 ],
        yvalues: values
    },
    {
        name: "subVO",
        func: function(x, y) { return x - o1; },
        xvalues: values,
        yvalues: [ o1 ]
    },
    {
        name: "subNaNV",
        func: function(x, y) { return NaN - y; },
        xvalues: [ NaN ],
        yvalues: values
    },
    {
        name: "subVNaN",
        func: function(x, y) { return x - NaN; },
        xvalues: values,
        yvalues: [ NaN ]
    },
];

function initializeTestCases() {
    for (var test of testCases) {
        noInline(test.func);
        test.scenarios = generateScenarios(test.xvalues, test.yvalues);
    }
}
initializeTestCases();

function runTest(test) {
    var failedScenario = [];
    var scenarios = test.scenarios;
    var testFunc = test.func;
    try {
        for (var i = 0; i < 10000; i++) {
            for (var scenarioID = 0; scenarioID < scenarios.length; scenarioID++) {
                var scenario = scenarios[scenarioID];
                var result = testFunc(scenario.x, scenario.y);
                if (result == scenario.expected)
                    continue;
                if (Number.isNaN(result) && Number.isNaN(scenario.expected))
                    continue;
                if (!failedScenario[scenarioID]) {
                    print("FAIL: " + test.name + ":" + scenario.name + " started failing on iteration " + i + ": expected " + scenario.expected + ", actual " + result);
                    failedScenario[scenarioID] = scenario;
                }
            }
        }
    } catch(e) {
        print("Unexpected exception: " + e);
    }
}

for (var test of testCases)
    runTest(test);

