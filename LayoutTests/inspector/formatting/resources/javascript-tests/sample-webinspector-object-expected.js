WebInspector.Object = class WebInspectorObject
{
    constructor()
    {
        this._listeners = null;
    }
    static addEventListener(eventType, listener, thisObject)
    {
        thisObject = thisObject || null;
        if (!eventType)
            return;
        if (!listener)
            return;
        if (!this._listeners)
            this._listeners = new Map();
        let listenersTable = this._listeners.get(eventType);
        if (!listenersTable) {
            listenersTable = new ListMultimap();
            this._listeners.set(eventType, listenersTable);
        }
        listenersTable.add(thisObject, listener);
    }
    static singleFireEventListener(eventType, listener, thisObject)
    {
        let wrappedCallback = function() {
            this.removeEventListener(eventType, wrappedCallback, null);
            listener.apply(thisObject, arguments);
        }.bind(this);
        this.addEventListener(eventType, wrappedCallback, null);
        return wrappedCallback;
    }
    static removeEventListener(eventType, listener, thisObject)
    {
        eventType = eventType || null;
        listener = listener || null;
        thisObject = thisObject || null;
        if (!this._listeners)
            return;
        if (thisObject && !eventType) {
            this._listeners.forEach(function(listenersTable) {
                let listenerPairs = listenersTable.toArray();
                for (let i = 0, length = listenerPairs.length; i < length; ++i) {
                    let existingThisObject = listenerPairs[i][0];
                    if (existingThisObject === thisObject)
                        listenersTable.deleteAll(existingThisObject);
                }
            });
            return;
        }
        let listenersTable = this._listeners.get(eventType);
        if (!listenersTable || listenersTable.size === 0)
            return;
        let didDelete = listenersTable.delete(thisObject, listener);
    }
    static hasEventListeners(eventType)
    {
        if (!this._listeners)
            return false;
        let listenersTable = this._listeners.get(eventType);
        return listenersTable && listenersTable.size > 0;
    }
    static retainedObjectsWithPrototype(proto)
    {
        let results = new Set;
        if (this._listeners) {
            this._listeners.forEach(function(listenersTable, eventType) {
                listenersTable.forEach(function(pair) {
                    let thisObject = pair[0];
                    if (thisObject instanceof proto)
                        results.add(thisObject);
                });
            });
        }
        return results;
    }
    addEventListener()
    {
        return WebInspector.Object.addEventListener.apply(this, arguments);
    }
    singleFireEventListener()
    {
        return WebInspector.Object.singleFireEventListener.apply(this, arguments);
    }
    removeEventListener()
    {
        return WebInspector.Object.removeEventListener.apply(this, arguments);
    }
    hasEventListeners()
    {
        return WebInspector.Object.hasEventListeners.apply(this, arguments);
    }
    retainedObjectsWithPrototype()
    {
        return WebInspector.Object.retainedObjectsWithPrototype.apply(this, arguments);
    }
    dispatchEventToListeners(eventType, eventData)
    {
        let event = new WebInspector.Event(this, eventType, eventData);
        function dispatch(object)
        {
            if (!object || event._stoppedPropagation)
                return;
            let listenerTypesMap = object._listeners;
            if (!listenerTypesMap || !object.hasOwnProperty("_listeners"))
                return;
            let listenersTable = listenerTypesMap.get(eventType);
            if (!listenersTable)
                return;
            let listeners = listenersTable.toArray();
            for (let i = 0, length = listeners.length; i < length; ++i) {
                let [thisObject, listener] = listeners[i];
                listener.call(thisObject, event);
                if (event._stoppedPropagation)
                    break;
            }
        }
        dispatch(this);
        event._stoppedPropagation = false;
        let constructor = this.constructor;
        while (constructor) {
            dispatch(constructor);
            if (!constructor.prototype.__proto__)
                break;
            constructor = constructor.prototype.__proto__.constructor;
        }
        return event.defaultPrevented;
    }
}
;
WebInspector.Event = class Event
{
    constructor(target, type, data)
    {
        this.target = target;
        this.type = type;
        this.data = data;
        this.defaultPrevented = false;
        this._stoppedPropagation = false;
    }
    stopPropagation()
    {
        this._stoppedPropagation = true;
    }
    preventDefault()
    {
        this.defaultPrevented = true;
    }
}
;
WebInspector.notifications = new WebInspector.Object;
WebInspector.Notification = {
    GlobalModifierKeysDidChange: "global-modifiers-did-change",
    PageArchiveStarted: "page-archive-started",
    PageArchiveEnded: "page-archive-ended",
    ExtraDomainsActivated: "extra-domains-activated",
    TabTypesChanged: "tab-types-changed",
    DebugUIEnabledDidChange: "debug-ui-enabled-did-change",
};
