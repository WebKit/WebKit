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

eval('var af = x=>x*2');
debug("eval('var af = x=>x*2')");
shouldBe('af(10)','20');

eval('var af1 = x=>x*3, af2=x=>x*4');
debug("eval('var af1 = x=>x*3, af2=x=>x*4')");
shouldBe('af1(10)','30');
shouldBe('af2(10)','40');

eval('var af3 = x=>x*3;');
debug("eval('var af1 = x=>x*3;')");
shouldBe('af3(10)','30');

eval('var af4 = x=>(x*3)');
debug("eval('var af4 = x=>(x*3)')");
shouldBe('af4(10)','30');

eval('var af5 = x => { return x*3; }');
debug("eval('var af5 = x=> { return x*3; }')");
shouldBe('af5(10)','30');

var successfullyParsed = true;
