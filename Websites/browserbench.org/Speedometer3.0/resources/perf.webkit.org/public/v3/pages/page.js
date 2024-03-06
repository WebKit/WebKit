
class Page extends ComponentBase {
    constructor(name)
    {
        super('page-component');
        this._name = name;
        this._router = null;
    }

    name() { return this._name; }
    pageTitle() { return this.name(); }

    open(state)
    {
        // FIXME: Do something better here.
        document.body.innerHTML = '';
        document.body.appendChild(this.element());
        document.title = this.pageTitle();
        if (this._router)
            this._router.pageDidOpen(this);
        this.updateFromSerializedState(state, true);
        this.enqueueToRender();
    }

    enqueueToRender()
    {
        if (this._router && this._router.currentPage() != this)
            return;
        super.enqueueToRender();
    }

    render()
    {
        const title = this.pageTitle();
        if (document.title != title)
            document.title = title;
        super.render();
    }

    setRouter(router) { this._router = router; }
    router() { return this._router; }
    scheduleUrlStateUpdate() { this._router.scheduleUrlStateUpdate(); }
    routeName() { throw 'NotImplemented'; }
    serializeState() { return {}; }
    updateFromSerializedState(state, isOpen) { }
}
