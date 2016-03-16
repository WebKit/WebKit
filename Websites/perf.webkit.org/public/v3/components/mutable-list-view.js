

class MutableListView extends ComponentBase {

    constructor()
    {
        super('mutable-list-view');
        this._list = [];
        this._kindList = [];
        this._addCallback = null;
        this._kindMap = new Map;
        this.content().querySelector('form').onsubmit = this._submitted.bind(this);
    }

    setList(list) { this._list = list; }
    setKindList(list) { this._kindList = list; }
    setAddCallback(callback) { this._addCallback = callback; }

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
        if (this._addCallback)
            this._addCallback(this._kindMap.get(this.content().querySelector('.kind').value), this.content().querySelector('.value').value);
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
        var link = ComponentBase.createLink;
        return ComponentBase.createElement('li', [
            this._kind.label(),
            ' ',
            link(this._value, this._valueTitle, this._valueLink),
            ' ',
            link(new CloseButton, this._removalTitle, this._removalLink)]);
    }
}

ComponentBase.defineElement('mutable-list-view', MutableListView);
