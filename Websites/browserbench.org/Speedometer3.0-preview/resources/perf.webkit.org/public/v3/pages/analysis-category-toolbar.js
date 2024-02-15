
class AnalysisCategoryToolbar extends ComponentBase {
    constructor(categoryPage)
    {
        super('analysis-category-toolbar');
        this._categoryPage = null;
        this._categories = AnalysisTask.categories();
        this._currentCategory = null;
        this._filter = null;
        this.setCategoryIfValid(null);

        this._filterInput = this.content().querySelector('input');
        this._filterInput.oninput = this._filterMayHaveChanged.bind(this, false);
        this._filterInput.onchange = this._filterMayHaveChanged.bind(this, true);
    }

    setCategoryPage(categoryPage) { this._categoryPage = categoryPage; }
    currentCategory() { return this._currentCategory; }
    filter() { return this._filter; }
    setFilter(filter) { this._filter = filter; }

    render()
    {
        if (!this._categoryPage)
            return;

        var router = this._categoryPage.router();
        console.assert(router);

        super.render();

        var element = ComponentBase.createElement;
        var link = ComponentBase.createLink;

        if (this._filterInput.value != this._filter)
            this._filterInput.value = this._filter;

        var currentCategory = this._currentCategory;
        var categoryPage = this._categoryPage;
        this.renderReplace(this.content().querySelector('.analysis-task-category-toolbar'),
            this._categories.map(function (category) {
                return element('li',
                    {class: category == currentCategory ? 'selected' : null},
                    link(category, router.url(categoryPage.routeName(), categoryPage.stateForCategory(category))));
            }));
    }

    _filterMayHaveChanged(shouldUpdateState, event)
    {
        var input = event.target;
        var oldFilter = this._filter;
        this._filter = input.value;
        if (this._filter != oldFilter && this._categoryPage || shouldUpdateState)
            this._categoryPage.filterDidChange(shouldUpdateState);
    }

    setCategoryIfValid(category)
    {
        if (!category)
            category = this._categories[0];
        if (this._categories.indexOf(category) < 0)
            return false;
        this._currentCategory = category;
        return true;
    }

    static cssTemplate()
    {
        return Toolbar.cssTemplate() + `
            .queue-toolbar {
                position: absolute;
                right: 1rem;
            }
            .trybot-button {
                position: absolute;
                left: 1rem;
            }
        `
    }

    static htmlTemplate()
    {
        return `
            <ul class="buttoned-toolbar trybot-button">
                <li><a href="#/analysis/task/create">Create</a></li>
            </ul>
            <ul class="analysis-task-category-toolbar buttoned-toolbar"></ul>
            <input type="text">
            <ul class="buttoned-toolbar queue-toolbar">
                <li><a href="#/analysis/queue">Queue</a></li>
            </ul>`;
    }
}

ComponentBase.defineElement('analysis-category-toolbar', AnalysisCategoryToolbar);
