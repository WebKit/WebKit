description('This test aims to check for rangeOverflow flag with type=date input fields');

var input = document.createElement('input');
input.type = 'date';

function checkOverflow(value, max)
{
    input.value = value;
    input.max = max;
    var overflow = input.validity.rangeOverflow;
    var resultText = 'The value "' + input.value + '" ' +
        (overflow ? 'overflows' : 'doesn\'t overflow') +
        ' the maximum value "' + input.max + '".';
    if (overflow)
        testPassed(resultText);
    else
        testFailed(resultText);
}

function checkNotOverflow(value, max)
{
    input.value = value;
    input.max = max;
    var overflow = input.validity.rangeOverflow;
    var resultText = 'The value "' + input.value + '" ' +
        (overflow ? 'overflows' : 'doesn\'t overflow') +
        ' the maximum value "' + input.max + '".';
    if (overflow)
        testFailed(resultText);
    else
        testPassed(resultText);
}

// No overflow cases
checkNotOverflow('2010-01-27', null);
checkNotOverflow('2010-01-27', '');
checkNotOverflow('2010-01-27', 'foo');
checkNotOverflow('2010-01-27', '2010-01-27');
checkNotOverflow('2010-01-27', '2010-01-28');
checkNotOverflow('2010-01-27', '2011-01-26');
checkNotOverflow('foo', '2011-01-26');
checkNotOverflow('2010-01-27', '1000-01-01'); // Too small max value.

// Overflow cases
checkOverflow('2010-01-27', '2010-01-26');
checkOverflow('9999-01-01', '2010-12-31');
input.min = '2010-01-28';  // value < min && value > max
checkOverflow('2010-01-27', '2010-01-26');

var successfullyParsed = true;
