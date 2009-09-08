description('This test aims to check for typeMismatch flag with type=number input fields');

var i = document.createElement('input');
i.type = 'number';

function check(value, mismatchExpected)
{
    i.value = value;
    var actual = i.validity.typeMismatch;
    var didPass = actual == mismatchExpected;
    var resultText = value + ' is ' + (didPass ? 'a correct ' : 'an incorrect ') + (actual ? 'invalid' : 'valid') + ' number.';
    if (didPass)
        testPassed(resultText);
    else
        testFailed(resultText);
}

// Valid values
check('0', false);
check('10', false);
check('01', false);
check('-0', false);
check('-1.2', false);
check('1.2E10', false);
check('1.2E-10', false);
check('1.2E+10', false);
check('12345678901234567890123456789012345678901234567890', false);
check('0.12345678901234567890123456789012345678901234567890', false);

// Invalid values
check('abc', true);
check('0xff', true);

check('+1', true);
check(' 10', true);
check('10 ', true);
check('1,2', true);
check('.2', true);
check('1E', true);
check('NaN', true);
check('nan', true);
check('Inf', true);
check('inf', true);
check('Infinity', true);
check('infinity', true);

// Assume empty string as valid.
check('', false);

// The spec allows, but our implementation doesn't
// Huge exponent.
check('1.2E65535', true);

// The spec doesn't allow, but our implementation does.
check('1.', false);
check('1.2e10', false);

var successfullyParsed = true;
