description('This test aims to check for rangeOverflow flag with input fields');

var input = document.createElement('input');

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
