description('This test aims to check for typeMismatch flag with type=time input fields');
var i = document.createElement('input');
i.type = 'time';

function check(value, mismatchExpected)
{
    i.value = value;
    var actual = i.validity.typeMismatch;
    var didPass = actual == mismatchExpected;
    var resultText = '"' + value + '" is ' + (didPass ? 'a correct ' : 'an incorrect ') + (actual ? 'invalid' : 'valid') + ' time string.';
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
shouldBeValid('00:00');
shouldBeValid('23:59');
shouldBeValid('23:59:59');
shouldBeValid('23:59:59.1');
shouldBeValid('23:59:59.12');
shouldBeValid('23:59:59.123');
shouldBeValid('23:59:59.1234567890');
shouldBeValid('00:00:00.0000000000');

// Invalid values
shouldBeInvalid(' 00:00 ');
shouldBeInvalid('1:23');
shouldBeInvalid('011:11');
shouldBeInvalid('ab:11');
shouldBeInvalid('-1:11');
shouldBeInvalid('24:11');
shouldBeInvalid('11');
shouldBeInvalid('11-');
shouldBeInvalid('11:-2');
shouldBeInvalid('11:60');
shouldBeInvalid('11:2b');
shouldBeInvalid('11:ab');
shouldBeInvalid('11:034');
shouldBeInvalid('23:45:');
shouldBeInvalid('23:45:6');
shouldBeInvalid('23:45:-1');
shouldBeInvalid('23:45:70');
shouldBeInvalid('23:45:zz');
shouldBeInvalid('23:45:06.');
shouldBeInvalid('23:45:06.abc');
shouldBeInvalid('23:45:06.789abc');

var successfullyParsed = true;
