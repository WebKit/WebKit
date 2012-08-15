description("Test that setting a font with size in 'ex' units doesn't crash.");

ctx = document.createElement('canvas').getContext('2d');

ctx.font = "5ex sans-serif";

size = parseInt(ctx.font.substr(0, 2));
family = ctx.font.substr(5);

shouldBeCloseTo("size", 25, 10);
shouldBe("family", "'sans-serif'");
