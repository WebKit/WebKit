
class DataModelObject {
    constructor(id)
    {
        this._id = id;
        this.namedStaticMap('id')[id] = this;
    }
    id() { return this._id; }

    namedStaticMap(name)
    {
        var newTarget = this.__proto__.constructor;
        if (!newTarget[DataModelObject.StaticMapSymbol])
            newTarget[DataModelObject.StaticMapSymbol] = {};
        var staticMap = newTarget[DataModelObject.StaticMapSymbol];
        if (!staticMap[name])
            staticMap[name] = [];
        return staticMap[name];
    }

    static namedStaticMap(name)
    {
        var staticMap = this[DataModelObject.StaticMapSymbol];
        return staticMap ? staticMap[name] : null;
    }

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
}
DataModelObject.StaticMapSymbol = Symbol();

class LabeledObject extends DataModelObject {
    constructor(id, object)
    {
        super(id);
        this._name = object.name;
        this.namedStaticMap('name')[this._name] = this;
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
