description(
"This test ensures that repeated use of the vm reentry cache does not eventually consume the entire register file."
);
var anArray = [1,2,3,4,5];
shouldBe("for(var i = 0; i < 50000; i++) anArray.sort(function(){ return 1; })", "[1,2,3,4,5]");

successfullyParsed = true;
