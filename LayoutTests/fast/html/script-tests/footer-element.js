description('Various tests for the footer element.');

var testParent = document.createElement('div');
document.body.appendChild(testParent);

debug('&lt;footer> closes &lt;p>:');
testParent.innerHTML = '<p>Test that <footer id="footer1">a footer element</footer> closes &lt;p>.</p>';
var footer1 = document.getElementById('footer1');
shouldBeFalse('footer1.parentNode.nodeName == "p"');

debug('&lt;p> does not close &lt;footer>:');
testParent.innerHTML = '<footer>Test that <p id="p1">a p element</p> does not close a footer element.</footer>';
var p1 = document.getElementById('p1');
shouldBe('p1.parentNode.nodeName', '"FOOTER"');

debug('&lt;footer> can be nested inside &lt;footer> or &lt;header>:');
testParent.innerHTML = '<footer id="footer2">Test that <footer id="footer3">a footer element</footer> can be nested inside another footer element.</footer>';
var footer3 = document.getElementById('footer3');
shouldBe('footer3.parentNode.id', '"footer2"');
testParent.innerHTML = '<header id="header1">Test that <footer id="footer5">a footer element</footer> can be nested inside a header element.</header>';
var footer5 = document.getElementById('footer5');
shouldBe('footer5.parentNode.id', '"header1"');

debug('Residual style:');
testParent.innerHTML = '<b><footer id="footer4">This text should be bold.</footer> <span id="span1">This is also bold.</span></b>';
function getWeight(id) {
    return document.defaultView.getComputedStyle(document.getElementById(id), null).getPropertyValue('font-weight');
}
shouldBe('getWeight("footer4")', '"bold"');
shouldBe('getWeight("span1")', '"bold"');
document.body.removeChild(testParent);

debug('FormatBlock:');
var editable = document.createElement('div');
editable.innerHTML = '[<span id="span2">The text will be a child of &lt;footer>.</span>]';
document.body.appendChild(editable);
editable.contentEditable = true;
var selection = window.getSelection();
selection.selectAllChildren(editable);
document.execCommand('FormatBlock', false, 'footer');
selection.collapse();
shouldBe('document.getElementById("span2").parentNode.nodeName', '"FOOTER"');
document.body.removeChild(editable);

