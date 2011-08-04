description('Check stepUp() and stepDown() bahevior for type=date, datetime, datetime-local, month, time.');

var input = document.createElement('input');
var invalidStateErr = '"Error: INVALID_STATE_ERR: DOM Exception 11"';

function setInputAttributes(min, max, step, value) {
    input.min = min;
    input.max = max;
    input.step = step;
    input.value = value;
}

function stepUp(value, step, max, optionalStepCount) {
    setInputAttributes(null, max, step, value);
    if (typeof optionalStepCount != "undefined")
        input.stepUp(optionalStepCount);
    else
        input.stepUp();
    return input.value;
}

function stepDown(value, step, min, optionalStepCount) {
    setInputAttributes(min, null, step, value);
    if (typeof optionalStepCount != "undefined")
        input.stepDown(optionalStepCount);
    else
        input.stepDown();
    return input.value;
}

// Range value gets automatically shifted based on bounds,
// So always set the min and max first to get expected behavior

function stepUpExplicitBounds(min, max, step, value, stepCount) {
    setInputAttributes(min, max, step, value);
    if (typeof stepCount !== 'undefined')
        input.stepUp(stepCount);
    else
        input.stepUp();
    return input.value;
}

function stepDownExplicitBounds(min, max, step, value, stepCount) {
    setInputAttributes(min, max, step, value);
    if (typeof stepCount !== 'undefined')
        input.stepDown(stepCount);
    else
        input.stepDown();
    return input.value;
}

debug('Date type');
input.type = 'date';
debug('Invalid value');
shouldThrow('stepUp("", null, null)', invalidStateErr);
shouldThrow('stepDown("", null, null)', invalidStateErr);
debug('Non-number arguments');
shouldBe('stepUp("2010-02-10", null, null, "0")', '"2010-02-10"');
shouldBe('stepDown("2010-02-10", null, null, "0")', '"2010-02-10"');
shouldBe('stepUp("2010-02-10", null, null, "foo")', '"2010-02-10"');
shouldBe('stepDown("2010-02-10", null, null, "foo")', '"2010-02-10"');
shouldBe('stepUp("2010-02-10", null, null, null)', '"2010-02-10"');
shouldBe('stepDown("2010-02-10", null, null, null)', '"2010-02-10"');
debug('Normal cases');
shouldBe('stepUp("2010-02-10", null, null)', '"2010-02-11"');
shouldBe('stepDown("2010-02-10", null, null)', '"2010-02-09"');
shouldBe('stepUp("2010-02-10", null, null, 10)', '"2010-02-20"');
shouldBe('stepDown("2010-02-10", null, null, 11)', '"2010-01-30"');
shouldBe('stepUp("1970-01-01", "4", null, 2)', '"1970-01-09"');
shouldBe('stepDown("1970-01-01", "4", null, 3)', '"1969-12-20"');
debug('Step=any');
shouldThrow('stepUp("2010-02-10", "any", null)', invalidStateErr);
shouldThrow('stepDown("2010-02-10", "any", null)', invalidStateErr);
debug('Overflow/underflow');
shouldThrow('stepUp("2010-02-10", "3.40282346e+38", null)', invalidStateErr);
shouldThrow('stepDown("2010-02-10", "3.40282346e+38", null)', invalidStateErr);
shouldThrow('stepUp("2010-02-10", "1", "2010-02-10")', invalidStateErr);
shouldThrow('stepDown("2010-02-10", "1", "2010-02-10")', invalidStateErr);

debug('');
debug('Datetime type');
input.type = 'datetime';
debug('Invalid value');
shouldThrow('stepUp("", null, null)', invalidStateErr);
shouldThrow('stepDown("", null, null)', invalidStateErr);
debug('Non-number arguments');
shouldBe('stepUp("2010-02-10T20:13Z", null, null, "0")', '"2010-02-10T20:13Z"');
shouldBe('stepDown("2010-02-10T20:13Z", null, null, "0")', '"2010-02-10T20:13Z"');
shouldBe('stepUp("2010-02-10T20:13Z", null, null, "foo")', '"2010-02-10T20:13Z"');
shouldBe('stepDown("2010-02-10T20:13Z", null, null, "foo")', '"2010-02-10T20:13Z"');
shouldBe('stepUp("2010-02-10T20:13Z", null, null, null)', '"2010-02-10T20:13Z"');
shouldBe('stepDown("2010-02-10T20:13Z", null, null, null)', '"2010-02-10T20:13Z"');
debug('Normal cases');
shouldBe('stepUp("2010-02-10T20:13Z", null, null)', '"2010-02-10T20:14Z"');
shouldBe('stepDown("2010-02-10T20:13Z", null, null)', '"2010-02-10T20:12Z"');
shouldBe('stepUp("2010-02-10T20:13Z", null, null, 10)', '"2010-02-10T20:23Z"');
shouldBe('stepDown("2010-02-10T20:13Z", null, null, 11)', '"2010-02-10T20:02Z"');
shouldBe('stepUp("1970-01-01T20:13Z", "4", null, 2)', '"1970-01-01T20:13:08Z"');
shouldBe('stepDown("1970-01-01T20:13Z", "4", null, 3)', '"1970-01-01T20:12:48Z"');
debug('Step=any');
shouldThrow('stepUp("2010-02-10T20:13Z", "any", null)', invalidStateErr);
shouldThrow('stepDown("2010-02-10T20:13Z", "any", null)', invalidStateErr);
debug('Overflow/underflow');
shouldThrow('stepUp("2010-02-10T20:13Z", "3.40282346e+38", null)', invalidStateErr);
shouldThrow('stepDown("2010-02-10T20:13Z", "3.40282346e+38", null)', invalidStateErr);
shouldThrow('stepUp("2010-02-10T20:13Z", "1", "2010-02-10T20:13Z")', invalidStateErr);
shouldThrow('stepDown("2010-02-10T20:13Z", "1", "2010-02-10T20:13Z")', invalidStateErr);

debug('');
debug('Datetime-local type');
input.type = 'datetime-local';
debug('Invalid value');
shouldThrow('stepUp("", null, null)', invalidStateErr);
shouldThrow('stepDown("", null, null)', invalidStateErr);
debug('Non-number arguments');
shouldBe('stepUp("2010-02-10T20:13", null, null, "0")', '"2010-02-10T20:13"');
shouldBe('stepDown("2010-02-10T20:13", null, null, "0")', '"2010-02-10T20:13"');
shouldBe('stepUp("2010-02-10T20:13", null, null, "foo")', '"2010-02-10T20:13"');
shouldBe('stepDown("2010-02-10T20:13", null, null, "foo")', '"2010-02-10T20:13"');
shouldBe('stepUp("2010-02-10T20:13", null, null, null)', '"2010-02-10T20:13"');
shouldBe('stepDown("2010-02-10T20:13", null, null, null)', '"2010-02-10T20:13"');
debug('Normal cases');
shouldBe('stepUp("2010-02-10T20:13", null, null)', '"2010-02-10T20:14"');
shouldBe('stepDown("2010-02-10T20:13", null, null)', '"2010-02-10T20:12"');
shouldBe('stepUp("2010-02-10T20:13", null, null, 10)', '"2010-02-10T20:23"');
shouldBe('stepDown("2010-02-10T20:13", null, null, 11)', '"2010-02-10T20:02"');
shouldBe('stepUp("1970-01-01T20:13", "4", null, 2)', '"1970-01-01T20:13:08"');
shouldBe('stepDown("1970-01-01T20:13", "4", null, 3)', '"1970-01-01T20:12:48"');
debug('Step=any');
shouldThrow('stepUp("2010-02-10T20:13", "any", null)', invalidStateErr);
shouldThrow('stepDown("2010-02-10T20:13", "any", null)', invalidStateErr);
debug('Overflow/underflow');
shouldThrow('stepUp("2010-02-10T20:13", "3.40282346e+38", null)', invalidStateErr);
shouldThrow('stepDown("2010-02-10T20:13", "3.40282346e+38", null)', invalidStateErr);
shouldThrow('stepUp("2010-02-10T20:13", "1", "2010-02-10T20:13")', invalidStateErr);
shouldThrow('stepDown("2010-02-10T20:13", "1", "2010-02-10T20:13")', invalidStateErr);

debug('');
debug('Month type');
input.type = 'month';
debug('Invalid value');
shouldThrow('stepUp("", null, null)', invalidStateErr);
shouldThrow('stepDown("", null, null)', invalidStateErr);
debug('Non-number arguments');
shouldBe('stepUp("2010-02", null, null, "0")', '"2010-02"');
shouldBe('stepDown("2010-02", null, null, "0")', '"2010-02"');
shouldBe('stepUp("2010-02", null, null, "foo")', '"2010-02"');
shouldBe('stepDown("2010-02", null, null, "foo")', '"2010-02"');
shouldBe('stepUp("2010-02", null, null, null)', '"2010-02"');
shouldBe('stepDown("2010-02", null, null, null)', '"2010-02"');
debug('Normal cases');
shouldBe('stepUp("2010-02", null, null)', '"2010-03"');
shouldBe('stepDown("2010-02", null, null)', '"2010-01"');
shouldBe('stepUp("2010-02", null, null, 10)', '"2010-12"');
shouldBe('stepDown("2010-02", null, null, 11)', '"2009-03"');
shouldBe('stepUp("1970-01", "4", null, 2)', '"1970-09"');
shouldBe('stepDown("1970-01", "4", null, 3)', '"1969-01"');
debug('Step=any');
shouldThrow('stepUp("2010-02", "any", null)', invalidStateErr);
shouldThrow('stepDown("2010-02", "any", null)', invalidStateErr);
debug('Overflow/underflow');
shouldThrow('stepUp("2010-02", "3.40282346e+38", null)', invalidStateErr);
shouldThrow('stepDown("2010-02", "3.40282346e+38", null)', invalidStateErr);
shouldThrow('stepUp("2010-02", "1", "2010-02")', invalidStateErr);
shouldThrow('stepDown("2010-02", "1", "2010-02")', invalidStateErr);

debug('');
debug('Number type');
input.type = 'number';
debug('Invalid value');
shouldThrow('stepUp("", null, null)', invalidStateErr);
shouldThrow('stepDown("", null, null)', invalidStateErr);
debug('Non-number arguments');
shouldBe('stepUp("0", null, null, "0")', '"0"');
shouldBe('stepDown("0", null, null, "0")', '"0"');
shouldBe('stepUp("0", null, null, "foo")', '"0"');
shouldBe('stepDown("0", null, null, "foo")', '"0"');
shouldBe('stepUp("0", null, null, null)', '"0"');
shouldBe('stepDown("0", null, null, null)', '"0"');
debug('Normal cases');
shouldBe('stepUp("0", null, null)', '"1"');
shouldBe('stepUp("1", null, null, 2)', '"3"');
shouldBe('stepUp("3", null, null, -1)', '"2"');
shouldBe('stepDown("2", null, null)', '"1"');
shouldBe('stepDown("1", null, null, 2)', '"-1"');
shouldBe('stepDown("-1", null, null, -1)', '"0"');
debug('Extra arguments');
shouldBe('input.value = "0"; input.min = null; input.step = null; input.stepUp(1, 2); input.value', '"1"');
shouldBe('input.value = "1"; input.stepDown(1, 3); input.value', '"0"');
debug('Invalid step value');
shouldBe('stepUp("0", "foo", null)', '"1"');
shouldBe('stepUp("1", "0", null)', '"2"');
shouldBe('stepUp("2", "-1", null)', '"3"');
debug('Step=any');
shouldThrow('stepUp("0", "any", null)', invalidStateErr);
shouldThrow('stepDown("0", "any", null)', invalidStateErr);
debug('Step=any corner case');
shouldThrow('stepUpExplicitBounds("0", "100", "any", "1.5", "1")', invalidStateErr);
shouldThrow('stepDownExplicitBounds("0", "100", "any", "1.5", "1")', invalidStateErr);
debug('Overflow/underflow');
shouldBe('stepDown("1", "1", "0")', '"0"');
shouldThrow('stepDown("0", "1", "0")', invalidStateErr);
shouldThrow('stepDown("1", "1", "0", 2)', invalidStateErr);
shouldBe('input.value', '"1"');
shouldThrow('stepDown("1", "3.40282346e+38", "", 2)', invalidStateErr);
shouldBe('stepUp("-1", "1", "0")', '"0"');
shouldThrow('stepUp("0", "1", "0")', invalidStateErr);
shouldThrow('stepUp("-1", "1", "0", 2)', invalidStateErr);
shouldBe('input.value', '"-1"');
shouldThrow('stepUp("1", "3.40282346e+38", "", 2)', invalidStateErr);
debug('stepDown()/stepUp() for stepMismatch values');
shouldBe('stepUp("1", "2", "")', '"3"');
shouldBe('input.stepDown(); input.value', '"1"');
shouldBe('input.min = "0"; stepUp("9", "10", "", 9)', '"99"');
shouldBe('stepDown("19", "10", "0")', '"9"');
shouldBe('stepUp("89", "10", "99")', '"99"');
debug('Huge value and small step');
shouldBe('input.min = ""; stepUp("1e+38", "1", "", 999999)', '"1e+38"');
shouldBe('input.max = ""; stepDown("1e+38", "1", "", 999999)', '"1e+38"');
debug('Fractional numbers');
shouldBe('input.min = ""; stepUp("0", "0.33333333333333333", "", 3)', '"1"');
shouldBe('stepUp("1", "0.1", "", 10)', '"2"');
shouldBe('input.stepUp(); input.stepUp(); input.stepUp(); input.stepUp(); input.stepUp(); input.stepUp(); input.stepUp(); input.stepUp(); input.stepUp(); input.stepUp(); input.value', '"3"');
shouldBe('input.min = "0"; stepUp("0", "0.003921568627450980", "1", 255)', '"1"');
shouldBe('for (var i = 0; i < 255; i++) { input.stepDown(); }; input.value', '"0"');
debug('Rounding');
shouldBe('stepUp("5.005", "0.005", "", 2)', '"5.015"');
shouldBe('stepUp("5.005", "0.005", "", 11)', '"5.06"');
shouldBe('stepUp("5.005", "0.005", "", 12)', '"5.065"');
shouldBe('stepUpExplicitBounds("4", "9", "0.005", "5.005", 2)', '"5.015"');
shouldBe('stepUpExplicitBounds("4", "9", "0.005", "5.005", 11)', '"5.06"');
shouldBe('stepUpExplicitBounds("4", "9", "0.005", "5.005", 12)', '"5.065"');

debug('');
debug('Range type');
input.type = 'range';
debug('function arguments are (min, max, step, value, [stepCount])');
debug('Using the default values');
shouldBe('stepUpExplicitBounds(null, null, null, "")', '"51"');
shouldBe('stepDownExplicitBounds(null, null, null, "")', '"49"');
debug('Non-number arguments (stepCount)');
shouldBe('stepUpExplicitBounds(null, null, null, "0", "0")', '"0"');
shouldBe('stepDownExplicitBounds(null, null, null, "0", "0")', '"0"');
shouldBe('stepUpExplicitBounds(null, null, null, "0", "foo")', '"0"');
shouldBe('stepDownExplicitBounds(null, null, null, "0", "foo")', '"0"');
shouldBe('stepUpExplicitBounds(null, null, null, "0", null)', '"0"');
shouldBe('stepDownExplicitBounds(null, null, null, "0", null)', '"0"');
debug('Normal cases');
shouldBe('stepUpExplicitBounds(null, null, null, "0")', '"1"');
shouldBe('stepUpExplicitBounds(null, null, null, "1", 2)', '"3"');
shouldBe('stepUpExplicitBounds(null, null, null, "3", -1)', '"2"');
shouldBe('stepDownExplicitBounds("-100", null, null, "2")', '"1"');
shouldBe('stepDownExplicitBounds("-100", null, null, "1", 2)', '"-1"');
shouldBe('stepDownExplicitBounds("-100", null, null, "-1", -1)', '"0"');
debug('Extra arguments');
shouldBe('setInputAttributes(null, null, null, "0"); input.stepUp(1,2); input.value', '"1"');
shouldBe('setInputAttributes(null, null, null, "1"); input.stepDown(1,3); input.value', '"0"');
debug('Invalid step value');
shouldBe('stepUpExplicitBounds(null, null, "foo", "0")', '"1"');
shouldBe('stepUpExplicitBounds(null, null, "0", "1")', '"2"');
shouldBe('stepUpExplicitBounds(null, null, "-1", "2")', '"3"');
shouldBe('stepDownExplicitBounds(null, null, "foo", "1")', '"0"');
shouldBe('stepDownExplicitBounds(null, null, "0", "2")', '"1"');
shouldBe('stepDownExplicitBounds(null, null, "-1", "3")', '"2"');
debug('Step=any');
shouldThrow('stepUpExplicitBounds(null, null, "any", "1")', invalidStateErr);
shouldThrow('stepDownExplicitBounds(null, null, "any", "1")', invalidStateErr);
debug('Overflow/underflow');
shouldBe('stepUpExplicitBounds(null, "100", "1", "99")', '"100"');
shouldThrow('stepUpExplicitBounds(null, "100", "1", "100")', invalidStateErr);
shouldBe('input.value', '"100"');
shouldThrow('stepUpExplicitBounds(null, "100", "1", "99", "2")', invalidStateErr);
shouldBe('input.value', '"99"');
shouldBe('stepDownExplicitBounds("0", null, "1", "1")', '"0"');
shouldThrow('stepDownExplicitBounds("0", null, "1", "0")', invalidStateErr);
shouldBe('input.value', '"0"');
shouldThrow('stepDownExplicitBounds("0", null, "1", "1", "2")', invalidStateErr);
shouldBe('input.value', '"1"');
shouldThrow('stepDownExplicitBounds(null, null, "3.40282346e+38", "1", "2")', invalidStateErr);
shouldBe('stepUpExplicitBounds(-100, 0, 1, -1)', '"0"');
shouldThrow('stepUpExplicitBounds(null, 0, 1, 0)', invalidStateErr);
shouldThrow('stepUpExplicitBounds(-100, 0, 1, -1, 2)', invalidStateErr);
shouldBe('input.value', '"-1"');
shouldThrow('stepUpExplicitBounds(null, null, "3.40282346e+38", "1", "2")', invalidStateErr);
debug('stepDown()/stepUp() for stepMismatch values');
shouldBe('stepUpExplicitBounds(null, null, 2, 1)', '"4"');
shouldBe('input.stepDown(); input.value', '"2"');
shouldBe('stepUpExplicitBounds(0, null, 10, 9, 9)', '"100"');
shouldBe('stepDownExplicitBounds(0, null, 10, 19)', '"10"');
debug('value + step is <= max, but rounded result would be > max.');
shouldThrow('stepUpExplicitBounds(null, 99, 10, 89)', invalidStateErr);
debug('Huge value and small step');
shouldBe('stepUpExplicitBounds(0, 1e38, 1, 1e38, 999999)', '"1e+38"');
shouldBe('stepDownExplicitBounds(0, 1e38, 1, 1e38, 999999)', '"1e+38"');
debug('Fractional numbers');
shouldBe('stepUpExplicitBounds(null, null, 0.33333333333333333, 0, 3)', '"1"');
shouldBe('stepUpExplicitBounds(null, null, 0.1, 1)', '"1.1"');
shouldBe('stepUpExplicitBounds(null, null, 0.1, 1, 8)', '"1.8"');
shouldBe('stepUpExplicitBounds(null, null, 0.1, 1, 10)', '"2"');
shouldBe('input.stepUp(); input.stepUp(); input.stepUp(); input.stepUp(); input.stepUp(); input.stepUp(); input.stepUp(); input.stepUp(); input.stepUp(); input.stepUp(); input.value', '"3"');
shouldBe('stepUpExplicitBounds(0, 1, 0.003921568627450980, 0, 255)', '"1"');
shouldBe('for (var i = 0; i < 255; i++) { input.stepDown(); }; input.value', '"0"');
shouldBe('stepDownExplicitBounds(null, null, 0.1, 1, 8)', '"0.2"');
shouldBe('stepDownExplicitBounds(null, null, 0.1, 1)', '"0.9"');

debug('');
debug('Time type');
input.type = 'time';
debug('Invalid value');
shouldThrow('stepUp("", null, null)', invalidStateErr);
shouldThrow('stepDown("", null, null)', invalidStateErr);
debug('Non-number arguments');
shouldBe('stepUp("20:13", null, null, "0")', '"20:13"');
shouldBe('stepDown("20:13", null, null, "0")', '"20:13"');
shouldBe('stepUp("20:13", null, null, "foo")', '"20:13"');
shouldBe('stepDown("20:13", null, null, "foo")', '"20:13"');
shouldBe('stepUp("20:13", null, null, null)', '"20:13"');
shouldBe('stepDown("20:13", null, null, null)', '"20:13"');
debug('Normal cases');
shouldBe('stepUp("20:13", null, null)', '"20:14"');
shouldBe('stepDown("20:13", null, null)', '"20:12"');
shouldBe('stepUp("20:13", null, null, 10)', '"20:23"');
shouldBe('stepDown("20:13", null, null, 11)', '"20:02"');
shouldBe('stepUp("20:13", "4", null, 2)', '"20:13:08"');
shouldBe('stepDown("20:13", "4", null, 3)', '"20:12:48"');
debug('Step=any');
shouldThrow('stepUp("20:13", "any", null)', invalidStateErr);
shouldThrow('stepDown("20:13", "any", null)', invalidStateErr);
debug('Overflow/underflow');
shouldThrow('stepUp("20:13", "3.40282346e+38", null)', invalidStateErr);
shouldThrow('stepDown("20:13", "3.40282346e+38", null)', invalidStateErr);
shouldThrow('stepUp("20:13", "1", "20:13")', invalidStateErr);
shouldThrow('stepDown("20:13", "1", "20:13")', invalidStateErr);
shouldThrow('stepUp("23:59", null, null)', invalidStateErr);
shouldThrow('stepDown("00:00", null, null)', invalidStateErr);

debug('');
debug('Unsupported type');
shouldThrow('input.step = "3"; input.min = ""; input.max = ""; input.value = "2"; input.stepDown()', '"Error: INVALID_STATE_ERR: DOM Exception 11"');
shouldThrow('input.stepDown(0)', '"Error: INVALID_STATE_ERR: DOM Exception 11"');
shouldThrow('input.stepUp()', '"Error: INVALID_STATE_ERR: DOM Exception 11"');
shouldThrow('input.stepUp(0)', '"Error: INVALID_STATE_ERR: DOM Exception 11"');

var successfullyParsed = true;
