description("Series of tests to ensure correct behaviour of the ImageData object");

imageData = document.getElementById("canvas").getContext("2d").getImageData(0,0,2,2);

shouldBe("imageData.width", "2");
shouldBe("imageData.height", "2");
shouldBe("imageData.data.length", "16");
for (var i = 0; i < imageData.data.length; i++)
    shouldBe("imageData.data["+i+"]", "0");

var testValues = [NaN, true, false, "\"garbage\"", "-1",
                  "\"0\"", "\"1\"", "\"2\"", Infinity, -Infinity,
                  -5, -0.5, 0, 0.5, 5,
                  5.4, 255, 256, null, undefined];
var testResults = [0, 1, 0, 0, 0,
                   0, 1, 2, 255, 0,
                   0, 0, 0, 0, 5,
                   5, 255, 255, 0, 0];
for (var i = 0; i < testValues.length; i++) {
    shouldBe("imageData.data[0] = "+testValues[i]+", imageData.data[0]", ""+testResults[i]);
}

shouldBe("imageData.data['foo']='garbage',imageData.data['foo']", "'garbage'");
shouldBe("imageData.data[-1]='garbage',imageData.data[-1]", "'garbage'");
shouldBe("imageData.data[17]='garbage',imageData.data[17]", "undefined");
