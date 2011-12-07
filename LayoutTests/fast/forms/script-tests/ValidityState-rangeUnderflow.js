description('This test aims to check for rangeUnderflow flag with input fields');

var input = document.createElement('input');

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
