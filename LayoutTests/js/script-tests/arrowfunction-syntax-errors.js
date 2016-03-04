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

shouldThrow('([a, b] => a + b)(["a_", "b_"])' );
shouldThrow('({a, b} => a + b)({a:"a_", b:"b_"})');
shouldThrow('({c:a,d:b} => a + b)({c:"a_", d:"b_"})');
shouldThrow('({c:b,d:a} => a + b)({c:"a_", d:"b_"})');

shouldThrow('var arr1 = [a, b] => a + b;');
shouldThrow('var arr2 = {a, b} => a + b;');
shouldThrow('var arr3 = {c:a,d:b} => a + b;');
shouldThrow('var arr3 = {c:b,d:a} => a + b;');

shouldThrow('var arr4 = () => { super(); };', '"SyntaxError: Cannot call super() outside of a class constructor."');
shouldThrow('var arr4 = () => { super; };', '"SyntaxError: Cannot reference super."');
shouldThrow('var arr5 = () => { super.getValue(); };', '"SyntaxError: super can only be used in a method of a derived class."');

shouldThrow('var arr6 = () =>  super();', '"SyntaxError: Cannot call super() outside of a class constructor."');
shouldThrow('var arr7 = () =>  super;', '"SyntaxError: Cannot reference super."');
shouldThrow('var arr8 = () =>  super.getValue();', '"SyntaxError: super can only be used in a method of a derived class."');

shouldThrow('class A { constructor() { function a () { return () => { super(); };}}', '"SyntaxError: Cannot call super() outside of a class constructor."');
shouldThrow('class B { constructor() { function b () { return () => { super; }; }; }}', '"SyntaxError: Cannot reference super."');
shouldThrow('class C { constructor() { function c () { return () => { super.getValue(); };}}', '"SyntaxError: super can only be used in a method of a derived class."');

shouldThrow('class D { constructor() { function a () { return () => super(); }}', '"SyntaxError: Cannot call super() outside of a class constructor."');
shouldThrow('class E { constructor() { function b () { return () => super; }; }}', '"SyntaxError: Cannot reference super."');
shouldThrow('class F { constructor() { function c () { return () => super.getValue(); }}', '"SyntaxError: super can only be used in a method of a derived class."');
shouldThrow('class G {}; class G2 extends G { getValue() { function c () { return () => super.getValue(); }}', '"SyntaxError: super can only be used in a method of a derived class."');
shouldThrow('class H {}; class H2 extends H { method() { function *gen() { let arr = () => super.getValue(); arr(); } } }', '"SyntaxError: super can only be used in a method of a derived class."');

var successfullyParsed = true;
