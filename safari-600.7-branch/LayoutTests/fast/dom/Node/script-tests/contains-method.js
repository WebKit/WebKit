description(
'This test checks behavior of Node.contains. <div id="test1">[test1 <span id="test2">[test 2]</span>]</div>'
);

var test1 = document.getElementById('test1');
var test1TextChild = document.getElementById('test1').firstChild;
var test2 = document.getElementById('test2');

shouldBeEqualToString("typeof document.contains", "function");
shouldBeTrue("test1.contains(test2)");
shouldBeTrue("test1.contains(test1TextChild)");
shouldBeTrue("document.contains(test1)");
shouldBeTrue("document.contains(test1TextChild)");
shouldBeTrue("document.contains(document)");
shouldBeTrue("test1.contains(test1)");
shouldBeTrue("test1TextChild.contains(test1TextChild)");
shouldBeFalse("test1.contains(123)");
shouldBeFalse("test1.contains(null)");

// Cross document tests
var iframe = document.createElement("iframe");

shouldBeFalse("document.contains(iframe)");  // Not in document
shouldBeTrue("iframe.contains(iframe)");

document.body.appendChild(iframe);
debug('iframe is now in document');
shouldBeTrue("document.contains(iframe)");

var iframeDoc = iframe.contentDocument;
var iframeBody = iframeDoc.body;

shouldBeFalse("document.contains(iframeDoc)");
shouldBeTrue("iframeDoc.contains(iframeBody)");
shouldBeFalse("document.contains(iframeBody)");
