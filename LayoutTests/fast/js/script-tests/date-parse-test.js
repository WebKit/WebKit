description(
'Test of JavaScript date parsing.'
);

function testDateParseExpr(dateExpr, numericResult)
{
    if (numericResult == "NaN") {
        shouldBeNaN('Date.parse(' + dateExpr + ')');
        shouldBeNaN('Date.parse((' + dateExpr + ').toUpperCase())');
        shouldBeNaN('Date.parse((' + dateExpr + ').toLowerCase())');
    } else {
        shouldBeTrue('Date.parse(' + dateExpr + ') == ' + numericResult);
        shouldBeTrue('Date.parse((' + dateExpr + ').toUpperCase()) == ' + numericResult);
        shouldBeTrue('Date.parse((' + dateExpr + ').toLowerCase()) == ' + numericResult);
    }
}

function testDateParse(date, numericResult)
{
    if (numericResult == "NaN") {
        shouldBeNaN('Date.parse("' + date + '")');
        shouldBeNaN('Date.parse("' + date.toUpperCase() + '")');
        shouldBeNaN('Date.parse("' + date.toLowerCase() + '")');
    } else {
        shouldBeTrue('Date.parse("' + date + '") == ' + numericResult.toString());
        shouldBeTrue('Date.parse("' + date.toUpperCase() + '") == ' + numericResult);
        shouldBeTrue('Date.parse("' + date.toLowerCase() + '") == ' + numericResult);
    }
}

var timeZoneOffset = Date.parse(" Dec 25 1995 1:30 ") - Date.parse(" Dec 25 1995 1:30 GMT ");

testDateParse("Dec 25 1995 GMT", "819849600000");
testDateParse("Dec 25 1995", "819849600000 + timeZoneOffset");

testDateParse("Dec 25 1995 1:30 GMT", "819855000000");
testDateParse("Dec 25 1995 1:30", "819855000000 + timeZoneOffset");
testDateParse("Dec 25 1995 1:30 ", "819855000000 + timeZoneOffset");
testDateParse("Dec 25 1995 1:30 AM GMT", "819855000000");
testDateParse("Dec 25 1995 1:30 AM", "819855000000 + timeZoneOffset");
testDateParse("Dec 25 1995 1:30AM", "NaN");
testDateParse("Dec 25 1995 1:30 AM ", "819855000000 + timeZoneOffset");

testDateParse("Dec 25 1995 13:30 GMT", "819898200000");
testDateParse("Dec 25 1995 13:30", "819898200000 + timeZoneOffset");
testDateParse('Dec 25 13:30 1995', "819898200000 + timeZoneOffset");
testDateParse("Dec 25 1995 13:30 ", "819898200000 + timeZoneOffset");
testDateParse("Dec 25 1995 1:30 PM GMT", "819898200000");
testDateParse("Dec 25 1995 1:30 PM", "819898200000 + timeZoneOffset");
testDateParse("Dec 25 1995 1:30PM", "NaN");
testDateParse("Dec 25 1995 1:30 PM ", "819898200000 + timeZoneOffset");

testDateParse("Dec 25 1995 UTC", "819849600000");
testDateParse("Dec 25 1995 UT", "819849600000");
testDateParse("Dec 25 1995 PST", "819878400000");
testDateParse("Dec 25 1995 PDT", "819874800000");

testDateParse("Dec 25 1995 1:30 UTC", "819855000000");
testDateParse("Dec 25 1995 1:30 UT", "819855000000");
testDateParse("Dec 25 1995 1:30 PST", "819883800000");
testDateParse("Dec 25 1995 1:30 PDT", "819880200000");

testDateParse("Dec 25 1995 1:30 PM UTC", "819898200000");
testDateParse("Dec 25 1995 1:30 PM UT", "819898200000");
testDateParse("Dec 25 1995 1:30 PM PST", "819927000000");
testDateParse("Dec 25 1995 1:30 PM PDT", "819923400000");

testDateParse("Dec 25 1995 XXX", "NaN");
testDateParse("Dec 25 1995 1:30 XXX", "NaN");

testDateParse("Dec 25 1995 1:30 U", "NaN");
testDateParse("Dec 25 1995 1:30 V", "NaN");
testDateParse("Dec 25 1995 1:30 W", "NaN");
testDateParse("Dec 25 1995 1:30 X", "NaN");

testDateParse("Dec 25 1995 0:30 GMT", "819851400000");
testDateParse("Dec 25 1995 0:30 AM GMT", "819851400000");
testDateParse("Dec 25 1995 0:30 PM GMT", "819894600000");
testDateParse("Dec 25 1995 12:30 AM GMT", "819851400000");
testDateParse("Dec 25 1995 12:30 PM GMT", "819894600000");
testDateParse("Dec 25 1995 13:30 AM GMT", "NaN");
testDateParse("Dec 25 1995 13:30 PM GMT", "NaN");

testDateParse("Anf 25 1995 GMT", "NaN");

testDateParse("Wed Dec 25 1995 1:30 GMT", "819855000000");

testDateParseExpr('"Dec 25" + String.fromCharCode(9) + "1995 13:30 GMT"', "819898200000");
testDateParseExpr('"Dec 25" + String.fromCharCode(10) + "1995 13:30 GMT"', "819898200000");

// Firefox compatibility
testDateParse("Dec 25, 1995 13:30", "819898200000 + timeZoneOffset");
testDateParse("Dec 25,1995 13:30", "819898200000 + timeZoneOffset");

testDateParse("Dec 25 1995, 13:30", "819898200000 + timeZoneOffset");
testDateParse("Dec 25 1995,13:30", "819898200000 + timeZoneOffset");

testDateParse("Dec 25, 1995, 13:30", "819898200000 + timeZoneOffset");
testDateParse("Dec 25,1995,13:30", "819898200000 + timeZoneOffset");

var successfullyParsed = true;
