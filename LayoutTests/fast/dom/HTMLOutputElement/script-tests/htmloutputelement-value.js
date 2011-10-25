description('Tests for assigning the value attribute to output elements.');

var output;
var childNode;

debug('- Sets the defaultValue attribute with the value mode flag is in mode "defalut".');
output = document.createElement('output');
output.defaultValue = "defaultValue";
shouldBeEqualToString('output.defaultValue', 'defaultValue');
shouldBeEqualToString('output.value', 'defaultValue');
shouldBeEqualToString('output.innerText', 'defaultValue');
shouldBeEqualToString('output.innerHTML', 'defaultValue');

debug('- Sets the value attribute. This will change the value mode flag from "default" to "value".');

output.value = 'aValue';
shouldBeEqualToString('output.defaultValue', 'defaultValue');
shouldBeEqualToString('output.value', 'aValue');
shouldBeEqualToString('output.innerText', 'aValue');
shouldBeEqualToString('output.innerHTML', 'aValue');

debug('- Sets the defaultValue attribute with the value mode flag is in mode "value".');
output.defaultValue = 'another defaultValue';
shouldBeEqualToString('output.defaultValue', 'another defaultValue');
shouldBeEqualToString('output.value', 'aValue');
shouldBeEqualToString('output.innerText', 'aValue');
shouldBeEqualToString('output.innerHTML', 'aValue');

debug('- Ensures that setting text to the value attribute works as setTextContent().');
output.value = '<strong>A <span style=\"color: red;\">strong</span> text</strong>';
shouldBe('output.value', '\'<strong>A <span style="color: red;">strong</span> text</strong>\'');
shouldBe('output.innerText', '\'<strong>A <span style="color: red;">strong</span> text</strong>\'');
shouldBe('output.innerHTML', '\'&lt;strong&gt;A &lt;span style="color: red;"&gt;strong&lt;/span&gt; text&lt;/strong&gt;\'');

debug('- Sets the innerText attribute with the value mode flag is in mode "default".');
output = document.createElement('output');
output.innerText = 'text';
shouldBeEqualToString('output.defaultValue', 'text');
shouldBeEqualToString('output.value', 'text');
shouldBeEqualToString('output.innerText', 'text');
shouldBeEqualToString('output.innerHTML', 'text');

output.innerText = '<strong>strong</strong> text';
shouldBeEqualToString('output.defaultValue', '<strong>strong</strong> text');
shouldBeEqualToString('output.value', '<strong>strong</strong> text');
shouldBeEqualToString('output.innerText', '<strong>strong</strong> text');
shouldBeEqualToString('output.innerHTML', '&lt;strong&gt;strong&lt;/strong&gt; text');

debug('- Sets the innerText attribute with the value mode flag is in mode "value".');
output = document.createElement('output');
output.value = 'aValue';
output.defaultValue = 'defaultValue';
output.innerText = 'text';
shouldBeEqualToString('output.defaultValue', 'defaultValue');
shouldBeEqualToString('output.value', 'text');
shouldBeEqualToString('output.innerText', 'text');
shouldBeEqualToString('output.innerHTML', 'text');

debug('- Sets the innerHTML attribute with the value mode flag is in mode "default".');
output = document.createElement('output');
output.innerHTML = 'text';
shouldBeEqualToString('output.defaultValue', 'text');
shouldBeEqualToString('output.value', 'text');
shouldBeEqualToString('output.innerText', 'text');
shouldBeEqualToString('output.innerHTML', 'text');

output.innerHTML = '<strong>strong</strong> text';
shouldBeEqualToString('output.defaultValue', 'strong text');
shouldBeEqualToString('output.value', 'strong text');
shouldBeEqualToString('output.innerText', 'strong text');
shouldBeEqualToString('output.innerHTML', '<strong>strong</strong> text');

debug('- Sets the innerHTML attribute with the value mode flag is in mode "value".');
output = document.createElement('output');
output.value = 'aValue';
output.defaultValue = 'defaultValue';
output.innerHTML = 'text';
shouldBeEqualToString('output.defaultValue', 'defaultValue');
shouldBeEqualToString('output.value', 'text');
shouldBeEqualToString('output.innerText', 'text');
shouldBeEqualToString('output.innerHTML', 'text');

output.innerHTML = '<strong>strong</strong> text';
shouldBeEqualToString('output.defaultValue', 'defaultValue');
shouldBeEqualToString('output.value', 'strong text');
shouldBeEqualToString('output.innerText', 'strong text');
shouldBeEqualToString('output.innerHTML', '<strong>strong</strong> text');

debug('- Appends a child node to the output element with the value mode flag is in mode "default".');
output = document.createElement('output');
childNode = document.createElement('span');
childNode.innerText = 'childText';
output.appendChild(childNode);
shouldBeEqualToString('output.defaultValue', 'childText');
shouldBeEqualToString('output.value', 'childText');
shouldBeEqualToString('output.innerText', 'childText');
shouldBeEqualToString('output.innerHTML', '<span>childText</span>');
debug('- Then removes the child node from the output element with the value mode flag is in mode "default".');
output.removeChild(childNode);
shouldBeEqualToString('output.defaultValue', '');
shouldBeEqualToString('output.value', '');
shouldBeEqualToString('output.innerText', '');
shouldBeEqualToString('output.innerHTML', '');

debug('- Appends a child node to the output element with the value mode flag is in mode "value".');
output = document.createElement('output');
output.value = 'aValue';
output.defaultValue = 'defaultValue';
childNode = document.createElement('span');
childNode.innerText = ' and childText';
output.appendChild(childNode);
shouldBeEqualToString('output.defaultValue', 'defaultValue');
shouldBeEqualToString('output.value', 'aValue and childText');
shouldBeEqualToString('output.innerText', 'aValue and childText');
shouldBeEqualToString('output.innerHTML', 'aValue<span> and childText</span>');
debug('- Then removes the child node from the output element with the value mode flag is in mode "default".');
output.removeChild(childNode);
shouldBeEqualToString('output.defaultValue', 'defaultValue');
shouldBeEqualToString('output.value', 'aValue');
shouldBeEqualToString('output.innerText', 'aValue');
shouldBeEqualToString('output.innerHTML', 'aValue');
