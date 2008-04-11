description(

"This test checks that toString and join share the same HashSet for visited elements."

);

var arr = [1, 2];
var obj = {};
obj.__proto__.toString = function() { return "*" + arr + "*"; }
arr[2] = arr;
arr[3] = obj;

shouldBe("arr.join()", "'1,2,,**'");

successfullyParsed = true;
