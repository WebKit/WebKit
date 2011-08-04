description('Check stepping-up and -down for <input> from renderer. No cases of empty initial values for type=date, datetime, datetime-local, month, time.');

var input = document.createElement('input');
var invalidStateErr = '"Error: INVALID_STATE_ERR: DOM Exception 11"';

function sendKey(keyName) {
    var event = document.createEvent('KeyboardEvent');
    event.initKeyboardEvent('keydown', true, true, document.defaultView, keyName);
    input.dispatchEvent(event);
}

function setInputAttributes(min, max, step, value) {
    input.min = min;
    input.max = max;
    input.step = step;
    input.value = value;
}

function stepUp(value, step, max, optionalStepCount) {
    setInputAttributes(null, max, step, value);
    if (typeof optionalStepCount != "undefined")
        if (optionalStepCount < 0)
            for (var i = 0; i < -optionalStepCount; i++)
                sendKey('Down');
        else
            for (var i = 0; i < optionalStepCount; i++)
                sendKey('Up');
    else
        sendKey('Up');
    return input.value;
}

function stepDown(value, step, min, optionalStepCount) {
    setInputAttributes(min, null, step, value);
    if (typeof optionalStepCount != "undefined")
        if (optionalStepCount < 0)
            for (var i = 0; i < -optionalStepCount; i++)
                sendKey('Up');
        else
            for (var i = 0; i < optionalStepCount; i++)
                sendKey('Down');
    else
        sendKey('Down');
    return input.value;
}

// Range value gets automatically shifted based on bounds,
// So always set the min and max first to get expected behavior

function stepUpExplicitBounds(min, max, step, value, stepCount) {
    setInputAttributes(min, max, step, value);
    if (typeof stepCount !== 'undefined')
        if (stepCount < 0) {
            for (var i = 0; i < -stepCount; i++)
                sendKey('Down');
        } else {
            for (var i = 0; i < stepCount; i++)
                sendKey('Up');
        }
    else
        sendKey('Up');
    return input.value;
}

function stepDownExplicitBounds(min, max, step, value, stepCount) {
    setInputAttributes(min, max, step, value);
    if (typeof stepCount !== 'undefined')
        if (stepCount < 0) {
            for (var i = 0; i < -stepCount; i++)
                sendKey('Up');
        } else {
            for (var i = 0; i < stepCount; i++)
                sendKey('Down');
        }
    else
        sendKey('Down');
    return input.value;
}

debug('Date type');
input.type = 'date';
debug('Function arguments are (value, step, {min or max}, [stepCount]).');
debug('Normal cases');
shouldBe('stepUp("2010-02-10", null, null)', '"2010-02-11"');
shouldBe('stepDown("2010-02-10", null, null)', '"2010-02-09"');
shouldBe('stepUp("2010-02-10", null, null, 10)', '"2010-02-20"');
shouldBe('stepDown("2010-02-10", null, null, 11)', '"2010-01-30"');
shouldBe('stepUp("1970-01-01", "4", null, 2)', '"1970-01-09"');
shouldBe('stepDown("1970-01-01", "4", null, 3)', '"1969-12-20"');
debug('Step=any');
shouldBe('stepUp("2010-02-10", "any", null)', '"2010-02-11"');
shouldBe('stepDown("2010-02-10", "any", null)', '"2010-02-09"');
debug('Overflow/underflow');
shouldBe('stepUp("2010-02-10", "3.40282346e+38", null)','"275760-09-13"');
shouldBe('stepDown("2010-02-10", "3.40282346e+38", null)', '"1970-01-01"');
shouldBe('stepUp("2010-02-10", "1", "2010-02-10")', '"2010-02-10"');
shouldBe('stepDown("2010-02-10", "1", "2010-02-10")', '"2010-02-10"');
debug('stepDown()/stepUp() for stepMismatch values');
shouldBe('stepDown("2010-02-10", "3", "2010-02-06")', '"2010-02-09"');
shouldBe('stepUp("1970-01-02", "2", "")', '"1970-01-03"');

debug('');
debug('Datetime type');
input.type = 'datetime';
debug('Function arguments are (value, step, {min or max}, [stepCount]).');
debug('Normal cases');
shouldBe('stepUp("2010-02-10T20:13Z", null, null)', '"2010-02-10T20:14Z"');
shouldBe('stepDown("2010-02-10T20:13Z", null, null)', '"2010-02-10T20:12Z"');
shouldBe('stepUp("2010-02-10T20:13Z", null, null, 10)', '"2010-02-10T20:23Z"');
shouldBe('stepDown("2010-02-10T20:13Z", null, null, 11)', '"2010-02-10T20:02Z"');
shouldBe('stepUp("1970-01-01T20:13Z", "4", null, 2)', '"1970-01-01T20:13:08Z"');
shouldBe('stepDown("1970-01-01T20:13Z", "4", null, 3)', '"1970-01-01T20:12:48Z"');
debug('Step=any');
shouldBe('stepUp("2010-02-10T20:13Z", "any", null)', '"2010-02-10T20:14Z"');
shouldBe('stepDown("2010-02-10T20:13Z", "any", null)', '"2010-02-10T20:12Z"');
debug('Overflow/underflow');
shouldBe('stepUp("2010-02-10T20:13Z", "3.40282346e+38", null)', '"275760-09-13T00:00:00.000Z"');
shouldBe('stepDown("2010-02-10T20:13Z", "3.40282346e+38", null)', '"1970-01-01T00:00:00.000Z"');
shouldBe('stepUp("2010-02-10T20:13Z", "1", "2010-02-10T20:13Z")', '"2010-02-10T20:13Z"');
shouldBe('stepDown("2010-02-10T20:13Z", "1", "2010-02-10T20:13Z")', '"2010-02-10T20:13Z"');
debug('stepDown()/stepUp() for stepMismatch values');
shouldBe('stepDown("2010-02-10T20:13Z", "3", "2010-02-10T20:12:56Z")', '"2010-02-10T20:12:59Z"');
shouldBe('stepUp("1970-01-01T00:13Z", "7", "")', '"1970-01-01T00:13:04Z"');

debug('');
debug('Datetime-local type');
input.type = 'datetime-local';
debug('Function arguments are (value, step, {min or max}, [stepCount]).');
debug('Normal cases');
shouldBe('stepUp("2010-02-10T20:13", null, null)', '"2010-02-10T20:14"');
shouldBe('stepDown("2010-02-10T20:13", null, null)', '"2010-02-10T20:12"');
shouldBe('stepUp("2010-02-10T20:13", null, null, 10)', '"2010-02-10T20:23"');
shouldBe('stepDown("2010-02-10T20:13", null, null, 11)', '"2010-02-10T20:02"');
shouldBe('stepUp("1970-01-01T20:13", "4", null, 2)', '"1970-01-01T20:13:08"');
shouldBe('stepDown("1970-01-01T20:13", "4", null, 3)', '"1970-01-01T20:12:48"');
debug('Step=any');
shouldBe('stepUp("2010-02-10T20:13", "any", null)', '"2010-02-10T20:14"');
shouldBe('stepDown("2010-02-10T20:13", "any", null)', '"2010-02-10T20:12"');
debug('Overflow/underflow');
shouldBe('stepUp("2010-02-10T20:13", "3.40282346e+38", null)', '"275760-09-13T00:00:00.000"');
shouldBe('stepDown("2010-02-10T20:13", "3.40282346e+38", null)', '"1970-01-01T00:00:00.000"');
shouldBe('stepUp("2010-02-10T20:13", "1", "2010-02-10T20:13")', '"2010-02-10T20:13"');
shouldBe('stepDown("2010-02-10T20:13", "1", "2010-02-10T20:13")', '"2010-02-10T20:13"');
debug('stepDown()/stepUp() for stepMismatch values');
shouldBe('stepDown("2010-02-10T20:13", "3", "2010-02-10T20:12:56")', '"2010-02-10T20:12:59"');
shouldBe('stepUp("1970-01-01T00:13", "7", "")', '"1970-01-01T00:13:04"');

debug('');
debug('Month type');
input.type = 'month';
debug('Function arguments are (value, step, {min or max}, [stepCount]).');
debug('Normal cases');
shouldBe('stepUp("2010-02", null, null)', '"2010-03"');
shouldBe('stepDown("2010-02", null, null)', '"2010-01"');
shouldBe('stepUp("2010-02", null, null, 10)', '"2010-12"');
shouldBe('stepDown("2010-02", null, null, 11)', '"2009-03"');
shouldBe('stepUp("1970-01", "4", null, 2)', '"1970-09"');
shouldBe('stepDown("1970-01", "4", null, 3)', '"1969-01"');
debug('Step=any');
shouldBe('stepUp("2010-02", "any", null)', '"2010-03"');
shouldBe('stepDown("2010-02", "any", null)', '"2010-01"');
debug('Overflow/underflow');
shouldBe('stepUp("2010-02", "3.40282346e+38", null)', '"275760-09"');
shouldBe('stepDown("2010-02", "3.40282346e+38", null)', '"1970-01"');
shouldBe('stepUp("2010-02", "1", "2010-02")', '"2010-02"');
shouldBe('stepDown("2010-02", "1", "2010-02")', '"2010-02"');
debug('stepDown()/stepUp() for stepMismatch values');
shouldBe('stepDown("2010-02", "3", "2009-10")', '"2010-01"');
shouldBe('stepUp("1970-02", "4", "")', '"1970-05"');

debug('');
debug('Number type');
input.type = 'number';
debug('Function arguments are (value, step, {min or max}, [stepCount]).');
debug('Invalid value');
shouldBe('stepUp("", null, null)', '"1"');
shouldBe('stepDown("", null, null)', '"-1"');
shouldBe('stepUp("", "any", null)', '"1"');
shouldBe('stepDown("", "any", null)', '"-1"');
shouldBe('stepUp("", "foo", null)', '"1"');
shouldBe('stepDown("", "foo", null)', '"-1"');
shouldBe('stepUp("foo", null, null)', '"1"');
shouldBe('stepDown("foo", null, null)', '"-1"');
shouldBe('stepUp("foo", "any", null)', '"1"');
shouldBe('stepDown("foo", "any", null)', '"-1"');
shouldBe('stepUp("foo", "foo", null)', '"1"');
shouldBe('stepDown("foo", "foo", null)', '"-1"');
debug('Normal cases');
shouldBe('stepUp("0", null, null)', '"1"');
shouldBe('stepUp("1", null, null, 2)', '"3"');
shouldBe('stepUp("3", null, null, -1)', '"2"');
shouldBe('stepDown("2", null, null)', '"1"');
shouldBe('stepDown("1", null, null, 2)', '"-1"');
shouldBe('stepDown("-1", null, null, -1)', '"0"');
debug('Invalid step value');
shouldBe('stepUp("0", "foo", null)', '"1"');
shouldBe('stepUp("1", "0", null)', '"2"');
shouldBe('stepUp("2", "-1", null)', '"3"');
debug('Step=any');
shouldBe('stepUp("0", "any", null)', '"1"');
shouldBe('stepDown("0", "any", null)', '"-1"');
debug('Step=any corner case');
shouldBe('stepUpExplicitBounds("0", "100", "any", "1.5", "1")', '"2.5"');
shouldBe('stepDownExplicitBounds("0", "100", "any", "1.5", "1")', '"0.5"');
debug('Overflow/underflow');
shouldBe('stepDown("1", "1", "0")', '"0"');
shouldBe('stepDown("0", "1", "0")', '"0"');
shouldBe('stepDown("1", "1", "0", 2)', '"0"');
shouldBe('stepDown("1", "3.40282346e+38", "", 2)', '"-3.40282346e+38"');
shouldBe('stepUp("-1", "1", "0")', '"0"');
shouldBe('stepUp("0", "1", "0")', '"0"');
shouldBe('stepUp("-1", "1", "0", 2)', '"0"');
shouldBe('stepUp("1", "3.40282346e+38", "", 2)', '"3.40282346e+38"');
debug('stepDown()/stepUp() for stepMismatch values');
shouldBe('stepUp("1", "2", "")', '"2"');
shouldBe('input.min = "0"; stepUp("9", "10", "")', '"10"');
shouldBe('stepDown("19", "10", "0")', '"10"');
shouldBe('stepUp("89", "10", "99")', '"90"');
debug('Huge value and small step');
shouldBe('input.min = ""; stepUp("1e+38", "1", "", 999)', '"1e+38"');
shouldBe('input.max = ""; stepDown("1e+38", "1", "", 999)', '"1e+38"');
debug('Fractional numbers');
shouldBe('input.min = ""; stepUp("0", "0.33333333333333333", "", 3)', '"1"');
shouldBe('stepUp("1", "0.1", "", 10)', '"2"');
shouldBe('input.min = "0"; stepUp("0", "0.003921568627450980", "1", 255)', '"1"');
debug('Rounding');
shouldBe('stepUp("5.005", "0.005", "", 2)', '"5.015"');
shouldBe('stepUp("5.005", "0.005", "", 11)', '"5.06"');
shouldBe('stepUp("5.005", "0.005", "", 12)', '"5.065"');
shouldBe('stepUpExplicitBounds("4", "9", "0.005", "5.005", 2)', '"5.015"');
shouldBe('stepUpExplicitBounds("4", "9", "0.005", "5.005", 11)', '"5.06"');
shouldBe('stepUpExplicitBounds("4", "9", "0.005", "5.005", 12)', '"5.065"');
shouldBe('stepUpExplicitBounds(-4, 4, 1, "")', '"1"');
shouldBe('stepDownExplicitBounds(-4, 4, 1, "")', '"-1"');
shouldBe('stepDownExplicitBounds(0, 4, 1, "")', '"0"');
shouldBe('stepUpExplicitBounds(-4, 0, 1, "")', '"0"');
shouldBe('stepDownExplicitBounds(1, 4, 1, "")', '"1"');
shouldBe('stepUpExplicitBounds(1, 4, 1, "")', '"1"');
shouldBe('stepDownExplicitBounds(-4, -1, 1, "")', '"-1"');
shouldBe('stepUpExplicitBounds(-4, -1, 1, "")', '"-1"');
shouldBe('stepUpExplicitBounds(-100, null, 3, "")', '"2"');
shouldBe('stepDownExplicitBounds(-100, null, 3, "")', '"-1"');
shouldBe('stepUpExplicitBounds(1, 4, 1, 0)', '"1"');
shouldBe('stepDownExplicitBounds(1, 4, 1, 0)', '"0"');
shouldBe('stepDownExplicitBounds(-4, -1, 1, 0)', '"-1"');
shouldBe('stepUpExplicitBounds(-4, -1, 1, 0)', '"0"');
shouldBe('stepUpExplicitBounds(-100, null, 3, 3)', '"5"');
shouldBe('stepDownExplicitBounds(-100, null, 3, 3)', '"2"');

debug('');
debug('Range type');
input.type = 'range';
debug('Function arguments are (min, max, step, value, [stepCount]).');
debug('Using the default values');
shouldBe('stepUpExplicitBounds(null, null, null, "")', '"51"');
shouldBe('stepDownExplicitBounds(null, null, null, "")', '"49"');
shouldBe('stepUpExplicitBounds(null, null, "any", "")', '"51"');
shouldBe('stepDownExplicitBounds(null, null, "any", "")', '"49"');
shouldBe('stepUpExplicitBounds(null, null, "foo", "")', '"51"');
shouldBe('stepDownExplicitBounds(null, null, "foo", "")', '"49"');
shouldBe('stepUpExplicitBounds(null, null, null, "foo")', '"51"');
shouldBe('stepDownExplicitBounds(null, null, null, "foo")', '"49"');
shouldBe('stepUpExplicitBounds(null, null, "any", "foo")', '"51"');
shouldBe('stepDownExplicitBounds(null, null, "any", "foo")', '"49"');
shouldBe('stepUpExplicitBounds(null, null, "foo", "foo")', '"51"');
shouldBe('stepDownExplicitBounds(null, null, "foo", "foo")', '"49"');
debug('Normal cases');
shouldBe('stepUpExplicitBounds(null, null, null, "0")', '"1"');
shouldBe('stepUpExplicitBounds(null, null, null, "1", 2)', '"3"');
shouldBe('stepUpExplicitBounds(null, null, null, "3", -1)', '"2"');
shouldBe('stepDownExplicitBounds("-100", null, null, "2")', '"1"');
shouldBe('stepDownExplicitBounds("-100", null, null, "1", 2)', '"-1"');
shouldBe('stepDownExplicitBounds("-100", null, null, "-1", -1)', '"0"');
debug('Invalid step value');
shouldBe('stepUpExplicitBounds(null, null, "foo", "0")', '"1"');
shouldBe('stepUpExplicitBounds(null, null, "0", "1")', '"2"');
shouldBe('stepUpExplicitBounds(null, null, "-1", "2")', '"3"');
shouldBe('stepDownExplicitBounds(null, null, "foo", "1")', '"0"');
shouldBe('stepDownExplicitBounds(null, null, "0", "2")', '"1"');
shouldBe('stepDownExplicitBounds(null, null, "-1", "3")', '"2"');
debug('Step=any');
shouldBe('stepUpExplicitBounds(null, null, "any", "1")', '"2"');
shouldBe('stepDownExplicitBounds(null, null, "any", "1")', '"0"');
debug('Overflow/underflow');
shouldBe('stepUpExplicitBounds(null, "100", "1", "99")', '"100"');
shouldBe('stepUpExplicitBounds(null, "100", "1", "100")', '"100"');
shouldBe('stepUpExplicitBounds(null, "100", "1", "99", 2)', '"100"');
shouldBe('stepDownExplicitBounds("0", null, "1", "1")', '"0"');
shouldBe('stepDownExplicitBounds("0", null, "1", "0")', '"0"');
shouldBe('stepDownExplicitBounds("0", null, "1", "1", 2)', '"0"');
shouldBe('stepDownExplicitBounds(null, null, "3.40282346e+38", "1", 2)', '"0"');
shouldBe('stepUpExplicitBounds(-100, 0, 1, -1)', '"0"');
shouldBe('stepUpExplicitBounds(null, 0, 1, 0)', '"0"');
shouldBe('stepUpExplicitBounds(-100, 0, 1, -1, 2)', '"0"');
shouldBe('stepUpExplicitBounds(null, null, "3.40282346e+38", "1", 2)', '"0"');
debug('stepDown()/stepUp() for stepMismatch values');
shouldBe('stepUpExplicitBounds(null, null, 2, 1)', '"4"');
shouldBe('stepUpExplicitBounds(0, null, 10, 9, 9)', '"100"');
shouldBe('stepDownExplicitBounds(0, null, 10, 19)', '"10"');
debug('value + step is <= max, but rounded result would be > max.');
shouldBe('stepUpExplicitBounds(null, 99, 10, 89)', '"90"');
debug('Huge value and small step');
shouldBe('stepUpExplicitBounds(0, 1e38, 1, 1e38, 999)', '"1e+38"');
shouldBe('stepDownExplicitBounds(0, 1e38, 1, 1e38, 999)', '"1e+38"');
debug('Fractional numbers');
shouldBe('stepUpExplicitBounds(null, null, 0.33333333333333333, 0, 3)', '"1"');
shouldBe('stepUpExplicitBounds(null, null, 0.1, 1)', '"1.1"');
shouldBe('stepUpExplicitBounds(null, null, 0.1, 1, 8)', '"1.8"');
shouldBe('stepUpExplicitBounds(null, null, 0.1, 1, 10)', '"2"');
shouldBe('stepUpExplicitBounds(0, 1, 0.003921568627450980, 0, 255)', '"1"');
shouldBe('stepDownExplicitBounds(null, null, 0.1, 1, 8)', '"0.2"');
shouldBe('stepDownExplicitBounds(null, null, 0.1, 1)', '"0.9"');

debug('');
debug('Time type');
input.type = 'time';
debug('Function arguments are (value, step, {min or max}, [stepCount]).');
debug('Normal cases');
shouldBe('stepUp("20:13", null, null)', '"20:14"');
shouldBe('stepDown("20:13", null, null)', '"20:12"');
shouldBe('stepUp("20:13", null, null, 10)', '"20:23"');
shouldBe('stepDown("20:13", null, null, 11)', '"20:02"');
shouldBe('stepUp("20:13", "4", null, 2)', '"20:13:08"');
shouldBe('stepDown("20:13", "4", null, 3)', '"20:12:48"');
debug('Step=any');
shouldBe('stepUp("20:13", "any", null)', '"20:14"');
shouldBe('stepDown("20:13", "any", null)', '"20:12"');
debug('Overflow/underflow');
shouldBe('stepUp("20:13", "3.40282346e+38", null)', '"23:59:59.999"');
shouldBe('stepDown("20:13", "3.40282346e+38", null)', '"00:00:00.000"');
shouldBe('stepUp("20:13", "1", "20:13")', '"20:13"');
shouldBe('stepDown("20:13", "1", "20:13")', '"20:13"');
shouldBe('stepUp("23:59", null, null)', '"23:59"');
shouldBe('stepDown("00:00", null, null)', '"00:00"');
debug('stepDown()/stepUp() for stepMismatch values');
shouldBe('stepDown("20:13", "3", "20:12:56")', '"20:12:59"');
shouldBe('stepUp("00:13", "7", "")', '"00:13:04"');

debug('');
var successfullyParsed = true;
