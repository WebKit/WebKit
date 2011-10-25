description('Existence tests for .setCustomValidity property');

var parent = document.createElement('div');
document.body.appendChild(parent);

debug('Existence of .setCustomValidity');
parent.innerHTML = '<form>'
    + '<input name="victim"/>'
    + '<textarea name="victim"></textarea>'
    + '<fieldset name="victim">Test</fieldset>'
    + '<button name="victim">'
    + '<select name="victim"></select>'
    + '<output name="victim"></output>'
    + '<object name="victim"></object>'
    + '<keygen name="victim">'
    + '</form>';

shouldBe('typeof document.getElementsByTagName("input")[0].setCustomValidity', '"function"');
shouldBe('typeof document.getElementsByTagName("button")[0].setCustomValidity', '"function"');
shouldBe('typeof document.getElementsByTagName("fieldset")[0].setCustomValidity', '"function"');
shouldBe('typeof document.getElementsByTagName("select")[0].setCustomValidity', '"function"');
shouldBe('typeof document.getElementsByTagName("textarea")[0].setCustomValidity', '"function"');
shouldBe('typeof document.getElementsByTagName("output")[0].setCustomValidity', '"function"');
shouldBe('typeof document.getElementsByTagName("object")[0].setCustomValidity', '"function"');
shouldBe('typeof document.getElementsByTagName("keygen")[0].setCustomValidity', '"function"');
