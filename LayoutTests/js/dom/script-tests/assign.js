description(
"This test checks whether various forms of assignment expression are allowed."
);

var x = 0;
var y = 0;

shouldBe('x = 1; x', '1'); 

shouldBe('window.x = 2; x', '2'); 
shouldBe('window["x"] = 3; x', '3'); 
shouldBe('(x) = 4; x', "4"); 
shouldBe('(window.x) = 5; x', '5'); 
shouldBe('(window["x"]) = 6; x', '6'); 
shouldBe('y, x = 7; x', '7'); 
shouldBe('((x)) = 8; x', '8'); 
shouldBe('((window.x)) = 9; x', '9'); 
shouldBe('((window["x"])) = 10; x', '10'); 

shouldThrow('(y, x) = "FAIL";'); 
shouldThrow('(true ? x : y) = "FAIL";'); 
shouldThrow('x++ = "FAIL";');
