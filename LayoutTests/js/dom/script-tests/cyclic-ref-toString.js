description(
"This will test Array.toString with circular references. A circular reference is not detected and skipped: the conversion recurses until the stack is exhausted and then throws a RangeError."
);

var ary1=[1,2];
ary1.push(ary1);
shouldThrow("ary1.toString()");

ary1=[1,2];
var ary2=[3,4];
ary1.push(ary2);
ary2.push(ary1);
shouldThrow("ary1.toString()");

ary1.push(5);
shouldThrow("ary1.toString()");

// An acyclic nesting converts normally, including when the same array occurs more than once.
var ary3=[3,4];
shouldBe("[1,2,ary3].toString()", "'1,2,3,4'");
shouldBe("[1,2,ary3,ary3,5].toString()", "'1,2,3,4,3,4,5'");
