(function() {
    if (window.__ceacBeforeUnloadFix) return;
    window.__ceacBeforeUnloadFix = true;
    var origAEL = window.addEventListener;
    window.addEventListener = function(type, fn, opts) {
        if (type === 'beforeunload') {
            return origAEL.call(this, type, function(e) {
                var ae = document.activeElement;
                if (ae && ae.tagName === 'INPUT') {
                    var t = (ae.type || '').toLowerCase();
                    if (t === 'radio' || t === 'checkbox' || t === 'submit' || t === 'button')
                        return;
                }
                if (typeof fn === 'function') fn.call(this, e);
            }, opts);
        }
        return origAEL.apply(this, arguments);
    };
})();
