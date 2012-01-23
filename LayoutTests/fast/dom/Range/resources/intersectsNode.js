description(
"This test checks the behavior of the intersectsNode() method on the Range object.<br>" +
"It covers all configurations of the node/Range relationship and some exception conditions."
);

var range = document.createRange();
var intersects = 0;

debug("1.1 Node starts before the range and ends before the range");
range.selectNode(document.getElementById("a2"));
intersects = range.intersectsNode(document.getElementById("b1"));
shouldBeFalse("intersects");
debug("");

debug("1.2 Node starts before the range, and range ends on a 1");
range.setStart(document.getElementById("b2"), 1);
range.setEnd(document.getElementById("c2"), 1);
intersects = range.intersectsNode(document.getElementById("b2"));
shouldBeTrue("intersects");
debug("");

debug("1.3 Node starts before the range and ends within the range");
range.setStart(document.getElementById("b2"), 0);
range.setEnd(document.getElementById("c3"), 0);
intersects = range.intersectsNode(document.getElementById("a2"));
shouldBeTrue("intersects");
debug("");

debug("1.4 Range starts on 0, and node starts before range and ends in range");
range.setStart(document.getElementById("b2"), 0);
range.setEnd(document.getElementById("c2"), 1);
intersects = range.intersectsNode(document.getElementById("b2"));
shouldBeTrue("intersects");
debug("");
            
debug("1.5 Node starts and ends in range");
range.selectNode(document.getElementById("a2"));
intersects = range.intersectsNode(document.getElementById("b2"));
shouldBeTrue("intersects");
debug("");

debug("1.6 Node starts in the range, and the range ends on 1");
range.setStart(document.getElementById("b1"), 1);
range.setEnd(document.getElementById("c2"), 1);
intersects = range.intersectsNode( document.getElementById("c2"));
shouldBeTrue("intersects");
debug("");

debug("1.7 Node starts in the range, and ends after the range");
range.setStart(document.getElementById("b1"), 1);
range.setEnd(document.getElementById("c2"), 0);
intersects = range.intersectsNode( document.getElementById("c2"));
shouldBeTrue("intersects");
debug("");

debug("1.8 Range start on 1, node starts in range and ends after");
range.setStart(document.getElementById("b2"), 1);
range.setEnd(document.getElementById("c2"), 0);
intersects = range.intersectsNode( document.getElementById("c2"));
shouldBeTrue("intersects");
debug("");

debug("1.9 Node starts on range start and ends on range end");
range.selectNode(document.getElementById("a2"));
intersects = range.intersectsNode(document.getElementById("a2"));
shouldBeTrue("intersects");
debug("");

debug("1.10 Node starts after range end and ends after range end");
range.selectNode(document.getElementById("b2"));
intersects = range.intersectsNode(document.getElementById("c3"));
shouldBeFalse("intersects");
debug("");

debug("1.11 Node starts before range start and ends after range end");
range.selectNode(document.getElementById("b2"));
intersects = range.intersectsNode(document.getElementById("a2"));
shouldBeTrue("intersects");
debug("");

debug("1.12 Node starts before range start and range begins and ends on 1");
range.setEnd(document.getElementById("c2"), 1);
range.setStart(document.getElementById("c2"), 1);
intersects = range.intersectsNode(document.getElementById("c2"));
shouldBeTrue("intersects");
debug("");

debug("1.13 Range starts at 0 and ends at 1");
range.setStart(document.getElementById("b2"), 0);
range.setEnd(document.getElementById("b2"), 1);
intersects = range.intersectsNode(document.getElementById("b2"));
shouldBeTrue("intersects");
debug("");

debug("2.1 Detached Range, attached node");
var detachedRange = document.createRange();
detachedRange.detach();
shouldThrow("detachedRange.intersectsNode(document.getElementById('a1'))", '"Error: INVALID_STATE_ERR: DOM Exception 11"');
debug("");

debug("2.2 attached range, detached node");
// firefox does not throw an exception but returns 0
range.selectNode(document.getElementById("a1"));
var node = document.getElementById("b1");
node.parentNode.removeChild(node);
intersects = range.intersectsNode(node);
shouldBeFalse("intersects");
debug("");

debug("2.3 Node has no parent");
range.selectNode(document.getElementById("a2"));
shouldThrow("range.intersectsNode(document)");
debug("");

debug("2.4 Range has no parent");
shouldThrow("range.selectNode(document)");
debug("");

debug("2.5 Wrong documents");
// firefox does not throw an exception here instead it returns 0
var src1 = "<html>\n<head>\n<body>\n<div id=f1>f1</div>\n</body>\n</head>\n<html>";
window.frames['frame1'].document.open("text/html", "replace");
window.frames['frame1'].document.write(src1);
window.frames['frame1'].document.close();

var src2 = "<html>\n<head>\n<body>\n<div id=f2>f2</div>\n</body>\n</head>\n<html>";
window.frames['frame2'].document.open("text/html", "replace");
window.frames['frame2'].document.write(src2);
window.frames['frame2'].document.close();

var framerange = window.frames['frame1'].document.createRange();    
var F1Div = window.frames['frame1'].document.getElementById("f1");
framerange.selectNode(F1Div);
    
intersects =framerange.intersectsNode(window.frames['frame2'].document.getElementById("f2"));
shouldBeFalse("intersects");
debug("");

debug("2.6 Node deleted");
range.selectNode(document.getElementById("a2"));
var node = null;
shouldThrow("range.intersectsNode(node)");
debug("");
    
if (window.layoutTestController)
    layoutTestController.dumpAsText();
