
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
        if (singleton)
            return singleton;
        return new (this)(id, object);
    }

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
            staticMap[name] = [];
        return staticMap[name];
    }

    namedStaticMap(name) { return this.__proto__.constructor.namedStaticMap(name); }
    ensureNamedStaticMap(name) { return this.__proto__.constructor.ensureNamedStaticMap(name); }

    static findById(id)
    {
        var idMap = this.namedStaticMap('id');
        return idMap ? idMap[id] : null;
    }

    static all()
    {
        var list = [];
        var idMap = this.namedStaticMap('id');
        if (idMap) {
            for (var id in idMap)
                list.push(idMap[id]);
        }
        return list;
    }

    static cachedFetch(path, params, noCache)
    {
        var query = [];
        if (params) {
            for (var key in params)
                query.push(key + '=' + parseInt(params[key]));
        }
        if (query.length)
            path += '?' + query.join('&');

        if (noCache)
            return getJSONWithStatus(path);

        var cacheMap = this.ensureNamedStaticMap(DataModelObject.CacheMapSymbol);
        if (!cacheMap[path])
            cacheMap[path] = getJSONWithStatus(path);

        return cacheMap[path];
    }

}
DataModelObject.StaticMapSymbol = Symbol();
DataModelObject.CacheMapSymbol = Symbol();

class LabeledObject extends DataModelObject {
    constructor(id, object)
    {
        super(id);
        this._name = object.name;
        this.ensureNamedStaticMap('name')[this._name] = this;
    }

    static findByName(name)
    {
        var nameMap = this.namedStaticMap('id');
        return nameMap ? nameMap[name] : null;
    }

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
