"use strict";

let objs = [
    function() { },
    new String(),
    {foo: 45},
    {bar:50, foo: 45},
    {baz:70, bar:50, foo: 45},
    new Date,
];

let has = ({}).hasOwnProperty;
function foo(o) {
    return has.call(o, "foo");
}
noInline(foo);

for (let i = 0; i < 10000; i++)
    foo(objs[i % objs.length]);

foo("foo");
