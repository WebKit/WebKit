description(
"test of JavaScript date parsing (comments in parentheses)"
);

function testDateParse(date, numericResult)
{
    if (numericResult == "NaN") {
        shouldBeNaN('Date.parse("' + date + '")');
        shouldBeNaN('Date.parse("' + date.toUpperCase() + '")');
        shouldBeNaN('Date.parse("' + date.toLowerCase() + '")');
    } else {
        shouldBeTrue('Date.parse("' + date + '") == ' + numericResult);
        shouldBeTrue('Date.parse("' + date.toUpperCase() + '") == ' + numericResult);
        shouldBeTrue('Date.parse("' + date.toLowerCase() + '") == ' + numericResult);
    }
}

var timeZoneOffset = Date.parse(" Dec 25 1995 1:30 ") - Date.parse(" Dec 25 1995 1:30 GMT ");

testDateParse("Dec ((27) 26 (24)) 25 1995 1:30 PM UTC", "819898200000");
testDateParse("Dec 25 1995 1:30 PM UTC (", "819898200000");
testDateParse("Dec 25 1995 1:30 (PM)) UTC", "NaN");
testDateParse("(Nov) Dec (24) 25 (26) 1995 (1996) GMT (EST)", "819849600000");
testDateParse("(Nov) Dec (24) 25 (26) 1995 (1996)", "819849600000 + timeZoneOffset");

testDateParse("(Nov) Dec (24) 25 (26) 1995 (1996) 1:30 (1:40) GMT (EST)", "819855000000");
testDateParse("(Nov) Dec (24) 25 (26) 1995 (1996) 1:30 (1:40)", "819855000000 + timeZoneOffset");
testDateParse("(Nov) Dec (24) 25 (26) 1995 (1996) 1:30 ", "819855000000 + timeZoneOffset");
testDateParse("(Nov) Dec (24) 25 (26) 1995 (1996) 1:30 AM (1:40 PM) GMT (EST)", "819855000000");
testDateParse("(Nov) Dec (24) 25 (26) 1995 (1996) 1:30 AM (1:40 PM)", "819855000000 + timeZoneOffset");
testDateParse("Dec 25 1995 1:30( )AM (PM)", "NaN");
testDateParse("Dec 25 1995 1:30 AM (PM)", "819855000000 + timeZoneOffset");

testDateParse("(Nov) Dec (24) 25 (26) 1995 (1996) 13:30 (13:40) GMT (PST)", "819898200000");
testDateParse("(Nov) Dec (24) 25 (26) 1995 (1996) 13:30 (13:40)", "819898200000 + timeZoneOffset");
testDateParse('(Nov) Dec (24) 25 (26) 13:30 (13:40) 1995 (1996)', "819898200000 + timeZoneOffset");
testDateParse("(Nov) Dec (24) 25 (26) 1995 (1996) 13:30 (13:40) ", "819898200000 + timeZoneOffset");
testDateParse("(Nov) Dec (24) 25 (26) 1995 (1996) 1:30 (1:40) PM (AM) GMT (PST)", "819898200000");
testDateParse("(Nov) Dec (24) 25 (26) 1995 (1996) 1:30 (1:40) PM (AM)", "819898200000 + timeZoneOffset");
testDateParse("Dec 25 1995 1:30(AM)PM", "NaN");
testDateParse("Dec 25 1995 1:30 (AM)PM ", "819898200000 + timeZoneOffset");

testDateParse("Dec 25 1995 (PDT)UTC(PST)", "819849600000");
testDateParse("Dec 25 1995 (PDT)UT(PST)", "819849600000");
testDateParse("Dec 25 1995 (UTC)PST(GMT)", "819878400000");
testDateParse("Dec 25 1995 (UTC)PDT(GMT)", "819874800000");

testDateParse("Dec 25 1995 1:30 (PDT)UTC(PST)", "819855000000");
testDateParse("Dec 25 1995 1:30 (PDT)UT(PST)", "819855000000");
testDateParse("Dec 25 1995 1:30 (UTC)PST(GMT)", "819883800000");
testDateParse("Dec 25 1995 1:30 (UTC)PDT(GMT)", "819880200000");

testDateParse("Dec 25 1995 1:30 (AM) PM (PST) UTC", "819898200000");
testDateParse("Dec 25 1995 1:30 PM (AM) (PST) UT", "819898200000");
testDateParse("Dec 25 1995 1:30 PM (AM) (UTC) PST", "819927000000");
testDateParse("Dec 25 1995 1:30 (AM) PM PDT (UTC)", "819923400000");

testDateParse("Dec 25 1995 XXX (GMT)", "NaN");
testDateParse("Dec 25 1995 1:30 XXX (GMT)", "NaN");

testDateParse("Dec 25 1995 1:30 U(TC)", "NaN");
testDateParse("Dec 25 1995 1:30 V(UTC)", "NaN");
testDateParse("Dec 25 1995 1:30 (UTC)W", "NaN");
testDateParse("Dec 25 1995 1:30 (GMT)X", "NaN");

testDateParse("Dec 25 1995 0:30 (PM) GMT", "819851400000");
testDateParse("Dec 25 1995 (1)0:30 AM GMT", "819851400000");
testDateParse("Dec 25 1995 (1)0:30 PM GMT", "819894600000");

testDateParse("Anf(Dec) 25 1995 GMT", "NaN");

testDateParse("(Sat) Wed (Nov) Dec (Nov) 25 1995 1:30 GMT", "819855000000");
testDateParse("Wed (comment 1) (comment 2) Dec 25 1995 1:30 GMT", "819855000000");
testDateParse("Wed(comment 1) (comment 2) Dec 25 1995 1:30 GMT", "819855000000");
testDateParse("We(comment 1) (comment 2) Dec 25 1995 1:30 GMT", "819855000000");

var successfullyParsed = true;
