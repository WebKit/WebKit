'use strict';

class DataModelObject {
    constructor(id)
    {
        this._id = id;
        this.ensureNamedStaticMap('id')[id] = this;
    }
    id() { return this._id; }

    static ensureSingleton(id, object)
    {
        var singleton = this.findById(id);
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
        var staticMap = this[DataModelObject.StaticMapSymbol];
        return staticMap ? staticMap[name] : null;
    }

    static ensureNamedStaticMap(name)
    {
        if (!this[DataModelObject.StaticMapSymbol])
            this[DataModelObject.StaticMapSymbol] = {};
        var staticMap = this[DataModelObject.StaticMapSymbol];
        if (!staticMap[name])
            staticMap[name] = {};
        return staticMap[name];
    }

    namedStaticMap(name) { return this.__proto__.constructor.namedStaticMap(name); }
    ensureNamedStaticMap(name) { return this.__proto__.constructor.ensureNamedStaticMap(name); }

    static findById(id)
    {
        var idMap = this.namedStaticMap('id');
        return idMap ? idMap[id] : null;
    }

    static listForStaticMap(name)
    {
        var list = [];
        var idMap = this.namedStaticMap(name);
        if (idMap) {
            for (var id in idMap)
                list.push(idMap[id]);
        }
        return list;
    }

    static all() { return this.listForStaticMap('id'); }

    static cachedFetch(path, params, noCache)
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

        var cacheMap = this.ensureNamedStaticMap(DataModelObject.CacheMapSymbol);
        if (!cacheMap[path])
            cacheMap[path] = RemoteAPI.getJSONWithStatus(path);

        return cacheMap[path].then((content) => {
            return JSON.parse(JSON.stringify(content));
        });
    }

}
DataModelObject.StaticMapSymbol = Symbol();
DataModelObject.CacheMapSymbol = Symbol();

class LabeledObject extends DataModelObject {
    constructor(id, object)
    {
        super(id);
        this._name = object.name;
    }

    updateSingleton(object) { this._name = object.name; }

    static sortByName(list)
    {
        return list.sort(function (a, b) {
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
