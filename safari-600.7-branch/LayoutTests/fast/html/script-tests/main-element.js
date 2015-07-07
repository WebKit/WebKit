description('Various tests for the main element.');

var testParent = document.createElement('div');
document.body.appendChild(testParent);

debug('&lt;main> closes &lt;p>:');
testParent.innerHTML = '<p>Test that <main id="main1">an main element</main> closes &lt;p>.</p>';
var main1 = document.getElementById('main1');
shouldBeFalse('main1.parentNode.nodeName == "p"');

debug('&lt;p> does not close &lt;main>:');
testParent.innerHTML = '<main>Test that <p id="p1">a p element</p> does not close an main element.</main>';
var p1 = document.getElementById('p1');
shouldBe('p1.parentNode.nodeName', '"MAIN"');

debug('&lt;main> can be nested inside &lt;main>:');
testParent.innerHTML = '<main id="main2">Test that <main id="main3">an main element</main> can be nested inside another.</main>';
var main3 = document.getElementById('main3');
shouldBe('main3.parentNode.id', '"main2"');

debug('Residual style:');
testParent.innerHTML = '<b><main id="main4">This text should be bold.</main> <span id="span1">This is also bold.</span></b>';
function getWeight(id) {
    return document.defaultView.getComputedStyle(document.getElementById(id), null).getPropertyValue('font-weight');
}
shouldBe('getWeight("main4")', '"bold"');
shouldBe('getWeight("span1")', '"bold"');
document.body.removeChild(testParent);

debug('FormatBlock:');
var editable = document.createElement('div');
editable.innerHTML = '[<span id="span2">The text will be a child of &lt;main>.</span>]';
document.body.appendChild(editable);
editable.contentEditable = true;
var selection = window.getSelection();
selection.selectAllChildren(editable);
document.execCommand('FormatBlock', false, 'main');
selection.collapse();
shouldBe('document.getElementById("span2").parentNode.nodeName', '"MAIN"');
document.body.removeChild(editable);

