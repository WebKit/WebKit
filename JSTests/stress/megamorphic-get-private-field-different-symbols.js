function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function shouldThrowTypeError(func) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof TypeError))
        throw new Error('expected TypeError but got: ' + error);
}

// Each evaluation of the class creates a fresh private symbol for #field, but every
// getField shares the same IC site. With 16 copies the site sees 16 (symbol, structure)
// pairs and goes megamorphic on the by-val path.
let objects = [];
for (let i = 0; i < 16; ++i) {
    class C {
        #field;

        constructor(value) {
            this.#field = value;
        }

        getField() {
            return this.#field;
        }
    }
    noInline(C.prototype.getField);
    objects.push(new C(i));
}

for (let i = 0; i < testLoopCount; ++i) {
    let index = i & 15;
    shouldBe(objects[index].getField(), index);
    if (!(i & 0xff)) {
        let other = (index + 1) & 15;
        shouldThrowTypeError(() => objects[other].getField.call(objects[index]));
    }
}
