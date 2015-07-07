description("Ensure that the constructor for Path object and dependent functions exist.");

shouldBe("typeof document.getElementById", '"function"');

var path = new Path2D();
shouldBeType("path", "Path2D");
shouldBe("typeof path.closePath", '"function"');
shouldBe("typeof path.moveTo", '"function"');
shouldBe("typeof path.lineTo", '"function"');
shouldBe("typeof path.quadraticCurveTo", '"function"');
shouldBe("typeof path.bezierCurveTo", '"function"');
shouldBe("typeof path.arcTo", '"function"');
shouldBe("typeof path.arc", '"function"');
shouldBe("typeof path.rect", '"function"');