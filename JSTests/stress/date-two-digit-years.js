function shouldBe(actual, expected) {
    if (Number.isNaN(expected)) {
        if (!Number.isNaN(actual)) {
            throw new Error(`Actual: ${actual}, Expected: ${expected}`);
        }
    } else if (actual !== expected) {
        throw new Error(`Actual: ${actual}, Expected: ${expected}`);
    }
}

for (let year of Array(100).keys()) {
    for (let month of Array(12).keys()) {
        for (let day of Array(31).keys()) {
            let fullYear = year >= 50 ? year + 1900 : year + 2000;
            let fullDate = new Date(`${month + 1}/${day + 1}/${fullYear}`);

            // mm/dd/yy
            let d1 = new Date(`${month + 1}/${day + 1}/${year}`);
            shouldBe(d1.getTime(), fullDate.getTime());

            // yy/mm/dd
            let d2 = new Date(`${year}/${month + 1}/${day + 1}`);
            if (year > 31) {
                shouldBe(d2.getTime(), fullDate.getTime());
            } else if (year > 12) {
                shouldBe(d2.getTime(), new Date(NaN).getTime());
            }
        }
    }
}

shouldBe(new Date("99/1/99").getTime(), new Date(NaN).getTime());
shouldBe(new Date("13/13/13").getTime(), new Date(NaN).getTime());
shouldBe(new Date("0/10/0").getTime(), new Date(NaN).getTime());

for (let year of Array(1000).keys()) {
    let fullDate = new Date(`5/1/${year}`);
    let d1 = new Date(`may 1 ${year}`);
    let d1_1 = new Date(`mayrandomstring 1 ${year}`);

    shouldBe(d1.getTime(), fullDate.getTime());
    shouldBe(d1_1.getTime(), fullDate.getTime());
}

shouldBe(new Date("may 1999 1999").getTime(), new Date(NaN).getTime());
shouldBe(new Date("may 0 0").getTime(), new Date(NaN).getTime());

shouldBe(new Date("00/2/29").getTime(), new Date(NaN).getTime());
shouldBe(new Date("00/2/30").getTime(), new Date(NaN).getTime());
shouldBe(new Date("00/4/31").getTime(), new Date(NaN).getTime());
shouldBe(new Date("00/4/0").getTime(), new Date(NaN).getTime());
shouldBe(new Date("00/4/-1").getTime(), new Date(NaN).getTime());

// leap years
shouldBe(new Date("80/2/29").getTime(), 320659200000);
shouldBe(new Date("80/2/30").getTime(), new Date("1980/2/30").getTime());
shouldBe(new Date("80/2/30").getTime(), new Date("1980/3/1").getTime());
shouldBe(new Date("80/2/30").getTime(), 320745600000);
shouldBe(new Date("80/2/31").getTime(), new Date("1980/3/2").getTime());
shouldBe(new Date("80/2/31").getTime(), 320832000000);
shouldBe(new Date("80/2/32").getTime(), new Date(NaN).getTime());
shouldBe(new Date("80/4/31").getTime(), new Date("1980/4/31").getTime());
shouldBe(new Date("80/4/31").getTime(), 326012400000);
shouldBe(new Date("80/4/32").getTime(), new Date(NaN).getTime());

for (let year = 50; year <= 99; ++year) {
    for (let month = 0; month < 12; ++month) {
        for (let day = 1; day <= 100; ++day) {
            const date = `${year}/${month + 1}/${day}`;
            if (day <= 31) {
                shouldBe(new Date(year, month, day).getTime(), new Date(date).getTime());
            } else {
                shouldBe(Number.isNaN(new Date(date).getTime()), true);
            }
        }
    }
}
