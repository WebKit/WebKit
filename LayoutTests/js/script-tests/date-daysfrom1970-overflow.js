description('This test confirms an assertion in dateToDaysFrom1970() in wtf/DateMath.cpp passes. The function had a bug setting a number larger than INT_MAX to an int variable.');

var d = new Date(20000000, 0, 1);
shouldBe('d.getTime()', 'NaN');

shouldBe('Date.UTC(1970, 0, 1, 0, 0, 0, 0)', '0');

d = new Date(-20000000, 0, 1);
shouldBe('d.getTime()', 'NaN');
