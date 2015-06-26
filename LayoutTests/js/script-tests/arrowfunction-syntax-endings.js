description("Tests for ES6 arrow function endings");

var afEOL = x=>x+1
result = afEOL(12);

shouldBe('afEOL(12)', '13');

shouldNotThrow('x=>x+1');

var afEOLTxt = 'x=>x+1' + String.fromCharCode(10);
shouldNotThrow(afEOLTxt);

var f = function () {
  var result = 0;
  var afEOF;


  afEOF = x => x*10000 + x*1000 - x*10000 - x*1000 + x



  result = afEOF(12);


  result = result + afEOF(13);


  result = result + afEOF(14);

  return result;
};

shouldBe('f()', '39');

var successfullyParsed = true;
