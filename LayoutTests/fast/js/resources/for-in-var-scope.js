description(
"This tests that for/in statements properly scope a variable that's declared in one. "
+ "In previous versions of JavaScriptCore there were two bugs that caused problems. "
+ "First, the loop variable declaration would not be processed. "
+ "Second, the code to set the loop variable would incorrectly walk the scope chain even after setting the loop variable."
);

var i = "start i";
var j = "start j";

function func() {
    var object = new Object;
    object.propName = "propValue";
    for (var i in object) { j = i; }
}
func();

shouldBe("i", "'start i'");
shouldBe("j", "'propName'");

var successfullyParsed = true;
