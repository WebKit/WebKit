
const createModelAndSource = (test, src) => {
    const model = document.createElement("model");
    document.body.appendChild(model);

    const source = document.createElement("source");
    if (src)
        source.src = src;
    model.appendChild(source);

    test.add_cleanup(() => model.remove());

    return [model, source];
};
