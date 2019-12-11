// Create an object with inline capacity 1.
let obj = { foo: 1 };

// Make it into a dictionary.
delete obj.foo;

// Get us to allocate out of line capacity.
obj.foo = 1;
obj.bar = 2;

// Delete the inline property.
delete obj.foo;

let o = Object.create(obj);

function foo() {
    return o.toString();
}
noInline(foo);

// Flatten into an empty butterfly.
for (let i = 0; i < 10000; i++)
    foo();
