class LazilyEvaluatedFunction {
    constructor(callback, ...observedPropertiesList)
    {
        console.assert(typeof(callback) == 'function');
        this._callback = callback;
        this._observedPropertiesList = observedPropertiesList;
        this._cachedArguments = null;
        this._cachedResult = undefined;
    }

    evaluate(...args)
    {
        if (this._cachedArguments) {
            const length = this._cachedArguments.length;
            if (args.length == length && (!length || this._cachedArguments.every((cached, i) => cached === args[i])))
                return this._cachedResult;
        }

        this._cachedArguments = args;
        this._cachedResult = this._callback.apply(null, args);

        return this._cachedResult;
    }
}

if (typeof module != 'undefined')
    module.exports.LazilyEvaluatedFunction = LazilyEvaluatedFunction;
