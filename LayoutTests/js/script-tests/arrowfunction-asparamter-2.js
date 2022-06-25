description('Tests for ES6 arrow function, passing arrow function as the paramter');

var f = function (cl, paramter) { return cl(paramter); };
var f2 = function (cl, paramter1, paramter2) { return cl(paramter1, paramter2); };

shouldBe('f(x=>{return x * 25;}, 121)', '25*121' );
shouldBe('f2((x, y)=>{return x * y;}, 14, 12)', '14*12');

var successfullyParsed = true;
