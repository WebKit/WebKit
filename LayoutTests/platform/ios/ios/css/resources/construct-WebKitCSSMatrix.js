description(
'Test constructor for WebKitCSSMatrix.  &lt;<a href="rdar://problem/6481690">rdar://problem/6481690</a>&gt;'
);

shouldBe("WebKitCSSMatrix.prototype.toString.call(new WebKitCSSMatrix)",
         "'matrix(1, 0, 0, 1, 0, 0)'");

shouldBe("WebKitCSSMatrix.prototype.toString.call(new WebKitCSSMatrix(new WebKitCSSMatrix))",
         "'matrix(1, 0, 0, 1, 0, 0)'");

shouldBe("WebKitCSSMatrix.prototype.toString.call(new WebKitCSSMatrix('matrix3d(1.000000, 0.000000, 0.000000, 0.000000, 0.000000, 1.000000, 0.000000, 0.000000, 0.000000, 0.000000, 1.000000, 0.000000, 0.000000, 0.000000, 0.000000, 1.000000)'))",
         "'matrix3d(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)'");

var successfullyParsed = true;
