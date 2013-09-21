description("Test to ensure correct behaviour of Array.prototype.splice when the array has holes in it.");

var actualArray = new Array(20);
var seedArray = ["Black","White","Blue","Red","Green","Orange","Purple","Cyan","Yellow"];
for (var i = 0; i < seedArray.length; i++)
    actualArray[i] = seedArray[i];
actualArray.splice(3, 1);
var expectedSeedArray = ["Black","White","Blue","Green","Orange","Purple","Cyan","Yellow"];
var expectedArray = new Array(19);
for (var i = 0; i < expectedSeedArray.length; i++)
    expectedArray[i] = expectedSeedArray[i];

shouldBe("actualArray.toString()", "expectedArray.toString()");
shouldBe("actualArray.length", "expectedArray.length");

actualArray = new Array(20);
for (var i = 0; i < seedArray.length; i += 2)
    actualArray[i] = seedArray[i];
actualArray.splice(2, 2);
var expectedArray = new Array(18);
expectedArray[0] = "Black";
expectedArray[2] = "Green";
expectedArray[4] = "Purple";
expectedArray[6] = "Yellow";

shouldBe("actualArray.toString()", "expectedArray.toString()");
shouldBe("actualArray.length", "expectedArray.length");
