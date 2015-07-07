debug("Start Of Test");

shouldBeTrue("isNaN(Date.prototype.valueOf())");

var d = new Date(1017492323515); // Sat Mar 30 13:45:23 GMT+0100 (CET) 2002
// shouldBe("d.getUTCHours()", "12");
// shouldBe("d.getHours()", "12");
shouldBe("d.getMinutes()", "45");
shouldBe("d.getUTCMinutes()", "45");
shouldBe("d.getDay()", "6");
shouldBe("d.getMonth()", "2");
shouldBe("d.getFullYear()", "2002");

// string/number conversions
shouldBe("Number(d)", "1017492323515");
shouldBe("Boolean(d)", "true");
shouldBe("(new Date(100)).valueOf()", "100");
shouldBe("(new Date(true)).valueOf()", "1");
shouldBe("(new Date(false)).valueOf()", "0");
shouldBe("typeof (new Date()).toString()", "'string'");
shouldBe("typeof ('' + new Date())", "'string'");
shouldBe("typeof (new Date() + new Date())", "'string'");
shouldBeTrue("isNaN(Number(new Date('foo')))");
shouldBe("new Date(10001) - new Date(10000)", "1");
shouldBe("'' - new Date(10000)", "-10000");
shouldBe("2 * new Date(10000)", "20000");
shouldBe("var d = new Date(); d == String(d)", "true");

var d0 = new Date(2004, 0, 1, 0, 0, 0, 0);
shouldBe("d0.getHours()","0");

var d1 = new Date(Date.UTC(2004, 0, 1, 0, 0, 0, 0));
shouldBe("d1.getUTCHours()","0");
d1.setUTCHours(1999,6,9);
d1.setUTCHours(11,22,33);
shouldBe("d1.getUTCHours()","11");

var d2 = new Date();
d2.setMilliseconds(11);
shouldBe("d2.getMilliseconds()", "11");
d2.setSeconds(11, 22);
shouldBe("d2.getSeconds()", "11");
shouldBe("d2.getMilliseconds()", "22");
d2.setMinutes(11, 22, 33);
shouldBe("d2.getMinutes()", "11");
shouldBe("d2.getSeconds()", "22");
shouldBe("d2.getMilliseconds()", "33");
d2.setHours(11, 22, 33, 44);
shouldBe("d2.getHours()", "11");
shouldBe("d2.getMinutes()", "22");
shouldBe("d2.getSeconds()", "33");
shouldBe("d2.getMilliseconds()", "44");
d2.setMonth(3, 20);
shouldBe("d2.getMonth()", "3");
shouldBe("d2.getDate()", "20");
d2.setFullYear(1976, 3, 20);
shouldBe("d2.getFullYear()", "1976");
shouldBe("d2.getMonth()", "3");
shouldBe("d2.getDate()", "20");

// ### fix: shouldBe("d2.setYear(-1), d2.getFullYear()", "-1");
shouldBe("d2.setYear(0), d2.getFullYear()", "1900");
shouldBe("d2.setYear(1), d2.getFullYear()", "1901");
shouldBe("d2.setYear(99), d2.getFullYear()", "1999");
shouldBe("d2.setYear(100), d2.getFullYear()", "100");
shouldBe("d2.setYear(2050), d2.getFullYear()", "2050");
shouldBe("d2.setYear(1899), d2.getFullYear()", "1899");
shouldBe("d2.setYear(2000), d2.getFullYear()", "2000");
shouldBe("d2.setYear(2100), d2.getFullYear()", "2100");

// date parsing
// from kdelibs/kdecore/tests/krfcdatetest.cpp
  var dateRef = new Date('Thu Nov 5 1994 18:15:30 GMT+0500');
  //  debug(dateRef);
  shouldBe( "dateRef.getDay()", "6"); // It was in fact a Saturday
  shouldBe( "dateRef.getDate()", "5");
  shouldBe( "dateRef.getMonth()", "10");
  shouldBe( "dateRef.getYear()", "94"); // like NS, and unlike IE, by default
  shouldBe( "dateRef.getFullYear()", "1994");
  shouldBe( "dateRef.getMinutes()", "15");
  shouldBe( "dateRef.getSeconds()", "30");
  shouldBe( "dateRef.getUTCDay()", "6"); // It was in fact a Saturday
  shouldBe( "dateRef.getUTCDate()", "5");
  shouldBe( "dateRef.getUTCMonth()", "10");
  shouldBe( "dateRef.getUTCFullYear()", "1994");
  shouldBe( "dateRef.getUTCHours()", "13");
  shouldBe( "dateRef.getUTCMinutes()", "15");
  shouldBe( "dateRef.getUTCSeconds()", "30");

  d = new Date('Thu Nov 5 1994 18:15:30 GMT+05:00');
  shouldBe( "d.toUTCString()", "dateRef.toUTCString()");
  shouldBe( "d.toUTCString().replace('GMT', '+0000')", "'Sat, 05 Nov 1994 13:15:30 +0000'"); // It was in fact a Saturday

  dateRef = new Date('Thu Nov 5 2065 18:15:30 GMT+0500');
  //  debug(dateRef);
  shouldBe( "dateRef.getDay()", "4");
  shouldBe( "dateRef.getDate()", "5");
  shouldBe( "dateRef.getMonth()", "10");
//  shouldBe( "dateRef.getYear()", "65"); // Should this be 65 or 165 ??
  shouldBe( "dateRef.getFullYear()", "2065");
  shouldBe( "dateRef.getMinutes()", "15");
  shouldBe( "dateRef.getSeconds()", "30");
  shouldBe( "dateRef.getUTCDay()", "4");
  shouldBe( "dateRef.getUTCDate()", "5");
  shouldBe( "dateRef.getUTCMonth()", "10");
  shouldBe( "dateRef.getUTCFullYear()", "2065");
  shouldBe( "dateRef.getUTCHours()", "13");
  shouldBe( "dateRef.getUTCMinutes()", "15");
  shouldBe( "dateRef.getUTCSeconds()", "30");

  dateRef = new Date('Wed Nov 5 2064 18:15:30 GMT+0500'); // Leap year
  //  debug(dateRef);
  shouldBe( "dateRef.getDay()", "3");
  shouldBe( "dateRef.getDate()", "5");
  shouldBe( "dateRef.getMonth()", "10");
//  shouldBe( "dateRef.getYear()", "64"); // Should this be 64 or 164 ??
  shouldBe( "dateRef.getFullYear()", "2064");
  shouldBe( "dateRef.getMinutes()", "15");
  shouldBe( "dateRef.getSeconds()", "30");
  shouldBe( "dateRef.getUTCDay()", "3");
  shouldBe( "dateRef.getUTCDate()", "5");
  shouldBe( "dateRef.getUTCMonth()", "10");
  shouldBe( "dateRef.getUTCFullYear()", "2064");
  shouldBe( "dateRef.getUTCHours()", "13");
  shouldBe( "dateRef.getUTCMinutes()", "15");
  shouldBe( "dateRef.getUTCSeconds()", "30");

/*
  // Shouldn't this work?
  dateRef = new Date('Sat Nov 5 1864 18:15:30 GMT+0500');
  //  debug(dateRef);
  shouldBe( "dateRef.getDay()", "6");
  shouldBe( "dateRef.getDate()", "5");
  shouldBe( "dateRef.getMonth()", "10");
//  shouldBe( "dateRef.getYear()", "64"); // Should this be 64 ??
  shouldBe( "dateRef.getFullYear()", "1864");
  shouldBe( "dateRef.getMinutes()", "15");
  shouldBe( "dateRef.getSeconds()", "30");
  shouldBe( "dateRef.getUTCDay()", "3");
  shouldBe( "dateRef.getUTCDate()", "5");
  shouldBe( "dateRef.getUTCMonth()", "10");
  shouldBe( "dateRef.getUTCFullYear()", "1864");
  shouldBe( "dateRef.getUTCHours()", "13");
  shouldBe( "dateRef.getUTCMinutes()", "15");
  shouldBe( "dateRef.getUTCSeconds()", "30");
*/

  d = new Date('Tue Nov 5 2024 18:15:30 GMT+05:00');
  shouldBe( "d.toUTCString().replace('GMT', '+0000')", "'Tue, 05 Nov 2024 13:15:30 +0000'");
  d = new Date('Mon Nov 5 2040 18:15:30 GMT+05:00');
  shouldBe( "d.toUTCString().replace('GMT', '+0000')", "'Mon, 05 Nov 2040 13:15:30 +0000'");
  d = new Date('Fri Nov 5 2100 18:15:30 GMT+05:00');
  shouldBe( "d.toUTCString().replace('GMT', '+0000')", "'Fri, 05 Nov 2100 13:15:30 +0000'");
  d = new Date('Fri Nov 5 2004 03:15:30 GMT+05:00'); // Timezone crosses day barrier
  shouldBe( "d.toUTCString().replace('GMT', '+0000')", "'Thu, 04 Nov 2004 22:15:30 +0000'");

// AM/PM
shouldBe("(new Date('Dec 25 1995 1:30 PM UTC')).valueOf()", "819898200000");
shouldBe("(new Date('Dec 25 1995 1:30 pm UTC')).valueOf()", "819898200000");
shouldBe("(new Date('Dec 25 1995 1:30 AM UTC')).valueOf()", "819855000000");
shouldBe("(new Date('Dec 25 1995 1:30 am UTC')).valueOf()", "819855000000");
shouldBe("(new Date('Dec 25 1995 12:00 PM UTC')).valueOf()", "819892800000");
shouldBe("(new Date('Dec 25 1995 12:00 AM UTC')).valueOf()", "819849600000");
shouldBe("(new Date('Dec 25 1995 00:00 AM UTC')).valueOf()", "819849600000");
shouldBe("(new Date('Dec 25 1995 00:00 PM UTC')).valueOf()", "819892800000");
shouldBeTrue("isNaN(new Date('Dec 25 1995 13:30 AM UTC')).valueOf()");

/*
  // Don't work in any other browsers
  d = new Date('Wednesday, 05-Nov-94 13:15:30 GMT');
  shouldBe( "d.toUTCString()", "dateRef.toUTCString()");

  d = new Date('Wed, 05-Nov-1994 13:15:30 GMT');
  shouldBe( "d.toUTCString()", "dateRef.toUTCString()");

  d = new Date('Wed, 05-November-1994 13:15:30 GMT');
  shouldBe( "d.toUTCString()", "dateRef.toUTCString()");

  // Works only in EST/EDT
  d = new Date('November 5, 1994 08:15:30');
  debug(d);
  shouldBe( "d.toUTCString()", "dateRef.toUTCString()");

  var dateRef2 = new Date('July 1, 2004 10:00 EDT');
  d = new Date('July 1, 2004 10:00');
  debug(d);
  shouldBe( "d.toUTCString()", "dateRef2.toUTCString()");

  shouldBe("new Date('Wednesday 09-Nov-99 13:12:40 GMT').getMonth()", "10"); // not parsed in moz
  shouldBe("new Date('Sat, 01-Dec-2000 08:00:00 GMT').getMonth()", "11"); // not parsed in moz
*/

shouldBe("new Date('Sat, 01 Dec 2000 08:00:00 GMT').getMonth()", "11");
shouldBe("new Date('01 Jan 99 22:00 +0100').getFullYear()", "1999");
shouldBe("new Date('May 09 1999 13:12:40 GMT').getDate()", "9");
shouldBe("new Date('Wednesday February 09 1999 13:12:40 GMT').getMonth()", "1");
shouldBe("new Date('Wednesday January 09 1999 13:12:40 GMT').getFullYear()", "1999");
shouldBe("new Date('Wednesday January 09 13:12:40 GMT 1999').getFullYear()", "1999");
shouldBe("new Date('Wednesday January 06 13:12:40 GMT 2100').getFullYear()", "2100");
shouldBe("(new Date('\\n21\\rFeb\\f\\v\\t 2004')).getFullYear()", "2004"); // ws
shouldBe("(new Date('Dec 25 1995 gmt')).valueOf()", "819849600000");
shouldBe("(new Date('Dec 25 1995 utc')).valueOf()", "819849600000");

// Those two fail in Konqueror, due to time_t being limited to 2037 !
// moved to evil-n.js shouldBe("new Date('3/31/2099').getFullYear()", "2099");
// moved to evil-n.js shouldBe("new Date('3/31/2099').getMonth()", "2");
//shouldBe("new Date('3/31/2099').getDate()",31);

shouldBe("new Date('3/31/2005').getDate()", "31");
shouldBe("new Date('3/31/2005').getHours()", "0");
shouldBe("new Date('7/31/2005').getHours()", "0"); // DST
shouldBe("new Date('3/31/2005 GMT').getFullYear()", "2005");
shouldBe("new Date('3/31/2005 12:45:15').getDate()", "31");
shouldBe("new Date('3/31/2005 12:45:15').getMonth()", "2");
shouldBe("new Date('3/31/2005 12:45:15').getSeconds()", "15");
shouldBe("new Date('2003/02/03').getMonth()", "1");
// ### not sure how to interpret this: new Date('25/12/2005') 
// ### shouldBe("new Date('1950/02/03').getFullYear()", "1950");
shouldBe("new Date('2003/02/03 02:01:04 UTC').getSeconds()", "4");

var jul27Str = 'July 27, 2003'
var jul27Num = new Date(jul27Str).valueOf();
shouldBe("jul27Num", "Date.parse(jul27Str).valueOf()");
// expect NaN, not undefined
shouldBe("typeof Date.parse(0)", "'number'");
shouldBeTrue("isNaN(Date.parse(0))");
// parse string *object*
shouldBe("Date.parse(new String(jul27Str)).valueOf()", "jul27Num");

// invalid dates
shouldBeTrue("isNaN(Number(new Date('01 ANF 2000')))"); // middle of JANFEB :)
shouldBeTrue("isNaN(Number(new Date('ANF 01 2000')))");

d = new Date("January 1, 2000");
var oldHours = d.getHours();
d.setMonth(8);
shouldBe("oldHours", "d.getHours()");

// some time values in different (implementation) ranges
shouldBe("Date.UTC(1800, 0, 1)", "-5364662400000");
shouldBe("Date.UTC(1800, 2, 1)", "-5359564800000"); // one day after Feb 28th
shouldBe("Date.UTC(1899, 0, 1)", "-2240524800000");
shouldBe("Date.UTC(1900, 0, 1)", "-2208988800000");
shouldBe("Date.UTC(1960, 0, 1)", "-315619200000");
shouldBe("Date.UTC(1970, 0, 1)", "0");
shouldBe("Date.UTC(3000, 0, 1)", "32503680000000");

// same dates as above
shouldBe("(new Date(-5364662400000)).valueOf()", "-5364662400000");
shouldBe("(new Date(-2240524800000)).valueOf()", "-2240524800000");
shouldBe("(new Date(-2208988800000)).valueOf()", "-2208988800000");
shouldBe("(new Date(-315619200000)).valueOf()", "-315619200000");
shouldBe("(new Date(0)).valueOf()", "0");
shouldBe("(new Date(32503680000000)).valueOf()", "32503680000000");

d = new Date(2010, 0, 1);
//shouldBe("d.valueOf()", "32503676400000");
shouldBe("d.getDay()", "5");

// large and small year numbers
d = new Date(3000, 0, 1);
//shouldBe("d.valueOf()", "32503676400000");
//shouldBe("(new Date(3000, 0, 1)).valueOf()", "Date.UTC(3000, 0, 1)");
shouldBeTrue("d.getYear() == 1100 || d.getYear() == 3000");
shouldBe("d.getFullYear()", "3000");
shouldBe("d.getDay()", "3");
shouldBe("d.getHours()", "0");
shouldBe("new Date('3/31/2099').getFullYear()", "2099");
shouldBe("new Date('3/31/2099').getMonth()", "2");

d = new Date(Date.UTC(3000, 0, 1));
shouldBe("d.valueOf()", "32503680000000");
shouldBe("d.getUTCDay()", "3");
shouldBe("d.getUTCHours()", "0");

d = new Date(1899, 0, 1);
shouldBe("d.getFullYear()", "1899");
shouldBeTrue("d.getYear() == -1 || d.getYear() == 1899"); // Moz or IE
shouldBe("d.getDay()", "0");
shouldBe("d.getHours()", "0");

d = new Date(Date.UTC(3000, 0, 1));
shouldBe("d.getUTCDay()", "3");

d = new Date(Date.UTC(1899, 0, 1));
shouldBe("d.getUTCDay()", "0");
shouldBe( "d.toUTCString().replace('GMT', '+0000')", "'Sun, 01 Jan 1899 00:00:00 +0000'");

// out of range values. have to be caught by TimeClip()
shouldBe("Number(new Date(8.64E15))", "8.64E15");
shouldBe("Number(new Date(-8.64E15))", "-8.64E15");
shouldBeTrue("isNaN(new Date(8.6400001E15))");;
shouldBeTrue("isNaN(new Date(-8.6400001E15))");;

// other browsers don't mind the missing space
shouldBe("(new Date('January29,2005')).getDate()", "29");

shouldBeTrue("(new Date('12/25/1995 ::')).valueOf() == (new Date('12/25/1995')).valueOf()");

// Tolerance for high values in xx/xx/xxxx
shouldBe("new Date('03/30/2006').getDate()", "30");
shouldBe("new Date('Mar 30 2006').toString()", "new Date('03/30/2006').toString()");
shouldBe("new Date('30/03/2006').toString()", "new Date('Jun 03 2008').toString()");
shouldBe("new Date('24/55/2006').getFullYear()", "2008");
shouldBe("new Date('70/55/2006').getDate()", "27");
shouldBe("new Date('00/00/2006').toString()", "new Date('Nov 30 2005').toString()");
shouldBe("new Date('01/452/2006').toString()", "new Date('Mar 28 2007').toString()");

debug("End Of Test");
