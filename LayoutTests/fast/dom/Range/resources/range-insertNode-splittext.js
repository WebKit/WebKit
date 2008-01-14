description('Test for proper behavior of Range.insertNode when splitting text nodes');

var p = document.createElement('p');
var t1 = document.createTextNode('12345');
p.appendChild(t1);
var t2 = document.createTextNode('ABCDE');
document.body.appendChild(p);
var r = document.createRange();
r.setStart(t1, 2);
r.setEnd(t1, 3);
r.insertNode(t2);

shouldBe("p.childNodes.length", "3");
shouldBe("p.childNodes[0]", "t1");
shouldBeEqualToString("p.childNodes[0].data", "12");
shouldBe("p.childNodes[1]", "t2");
shouldBeEqualToString("p.childNodes[1].data", "ABCDE");
shouldBeEqualToString("p.childNodes[2].data", "345");

var t3 = p.childNodes[2];

shouldBeFalse("r.collapsed");
shouldBe("r.commonAncestorContainer", "p");
shouldBe("r.startContainer", "t1");
shouldBe("r.startOffset", "2");
shouldBe("r.endContainer", "t3");
shouldBe("r.endOffset", "1");
shouldBeEqualToString("r.toString()", "ABCDE3")

// clean up after ourselves
document.body.removeChild(p);

var successfullyParsed = true;
