function sleep(ms) {
    return new Promise(resolve => { setTimeout(resolve, ms); });
}

function createIframe(t, src) {
    return new Promise(resolve => {
        const iframe = document.createElement("iframe");
        iframe.onload = () => { resolve(iframe); };
        iframe.src = src;

        t.add_cleanup(() => { iframe.remove(); });
        document.body.append(iframe);
    });
}
