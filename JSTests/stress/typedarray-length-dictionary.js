// This tests that we do not inline cache intrinsic getters when the base structure is a dictionary.

foo = new Int32Array(10);

function len() {
    return foo.length;
}
noInline(len);

foo.bar = 1;
foo.baz = 2;
delete foo.bar;

for (i = 0; i < 1000; i++)
    len()

Object.defineProperty(foo, "length", { value: 1 });
if (len() !== 1)
    throw "bad result";
