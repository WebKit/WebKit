description('Various tests for the article element.');

var testParent = document.createElement('div');
document.body.appendChild(testParent);

debug('&lt;article> closes &lt;p>:');
testParent.innerHTML = '<p>Test that <article id="article1">an article element</article> closes &lt;p>.</p>';
var article1 = document.getElementById('article1');
shouldBeFalse('article1.parentNode.nodeName == "p"');

debug('&lt;p> does not close &lt;article>:');
testParent.innerHTML = '<article>Test that <p id="p1">a p element</p> does not close an article element.</article>';
var p1 = document.getElementById('p1');
shouldBe('p1.parentNode.nodeName', '"ARTICLE"');

debug('&lt;article> can be nested inside &lt;article>:');
testParent.innerHTML = '<article id="article2">Test that <article id="article3">an article element</article> can be nested inside another.</article>';
var article3 = document.getElementById('article3');
shouldBe('article3.parentNode.id', '"article2"');

debug('Residual style:');
testParent.innerHTML = '<b><article id="article4">This text should be bold.</article> <span id="span1">This is also bold.</span></b>';
function getWeight(id) {
    return document.defaultView.getComputedStyle(document.getElementById(id), null).getPropertyValue('font-weight');
}
shouldBe('getWeight("article4")', '"bold"');
shouldBe('getWeight("span1")', '"bold"');
document.body.removeChild(testParent);

debug('FormatBlock:');
var editable = document.createElement('div');
editable.innerHTML = '[<span id="span2">The text will be a child of &lt;article>.</span>]';
document.body.appendChild(editable);
editable.contentEditable = true;
var selection = window.getSelection();
selection.selectAllChildren(editable);
document.execCommand('FormatBlock', false, 'article');
selection.collapse();
shouldBe('document.getElementById("span2").parentNode.nodeName', '"ARTICLE"');
document.body.removeChild(editable);

