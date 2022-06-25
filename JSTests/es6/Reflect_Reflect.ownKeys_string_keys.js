function test() {

var obj = Object.create({ C: true });
obj.A = true;
Object.defineProperty(obj, 'B', { value: true, enumerable: false });

return Reflect.ownKeys(obj).sort() + '' === "A,B";
      
}

if (!test())
    throw new Error("Test failed");

