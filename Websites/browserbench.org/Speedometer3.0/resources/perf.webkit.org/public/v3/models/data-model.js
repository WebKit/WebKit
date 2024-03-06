'use strict';

class DataModelObject {
    _id;

    constructor(id)
    {
        this._id = id;
        this.ensureNamedStaticMap('id')[id] = this;
    }
    id() { return this._id; }

    static ensureSingleton(id, object)
    {
        const singleton = this.findById(id);
        if (singleton) {
            singleton.updateSingleton(object)
            return singleton;
        }
        return new (this)(id, object);
    }

    updateSingleton(object) { }

    static clearStaticMap() { this[DataModelObject.StaticMapSymbol] = null; }

    static namedStaticMap(name)
    {
        const staticMap = this[DataModelObject.StaticMapSymbol];
        return staticMap ? staticMap.get(name) : null;
    }

    static ensureNamedStaticMap(name)
    {
        if (!this[DataModelObject.StaticMapSymbol])
            this[DataModelObject.StaticMapSymbol] = new Map;
        const staticMap = this[DataModelObject.StaticMapSymbol];
        let namedMap = staticMap.get(name);
        if (!namedMap) {
            namedMap = { }; // Use a regular object to implicitly convert each key to a string.
            staticMap.set(name, namedMap);
        }
        return namedMap;
    }

    namedStaticMap(name) { return this.__proto__.constructor.namedStaticMap(name); }
    ensureNamedStaticMap(name) { return this.__proto__.constructor.ensureNamedStaticMap(name); }

    static findById(id)
    {
        const idMap = this.namedStaticMap('id');
        return idMap ? idMap[id] : null;
    }

    static listForStaticMap(name)
    {
        const list = [];
        const idMap = this.namedStaticMap(name);
        if (idMap) {
            for (const id in idMap)
                list.push(idMap[id]);
        }
        return list;
    }

    static all() { return this.listForStaticMap('id'); }

    static async cachedFetch(path, params = { }, noCache = false)
    {
        const query = [];
        if (params) {
            for (let key in params)
                query.push(key + '=' + escape(params[key]));
        }
        if (query.length)
            path += '?' + query.join('&');

        if (noCache)
            return RemoteAPI.getJSONWithStatus(path);

        const cacheMap = this.ensureNamedStaticMap(DataModelObject.CacheMapSymbol);
        let promise = cacheMap[path];
        if (!promise) {
            promise = RemoteAPI.getJSONWithStatus(path);
            cacheMap[path] = promise;
        }

        const content = await cacheMap[path];
        return JSON.parse(JSON.stringify(content));
    }

}
DataModelObject.StaticMapSymbol = Symbol();
DataModelObject.CacheMapSymbol = Symbol();

class LabeledObject extends DataModelObject {
    _name;

    constructor(id, object)
    {
        super(id);
        this._name = object.name;
    }

    updateSingleton(object) { this._name = object.name; }

    static sortByName(list)
    {
        return list.sort((a, b) => {
            if (a.name() < b.name())
                return -1;
            else if (a.name() > b.name())
                return 1;
            return 0;
        });
    }
    sortByName(list) { return LabeledObject.sortByName(list); }

    name() { return this._name; }
    label() { return this.name(); }
}

if (typeof module != 'undefined') {
    module.exports.DataModelObject = DataModelObject;
    module.exports.LabeledObject = LabeledObject;
}
