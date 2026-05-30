promise_test(async t => {
    // Pre-populate allDocumentsMap so the matching Document is not the only entry.
    let preDocs = [];
    for (let i = 0; i < 8; i++)
        preDocs.push(document.implementation.createHTMLDocument());

    const swr = await navigator.serviceWorker.register('change-documents-in-progress-event-sw.js', { scope: './' });
    await navigator.serviceWorker.ready;

    const request = await swr.backgroundFetch.fetch('repro-' + Date.now(), ['change-documents-in-progress-event-sw.js']);

    let newDocs = [];
    let fired = false;

    let resolveCallback;
    const promise = new Promise(resolve => resolveCallback = resolve);
    request.onprogress = () => {
        if (fired)
            return;
        fired = true;
        for (let i = 0; i < 512; i++)
           newDocs.push(document.implementation.createHTMLDocument());
        resolveCallback();
    };

    return promise;
});
