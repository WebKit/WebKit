function test() {

function f(key) {
  return {
    get: function() { result += key; return true; },
    set: Object,
    enumerable: true
  };
};
var result = '';
var obj = Object.defineProperties({}, {
  2:    f(2),
  0:    f(0),
  1:    f(1),
  ' ':  f(' '),
  9:    f(9),
  D:    f('D'),
  B:    f('B'),
  '-1': f('-1'),
});
Object.defineProperty(obj,'A',f('A'));
Object.defineProperty(obj,'3',f('3'));
Object.defineProperty(obj,'C',f('C'));
Object.defineProperty(obj,'4',f('4'));
delete obj[2];
obj[2] = true;

Object.assign({}, obj);

return result === "012349 DB-1AC";
      
}

if (!test())
    throw new Error("Test failed");

