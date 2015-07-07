description("Tests for Date.toISOString");

function throwsRangeError(str)
{
    try {
        eval(str);
    } catch (e) {
        return e instanceof RangeError;
    }
    return false;
}

shouldThrow("Date.toISOString.call({})");
shouldThrow("Date.toISOString.call(0)");

shouldBe("new Date(-400).toISOString()", "'1969-12-31T23:59:59.600Z'");
shouldBe("new Date(0).toISOString()", "'1970-01-01T00:00:00.000Z'");
shouldBe("new Date('1 January 1500 UTC').toISOString()", "'1500-01-01T00:00:00.000Z'");
shouldBe("new Date('1 January 2000 UTC').toISOString()", "'2000-01-01T00:00:00.000Z'");
shouldBe("new Date('1 January 4000 UTC').toISOString()", "'4000-01-01T00:00:00.000Z'");
shouldBe("new Date('1 January 100000 UTC').toISOString()", "'+100000-01-01T00:00:00.000Z'");
shouldBe("new Date('1 January -1 UTC').toISOString()", "'-000001-01-01T00:00:00.000Z'");
shouldBe("new Date('10 March 2000 UTC').toISOString()", "'2000-03-10T00:00:00.000Z'");
shouldBeTrue('throwsRangeError("new Date(NaN).toISOString()")');
