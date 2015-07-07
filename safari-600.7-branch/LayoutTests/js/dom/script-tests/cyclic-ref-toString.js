description(
"This will test Array.toString with circular references.  If an element contains a reference to itself or one of its children (at any depth) contains a reference to it, it will be skipped. This can result in either a trailing ',' (if the self reference is at the end of the array) or ',,' if the self reference is contained at some mid point in the array."
);

var ary1=[1,2];
ary1.push(ary1);
shouldBe("ary1.toString()", "'1,2,'");

ary1=[1,2];
var ary2=[3,4];
ary1.push(ary2);
ary2.push(ary1);
shouldBe("ary1.toString()", "'1,2,3,4,'");

ary1.push(5);
shouldBe("ary1.toString()", "'1,2,3,4,,5'");
