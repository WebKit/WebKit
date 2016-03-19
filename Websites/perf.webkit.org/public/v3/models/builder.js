'use strict';

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
    constructor(id, builder, buildNumber, buildTime)
    {
        console.assert(builder instanceof Builder);
        super(id);
        this._builder = builder;
        this._buildNumber = buildNumber;
        this._buildTime = new Date(buildTime);
    }

    builder() { return this._builder; }
    buildNumber() { return this._buildNumber; }
    buildTime() { return this._buildTime; }
    label() { return `Build ${this._buildNumber} on ${this._builder.label()}`; }
    url() { return this._builder.urlForBuild(this._buildNumber); }
}

if (typeof module != 'undefined') {
    module.exports.Builder = Builder;
    module.exports.Build = Build;
}
