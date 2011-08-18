description("This test checks the form attribute of the form-associated elements.");

var container = document.createElement('div');
document.body.appendChild(container);

debug('- Checks the existence of the form attribute for each form-associated elements.');
debug('FIXME: &lt;label&gt; doesn\'t support the form attribute for now.');
container.innerHTML = '<form id=owner></form>' +
    '<button name=victim form=owner />' +
    '<fieldset name=victim form=owner />' +
    '<input name=victim form=owner />' +
    '<keygen name=victim form=owner />' +
    '<label name=victim form=owner />' +
    '<meter name=victim form=owner />' +
    '<object name=victim form=owner></object>' +
    '<output name=victim form=owner />' +
    '<progress name=victim form=owner />' +
    '<select name=victim form=owner />' +
    '<textarea name=victim form=owner />';

var owner = document.getElementById('owner');
shouldBe('document.getElementsByTagName("button")[0].form', 'owner');
shouldBe('document.getElementsByTagName("fieldset")[0].form', 'owner');
shouldBe('document.getElementsByTagName("input")[0].form', 'owner');
shouldBe('document.getElementsByTagName("keygen")[0].form', 'owner');
shouldBe('document.getElementsByTagName("label")[0].form', 'owner');
shouldBe('document.getElementsByTagName("meter")[0].form', 'owner');
shouldBe('document.getElementsByTagName("object")[0].form', 'owner');
shouldBe('document.getElementsByTagName("output")[0].form', 'owner');
shouldBe('document.getElementsByTagName("progress")[0].form', 'owner');
shouldBe('document.getElementsByTagName("select")[0].form', 'owner');
shouldBe('document.getElementsByTagName("textarea")[0].form', 'owner');

debug('');
debug('- Ensures that the form attribute points the form owner even if the element is within another form element.');
container.innerHTML = '<form id=owner></form>' +
    '<form id=shouldNotBeOwner>' +
    '    <input id=inputElement name=victim form=owner />' +
    '</form>';
owner = document.getElementById('owner');
var inputElement = document.getElementById('inputElement');
shouldBe('inputElement.form', 'owner');

debug('');
debug('- Ensures that the form attribute of all form-associated element with or witout form attribute points the form owner.');
container.innerHTML = '<form id=owner>' +
    '    <input id=inputElement1 name=victim />' +
    '    <input id=inputElement2 name=victim form=owner />' +
    '    <input id=inputElement3 name=victim />' +
    '</form>';
owner = document.getElementById('owner');
var inputElement1 = document.getElementById('inputElement1');
var inputElement2 = document.getElementById('inputElement2');
var inputElement3 = document.getElementById('inputElement3');
shouldBe('inputElement1.form', 'owner');
shouldBe('inputElement2.form', 'owner');
shouldBe('inputElement3.form', 'owner');

debug('');
debug('- Ensures that the form attribute points the form owner even if the form element is nested another form element.');
debug('NOTE: It seems that nesting form elements is not allowed so we ensure each form-associated elements associate with the outmost form element.');
container.innerHTML = '<form id=owner>' +
    '    <form>' +
    '        <input id=inputElement1 name=victim form=owner />' +
    '        <input id=inputElement2 name=victim />' +
    '        <input id=inputElement3 name=victim form=owner />' +
    '    </form>' +
    '</form>';
owner = document.getElementById('owner');
inputElement1 = document.getElementById('inputElement1');
inputElement2 = document.getElementById('inputElement2');
inputElement3 = document.getElementById('inputElement3');
shouldBe('inputElement1.form', 'owner');
shouldBe('inputElement2.form', 'owner');
shouldBe('inputElement3.form', 'owner');

debug('');
debug('- Ensures whether the form owner is set correctly when the value of form attribute of a form-associated element changed.');
container.innerHTML = '<form id=form1></form>' +
    '<form id=form2></form>' +
    '<input id=inputElement name=victim form=form1 />' +
    '<object id=objectElement name=victim form=form1></object>';
var form1 = document.getElementById('form1');
var form2 = document.getElementById('form2');
inputElement = document.getElementById('inputElement');
shouldBe('inputElement.form', 'form1');
inputElement.attributes['form'].value = 'form2';
shouldBe('inputElement.form', 'form2');

// HTMLObjectElement has its own implementation of formAttr processing and so needs its own test.
objectElement = document.getElementById('objectElement');
shouldBe('objectElement.form', 'form1');
objectElement.attributes['form'].value = 'form2';
shouldBe('objectElement.form', 'form2');

debug('');
debug('- Ensures whether the form owner is set correctly when the value of form attribute is added/removed.');
container.innerHTML = '<form id=owner name=firstOwner></form>' +
    '<input id=inputElement name=victim />' +
    '<object id=objectElement name=victim></object>';
owner = document.getElementById('owner');
inputElement = document.getElementById('inputElement');
shouldBe('inputElement.form', 'null');
var formAttribute = document.createAttribute('form');
inputElement.setAttribute('form', 'owner');
shouldBe('inputElement.form', 'owner');
inputElement.removeAttribute('form');
shouldBe('inputElement.form', 'null');

// HTMLObjectElement has its own implementation of formAttr processing and so needs its own test.
objectElement = document.getElementById('objectElement');
shouldBe('objectElement.form', 'null');
objectElement.setAttribute('form', 'owner');
shouldBe('objectElement.form', 'owner');
objectElement.removeAttribute('form');
shouldBe('objectElement.form', 'null');

debug('');
debug('- Ensures whether the form owner is set correctly when the form owner is added/removed.');
container.innerHTML = '<form id=owner name=firstOwner></form>' +
    '<form id=owner name=secondOwner></form>' +
    '<input id=inputElement name=victim form=owner />';
owner = document.getElementById('owner');
shouldBeEqualToString('owner.name', 'firstOwner');
inputElement = document.getElementById('inputElement');
container.removeChild(owner);
owner = document.getElementById('owner');
shouldBeEqualToString('owner.name', 'secondOwner');
shouldBe('inputElement.form', 'owner');
container.removeChild(owner);
shouldBe('inputElement.form', 'null');
container.appendChild(owner);
shouldBe('inputElement.form', 'owner');

var successfullyParsed = true;
