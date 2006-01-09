description(
"This test checks for correct handling ofr backslash-newline in JS strings."
);

mystring = 'hello\
there';  
shouldBe('mystring', '"hellothere"');

var successfullyParsed = true;
