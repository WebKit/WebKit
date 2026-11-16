function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

let date = new Date(1654191240000);
let t = Intl.DateTimeFormat("en-US", {
    timeZone: "America/New_York",
    weekday: "short",
    year: "numeric",
    month: "short",
    day: "numeric",
    hour: "numeric",
    minute: "numeric"
}).format(date);
let reparsed = new Date(t)
shouldBe(reparsed.getTime(), date.getTime());

// "at" case
shouldBe(new Date(`Thu, May 26, 2022, 6:27 PM`).getTime(), 1653604020000);
shouldBe(new Date(`Thu, May 26, 2022 at 6:27 PM`).getTime(), 1653604020000);
