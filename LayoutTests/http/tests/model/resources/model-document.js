
const model_document_test = (callback, description) => {
    return promise_test(async test => {
        const frame = document.createElement("iframe");
        frame.src = "UnitBox.usdz";
        document.body.appendChild(frame);

        test.add_cleanup(() => frame.remove());

        await new Promise(resolve => frame.addEventListener("load", resolve, { once: true }));

        await callback(frame.contentDocument);
    }, description);
};
