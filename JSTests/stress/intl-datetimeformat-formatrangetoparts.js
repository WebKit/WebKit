function shouldBe(actual, expected) {
    // Tolerate different space characters used by different ICU versions.
    // Older ICU uses U+2009 Thin Space in ranges, whereas newer ICU uses
    // regular old U+0020. Let's ignore these differences.
    if (typeof actual === 'string')
        actual = actual.replaceAll(' ', ' ');

    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected value: ' + expected);
}

function compareParts(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (var i = 0; i < actual.length; ++i) {
        shouldBe(actual[i].type, expected[i].type);
        shouldBe(actual[i].value, expected[i].value);
        shouldBe(actual[i].source, expected[i].source);
    }
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

function test() {
    let date1 = new Date(Date.UTC(2007, 0, 10, 10, 0, 0));
    let date2 = new Date(Date.UTC(2007, 0, 10, 11, 0, 0));
    let date3 = new Date(Date.UTC(2007, 0, 20, 10, 0, 0));
    let date4 = new Date(Date.UTC(2008, 0, 20, 10, 0, 0));

    {
        let fmt0 = new Intl.DateTimeFormat("en", {
            year: '2-digit',
            month: 'numeric',
            day: 'numeric',
            hour: 'numeric',
            minute: 'numeric',
            timeZone: 'America/Los_Angeles',
        });

        shouldThrow(() => {
            fmt0.formatRangeToParts();
        }, `TypeError: startDate or endDate is undefined`);

        shouldThrow(() => {
            fmt0.formatRangeToParts(new Date(Date.UTC(2008, 0, 20, 10, 0, 0)), new Date(Date.UTC(2007, 0, 10, 10, 0, 0)));
        }, `RangeError: startDate is larger than endDate`);
    }

    {
        let fmt1 = new Intl.DateTimeFormat("en", {
            year: '2-digit',
            month: 'numeric',
            day: 'numeric',
            hour: 'numeric',
            minute: 'numeric',
            timeZone: 'America/Los_Angeles',
        });
        shouldBe(fmt1.format(date1), `1/10/07, 2:00 AM`);
        compareParts(fmt1.formatRangeToParts(date1, date1), [
            {"type":"month","value":"1","source":"shared"},
            {"type":"literal","value":"/","source":"shared"},
            {"type":"day","value":"10","source":"shared"},
            {"type":"literal","value":"/","source":"shared"},
            {"type":"year","value":"07","source":"shared"},
            {"type":"literal","value":", ","source":"shared"},
            {"type":"hour","value":"2","source":"shared"},
            {"type":"literal","value":":","source":"shared"},
            {"type":"minute","value":"00","source":"shared"},
            {"type":"literal","value":" ","source":"shared"},
            {"type":"dayPeriod","value":"AM","source":"shared"},
        ]);
        compareParts(fmt1.formatRangeToParts(date1, date2), [
            {"type":"month","value":"1","source":"shared"},
            {"type":"literal","value":"/","source":"shared"},
            {"type":"day","value":"10","source":"shared"},
            {"type":"literal","value":"/","source":"shared"},
            {"type":"year","value":"07","source":"shared"},
            {"type":"literal","value":", ","source":"shared"},
            {"type":"hour","value":"2","source":"startRange"},
            {"type":"literal","value":":","source":"startRange"},
            {"type":"minute","value":"00","source":"startRange"},
            {"type":"literal","value":" – ","source":"shared"},
            {"type":"hour","value":"3","source":"endRange"},
            {"type":"literal","value":":","source":"endRange"},
            {"type":"minute","value":"00","source":"endRange"},
            {"type":"literal","value":" ","source":"shared"},
            {"type":"dayPeriod","value":"AM","source":"shared"},
        ]);
        compareParts(fmt1.formatRangeToParts(date1, date3), [
            {"type":"month","value":"1","source":"startRange"},
            {"type":"literal","value":"/","source":"startRange"},
            {"type":"day","value":"10","source":"startRange"},
            {"type":"literal","value":"/","source":"startRange"},
            {"type":"year","value":"07","source":"startRange"},
            {"type":"literal","value":", ","source":"startRange"},
            {"type":"hour","value":"2","source":"startRange"},
            {"type":"literal","value":":","source":"startRange"},
            {"type":"minute","value":"00","source":"startRange"},
            {"type":"literal","value":" ","source":"startRange"},
            {"type":"dayPeriod","value":"AM","source":"startRange"},
            {"type":"literal","value":" – ","source":"shared"},
            {"type":"month","value":"1","source":"endRange"},
            {"type":"literal","value":"/","source":"endRange"},
            {"type":"day","value":"20","source":"endRange"},
            {"type":"literal","value":"/","source":"endRange"},
            {"type":"year","value":"07","source":"endRange"},
            {"type":"literal","value":", ","source":"endRange"},
            {"type":"hour","value":"2","source":"endRange"},
            {"type":"literal","value":":","source":"endRange"},
            {"type":"minute","value":"00","source":"endRange"},
            {"type":"literal","value":" ","source":"endRange"},
            {"type":"dayPeriod","value":"AM","source":"endRange"},
        ]);
    }

    {
        let fmt1 = new Intl.DateTimeFormat("en", {
            year: '2-digit',
            month: 'numeric',
            day: 'numeric',
            timeZone: 'America/Los_Angeles',
        });
        shouldBe(fmt1.format(date1), `1/10/07`);
        compareParts(fmt1.formatRangeToParts(date1, date1), [
            {"type":"month","value":"1","source":"shared"},
            {"type":"literal","value":"/","source":"shared"},
            {"type":"day","value":"10","source":"shared"},
            {"type":"literal","value":"/","source":"shared"},
            {"type":"year","value":"07","source":"shared"},
        ]);
        compareParts(fmt1.formatRangeToParts(date1, date2), [
            {"type":"month","value":"1","source":"shared"},
            {"type":"literal","value":"/","source":"shared"},
            {"type":"day","value":"10","source":"shared"},
            {"type":"literal","value":"/","source":"shared"},
            {"type":"year","value":"07","source":"shared"},
        ]);
        compareParts(fmt1.formatRangeToParts(date1, date3), [
            {"type":"month","value":"1","source":"startRange"},
            {"type":"literal","value":"/","source":"startRange"},
            {"type":"day","value":"10","source":"startRange"},
            {"type":"literal","value":"/","source":"startRange"},
            {"type":"year","value":"07","source":"startRange"},
            {"type":"literal","value":" – ","source":"shared"},
            {"type":"month","value":"1","source":"endRange"},
            {"type":"literal","value":"/","source":"endRange"},
            {"type":"day","value":"20","source":"endRange"},
            {"type":"literal","value":"/","source":"endRange"},
            {"type":"year","value":"07","source":"endRange"},
        ]);
    }


    {
        let fmt1 = new Intl.DateTimeFormat("en", {
            year: 'numeric',
            timeZone: 'America/Los_Angeles',
        });
        shouldBe(fmt1.format(date1), `2007`);
        compareParts(fmt1.formatRangeToParts(date1, date1), [
            {"type":"year","value":"2007","source":"shared"},
        ]);
        compareParts(fmt1.formatRangeToParts(date1, date2), [
            {"type":"year","value":"2007","source":"shared"},
        ]);
        compareParts(fmt1.formatRangeToParts(date1, date4), [
            {"type":"year","value":"2007","source":"startRange"},
            {"type":"literal","value":" – ","source":"shared"},
            {"type":"year","value":"2008","source":"endRange"},
        ]);
    }

    {
        let fmt1 = new Intl.DateTimeFormat("en", {
            year: '2-digit',
            month: 'numeric',
            day: 'numeric',
            hour: 'numeric',
            minute: 'numeric',
            timeZone: 'UTC',
        });
        shouldBe(fmt1.format(date1), `1/10/07, 10:00 AM`);
        compareParts(fmt1.formatRangeToParts(date1, date1), [
            {"type":"month","value":"1","source":"shared"},
            {"type":"literal","value":"/","source":"shared"},
            {"type":"day","value":"10","source":"shared"},
            {"type":"literal","value":"/","source":"shared"},
            {"type":"year","value":"07","source":"shared"},
            {"type":"literal","value":", ","source":"shared"},
            {"type":"hour","value":"10","source":"shared"},
            {"type":"literal","value":":","source":"shared"},
            {"type":"minute","value":"00","source":"shared"},
            {"type":"literal","value":" ","source":"shared"},
            {"type":"dayPeriod","value":"AM","source":"shared"},
        ]);
        compareParts(fmt1.formatRangeToParts(date1, date2), [
            {"type":"month","value":"1","source":"shared"},
            {"type":"literal","value":"/","source":"shared"},
            {"type":"day","value":"10","source":"shared"},
            {"type":"literal","value":"/","source":"shared"},
            {"type":"year","value":"07","source":"shared"},
            {"type":"literal","value":", ","source":"shared"},
            {"type":"hour","value":"10","source":"startRange"},
            {"type":"literal","value":":","source":"startRange"},
            {"type":"minute","value":"00","source":"startRange"},
            {"type":"literal","value":" – ","source":"shared"},
            {"type":"hour","value":"11","source":"endRange"},
            {"type":"literal","value":":","source":"endRange"},
            {"type":"minute","value":"00","source":"endRange"},
            {"type":"literal","value":" ","source":"shared"},
            {"type":"dayPeriod","value":"AM","source":"shared"},
        ]);
        compareParts(fmt1.formatRangeToParts(date1, date3), [
            {"type":"month","value":"1","source":"startRange"},
            {"type":"literal","value":"/","source":"startRange"},
            {"type":"day","value":"10","source":"startRange"},
            {"type":"literal","value":"/","source":"startRange"},
            {"type":"year","value":"07","source":"startRange"},
            {"type":"literal","value":", ","source":"startRange"},
            {"type":"hour","value":"10","source":"startRange"},
            {"type":"literal","value":":","source":"startRange"},
            {"type":"minute","value":"00","source":"startRange"},
            {"type":"literal","value":" ","source":"startRange"},
            {"type":"dayPeriod","value":"AM","source":"startRange"},
            {"type":"literal","value":" – ","source":"shared"},
            {"type":"month","value":"1","source":"endRange"},
            {"type":"literal","value":"/","source":"endRange"},
            {"type":"day","value":"20","source":"endRange"},
            {"type":"literal","value":"/","source":"endRange"},
            {"type":"year","value":"07","source":"endRange"},
            {"type":"literal","value":", ","source":"endRange"},
            {"type":"hour","value":"10","source":"endRange"},
            {"type":"literal","value":":","source":"endRange"},
            {"type":"minute","value":"00","source":"endRange"},
            {"type":"literal","value":" ","source":"endRange"},
            {"type":"dayPeriod","value":"AM","source":"endRange"},
        ]);
    }

    {
        let fmt2 = new Intl.DateTimeFormat("en", {
            year: 'numeric',
            month: 'short',
            day: 'numeric',
            timeZone: 'America/Los_Angeles',
        });
        shouldBe(fmt2.format(date1), `Jan 10, 2007`);
        compareParts(fmt2.formatRangeToParts(date1, date1), [
            {"type":"month","value":"Jan","source":"shared"},
            {"type":"literal","value":" ","source":"shared"},
            {"type":"day","value":"10","source":"shared"},
            {"type":"literal","value":", ","source":"shared"},
            {"type":"year","value":"2007","source":"shared"},
        ]);
        compareParts(fmt2.formatRangeToParts(date1, date2), [
            {"type":"month","value":"Jan","source":"shared"},
            {"type":"literal","value":" ","source":"shared"},
            {"type":"day","value":"10","source":"shared"},
            {"type":"literal","value":", ","source":"shared"},
            {"type":"year","value":"2007","source":"shared"},
        ]);
        compareParts(fmt2.formatRangeToParts(date1, date3), [
            {"type":"month","value":"Jan","source":"shared"},
            {"type":"literal","value":" ","source":"shared"},
            {"type":"day","value":"10","source":"startRange"},
            {"type":"literal","value":" – ","source":"shared"},
            {"type":"day","value":"20","source":"endRange"},
            {"type":"literal","value":", ","source":"shared"},
            {"type":"year","value":"2007","source":"shared"},
        ]);
    }
    {
        const date1 = new Date("2019-01-03T00:00:00");
        const date2 = new Date("2019-01-05T00:00:00");
        const date3 = new Date("2019-03-04T00:00:00");
        const date4 = new Date("2020-03-04T00:00:00");

        let dtf = new Intl.DateTimeFormat("en-US");
        compareParts(dtf.formatRangeToParts(date1, date1), [
            {"type":"month","value":"1","source":"shared"},
            {"type":"literal","value":"/","source":"shared"},
            {"type":"day","value":"3","source":"shared"},
            {"type":"literal","value":"/","source":"shared"},
            {"type":"year","value":"2019","source":"shared"},
        ]);
        compareParts(dtf.formatRangeToParts(date1, date2), [
            {"type":"month","value":"1","source":"startRange"},
            {"type":"literal","value":"/","source":"startRange"},
            {"type":"day","value":"3","source":"startRange"},
            {"type":"literal","value":"/","source":"startRange"},
            {"type":"year","value":"2019","source":"startRange"},
            {"type":"literal","value":" – ","source":"shared"},
            {"type":"month","value":"1","source":"endRange"},
            {"type":"literal","value":"/","source":"endRange"},
            {"type":"day","value":"5","source":"endRange"},
            {"type":"literal","value":"/","source":"endRange"},
            {"type":"year","value":"2019","source":"endRange"},
        ]);
        compareParts(dtf.formatRangeToParts(date1, date3), [
            {"type":"month","value":"1","source":"startRange"},
            {"type":"literal","value":"/","source":"startRange"},
            {"type":"day","value":"3","source":"startRange"},
            {"type":"literal","value":"/","source":"startRange"},
            {"type":"year","value":"2019","source":"startRange"},
            {"type":"literal","value":" – ","source":"shared"},
            {"type":"month","value":"3","source":"endRange"},
            {"type":"literal","value":"/","source":"endRange"},
            {"type":"day","value":"4","source":"endRange"},
            {"type":"literal","value":"/","source":"endRange"},
            {"type":"year","value":"2019","source":"endRange"},
        ]);
        compareParts(dtf.formatRangeToParts(date1, date4), [
            {"type":"month","value":"1","source":"startRange"},
            {"type":"literal","value":"/","source":"startRange"},
            {"type":"day","value":"3","source":"startRange"},
            {"type":"literal","value":"/","source":"startRange"},
            {"type":"year","value":"2019","source":"startRange"},
            {"type":"literal","value":" – ","source":"shared"},
            {"type":"month","value":"3","source":"endRange"},
            {"type":"literal","value":"/","source":"endRange"},
            {"type":"day","value":"4","source":"endRange"},
            {"type":"literal","value":"/","source":"endRange"},
            {"type":"year","value":"2020","source":"endRange"},
        ]);
        compareParts(dtf.formatRangeToParts(date2, date3), [
            {"type":"month","value":"1","source":"startRange"},
            {"type":"literal","value":"/","source":"startRange"},
            {"type":"day","value":"5","source":"startRange"},
            {"type":"literal","value":"/","source":"startRange"},
            {"type":"year","value":"2019","source":"startRange"},
            {"type":"literal","value":" – ","source":"shared"},
            {"type":"month","value":"3","source":"endRange"},
            {"type":"literal","value":"/","source":"endRange"},
            {"type":"day","value":"4","source":"endRange"},
            {"type":"literal","value":"/","source":"endRange"},
            {"type":"year","value":"2019","source":"endRange"},
        ]);
        compareParts(dtf.formatRangeToParts(date2, date4), [
            {"type":"month","value":"1","source":"startRange"},
            {"type":"literal","value":"/","source":"startRange"},
            {"type":"day","value":"5","source":"startRange"},
            {"type":"literal","value":"/","source":"startRange"},
            {"type":"year","value":"2019","source":"startRange"},
            {"type":"literal","value":" – ","source":"shared"},
            {"type":"month","value":"3","source":"endRange"},
            {"type":"literal","value":"/","source":"endRange"},
            {"type":"day","value":"4","source":"endRange"},
            {"type":"literal","value":"/","source":"endRange"},
            {"type":"year","value":"2020","source":"endRange"},
        ]);
        compareParts(dtf.formatRangeToParts(date3, date4), [
            {"type":"month","value":"3","source":"startRange"},
            {"type":"literal","value":"/","source":"startRange"},
            {"type":"day","value":"4","source":"startRange"},
            {"type":"literal","value":"/","source":"startRange"},
            {"type":"year","value":"2019","source":"startRange"},
            {"type":"literal","value":" – ","source":"shared"},
            {"type":"month","value":"3","source":"endRange"},
            {"type":"literal","value":"/","source":"endRange"},
            {"type":"day","value":"4","source":"endRange"},
            {"type":"literal","value":"/","source":"endRange"},
            {"type":"year","value":"2020","source":"endRange"},
        ]);

        dtf = new Intl.DateTimeFormat("en-US", {year: "numeric", month: "short", day: "numeric"});
        compareParts(dtf.formatRangeToParts(date1, date1), [
            {"type":"month","value":"Jan","source":"shared"},
            {"type":"literal","value":" ","source":"shared"},
            {"type":"day","value":"3","source":"shared"},
            {"type":"literal","value":", ","source":"shared"},
            {"type":"year","value":"2019","source":"shared"},
        ]);
        compareParts(dtf.formatRangeToParts(date1, date2), [
            {"type":"month","value":"Jan","source":"shared"},
            {"type":"literal","value":" ","source":"shared"},
            {"type":"day","value":"3","source":"startRange"},
            {"type":"literal","value":" – ","source":"shared"},
            {"type":"day","value":"5","source":"endRange"},
            {"type":"literal","value":", ","source":"shared"},
            {"type":"year","value":"2019","source":"shared"},
        ]);
        compareParts(dtf.formatRangeToParts(date1, date3), [
            {"type":"month","value":"Jan","source":"startRange"},
            {"type":"literal","value":" ","source":"startRange"},
            {"type":"day","value":"3","source":"startRange"},
            {"type":"literal","value":" – ","source":"shared"},
            {"type":"month","value":"Mar","source":"endRange"},
            {"type":"literal","value":" ","source":"endRange"},
            {"type":"day","value":"4","source":"endRange"},
            {"type":"literal","value":", ","source":"shared"},
            {"type":"year","value":"2019","source":"shared"},
        ]);
        compareParts(dtf.formatRangeToParts(date1, date4), [
            {"type":"month","value":"Jan","source":"startRange"},
            {"type":"literal","value":" ","source":"startRange"},
            {"type":"day","value":"3","source":"startRange"},
            {"type":"literal","value":", ","source":"startRange"},
            {"type":"year","value":"2019","source":"startRange"},
            {"type":"literal","value":" – ","source":"shared"},
            {"type":"month","value":"Mar","source":"endRange"},
            {"type":"literal","value":" ","source":"endRange"},
            {"type":"day","value":"4","source":"endRange"},
            {"type":"literal","value":", ","source":"endRange"},
            {"type":"year","value":"2020","source":"endRange"},
        ]);
        compareParts(dtf.formatRangeToParts(date2, date3), [
            {"type":"month","value":"Jan","source":"startRange"},
            {"type":"literal","value":" ","source":"startRange"},
            {"type":"day","value":"5","source":"startRange"},
            {"type":"literal","value":" – ","source":"shared"},
            {"type":"month","value":"Mar","source":"endRange"},
            {"type":"literal","value":" ","source":"endRange"},
            {"type":"day","value":"4","source":"endRange"},
            {"type":"literal","value":", ","source":"shared"},
            {"type":"year","value":"2019","source":"shared"},
        ]);
        compareParts(dtf.formatRangeToParts(date2, date4), [
            {"type":"month","value":"Jan","source":"startRange"},
            {"type":"literal","value":" ","source":"startRange"},
            {"type":"day","value":"5","source":"startRange"},
            {"type":"literal","value":", ","source":"startRange"},
            {"type":"year","value":"2019","source":"startRange"},
            {"type":"literal","value":" – ","source":"shared"},
            {"type":"month","value":"Mar","source":"endRange"},
            {"type":"literal","value":" ","source":"endRange"},
            {"type":"day","value":"4","source":"endRange"},
            {"type":"literal","value":", ","source":"endRange"},
            {"type":"year","value":"2020","source":"endRange"},
        ]);
        compareParts(dtf.formatRangeToParts(date3, date4), [
            {"type":"month","value":"Mar","source":"startRange"},
            {"type":"literal","value":" ","source":"startRange"},
            {"type":"day","value":"4","source":"startRange"},
            {"type":"literal","value":", ","source":"startRange"},
            {"type":"year","value":"2019","source":"startRange"},
            {"type":"literal","value":" – ","source":"shared"},
            {"type":"month","value":"Mar","source":"endRange"},
            {"type":"literal","value":" ","source":"endRange"},
            {"type":"day","value":"4","source":"endRange"},
            {"type":"literal","value":", ","source":"endRange"},
            {"type":"year","value":"2020","source":"endRange"},
        ]);
    }
}

if (Intl.DateTimeFormat.prototype.formatRangeToParts)
    test();
