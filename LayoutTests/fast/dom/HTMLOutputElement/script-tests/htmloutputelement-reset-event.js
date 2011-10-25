description('A Test for sending a reset event to output elements.');

var form = document.createElement('form');
var output = document.createElement('output');
output.defaultValue = 'defaultValue';
form.appendChild(output);

debug('- Sets the value attribute of the output element.');
output.value = 'aValue';
shouldBeEqualToString('output.defaultValue', 'defaultValue');
shouldBeEqualToString('output.value', 'aValue');
shouldBeEqualToString('output.innerText', 'aValue');
shouldBeEqualToString('output.innerHTML', 'aValue');

debug('- Sends a reset event to reset the value to the default value.');
form.reset();
shouldBeEqualToString('output.defaultValue', 'defaultValue');
shouldBeEqualToString('output.value', 'defaultValue');
shouldBeEqualToString('output.innerText', 'defaultValue');
shouldBeEqualToString('output.innerHTML', 'defaultValue');

debug('- Ensures that the value mode flags is in mode "default".');
output.defaultValue = 'another defaultValue';
shouldBeEqualToString('output.defaultValue', 'another defaultValue');
shouldBeEqualToString('output.value', 'another defaultValue');
shouldBeEqualToString('output.innerText', 'another defaultValue');
shouldBeEqualToString('output.innerHTML', 'another defaultValue');
