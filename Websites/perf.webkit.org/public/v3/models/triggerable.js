class Triggerable extends LabeledObject {

    constructor(id, object)
    {
        super(id, object);
        this._name = object.name;
        this._acceptedRepositories = object.acceptedRepositories;
        this._configurationList = object.configurations;

        let configurationMap = this.ensureNamedStaticMap('testConfigurations');
        for (const config of object.configurations) {
            const [testId, platformId] = config;
            const key = `${testId}-${platformId}`;
            console.assert(!(key in configurationMap));
            configurationMap[key] = this;
        }
    }

    acceptedRepositories() { return this._acceptedRepositories; }

    static findByTestConfiguration(test, platform)
    {
        let configurationMap = this.ensureNamedStaticMap('testConfigurations');
        if (!configurationMap)
            return null;
        for (; test; test = test.parentTest()) {
            const key = `${test.id()}-${platform.id()}`;
            if (key in configurationMap)
                return configurationMap[key];
        }
        return null;
    }

}

if (typeof module != 'undefined')
    module.exports.Triggerable = Triggerable;
