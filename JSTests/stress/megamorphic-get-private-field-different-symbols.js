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
function makeClass() {
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
    return C;
}
noInline(makeClass);

let classes = [];
let objects = [];
for (let i = 0; i < 16; ++i) {
    let C = makeClass();
    classes.push(C);
    objects.push(new C(i));
}

for (let i = 0; i < testLoopCount; ++i) {
    let index = i & 15;
    shouldBe(classes[index].prototype.getField.call(objects[index]), index);
    if (!(i & 0xff)) {
        // Another copy's getter looks for a different private symbol, so this must throw
        // even though the receiver has a private field at the same offset.
        let other = (index + 1) & 15;
        shouldThrowTypeError(() => classes[other].prototype.getField.call(objects[index]));
    }
}
