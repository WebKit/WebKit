var SubPixelLayout = (function() {
    var _subPixelLayout = null;
    document.addEventListener('DOMContentLoaded', function() {
        var elem = document.createElement('div');
        elem.style.setProperty('width', '4.5px');
        document.body.appendChild(elem);
        var bounds = elem.getBoundingClientRect();
        _subPixelLayout =  (bounds.width != Math.floor(bounds.width));
        document.body.removeChild(elem);
    });
    return {
        roundLineLeft: function(f) {
            if (!_subPixelLayout)
                return Math.floor(f);
            return Math.floor((Math.floor(f * 64) + 32) / 64); // see FractionLayoutUnit::round()
        },
        roundLineRight: function(f) {
            if (!_subPixelLayout)
                return Math.floor(f);
            return Math.floor(Math.floor(f * 64) / 64); // see FractionLayoutUnit::floor()
        },
        isSubPixelLayoutEnabled: function() {
            return _subPixelLayout;
        }
    }
}());