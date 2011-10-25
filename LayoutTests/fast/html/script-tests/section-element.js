description('Various tests for the section element.');

var testParent = document.createElement('div');
document.body.appendChild(testParent);

debug('&lt;section> closes &lt;p>:');
testParent.innerHTML = '<p>Test that <section id="section1">a section element</section> closes &lt;p>.</p>';
var section1 = document.getElementById('section1');
shouldBeFalse('section1.parentNode.nodeName == "p"');

debug('&lt;p> does not close &lt;section>:');
testParent.innerHTML = '<section>Test that <p id="p1">a p element</p> does not close a section element.</section>';
var p1 = document.getElementById('p1');
shouldBe('p1.parentNode.nodeName', '"SECTION"');

debug('&lt;section> can be nested inside &lt;section>:');
testParent.innerHTML = '<section id="section2">Test that <section id="section3">a section element</section> can be nested inside another.</section>';
var section3 = document.getElementById('section3');
shouldBe('section3.parentNode.id', '"section2"');

debug('Residual style:');
testParent.innerHTML = '<b><section id="section4">This text should be bold.</section> <span id="span1">This is also bold.</span></b>';
function getWeight(id) {
    return document.defaultView.getComputedStyle(document.getElementById(id), null).getPropertyValue('font-weight');
}
shouldBe('getWeight("section4")', '"bold"');
shouldBe('getWeight("span1")', '"bold"');
document.body.removeChild(testParent);

debug('FormatBlock:');
var editable = document.createElement('div');
editable.innerHTML = '[<span id="span2">The text will be a child of &lt;section>.</span>]';
document.body.appendChild(editable);
editable.contentEditable = true;
var selection = window.getSelection();
selection.selectAllChildren(editable);
document.execCommand('FormatBlock', false, 'section');
selection.collapse();
shouldBe('document.getElementById("span2").parentNode.nodeName', '"SECTION"');
document.body.removeChild(editable);

