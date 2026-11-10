// Loads an image as the main resource of an iframe. The resulting ImageDocument has no text decoder,
// so Document::charset() / Document::encoding() are null.
function loadImageDocumentFrame()
{
    return new Promise((resolve) => {
        const iframe = document.createElement("iframe");
        iframe.onload = () => resolve(iframe);
        iframe.src = "../resources/abe.png";
        document.body.appendChild(iframe);
    });
}
