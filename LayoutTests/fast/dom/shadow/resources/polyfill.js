if (!Element.prototype.webkitCreateShadowRoot && window.internals) {
    Element.prototype.webkitCreateShadowRoot = function() {
        return window.internals.createShadowRoot(this);
    };
}
