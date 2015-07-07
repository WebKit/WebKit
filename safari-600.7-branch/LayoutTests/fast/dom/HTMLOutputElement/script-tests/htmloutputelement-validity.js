description('Tests for the validation APIs of output elements.');

var container;
var output;

container = document.createElement('div');
document.body.appendChild(container);

debug('- Ensures whether the willValidate is defined and it always returns false.');
container.innerHTML = '<form><output id="outputElement">aValue</output></form>';
output = document.getElementById('outputElement');
shouldBe('typeof output.willValidate', '"boolean"');
shouldBeFalse('output.willValidate');
container.innerHTML = '<output id="outputElement">aValue</output>';
output = document.getElementById('outputElement');
shouldBeFalse('output.willValidate');

debug('- Ensures validity is always "valid" and validationMessage() always returns an empty string.');
container.innerHTML = '<form><output id="outputElement">aValue</output></form>';
output = document.getElementById('outputElement');
shouldBeEqualToString('output.validationMessage', '');
shouldBeTrue('output.checkValidity()');
output.setCustomValidity('This should not be affected.');
shouldBeEqualToString('output.validationMessage', '');
shouldBeTrue('output.checkValidity()');
