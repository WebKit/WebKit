'use strict';

class Builder extends LabeledObject {
    constructor(id, object)
    {
        super(id, object);
        this._buildUrlTemplate = object.buildUrl;
    }

    urlForBuild(buildTag)
    {
        if (!this._buildUrlTemplate)
            return null;
        return this._buildUrlTemplate.replace(/\$builderName/g, this.name()).replace(/\$buildTag/g, buildTag);
    }
}

class Build extends DataModelObject {
    constructor(id, builder, buildTag, buildTime)
    {
        console.assert(builder instanceof Builder);
        super(id);
        this._builder = builder;
        this._buildTag = buildTag;
        this._buildTime = new Date(buildTime);
    }

    builder() { return this._builder; }
    buildTag() { return this._buildTag; }
    buildTime() { return this._buildTime; }
    label() { return `Build ${this._buildTag} on ${this._builder.label()}`; }
    url() { return this._builder.urlForBuild(this._buildTag); }
}

if (typeof module != 'undefined') {
    module.exports.Builder = Builder;
    module.exports.Build = Build;
}
