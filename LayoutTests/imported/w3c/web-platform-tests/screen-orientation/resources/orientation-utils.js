/**
 *
 * @param {object} options
 * @param {string} options.src - The iframe src
 * @param {Window} options.context - The browsing context in which the iframe will be created
 * @param {string} options.sandbox - The sandbox attribute for the iframe
 * @returns
 */
export async function attachIframe(options = {}) {
  const { src, context, sandbox, allowFullscreen } = {
    ...{
      src: "about:blank",
      context: self,
      allowFullscreen: true,
      sandbox: null,
    },
    ...options,
  };
  const iframe = context.document.createElement("iframe");
  iframe.allowFullscreen = allowFullscreen;
  await new Promise((resolve) => {
    if (sandbox != null) iframe.sandbox = sandbox;
    iframe.onload = resolve;
    iframe.src = src;
    context.document.body.appendChild(iframe);
  });
  return iframe;
}

export function getOppositeOrientation() {
  return screen.orientation.type.startsWith("portrait")
    ? "landscape"
    : "portrait";
}

/**
 * Exits full screen, and removes the iframe.
 * @param {HTMLIFrameElement} iframe same-origin iframe.
 * @returns {() => Promise<void>} The cleanup function.
 */
export function makeCleanup(iframe = null) {
  const context = iframe?.contentWindow ?? window;
  const doc = iframe?.contentDocument ?? context.document;
  return async () => {
    await doc.fullscreenElement?.ownerDocument.exitFullscreen();
    iframe?.remove();
  };
}
