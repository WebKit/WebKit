description("Test canvas createImageData()");

ctx = document.createElement('canvas').getContext('2d');

shouldThrow("ctx.createImageData(null)", '"Error: NotSupportedError: DOM Exception 9"');

// create a 100x50 imagedata and fill it with white pixels

imageData = ctx.createImageData(100, 50);

for (i = 0; i < imageData.data.length; ++i)
    imageData.data[i] = 255;

shouldBe("imageData.width", "100");
shouldBe("imageData.height", "50");
shouldBe("imageData.data[32]", "255");

// createImageData(imageData) should create a new ImageData of the same size as 'imageData'
// but filled with transparent black

sameSizeImageData = ctx.createImageData(imageData);
shouldBe("sameSizeImageData.width", "100");
shouldBe("sameSizeImageData.height", "50");
shouldBe("sameSizeImageData.data[32]", "0");

// createImageData(width, height) takes the absolute magnitude of the size arguments

imgdata1 = ctx.createImageData(10, 20);
imgdata2 = ctx.createImageData(-10, 20);
imgdata3 = ctx.createImageData(10, -20);
imgdata4 = ctx.createImageData(-10, -20);

shouldBe("imgdata1.data.length", "800");
shouldBe("imgdata1.data.length", "imgdata2.data.length");
shouldBe("imgdata2.data.length", "imgdata3.data.length");
shouldBe("imgdata3.data.length", "imgdata4.data.length");
