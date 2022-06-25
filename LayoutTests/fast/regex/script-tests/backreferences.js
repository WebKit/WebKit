description("This tests backreferences"); 

// Basic counted, greedy and non-greedy back references
shouldBe("/^(.)\\1{3}/.exec('=====')", '["====", "="]');

shouldBe("/^(.)\\1*/.exec('=====')", '["=====", "="]');
shouldBe("/^(.)\\1*?/.exec('=====')", '["=", "="]');
shouldBe("/^(.)\\1*?$/.exec('=====')", '["=====", "="]');

// Back reference back tracking
shouldBe("/(.*)\\1/.exec('======')", '["======", "==="]');
shouldBe("/(.*)\\1{2}/.exec('======')", '["======", "=="]');
shouldBe("/(.*)\\1{4}/.exec('======')", '["=====", "="]');
shouldBe("/(.*)\\1{5}/.exec('======')", '["======", "="]');
shouldBe("/(=).\\1{3}/.exec('=a==b===')", '["=b===", "="]');
shouldBe("/(===).\\1*X/.exec('===a==X===b======X')", '["===b======X", "==="]');

// Multiple back references
shouldBe("/\\w*?(\\w*) (c\\1) is a f\\1 \\2/.exec('That cat is a fat cat')", '["That cat is a fat cat", "at", "cat"]');
shouldBe("/(\\w)(\\w)(\\w)e\\3\\2\\1/i.exec('Racecar')", '["Racecar", "R", "a", "c"]');

// Named capture group back references
shouldBe("/^(?<equals>=*)\\k<equals>+?$/.exec('======')", '["======", "==="]');

// Unicode back references
shouldBe("/^(\\u{10123}*)x\\1?$/u.exec('\\u{10123}x\\u{10123}')", '["\\u{10123}x\\u{10123}", "\\u{10123}"]');

// Ignore case back references
shouldBe("/(.{4})\\1/i.exec('ABcdAbCd')", '["ABcdAbCd", "ABcd"]');
shouldBe("/(.{4})\\1/i.exec('ABc\\u{fd}AbC\\u{dd}')", '["ABc\\u{fd}AbC\\u{dd}", "ABc\\u{fd}"]');
shouldBe("/(.{4})\\1/i.exec('ABc\\u{b5}AbC\\u{b5}')", '["ABc\\u{b5}AbC\\u{b5}", "ABc\\u{b5}"]');
shouldBe("/(.{4})\\1/i.exec('ABc\\u{ff}AbC\\u{ff}')", '["ABc\\u{ff}AbC\\u{ff}", "ABc\\u{ff}"]');
