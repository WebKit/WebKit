description('Various tests for the mark element.');

var testParent = document.createElement('div');
document.body.appendChild(testParent);

debug('&lt;p> closes &lt;mark>:');
testParent.innerHTML = '<mark>Test that <p id="paragraph1">a p element</p> closes &lt;mark>.</p>';
var paragraph1 = document.getElementById('paragraph1');
shouldBeFalse('paragraph1.parentNode.nodeName == "mark"');

debug('&lt;b> does not close &lt;mark>:');
testParent.innerHTML = '<mark>Test that <b id="b1">a b element</b> does not close a mark element.</mark>';
var b1 = document.getElementById('b1');
shouldBe('b1.parentNode.nodeName', '"MARK"');

debug('Residual style:');
testParent.innerHTML = '<b><mark id="mark2">This text should be bold.</mark> <span id="span1">This is also bold.</span></b>';
function getWeight(id) {
    return document.defaultView.getComputedStyle(document.getElementById(id), null).getPropertyValue('font-weight');
}
shouldBe('getWeight("mark2")', '"bold"');
shouldBe('getWeight("span1")', '"bold"');
document.body.removeChild(testParent);
