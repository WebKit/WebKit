// Check that an initial ^ will result in a faster match fail.
var s = "foo";
var i;

for (i = 0; i < 18; i++) {
  s = s + s;
}

var re = /^bar/;

var startDate = new Date();
for (i = 0; i < 10000; i++) {
  re.test(s);
}

testPassed("Congrats, your browser didn't hang!");

var successfullyParsed = true;
