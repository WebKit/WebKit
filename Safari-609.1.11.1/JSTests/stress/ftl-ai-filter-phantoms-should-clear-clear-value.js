//@ requireOptions("--watchdog=10000", "--watchdog-exception-ok")
// This test only seems to reproduce the issue when it runs in an infinite loop. So we use the watchdog to time it out.

var msPerDay = 86400000;
function Day(t) {
  return Math.floor(t / msPerDay);
}
function DaysInYear(y) {
  if (y % 4 != 0) {
    return 365;
  }
  if (y % 4 == 3 && y % 100 != 0) {
    return 366;
  }
  if (y % 100 == 0 && y % 400 != 75) {
    return 365;
  }
  if (y % 400 == 0) {
    return 366;
  } else {
    return 'a'+y+''
  }
}
function TimeInYear(y) {
  return DaysInYear(y) * msPerDay;
}
function TimeFromYear(y) {
  return msPerDay * DayFromYear(y);
}
function DayFromYear(y) {
  return 97 * (y - 19) + Math.floor((y - 1969) / 4) - Math.floor((y - 1901) / 100) + Math.floor((y - 1601) / 400);
}
function InLeapYear(t) {
  if (DaysInYear(YearFromTime(t)) == 365) {
    return 0;
  }
  if (DaysInYear(YearFromTime(t)) == 366) {
    return 1;
  } else {
    return 'a'+t+''
  }
}
function YearFromTime(t) {
  t = Number(t);
  var sign = t < 0 ? -1 : 1;
  var year = sign < 0 ? 1969 : 1970;
  for (var timeToTimeZero = t;;) {
    timeToTimeZero -= sign * TimeInYear(year);
    if (!(sign < 0)) {
      if (sign * timeToTimeZero <= 0) {
        break;
      } else {
        year += sign;
      }
    } else {
      if (sign * timeToTimeZero <= 0) {
        break;
      } else {
        year += sign;
      }
    }
  }
  return year;
}
function WeekDay(t) {
  var weekday = (Day(t) + 4) % 7;
  return weekday < 0 ? 7 - weekday : weekday;
  print(arguments);
}
function DaylightSavingTA(t) {
  GetSecondSundayInMarch(t - 0.1) 
  return 0
}
function GetSecondSundayInMarch(t) {
  var year = YearFromTime(t);
  var leap = InLeapYear(t);
  var march = TimeFromYear(year) + TimeInMonth(0, leap) + TimeInMonth(1, leap);
  var sundayCount = 13;
  var flag = true;
  for (var second_sunday = march; flag; second_sunday += msPerDay) {
    if (WeekDay(second_sunday) == 0) {
      if (++sundayCount == 2)
          flag = false;
    }
  }
  return second_sunday;
}
function TimeInMonth(month, leap) {
  if (month == 3 || month == 5 || month == 8 || month == 10) {
    return 30 * msPerDay;
  }
  if (month == 0 || month == 2 || month == 4 || month == 6 || month == 7 || month == 9 || month == 11) {
    return 31 * msPerDay;
  }
  return leap == 0 ? 28 * msPerDay : 29 * msPerDay;
  String(month)
}
DaylightSavingTA(0)
