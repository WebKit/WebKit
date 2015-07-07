description('Various tests for the aside element.');

var testParent = document.createElement('div');
document.body.appendChild(testParent);

debug('&lt;aside> closes &lt;p>:');
testParent.innerHTML = '<p>Test that <aside id="aside1">an aside element</aside> closes &lt;p>.</p>';
var aside1 = document.getElementById('aside1');
shouldBeFalse('aside1.parentNode.nodeName == "p"');

debug('&lt;p> does not close &lt;aside>:');
testParent.innerHTML = '<aside>Test that <p id="p1">a p element</p> does not close an aside element.</aside>';
var p1 = document.getElementById('p1');
shouldBe('p1.parentNode.nodeName', '"ASIDE"');

debug('&lt;aside> can be nested inside &lt;aside>:');
testParent.innerHTML = '<aside id="aside2">Test that <aside id="aside3">an aside element</aside> can be nested inside another.</aside>';
var aside3 = document.getElementById('aside3');
shouldBe('aside3.parentNode.id', '"aside2"');

debug('Residual style:');
testParent.innerHTML = '<b><aside id="aside4">This text should be bold.</aside> <span id="span1">This is also bold.</span></b>';
function getWeight(id) {
    return document.defaultView.getComputedStyle(document.getElementById(id), null).getPropertyValue('font-weight');
}
shouldBe('getWeight("aside4")', '"bold"');
shouldBe('getWeight("span1")', '"bold"');
document.body.removeChild(testParent);

debug('FormatBlock:');
var editable = document.createElement('div');
editable.innerHTML = '[<span id="span2">The text will be a child of &lt;aside>.</span>]';
document.body.appendChild(editable);
editable.contentEditable = true;
var selection = window.getSelection();
selection.selectAllChildren(editable);
document.execCommand('FormatBlock', false, 'aside');
selection.collapse();
shouldBe('document.getElementById("span2").parentNode.nodeName', '"ASIDE"');
document.body.removeChild(editable);

