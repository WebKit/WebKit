//@ runFTLNoCJIT

// This test module aims to test the multiplication operator by comparing its runtime
// behavior (using the different tiers) with expected values computed at initialization
// time using the LLINT / bytecode generator.
//
// It works by generating test scenarios from permutations of value pairs to exercise
// the multiplication operator. It computes the expected results by evaluating an
// expression to multiply the values in an initialization pass. The scenarios are later
// applied to a set of test functions of the forms:
//
//     variable * variable
//     constant * variable
//     variable * constant
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

var posInfinity = 1 / 0;
var negInfinity = -1 / 0;

var set1 = [
    'o1',
    'null',
    'undefined',
    'NaN',
    'posInfinity',
    'negInfinity',
    '"abc"',
];

var set2 = [
    '0',
    '-0',
    '1',
    '-1',
    '0x3fff',
    '-0x3fff',
    '0x7fff',
    '-0x7fff',
    '0x10000',
    '-0x10000',
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
            var name = "(" + xStr + " * " + yStr + ")";
            var expected = eval("" + xStr + " * " + yStr);
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
        name: "mul",
        func: function(x, y) { return x * y; },
        xvalues: values,
        yvalues: values
    },
    {
        name: "mulI32V",
        func: function(x, y) { return 1 * y; },
        xvalues: [ '1' ],
        yvalues: values
    },
    {
        name: "mulVI32",
        func: function(x, y) { return x * 1; },
        xvalues: values,
        yvalues: [ '1' ]
    },
    {
        name: "mulI32oV",
        func: function(x, y) { return 2147483647 * y; },
        xvalues: [ '2147483647' ],
        yvalues: values
    },
    {
        name: "mulVI32o",
        func: function(x, y) { return x * 2147483647; },
        xvalues: values,
        yvalues: [ '2147483647' ]
    },
    {
        name: "mulI32onV",
        func: function(x, y) { return -2147483647 * y; },
        xvalues: [ '-2147483647' ],
        yvalues: values
    },
    {
        name: "mulVI32on",
        func: function(x, y) { return x * (-2147483647); },
        xvalues: values,
        yvalues: [ '-2147483647' ]
    },
    {
        name: "mulI52V",
        func: function(x, y) { return 4294967296 * y; },
        xvalues: [ '4294967296' ],
        yvalues: values
    },
    {
        name: "mulVI52",
        func: function(x, y) { return x * 4294967296; },
        xvalues: values,
        yvalues: [ '4294967296' ]
    },
    {
        name: "mulI52nV",
        func: function(x, y) { return -4294967296 * y; },
        xvalues: [ '-4294967296' ],
        yvalues: values
    },
    {
        name: "mulVI52n",
        func: function(x, y) { return x * (-4294967296); },
        xvalues: values,
        yvalues: [ '-4294967296' ]
    },
    {
        name: "mulDV",
        func: function(x, y) { return 100.2 * y; },
        xvalues: [ '100.2' ],
        yvalues: values
    },
    {
        name: "mulVD",
        func: function(x, y) { return x * 100.2; },
        xvalues: values,
        yvalues: [ '100.2' ]
    },
    {
        name: "mulBV",
        func: function(x, y) { return true * y; },
        xvalues: [ 'true' ],
        yvalues: values
    },
    {
        name: "mulVB",
        func: function(x, y) { return x * true; },
        xvalues: values,
        yvalues: [ 'true' ]
    },
    {
        name: "mulSi32V",
        func: function(x, y) { return "10" * y; },
        xvalues: [ '"10"' ],
        yvalues: values
    },
    {
        name: "mulVSi32",
        func: function(x, y) { return x * "10"; },
        xvalues: values,
        yvalues: [ '"10"' ]
    },

    {
        name: "mulSi32oV",
        func: function(x, y) { return "2147483647" * y; },
        xvalues: [ '"2147483647"' ],
        yvalues: values
    },
    {
        name: "mulVSi32o",
        func: function(x, y) { return x * "2147483647"; },
        xvalues: values,
        yvalues: [ '"2147483647"' ]
    },
    {
        name: "mulSi32onV",
        func: function(x, y) { return "-2147483647" * y; },
        xvalues: [ '"-2147483647"' ],
        yvalues: values
    },
    {
        name: "mulVSi32on",
        func: function(x, y) { return x * "-2147483647"; },
        xvalues: values,
        yvalues: [ '"-2147483647"' ]
    },
    {
        name: "mulSi52V",
        func: function(x, y) { return "4294967296" * y; },
        xvalues: [ '"4294967296"' ],
        yvalues: values
    },
    {
        name: "mulVSi52",
        func: function(x, y) { return x * "4294967296"; },
        xvalues: values,
        yvalues: [ '"4294967296"' ]
    },
    {
        name: "mulSi52nV",
        func: function(x, y) { return "-4294967296" * y; },
        xvalues: [ '"-4294967296"' ],
        yvalues: values
    },
    {
        name: "mulVSi52n",
        func: function(x, y) { return x * "-4294967296"; },
        xvalues: values,
        yvalues: [ '"-4294967296"' ]
    },
    {
        name: "mulSdV",
        func: function(x, y) { return "100.2" * y; },
        xvalues: [ '"100.2"' ],
        yvalues: values
    },
    {
        name: "mulVSd",
        func: function(x, y) { return x * "100.2"; },
        xvalues: values,
        yvalues: [ '"100.2"' ]
    },
    {
        name: "mulSbV",
        func: function(x, y) { return "true" * y; },
        xvalues: [ '"true"' ],
        yvalues: values
    },
    {
        name: "mulVSb",
        func: function(x, y) { return x * "true"; },
        xvalues: values,
        yvalues: [ '"true"' ]
    },

    {
        name: "mulSV",
        func: function(x, y) { return "abc" * y; },
        xvalues: [ '"abc"' ],
        yvalues: values
    },
    {
        name: "mulVS",
        func: function(x, y) { return x * "abc"; },
        xvalues: values,
        yvalues: [ '"abc"' ]
    },
    {
        name: "mulNV",
        func: function(x, y) { return null * y; },
        xvalues: [ 'null' ],
        yvalues: values
    },
    {
        name: "mulVN",
        func: function(x, y) { return x * null; },
        xvalues: values,
        yvalues: [ 'null' ]
    },
    {
        name: "mulOV",
        func: function(x, y) { return o1 * y; },
        xvalues: [ 'o1' ],
        yvalues: values
    },
    {
        name: "mulVO",
        func: function(x, y) { return x * o1; },
        xvalues: values,
        yvalues: [ 'o1' ]
    },
    {
        name: "mulNaNV",
        func: function(x, y) { return NaN * y; },
        xvalues: [ 'NaN' ],
        yvalues: values
    },
    {
        name: "mulVNaN",
        func: function(x, y) { return x * NaN; },
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

function isIdentical(x, y) {
    if (x == y) {
        if (x)
            return true;
        // Distinguish between 0 and negative 0.
        if (1 / x == 1 / y)
            return true;
    } else if (Number.isNaN(x) && Number.isNaN(y))
        return true;
    return false;
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
                if (isIdentical(result, scenario.expected))
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
