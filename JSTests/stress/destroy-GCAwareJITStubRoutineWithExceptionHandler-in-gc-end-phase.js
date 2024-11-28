//@ slow!
//@ runDefault("--useDollarVM=1", "--jitPolicyScale=0.1", "--watchdog=12000", "--watchdog-exception-ok")

// If a sample with this instrumentation crashes, it may need the `fuzzilli` function to reproduce the crash.
if (typeof fuzzilli === 'undefined') fuzzilli = function() {};

const explore = (function() {
    //
    // "Import" the common runtime-assisted mutator code. This will make various utility functions available.
    //
        // Note: runtime instrumentation code must generally assume that any operation performed on any object coming from the "outside", may raise an exception, for example due to triggering a Proxy trap.
    // Further, it must also assume that the environment has been modified arbitrarily. For example, the Array.prototype[@@iterator] may have been set to an invalid value, so using `for...of` syntax could trigger an exception.

    // Load all necessary routines and objects into local variables as they may be overwritten by the program.
    // We generally want to avoid triggerring observable side-effects, such as storing or loading
    // properties. For that reason, we prefer to use builtins like Object.defineProperty.

    const ProxyConstructor = Proxy;
    const BigIntConstructor = BigInt;
    const SetConstructor = Set;

    const ObjectPrototype = Object.prototype;

    const getOwnPropertyNames = Object.getOwnPropertyNames;
    const getPrototypeOf = Object.getPrototypeOf;
    const setPrototypeOf = Object.setPrototypeOf;
    const stringify = JSON.stringify;
    const hasOwnProperty = Object.hasOwn;
    const defineProperty = Object.defineProperty;
    const propertyValues = Object.values;
    const parseInteger = parseInt;
    const NumberIsInteger = Number.isInteger;
    const isNaN = Number.isNaN;
    const isFinite = Number.isFinite;
    const truncate = Math.trunc;
    const apply = Reflect.apply;
    const construct = Reflect.construct;
    const ReflectGet = Reflect.get;
    const ReflectSet = Reflect.set;
    const ReflectHas = Reflect.has;

    // Bind methods to local variables. These all expect the 'this' object as first parameter.
    const concat = Function.prototype.call.bind(Array.prototype.concat);
    const findIndex = Function.prototype.call.bind(Array.prototype.findIndex);
    const includes = Function.prototype.call.bind(Array.prototype.includes);
    const shift = Function.prototype.call.bind(Array.prototype.shift);
    const pop = Function.prototype.call.bind(Array.prototype.pop);
    const push = Function.prototype.call.bind(Array.prototype.push);
    const filter = Function.prototype.call.bind(Array.prototype.filter);
    const execRegExp = Function.prototype.call.bind(RegExp.prototype.exec);
    const stringSlice = Function.prototype.call.bind(String.prototype.slice);
    const toUpperCase = Function.prototype.call.bind(String.prototype.toUpperCase);
    const numberToString = Function.prototype.call.bind(Number.prototype.toString);
    const bigintToString = Function.prototype.call.bind(BigInt.prototype.toString);
    const stringStartsWith = Function.prototype.call.bind(String.prototype.startsWith);
    const setAdd = Function.prototype.call.bind(Set.prototype.add);
    const setHas = Function.prototype.call.bind(Set.prototype.has);

    const MIN_SAFE_INTEGER = Number.MIN_SAFE_INTEGER;
    const MAX_SAFE_INTEGER = Number.MAX_SAFE_INTEGER;

    // Simple, seedable PRNG based on a LCG.
    class RNG {
        m = 2 ** 32;
        a = 1664525;
        c = 1013904223;
        x;

        constructor(seed) {
            this.x = seed;
        }
        randomInt() {
            this.x = (this.x * this.a + this.c) % this.m;
            if (!isInteger(this.x)) throw "RNG state is not an Integer!"
            return this.x;
        }
        randomFloat() {
            return this.randomInt() / this.m;
        }
        probability(p) {
            return this.randomFloat() < p;
        }
        reseed(seed) {
            this.x = seed;
        }
    }

    // When creating empty arrays to which elements are later added, use a custom array type that has a null prototype. This way, the arrays are not
    // affected by changes to the Array.prototype that could interfere with array builtins (e.g. indexed setters or a modified .constructor property).
    function EmptyArray() {
        let array = [];
        setPrototypeOf(array, null);
        return array;
    }

    //
    // Misc. helper functions.
    //
    // Type check helpers. These are less error-prone than manually using typeof and comparing against a string.
    function isObject(v) {
        return typeof v === 'object';
    }
    function isFunction(v) {
        return typeof v === 'function';
    }
    function isString(v) {
        return typeof v === 'string';
    }
    function isNumber(v) {
        return typeof v === 'number';
    }
    function isBigint(v) {
        return typeof v === 'bigint';
    }
    function isSymbol(v) {
        return typeof v === 'symbol';
    }
    function isBoolean(v) {
        return typeof v === 'boolean';
    }
    function isUndefined(v) {
        return typeof v === 'undefined';
    }

    // Helper function to determine if a value is an integer, and within [MIN_SAFE_INTEGER, MAX_SAFE_INTEGER].
    function isInteger(n) {
        return isNumber(n) && NumberIsInteger(n) && n>= MIN_SAFE_INTEGER && n <= MAX_SAFE_INTEGER;
    }

    // Helper function to determine if a string is "simple". We only include simple strings for property/method names or string literals.
    // A simple string is basically a valid, property name with a maximum length.
    const simpleStringRegExp = /^[0-9a-zA-Z_$]+$/;
    function isSimpleString(s) {
        if (!isString(s)) throw "Non-string argument to isSimpleString: " + s;
        return s.length < 50 && execRegExp(simpleStringRegExp, s) !== null;
    }

    // Helper function to determine if a string is numeric and its numeric value representable as an integer.
    function isNumericString(s) {
        if (!isString(s)) return false;
        let number = parseInteger(s);
        return number >= MIN_SAFE_INTEGER && number <= MAX_SAFE_INTEGER && numberToString(number) === s;
    }

    // Helper function to determine whether a property can be accessed without raising an exception.
    function tryAccessProperty(prop, obj) {
        try {
            obj[prop];
            return true;
        } catch (e) {
            return false;
        }
    }

    // Helper function to determine if a property exists on an object or one of its prototypes. If an exception is raised, false is returned.
    function tryHasProperty(prop, obj) {
        try {
            return prop in obj;
        } catch (e) {
            return false;
        }
    }

    // Helper function to load a property from an object. If an exception is raised, undefined is returned.
    function tryGetProperty(prop, obj) {
        try {
            return obj[prop];
        } catch (e) {
            return undefined;
        }
    }

    // Helper function to obtain the own properties of an object. If that raises an exception (e.g. on a Proxy object), an empty array is returned.
    function tryGetOwnPropertyNames(obj) {
        try {
            return getOwnPropertyNames(obj);
        } catch (e) {
            return new Array();
        }
    }

    // Helper function to fetch the prototype of an object. If that raises an exception (e.g. on a Proxy object), null is returned.
    function tryGetPrototypeOf(obj) {
        try {
            return getPrototypeOf(obj);
        } catch (e) {
            return null;
        }
    }

    // Helper function to that creates a wrapper function for the given function which will call it in a try-catch and return false on exception.
    function wrapInTryCatch(f) {
        return function() {
            try {
                return apply(f, this, arguments);
            } catch (e) {
                return false;
            }
        };
    }

    //
    // Basic random number generation utility functions.
    //

    // Initially the rng is seeded randomly, specific mutators can reseed() the rng if they need deterministic behavior.
    // See the explore operation in JsOperations.swift for an example.
    let rng = new RNG(truncate(Math.random() * 2**32));

    function probability(p) {
        if (p < 0 || p > 1) throw "Argument to probability must be a number between zero and one";
        return rng.probability(p);
    }

    function randomIntBetween(start, end) {
        if (!isInteger(start) || !isInteger(end)) throw "Arguments to randomIntBetween must be integers";
        return (rng.randomInt() % (end - start)) + start;
    }

    function randomFloat() {
        return rng.randomFloat();
    }

    function randomBigintBetween(start, end) {
        if (!isBigint(start) || !isBigint(end)) throw "Arguments to randomBigintBetween must be bigints";
        if (!isInteger(Number(start)) || !isInteger(Number(end))) throw "Arguments to randomBigintBetween must be representable as regular intergers";
        return BigIntConstructor(randomIntBetween(Number(start), Number(end)));
    }

    function randomIntBelow(n) {
        if (!isInteger(n)) throw "Argument to randomIntBelow must be an integer";
        return rng.randomInt() % n;
    }

    function randomElement(array) {
        return array[randomIntBelow(array.length)];
    }


    //
    // "Import" the object introspection code. This is used to find properties and methods when exploring an object.
    //
        // Note: this code assumes that the common code from above has been included before.

    //
    // Object introspection.
    //

    //
    // Enumerate all properties on the given object and its prototypes.
    //
    // These include elements and methods (callable properties). Each property is assigned a "weight" which expresses roughly how "important"/"interesting" a property is:
    // a property that exists on a prototype is considered less interesting as it can probably be reached through other objects as well, so we prefer properties closer to the start object.
    //
    // The result is returned in an array like the following:
    // [
    //   length: <total number of properties>
    //   totalWeight: <combined weight of all properties>
    //   0, 1, ..., length - 1: { name: <property name/index>, weight: <weight as number> }
    // ]
    function enumeratePropertiesOf(o) {
        // Collect all properties, including those from prototypes, in this array.
        let properties = EmptyArray();

        // Give properties from prototypes a lower weight. Currently, the weight is halved for every new level of the prototype chain.
        let currentWeight = 1.0;
        properties.totalWeight = 0.0;
        function recordProperty(p) {
            push(properties, {name: p, weight: currentWeight});
            properties.totalWeight += currentWeight;
        }

        // Iterate over the prototype chain and record all properties.
        let obj = o;
        while (obj !== null) {
            // Special handling for array-like things: if the array is too large, skip this level and just include a couple random indices.
            // We need to be careful when accessing the length though: for TypedArrays, the length property is defined on the prototype but
            // must be accessed with the TypedArray as |this| value (not the prototype).
            let maybeLength = tryGetProperty('length', obj);
            if (isInteger(maybeLength) && maybeLength > 100) {
                for (let i = 0; i < 10; i++) {
                    let randomElement = randomIntBelow(maybeLength);
                    recordProperty(randomElement);
                }
            } else {
                // TODO do we want to also enumerate symbol properties here (using Object.getOwnPropertySymbols)? If we do, we should probable also add IL-level support for Symbols (i.e. a `LoadSymbol` instruction that ensures the symbol is in the global Symbol registry).
                let allOwnPropertyNames = tryGetOwnPropertyNames(obj);
                let allOwnElements = EmptyArray();
                for (let i = 0; i < allOwnPropertyNames.length; i++) {
                    let p = allOwnPropertyNames[i];
                    let index = parseInteger(p);
                    // TODO should we allow negative indices here as well?
                    if (index >= 0 && index <= MAX_SAFE_INTEGER && numberToString(index) === p) {
                        push(allOwnElements, index);
                    } else if (isSimpleString(p) && tryAccessProperty(p, o)) {
                        // Only include properties with "simple" names and only if they can be accessed on the original object without raising an exception.
                        recordProperty(p);
                    }
                }

                // Limit array-like objects to at most 10 random elements.
                for (let i = 0; i < 10 && allOwnElements.length > 0; i++) {
                    let index = randomIntBelow(allOwnElements.length);
                    recordProperty(allOwnElements[index]);
                    allOwnElements[index] = pop(allOwnElements);
                }
            }

            obj = tryGetPrototypeOf(obj);
            currentWeight /= 2.0;

            // Greatly reduce the property weights for the Object.prototype. These methods are always available and we can use more targeted mechanisms like CodeGenerators to call them if we want to.
            if (obj === ObjectPrototype) {
                // Somewhat arbitrarily reduce the weight as if there were another 3 levels.
                currentWeight /= 8.0;

                // However, if we've reached the Object prototype without any other properties (i.e. are inspecting an empty, plain object), then always return an empty list since these properties are not very intersting.
                if (properties.length == 0) {
                    return properties;
                }
            }
        }

        return properties;
    }

    //
    // Returns a random property available on the given object or one of its prototypes.
    //
    // This will return more "interesting" properties with a higher probability. In general, properties closer to the start object
    // are preferred over properties on prototypes (as these are likely shared with other objects).
    //
    // If no (interesting) property exists, null is returned. Otherwise, the key of the property is returned.
    function randomPropertyOf(o) {
        let properties = enumeratePropertiesOf(o);

        // We need at least one property to chose from.
        if (properties.length === 0) {
            return null;
        }

        // Now choose a random property. If a property has weight 2W, it will be selected with twice the probability of a property with weight W.
        let selectedProperty;
        let remainingWeight = randomFloat() * properties.totalWeight;
        for (let i = 0; i < properties.length; i++) {
            let candidate = properties[i];
            remainingWeight -= candidate.weight;
            if (remainingWeight < 0) {
                selectedProperty = candidate.name;
                break;
            }
        }

        // Sanity checking. This may fail for example if Proxies are involved. In that case, just fail here.
        if (!tryHasProperty(selectedProperty, o)) return null;

        return selectedProperty;
    }


    //
    // "Import" the Action implementation code.
    //
        // Note: this code assumes that the common code from above has been included before.

    //
    // List of all supported operations. Must be kept in sync with the ActionOperation enum.
    //
    const OP_CALL_FUNCTION = 'CALL_FUNCTION';
    const OP_CONSTRUCT = 'CONSTRUCT';
    const OP_CALL_METHOD = 'CALL_METHOD';
    const OP_CONSTRUCT_METHOD = 'CONSTRUCT_METHOD';
    const OP_GET_PROPERTY = 'GET_PROPERTY';
    const OP_SET_PROPERTY = 'SET_PROPERTY';
    const OP_DELETE_PROPERTY = 'DELETE_PROPERTY';

    const OP_ADD = 'ADD';
    const OP_SUB = 'SUB';
    const OP_MUL = 'MUL';
    const OP_DIV = 'DIV';
    const OP_MOD = 'MOD';
    const OP_INC = 'INC';
    const OP_DEC = 'DEC';
    const OP_NEG = 'NEG';

    const OP_LOGICAL_AND = 'LOGICAL_AND';
    const OP_LOGICAL_OR = 'LOGICAL_OR';
    const OP_LOGICAL_NOT = 'LOGICAL_NOT';

    const OP_BITWISE_AND = 'BITWISE_AND';
    const OP_BITWISE_OR = 'BITWISE_OR';
    const OP_BITWISE_XOR = 'BITWISE_XOR';
    const OP_LEFT_SHIFT = 'LEFT_SHIFT';
    const OP_SIGNED_RIGHT_SHIFT = 'SIGNED_RIGHT_SHIFT';
    const OP_UNSIGNED_RIGHT_SHIFT = 'UNSIGNED_RIGHT_SHIFT';
    const OP_BITWISE_NOT = 'BITWISE_NOT';

    const OP_COMPARE_EQUAL = 'COMPARE_EQUAL';
    const OP_COMPARE_STRICT_EQUAL = 'COMPARE_STRICT_EQUAL';
    const OP_COMPARE_NOT_EQUAL = 'COMPARE_NOT_EQUAL';
    const OP_COMPARE_STRICT_NOT_EQUAL = 'COMPARE_STRICT_NOT_EQUAL';
    const OP_COMPARE_GREATER_THAN = 'COMPARE_GREATER_THAN';
    const OP_COMPARE_LESS_THAN = 'COMPARE_LESS_THAN';
    const OP_COMPARE_GREATER_THAN_OR_EQUAL = 'COMPARE_GREATER_THAN_OR_EQUAL';
    const OP_COMPARE_LESS_THAN_OR_EQUAL = 'COMPARE_LESS_THAN_OR_EQUAL';
    const OP_TEST_IS_NAN = 'TEST_IS_NAN';
    const OP_TEST_IS_FINITE = 'TEST_IS_FINITE';

    const OP_SYMBOL_REGISTRATION = 'SYMBOL_REGISTRATION';

    //
    // Action constructors.
    //
    function Action(operation, inputs = EmptyArray()) {
        this.operation = operation;
        this.inputs = inputs;
        this.isGuarded = false;
    }

    // A guarded action is an action that is allowed to raise an exception.
    //
    // These are for example used for by the ExplorationMutator for function/method call
    // which may throw an exception if they aren't given the right arguments. In that case,
    // we may still want to keep the function call so that it can be mutated further to
    // hopefully eventually find the correct arguments. This is especially true if finding
    // the right arguments reqires the ProbingMutator to install the right properties on an
    // argument object, in which case the ExplorationMutator on its own would (likely) never
    // be able to generate a valid call, and so the function/method may be missed entirely.
    //
    // If a guarded action succeeds (doesn't raise an exception), it will be converted to
    // a regular action to limit the number of generated try-catch blocks.
    function GuardedAction(operation, inputs = EmptyArray()) {
        this.operation = operation;
        this.inputs = inputs;
        this.isGuarded = true;
    }

    // Special value to indicate that no action should be performed.
    const NO_ACTION = null;

    //
    // Action Input constructors.
    //
    // The inputs for actions are encoded as objects that specify both the type and the value of the input. They are basically enum values with associated values.
    // These must be kept compatible with the Action.Input enum in RuntimeAssistedMutator.swift as they have to be encodable to/decodable from that enum.
    //
    function ArgumentInput(index) {
        if (!isInteger(index)) throw "ArgumentInput index is not an integer: " + index;
        return { argument: { index } };
    }
    function SpecialInput(name) {
        if (!isString(name) || !isSimpleString(name)) throw "SpecialInput name is not a (simple) string: " + name;
        return { special: { name } };
    }
    function IntInput(value) {
        if (!isInteger(value)) throw "IntInput value is not an integer: " + value;
        return { int: { value } };
    }
    function FloatInput(value) {
        if (!isNumber(value) || !isFinite(value)) throw "FloatInput value is not a (finite) number: " + value;
        return { float: { value } };
    }
    function BigintInput(value) {
        if (!isBigint(value)) throw "BigintInput value is not a bigint: " + value;
        // Bigints can't be serialized by JSON.stringify, so store them as strings instead.
        return { bigint: { value: bigintToString(value) } };
    }
    function StringInput(value) {
        if (!isString(value) || !isSimpleString(value)) throw "StringInput value is not a (simple) string: " + value;
        return { string: { value } };
    }

    // Type checkers for Input objects. We use these instead of for example 'instanceof' since we allow Input
    // objects to be decoded from JSON, in which case they will not have the right .constructor property.
    function isArgumentInput(input) { return hasOwnProperty(input, 'argument'); }
    function isSpecialInput(input) { return hasOwnProperty(input, 'special'); }
    function isIntInput(input) { return hasOwnProperty(input, 'int'); }
    function isFloatInput(input) { return hasOwnProperty(input, 'float'); }
    function isBigintInput(input) { return hasOwnProperty(input, 'bigint'); }
    function isStringInput(input) { return hasOwnProperty(input, 'string'); }

    // Helper routines to extract the associated values from Input objects.
    function getArgumentInputIndex(input) { return input.argument.index; }
    function getSpecialInputName(input) { return input.special.name; }
    function getIntInputValue(input) { return input.int.value; }
    function getFloatInputValue(input) { return input.float.value; }
    function getBigintInputValue(input) { return BigIntConstructor(input.bigint.value); }
    function getStringInputValue(input) { return input.string.value; }

    // Handlers for executing actions.
    // These will receive the array of concrete inputs (i.e. JavaScript values) as first parameter and the current value of |this| as second parameter (which can be ignored if not needed).
    const ACTION_HANDLERS = {
      [OP_CALL_FUNCTION]: (inputs, currentThis) => { let f = shift(inputs); return apply(f, currentThis, inputs); },
      [OP_CONSTRUCT]: (inputs) => { let c = shift(inputs); return construct(c, inputs); },
      [OP_CALL_METHOD]: (inputs) => { let o = shift(inputs); let m = shift(inputs); return apply(o[m], o, inputs); },
      [OP_CONSTRUCT_METHOD]: (v, inputs) => { let o = shift(inputs); let m = shift(inputs); return construct(o[m], inputs); },
      [OP_GET_PROPERTY]: (inputs) => { let o = inputs[0]; let p = inputs[1]; return o[p]; },
      [OP_SET_PROPERTY]: (inputs) => { let o = inputs[0]; let p = inputs[1]; let v = inputs[2]; o[p] = v; },
      [OP_DELETE_PROPERTY]: (inputs) => { let o = inputs[0]; let p = inputs[1]; return delete o[p]; },
      [OP_ADD]: (inputs) => inputs[0] + inputs[1],
      [OP_SUB]: (inputs) => inputs[0] - inputs[1],
      [OP_MUL]: (inputs) => inputs[0] * inputs[1],
      [OP_DIV]: (inputs) => inputs[0] / inputs[1],
      [OP_MOD]: (inputs) => inputs[0] % inputs[1],
      [OP_INC]: (inputs) => inputs[0]++,
      [OP_DEC]: (inputs) => inputs[0]--,
      [OP_NEG]: (inputs) => -inputs[0],
      [OP_LOGICAL_AND]: (inputs) => inputs[0] && inputs[1],
      [OP_LOGICAL_OR]: (inputs) => inputs[0] || inputs[1],
      [OP_LOGICAL_NOT]: (inputs) => !inputs[0],
      [OP_BITWISE_AND]: (inputs) => inputs[0] & inputs[1],
      [OP_BITWISE_OR]: (inputs) => inputs[0] | inputs[1],
      [OP_BITWISE_XOR]: (inputs) => inputs[0] ^ inputs[1],
      [OP_LEFT_SHIFT]: (inputs) => inputs[0] << inputs[1],
      [OP_SIGNED_RIGHT_SHIFT]: (inputs) => inputs[0] >> inputs[1],
      [OP_UNSIGNED_RIGHT_SHIFT]: (inputs) => inputs[0] >>> inputs[1],
      [OP_BITWISE_NOT]: (inputs) => ~inputs[0],
      [OP_COMPARE_EQUAL]: (inputs) => inputs[0] == inputs[1],
      [OP_COMPARE_STRICT_EQUAL]: (inputs) => inputs[0] === inputs[1],
      [OP_COMPARE_NOT_EQUAL]: (inputs) => inputs[0] != inputs[1],
      [OP_COMPARE_STRICT_NOT_EQUAL]: (inputs) => inputs[0] !== inputs[1],
      [OP_COMPARE_GREATER_THAN]: (inputs) => inputs[0] > inputs[1],
      [OP_COMPARE_LESS_THAN]: (inputs) => inputs[0] < inputs[1],
      [OP_COMPARE_GREATER_THAN_OR_EQUAL]: (inputs) => inputs[0] >= inputs[1],
      [OP_COMPARE_LESS_THAN_OR_EQUAL]: (inputs) => inputs[0] <= inputs[1],
      [OP_TEST_IS_NAN]: (inputs) => Number.isNaN(inputs[0]),
      [OP_TEST_IS_FINITE]: (inputs) => Number.isFinite(inputs[0]),
      [OP_SYMBOL_REGISTRATION]: (inputs) => Symbol.for(inputs[0].description),
    };

    // Executes the given action.
    //
    // This will convert the inputs to concrete JavaScript values, then execute the operation with these inputs.
    // Executing an action may change its guarding state: if a guarded action executes without raising an exception,
    // it will be converted to an unguarded operation (as the guarding apears to not be needed). This way, we will
    // ultimately end up emitting fewer try-catch (or equivalent) constructs in the final JavaScript code generated
    // from these actions.
    //
    // Returns true if either the action succeeded without raising an exception or if the action is guarded, false otherwise.
    // The output of the action is stored in |context.output| upon successful execution.
    function execute(action, context) {
        if (action === NO_ACTION) {
            return true;
        }

        // Convert the action's inputs to the concrete JS values to use for executing the action.
        let concreteInputs = EmptyArray();
        for (let i = 0; i < action.inputs.length; i++) {
            let input = action.inputs[i];
            if (isArgumentInput(input)) {
                let index = getArgumentInputIndex(input);
                if (index >= context.arguments.length) throw "Invalid argument index: " + index;
                push(concreteInputs, context.arguments[index]);
            } else if (isSpecialInput(input)) {
                let name = getSpecialInputName(input);
                if (!hasOwnProperty(context.specialValues, name)) throw "Unknown special value: " + name;
                push(concreteInputs, context.specialValues[name]);
            } else if (isIntInput(input)) {
                push(concreteInputs, getIntInputValue(input));
            } else if (isFloatInput(input)) {
                push(concreteInputs, getFloatInputValue(input));
            } else if (isBigintInput(input)) {
                // These need special handling because BigInts cannot be serialized into JSON, so are stored as strings.
                push(concreteInputs, getBigintInputValue(input));
            } else if (isStringInput(input)) {
                push(concreteInputs, getStringInputValue(input));
            } else {
                throw "Unknown action input: " + stringify(input);
            }
        }

        let handler = ACTION_HANDLERS[action.operation];
        if (isUndefined(handler)) throw "Unhandled operation " + action.operation;

        try {
            context.output = handler(concreteInputs, context.currentThis);
            // If the action succeeded, mark it as non-guarded so that we don't emit try-catch blocks for it later on.
            // We could alternatively only do that if all executions succeeded, but it's probably fine to do it if at least one execution succeeded.
            if (action.isGuarded) action.isGuarded = false;
        } catch (e) {
            return action.isGuarded;
        }

        return true;
    }

    // JS Action Operation groups. See e.g. exploreNumber() for examples of how they are used.
    const SHIFT_OPS = [OP_LEFT_SHIFT, OP_SIGNED_RIGHT_SHIFT, OP_UNSIGNED_RIGHT_SHIFT];
    // Unsigned shift is not defined for bigints.
    const BIGINT_SHIFT_OPS = [OP_LEFT_SHIFT, OP_SIGNED_RIGHT_SHIFT];
    const BITWISE_OPS = [OP_BITWISE_OR, OP_BITWISE_AND, OP_BITWISE_XOR];
    const ARITHMETIC_OPS = [OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD];
    const UNARY_OPS = [OP_INC, OP_DEC, OP_NEG, OP_BITWISE_NOT];
    const COMPARISON_OPS = [OP_COMPARE_EQUAL, OP_COMPARE_STRICT_EQUAL, OP_COMPARE_NOT_EQUAL, OP_COMPARE_STRICT_NOT_EQUAL, OP_COMPARE_GREATER_THAN, OP_COMPARE_LESS_THAN, OP_COMPARE_GREATER_THAN_OR_EQUAL, OP_COMPARE_LESS_THAN_OR_EQUAL];
    const BOOLEAN_BINARY_OPS = [OP_LOGICAL_AND, OP_LOGICAL_OR];
    const BOOLEAN_UNARY_OPS = [OP_LOGICAL_NOT];


    //
    // Global constants.
    //
    // Property names to use when defining new properties. Should be kept in sync with the equivalent set in JavaScriptEnvironment.swift
    const customPropertyNames = ["a", "b", "c", "d", "e", "f", "g", "h"];

    // Maximum number of parameters for function/method calls. Everything above this is consiered an invalid .length property of the function.
    const MAX_PARAMETERS = 10;

    // Well known integer/number values to use when generating random values.
    const WELL_KNOWN_INTEGERS = filter([-9223372036854775808, -9223372036854775807, -9007199254740992, -9007199254740991, -9007199254740990, -4294967297, -4294967296, -4294967295, -2147483649, -2147483648, -2147483647, -1073741824, -536870912, -268435456, -65537, -65536, -65535, -4096, -1024, -256, -128, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 64, 127, 128, 129, 255, 256, 257, 512, 1000, 1024, 4096, 10000, 65535, 65536, 65537, 268435439, 268435440, 268435441, 536870887, 536870888, 536870889, 268435456, 536870912, 1073741824, 1073741823, 1073741824, 1073741825, 2147483647, 2147483648, 2147483649, 4294967295, 4294967296, 4294967297, 9007199254740990, 9007199254740991, 9007199254740992, 9223372036854775807], isInteger);
    const WELL_KNOWN_NUMBERS = concat(WELL_KNOWN_INTEGERS, [-1e6, -1e3, -5.0, -4.0, -3.0, -2.0, -1.0, -0.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 1e3, 1e6]);
    const WELL_KNOWN_BIGINTS = [-9223372036854775808n, -9223372036854775807n, -9007199254740992n, -9007199254740991n, -9007199254740990n, -4294967297n, -4294967296n, -4294967295n, -2147483649n, -2147483648n, -2147483647n, -1073741824n, -536870912n, -268435456n, -65537n, -65536n, -65535n, -4096n, -1024n, -256n, -128n, -2n, -1n, 0n, 1n, 2n, 3n, 4n, 5n, 6n, 7n, 8n, 9n, 10n, 16n, 64n, 127n, 128n, 129n, 255n, 256n, 257n, 512n, 1000n, 1024n, 4096n, 10000n, 65535n, 65536n, 65537n, 268435439n, 268435440n, 268435441n, 536870887n, 536870888n, 536870889n, 268435456n, 536870912n, 1073741824n, 1073741823n, 1073741824n, 1073741825n, 2147483647n, 2147483648n, 2147483649n, 4294967295n, 4294967296n, 4294967297n, 9007199254740990n, 9007199254740991n, 9007199254740992n, 9223372036854775807n];


    //
    // Global state.
    //

    // The concrete argument values that will be used when executing the Action for the current exploration operation.
    let exploreArguments;

    // The input containing the value being explored. This should always be the first input to every Action created during exploration.
    let exploredValueInput;

    // Whether exploration is currently happening. This is required to detect recursive exploration, where for example a callback
    // triggered during property enumeration triggers further exploration calls. See explore().
    let currentlyExploring = false;


    //
    // Error and result reporting.
    //
    // Results (indexed by their ID) will be stored in here.
    const results = { __proto__: null };

    function reportError(msg) {
        fuzzilli('FUZZILLI_PRINT', 'EXPLORE_ERROR: ' + msg);
    }

    function recordFailure(id) {
        // Delete the property if it already exists (from recordAction).
        delete results[id];
        defineProperty(results, id, {__proto__: null, value: NO_ACTION});

        fuzzilli('FUZZILLI_PRINT', 'EXPLORE_FAILURE: ' + id);
    }

    function recordAction(id, action) {
        if (hasOwnProperty(results, id)) {
            throw "Duplicate action for " + id;
        }

        if (action === NO_ACTION) {
            // This is equivalent to a failure.
            return recordFailure(id);
        }

        action.id = id;

        // These are configurable as they may need to be overwritten (by recordFailure) in the future.
        defineProperty(results, id, {__proto__: null, value: action, configurable: true});

        fuzzilli('FUZZILLI_PRINT', 'EXPLORE_ACTION: ' + stringify(action));
    }

    function hasActionFor(id) {
        return hasOwnProperty(results, id);
    }

    function getActionFor(id) {
        return results[id];
    }

    //
    // Access to random inputs.
    //
    // These functions prefer to take an existing variable from the arguments to the Explore operations if it satistifes the specified criteria. Otherwise, they generate a new random value.
    // Testing whether an argument satisfies some criteria (e.g. be a value in a certain range) may trigger type conversions. This is expected as it may lead to interesting values being used.
    // These are grouped into a namespace object to make it more clear that they return an Input object (from one of the above constructors), rather than a value.
    //
    let Inputs = {
        randomArgument() {
            return new ArgumentInput(randomIntBelow(exploreArguments.length));
        },

        randomArguments(n) {
            let args = EmptyArray();
            for (let i = 0; i < n; i++) {
                push(args, new ArgumentInput(randomIntBelow(exploreArguments.length)));
            }
            return args;
        },

        randomArgumentForReplacing(propertyName, obj) {
            let curValue = tryGetProperty(propertyName, obj);
            if (isUndefined(curValue)) {
                return Inputs.randomArgument();
            }

            function isCompatible(arg) {
                let sameType = typeof curValue === typeof arg;
                if (sameType && isObject(curValue)) {
                    sameType = arg instanceof curValue.constructor;
                }
                return sameType;
            }

            let idx = findIndex(exploreArguments, wrapInTryCatch(isCompatible));
            if (idx != -1) return new ArgumentInput(idx);
            return Inputs.randomArgument();
        },

        randomInt() {
            let idx = findIndex(exploreArguments, isInteger);
            if (idx != -1) return new ArgumentInput(idx);
            return new IntInput(randomElement(WELL_KNOWN_INTEGERS));
        },

        randomNumber() {
            let idx = findIndex(exploreArguments, isNumber);
            if (idx != -1) return new ArgumentInput(idx);
            return new FloatInput(randomElement(WELL_KNOWN_NUMBERS));
        },

        randomBigint() {
            let idx = findIndex(exploreArguments, isBigint);
            if (idx != -1) return new ArgumentInput(idx);
            return new BigintInput(randomElement(WELL_KNOWN_BIGINTS));
        },

        randomIntBetween(start, end) {
            if (!isInteger(start) || !isInteger(end)) throw "Arguments to randomIntBetween must be integers";
            let idx = findIndex(exploreArguments, wrapInTryCatch((e) => NumberIsInteger(e) && (e >= start) && (e < end)));
            if (idx != -1) return new ArgumentInput(idx);
            return new IntInput(randomIntBetween(start, end));
        },

        randomBigintBetween(start, end) {
            if (!isBigint(start) || !isBigint(end)) throw "Arguments to randomBigintBetween must be bigints";
            if (!isInteger(Number(start)) || !isInteger(Number(end))) throw "Arguments to randomBigintBetween must be representable as regular integers";
            let idx = findIndex(exploreArguments, wrapInTryCatch((e) => (e >= start) && (e < end)));
            if (idx != -1) return new ArgumentInput(idx);
            return new BigintInput(randomBigintBetween(start, end));
        },

        randomNumberCloseTo(v) {
            if (!isFinite(v)) throw "Argument to randomNumberCloseTo is not a finite number: " + v;
            let idx = findIndex(exploreArguments, wrapInTryCatch((e) => (e >= v - 10) && (e <= v + 10)));
            if (idx != -1) return new ArgumentInput(idx);
            let step = randomIntBetween(-10, 10);
            let value = v + step;
            if (isInteger(value)) {
              return new IntInput(value);
            } else {
              return new FloatInput(v + step);
            }
        },

        randomBigintCloseTo(v) {
            if (!isBigint(v)) throw "Argument to randomBigintCloseTo is not a bigint: " + v;
            let idx = findIndex(exploreArguments, wrapInTryCatch((e) => (e >= v - 10n) && (e <= v + 10n)));
            if (idx != -1) return new ArgumentInput(idx);
            let step = randomBigintBetween(-10n, 10n);
            let value = v + step;
            return new BigintInput(value);
        }
    }

    // Heuristic to determine when a function should be invoked as a constructor. Used by the object and function exploration code.
    function shouldTreatAsConstructor(f) {
        let name = tryGetProperty('name', f);

        // If the function has no name (or a non-string name), it's probably a regular function (or something like an arrow function).
        if (!isString(name) || name.length < 1) {
            return probability(0.1);
        }

        // If the name is something like `f42`, it's probably a function defined by Fuzzilli. These can typicall be used as function or constructor, but prefer to call them as regular functions.
        if (name[0] === 'f' && !isNaN(parseInteger(stringSlice(name, 1)))) {
          return probability(0.2);
        }

        // Otherwise, the basic heuristic is that functions that start with an uppercase letter (e.g. Object, Uint8Array, etc.) are probably constructors (but not always, e.g. BigInt).
        // This is also compatible with Fuzzilli's lifting of classes, as they will look something like this: `class V3 { ...`.
        if (name[0] === toUpperCase(name[0])) {
          return probability(0.9);
        } else {
          return probability(0.1);
        }
    }

    //
    // Explore implementation for different basic types.
    //
    // These all return an Action object or the special NO_ACTION value (null).
    //
    function exploreObject(o) {
        if (o === null) {
            // Can't do anything with null.
            return NO_ACTION;
        }

        // TODO: Add special handling for ArrayBuffers: most of the time, wrap these into a Uint8Array to be able to modify them.
        // TODO: Sometimes iterate over iterable objects (e.g. Arrays)?

        // Determine a random property, which can generally either be a method, an element, or a "regular" property.
        let propertyName = randomPropertyOf(o);

        // Determine the appropriate action to perform given the selected property.
        // If the property lookup failed (for whatever reason), we always define a new property.
        if (propertyName === null) {
            let propertyNameInput = new StringInput(randomElement(customPropertyNames));
            return new Action(OP_SET_PROPERTY, [exploredValueInput, propertyNameInput, Inputs.randomArgument()]);
        } else if (isInteger(propertyName)) {
            let propertyNameInput = new IntInput(propertyName);
            if (probability(0.5)) {
                return new Action(OP_GET_PROPERTY, [exploredValueInput, propertyNameInput]);
            } else {
                let newValue = Inputs.randomArgumentForReplacing(propertyName, o);
                return new Action(OP_SET_PROPERTY, [exploredValueInput, propertyNameInput, newValue]);
            }
        } else if (isString(propertyName)) {
            let propertyNameInput = new StringInput(propertyName);
            let propertyValue = tryGetProperty(propertyName, o);
            if (isFunction(propertyValue)) {
                // Perform a method call/construct.
                let numParameters = tryGetProperty('length', propertyValue);
                if (!isInteger(numParameters) || numParameters > MAX_PARAMETERS || numParameters < 0) return NO_ACTION;
                let inputs = EmptyArray();
                push(inputs, exploredValueInput);
                push(inputs, propertyNameInput);
                for (let i = 0; i < numParameters; i++) {
                    push(inputs, Inputs.randomArgument());
                }
                if (shouldTreatAsConstructor(propertyValue)) {
                  return new GuardedAction(OP_CONSTRUCT_METHOD, inputs);
                } else {
                  return new GuardedAction(OP_CALL_METHOD, inputs);
                }
            } else {
                // Perform a property access.
                // Besides getting and setting the property, we also sometimes define a new property instead.
                if (probability(1/3)) {
                    propertyNameInput = new StringInput(randomElement(customPropertyNames));
                    return new Action(OP_SET_PROPERTY, [exploredValueInput, propertyNameInput, Inputs.randomArgument()]);
                } else if (probability(0.5)) {
                    return new Action(OP_GET_PROPERTY, [exploredValueInput, propertyNameInput]);
                } else {
                    let newValue = Inputs.randomArgumentForReplacing(propertyName, o);
                    return new Action(OP_SET_PROPERTY, [exploredValueInput, propertyNameInput, newValue]);
                }
            }
        } else {
          throw "Got unexpected property name from Inputs.randomPropertyOf(): " + propertyName;
        }
    }

    function exploreFunction(f) {
        // Sometimes treat functions as objects.
        // This will cause interesting properties like 'arguments' or 'prototype' to be accessed, methods like 'apply' or 'bind' to be called, and methods on builtin constructors like 'Array', and 'Object' to be used.
        if (probability(0.5)) {
            return exploreObject(f);
        }

        // Otherwise, call or construct the function/constructor.
        let numParameters = tryGetProperty('length', f);
        if (!isInteger(numParameters) || numParameters > MAX_PARAMETERS || numParameters < 0) {
            numParameters = 0;
        }
        let inputs = EmptyArray();
        push(inputs, exploredValueInput);
        for (let i = 0; i < numParameters; i++) {
            push(inputs, Inputs.randomArgument());
        }
        let operation = shouldTreatAsConstructor(f) ? OP_CONSTRUCT : OP_CALL_FUNCTION;
        return new GuardedAction(operation, inputs);
    }

    function exploreString(s) {
        // Sometimes (rarely) compare the string against it's original value. Otherwise, treat the string as an object.
        // TODO: sometimes access a character of the string or iterate over it?
        if (probability(0.1) && isSimpleString(s)) {
            return new Action(OP_COMPARE_EQUAL, [exploredValueInput, new StringInput(s)]);
        } else {
            return exploreObject(new String(s));
        }
    }

    const ALL_NUMBER_OPERATIONS = concat(SHIFT_OPS, BITWISE_OPS, ARITHMETIC_OPS, UNARY_OPS);
    const ALL_NUMBER_OPERATIONS_AND_COMPARISONS = concat(ALL_NUMBER_OPERATIONS, COMPARISON_OPS);
    function exploreNumber(n) {
        // Somewhat arbitrarily give comparisons a lower probability when choosing the operation to perform.
        let operation = randomElement(probability(0.5) ? ALL_NUMBER_OPERATIONS : ALL_NUMBER_OPERATIONS_AND_COMPARISONS);

        let action = new Action(operation);
        push(action.inputs, exploredValueInput);
        if (includes(COMPARISON_OPS, operation)) {
            if (isNaN(n)) {
                // In that case, regular comparisons don't make sense, so just test for isNaN instead.
                action.operation = OP_TEST_IS_NAN;
            } else if (!isFinite(n)) {
                // Similar to the NaN case, just test for isFinite here.
                action.operation = OP_TEST_IS_FINITE;
            } else {
                push(action.inputs, Inputs.randomNumberCloseTo(n));
            }
        } else if (includes(SHIFT_OPS, operation)) {
            push(action.inputs, Inputs.randomIntBetween(1, 32));
        } else if (includes(BITWISE_OPS, operation)) {
            push(action.inputs, Inputs.randomInt());
        } else if (includes(ARITHMETIC_OPS, operation)) {
            if (isInteger(n)) {
                push(action.inputs, Inputs.randomInt());
            } else {
                push(action.inputs, Inputs.randomNumber());
            }
        }
        return action;
    }

    const ALL_BIGINT_OPERATIONS = concat(BIGINT_SHIFT_OPS, BITWISE_OPS, ARITHMETIC_OPS, UNARY_OPS);
    const ALL_BIGINT_OPERATIONS_AND_COMPARISONS = concat(ALL_BIGINT_OPERATIONS, COMPARISON_OPS);
    function exploreBigint(b) {
        // Somewhat arbitrarily give comparisons a lower probability when choosing the operation to perform.
        let operation = randomElement(probability(0.5) ? ALL_BIGINT_OPERATIONS : ALL_BIGINT_OPERATIONS_AND_COMPARISONS);

        let action = new Action(operation);
        push(action.inputs, exploredValueInput);
        if (includes(COMPARISON_OPS, operation)) {
            push(action.inputs, Inputs.randomBigintCloseTo(b));
        } else if (includes(BIGINT_SHIFT_OPS, operation)) {
            push(action.inputs, Inputs.randomBigintBetween(1n, 128n));
        } else if (includes(BITWISE_OPS, operation) || includes(ARITHMETIC_OPS, operation)) {
            push(action.inputs, Inputs.randomBigint());
        }
        return action;
    }

    function exploreSymbol(s) {
        // Lookup or insert the symbol into the global symbol registry. This will also allow static typing of the output.
        return new Action(OP_SYMBOL_REGISTRATION, [exploredValueInput]);
    }

    const ALL_BOOLEAN_OPERATIONS = concat(BOOLEAN_BINARY_OPS, BOOLEAN_UNARY_OPS);
    function exploreBoolean(b) {
        let operation = randomElement(ALL_BOOLEAN_OPERATIONS);

        let action = new Action(operation);
        push(action.inputs, exploredValueInput);
        if (includes(BOOLEAN_BINARY_OPS, operation)) {
            // It probably doesn't make sense to hardcode boolean constants, so always use an existing argument.
            push(action.inputs, Inputs.randomArgument());
        }
        return action;
    }

    // Explores the given value and returns an action to perform on it.
    function exploreValue(id, v) {
        if (isObject(v)) {
            return exploreObject(v);
        } else if (isFunction(v)) {
            return exploreFunction(v);
        } else if (isString(v)) {
            return exploreString(v);
        } else if (isNumber(v)) {
            return exploreNumber(v);
        } else if (isBigint(v)) {
            return exploreBigint(v);
        } else if (isSymbol(v)) {
            return exploreSymbol(v);
        } else if (isBoolean(v)) {
            return exploreBoolean(v);
        } else if (isUndefined(v)) {
            // Can't do anything with undefined.
            return NO_ACTION;
        } else {
            throw "Unexpected value type: " + typeof v;
        }
    }

    //
    // Exploration entrypoint.
    //
    function explore(id, v, currentThis, args, rngSeed) {
        rng.reseed(rngSeed);

        // The given arguments may be used as inputs for the action.
        if (isUndefined(args) || args.length < 1) throw "Exploration requires at least one additional argument";

        // We may get here recursively for example if a Proxy is being explored which triggers further explore calls during e.g. property enumeration.
        // Probably the best way to deal with these cases is to just bail out from recursive explorations.
        if (currentlyExploring) return;
        currentlyExploring = true;

        // Set the global state for this explore operation.
        exploreArguments = args;
        exploredValueInput = new SpecialInput("exploredValue");

        // Check if we already have a result for this id, and if so repeat the same action again. Otherwise, explore.
        let action;
        if (hasActionFor(id)) {
            action = getActionFor(id);
        } else {
            action = exploreValue(id, v);
            recordAction(id, action);
        }

        // Now perform the selected action.
        let context = { arguments: args, specialValues: { "exploredValue": v }, currentThis: currentThis };
        let success = execute(action, context);

        // If the action failed, mark this explore operation as failing (which will set the action to NO_ACTION) so it won't be retried again.
        if (!success) {
            recordFailure(id);
        }

        currentlyExploring = false;
    }

    function exploreWithErrorHandling(id, v, thisValue, args, rngSeed) {
        try {
            explore(id, v, thisValue, args, rngSeed);
        } catch (e) {
            let line = tryHasProperty('line', e) ? tryGetProperty('line', e) : tryGetProperty('lineNumber', e);
            if (isNumber(line)) {
                reportError("In line " + line + ": " + e);
            } else {
                reportError(e);
            }
        }
    }
    return exploreWithErrorHandling;
})();

const var_1_ = new Float64Array(1048576);
for (var var_2_ = 0; var_2_ < 10000; (() => {
    this.g ??= createGlobalObject();
    try {
      $vm.haveABadTime();
      Intl.Segmenter.prototype.__proto__ = var_2_;
      Object.__proto__ = var_1_;
    } catch {}
    try {
      var_1_.__proto__ = BigInt;
    } catch {}
    const var_3_ = new this.g.Int32Array(14785);
    var_1_.__proto__ = var_3_;
  })(), ++var_2_) {
  flashHeapAccess(10);
}
