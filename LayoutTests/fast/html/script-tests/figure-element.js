description('Various tests for the figure element.');

function getStyleValue(id, propertyName) {
    return document.defaultView.getComputedStyle(document.getElementById(id), null).getPropertyValue(propertyName);
}

var testParent = document.createElement('div');
document.body.appendChild(testParent);

debug('&lt;figure> default styling:');
testParent.innerHTML = '<figure id="figure0">element</figure>';
var emSize = getStyleValue("figure0","font-size");
shouldBe('getStyleValue("figure0","display")', '"block"');
shouldBe('getStyleValue("figure0","margin-top")', 'emSize');
shouldBe('getStyleValue("figure0","margin-right")', '"40px"');
shouldBe('getStyleValue("figure0","margin-bottom")', 'emSize');
shouldBe('getStyleValue("figure0","margin-left")', '"40px"');

debug('&lt;figure> closes &lt;p>:');
testParent.innerHTML = '<p>Test that <figure id="figure1">a figure element</figure> closes &lt;p>.</p>';
var figure1 = document.getElementById('figure1');
shouldBeFalse('figure1.parentNode.nodeName == "p"');

debug('&lt;p> does not close &lt;figure>:');
testParent.innerHTML = '<figure>Test that <p id="p1">a p element</p> does not close a figure element.</figure>';
var p1 = document.getElementById('p1');
shouldBe('p1.parentNode.nodeName', '"FIGURE"');

debug('&lt;figure> can be nested inside &lt;figure> or &lt;footer>:');
testParent.innerHTML = '<figure id="figure2">Test that <figure id="figure3">a figure element</figure> can be nested inside another figure element.</figure>';
var figure3 = document.getElementById('figure3');
shouldBe('figure3.parentNode.id', '"figure2"');
testParent.innerHTML = '<footer id="footer1">Test that <figure id="figure5">a figure element</figure> can be nested inside a footer element.</footer>';
var figure5 = document.getElementById('figure5');
shouldBe('figure5.parentNode.id', '"footer1"');

debug('Residual style:');
testParent.innerHTML = '<b><figure id="figure4">This text should be bold.</figure> <span id="span1">This is also bold.</span></b>';
shouldBe('getStyleValue("figure4","font-weight")', '"bold"');
shouldBe('getStyleValue("span1","font-weight")', '"bold"');
document.body.removeChild(testParent);

