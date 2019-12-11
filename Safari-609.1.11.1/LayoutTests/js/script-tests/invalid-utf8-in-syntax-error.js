description('Ensures that we correctly propagate the error message for lexer errors containing invalid utf8 code sequences');

shouldThrow('({f("\udead")})');

var successfullyParsed = true;

