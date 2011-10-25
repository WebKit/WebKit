description("Test that setting a font with size in 'ex' units doesn't crash.");

ctx = document.createElement('canvas').getContext('2d');

ctx.font = "5ex sans-serif";
shouldBe("ctx.font = '5ex sans-serif'; ctx.font", "'5ex sans-serif'");
