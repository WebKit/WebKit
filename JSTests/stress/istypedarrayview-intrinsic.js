let typedArrays = [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array];

function makeFn(dontInline) {
    let foo = createBuiltin(`(function (a) { "use strict"; return @isTypedArrayView(a); })`);
    if (dontInline)
        noInline(foo)
    return foo;
}

typedArrays.forEach(function() {
    let test = Function(
        `
        let foo = makeFn();
        let bar = makeFn(true);
        let view = new Int8Array(10);

        for (i = 0; i < 100000; i++) {
            if (!foo(view))
                throw new Error(i);
            if (!bar(view))
                throw new Error(i);
        }
        `
    );
    test();
});

typedArrays.forEach(constructor1 => {
    typedArrays.forEach(constructor2 => {
        let test = Function(
            `
            let foo = makeFn();
            let bar = makeFn(true);
            let view1 = new ${constructor1.name}(10);
            let view2 = new ${constructor2.name}(10);

            for (i = 0; i < 100000; i++) {
                let view = i % 2 === 0 ? view1 : view2;
                if (!foo(view))
                    throw new Error(i);
                if (!bar(view))
                    throw new Error(i);
            }
            `
        );
        test();
    });
});

let test = function() {
    let foo = makeFn();
    let bar = makeFn(true);
    for (i = 0; i < 100000; i++) {
        if (foo(true))
            throw new Error(i);
        if (bar(true))
            throw new Error(i);
    }
}
test();

test = function() {
    let bar = makeFn(true);
    let view = new Int8Array(10);
    let obj = new DataView(new ArrayBuffer(10));
    for (i = 0; i < 100000; i++) {
        if (i % 2 === 0) {
            if (!foo(view))
                throw new Error(i);
        } else {
            if (foo(obj))
                throw new Error(i);
        }
    }
}
