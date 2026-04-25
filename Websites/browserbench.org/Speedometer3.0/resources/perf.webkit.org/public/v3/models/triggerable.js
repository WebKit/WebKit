class Triggerable extends LabeledObject {

    constructor(id, object)
    {
        super(id, object);
        this._name = object.name;
        this._isDisabled = !!object.isDisabled;
        this._acceptedRepositories = object.acceptedRepositories;
        this._repositoryGroups = object.repositoryGroups;
        this._acceptedTests = new Set;

        this._triggerableConfigurationList = object.configurations.map(config => {
            this._acceptedTests.add(config.test);
            return TriggerableConfiguration.createForTriggerable(this, config.test, config.platform, config.supportedRepetitionTypes);
        });
    }

    name() { return this._name; }
    isDisabled() { return this._isDisabled; }
    repositoryGroups() { return this._repositoryGroups; }

    acceptedTests() { return this._acceptedTests; }

    static findByTestConfiguration(test, platform) { return TriggerableConfiguration.findByTestAndPlatform(test, platform)?.triggerable; }

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


class TriggerableConfiguration extends DataModelObject {
    #triggerable;
    #test;
    #platform;
    #supportedRepetitionTypes;

    constructor(id, object)
    {
        super(id);
        this.#triggerable = object.triggerable;
        this.#test = object.test;
        this.#platform = object.platform;
        console.assert(object.supportedRepetitionTypes.every(TriggerableConfiguration.isValidRepetitionType.bind(TriggerableConfiguration)));
        this.#supportedRepetitionTypes = object.supportedRepetitionTypes;
    }

    get triggerable() { return this.#triggerable; }
    get supportedRepetitionTypes() { return [...this.#supportedRepetitionTypes]; }
    isSupportedRepetitionType(repetitionType) { return this.#supportedRepetitionTypes.includes(repetitionType); }

    static idForTestAndPlatform(test, platform) { return `${test.id()}-${platform.id()}`; }

    static createForTriggerable(triggerable, test, platform, supportedRepetitionTypes)
    {
        const id = this.idForTestAndPlatform(test, platform);
        return TriggerableConfiguration.ensureSingleton(id, {test, platform, supportedRepetitionTypes, triggerable});
    }

    static findByTestAndPlatform(test, platform)
    {
        for (; test; test = test.parentTest()) {
            const id = this.idForTestAndPlatform(test, platform);
            const triggerableConfiguration = TriggerableConfiguration.findById(id);
            if (triggerableConfiguration)
                return triggerableConfiguration;
        }
        return null;
    }

    static isValidRepetitionType(repetitionType) { return ['alternating', 'sequential', 'paired-parallel'].includes(repetitionType); }
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
    module.exports.TriggerableConfiguration = TriggerableConfiguration;
    module.exports.TriggerableRepositoryGroup = TriggerableRepositoryGroup;
}

