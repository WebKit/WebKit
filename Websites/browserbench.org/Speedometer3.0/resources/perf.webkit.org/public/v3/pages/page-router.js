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
        let destinationPage = this._defaultPage;
        const parsed = this._deserializeFromHash(location.hash);
        if (parsed.route) {
            let hashUrl = parsed.route;
            let bestMatchingRouteName = null;
            const queryIndex = hashUrl.indexOf('?');
            if (queryIndex >= 0)
                hashUrl = hashUrl.substring(0, queryIndex);

            for (const page of this._pages) {
                const routeName = page.routeName();

                if (routeName == hashUrl) {
                    bestMatchingRouteName = routeName;
                    destinationPage = page;
                    break;
                } else if (hashUrl.startsWith(routeName) && hashUrl.charAt(routeName.length) == '/'
                    && (!bestMatchingRouteName || bestMatchingRouteName.length < routeName.length)) {
                    bestMatchingRouteName = routeName;
                    destinationPage = page;
                }
            }

            if (bestMatchingRouteName)
                parsed.state.remainingRoute = hashUrl.substring(bestMatchingRouteName.length + 1);
        }

        if (!destinationPage)
            return false;

        if (this._currentPage != destinationPage) {
            this._currentPage = destinationPage;
            destinationPage.open(parsed.state);
        } else
            destinationPage.updateFromSerializedState(parsed.state, false);
        destinationPage.enqueueToRender();

        return true;
    }

    pageDidOpen(page)
    {
        console.assert(page instanceof Page);
        const pageDidChange = this._currentPage != page;
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
        const currentPage = this._currentPage;
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
        const params = [];
        for (const key in state)
            params.push(key + '=' + this._serializeHashQueryValue(state[key]));
        const query = params.length ? ('?' + params.join('&')) : '';
        return `#/${route}${query}`;
    }
    
    _deserializeFromHash(hash)
    {
        if (!hash || !hash.startsWith('#/'))
            return {route: null, state: {}};

        hash = unescape(hash); // For Firefox.

        const queryIndex = hash.indexOf('?');
        let route;
        const state = {};
        if (queryIndex >= 0) {
            route = hash.substring(2, queryIndex);
            for (const part of hash.substring(queryIndex + 1).split('&')) {
                const keyValuePair = part.split('=');
                state[keyValuePair[0]] = this._deserializeHashQueryValue(keyValuePair[1]);
            }
        } else
            route = hash.substring(2);

        return {route: route, state: state};
    }

    _serializeHashQueryValue(value)
    {
        if (value instanceof Array) {
            const serializedItems = [];
            for (const item of value)
                serializedItems.push(this._serializeHashQueryValue(item));
            return '(' + serializedItems.join('-') + ')';
        }
        if (value instanceof Set)
            return Array.from(value).sort().join('|');
        console.assert(value === null || value === undefined || typeof(value) === 'number' || /[0-9]*/.test(value));
        return value === null || value === undefined ? 'null' : value;
    }

    _deserializeHashQueryValue(value)
    {
        if (value.charAt(0) == '(') {
            let nestingLevel = 0;
            let end = 0;
            let start = 1;
            const result = [];
            for (const character of value) {
                if (character == '(')
                    nestingLevel++;
                else if (character == ')') {
                    nestingLevel--;
                    if (!nestingLevel)
                        break;
                } else if (nestingLevel == 1 && character == '-') {
                    result.push(this._deserializeHashQueryValue(value.substring(start, end)));
                    start = end + 1;
                }
                end++;
            }
            result.push(this._deserializeHashQueryValue(value.substring(start, end)));
            return result;
        }
        if (value == 'true')
            return true;
        if (value == 'false')
            return true;
        if (value.match(/^[0-9\.]+$/))
            return parseFloat(value);
        if (value.match(/^[A-Za-z][A-Za-z0-9|]*$/))
            return new Set(value.toLowerCase().split('|'));
        return null;
    }

    _countOccurrences(string, regex)
    {
        const match = string.match(regex);
        return match ? match.length : 0;
    }
}
