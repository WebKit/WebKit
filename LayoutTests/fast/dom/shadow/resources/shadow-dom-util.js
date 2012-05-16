// ----------------------------------------------------------------------
// shadow-dom-util.js is a set of utility to test Shadow DOM.

function getElementByIdConsideringShadowDOM(root, id) {
    function iter(root, id) {
        if (!root)
            return null;

        if (root.id == id)
            return root;

        // We don't collect div having a shadow root, since we cannot point it correctly.
        // Such div should have an inner div to be pointed correctly.
        for (var child = root.firstChild; child; child = child.nextSibling) {
            var node = iter(child, id);
            if (node != null)
                return node;
        }

        if (root.nodeType != 1)
            return null;

        for (var shadowRoot = internals.youngestShadowRoot(root); shadowRoot; shadowRoot = internals.olderShadowRoot(shadowRoot)) {
            var node = iter(shadowRoot, id);
            if (node != null)
                return node;
        }

        return null;
    };

    if (!window.internals)
        return null;
    return iter(root, id);
}
