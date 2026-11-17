import * as assert from "../assert.js";

function construct(descriptor) {
    try {
        new WebAssembly.Table(descriptor);
    } catch (error) {
        return error;
    }
    return undefined;
}

{
    const order = [];
    const error = construct({
        get element() {
            order.push("element");
            throw new TypeError("element");
        },
        get address() {
            order.push("address");
            return "i32";
        },
        get initial() {
            order.push("initial");
            return 1;
        },
    });
    assert.eq(error.message, "element");
    assert.eq(order.join(","), "element");
}

{
    const order = [];
    const error = construct({
        get element() {
            order.push("element");
            return "funcref";
        },
        get address() {
            order.push("address");
            throw new TypeError("address");
        },
        get initial() {
            order.push("initial");
            return 1;
        },
    });
    assert.eq(error.message, "address");
    assert.eq(order.join(","), "element,address");
}

{
    const order = [];
    const error = construct({
        get element() {
            order.push("element");
            return "funcref";
        },
        get address() {
            order.push("address");
            return "i32";
        },
        get initial() {
            order.push("initial");
            throw new TypeError("initial");
        },
    });
    assert.eq(error.message, "initial");
    assert.eq(order.join(","), "element,address,initial");
}

{
    const table = new WebAssembly.Table({ element: "externref", address: "i32", initial: 2 });
    assert.eq(table.length, 2);
}
