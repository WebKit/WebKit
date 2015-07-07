description(
'Test constructor for Touch.  &lt;<a href="rdar://problem/6481690">rdar://problem/6481690</a>&gt;'
);

var element = document.getElementById('description');

shouldBe("Touch.prototype.toString.call(new Touch)", "'[object Touch]'");
shouldBe("Touch.prototype.toString.call(new Touch(document))", "'[object Touch]'");
shouldBe("Touch.prototype.toString.call(new Touch(document, element))", "'[object Touch]'");
shouldBe("Touch.prototype.toString.call(new Touch(document, element, 1))", "'[object Touch]'");
shouldBe("Touch.prototype.toString.call(new Touch(document, element, 1, 2))", "'[object Touch]'");
shouldBe("Touch.prototype.toString.call(new Touch(document, element, 1, 2, 3))", "'[object Touch]'");
shouldBe("Touch.prototype.toString.call(new Touch(document, element, 1, 2, 3, 4, 5))", "'[object Touch]'");

var successfullyParsed = true;
