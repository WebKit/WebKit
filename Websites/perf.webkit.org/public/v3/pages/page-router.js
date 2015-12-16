class PageRouter {
    constructor()
    {
        this._pages = [];
        this._defaultPage = null;
        this._currentPage = null;
        this._historyTimer = null;
        this._hash = null;

        window.onhashchange = this._hashDidChange.bind(this);
    }

    addPage(page)
    {
        this._pages.push(page);
        page.setRouter(this);
    }

    setDefaultPage(defaultPage)
    {
        this._defaultPage = defaultPage;
    }

    currentPage() { return this._currentPage; }

    route()
    {
        var destinationPage = this._defaultPage;
        var parsed = this._deserializeFromHash(location.hash);
        if (parsed.route) {
            var hashUrl = parsed.route;
            var queryIndex = hashUrl.indexOf('?');
            if (queryIndex >= 0)
                hashUrl = hashUrl.substring(0, queryIndex);

            for (var page of this._pages) {
                var routeName = page.routeName();
                if (routeName == hashUrl
                    || (hashUrl.startsWith(routeName) && hashUrl.charAt(routeName.length) == '/')) {
                    parsed.state.remainingRoute = hashUrl.substring(routeName.length + 1);
                    destinationPage = page;
                    break;
                }
            }
        }

        if (!destinationPage)
            return false;

        if (this._currentPage != destinationPage) {
            this._currentPage = destinationPage;
            destinationPage.open(parsed.state);
        } else
            destinationPage.updateFromSerializedState(parsed.state, false);

        return true;
    }

    pageDidOpen(page)
    {
        console.assert(page instanceof Page);
        var pageDidChange = this._currentPage != page;
        this._currentPage = page;
        if (pageDidChange)
            this.scheduleUrlStateUpdate();
    }

    scheduleUrlStateUpdate()
    {
        if (this._historyTimer)
            return;
        this._historyTimer = setTimeout(this._updateURLState.bind(this), 0);
    }

    url(routeName, state)
    {
        return this._serializeToHash(routeName, state);
    }

    _updateURLState()
    {
        this._historyTimer = null;
        console.assert(this._currentPage);
        var currentPage = this._currentPage;
        this._hash = this._serializeToHash(currentPage.routeName(), currentPage.serializeState());
        location.hash = this._hash;
    }

    _hashDidChange()
    {
        if (unescape(location.hash) == this._hash)
            return;
        this.route();
        this._hash = null;
    }

    _serializeToHash(route, state)
    {
        var params = [];
        for (var key in state)
            params.push(key + '=' + this._serializeHashQueryValue(state[key]));
        var query = params.length ? ('?' + params.join('&')) : '';
        return `#/${route}${query}`;
    }
    
    _deserializeFromHash(hash)
    {
        if (!hash || !hash.startsWith('#/'))
            return {route: null, state: {}};

        hash = unescape(hash); // For Firefox.

        var queryIndex = hash.indexOf('?');
        var route;
        var state = {};
        if (queryIndex >= 0) {
            route = hash.substring(2, queryIndex);
            for (var part of hash.substring(queryIndex + 1).split('&')) {
                var keyValuePair = part.split('=');
                state[keyValuePair[0]] = this._deserializeHashQueryValue(keyValuePair[1]);
            }
        } else
            route = hash.substring(2);

        return {route: route, state: state};
    }

    _serializeHashQueryValue(value)
    {
        if (!(value instanceof Array)) {
            console.assert(value === null || typeof(value) === 'number' || /[A-Za-z0-9]*/.test(value));
            return value === null ? 'null' : value;
        }

        var serializedItems = [];
        for (var item of value)
            serializedItems.push(this._serializeHashQueryValue(item));
        return '(' + serializedItems.join('-') + ')';
    }

    _deserializeHashQueryValue(value)
    {
        try {
            return JSON.parse(value.replace(/\(/g, '[').replace(/\)/g, ']').replace(/-/g, ','));
        } catch (error) {
            return value;
        }
    }
}
