description('Test the elements collection when the form is not a descendant of the document. This test case failed in an early version of Acid3.');

var f = document.createElement('form');
var i = document.createElement('input');
i.name = 'first';
i.type = 'text';
i.value = 'test';
f.appendChild(i);

shouldBe("i.getAttribute('name')", "'first'");
shouldBe("i.name", "'first'");
shouldBe("i.getAttribute('type')", "'text'");
shouldBe("i.type", "'text'");
shouldBe("i.value", "'test'");
shouldBe("f.elements.length", "1");
shouldBe("f.elements[0]", "i");
shouldBe("f.elements.first", "i");

f.elements.second;
i.name = 'second';
i.type = 'password';
i.value = 'TEST';

// This has to be the first expression tested, because reporting the result will fix the bug.
shouldBe("f.elements.second", "i");

shouldBe("i.getAttribute('name')", "'second'");
shouldBe("i.name", "'second'");
shouldBe("i.getAttribute('type')", "'password'");
shouldBe("i.type", "'password'");
shouldBe("i.value", "'TEST'");
shouldBe("f.elements.length", "1");
shouldBe("f.elements[0]", "i");
shouldBe("f.elements.first", "undefined");
shouldBe("f.elements.second", "i");

var successfullyParsed = true;
