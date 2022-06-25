function test() {

var s1 = Symbol(), s2 = Symbol(), s3 = Symbol();
var proto = {};
proto[s1] = true;
var obj = Object.create(proto);
obj[s2] = true;
Object.defineProperty(obj, s3, { value: true, enumerable: false });

var keys = Reflect.ownKeys(obj);
return keys.indexOf(s2) >-1 && keys.indexOf(s3) >-1 && keys.length === 2;
      
}

if (!test())
    throw new Error("Test failed");

