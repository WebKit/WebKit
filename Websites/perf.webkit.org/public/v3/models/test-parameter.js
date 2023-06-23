'use strict';

class TestParameter extends LabeledObject {
    #disabled;
    #type;
    #hasFile;
    #hasValue;
    #description;

    constructor(id, object)
    {
        super(id, object);
        this.#disabled = object.disabled;
        console.assert(['build', 'test'].includes(object.type));
        this.#type = object.type;
        this.#hasFile = !!object.hasFile;
        this.#hasValue = !!object.hasValue;
        this.#description = object.description;

        this.ensureNamedStaticMap('name')[object.name] = this;
    }

    get disabled() { return this.#disabled; }
    get type() { return this.#type; }
    get description() { return this.#description; }
    get hasFile() { return this.#hasFile; }
    get hasValue() { return this.#hasValue; }

    static findByName(name) { return this.namedStaticMap('name')?.[name]; }
}

class TestParameterSet extends DataModelObject {
    #buildTypeParameters;
    #testTypeParameters;
    #testParameterMap;

    constructor(id, object)
    {
        super(id);
        this.#testParameterMap = new Map;
        this.#buildTypeParameters = [];
        this.#testTypeParameters = [];
        if (!object)
            return;
        this._updateFromObject(object)
    }

    updateSingleton(object)
    {
        this.#testParameterMap.clear();
        this.#buildTypeParameters = [];
        this.#testTypeParameters = [];
        this._updateFromObject(object);
    }

    _updateFromObject(object)
    {
        for (const item of object.testParameterItems) {
            console.assert(item.parameter && item.parameter instanceof TestParameter);

            const entry = {};
            if (item.parameter.hasValue)
                entry.value = item.value;
            if (item.parameter.hasFile)
                entry.file = item.file;
            this.#testParameterMap.set(item.parameter, entry);
            (item.parameter.type === 'build' ? this.#buildTypeParameters : this.#testTypeParameters).push(item.parameter);
        }
    }
    get buildTypeParameters() { return this.#buildTypeParameters; }
    get testTypeParameters() { return this.#testTypeParameters; }
    get parameters() { return [...this.#buildTypeParameters, ...this.#testTypeParameters]; }
    asPayload() { return this._createPayloadForParameters(this.parameters);}
    hasSameBuildParametersAs(other) { return this._checkEquals(other, 'buildTypeParameters'); }
    equals(other) { return this._checkEquals(other, 'parameters'); }
    get(parameter) { return this.#testParameterMap.get(parameter); }
    valueForParameter(parameter) { return this.#testParameterMap.get(parameter)?.value; }
    fileForParameter(parameter) { return this.#testParameterMap.get(parameter)?.file; }
    valueForParameterName(parameterName) { return this.valueForParameter(TestParameter.findByName(parameterName)); }
    fileForParameterName(parameterName) { return this.fileForParameter(TestParameter.findByName(parameterName)); }

    _createPayloadForParameters(parameters)
    {
        const payload = {};
        for (const parameter of parameters) {
            if (!this.#testParameterMap.has(parameter))
                continue;
            const entry = {};
            if (parameter.hasValue)
                entry.value = this.valueForParameter(parameter);
            if (parameter.hasFile)
                entry.value = this.fileForParameter(parameter).id()
            payload[parameter.id()] = entry;
        }
        return payload;
    }

    _checkEquals(other, parameterName)
    {
        const thisParameters = this[parameterName];
        if (!other)
            return !thisParameters.length;
        console.assert(other instanceof TestParameterSet);
        const otherParameters = other[parameterName];
        if (thisParameters.length !== otherParameters.length)
            return false;
        return thisParameters.every(parameter => TestParameterSet.isValueEqual(this.get(parameter), other.get(parameter)));
    }

    static isValueEqual(a, b) {
        if (a === b)
            return true;

        if (typeof a !== 'object' || typeof b !== 'object' || a === null || b === null)
            return false;

        const aKeys = Object.keys(a);
        const bKeySet = new Set(Object.keys(b));

        if (aKeys.length !== bKeySet.size)
            return false;

        return aKeys.every(key => bKeySet.has(key) && TestParameterSet.isValueEqual(a[key], b[key]));
    }

    static equals(a, b)
    {
        return a == b || a?.equals(b) || b?.equals(a);
    }

    static hasSameBuildParameters(a, b)
    {
        return a?.hasSameBuildParametersAs(b) || b?.hasSameBuildParametersAs(a) || a == b;
    }
}

class CustomTestParameterSet {
    #buildTypeParameters;
    #testTypeParameters;
    #testParameterMap;

    constructor() { this.reset(); }

    reset()
    {
        this.#testParameterMap = new Map;
        this.#buildTypeParameters = [];
        this.#testTypeParameters = [];
    }

    get buildTypeParameters() { return this.#buildTypeParameters; }
    get testTypeParameters() { return this.#testTypeParameters; }
    get parameters() { return [...this.#buildTypeParameters, ...this.#testTypeParameters]; }
    asPayload() { return this._createPayloadForParameters(this.parameters);}
    valueForParameter(parameter) { return this.#testParameterMap.get(parameter)?.value || null; }
    fileForParameter(parameter) { return this.#testParameterMap.get(parameter)?.file || null; }
    valueForParameterName(parameterName) { return this.valueForParameter(TestParameter.findByName(parameterName)); }
    fileForParameterName(parameterName) { return this.fileForParameter(TestParameter.findByName(parameterName)); }
    set(parameter, entry)
    {
        console.assert(parameter instanceof TestParameter);
        console.assert(!parameter.hasValue || entry.hasOwnProperty('value'));
        console.assert(!parameter.hasFile || entry.hasOwnProperty('file'));
        if (!this.#testParameterMap.has(parameter))
            (parameter.type === 'build' ? this.#buildTypeParameters : this.#testTypeParameters).push(parameter);
        const filteredEntry = {};
        if (parameter.hasValue)
            filteredEntry.value = entry.value;
        if (parameter.hasFile)
            filteredEntry.file = entry.file;
        this.#testParameterMap.set(parameter, filteredEntry);
    }
    setByName(parameterName, entry)
    {
        const parameter = TestParameter.findByName(parameterName);
        console.assert(parameter);
        this.set(parameter, entry);
    }
    get(parameter) { return this.#testParameterMap.get(parameter); }
    getByName(parameterName)
    {
        const parameter = TestParameter.findByName(parameterName);
        console.assert(parameter);
        return this.get(parameter);
    }
    delete(parameter)
    {
        console.assert(parameter instanceof TestParameter);
        if(!this.#testParameterMap.delete(parameter))
            return;
        const parameterList = (parameter.type === 'build' ? this.#buildTypeParameters : this.#testTypeParameters);
        const index = parameterList.indexOf(parameter);
        if (index !== -1)
            parameterList.splice(index, 1);
    }
    _createPayloadForParameters(parameters)
    {
        const payload = {};
        for (const parameter of parameters) {
            if (!this.#testParameterMap.has(parameter))
                continue;
            payload[parameter.id()] = {value: this.valueForParameter(parameter), file: this.fileForParameter(parameter)?.id()};
        }
        return payload;
    }
}

if (typeof module != 'undefined') {
    module.exports.TestParameter = TestParameter;
    module.exports.TestParameterSet = TestParameterSet;
    module.exports.CustomTestParameterSet = CustomTestParameterSet;
}