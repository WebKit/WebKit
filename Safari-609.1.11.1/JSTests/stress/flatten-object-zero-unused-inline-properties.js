let o = { foo: 1, bar: 2, baz: 3 };
if ($vm.inlineCapacity(o) <= 3)
    throw new Error("There should be inline capacity");

delete o.foo;
$vm.flattenDictionaryObject(o);
o.foo = 1;
