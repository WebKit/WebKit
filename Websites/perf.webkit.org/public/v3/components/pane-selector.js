
class PaneSelector extends ComponentBase {
    constructor()
    {
        super('pane-selector');
        this._currentPlatform = null;
        this._currentPath = [];
        this._platformItems = [];
        this._renderedMetric = null;
        this._renderedPath = null;
        this._updateTimer = null;
        this._platformContainer = this.content().querySelector('#platform');
        this._testsContainer = this.content().querySelector('#tests');
        this._callback = null;
        this._previouslySelectedItem = null;
    }

    render()
    {
        this._renderPlatformList();
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

    _renderPlatformList()
    {
        var currentMetric = null;
        if (this._currentPath.length) {
            var lastItem = this._currentPath[this._currentPath.length - 1];
            if (lastItem instanceof Metric)
                currentMetric = lastItem;
        }

        if (this._renderedMetric != currentMetric) {
            if (this._platformContainer.firstChild)
                this._platformContainer.removeChild(this._platformContainer.firstChild);

            this._renderedMetric = currentMetric;
            this._platformItems = [];

            if (currentMetric) {
                for (var platform of Platform.sortByName(Platform.all())) {
                    if (platform.hasMetric(currentMetric))
                        this._platformItems.push(this._createListItem(platform, platform.label()));
                }
                this._platformContainer.appendChild(this._buildList(this._platformItems));
            }
        }

        for (var li of this._platformItems) {
            if (li.data == this._currentPlatform)
                li.selected = true;
        }
    }

    _renderTestLists()
    {
        if (this._renderedPath == null)
            this._replaceList(0, this._buildTestList(Test.topLevelTests()), []);

        for (var i = 0; i < this._currentPath.length; i++) {
            var item = this._currentPath[i];
            if (this._renderedPath[i] == item)
                continue;
            if (item instanceof Metric)
                break;
            var newList = this._buildTestList(Test.sortByName(item.childTests()), Metric.sortByName(item.metrics()));
            this._replaceList(i + 1, newList);
        }

        var removeNodeCount = this._testsContainer.childNodes.length - i - 1;
        if (removeNodeCount > 0) {
            while (removeNodeCount--)
                this._testsContainer.removeChild(this._testsContainer.lastChild);
        }

        for (var i = 0; i < this._currentPath.length; i++) {
            var list = this._testsContainer.childNodes[i];
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
            .map(function (metric) { return self._createListItem(metric, metric.label()); });

        var testItems = tests
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
        var existingList = this._testsContainer.childNodes[position];
        if (existingList)
            this._testsContainer.replaceChild(newList, existingList);
        else
            this._testsContainer.appendChild(newList);
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

        if (data instanceof Platform)
            this._currentPlatform = data;
        else {
            this._currentPath = data.path();
            if (data instanceof Metric && data.test().onlyContainsSingleMetric())
                this._currentPath.splice(this._currentPath.length - 2, 1);
        }
        this.enqueueToRender();
    }

    setCallback(callback)
    {
        this._callback = callback;
    }

    _clickedItem(data, event)
    {
        if (!(data instanceof Platform) || !this._callback || !this._currentPlatform || !this._currentPath.length)
            return;
        event.preventDefault();
        this._callback(this._currentPlatform, this._currentPath[this._currentPath.length - 1]);
    }

    static htmlTemplate()
    {
        return `
            <div class="pane-selector-container"><div id="tests"></div><div id="platform"></div></div>
        `;
    }

    static cssTemplate()
    {
        return `
            .pane-selector-container,
            .pane-selector-container > div {
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
