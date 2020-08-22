function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
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

let fmt0 = new Intl.DateTimeFormat("en", {
    year: '2-digit',
    month: 'numeric',
    day: 'numeric',
    hour: 'numeric',
    minute: 'numeric',
    timeZone: 'America/Los_Angeles',
});

shouldThrow(() => {
    fmt0.formatRange();
}, `TypeError: startDate or endDate is undefined`);


function test() {
    let range = " – "; // This is not usual space unfortuantely in ICU 66.
    if ($vm.icuVersion() > 66)
        range = " – ";

    let date1 = new Date(Date.UTC(2007, 0, 10, 10, 0, 0));
    let date2 = new Date(Date.UTC(2007, 0, 10, 11, 0, 0));
    let date3 = new Date(Date.UTC(2007, 0, 20, 10, 0, 0));
    let date4 = new Date(Date.UTC(2008, 0, 20, 10, 0, 0));

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
        shouldBe(fmt1.formatRange(date1, date2), `1/10/07, 2:00${range}3:00 AM`);
        shouldBe(fmt1.formatRange(date1, date3), `1/10/07, 2:00 AM${range}1/20/07, 2:00 AM`);
    }

    {
        let fmt1 = new Intl.DateTimeFormat("en", {
            year: '2-digit',
            month: 'numeric',
            day: 'numeric',
            timeZone: 'America/Los_Angeles',
        });
        shouldBe(fmt1.format(date1), `1/10/07`);
        shouldBe(fmt1.formatRange(date1, date2), `1/10/07`);
        shouldBe(fmt1.formatRange(date1, date3), `1/10/07${range}1/20/07`);
    }


    {
        let fmt1 = new Intl.DateTimeFormat("en", {
            year: 'numeric',
            timeZone: 'America/Los_Angeles',
        });
        shouldBe(fmt1.format(date1), `2007`);
        shouldBe(fmt1.formatRange(date1, date2), `2007`);
        shouldBe(fmt1.formatRange(date1, date4), `2007${range}2008`);
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
        shouldBe(fmt1.formatRange(date1, date2), `1/10/07, 10:00${range}11:00 AM`);
        shouldBe(fmt1.formatRange(date1, date3), `1/10/07, 10:00 AM${range}1/20/07, 10:00 AM`);
    }

    {
        let fmt2 = new Intl.DateTimeFormat("en", {
            year: 'numeric',
            month: 'short',
            day: 'numeric',
            timeZone: 'America/Los_Angeles',
        });
        shouldBe(fmt2.format(date1), `Jan 10, 2007`);
        shouldBe(fmt2.formatRange(date1, date2), `Jan 10, 2007`);
        shouldBe(fmt2.formatRange(date1, date3), `Jan 10${range}20, 2007`);
    }
    {
        const date1 = new Date("2019-01-03T00:00:00");
        const date2 = new Date("2019-01-05T00:00:00");
        const date3 = new Date("2019-03-04T00:00:00");
        const date4 = new Date("2020-03-04T00:00:00");

        let dtf = new Intl.DateTimeFormat("en-US");
        shouldBe(dtf.formatRange(date1, date1), "1/3/2019");
        shouldBe(dtf.formatRange(date1, date2), `1/3/2019${range}1/5/2019`);
        shouldBe(dtf.formatRange(date1, date3), `1/3/2019${range}3/4/2019`);
        shouldBe(dtf.formatRange(date1, date4), `1/3/2019${range}3/4/2020`);
        shouldBe(dtf.formatRange(date2, date3), `1/5/2019${range}3/4/2019`);
        shouldBe(dtf.formatRange(date2, date4), `1/5/2019${range}3/4/2020`);
        shouldBe(dtf.formatRange(date3, date4), `3/4/2019${range}3/4/2020`);

        dtf = new Intl.DateTimeFormat("en-US", {year: "numeric", month: "short", day: "numeric"});
        shouldBe(dtf.formatRange(date1, date1), `Jan 3, 2019`);
        shouldBe(dtf.formatRange(date1, date2), `Jan 3${range}5, 2019`);
        shouldBe(dtf.formatRange(date1, date3), `Jan 3${range}Mar 4, 2019`);
        shouldBe(dtf.formatRange(date1, date4), `Jan 3, 2019${range}Mar 4, 2020`);
        shouldBe(dtf.formatRange(date2, date3), `Jan 5${range}Mar 4, 2019`);
        shouldBe(dtf.formatRange(date2, date4), `Jan 5, 2019${range}Mar 4, 2020`);
        shouldBe(dtf.formatRange(date3, date4), `Mar 4, 2019${range}Mar 4, 2020`);
    }
}

if ($vm.icuVersion() >= 64)
    test();
