description(
'Test for equality of many combinations types.'
);

var values = [ '0', '1', '0.1', '2', '3', '4', '5', '6', '7', '-0', '"0"', '"1"', '"0.1"', '"-0"', 'null', 'undefined', 'false', 'true', 'new String("0")', 'new Object' ];

var exceptions = [
    '"-0" == false',
    '"0" == false',
    '"0" == new String("0")',
    '"1" == true',
    '-0 == "-0"',
    '-0 == "0"',
    '-0 == false',
    '-0 == new String("0")',
    '0 == "-0"',
    '0 == "0"',
    '0 == -0',
    '0 == false',
    '0 == new String("0")',
    '0 === -0',
    '0.1 == "0.1"',
    '1 == "1"',
    '1 == true',
    'new Object == new Object',
    'new Object === new Object',
    'new String("0") == false',
    'new String("0") == new String("0")',
    'new String("0") === new String("0")',
    'null == undefined',
];

var exceptionMap = new Object;

var i, j;

for (i = 0; i < exceptions.length; ++i)
    exceptionMap[exceptions[i]] = 1;

for (i = 0; i < values.length; ++i) {
    for (j = 0; j < values.length; ++j) {
        var expression = values[i] + " == " + values[j];
        var reversed = values[j] + " == " + values[i];
        shouldBe(expression, ((i == j) ^ (exceptionMap[expression] || exceptionMap[reversed])) ? "true" : "false");
    }
}

for (i = 0; i < values.length; ++i) {
    for (j = 0; j < values.length; ++j) {
        var expression = values[i] + " === " + values[j];
        var reversed = values[j] + " === " + values[i];
        shouldBe(expression, ((i == j) ^ (exceptionMap[expression] || exceptionMap[reversed])) ? "true" : "false");
    }
}

var successfullyParsed = true;
