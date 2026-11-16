function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected} (${message})`);
}

function referenceDay(days) {
    let r = (days + 4) % 7;
    return r < 0 ? r + 7 : r;
}

const msPerDay = 86400000;
const maxDays = 100000000;

// 0: Sunday ... 6: Saturday
shouldBe(new Date(Date.UTC(1970, 0, 1)).getUTCDay(), 4, "1970-01-01 is Thursday");
shouldBe(new Date(Date.UTC(1969, 11, 31)).getUTCDay(), 3, "1969-12-31 is Wednesday");
shouldBe(new Date(Date.UTC(2000, 0, 1)).getUTCDay(), 6, "2000-01-01 is Saturday");
shouldBe(new Date(Date.UTC(1900, 0, 1)).getUTCDay(), 1, "1900-01-01 is Monday");
shouldBe(new Date(Date.UTC(1600, 0, 1)).getUTCDay(), 6, "1600-01-01 is Saturday");
shouldBe(new Date(0).getUTCDay(), 4);

// Extremes of the ECMAScript time range.
shouldBe(new Date(maxDays * msPerDay).getUTCDay(), referenceDay(maxDays), "max time value");
shouldBe(new Date(-maxDays * msPerDay).getUTCDay(), referenceDay(-maxDays), "min time value");
shouldBe(new Date(maxDays * msPerDay).getUTCDay(), 6, "+275760-09-13 is Saturday");
shouldBe(new Date(-maxDays * msPerDay).getUTCDay(), 2, "-271821-04-20 is Tuesday");

// Days around zero and around each boundary.
for (let days = -20; days <= 20; ++days)
    shouldBe(new Date(days * msPerDay).getUTCDay(), referenceDay(days), `days=${days}`);
for (let days = maxDays - 20; days <= maxDays; ++days) {
    shouldBe(new Date(days * msPerDay).getUTCDay(), referenceDay(days), `days=${days}`);
    shouldBe(new Date(-days * msPerDay).getUTCDay(), referenceDay(-days), `days=${-days}`);
}

// Strided sweep over the whole range.
for (let days = -maxDays; days <= maxDays; days += 4093)
    shouldBe(new Date(days * msPerDay).getUTCDay(), referenceDay(days), `days=${days}`);

// Last millisecond of a day still belongs to that day.
for (let days = -1000; days <= 1000; days += 97)
    shouldBe(new Date(days * msPerDay + msPerDay - 1).getUTCDay(), referenceDay(days), `days=${days} end`);
