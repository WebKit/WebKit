
class PaneSelector extends ComponentBase {
    constructor()
    {
        super('pane-selector');
        this._currentPlatform = null;
        this._currentPath = [];
        this._platformItems = null;
        this._renderedPlatform = null;
        this._renderedPath = null;
        this._updateTimer = null;
        this._container = this.content().querySelector('.pane-selector-container');
        this._callback = null;
        this._previouslySelectedItem = null;
    }

    render()
    {
        this._renderPlatformLists();
        this._renderTestLists();
    }

    focus()
    {
        var select = this.content().querySelector('select');
        if (select) {
            if (select.selectedIndex < 0)
                select.selectedIndex = 0;
            select.focus();
        }
    }

    _renderPlatformLists()
    {
        if (!this._platformItems) {
            this._platformItems = [];

            var platforms = Platform.sortByName(Platform.all());
            for (var platform of platforms)
                this._platformItems.push(this._createListItem(platform, platform.label()));

            this._replaceList(0, this._buildList(this._platformItems));
        }

        for (var li of this._platformItems) {
            if (li.data == this._currentPlatform)
                li.selected = true;
        }
    }

    _renderTestLists()
    {
        if (this._renderedPlatform != this._currentPlatform) {
            this._replaceList(1, this._buildTestList(Test.topLevelTests()), []);
            this._renderedPlatform = this._currentPlatform;
        }

        for (var i = 0; i < this._currentPath.length; i++) {
            var item = this._currentPath[i];
            if (this._renderedPath[i] == item)
                continue;
            if (item instanceof Metric)
                break;
            var newList = this._buildTestList(Test.sortByName(item.childTests()), Metric.sortByName(item.metrics()));
            this._replaceList(i + 2, newList);
        }

        var removeNodeCount = this._container.childNodes.length - i - 2;
        if (removeNodeCount > 0) {
            while (removeNodeCount--)
                this._container.removeChild(this._container.lastChild);
        }

        for (var i = 0; i < this._currentPath.length; i++) {
            var list = this._container.childNodes[i + 1];
            var item = this._currentPath[i];
            for (var j = 0; j < list.childNodes.length; j++) {
                var option = list.childNodes[j];
                if (option.data == item)
                    option.selected = true;
            }
        }

        this._renderedPath = this._currentPath;
    }

    _buildTestList(tests, metrics)
    {
        var self = this;
        var platform = this._currentPlatform;

        var metricItems = (metrics || [])
            .filter(function (metric) { return platform && platform.hasMetric(metric); })
            .map(function (metric) { return self._createListItem(metric, metric.label()); });

        var testItems = tests
            .filter(function (test) { return platform && platform.hasTest(test); })
            .map(function (test) {
                var data = test;
                var label = test.label();
                if (test.onlyContainsSingleMetric()) {
                    data = test.metrics()[0];
                    label = test.label() + ' (' + data.label() + ')';
                }
                return self._createListItem(data, label);
            });

        return this._buildList([metricItems, testItems]);
    }

    _replaceList(position, newList)
    {
        var existingList = this._container.childNodes[position];
        if (existingList)
            this._container.replaceChild(newList, existingList);
        else
            this._container.appendChild(newList);
    }

    _createListItem(data, label, hoverCallback, activationCallback)
    {
        var element = ComponentBase.createElement;
        var item = element('option', {
            // Can't use mouseenter because of webkit.org/b/152149.
            onmousemove: this._selectedItem.bind(this, data),
            onclick: this._clickedItem.bind(this, data),
        }, label);
        item.data = data;
        return item;
    }

    _buildList(items, onchange)
    {
        var self = this;
        return ComponentBase.createElement('select', {
            size: 10,
            onmousemove: function (event) {
                
            },
            onchange: function () {
                if (this.selectedIndex >= 0)
                    self._selectedItem(this.options[this.selectedIndex].data);
            }
        }, items);
    }

    _selectedItem(data)
    {
        if (data == this._previouslySelectedItem)
            return;
        this._previouslySelectedItem = data;

        if (data instanceof Platform) {
            this._currentPlatform = data;
            this._currentPath = [];
        } else {
            this._currentPath = data.path();
            if (data instanceof Metric && data.test().onlyContainsSingleMetric())
                this._currentPath.splice(this._currentPath.length - 2, 1);
        }
        this.render();
    }

    setCallback(callback)
    {
        this._callback = callback;
    }

    _clickedItem(data, event)
    {
        if (!(data instanceof Metric) || !this._callback || !this._currentPlatform || !this._currentPath.length)
            return;
        event.preventDefault();
        this._callback(this._currentPlatform, this._currentPath[this._currentPath.length - 1]);
    }

    static htmlTemplate()
    {
        return `
            <div class="pane-selector-container"></div>
        `;
    }

    static cssTemplate()
    {
        return `
            .pane-selector-container {
                display: flex;
                flex-direction: row-reverse;
                height: 10rem;
                font-size: 0.9rem;
                white-space: nowrap;
            }

            .pane-selector-container select {
                height: 100%;
                border: solid 1px red;
                font-size: 0.9rem;
                border: solid 1px #ccc;
                border-radius: 0.2rem;
                margin-right: 0.2rem;
                background: transparent;
                max-width: 20rem;
            }

            .pane-selector-container li.selected a {
                background: rgba(204, 153, 51, 0.1);
            }
        `;
    }
}

ComponentBase.defineElement('pane-selector', PaneSelector);
