description("Tests find for strings with letters that have diacritical marks.");

shouldBeEqualToString("testFind('cafe', 'cafÉ', forward, CaseInsensitive)", "not found");
shouldBeEqualToString("testFind('cafe', 'cafÉ', forward, CaseSensitive)", "not found");
shouldBeEqualToString("testFind('cafe', 'café', forward, CaseInsensitive)", "not found");
shouldBeEqualToString("testFind('cafe', 'café', forward, CaseSensitive)", "not found");
shouldBeEqualToString("testFind('cafÉ', 'cafE', forward, CaseInsensitive)", "0, 4");
shouldBeEqualToString("testFind('cafÉ', 'cafE', forward, CaseSensitive)", "not found");
shouldBeEqualToString("testFind('cafÉ', 'cafe', forward, CaseInsensitive)", "0, 4");
shouldBeEqualToString("testFind('cafÉ', 'cafe', forward, CaseSensitive)", "not found");
shouldBeEqualToString("testFind('cafÉ', 'café', forward, CaseInsensitive)", "0, 4");
shouldBeEqualToString("testFind('cafÉ', 'café', forward, CaseSensitive)", "not found");
shouldBeEqualToString("testFind('café', 'cafE', forward, CaseInsensitive)", "0, 4");
shouldBeEqualToString("testFind('café', 'cafE', forward, CaseSensitive)", "not found");
shouldBeEqualToString("testFind('café', 'cafe', forward, CaseInsensitive)", "0, 4");
shouldBeEqualToString("testFind('café', 'cafe', forward, CaseSensitive)", "not found");
shouldBeEqualToString("testFind('café', 'cafÉ', forward, CaseInsensitive)", "0, 4");
shouldBeEqualToString("testFind('café', 'cafÉ', forward, CaseSensitive)", "not found");
shouldBeEqualToString("testFind('café', 'café', forward, CaseInsensitive)", "0, 4");
shouldBeEqualToString("testFind('café', 'café', forward, CaseSensitive)", "0, 4");

debug("<br>Thai letters:");

var thaiLines = [
    "สื่ออิเล็กทรอนิกส์ อินเทอร์เน็ต หรือทางใดทางหนึ่งทั้งสิ้น",
    "หากท่านใดประสงค์จะพิมพ์แจกเป็นธรรมทาน หรือร่วมสนับสนุนสื่อธรรมะโปรดติดต่อ",
    "และเทคโนโลยี ก็พัฒนามาจนทำให้ผู้สนใจสามารถฟังไฟล์เสียง ดูวีดีโอ ได้จากอินเทอร์เน็ต ตามเวลา",
    "และขอขอบคุณทุกท่านที่มีส่วนร่วมในการทำให้งานนี้สำเร็จจนมาอยู่ในมือท่านในขณะนี้"
];

var thaiPatterns = [
    "ทั้ง",
    "ร่วม",
    "ดู"
];

shouldBeEqualToString("testFind('" + thaiLines[0] + "', '" + thaiPatterns[0] + "', forward, CaseInsensitive)", "49, 53");
shouldBe("testFind('" + thaiLines[0] + "', '" + thaiPatterns[0] + "', forward, CaseSensitive)", "testFind('" + thaiLines[0] + "', '" + thaiPatterns[0] + "', forward, CaseInsensitive)");

shouldBeEqualToString("testFind('" + thaiLines[0] + "', '" + thaiPatterns[1] + "', forward, CaseInsensitive)", "not found");
shouldBe("testFind('" + thaiLines[0] + "', '" + thaiPatterns[1] + "', forward, CaseSensitive)", "testFind('" + thaiLines[0] + "', '" + thaiPatterns[1] + "', forward, CaseInsensitive)");

shouldBeEqualToString("testFind('" + thaiLines[0] + "', '" + thaiPatterns[2] + "', forward, CaseInsensitive)", "not found");
shouldBe("testFind('" + thaiLines[0] + "', '" + thaiPatterns[2] + "', forward, CaseSensitive)", "testFind('" + thaiLines[0] + "', '" + thaiPatterns[2] + "', forward, CaseInsensitive)");

shouldBeEqualToString("testFind('" + thaiLines[1] + "', '" + thaiPatterns[0] + "', forward, CaseInsensitive)", "not found");
shouldBe("testFind('" + thaiLines[1] + "', '" + thaiPatterns[0] + "', forward, CaseSensitive)", "testFind('" + thaiLines[1] + "', '" + thaiPatterns[0] + "', forward, CaseInsensitive)");

shouldBeEqualToString("testFind('" + thaiLines[1] + "', '" + thaiPatterns[1] + "', forward, CaseInsensitive)", "42, 46");
shouldBe("testFind('" + thaiLines[1] + "', '" + thaiPatterns[1] + "', forward, CaseSensitive)", "testFind('" + thaiLines[1] + "', '" + thaiPatterns[1] + "', forward, CaseInsensitive)");

shouldBeEqualToString("testFind('" + thaiLines[1] + "', '" + thaiPatterns[2] + "', forward, CaseInsensitive)", "not found");
shouldBe("testFind('" + thaiLines[1] + "', '" + thaiPatterns[2] + "', forward, CaseSensitive)", "testFind('" + thaiLines[1] + "', '" + thaiPatterns[2] + "', forward, CaseInsensitive)");


shouldBeEqualToString("testFind('" + thaiLines[2] + "', '" + thaiPatterns[0] + "', forward, CaseInsensitive)", "not found");
shouldBe("testFind('" + thaiLines[2] + "', '" + thaiPatterns[0] + "', forward, CaseSensitive)", "testFind('" + thaiLines[2] + "', '" + thaiPatterns[0] + "', forward, CaseInsensitive)");

shouldBeEqualToString("testFind('" + thaiLines[2] + "', '" + thaiPatterns[1] + "', forward, CaseInsensitive)", "not found");
shouldBe("testFind('" + thaiLines[2] + "', '" + thaiPatterns[1] + "', forward, CaseSensitive)", "testFind('" + thaiLines[2] + "', '" + thaiPatterns[1] + "', forward, CaseInsensitive)");

shouldBeEqualToString("testFind('" + thaiLines[2] + "', '" + thaiPatterns[2] + "', forward, CaseInsensitive)", "55, 57");
shouldBe("testFind('" + thaiLines[2] + "', '" + thaiPatterns[2] + "', forward, CaseSensitive)", "testFind('" + thaiLines[2] + "', '" + thaiPatterns[2] + "', forward, CaseInsensitive)");


shouldBeEqualToString("testFind('" + thaiLines[3] + "', '" + thaiPatterns[0] + "', forward, CaseInsensitive)", "not found");
shouldBe("testFind('" + thaiLines[3] + "', '" + thaiPatterns[0] + "', forward, CaseSensitive)", "testFind('" + thaiLines[3] + "', '" + thaiPatterns[0] + "', forward, CaseInsensitive)");

shouldBeEqualToString("testFind('" + thaiLines[3] + "', '" + thaiPatterns[1] + "', forward, CaseInsensitive)", "27, 31");
shouldBe("testFind('" + thaiLines[3] + "', '" + thaiPatterns[1] + "', forward, CaseSensitive)", "testFind('" + thaiLines[3] + "', '" + thaiPatterns[1] + "', forward, CaseInsensitive)");

shouldBeEqualToString("testFind('" + thaiLines[3] + "', '" + thaiPatterns[2] + "', forward, CaseInsensitive)", "not found");
shouldBe("testFind('" + thaiLines[3] + "', '" + thaiPatterns[2] + "', forward, CaseSensitive)", "testFind('" + thaiLines[3] + "', '" + thaiPatterns[2] + "', forward, CaseInsensitive)");
