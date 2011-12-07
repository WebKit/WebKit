description('Check stepUp() and stepDown() bahevior for unsupported types.');

var input = document.createElement('input');
var invalidStateErr = '"Error: INVALID_STATE_ERR: DOM Exception 11"';

debug('Unsupported type');
input.type = 'text';
shouldThrow('input.step = "3"; input.min = ""; input.max = ""; input.value = "2"; input.stepDown()', '"Error: INVALID_STATE_ERR: DOM Exception 11"');
shouldThrow('input.stepDown(0)', '"Error: INVALID_STATE_ERR: DOM Exception 11"');
shouldThrow('input.stepUp()', '"Error: INVALID_STATE_ERR: DOM Exception 11"');
shouldThrow('input.stepUp(0)', '"Error: INVALID_STATE_ERR: DOM Exception 11"');
