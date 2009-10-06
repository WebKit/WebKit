description('This test aims to check for rangeOverflow flag with type=number input fields');

var input = document.createElement('input');

function check(value, max, overflowExpected)
{
    input.value = value;
    input.max = max;
    var actual = input.validity.rangeOverflow;
    var didPass = actual == overflowExpected;
    var resultText = 'The value "' + input.value + '" ' + (actual ? 'overflows' : 'doesn\'t overflow') + ' the maximum value "' + input.max + '".';
    if (didPass)
        testPassed(resultText);
    else
        testFailed(resultText);
}

// No overflow cases
input.type = 'text';  // No overflow for type=text.
check('101', '100', false);
input.type = 'number';
check('99', '100', false);  // Very normal case.
check('-101', '-100', false);
check('99', '1E+2', false);
check('0.99', '1.00', false);
check('abc', '100', false);  // Invalid value.
check('', '-1', false);  // No value.
check('101', '', false);  // No max.
check('101', 'xxx', false);  // Invalid max.
// The following case should be rangeOverflow==true ideally.  But the "double" type doesn't have enough precision.
check('0.999999999999999999999999999999999999999999', '0.999999999999999999999999999999999999999998', false);

// Overflow cases
check('101', '100', true);
check('-99', '-100', true);
check('101', '1E+2', true);
input.min = '200';  // value < min && value > max
check('101', '100', true);

var successfullyParsed = true;
