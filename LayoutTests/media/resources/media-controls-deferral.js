// The first media controls built in a document are built at once, so a test of deferral builds them on a throwaway element first.
function buildFirstMediaControls(targetDocument = document) {
    const audio = targetDocument.createElement("audio");
    audio.controls = true;
    (targetDocument.body || targetDocument.documentElement).appendChild(audio);
    const built = !!internals.ensureUserAgentShadowRoot(audio).querySelector(".media-controls");
    audio.remove();
    if (!built)
        throw new Error("The first media controls in the document were not built at once");
}
