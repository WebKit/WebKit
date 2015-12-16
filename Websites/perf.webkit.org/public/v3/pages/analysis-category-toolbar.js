
class AnalysisCategoryToolbar extends Toolbar {
    constructor()
    {
        super();
        this._categories = AnalysisTask.categories();
        this._currentCategory = null;
        this._filter = null;
        this._filterCallback = null;
        this.setCategoryIfValid(null);
    }

    currentCategory() { return this._currentCategory; }

    filter() { return this._filter; }
    setFilterCallback(callback)
    {
        console.assert(!callback || callback instanceof Function);
        this._filterCallback = callback;
    }

    render()
    {
        var router = this.router();
        console.assert(router);

        var currentPage = router.currentPage();
        console.assert(currentPage instanceof AnalysisCategoryPage);

        super.render();

        var element = ComponentBase.createElement;
        var link = ComponentBase.createLink;

        var input = element('input',
            {
                oninput: this._filterMayHaveChanged.bind(this),
                onchange: this._filterMayHaveChanged.bind(this),
            });
        if (this._filter != null)
            input.value = this._filter;

        var currentCategory = this._currentCategory;
        this.renderReplace(this.content().querySelector('.analysis-task-category-toolbar'), [
            element('ul', {class: 'buttoned-toolbar'},
                this._categories.map(function (category) {
                    return element('li',
                        {class: category == currentCategory ? 'selected' : null},
                        link(category, router.url(currentPage.routeName(), {category: category})));
                })),
            input]);
    }

    _filterMayHaveChanged(event)
    {
        var input = event.target;
        var oldFilter = this._filter;
        this._filter = input.value;
        if (this._filter != oldFilter && this._filterCallback)
            this._filterCallback(this._filter);
    }

    setCategoryIfValid(category)
    {
        if (!category)
            category = this._categories[0];
        if (this._categories.indexOf(category) < 0)
            return false;
        this._currentCategory = category;

        var filterDidChange = !!this._filter;
        this._filter = null;
        if (filterDidChange && this._filterCallback)
            this._filterCallback(this._filter);

        return true;
    }

    static htmlTemplate()
    {
        return `<div class="buttoned-toolbar analysis-task-category-toolbar"></div>`;
    }
}
