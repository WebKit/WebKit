function shouldBe(actual, expected) {
    if (Number.isNaN(actual) || actual !== expected)
        throw new Error('bad value: ' + actual);
}

function parsedDate(string) {
    return Date.parse(string);
}

shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+0`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-0`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+000`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-000`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+00000`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-00000`)), true);

shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+:00`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-:00`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+0:00`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-0:00`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+000:00`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-000:00`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+0000:00`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-0000:00`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+00000:00`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-00000:00`)), true);

shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+00:`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-00:`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+00:0`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-00:0`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+00:000`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-00:000`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+00:0000`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-00:0000`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+00:00000`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-00:00000`)), true);

shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+25`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-25`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+2500`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-2500`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+0080`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-0080`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+0060`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-0060`)), true);

shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+24A`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-24A`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+2400A`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-2400A`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+0080A`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-0080A`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+0059A`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-0059A`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+24:00A`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-24:00A`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+00:80A`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-00:80A`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+00:59A`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-00:59A`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+24:00:`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-24:00:`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+00:80:`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-00:80:`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+00:59:`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-00:59:`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+24:00:00`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-24:00:00`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+00:80:00`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-00:80:00`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+00:59:00`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-00:59:00`)), true);

shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000++2`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-+2`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000++200`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-+200`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+00:0+`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-00:0+`)), true);

shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+00`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-00`)), true);
shouldBe(parsedDate(`1970-01-01T00:00:00.000+0000`), 0);
shouldBe(parsedDate(`1970-01-01T00:00:00.000-0000`), 0);
shouldBe(parsedDate(`1970-01-01T00:00:00.000+0030`), -(60 * 30 * 1000));
shouldBe(parsedDate(`1970-01-01T00:00:00.000-0030`), +(60 * 30 * 1000));
shouldBe(parsedDate(`1970-01-01T00:00:00.000+0130`), -(60 * (30 + 60) * 1000));
shouldBe(parsedDate(`1970-01-01T00:00:00.000-0130`), +(60 * (30 + 60) * 1000));

shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+24:59`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-24:59`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000+2459`)), true);
shouldBe(Number.isNaN(parsedDate(`1970-01-01T00:00:00.000-2459`)), true);
