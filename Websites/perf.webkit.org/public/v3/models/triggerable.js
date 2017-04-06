class Triggerable extends LabeledObject {

    constructor(id, object)
    {
        super(id, object);
        this._name = object.name;
        this._isDisabled = !!object.isDisabled;
        this._acceptedRepositories = object.acceptedRepositories;
        this._repositoryGroups = object.repositoryGroups;
        this._configurationList = object.configurations;
        this._acceptedTests = new Set;

        const configurationMap = this.ensureNamedStaticMap('testConfigurations');
        for (const config of object.configurations) {
            const key = `${config.test.id()}-${config.platform.id()}`;
            this._acceptedTests.add(config.test);
            console.assert(!(key in configurationMap));
            configurationMap[key] = this;
        }
    }

    name() { return this._name; }
    isDisabled() { return this._isDisabled; }
    repositoryGroups() { return this._repositoryGroups; }

    acceptsTest(test) { return this._acceptedTests.has(test); }

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

class TriggerableRepositoryGroup extends LabeledObject {

    constructor(id, object)
    {
        super(id, object);
        this._description = object.description;
        this._acceptsCustomRoots = !!object.acceptsCustomRoots;
        this._repositories = Repository.sortByName(object.repositories);
    }

    accepts(commitSet)
    {
        const commitSetRepositories = Repository.sortByName(commitSet.repositories());
        if (this._repositories.length != commitSetRepositories.length)
            return false;
        for (let i = 0; i < this._repositories.length; i++) {
            if (this._repositories[i] != commitSetRepositories[i])
                return false;
        }
        return true;
    }

    description() { return this._description || this.name(); }
    acceptsCustomRoots() { return this._acceptsCustomRoots; }
    repositories() { return this._repositories; }
}

if (typeof module != 'undefined') {
    module.exports.Triggerable = Triggerable;
    module.exports.TriggerableRepositoryGroup = TriggerableRepositoryGroup;
}

