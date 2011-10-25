description('Various tests for the hgroup element.');

var testParent = document.createElement('div');
document.body.appendChild(testParent);

debug('&lt;hgroup> closes &lt;p>:');
testParent.innerHTML = '<p>Test that <hgroup id="hgroup1"><h1>a hgroup element</h1></hgroup> closes &lt;p>.</p>';
var hgroup1 = document.getElementById('hgroup1');
shouldBeFalse('hgroup1.parentNode.nodeName == "p"');

debug('&lt;p> does not close &lt;hgroup>:');
testParent.innerHTML = '<hgroup>Test that <p id="p1">a p element</p> does not close a hgroup element.</hgroup>';
var p1 = document.getElementById('p1');
shouldBe('p1.parentNode.nodeName', '"HGROUP"');

// Note: hgroup *should* have only h1-h6 elements, but *can* have any elements.
debug('&lt;hgroup> can be nested inside &lt;hgroup>:');
testParent.innerHTML = '<hgroup id="hgroup2">Test that <hgroup id="hgroup3">a hgroup element</hgroup> can be nested inside another.</hgroup>';
var hgroup3 = document.getElementById('hgroup3');
shouldBe('hgroup3.parentNode.id', '"hgroup2"');

debug('Residual style:');
testParent.innerHTML = '<b><hgroup id="hgroup4"><h2>This text should be bold.</h2></hgroup> <span id="span1">This is also bold.</span></b>';
function getWeight(id) {
    return document.defaultView.getComputedStyle(document.getElementById(id), null).getPropertyValue('font-weight');
}
shouldBe('getWeight("hgroup4")', '"bold"');
shouldBe('getWeight("span1")', '"bold"');
document.body.removeChild(testParent);

debug('FormatBlock:');
var editable = document.createElement('div');
editable.innerHTML = '[<span id="span2">The text will be a child of &lt;hgroup>.</span>]';
document.body.appendChild(editable);
editable.contentEditable = true;
var selection = window.getSelection();
selection.selectAllChildren(editable);
document.execCommand('FormatBlock', false, 'hgroup');
selection.collapse();
shouldBe('document.getElementById("span2").parentNode.nodeName', '"HGROUP"');
document.body.removeChild(editable);

