function shouldThrow(fn, error, message, stackFunctions) {
    try {
        fn();
        throw new Error('Expected to throw, but succeeded');
    } catch (e) {
        if (!(e instanceof error))
            throw new Error(`Expected to throw ${error.name} but got ${e.name}`);
        if (e.message !== message)
            throw new Error(`Expected ${error.name} with '${message}' but got '${e.message}'`);

        const stackLines = e.stack.split('\n').filter(line => line.trim());
        if (stackLines.length !== stackFunctions.length) {
            throw new Error(
                `\nActual stack trace:\n${e.stack}\n`
            );
        }
        for (let i = 0; i < stackFunctions.length; i++) {
            const expectedFunction = stackFunctions[i];
            const stackLine = stackLines[i];
            let found = false;
            if (stackLine.startsWith(`${expectedFunction}@`))
                found = true;
            if (!found) {
                throw new Error(`Actual stack trace:\n${e.stack}`);
            }
        }
    }
}

{
    class SimpleClass {
        method() {
            this.innerMethod();
        }
        innerMethod() {
            throw new Error("error from class method");
        }
    }
    const instance = new SimpleClass();
    shouldThrow(() => instance.method(), Error, "error from class method", [
        "SimpleClass.innerMethod",
        "SimpleClass.method",
        "",
        "shouldThrow",
        "global code",
    ]);
}

{
    class BaseClass {
        method() {
            this.innerMethod();
        }
        innerMethod() {
            throw new Error("error from class method");
        }
    }
    class Derived extends BaseClass {}
    const instance = new Derived();
    shouldThrow(() => instance.method(), Error, "error from class method", [
        "Derived.innerMethod",
        "Derived.method",
        "",
        "shouldThrow",
        "global code",
    ]);
}

{
    const AnonymousClass = class {
        anonMethod() {
            throw new Error("error from anonymous class");
        }
    };
    const anon = new AnonymousClass();
    shouldThrow(() => anon.anonMethod(), Error, "error from anonymous class", [
        "AnonymousClass.anonMethod",
        "",
        "shouldThrow",
        "global code"
    ]);
}

{
    class ClassWithGetter {
        get prop() {
            throw new Error("error from getter");
        }
    }
    const withGetter = new ClassWithGetter();
    shouldThrow(() => withGetter.prop, Error, "error from getter", [
        "ClassWithGetter.prop",
        "",
        "shouldThrow",
        "global code"
    ]);
}

{
    class ClassWithSetter {
        set prop(value) {
            throw new Error("error from setter");
        }
    }
    const withSetter = new ClassWithSetter();
    shouldThrow(() => { withSetter.prop = 1; }, Error, "error from setter", [
        "ClassWithSetter.prop",
        "",
        "shouldThrow",
        "global code"
    ]);
}
