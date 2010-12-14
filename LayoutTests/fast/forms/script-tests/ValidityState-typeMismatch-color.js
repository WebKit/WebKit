description('This test aims to check for typeMismatch flag with type=color input fields');
var i = document.createElement('input');
i.type = 'color';

function check(value, mismatchExpected, disabled)
{
    i.value = value;
    i.disabled = !!disabled;
    var actual = i.validity.typeMismatch;
    var didPass = actual == mismatchExpected;
    var resultText = '"' + value + '" is ' + (didPass ? 'a correct ' : 'an incorrect ') + (actual ? 'invalid' : 'valid') + ' color' + (disabled ? ' when disabled.' : '.');
    if (didPass)
        testPassed(resultText);
    else
        testFailed(resultText);
}

// Valid values
check('', false);
check('#000000', false);
check('#123456', false);
check('#789abc', false);
check('#defABC', false);
check('#DEF012', false);

// Invalid values: named colors
check('black', true);
check('blue', true);
check('red', true);
check('purple', true);
check('green', true);
check('cyan', true);
check('yellow', true);
check('white', true);
check('White', true);
check('WHITE', true);

// Invalid values
check('000000', true);
check('#FFF', true);
check(' #ffffff', true);
check('#ffffff ', true);
check('#00112233', true);
check('rgb(0,0,0)', true);
check('xxx-non-existent-color-name', true);
check('transparent', true);

// Disabled
check('invalid', false, true);

var successfullyParsed = true;
