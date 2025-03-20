
class MutableListView extends ComponentBase {

    constructor()
    {
        super('mutable-list-view');
        this._list = [];
        this._kindList = [];
        this._kindMap = new Map;
        this.content().querySelector('form').onsubmit = this._submitted.bind(this);
    }

    setList(list)
    {
        this._list = list;
        this.enqueueToRender();
    }

    setKindList(list)
    {
        this._kindList = list;
        this.enqueueToRender();
    }

    render()
    {
        this.renderReplace(this.content().querySelector('.mutable-list'),
            this._list.map(function (item) {
                console.assert(item instanceof MutableListItem);
                return item.content();
            }));

        var element = ComponentBase.createElement;
        var kindMap = this._kindMap;
        kindMap.clear();
        this.renderReplace(this.content().querySelector('.kind'),
            this._kindList.map(function (kind) {
                kindMap.set(kind.id(), kind);
                return element('option', {value: kind.id()}, kind.label());
            }));
    }

    _submitted(event)
    {
        event.preventDefault();
        const kind = this._kindMap.get(this.content().querySelector('.kind').value);
        const item = this.content().querySelector('.value').value;
        this.dispatchAction('addItem', kind, item);
    }

    static cssTemplate()
    {
        return `
            .mutable-list,
            .mutable-list li {
                list-style: none;
                padding: 0;
                margin: 0;
            }
            
            .mutable-list:not(:empty) {
                margin-bottom: 1rem;
            }

            .mutable-list {
                margin-bottom: 1rem;
            }

            .new-list-item-form {
                white-space: nowrap;
            }
        `;
    }

    static htmlTemplate()
    {
        return `
            <ul class="mutable-list"></ul>
            <form class="new-list-item-form">
                <select class="kind"></select>
                <input class="value">
                <button type="submit">Add</button>
            </form>`;
    }

}

class MutableListItem {
    constructor(kind, value, valueTitle, valueLink, removalTitle, removalLink)
    {
        this._kind = kind;
        this._value = value;
        this._valueTitle = valueTitle;
        this._valueLink = valueLink;
        this._removalTitle = removalTitle;
        this._removalLink = removalLink;
    }

    content()
    {
        const link = ComponentBase.createLink;
        const closeButton = new CloseButton;
        closeButton.listenToAction('activate', this._removalLink);
        return ComponentBase.createElement('li', [
            this._kind.label(),
            ' ',
            link(this._value, this._valueTitle, this._valueLink),
            ' ',
            link(closeButton, this._removalTitle, this._removalLink)]);
    }
}

ComponentBase.defineElement('mutable-list-view', MutableListView);
