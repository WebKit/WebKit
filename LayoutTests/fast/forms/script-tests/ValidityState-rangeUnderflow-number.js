description('This test aims to check for rangeUnderflow flag with type=number input fields');

var input = document.createElement('input');

function check(value, min, underflowExpected)
{
    input.value = value;
    input.min = min;
    var actual = input.validity.rangeUnderflow;
    var didPass = actual == underflowExpected;
    var resultText = 'The value "' + input.value + '" ' + (actual ? 'underflows' : 'doesn\'t underflow') + ' the minimum value "' + input.min + '".';
    if (didPass)
        testPassed(resultText);
    else
        testFailed(resultText);
}

// No underflow cases
input.type = 'text';  // No underflow for type=text.
check('99', '100', false);
input.type = 'number';
check('101', '100', false);  // Very normal case.
check('-99', '-100', false);
check('101', '1E+2', false);
check('1.01', '1.00', false);
check('abc', '100', false);  // Invalid value.
check('', '1', false);  // No value.
check('-1', '', false);  // No min.
check('-1', 'xxx', false);  // Invalid min.
// The following case should be rangeUnderflow==true ideally.  But the "double" type doesn't have enough precision.
check('0.999999999999999999999999999999999999999998', '0.999999999999999999999999999999999999999999', false);

// Underflow cases
check('99', '100', true);
check('-101', '-100', true);
check('99', '1E+2', true);
input.max = '100';  // value < min && value > max
check('101', '200', true);

var successfullyParsed = true;
