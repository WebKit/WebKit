async function getLastModified(header_value) {
    const iframe = document.createElement("iframe");
    document.body.appendChild(iframe);
    try { 
        const load_promise = new Promise((resolve, reject) => {
            iframe.addEventListener('load', resolve);
        });
        iframe.src = "/common/blank.html?pipe=header(Last-Modified," + encodeURIComponent(header_value).replace("%2C", "\\,") + ")";
        await load_promise;
        return iframe.contentDocument.lastModified;
    } finally {
        iframe.remove();
    }
}
