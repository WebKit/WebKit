description(
'Test constructor for TouchList.  &lt;<a href="rdar://problem/6481690">rdar://problem/6481690</a>&gt;'
);

var element = document.getElementById('description');
var touch1 = new Touch(document, element, 1, 2, 3, 4);
var touch2 = new Touch(document, element, 2, 3, 4, 5);
var touch3 = new Touch(document, element, 3, 4, 5, 6);

shouldBe("TouchList.prototype.toString.call(new TouchList)", "'[object TouchList]'");
shouldBe("TouchList.prototype.toString.call(new TouchList(touch1))", "'[object TouchList]'");
shouldBe("TouchList.prototype.toString.call(new TouchList(touch1, touch2))", "'[object TouchList]'");
shouldBe("TouchList.prototype.toString.call(new TouchList(touch1, touch2, touch3))", "'[object TouchList]'");

var successfullyParsed = true;
