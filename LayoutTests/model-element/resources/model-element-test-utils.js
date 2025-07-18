
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

const createModelWithAttributesAndSource= (test, attributes, src) => {
    [model, source] = createModelAndSource(test, src);

    for (const attrKey in attributes) {
        if (attributes.hasOwnProperty(attrKey)) {
            model.setAttribute(attrKey, attributes[attrKey]);
        }
    }

    return [model, source];
};

const makeSource = (src, type) => {
    const source = document.createElement("source");
    source.src = src;
    if (type)
        source.type = type;
    return source;
}

async function waitForModelState(element, expectedState, timeout = 5000) {
    return new Promise((resolve, reject) => {
        const startTime = Date.now();

        function checkState() {
            try {
                const currentState = window.internals.modelElementState(element);
                if (currentState === expectedState) {
                    resolve(currentState);
                    return;
                }

                if (Date.now() - startTime > timeout) {
                    reject(new Error(`Timeout waiting for state ${expectedState}, current state: ${currentState}`));
                    return;
                }

                setTimeout(checkState, 100);
            } catch (error) {
                reject(error);
            }
        }

        checkState();
    });
}

function scrollElementIntoView(element) {
    const rect = element.getBoundingClientRect();
    const elementTop = rect.top + window.pageYOffset;
    const elementCenter = elementTop + (rect.height / 2);
    const viewportCenter = window.innerHeight / 2;

    window.scrollTo(0, elementCenter - viewportCenter);
}
