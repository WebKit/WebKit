let string = $vm.make16BitStringIfPossible("test");

if (is8BitString(string))
    throw new Error("Should be 16 bit");


object = { };
object.foo = string;

let stringified = JSON.stringify(object);

if (!is8BitString(stringified))
    throw new Error("JSON.stringify should have produced an 8-bit string");
