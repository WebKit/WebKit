description('This test aims to check for rangeUnderflow flag with input fields');

var input = document.createElement('input');

function checkUnderflow(value, min, disabled)
{
    input.value = value;
    input.min = min;
    input.disabled = !!disabled;
    var underflow = input.validity.rangeUnderflow;
    var resultText = 'The value "' + input.value + '" ' +
        (underflow ? 'undeflows' : 'doesn\'t underflow') +
        ' the minimum value "' + input.min + '"' + (disabled ? ' when disabled.' : '.');
    if (underflow)
        testPassed(resultText);
    else
        testFailed(resultText);
}

function checkNotUnderflow(value, min, disabled)
{
    input.value = value;
    input.min = min;
    input.disabled = !!disabled;
    var underflow = input.validity.rangeUnderflow;
    var resultText = 'The value "' + input.value + '" ' +
        (underflow ? 'underflows' : 'doesn\'t underflow') +
        ' the minimum value "' + input.min + '"' + (disabled ? ' when disabled.' : '.');
    if (underflow)
        testFailed(resultText);
    else
        testPassed(resultText);
}

// ----------------------------------------------------------------
debug('Type=text');
input.type = 'text';  // No underflow for type=text.
checkNotUnderflow('99', '100');

// ----------------------------------------------------------------
debug('');
debug('Type=date');
input.type = 'date';
input.max = '';
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

// Disabled
checkNotUnderflow('9999-01-01', '10000-12-31', true);

// ----------------------------------------------------------------
debug('');
debug('Type=datetime');
input.type = 'datetime';
input.max = '';
// No underflow cases
checkNotUnderflow('2010-01-27T12:34Z', null);
checkNotUnderflow('2010-01-27T12:34Z', '');
checkNotUnderflow('2010-01-27T12:34Z', 'foo');
// 1000-01-01 is smaller than the implicit minimum value.
// But the date parser rejects it before comparing the minimum value.
checkNotUnderflow('1000-01-01T12:34Z', '');
checkNotUnderflow('1582-10-15T00:00Z', '');
checkNotUnderflow('2010-01-27T12:34Z', '2010-01-26T00:00Z');
checkNotUnderflow('2010-01-27T12:34Z', '2009-01-28T00:00Z');
checkNotUnderflow('foo', '2011-01-26T00:00Z');

// Underflow cases
checkUnderflow('2010-01-27T12:34Z', '2010-01-27T13:00Z');
checkUnderflow('9999-01-01T12:00Z', '10000-12-31T12:00Z');
input.max = '2010-01-01T12:00Z';  // value < min && value > max
checkUnderflow('2010-01-27T12:00Z', '2010-02-01T12:00Z');

// Disabled
checkNotUnderflow('9999-01-01T12:00Z', '10000-12-31T12:00Z', true);

// ----------------------------------------------------------------
debug('');
debug('Type=datetime-local');
input.type = 'datetime-local';
input.max = '';
// No underflow cases
checkNotUnderflow('2010-01-27T12:34', null);
checkNotUnderflow('2010-01-27T12:34', '');
checkNotUnderflow('2010-01-27T12:34', 'foo');
// 1000-01-01 is smaller than the implicit minimum value.
// But the date parser rejects it before comparing the minimum value.
checkNotUnderflow('1000-01-01T12:34', '');
checkNotUnderflow('1582-10-15T00:00', '');
checkNotUnderflow('2010-01-27T12:34', '2010-01-26T00:00');
checkNotUnderflow('2010-01-27T12:34', '2009-01-28T00:00');
checkNotUnderflow('foo', '2011-01-26T00:00');

// Underflow cases
checkUnderflow('2010-01-27T12:34', '2010-01-27T13:00');
checkUnderflow('9999-01-01T12:00', '10000-12-31T12:00');
input.max = '2010-01-01T12:00';  // value < min && value > max
checkUnderflow('2010-01-27T12:00', '2010-02-01T12:00');

// Disabled
checkNotUnderflow('9999-01-01T12:00', '10000-12-31T12:00', true);

// ----------------------------------------------------------------
debug('');
debug('Type=number');
input.type = 'number';
input.max = '';
// No underflow cases
input.type = 'number';
checkNotUnderflow('101', '100');  // Very normal case.
checkNotUnderflow('-99', '-100');
checkNotUnderflow('101', '1E+2');
checkNotUnderflow('1.01', '1.00');
checkNotUnderflow('abc', '100');  // Invalid value.
checkNotUnderflow('', '1');  // No value.
checkNotUnderflow('-1', '');  // No min.
checkNotUnderflow('-1', 'xxx');  // Invalid min.
// The following case should be rangeUnderflow==true ideally.  But the "double" type doesn't have enough precision.
checkNotUnderflow('0.999999999999999999999999999999999999999998', '0.999999999999999999999999999999999999999999');

// Underflow cases
checkUnderflow('99', '100');
checkUnderflow('-101', '-100');
checkUnderflow('99', '1E+2');
input.max = '100';  // value < min && value > max
checkUnderflow('101', '200');

// Disabled
checkNotUnderflow('99', '1E+2', true);

var successfullyParsed = true;
