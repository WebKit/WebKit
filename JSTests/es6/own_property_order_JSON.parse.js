function test() {

var result = '';
JSON.parse(
  '{"0":true,"1":true,"2":true,"3":true,"4":true,"9":true," ":true,"D":true,"B":true,"-1":true,"A":true,"C":true}',
  function reviver(k,v) {
    result += k;
    return v;
  }
);
return result === "012349 DB-1AC";
      
}

if (!test())
    throw new Error("Test failed");

