
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
