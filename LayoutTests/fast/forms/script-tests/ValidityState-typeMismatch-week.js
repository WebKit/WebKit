description('This test aims to check for typeMismatch flag with type=week input fields');
var i = document.createElement('input');
i.type = 'week';

function check(value, mismatchExpected)
{
    i.value = value;
    var actual = i.validity.typeMismatch;
    var didPass = actual == mismatchExpected;
    var resultText = '"' + value + '" is ' + (didPass ? 'a correct ' : 'an incorrect ') + (actual ? 'invalid' : 'valid') + ' week string.';
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
shouldBeValid('0001-W01');
shouldBeValid('1583-W01');
shouldBeValid('9999-W52');
shouldBeValid('275760-W37');
shouldBeValid('2009-W01');
shouldBeValid('2004-W53');  // 2004 started on Thursday.
shouldBeValid('2003-W52');  // 2003 started on Wednesday, but wasn't a leap year.
shouldBeValid('1992-W53');  // 1992 started on Wednesday, and was a leap year.

// Invalid values
shouldBeInvalid(' 2009-W01 ');
shouldBeInvalid('2009W01');
shouldBeInvalid('2009-w01');
shouldBeInvalid('2009-W1');
shouldBeInvalid('2009-W001');
shouldBeInvalid('a');
shouldBeInvalid('-1-W09');
shouldBeInvalid('0000-W52');
shouldBeInvalid('2147483648-W01');
shouldBeInvalid('275760-W38');
shouldBeInvalid('2009-W00');
shouldBeInvalid('2009-W-1');
shouldBeInvalid('2004-W54');  // 2004 started on Thursday.
shouldBeInvalid('2003-W53');  // 2003 started on Wednesday, but wasn't a leap year.
shouldBeInvalid('1992-W54');  // 1992 started on Wednesday, and was a leap year.
shouldBeInvalid('2009/09');
shouldBeInvalid('200909');
shouldBeInvalid('2009-Wxx');
shouldBeInvalid('2009');

var successfullyParsed = true;
