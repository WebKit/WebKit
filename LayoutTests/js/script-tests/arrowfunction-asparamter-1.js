description('Tests for ES6 arrow function, passing arrow function as the paramter');

shouldBe('"" + [1, 2, 3, 4].map(x => x, 32)', "'1,2,3,4'");

var successfullyParsed = true;
