// Test for WebKit Bug 15136 - HTML5 spec violation: </h1> doesn't end <h3> element in Webkit
// https://bugs.webkit.org/show_bug.cgi?id=15136
// rdar://problem/5762882

description('Test that any numbered header element end tag can close any other open numbered header element.');

var testParent = document.createElement('div');
testParent.id = 'test0';
document.body.appendChild(testParent);

// h1

debug('&lt;h1> closes &lt;h1>:');
testParent.innerHTML = '<h1 id="test1"></h1><div id="test2"></div><p>Test that &lt;h1> closes &lt;h1.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h2> closes &lt;h1>:');
testParent.innerHTML = '<h1 id="test1"></h2><div id="test2"></div><p>Test that &lt;h2> closes &lt;h1.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h3> closes &lt;h1>:');
testParent.innerHTML = '<h1 id="test1"></h3><div id="test2"></div><p>Test that &lt;h3> closes &lt;h1.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h4> closes &lt;h1>:');
testParent.innerHTML = '<h1 id="test1"></h4><div id="test2"></div><p>Test that &lt;h4> closes &lt;h1.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h5> closes &lt;h1>:');
testParent.innerHTML = '<h1 id="test1"></h5><div id="test2"></div><p>Test that &lt;h5> closes &lt;h1.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h6> closes &lt;h1>:');
testParent.innerHTML = '<h1 id="test1"></h6><div id="test2"></div><p>Test that &lt;h6> closes &lt;h1.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

// h2

debug('&lt;h1> closes &lt;h2>:');
testParent.innerHTML = '<h2 id="test1"></h1><div id="test2"></div><p>Test that &lt;h1> closes &lt;h2.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h2> closes &lt;h2>:');
testParent.innerHTML = '<h2 id="test1"></h2><div id="test2"></div><p>Test that &lt;h2> closes &lt;h2.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h3> closes &lt;h2>:');
testParent.innerHTML = '<h2 id="test1"></h3><div id="test2"></div><p>Test that &lt;h3> closes &lt;h2.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h4> closes &lt;h2>:');
testParent.innerHTML = '<h2 id="test1"></h4><div id="test2"></div><p>Test that &lt;h4> closes &lt;h2.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h5> closes &lt;h2>:');
testParent.innerHTML = '<h2 id="test1"></h5><div id="test2"></div><p>Test that &lt;h5> closes &lt;h2.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h6> closes &lt;h2>:');
testParent.innerHTML = '<h2 id="test1"></h6><div id="test2"></div><p>Test that &lt;h6> closes &lt;h2.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

// h3

debug('&lt;h1> closes &lt;h3>:');
testParent.innerHTML = '<h3 id="test1"></h1><div id="test2"></div><p>Test that &lt;h1> closes &lt;h3.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h2> closes &lt;h3>:');
testParent.innerHTML = '<h3 id="test1"></h2><div id="test2"></div><p>Test that &lt;h2> closes &lt;h3.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h3> closes &lt;h3>:');
testParent.innerHTML = '<h3 id="test1"></h3><div id="test2"></div><p>Test that &lt;h3> closes &lt;h3.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h4> closes &lt;h3>:');
testParent.innerHTML = '<h3 id="test1"></h4><div id="test2"></div><p>Test that &lt;h4> closes &lt;h3.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h5> closes &lt;h3>:');
testParent.innerHTML = '<h3 id="test1"></h5><div id="test2"></div><p>Test that &lt;h5> closes &lt;h3.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h6> closes &lt;h3>:');
testParent.innerHTML = '<h3 id="test1"></h6><div id="test2"></div><p>Test that &lt;h6> closes &lt;h3.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

// h4

debug('&lt;h1> closes &lt;h4>:');
testParent.innerHTML = '<h4 id="test1"></h1><div id="test2"></div><p>Test that &lt;h1> closes &lt;h4.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h2> closes &lt;h4>:');
testParent.innerHTML = '<h4 id="test1"></h2><div id="test2"></div><p>Test that &lt;h2> closes &lt;h4.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h3> closes &lt;h4>:');
testParent.innerHTML = '<h4 id="test1"></h3><div id="test2"></div><p>Test that &lt;h3> closes &lt;h4.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h4> closes &lt;h4>:');
testParent.innerHTML = '<h4 id="test1"></h4><div id="test2"></div><p>Test that &lt;h4> closes &lt;h4.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h5> closes &lt;h4>:');
testParent.innerHTML = '<h4 id="test1"></h5><div id="test2"></div><p>Test that &lt;h5> closes &lt;h4.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h6> closes &lt;h4>:');
testParent.innerHTML = '<h4 id="test1"></h6><div id="test2"></div><p>Test that &lt;h6> closes &lt;h4.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

// h5

debug('&lt;h1> closes &lt;h5>:');
testParent.innerHTML = '<h5 id="test1"></h1><div id="test2"></div><p>Test that &lt;h1> closes &lt;h5.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h2> closes &lt;h5>:');
testParent.innerHTML = '<h5 id="test1"></h2><div id="test2"></div><p>Test that &lt;h2> closes &lt;h5.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h3> closes &lt;h5>:');
testParent.innerHTML = '<h5 id="test1"></h3><div id="test2"></div><p>Test that &lt;h3> closes &lt;h5.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h4> closes &lt;h5>:');
testParent.innerHTML = '<h5 id="test1"></h4><div id="test2"></div><p>Test that &lt;h4> closes &lt;h5.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h5> closes &lt;h5>:');
testParent.innerHTML = '<h5 id="test1"></h5><div id="test2"></div><p>Test that &lt;h5> closes &lt;h5.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h6> closes &lt;h5>:');
testParent.innerHTML = '<h5 id="test1"></h6><div id="test2"></div><p>Test that &lt;h6> closes &lt;h5.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

// h6

debug('&lt;h1> closes &lt;h6>:');
testParent.innerHTML = '<h6 id="test1"></h1><div id="test2"></div><p>Test that &lt;h1> closes &lt;h6.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h2> closes &lt;h6>:');
testParent.innerHTML = '<h6 id="test1"></h2><div id="test2"></div><p>Test that &lt;h2> closes &lt;h6.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h3> closes &lt;h6>:');
testParent.innerHTML = '<h6 id="test1"></h3><div id="test2"></div><p>Test that &lt;h3> closes &lt;h6.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h4> closes &lt;h6>:');
testParent.innerHTML = '<h6 id="test1"></h4><div id="test2"></div><p>Test that &lt;h4> closes &lt;h6.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h5> closes &lt;h6>:');
testParent.innerHTML = '<h6 id="test1"></h5><div id="test2"></div><p>Test that &lt;h5> closes &lt;h6.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

debug('&lt;h6> closes &lt;h6>:');
testParent.innerHTML = '<h6 id="test1"></h6><div id="test2"></div><p>Test that &lt;h6> closes &lt;h6.</p>';
var h1 = document.getElementById('test2');
shouldBeFalse('test2.parentNode.id == "test1"');
shouldBeTrue('test2.parentNode.id == "test0"');

document.body.removeChild(testParent);

var successfullyParsed = true;
