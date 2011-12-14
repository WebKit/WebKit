description('Test to ensure RegExp generates single character matches in the correct order');

shouldBe("/[\\w']+/.exec(\"'_'\").toString()", "\"'_'\"");

var successfullyParsed = true;
