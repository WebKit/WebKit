description('Tests stepDown()/stepUp() for type=number.');

var input = document.createElement('input');
input.type = 'number';
var invalidStateErr = '"Error: INVALID_STATE_ERR: DOM Exception 11"';

debug('Tests for an invalid current value:');
input.value = '';
shouldThrow('input.stepDown()', invalidStateErr);
shouldThrow('input.stepUp()', invalidStateErr);

// Non-number argument
debug('stepDown("foo") with value=0:');
input.value = '0';
input.stepDown("foo");
shouldBe('input.value', '"0"');

debug('stepUp("foo") with value=0:');
input.stepUp("foo");
shouldBe('input.value', '"0"');

debug('stepDown(null) with value=0:');
input.stepDown(null);
shouldBe('input.value', '"0"');

debug('stepUp(null) with value=0:');
input.stepUp(null);
shouldBe('input.value', '"0"');

debug('stepDown(undefined) with value=0:');
input.stepDown(undefined);
shouldBe('input.value', '"0"');

debug('stepUp(undefined) with value=0:');
input.stepUp(undefined);
shouldBe('input.value', '"0"');

// Default step value for type=number is 1.
debug('stepUp() with value=0:');
input.value = '0';
input.stepUp();
shouldBe('input.value', '"1"');

debug('stepUp(2) with value=1:');
input.stepUp(2);
shouldBe('input.value', '"3"');

debug('stepUp(-1) with value=3:');
input.stepUp(-1);
shouldBe('input.value', '"2"');

debug('stepDown(-1) with value=2:');
input.stepDown();
shouldBe('input.value', '"1"');

debug('stepDown(2) with value=1:');
input.stepDown(2);
shouldBe('input.value', '"-1"');

debug('stepDown(-1) with value=-1:');
input.stepDown(-1);
shouldBe('input.value', '"0"');

// Extra arguments are ignored.
debug('stepUp(1, 2) with value=0:');
input.stepUp(1, 2);
shouldBe('input.value', '"1"');

debug('stepUp(1, 3) with value=1:');
input.stepDown(1, 3);
shouldBe('input.value', '"0"');

// The default value is used for invalid step values.
debug('stepUp() with value=0 step=foo:');
input.value = '0';
input.step = 'foo';
input.stepUp();
shouldBe('input.value', '"1"');

debug('stepUp() with value=1 step=0:');
input.step = '0';
input.stepUp();
shouldBe('input.value', '"2"');

debug('stepUp() with value=2 step=-1:');
input.step = '-1';
input.stepUp();
shouldBe('input.value', '"3"');

// No step value
debug('stepDown() and stepUp() with value=0 step=any:');
input.value = '0';
input.step = 'any';
shouldThrow('input.stepDown()', invalidStateErr);
shouldThrow('input.stepUp()', invalidStateErr);

// Minimum limit
debug('stepDown() with value=1 min=0 step=1:');
input.min = '0';
input.step = '1';
input.value = '1';
input.stepDown();
shouldBe('input.value', '"0"');
debug('stepDown() with value=0 min=0 step=1:');
shouldThrow('input.stepDown()', invalidStateErr);

debug('stepDown(2) with value=1 min=0 step=1:');
input.value = '1';
shouldThrow('input.stepDown(2)', invalidStateErr);
shouldBe('input.value', '"1"');

// Should not be -Infinity.
debug('stepDown(2) with value=1 step=DBL_MAX:');
input.value = '1';
input.min = '';
input.step = '1.7976931348623156e+308';
shouldThrow('input.stepDown(2)', invalidStateErr);

// Maximum limit
debug('stepUp() with value=-1 max=0 step=1:');
input.value = '-1';
input.min = '';
input.max = '0';
input.step = '1';
input.stepUp();
shouldBe('input.value', '"0"');
debug('stepUp() with value=0 max=0 step=1:');
shouldThrow('input.stepUp()', invalidStateErr);

debug('stepUp(2) with value=-1 max=0 step=1:');
input.value = '-1';
shouldThrow('input.stepUp(2)', invalidStateErr);
shouldBe('input.value', '"-1"');

// Should not be Infinity.
debug('stepUp(2) with value=1 step=DBL_MAX:');
input.value = '1';
input.max = '';
input.step = '1.7976931348623156e+308';
shouldThrow('input.stepUp(2)', invalidStateErr);

// stepDown()/stepUp() for stepMismatch values
debug('stepUp() with value=0 min=-1 step=2:');
input.value = '0';
input.min = '-1';
input.step = '2';
input.stepUp();
shouldBe('input.value', '"3"');
input.stepDown();
shouldBe('input.value', '"1"');

debug('stepUp(9) with value=9 min=0 step=10:');
input.value = '9';
input.min = '0';
input.max = '';
input.step = '10';
input.stepUp(9);
shouldBe('input.value', '"100"');

debug('stepDown() with value=19 min=0 step=10:');
input.value = '19';
input.stepDown();
shouldBe('input.value', '"10"');

// value + step is <= max, but rounded result would be > max.
debug('stepUp() with value=89 min=0 max=99 step=10:');
input.value = '89';
input.max = '99';
shouldThrow('input.stepUp()', invalidStateErr);

// Huge value and small step
debug('stepUp(999999) with value=1e+308 step=1:');
input.value = '1e+308';
input.min = '';
input.max = '';
input.step = '1';
input.stepUp(999999);
shouldBe('input.value', '"1e+308"');
input.stepDown(999999);
shouldBe('input.value', '"1e+308"');

// Fractional numbers
debug('stepUp(3) with value=0 step=0.33333333333333333:');
input.value = '0';
input.min = '';
input.max = '';
input.step = '0.33333333333333333';
input.stepUp(3);
shouldBe('input.value', '"1"');

debug('stepUp(10) with value=1 step=0.1:');
input.step = '0.1';
input.stepUp(10);
shouldBe('input.value', '"2"');
debug('stepUp() x10 with value=2 step=0.1:');
for (var i = 0; i < 10; i++)
    input.stepUp();
shouldBe('input.value', '"3"');

debug('stepUp(255) with value=0 min=0 max=1 step=0.003921568627450980:');
input.value = '0';
input.min = '0';
input.max = '1';
input.step = '0.003921568627450980';
input.stepUp(255);
shouldBe('input.value', '"1"');
debug('stepDown() x255 with value=1 min=0 max=1 step=0.003921568627450980:');
for (var i = 0; i < 255; i++)
    input.stepDown();
shouldBe('input.value', '"0"');

var successfullyParsed = true;
