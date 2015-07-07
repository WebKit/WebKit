description('Various tests for the figcaption element.');

function getStyleValue(id, propertyName) {
    return document.defaultView.getComputedStyle(document.getElementById(id), null).getPropertyValue(propertyName);
}

var testParent = document.createElement('div');
document.body.appendChild(testParent);

debug('&lt;figcaption> default styling:');
testParent.innerHTML = '<figcaption id="figcaption0">element</figure>';
var emSize = getStyleValue("figcaption0","font-size");
shouldBe('getStyleValue("figcaption0","display")', '"block"');

debug('&lt;figcaption> closes &lt;p>:');
testParent.innerHTML = '<p>Test that <figcaption id="figcaption1">a figcaption element</figcaption> closes &lt;p>.</p>';
var figcaption1 = document.getElementById('figcaption1');
shouldBeFalse('figcaption1.parentNode.nodeName == "p"');

debug('&lt;p> does not close &lt;figcaption>:');
testParent.innerHTML = '<figcaption>Test that <p id="p1">a p element</p> does not close a figcaption element.</figcaption>';
var p1 = document.getElementById('p1');
shouldBe('p1.parentNode.nodeName', '"FIGCAPTION"');

debug('&lt;figcaption> can be nested inside &lt;figcaption> or &lt;footer>:');
testParent.innerHTML = '<figcaption id="figcaption2">Test that <figcaption id="figcaption3">a figcaption element</figcaption> can be nested inside another figcaption element.</figcaption>';
var figcaption3 = document.getElementById('figcaption3');
shouldBe('figcaption3.parentNode.id', '"figcaption2"');
testParent.innerHTML = '<footer id="footer1">Test that <figcaption id="figcaption5">a figcaption element</figcaption> can be nested inside a footer element.</footer>';
var figcaption5 = document.getElementById('figcaption5');
shouldBe('figcaption5.parentNode.id', '"footer1"');

debug('Residual style:');
testParent.innerHTML = '<b><figcaption id="figcaption4">This text should be bold.</figcaption> <span id="span1">This is also bold.</span></b>';
shouldBe('getStyleValue("figcaption4","font-weight")', '"bold"');
shouldBe('getStyleValue("span1","font-weight")', '"bold"');
document.body.removeChild(testParent);

