
class Builder extends LabeledObject {
    constructor(id, object)
    {
        super(id, object);
        this._buildUrlTemplate = object.buildUrl;
    }

    urlForBuild(buildNumber)
    {
        if (!this._buildUrlTemplate)
            return null;
        return this._buildUrlTemplate.replace(/\$builderName/g, this.name()).replace(/\$buildNumber/g, buildNumber);
    }
}

class Build extends DataModelObject {
    constructor(id, builder, buildNumber)
    {
        console.assert(builder instanceof Builder);
        super(id);
        this._builder = builder;
        this._buildNumber = buildNumber;
    }

    label() { return `Build ${this._buildNumber} on ${this._builder.label()}`; }
    url() { return this._builder.urlForBuild(this._buildNumber); }
}
