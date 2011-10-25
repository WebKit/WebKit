description("Test that invalid canvas getContext() requests return null.");

canvas = document.createElement('canvas');

shouldBe("canvas.getContext('')", "null");
shouldBe("canvas.getContext('2d#')", "null");
shouldBe("canvas.getContext('This is clearly not a valid context name.')", "null");
shouldBe("canvas.getContext('2d\0')", "null");
shouldBe("canvas.getContext('2\uFF44')", "null");
shouldBe("canvas.getContext('2D')", "null");
