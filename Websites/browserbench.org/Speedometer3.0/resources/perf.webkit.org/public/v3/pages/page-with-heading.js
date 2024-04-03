
class PageWithHeading extends Page {
    constructor(name, toolbar)
    {
        super(name);
        this._heading = null;
        this._parentPage = null;
        this._toolbar = toolbar;
    }

    open(state)
    {
        console.assert(this.heading());
        this.heading().setToolbar(this._toolbar);
        super.open(state);
    }

    setParentPage(page) { this._parentPage = page; }
    belongsTo(page) { return this == page || this._parentPage == page; }
    setHeading(heading) { this._heading = heading; }
    // FIXME: We shouldn't rely on the page hierarchy to find the heading. 
    heading() { return this._heading || this._parentPage.heading(); }
    toolbar() { return this._toolbar; }
    title() { return this.name(); }
    pageTitle() { return this.title() + ' - ' + this.heading().title(); }

    render()
    {
        console.assert(this.heading());

        if (document.body.firstChild != this.heading().element())
            document.body.insertBefore(this.heading().element(), document.body.firstChild);

        super.render();
        this.heading().enqueueToRender();
    }

    static htmlTemplate()
    {
        return `<section class="page-with-heading"></section>`;
    }

}
