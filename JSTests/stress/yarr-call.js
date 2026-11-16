var pattern = /([a-z])\1*|'([^']|'')+'|''|./gi;
var string = "y년 MMM d일";
for (var i = 0; i < 1e3; ++i)
    string.replace(pattern, "hello");
