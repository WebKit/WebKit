/*---
defines: [ISOFields, assertSameISOFields]
allow_unused: True
---*/

function ISOFields(monthDay) {
  let re = /^(?<year>-?\d{4,6})-(?<month>\d{2})-(?<day>\d{2})\[u-ca=(?<calendar>[\w\-]+)\]$/;

  let str = monthDay.toString({calendarName: "always"});
  let match = str.match(re);
  assertEq(match !== null, true, `can't match: ${str}`);

  let {year, month, day, calendar} = match.groups;
  let isoYear = Number(year);
  let isoMonth = Number(month);
  let isoDay = Number(day);

  let date = Temporal.PlainDate.from(str);
  let isoDate = date.withCalendar("iso8601");

  assertEq(calendar, date.calendarId);
  assertEq(isoYear, isoDate.year);
  assertEq(isoMonth, isoDate.month);
  assertEq(isoDay, isoDate.day);

  return {
    isoYear,
    isoMonth,
    isoDay,
    calendar,
  };
}

function assertSameISOFields(actual, expected) {
  let actualFields = ISOFields(actual);
  let expectedFields = ISOFields(expected);

  assertEq(typeof actualFields.isoYear, "number");
  assertEq(typeof actualFields.isoMonth, "number");
  assertEq(typeof actualFields.isoDay, "number");

  assertEq(actualFields.isoMonth > 0, true);
  assertEq(actualFields.isoDay > 0, true);

  assertEq(actualFields.isoYear, expectedFields.isoYear);
  assertEq(actualFields.isoMonth, expectedFields.isoMonth);
  assertEq(actualFields.isoDay, expectedFields.isoDay);
  assertEq(actualFields.calendar, expectedFields.calendar);
}
