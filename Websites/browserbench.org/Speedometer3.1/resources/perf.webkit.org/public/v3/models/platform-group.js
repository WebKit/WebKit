'use strict';

class PlatformGroup extends LabeledObject {
    constructor(id, object)
    {
        super(id, object);
        this.ensureNamedStaticMap('name')[object.name] = this;
        this._platforms = new Set;
    }
    addPlatform(platform) {this._platforms.add(platform);}
    platforms() { return Array.from(this._platforms); }
}

if (typeof module != 'undefined')
    module.exports.PlatformGroup = PlatformGroup;
