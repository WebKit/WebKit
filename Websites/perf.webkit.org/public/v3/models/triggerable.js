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

    acceptedTests() { return this._acceptedTests; }

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

    static triggerablePlatformsForTests(tests)
    {
        console.assert(tests instanceof Array);
        if (!tests.length)
            return [];
        return this.sortByName(Platform.all().filter((platform) => {
            return tests.every((test) => {
                const triggerable = Triggerable.findByTestConfiguration(test, platform);
                return triggerable && !triggerable.isDisabled();
            });
        }));
    }
}

class TriggerableRepositoryGroup extends LabeledObject {

    constructor(id, object)
    {
        super(id, object);
        this._description = object.description;
        this._isHidden = !!object.hidden;
        this._acceptsCustomRoots = !!object.acceptsCustomRoots;
        this._repositories = Repository.sortByNamePreferringOnesWithURL(object.repositories.map((item) => item.repository));
        this._patchAcceptingSet = new Set(object.repositories.filter((item) => item.acceptsPatch).map((item) => item.repository));
    }

    accepts(commitSet)
    {
        const commitSetRepositories = commitSet.topLevelRepositories();
        if (this._repositories.length != commitSetRepositories.length)
            return false;
        for (let i = 0; i < this._repositories.length; i++) {
            const currentRepository = this._repositories[i];
            if (currentRepository != commitSetRepositories[i])
                return false;
            if (commitSet.patchForRepository(currentRepository) && !this._patchAcceptingSet.has(currentRepository))
                return false;
        }
        if (commitSet.customRoots().length && !this._acceptsCustomRoots)
            return false;
        return true;
    }

    acceptsPatchForRepository(repository)
    {
        return this._patchAcceptingSet.has(repository);
    }

    description() { return this._description || this.name(); }
    isHidden() { return this._isHidden; }
    acceptsCustomRoots() { return this._acceptsCustomRoots; }
    repositories() { return this._repositories; }

    static sortByNamePreferringSmallerRepositories(groupList)
    {
        return groupList.sort((a, b) => {
            if (a.repositories().length < b.repositories().length)
                return -1;
            else if (a.repositories().length > b.repositories().length)
                return 1;
            if (a.name() < b.name())
                return -1;
            else if (a.name() > b.name())
                return 1;
            return 0;
        });
    }
}

if (typeof module != 'undefined') {
    module.exports.Triggerable = Triggerable;
    module.exports.TriggerableRepositoryGroup = TriggerableRepositoryGroup;
}

