description('This test aims to check for typeMismatch flag with type=color input fields');
var i = document.createElement('input');
i.type = 'color';

function check(value, mismatchExpected)
{
    i.value = value;
    var actual = i.validity.typeMismatch;
    var didPass = actual == mismatchExpected;
    var resultText = '"' + value + '" is ' + (didPass ? 'a correct ' : 'an incorrect ') + (actual ? 'invalid' : 'valid') + ' color.';
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

// Extension: named colors
check('black', false);
check('blue', false);
check('red', false);
check('purple', false);
check('green', false);
check('cyan', false);
check('yellow', false);
check('white', false);
check('White', false);
check('WHITE', false);

// Invalid values
check('000000', true);
check('#FFF', true);
check(' #ffffff', true);
check('#ffffff ', true);
check('#00112233', true);
check('rgb(0,0,0)', true);
check('xxx-non-existent-color-name', true);
check('transparent', true);

var successfullyParsed = true;
