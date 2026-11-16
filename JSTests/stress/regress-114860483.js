function assertArgumentsContent() {
    const str = [...arguments].join();
    if (str !== `,1.1,1.1,1.1,1.1,1.1`)
        throw new Error("Bad assertion!");
}

function createClonedArguments() {
    return arguments.callee.arguments;
}

function main() {
    gc();

    const global_proxy = this;
    Reflect.defineProperty(global_proxy, 0, {
        get() {
            for (let i = 100; i < 200; i++)
                cloned_arguments[i] = 1.1;

            for (let i = 0; i < 100; i++)
                cloned_arguments[i] = 1.1;

            gc();

            // Creating invalid date objects.
            for (let i = 0; i < 100; i++) {
                new Date('a');
            }
        }
    });

    const cloned_arguments = createClonedArguments(null, new Date(), new Date(), new Date(), new Date(), new Date());
    delete cloned_arguments[0];

    Reflect.setPrototypeOf(cloned_arguments, global_proxy);

    assertArgumentsContent.apply(null, cloned_arguments);
}

main();
