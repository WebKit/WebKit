// This test checks that dates follow the range described in ecma262/#sec-date-time-string-format

function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`bad value: ${actual}`);
}

function shouldBeNaN(actual)
{
    if (!Number.isNaN(actual))
        throw new Error(`bad value: ${actual}`);
}

{
    let dateValue = Date.parse("275760-09-13T00:00:00.000Z");
    shouldBe(dateValue, 8640000000000000);

    let date = new Date(dateValue);
    shouldBe(date.getUTCFullYear(), 275760);
    shouldBe(date.getUTCMonth(), 8);
    shouldBe(date.getUTCDate(), 13);
    shouldBe(date.getUTCHours(), 0);
    shouldBe(date.getUTCMinutes(), 0);
    shouldBe(date.getUTCSeconds(), 0);
    shouldBe(date.getUTCMilliseconds(), 0);
}

{
    let dateValue = Date.UTC(275760, 8, 13, 0, 0, 0, 0);
    shouldBe(dateValue, 8640000000000000);

    let date = new Date(dateValue);
    shouldBe(date.getUTCFullYear(), 275760);
    shouldBe(date.getUTCMonth(), 8);
    shouldBe(date.getUTCDate(), 13);
    shouldBe(date.getUTCHours(), 0);
    shouldBe(date.getUTCMinutes(), 0);
    shouldBe(date.getUTCSeconds(), 0);
    shouldBe(date.getUTCMilliseconds(), 0);
}

{
    let dateValue = Date.parse("275760-09-12T23:59:59.999Z");
    shouldBe(dateValue, 8639999999999999);

    let date = new Date(dateValue);
    shouldBe(date.getUTCFullYear(), 275760);
    shouldBe(date.getUTCMonth(), 8);
    shouldBe(date.getUTCDate(), 12);
    shouldBe(date.getUTCHours(), 23);
    shouldBe(date.getUTCMinutes(), 59);
    shouldBe(date.getUTCSeconds(), 59);
    shouldBe(date.getUTCMilliseconds(), 999);
}

{
    let dateValue = Date.UTC(275760, 8, 12, 23, 59, 59, 999);
    shouldBe(dateValue, 8639999999999999);

    let date = new Date(dateValue);
    shouldBe(date.getUTCFullYear(), 275760);
    shouldBe(date.getUTCMonth(), 8);
    shouldBe(date.getUTCDate(), 12);
    shouldBe(date.getUTCHours(), 23);
    shouldBe(date.getUTCMinutes(), 59);
    shouldBe(date.getUTCSeconds(), 59);
    shouldBe(date.getUTCMilliseconds(), 999);
}

{
    let dateValue = Date.parse("275760-09-13T00:00:00.001Z");
    shouldBeNaN(dateValue);
}

{
    let dateValue = Date.UTC(275760, 8, 13, 0, 0, 0, 1);
    shouldBeNaN(dateValue);
}

{
    let dateValue = Date.parse("-271821-04-20T00:00:00.000Z");
    shouldBe(dateValue, -8640000000000000);

    let date = new Date(dateValue);
    shouldBe(date.getUTCFullYear(), -271821);
    shouldBe(date.getUTCMonth(), 3);
    shouldBe(date.getUTCDate(), 20);
    shouldBe(date.getUTCHours(), 0);
    shouldBe(date.getUTCMinutes(), 0);
    shouldBe(date.getUTCSeconds(), 0);
    shouldBe(date.getUTCMilliseconds(), 0);
}

{
    let dateValue = Date.UTC(-271821, 3, 20, 0, 0, 0, 0);
    shouldBe(dateValue, -8640000000000000);

    let date = new Date(dateValue);
    shouldBe(date.getUTCFullYear(), -271821);
    shouldBe(date.getUTCMonth(), 3);
    shouldBe(date.getUTCDate(), 20);
    shouldBe(date.getUTCHours(), 0);
    shouldBe(date.getUTCMinutes(), 0);
    shouldBe(date.getUTCSeconds(), 0);
    shouldBe(date.getUTCMilliseconds(), 0);
}

{
    let dateValue = Date.parse("-271821-04-20T00:00:00.001Z");
    shouldBe(dateValue, -8639999999999999);

    let date = new Date(dateValue);
    shouldBe(date.getUTCFullYear(), -271821);
    shouldBe(date.getUTCMonth(), 3);
    shouldBe(date.getUTCDate(), 20);
    shouldBe(date.getUTCHours(), 0);
    shouldBe(date.getUTCMinutes(), 0);
    shouldBe(date.getUTCSeconds(), 0);
    shouldBe(date.getUTCMilliseconds(), 1);
}

{
    let dateValue = Date.UTC(-271821, 3, 20, 0, 0, 0, 1);
    shouldBe(dateValue, -8639999999999999);

    let date = new Date(dateValue);
    shouldBe(date.getUTCFullYear(), -271821);
    shouldBe(date.getUTCMonth(), 3);
    shouldBe(date.getUTCDate(), 20);
    shouldBe(date.getUTCHours(), 0);
    shouldBe(date.getUTCMinutes(), 0);
    shouldBe(date.getUTCSeconds(), 0);
    shouldBe(date.getUTCMilliseconds(), 1);
}

{
    let dateValue = Date.parse("-271821-04-19T23:59:59.999Z");
    shouldBeNaN(dateValue);
}

{
    let dateValue = Date.UTC(-271821, 3, 19, 23, 59, 59, 999);
    shouldBeNaN(dateValue);
}
