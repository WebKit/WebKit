
class Heading extends ComponentBase {
    constructor()
    {
        super('page-heading');
        this._title = '';
        this._pageGroups = [];
        this._renderedCurrentPage = null;
        this._toolbar = null;
        this._toolbarChanged = false;
        this._router = null;
    }

    title() { return this._title; }
    setTitle(title) { this._title = title; }

    addPageGroup(group)
    {
        for (var page of group)
            page.setHeading(this);
        this._pageGroups.push(group);
    }

    setToolbar(toolbar)
    {
        console.assert(!toolbar || toolbar instanceof Toolbar);
        this._toolbar = toolbar;
        if (toolbar)
            toolbar.setRouter(this._router);

        this._toolbarChanged = true;
    }

    setRouter(router)
    {
        this._router = router;
        if (this._toolbar)
            this._toolbar.setRouter(router);
    }

    render()
    {
        console.assert(this._router);
        super.render();

        if (this._toolbarChanged) {
            this.renderReplace(this.content().querySelector('.heading-toolbar'),
                this._toolbar ? this._toolbar.element() : null);
            this._toolbarChanged = false;
        }

        if (this._toolbar)
            this._toolbar.enqueueToRender();

        var currentPage = this._router.currentPage();
        if (this._renderedCurrentPage == currentPage)
            return;
        this._renderedCurrentPage = currentPage;

        var title = this.content().querySelector('.heading-title a');
        title.textContent = this._title;

        var element = ComponentBase.createElement;
        var link = ComponentBase.createLink;
        var router = this._router;

        this.renderReplace(this.content().querySelector('.heading-navigation-list'),
            this._pageGroups.map(function (group) {
                return element('ul', group.map(function (page) {
                    return element('li',
                        { class: currentPage.belongsTo(page) ? 'selected' : '', },
                        link(page.name(), router.url(page.routeName(), page.serializeState())));
                }));
            }));
    }

    static htmlTemplate()
    {
        return `
            <nav class="heading-navigation" role="navigation">
                <h1 class="heading-title"><a href="#"></a></h1>
                <div class="heading-navigation-list"></div>
                <div class="heading-toolbar"></div>
            </nav>
        `;
    }

    static cssTemplate()
    {
        return `
            .heading-navigation {
                position: relative;
                font-size: 1rem;
                line-height: 1rem;
            }

            .heading-title {
                position: relative;
                z-index: 2;
                margin: 0;
                padding: 1rem;
                border-bottom: solid 1px #ccc;
                background: #fff;
                color: #c93;
                font-size: 1.5rem;
                font-weight: inherit;
            }

            .heading-title a {
                text-decoration: none;
                color: inherit;
            }

            .heading-navigation-list {
                display: block;
                white-space: nowrap;
                border-bottom: solid 1px #ccc;
                text-align: center;
                margin: 0;
                margin-bottom: 1rem;
                padding: 0;
                padding-bottom: 0.3rem;
            }

            .heading-navigation-list ul {
                display: inline;
                margin: 0;
                padding: 0;
                margin-left: 1rem;
                border-left: solid 1px #ccc;
                padding-left: 1rem;
            }

            .heading-navigation-list ul:first-child {
                border-left: none;
            }

            .heading-navigation-list li {
                display: inline-block;
                position: relative;
                list-style: none;
                margin: 0.3rem 0.5rem;
                padding: 0;
            }

            .heading-navigation-list a {
                text-decoration: none;
                color: inherit;
                color: #666;
            }

            .heading-navigation-list a:hover {
                color: #369;
            }

            .heading-navigation-list li.selected a {
                color: #000;
            }

            .heading-navigation-list li.selected a:before {
                content: '';
                display: block;
                border: solid 5px #ccc;
                border-color: transparent transparent #ccc transparent;
                width: 0px;
                height: 0px;
                position: absolute;
                left: 50%;
                margin-left: -5px;
                bottom: -0.55rem;
            }

            .heading-toolbar {
                position: absolute;
                right: 1rem;
                top: 0.8rem;
                z-index: 3;
            }`;
    }

}
