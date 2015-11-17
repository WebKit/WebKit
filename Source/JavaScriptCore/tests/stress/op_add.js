//@ runFTLNoCJIT

// This test module aims to test the addition operator by comparing its runtime
// behavior (using the different tiers) with expected values computed at initialization
// time using the LLINT / bytecode generator.
//
// It works by generating test scenarios from permutations of value pairs to exercise
// the addition operator. It computes the expected results by evaluating an expression
// to add the values in an initialization pass. The scenarios are later applied to
// a set of test functions of the forms:
//
//     variable + variable
//     constant + variable
//     variable + constant
//
// See generateScenarios() and initializeTestCases() for details on how the test
// cases are generated.
//
// If all goes well, this test module will terminate silently. If not, it will print
// errors.

var verbose = false;
var abortOnFirstFail = false;

var o1 = {
    valueOf: function() { return 10; }
};

var set1 = [
    'o1',
    'null',
    'undefined',
    'NaN',
    '"abc"',
];

var set2 = [
    '10',
    '-10',
    '2147483647',
    '-2147483647',
    '4294967296',
    '-4294967296',
    '100.2',
    '-100.2',
    'true',
    'false',
];

// Assemble the values that we'll be testing with:
var values = [];
for (var i = 0; i < set1.length; i++)
    values.push(set1[i]);
for (var i = 0; i < set2.length; i++)
    values.push(set2[i]);
for (var i = 0; i < set2.length; i++)
    values.push('"' + set2[i] + '"');

function generateScenarios(xvalues, yvalues) {
    var scenarios = [];
    for (var i = 0; i < xvalues.length; i++) {
        for (var j = 0; j < yvalues.length; j++) {
            var xStr = xvalues[i];
            var yStr = yvalues[j];
            var x = eval(xStr);
            var y = eval(yStr);
            var name = "(" + xStr + " + " + yStr + ")";
            var expected = eval("" + xStr + " + " + yStr);
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
        name: "add",
        func: function(x, y) { return x + y; },
        xvalues: values,
        yvalues: values
    },
    {
        name: "addI32V",
        func: function(x, y) { return 10 + y; },
        xvalues: [ '10' ],
        yvalues: values
    },
    {
        name: "addVI32",
        func: function(x, y) { return x + 10; },
        xvalues: values,
        yvalues: [ '10' ]
    },
    {
        name: "addI32oV",
        func: function(x, y) { return 2147483647 + y; },
        xvalues: [ '2147483647' ],
        yvalues: values
    },
    {
        name: "addVI32o",
        func: function(x, y) { return x + 2147483647; },
        xvalues: values,
        yvalues: [ '2147483647' ]
    },
    {
        name: "addI32onV",
        func: function(x, y) { return -2147483647 + y; },
        xvalues: [ '-2147483647' ],
        yvalues: values
    },
    {
        name: "addVI32on",
        func: function(x, y) { return x + (-2147483647); },
        xvalues: values,
        yvalues: [ '-2147483647' ]
    },
    {
        name: "addI52V",
        func: function(x, y) { return 4294967296 + y; },
        xvalues: [ '4294967296' ],
        yvalues: values
    },
    {
        name: "addVI52",
        func: function(x, y) { return x + 4294967296; },
        xvalues: values,
        yvalues: [ '4294967296' ]
    },
    {
        name: "addI52nV",
        func: function(x, y) { return -4294967296 + y; },
        xvalues: [ '-4294967296' ],
        yvalues: values
    },
    {
        name: "addVI52n",
        func: function(x, y) { return x + (-4294967296); },
        xvalues: values,
        yvalues: [ '-4294967296' ]
    },
    {
        name: "addDV",
        func: function(x, y) { return 100.2 + y; },
        xvalues: [ '100.2' ],
        yvalues: values
    },
    {
        name: "addVD",
        func: function(x, y) { return x + 100.2; },
        xvalues: values,
        yvalues: [ '100.2' ]
    },
    {
        name: "addBV",
        func: function(x, y) { return true + y; },
        xvalues: [ 'true' ],
        yvalues: values
    },
    {
        name: "addVB",
        func: function(x, y) { return x + true; },
        xvalues: values,
        yvalues: [ 'true' ]
    },
    {
        name: "addSi32V",
        func: function(x, y) { return "10" + y; },
        xvalues: [ '"10"' ],
        yvalues: values
    },
    {
        name: "addVSi32",
        func: function(x, y) { return x + "10"; },
        xvalues: values,
        yvalues: [ '"10"' ]
    },

    {
        name: "addSi32oV",
        func: function(x, y) { return "2147483647" + y; },
        xvalues: [ '"2147483647"' ],
        yvalues: values
    },
    {
        name: "addVSi32o",
        func: function(x, y) { return x + "2147483647"; },
        xvalues: values,
        yvalues: [ '"2147483647"' ]
    },
    {
        name: "addSi32onV",
        func: function(x, y) { return "-2147483647" + y; },
        xvalues: [ '"-2147483647"' ],
        yvalues: values
    },
    {
        name: "addVSi32on",
        func: function(x, y) { return x + "-2147483647"; },
        xvalues: values,
        yvalues: [ '"-2147483647"' ]
    },
    {
        name: "addSi52V",
        func: function(x, y) { return "4294967296" + y; },
        xvalues: [ '"4294967296"' ],
        yvalues: values
    },
    {
        name: "addVSi52",
        func: function(x, y) { return x + "4294967296"; },
        xvalues: values,
        yvalues: [ '"4294967296"' ]
    },
    {
        name: "addSi52nV",
        func: function(x, y) { return "-4294967296" + y; },
        xvalues: [ '"-4294967296"' ],
        yvalues: values
    },
    {
        name: "addVSi52n",
        func: function(x, y) { return x + "-4294967296"; },
        xvalues: values,
        yvalues: [ '"-4294967296"' ]
    },
    {
        name: "addSdV",
        func: function(x, y) { return "100.2" + y; },
        xvalues: [ '"100.2"' ],
        yvalues: values
    },
    {
        name: "addVSd",
        func: function(x, y) { return x + "100.2"; },
        xvalues: values,
        yvalues: [ '"100.2"' ]
    },
    {
        name: "addSbV",
        func: function(x, y) { return "true" + y; },
        xvalues: [ '"true"' ],
        yvalues: values
    },
    {
        name: "addVSb",
        func: function(x, y) { return x + "true"; },
        xvalues: values,
        yvalues: [ '"true"' ]
    },

    {
        name: "addSV",
        func: function(x, y) { return "abc" + y; },
        xvalues: [ '"abc"' ],
        yvalues: values
    },
    {
        name: "addVS",
        func: function(x, y) { return x + "abc"; },
        xvalues: values,
        yvalues: [ '"abc"' ]
    },
    {
        name: "addNV",
        func: function(x, y) { return null + y; },
        xvalues: [ 'null' ],
        yvalues: values
    },
    {
        name: "addVN",
        func: function(x, y) { return x + null; },
        xvalues: values,
        yvalues: [ 'null' ]
    },
    {
        name: "addOV",
        func: function(x, y) { return o1 + y; },
        xvalues: [ 'o1' ],
        yvalues: values
    },
    {
        name: "addVO",
        func: function(x, y) { return x + o1; },
        xvalues: values,
        yvalues: [ 'o1' ]
    },
    {
        name: "addNaNV",
        func: function(x, y) { return NaN + y; },
        xvalues: [ 'NaN' ],
        yvalues: values
    },
    {
        name: "addVNaN",
        func: function(x, y) { return x + NaN; },
        xvalues: values,
        yvalues: [ 'NaN' ]
    },
];

function initializeTestCases() {
    for (var test of testCases) {
        noInline(test.func);
        test.scenarios = generateScenarios(test.xvalues, test.yvalues);
    }
}
initializeTestCases();

var errorReport = "";

function stringifyIfNeeded(x) {
    if (typeof x == "string")
        return '"' + x + '"';
    return x;
}

function runTest(test) {
    var failedScenario = [];
    var scenarios = test.scenarios;
    var testFunc = test.func;
    try {
        for (var i = 0; i < 10000; i++) {
            for (var scenarioID = 0; scenarioID < scenarios.length; scenarioID++) {
                var scenario = scenarios[scenarioID];
                if (verbose)
                    print("Testing " + test.name + ":" + scenario.name + " on iteration " + i + ": expecting " + stringifyIfNeeded(scenario.expected)); 

                var result = testFunc(scenario.x, scenario.y);
                if (result == scenario.expected)
                    continue;
                if (Number.isNaN(result) && Number.isNaN(scenario.expected))
                    continue;
                if (!failedScenario[scenarioID]) {
                    errorReport += "FAIL: " + test.name + ":" + scenario.name + " started failing on iteration " + i
                        + ": expected " + stringifyIfNeeded(scenario.expected) + ", actual " + stringifyIfNeeded(result) + "\n";
                    if (abortOnFirstFail)
                        throw errorReport;
                    failedScenario[scenarioID] = scenario;
                }
            }
        }
    } catch(e) {
        if (abortOnFirstFail)
            throw e; // Negate the catch by re-throwing.
        errorReport += "Unexpected exception: " + e + "\n";
    }
}

for (var test of testCases)
    runTest(test);

if (errorReport !== "")
    throw "Error: bad result:\n" + errorReport;
