description(
"This tests that querySelector and querySelectorAll don't crash when used in a viewless document."
);

var testDoc = document.implementation.createDocument("http://www.w3.org/1999/xhtml", "html");
testDoc.documentElement.appendChild(testDoc.createElement("body"));
testDoc.body.appendChild(testDoc.createElement("p")).id = "p1";
testDoc.getElementById("p1").appendChild(testDoc.createElement("span")).id = "s1";
testDoc.body.appendChild(testDoc.createElement("span")).id = "s2";
testDoc.body.appendChild(testDoc.createElement("div")).className = "d1";

var p1 = testDoc.getElementById("p1");
var s2 = testDoc.getElementById("s2");
var d1 = testDoc.body.lastChild;

shouldBe("testDoc.querySelector('p')", "p1");
shouldBe("testDoc.querySelectorAll('span').length", "2");
shouldBe("testDoc.querySelectorAll('span').item(1)", "s2");
shouldBe("testDoc.querySelector('.d1')", "d1");
shouldBe("testDoc.querySelectorAll('p span').length", "1");

var successfullyParsed = true;
