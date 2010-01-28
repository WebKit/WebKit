description('This test aims to check for rangeUnderflow flag with type=date input fields');

var input = document.createElement('input');
input.type = 'date';

function checkUnderflow(value, min)
{
    input.value = value;
    input.min = min;
    var underflow = input.validity.rangeUnderflow;
    var resultText = 'The value "' + input.value + '" ' +
        (underflow ? 'undeflows' : 'doesn\'t underflow') +
        ' the minimum value "' + input.min + '".';
    if (underflow)
        testPassed(resultText);
    else
        testFailed(resultText);
}

function checkNotUnderflow(value, min)
{
    input.value = value;
    input.min = min;
    var underflow = input.validity.rangeUnderflow;
    var resultText = 'The value "' + input.value + '" ' +
        (underflow ? 'underflows' : 'doesn\'t underflow') +
        ' the minimum value "' + input.min + '".';
    if (underflow)
        testFailed(resultText);
    else
        testPassed(resultText);
}

// No underflow cases
checkNotUnderflow('2010-01-27', null);
checkNotUnderflow('2010-01-27', '');
checkNotUnderflow('2010-01-27', 'foo');
// 1000-01-01 is smaller than the implicit minimum value.
// But the date parser rejects it before comparing the minimum value.
checkNotUnderflow('1000-01-01', '');
checkNotUnderflow('1582-10-15', '');
checkNotUnderflow('2010-01-27', '2010-01-26');
checkNotUnderflow('2010-01-27', '2009-01-28');
checkNotUnderflow('foo', '2011-01-26');

// Underflow cases
checkUnderflow('2010-01-27', '2010-01-28');
checkUnderflow('9999-01-01', '10000-12-31');
input.max = '2010-01-01';  // value < min && value > max
checkUnderflow('2010-01-27', '2010-02-01');

var successfullyParsed = true;
