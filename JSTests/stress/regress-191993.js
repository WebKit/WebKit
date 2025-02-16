function foo(o) {
    return o.r.input;
}
noInline(foo);

Object.assign({}, RegExp);

for (let i = 0; i < testLoopCount; i++)
    foo({r: RegExp});

let input = foo({r: RegExp});
if (typeof input !== "string")
    throw "FAILED";
