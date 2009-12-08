description('Tests stepDown()/stepUp() for unsupported types');

var input = document.createElement('input');
input.type = 'text';
input.step = '3';
input.value = '2';
shouldThrow('input.stepDown()', '"Error: INVALID_STATE_ERR: DOM Exception 11"');
shouldThrow('input.stepDown(0)', '"Error: INVALID_STATE_ERR: DOM Exception 11"');
shouldThrow('input.stepUp()', '"Error: INVALID_STATE_ERR: DOM Exception 11"');
shouldThrow('input.stepUp(0)', '"Error: INVALID_STATE_ERR: DOM Exception 11"');

var successfullyParsed = true;
