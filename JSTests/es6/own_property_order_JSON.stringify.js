function test() {

var obj = {
  2:    true,
  0:    true,
  1:    true,
  ' ':  true,
  9:    true,
  D:    true,
  B:    true,
  '-1': true,
};
obj.A = true;
obj[3] = true;
Object.defineProperty(obj, 'C', { value: true, enumerable: true });
Object.defineProperty(obj, '4', { value: true, enumerable: true });
delete obj[2];
obj[2] = true;

return JSON.stringify(obj) ===
  '{"0":true,"1":true,"2":true,"3":true,"4":true,"9":true," ":true,"D":true,"B":true,"-1":true,"A":true,"C":true}';
      
}

if (!test())
    throw new Error("Test failed");

