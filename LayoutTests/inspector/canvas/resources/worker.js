globalThis.contexts = [];

function createContext(contextType) {
    let canvas = new OffscreenCanvas(10, 10);
    let context = canvas.getContext(contextType);
    globalThis.contexts.push(context);
}

function destroyContexts() {
    globalThis.contexts = [];
}

addEventListener("message", (event) => {
    let {name, args} = event.data;
    globalThis[name](...args);
});
