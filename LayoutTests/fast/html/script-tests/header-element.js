description('Various tests for the header element.');

var testParent = document.createElement('div');
document.body.appendChild(testParent);

debug('&lt;header> closes &lt;p>:');
testParent.innerHTML = '<p>Test that <header id="header1">a header element</header> closes &lt;p>.</p>';
var header1 = document.getElementById('header1');
shouldBeFalse('header1.parentNode.nodeName == "p"');

debug('&lt;p> does not close &lt;header>:');
testParent.innerHTML = '<header>Test that <p id="p1">a p element</p> does not close a header element.</header>';
var p1 = document.getElementById('p1');
shouldBe('p1.parentNode.nodeName', '"HEADER"');

debug('&lt;header> can be nested inside &lt;header> or &lt;footer>:');
testParent.innerHTML = '<header id="header2">Test that <header id="header3">a header element</header> can be nested inside another header element.</header>';
var header3 = document.getElementById('header3');
shouldBe('header3.parentNode.id', '"header2"');
testParent.innerHTML = '<footer id="footer1">Test that <header id="header5">a header element</header> can be nested inside a footer element.</footer>';
var header5 = document.getElementById('header5');
shouldBe('header5.parentNode.id', '"footer1"');

debug('Residual style:');
testParent.innerHTML = '<b><header id="header4">This text should be bold.</header> <span id="span1">This is also bold.</span></b>';
function getWeight(id) {
    return document.defaultView.getComputedStyle(document.getElementById(id), null).getPropertyValue('font-weight');
}
shouldBe('getWeight("header4")', '"bold"');
shouldBe('getWeight("span1")', '"bold"');
document.body.removeChild(testParent);

debug('FormatBlock:');
var editable = document.createElement('div');
editable.innerHTML = '[<span id="span2">The text will be a child of &lt;header>.</span>]';
document.body.appendChild(editable);
editable.contentEditable = true;
var selection = window.getSelection();
selection.selectAllChildren(editable);
document.execCommand('FormatBlock', false, 'header');
selection.collapse();
shouldBe('document.getElementById("span2").parentNode.nodeName', '"HEADER"');
document.body.removeChild(editable);

