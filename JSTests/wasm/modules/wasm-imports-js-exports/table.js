export const table = new WebAssembly.Table({
    element: "externref",
    initial: 10,
});

table.set(0, "foo");
