description("Test for correct handling of exceptions from instanceof and 'new' expressions");

shouldThrow("new {}.undefined");
shouldThrow("1 instanceof {}.undefined");

successfullyParsed = true;
