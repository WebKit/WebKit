description("Tests for ES6 arrow function syntax errors");

shouldThrow('=>{}');

function checkStatement(statement) {
  var unexpectedSymbols = [ "",
      "*", "/", "%" ,  "+", "-" ,
       "<<", ">>", ">>>" ,
       "<", ">", "<=", ">=", "instanceof", "in" ,
       "==", "!=", "===", "!==",
       "&" ,  "^" ,  "|" ,
       "&&" ,  "||", ";" , ","
  ];

  for (var i = 0; i < unexpectedSymbols.length; i++) {
    shouldThrow(statement + unexpectedSymbols[i]);
  }
}

checkStatement('x=>');
checkStatement('x=>{');
shouldThrow('x=>}');

checkStatement('var y = x=>');
checkStatement('var y = x=>{');
shouldThrow('var y = x=>}');


shouldThrow('var t = x=>x+1; =>{}');
shouldThrow('[=>x+1]');
shouldThrow('[x=>x+1, =>x+1]');
shouldThrow('var f=>x+1;');
shouldThrow('var x, y=>y+1;');
shouldThrow('debug(=>x+1)');
shouldThrow('debug("xyz", =>x+1)');

shouldThrow("var af1=y\n=>y+1");
shouldThrow("var af2=(y)\n=>y+1");
shouldThrow("var af3=(x, y)\n=>y+1");

var successfullyParsed = true;
