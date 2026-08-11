function shouldBe(actual, expected) {
  if (actual !== expected)
    throw new Error('bad value: ' + actual);
}

// Thu Apr 28 2022 14:42:34 GMT-0700 (Pacific Daylight Time)
const date1 = new Date(1651182154000);
$vm.setUserPreferredLanguages(['de-DE']);
shouldBe(date1.toString(), 'Thu Apr 28 2022 23:42:34 GMT+0200 (Mitteleuropäische Sommerzeit)');
shouldBe(date1.toTimeString(), '23:42:34 GMT+0200 (Mitteleuropäische Sommerzeit)');

// Tue Jan 18 2022 13:42:34 GMT-0800 (Pacific Standard Time)
const date2 = new Date(1642542154000);
shouldBe(date2.toString(), 'Tue Jan 18 2022 22:42:34 GMT+0100 (Mitteleuropäische Normalzeit)');
shouldBe(date2.toTimeString(), '22:42:34 GMT+0100 (Mitteleuropäische Normalzeit)');
