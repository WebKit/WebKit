var ui = ui || {};
ui.results = ui.results || {};

(function(){

// FIXME: Rather than using table, should we be using something fancier?
ui.results.Comparison = base.extends('table', {
    init: function()
    {
        this.className = 'comparison';
        this.innerHTML = '<thead><tr><th>Expected</th><th>Actual</th><th>Diff</th></tr></thead>' +
                         '<tbody><tr><td class="expected"></td><td class="actual"></td><td class="diff"></td></tr></tbody>';
    },
    _selectorForKind: function(kind)
    {
        switch (kind) {
        case results.kExpectedKind:
            return '.expected';
        case results.kActualKind:
            return '.actual';
        case results.kDiffKind:
            return '.diff';
        }
        return '.unknown';
    },
    update: function(kind, result)
    {
        var selector = this._selectorForKind(kind);
        $(selector, this).empty().append(result);
        return result;
    },
});

// We'd really like TextResult and ImageResult to extend a common Result base
// class, but we can't seem to do that because they inherit from different
// HTMLElements. We could have them inherit from <div>, but that seems lame.

ui.results.TextResult = base.extends('iframe', {
    init: function(url)
    {
        this.className = 'text-result';
        this.src = url;
    }
});

ui.results.ImageResult = base.extends('img', {
    init: function(url)
    {
        this.className = 'image-result';
        this.src = url;
    }
});

function constructorForResultType(type)
{
    return (type == results.kImageType) ? ui.results.ImageResult : ui.results.TextResult;
}

ui.results.ResultsGrid = base.extends('div', {
    init: function()
    {
        this.className = 'results-grid';
    },
    _addResult: function(comparison, constructor, resultsURLsByKind, kind)
    {
        var url = resultsURLsByKind[kind];
        if (!url)
            return;
        comparison.update(kind, new constructor(url));
    },
    addComparison: function(resultType, resultsURLsByKind)
    {
        var comparison = new ui.results.Comparison();
        var constructor = constructorForResultType(resultType);

        this._addResult(comparison, constructor, resultsURLsByKind, results.kExpectedKind);
        this._addResult(comparison, constructor, resultsURLsByKind, results.kActualKind);
        this._addResult(comparison, constructor, resultsURLsByKind, results.kDiffKind);

        this.appendChild(comparison);
        return comparison;
    },
    addRow: function(resultType, url)
    {
        var constructor = constructorForResultType(resultType);
        var view = new constructor(url);
        this.appendChild(view);
        return view;
    },
    addResults: function(resultsURLs)
    {
        var resultsURLsByTypeAndKind = {};

        resultsURLsByTypeAndKind[results.kImageType] = {};
        resultsURLsByTypeAndKind[results.kTextType] = {};

        resultsURLs.forEach(function(url) {
            resultsURLsByTypeAndKind[results.resultType(url)][results.resultKind(url)] = url;
        });

        $.each(resultsURLsByTypeAndKind, function(resultType, resultsURLsByKind) {
            if ($.isEmptyObject(resultsURLsByKind))
                return;
            if (results.kUnknownKind in resultsURLsByKind) {
                // This is something like "crash" that isn't a comparison.
                this.addRow(resultType, resultsURLsByKind[results.kUnknownKind]);
                return;
            }
            this.addComparison(resultType, resultsURLsByKind);
        }.bind(this));
    }
});

})();
