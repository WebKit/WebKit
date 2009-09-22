description(
"This test checks that array accessing doesn't do the wrong thing for negative indices"
);
var a = [];
a[-5] =  true;
shouldBe('a.length', '0');
shouldBe('a["-5"]', 'a[-5]');

// Just some bounds paranoia
a = [1,2,3,4];
shouldBe('a[4]', 'undefined');

a = [];
for (var i = 0; i > -1000; i--) a[i] = i;

var successfullyParsed = true;
