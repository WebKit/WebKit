description(
'Test constructor for WebKitPoint.  &lt;<a href="rdar://problem/6481690">rdar://problem/6481690</a>&gt;'
);

shouldBe("WebKitPoint.prototype.toString.call(new WebKitPoint)", "'[object WebKitPoint]'");
shouldBe("WebKitPoint.prototype.toString.call(new WebKitPoint(0))", "'[object WebKitPoint]'");
shouldBe("WebKitPoint.prototype.toString.call(new WebKitPoint(1, 1))", "'[object WebKitPoint]'");
shouldBe("WebKitPoint.prototype.toString.call(new WebKitPoint(1, 2, 3))", "'[object WebKitPoint]'");

var successfullyParsed = true;
