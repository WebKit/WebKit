function shouldBe(actual, expected) {
  if (actual !== expected)
    throw new Error('bad value: ' + actual);
}

// Thu Apr 28 2022 14:42:34 GMT-0700 (Pacific Daylight Time)
const date1 = new Date(1651182154000);
$vm.setUserPreferredLanguages(['en-US']);
shouldBe(date1.toString(), 'Thu Apr 28 2022 17:42:34 GMT-0400 (Eastern Daylight Time)');
shouldBe(date1.toTimeString(), '17:42:34 GMT-0400 (Eastern Daylight Time)');

// Tue Jan 18 2022 13:42:34 GMT-0800 (Pacific Standard Time)
const date2 = new Date(1642542154000);
shouldBe(date2.toString(), 'Tue Jan 18 2022 16:42:34 GMT-0500 (Eastern Standard Time)');
shouldBe(date2.toTimeString(), '16:42:34 GMT-0500 (Eastern Standard Time)');
