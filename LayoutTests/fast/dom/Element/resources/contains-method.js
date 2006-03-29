description(
'This test checks behavior of Element.contains. <div id="test1">[test1 <span id="test2">[test 2]</span>]</div>'
);

var test1 = document.getElementById('test1');
var test1TextChild = document.getElementById('test1').firstChild;
var test2 = document.getElementById('test2');

shouldBeUndefined("document.contains");
shouldBeTrue("test1.contains(test2)");
shouldBeFalse("test1.contains(test1TextChild)");
shouldBeFalse("test1.contains(123)");
shouldBeFalse("test1.contains(null)");

var successfullyParsed = true;
