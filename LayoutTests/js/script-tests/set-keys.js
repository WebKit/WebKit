description("Tests basic correctness of ES Set's keys() API");

shouldBe("Set.prototype.keys.length", "0");
shouldBeTrue("Set.prototype.keys === Set.prototype.values");
