description(
'Test constructor for WebKitCSSMatrix.  &lt;<a href="rdar://problem/6481690">rdar://problem/6481690</a>&gt;'
);

shouldBe("WebKitCSSMatrix.prototype.toString.call(new WebKitCSSMatrix)",
         "'matrix(1.000000, 0.000000, 0.000000, 1.000000, 0.000000, 0.000000)'");

shouldBe("WebKitCSSMatrix.prototype.toString.call(new WebKitCSSMatrix(new WebKitCSSMatrix))",
         "'matrix(1.000000, 0.000000, 0.000000, 1.000000, 0.000000, 0.000000)'");

shouldBe("WebKitCSSMatrix.prototype.toString.call(new WebKitCSSMatrix('matrix3d(1.000000, 0.000000, 0.000000, 0.000000, 0.000000, 1.000000, 0.000000, 0.000000, 0.000000, 0.000000, 1.000000, 0.000000, 0.000000, 0.000000, 0.000000, 1.000000)'))",
         "'matrix(1.000000, 0.000000, 0.000000, 1.000000, 0.000000, 0.000000)'");

var successfullyParsed = true;
