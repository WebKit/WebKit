let o = { foo: 1, bar: 2, baz: 3 };
for (let i=0; i<100; ++i) {
    o.a = i
    o.b = i
    delete o.a
    delete o.b
}
if ($vm.inlineCapacity(o) <= 3)
    throw new Error("There should be inline capacity");

delete o.foo;
$vm.flattenDictionaryObject(o);
o.foo = 1;
