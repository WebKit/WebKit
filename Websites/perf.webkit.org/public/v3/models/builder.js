
class Builder extends LabeledObject {
    constructor(id, object)
    {
        super(id, object);
        this._buildURL = object.buildUrl;
    }
}
