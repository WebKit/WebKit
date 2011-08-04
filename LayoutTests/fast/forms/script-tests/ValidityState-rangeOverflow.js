description('This test aims to check for rangeOverflow flag with input fields');

var input = document.createElement('input');

function checkOverflow(value, max, disabled)
{
    input.value = value;
    input.max = max;
    input.disabled = !!disabled;
    var overflow = input.validity.rangeOverflow;
    var resultText = 'The value "' + input.value + '" ' +
        (overflow ? 'overflows' : 'doesn\'t overflow') +
        ' the maximum value "' + input.max + '"' + (disabled ? ' when disabled.' : '.');
    if (overflow)
        testPassed(resultText);
    else
        testFailed(resultText);
}

function checkNotOverflow(value, max, disabled)
{
    input.value = value;
    input.max = max;
    input.disabled = !!disabled;
    var overflow = input.validity.rangeOverflow;
    var resultText = 'The value "' + input.value + '" ' +
        (overflow ? 'overflows' : 'doesn\'t overflow') +
        ' the maximum value "' + input.max + '"' + (disabled ? ' when disabled.' : '.');
    if (overflow)
        testFailed(resultText);
    else
        testPassed(resultText);
}

// ----------------------------------------------------------------
debug('Type=text');
input.type = 'text';  // No overflow for type=text.
checkNotOverflow('101', '100');

// ----------------------------------------------------------------
debug('');
debug('Type=date');
input.type = 'date';
input.min = '';
// No overflow cases
checkNotOverflow('2010-01-27', null);
checkNotOverflow('2010-01-27', '');
checkNotOverflow('2010-01-27', 'foo');
checkNotOverflow('2010-01-27', '2010-01-27');
checkNotOverflow('2010-01-27', '2010-01-28');
checkNotOverflow('2010-01-27', '2011-01-26');
checkNotOverflow('foo', '2011-01-26');
checkNotOverflow('2010-01-27', '0000-01-01'); // Too small max value.

// Overflow cases
checkOverflow('2010-01-27', '2010-01-26');
checkOverflow('9999-01-01', '2010-12-31');
input.min = '2010-01-28';  // value < min && value > max
checkOverflow('2010-01-27', '2010-01-26');

// Disabled
checkNotOverflow('9999-01-01', '2010-12-31', true);

// ----------------------------------------------------------------
debug('');
debug('Type=datetime');
input.type = 'datetime';
input.min = '';
// No overflow cases
checkNotOverflow('2010-01-27T12:34Z', null);
checkNotOverflow('2010-01-27T12:34Z', '');
checkNotOverflow('2010-01-27T12:34Z', 'foo');
checkNotOverflow('2010-01-27T12:34Z', '2010-01-27T12:34Z');
checkNotOverflow('2010-01-27T12:34Z', '2010-01-27T12:34:56Z');
checkNotOverflow('2010-01-27T12:34Z', '2011-01-26T12:34Z');
checkNotOverflow('foo', '2011-01-26T12:34Z');
checkNotOverflow('2010-01-27T12:34Z', '0000-01-01T00:00Z'); // Too small max value.

// Overflow cases
checkOverflow('2010-01-27T12:34Z', '2010-01-26T12:33:59.999Z');
checkOverflow('9999-01-01T23:59Z', '2010-12-31T00:00Z');
input.min = '2010-01-28T12:00Z';  // value < min && value > max
checkOverflow('2010-01-27T12:34Z', '2010-01-26T12:34Z');

// Disabled
checkNotOverflow('9999-01-01T23:59Z', '2010-12-31T00:00Z', true);

// ----------------------------------------------------------------
debug('');
debug('Type=datetime-local');
input.type = 'datetime-local';
input.min = '';
// No overflow cases
checkNotOverflow('2010-01-27T12:34', null);
checkNotOverflow('2010-01-27T12:34', '');
checkNotOverflow('2010-01-27T12:34', 'foo');
checkNotOverflow('2010-01-27T12:34', '2010-01-27T12:34');
checkNotOverflow('2010-01-27T12:34', '2010-01-27T12:34:56');
checkNotOverflow('2010-01-27T12:34', '2011-01-26T12:34');
checkNotOverflow('foo', '2011-01-26T12:34');
checkNotOverflow('2010-01-27T12:34', '0000-01-01T00:00'); // Too small max value.

// Overflow cases
checkOverflow('2010-01-27T12:34', '2010-01-26T12:33:59.999');
checkOverflow('9999-01-01T23:59', '2010-12-31T00:00');
input.min = '2010-01-28T12:00';  // value < min && value > max
checkOverflow('2010-01-27T12:34', '2010-01-26T12:34');

// Disabled
checkNotOverflow('9999-01-01T23:59', '2010-12-31T00:00', true);

// ----------------------------------------------------------------
debug('');
debug('Type=month');
input.type = 'month';
input.min = '';
// No overflow cases
checkNotOverflow('2010-01', null);
checkNotOverflow('2010-01', '');
checkNotOverflow('2010-01', 'foo');
checkNotOverflow('2010-01', '2010-01');
checkNotOverflow('2010-01', '2010-02');
checkNotOverflow('2010-01', '2011-01');
checkNotOverflow('foo', '2011-01');
checkNotOverflow('2010-01', '0000-01'); // Too small max value.

// Overflow cases
checkOverflow('2010-01', '2009-12');
checkOverflow('9999-01', '2010-12');
input.min = '2010-02';  // value < min && value > max
checkOverflow('2010-01', '2009-12');

// Disabled
checkNotOverflow('9999-01', '2010-12', true);

// ----------------------------------------------------------------
debug('');
debug('Type=number');
input.type = 'number';
input.min = '';
checkNotOverflow('99', '100');  // Very normal case.
checkNotOverflow('-101', '-100');
checkNotOverflow('99', '1E+2');
checkNotOverflow('0.99', '1.00');
checkNotOverflow('abc', '100');  // Invalid value.
checkNotOverflow('', '-1');  // No value.
checkNotOverflow('101', '');  // No max.
checkNotOverflow('101', 'xxx');  // Invalid max.
// The following case should be rangeOverflow==true ideally.  But the "double" type doesn't have enough precision.
checkNotOverflow('0.999999999999999999999999999999999999999999', '0.999999999999999999999999999999999999999998');

// Overflow cases
checkOverflow('101', '100');
checkOverflow('-99', '-100');
checkOverflow('101', '1E+2');
input.min = '200';  // value < min && value > max
checkOverflow('101', '100');

// Disabled
checkNotOverflow('101', '1E+2', true);

// ----------------------------------------------------------------
debug('');
debug('Type=time');
input.type = 'time';
input.min = '';
// No overflow cases
checkNotOverflow('13:16', null);
checkNotOverflow('13:16', '');
checkNotOverflow('13:16', 'foo');
checkNotOverflow('13:16', '13:16');
checkNotOverflow('13:16', '13:17');
checkNotOverflow('13:16', '14:15');
checkNotOverflow('foo', '13:16');

// Overflow cases
checkOverflow('13:16', '13:15');
checkOverflow('23:59:59.999', '13:16');
input.min = '14:00';  // value < min && value > max
checkOverflow('13:16', '12:00');

// Disabled
checkNotOverflow('23:59:59.999', '13:16', true);

var successfullyParsed = true;
