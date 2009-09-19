description('Test for proper behavior of Range.insertNode(documentFragment) when startContainer != endContainer');

var p = document.createElement('p');
var t1 = document.createTextNode('12345');
p.appendChild(t1);
var t2 = document.createTextNode('ABCDE');
p.appendChild(t2);
document.body.appendChild(p);
var r = document.createRange();
r.setStart(p, 1);
r.setEnd(t2, 3);
shouldBeEqualToString("r.toString()", "ABC");

var df = document.createDocumentFragment();
var t3 = document.createTextNode("PQR");
var t4 = document.createTextNode("XYZ");
df.appendChild(t3);
df.appendChild(t4);
r.insertNode(df);

shouldBe("p.childNodes.length", "4");
shouldBe("p.childNodes[0]", "t1");
shouldBe("p.childNodes[1]", "t3");
shouldBe("p.childNodes[2]", "t4");
shouldBe("p.childNodes[3]", "t2");

shouldBeFalse("r.collapsed");
shouldBe("r.commonAncestorContainer", "p");
shouldBe("r.startContainer", "p");
shouldBe("r.startOffset", "1");
shouldBe("r.endContainer", "t2");
shouldBe("r.endOffset", "3");
shouldBeEqualToString("r.toString()", "PQRXYZABC")

// clean up after ourselves
document.body.removeChild(p);

var successfullyParsed = true;
