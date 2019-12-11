description('Date.UTC() should apply TimeClip operation according to ECMA-262.');

shouldBe('Date.UTC(275760, 8, 12, 23, 59, 59, 999)', '8639999999999999');
shouldBe('Date.UTC(275760, 8, 13)', '8640000000000000');
shouldBeTrue('isNaN(Date.UTC(275760, 8, 13, 0, 0, 0, 1))');
shouldBeTrue('isNaN(Date.UTC(275760, 8, 14))');

shouldBe('Date.UTC(-271821, 3, 20, 0, 0, 0, 1)', '-8639999999999999');
shouldBe('Date.UTC(-271821, 3, 20)', '-8640000000000000');
shouldBeTrue('isNaN(Date.UTC(-271821, 3, 19, 23, 59, 59, 999))');
shouldBeTrue('isNaN(Date.UTC(-271821, 3, 19))');
