description('This test aims to check for typeMismatch flag with type=datetime-local input fields');
var i = document.createElement('input');
i.type = 'datetime-local';

function check(value, mismatchExpected)
{
    i.value = value;
    var actual = i.validity.typeMismatch;
    var didPass = actual == mismatchExpected;
    var resultText = '"' + value + '" is ' + (didPass ? 'a correct ' : 'an incorrect ') + (actual ? 'invalid' : 'valid') + ' datetime-local string.';
    if (didPass)
        testPassed(resultText);
    else
        testFailed(resultText);
}

function shouldBeValid(value)
{
    check(value, false);
}

function shouldBeInvalid(value)
{
    check(value, true);
}

// Valid values
shouldBeValid('');
shouldBeValid('2009-09-07T16:49');
shouldBeValid('2009-09-07T16:49:31');
shouldBeValid('2009-09-07T16:49:31.1');
shouldBeValid('2009-09-07T16:49:31.12');
shouldBeValid('2009-09-07T16:49:31.123');
shouldBeValid('2009-09-07T16:49:31.1234567890');
shouldBeValid('2147483647-12-31T23:59:59.999');
shouldBeValid('1582-10-15T00:00:00.000');

// Invalid values
shouldBeInvalid(' 2009-09-07T16:49 ');
shouldBeInvalid('2009-09-07t16:49');
shouldBeInvalid('2009-09-07 16:49');
shouldBeInvalid('2009/09/07T16:49');
shouldBeInvalid('a');
shouldBeInvalid('-1-09-07T16:49');
shouldBeInvalid('1582-10-14T23:59:59');
shouldBeInvalid('2147483648-01-01T00:00:00');

var successfullyParsed = true;
