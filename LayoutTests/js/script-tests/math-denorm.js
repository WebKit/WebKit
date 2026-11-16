//@ skip if $model == "Apple Watch Series 3" # denormalized numbers will be round to zero in ARMv7k.
//@ skip if $architecture == "arm" and $hostOS == "darwin"

description("This test checks the behavior of the Math.ceil and Math.floor on denormalized FP values.");

shouldBe("Math.ceil(Number.MIN_VALUE)", "1");
shouldBe("Math.ceil(-Number.MIN_VALUE)", "-0");

shouldBe("Math.floor(Number.MIN_VALUE)", "0");
shouldBe("Math.floor(-Number.MIN_VALUE)", "-1");
