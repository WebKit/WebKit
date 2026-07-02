function shouldBe(actual, expected) {
    if (actual !== expected)
        print(`expected ${expected} but got ${actual}`);
}

// "Coming Soon" in Greek.
shouldBe(`Σύντομα διαθέσιμο`.toLocaleUpperCase('en-US'), `ΣΎΝΤΟΜΑ ΔΙΑΘΈΣΙΜΟ`);
shouldBe(`Σύντομα διαθέσιμο`.toLocaleUpperCase('el'), `ΣΥΝΤΟΜΑ ΔΙΑΘΕΣΙΜΟ`);
shouldBe(`Σύντομα διαθέσιμο`.toLocaleUpperCase('el-gr'), `ΣΥΝΤΟΜΑ ΔΙΑΘΕΣΙΜΟ`);

// Sigma handling is not locale-sensitive but included in this test.
shouldBe(`AAAΣ`.toLocaleLowerCase('en-US'), `aaaς`);
shouldBe(`AAAΣ`.toLocaleLowerCase('el'), `aaaς`);
shouldBe(`AAAΣ`.toLocaleLowerCase('el-gr'), `aaaς`);

shouldBe(`ΣAAA`.toLocaleLowerCase('en-US'), `σaaa`);
shouldBe(`ΣAAA`.toLocaleLowerCase('el'), `σaaa`);
shouldBe(`ΣAAA`.toLocaleLowerCase('el-gr'), `σaaa`);
