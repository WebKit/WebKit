function test() {

var sym1 = Symbol(), sym2 = Symbol(), sym3 = Symbol();
var obj = {
  1:    true,
  A:    true,
};
obj.B = true;
obj[sym1] = true;
obj[2] = true;
obj[sym2] = true;
Object.defineProperty(obj, 'C', { value: true, enumerable: true });
Object.defineProperty(obj, sym3,{ value: true, enumerable: true });
Object.defineProperty(obj, 'D', { value: true, enumerable: true });

var result = Reflect.ownKeys(obj);
var l = result.length;
return result[l-3] === sym1 && result[l-2] === sym2 && result[l-1] === sym3;
      
}

if (!test())
    throw new Error("Test failed");

