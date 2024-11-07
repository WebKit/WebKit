const MAX_UINT8 = Math.pow(2, 8)-1;
const MIN_UINT = 0;
const MAX_UINT16 = Math.pow(2, 16)-1;
const MAX_UINT32 = Math.pow(2, 32)-1;
const MAX_UINT64 = Math.pow(2, 64)-1;

const MAX_INT8 = Math.pow(2, 8-1)-1;
const MIN_INT8 = -Math.pow(2, 8-1)+1;
const MAX_INT16 = Math.pow(2, 16-1)-1;
const MIN_INT16 = -Math.pow(2, 16-1)+1;
const MAX_INT32 = Math.pow(2, 32-1)-1;
const MIN_INT32 = -Math.pow(2, 32-1)+1;
const MAX_INT64 = Math.pow(2, 64-1)-1;
const MIN_INT64 = -Math.pow(2, 64-1)+1;


function deepCopy(object) {
    return JSON.parse(JSON.stringify(object));
}

const FUZZ_MARKER = [
    {type: 'uint64_t', value: 0x0},
    {type: 'uint8_t', value: 0x46},
    {type: 'uint8_t', value: 0x55},
    {type: 'uint8_t', value: 0x5a},
    {type: 'uint8_t', value: 0x5a},
    {type: 'uint8_t', value: 0x46},
    {type: 'uint8_t', value: 0x55},
    {type: 'uint8_t', value: 0x5a},
    {type: 'uint8_t', value: 0x5a},
    {type: 'uint64_t', value: 0x41},
];

class CoreIPCClass {
    messages;
    typeInfo;
    UI;
    GPU;
    Networking;
    messageByName = {};

    default_timeout = 1;

    constructor() {
        // Take a copy of IPC dictionaries to avoid calling into IPC API frequently
        this.messages = deepCopy(IPC.messages);
        this.typeInfo = deepCopy(IPC.serializedTypeInfo);
        this.enumInfo = deepCopy(IPC.serializedEnumInfo);
        this.objectIdentifiers = deepCopy(IPC.objectIdentifiers);

        this.initializeMessageByName();

        // fix-ups
        this.objectIdentifiers.push('WebKit::WebPageProxyIdentifier');
        this.objectIdentifiers.push('WebCore::ProcessIdentifier');
        this.objectIdentifiers.push('WebKit::RemoteCDMIdentifier');

        this.UI = this.initializeMessages("UI");
        this.GPU = this.initializeMessages("GPU");
        this.Networking = this.initializeMessages("Networking");
    }

    initializeMessageByName() {
        for(const [key, value] of Object.entries(this.messages)) {
            let [className, functionName] = key.split("_");
            value.className = className;
            value.functionName = functionName;
            this.messageByName[value.name] = value;
        }
    }

    initializeMessages(process) {
        const messages = {};
        for(const [key, value] of Object.entries(this.messages)) {
            let [className, functionName] = key.split("_");
            if(!(className in messages)) {
                messages[className] = {};
            }
            messages[className][functionName] = this.generateSendingFunction(process, value);
        }
        return messages;
    }

    generateSendingFunction(process, definition) {
        const name = definition.name;
        if(definition.replyArguments === null) {
            if(definition.isSync) {
                return (connectionIdentifier, messageArguments) => {
                    const serializedArguments = ArgumentSerializer.serializeArguments(definition.arguments, messageArguments);
                    serializedArguments.push(FUZZ_MARKER);
                    IPC.sendSyncMessage(process, connectionIdentifier, name, this.default_timeout, serializedArguments);
                }
            } else {
                return (connectionIdentifier, messageArguments) => {
                    const serializedArguments = ArgumentSerializer.serializeArguments(definition.arguments, messageArguments);
                    serializedArguments.push(FUZZ_MARKER);
                    IPC.sendMessage(process, connectionIdentifier, name, serializedArguments);
                }
            }
        } else {
            const replyArguments = definition.replyArguments;
            if(definition.isSync) {
                return (connectionIdentifier, messageArguments, replyHandler) => {
                    const serializedArguments = ArgumentSerializer.serializeArguments(definition.arguments, messageArguments);
                    serializedArguments.push(FUZZ_MARKER);
                    const reply = IPC.sendSyncMessage(process, connectionIdentifier, name, this.default_timeout, serializedArguments);
                    if(reply && replyHandler) {
                        const [parsedReply, typedReply] = ArgumentParser.parseReply(reply, replyArguments);
                        replyHandler(parsedReply, reply.buffer, typedReply);
                    }
                }
            } else {
                return (connectionIdentifier, messageArguments, replyHandler) => {
                    const connection = IPC.connectionForProcessTarget(process);
                    const serializedArguments = ArgumentSerializer.serializeArguments(definition.arguments, messageArguments);
                    serializedArguments.push(FUZZ_MARKER);
                    connection.sendWithAsyncReply(connectionIdentifier, name, serializedArguments, (reply)=> {
                        if(replyHandler) {
                            const [parsedReply, typedReply] = ArgumentParser.parseReply(reply, replyArguments);
                            replyHandler(parsedReply, reply.buffer, typedReply);
                        }
                    });
                }
            }
        }
    }

    newStreamConnection() {
        return new StreamConnection();
    }
}

export class StreamConnection {
    #connectionPair;
    handle;
    connection;
    constructor() {
        this.#connectionPair = IPC.createStreamClientConnection(14, 0.1);
        this.handle = this.#connectionPair[1];
        this.connection = this.#connectionPair[0];
        this.connection.open();
    }

    newInterface(interfaceName, connectionIdentifier) {
        return new StreamConnectionInterface(interfaceName, connectionIdentifier, this.connection);
    }
}

export class StreamConnectionInterface {
    #connection;
    #interfaceName;
    #connectionIdentifier;
    constructor(interfaceName, connectionIdentifier, connection) {
        this.#interfaceName = interfaceName;
        this.#connection = connection;
        this.#connectionIdentifier = connectionIdentifier;
        this.initializeMessages(interfaceName);
    }

    initializeMessages(interfaceName) {
        for(const [key, value] of Object.entries(CoreIPC.messages)) {
            let [className, functionName] = key.split("_");
            if(className == interfaceName) {
                this[functionName] = this.generateStreamSendingFunction(value);
            }
        }
    }

    generateStreamSendingFunction(definition) {
        const name = definition.name;
        if(definition.replyArguments === null) {
            if(definition.isSync) {
                return (messageArguments) => {
                    const serializedArguments = ArgumentSerializer.serializeArguments(definition.arguments, messageArguments);
                    this.#connection.sendSyncMessage(this.#connectionIdentifier, name, serializedArguments);
                }
            } else {
                return (messageArguments) => {
                    const serializedArguments = ArgumentSerializer.serializeArguments(definition.arguments, messageArguments);
                    this.#connection.sendMessage(this.#connectionIdentifier, name, serializedArguments);
                }
            }
        } else {
            const replyArguments = definition.replyArguments;
            if(definition.isSync) {
                return (messageArguments, replyHandler) => {
                    const serializedArguments = ArgumentSerializer.serializeArguments(definition.arguments, messageArguments);
                    const reply = this.#connection.sendSyncMessage(this.#connectionIdentifier, name, serializedArguments);
                    if(reply && replyHandler) {
                        const [parsedReply, typedReply] = ArgumentParser.parseReply(reply, replyArguments);
                        replyHandler(parsedReply, reply.buffer, typedReply);
                    }
                }
            } else {
                return (messageArguments, replyHandler) => {
                    const serializedArguments = ArgumentSerializer.serializeArguments(definition.arguments, messageArguments);
                    this.#connection.sendWithAsyncReply(this.#connectionIdentifier, name, serializedArguments, (reply)=> {
                        if(replyHandler) {
                            const [parsedReply, typedReply] = ArgumentParser.parseReply(reply, replyArguments);
                            replyHandler(parsedReply, reply.buffer, typedReply);
                        }
                    });
                }
            }
        }
    }
}

export class SerializationError extends Error {
    constructor(message) {
        super(message);
        this.name = 'SerializationError';
    }
}

const aliases = {
    'int': 'uint32_t',
    'unsigned': 'uint32_t',
    'char': 'uint8_t',
    'size_t': 'uint64_t',
    'pid_t': 'uint32_t',
    'unsigned short': 'uint16_t',
    'const uint8_t': 'uint8_t',
    'const char': 'uint8_t',
    'unsigned long': 'uint64_t',
    'long': 'uint64_t',
    'unsigned char': 'uint8_t',
    'CGFloat': 'double',
    'long long': 'int64_t',
    'unsigned long long': 'uint64_t',
    'short': 'int16_t',
    'GCGLenum': 'uint32_t',
    'const float': 'float',
    'const int32_t': 'int32_t',
    'const uint32_t': 'uint32_t',
    'NSInteger': 'int64_t',
    'NSUInteger': 'uint64_t',
    'WebCore::ContextMenuAction': 'uint32_t'
}

export function resolveAlias(argumentType) {
    if(argumentType in aliases) {
        return resolveAlias(aliases[argumentType]);
    }
    return argumentType;
}

function isPrimtiveType(type) {
    return ["double", "float", "bool","String","uint8_t", "int8_t","uint16_t", "int16_t","uint32_t", "int32_t","uint64_t", "int64_t",].includes(type);
}

function isEnum(type) {
    return CoreIPC.enumInfo[type] != undefined;
}

function isIdentifier(type) {
    return CoreIPC.objectIdentifiers.includes(type);
}

export class ArgumentSerializer {
    static enumSizeMap = {
      1: 'uint8_t',
      2: 'uint16_t',
      4: 'uint32_t',
      8: 'uint64_t'
    };

    static splitTemplateType(templateType) {
        const result = [];
        let current = '';
        let depth = 0;
        for(let index=0; index<templateType.length; index++) {
            const currentCharacter = templateType.charAt(index);
            if(currentCharacter == '>') {
                depth -= 1;
            }
            if(currentCharacter == '<') {
                depth += 1;
            }
            if(depth==0 && currentCharacter == ',') {
                result.push(current.trim());
                current = '';
            } else {
                current += currentCharacter;
            }
        }
        if(depth != 0) {
            throw new SerializationError(`unbalanced angle brackets when parsing '${ templateType }'`)
        }
        result.push(current.trim());
        return result;
    }

    static parseTemplate(name) {
        if(!name.endsWith(">")) {
            throw new SerializationError(`Cannot parse template '${ name }'. Does not end with '>'`);
        }
        const start = name.indexOf("<");
        if(start == -1) {
            throw new SerializationError(`Couldn't find start of template '${ name }'`);
        }
        return [name.substring(0, start), name.substring(start + 1, name.length-1)];
    }

    static serializeOptional(innerType, argument) {
        if(Object.hasOwn(argument, "optionalValue")) {
            const argumentDefinition = {
                type: innerType,
                name: 'optionalValue'
            };
            try {
                const serializedValue = ArgumentSerializer.serializeArgument(argumentDefinition, argument.optionalValue);
                return [{value: 1, type: 'bool'}, serializedValue];
            } catch (error) {
                if(error instanceof SerializationError) {
                    throw new SerializationError(`when serializing optional value of type '${ innerType }': ${ error.message }`);
                } else {
                    throw error;
                }
            }
        } else {
            return [{value: 0, type: 'bool'}];
        }
    }

    static serializeMarkable(innerType, argument) {
        if(Object.hasOwn(argument, "optionalValue")) {
            const argumentDefinition = {
                type: innerType,
                name: 'optionalValue'
            };
            try {
                const serializedValue = ArgumentSerializer.serializeArgument(argumentDefinition, argument.optionalValue);
                return [{value: 0, type: 'bool'}, serializedValue];
            } catch (error) {
                if(error instanceof SerializationError) {
                    throw new SerializationError(`when serializing optional value of type '${ innerType }': ${ error.message }`);
                } else {
                    throw error;
                }
            }
        } else {
            return [{value: 1, type: 'bool'}];
        }
    }

    static serializeVector(innerType, argument) {
        const splitType = ArgumentSerializer.splitTemplateType(innerType);
        innerType = splitType[0];
        if(Array.isArray(argument)) {
            const result = [];
            const argumentDefinition = {
                type: innerType,
            };
            let count = 0;
            for(const element of argument) {
                argumentDefinition.name = `element#${ count }`;
                count += 1;
                result.push([ ArgumentSerializer.serializeArgument(argumentDefinition, element) ]);
            }
            return [{value: result, type: 'Vector'}];
        }
        throw new SerializationError('argument of vector-type is not an array');
    }

    static serializeHashSet(innerType, argument) {
        const splitType = ArgumentSerializer.splitTemplateType(innerType);
        innerType = splitType[0];
        if(Array.isArray(argument)) {
            const result = [];
            const argumentDefinition = {
                type: innerType,
            };
            let count = 0;
            for(const element of argument) {
                argumentDefinition.name = `element#${ count }`;
                count += 1;
                result.push([ ArgumentSerializer.serializeArgument(argumentDefinition, element) ]);
            }
            return [{value: count, type: 'uint32_t'}, result];
        }
        throw new SerializationError('argument of HashSet type is not an array');
    }

    static serializeHashMap(innerType, argument) {
        if(Array.isArray(argument)) {
            const result = [];
            const argumentDefinition = {
                type: innerType,
            };
            let count = 0;
            for(const element of argument) {
                argumentDefinition.name = `key#${ count }`;
                count += 1;
                result.push([ ArgumentSerializer.serializeKeyValuePair(argumentDefinition, element) ]);
            }
            return [{value: count, type: 'uint32_t'}, result];
        }
        throw new SerializationError('argument of HashMap type is not an array');
    }

    static serializeStdArray(innerType, argument) {
        const splitType = ArgumentSerializer.splitTemplateType(innerType);
        if(splitType.length != 2) {
            throw new SerializationError('std::pair does not have a fixed cardinality')
        }
        const size = Number(splitType[1]);
        innerType = splitType[0];
        if(Array.isArray(argument) || argument.length != size) {
            const result = [];
            const argumentDefinition = {
                type: innerType,
            };
            let count = 0;
            for(const element of argument) {
                argumentDefinition.name = `element#${ count }`;
                count += 1;
                result.push([ ArgumentSerializer.serializeArgument(argumentDefinition, element) ]);
            }
            return result;
        }
        throw new SerializationError('argument of std::pair type is not an array');
    }

    static serializePair(innerType, argument) {
        if(Array.isArray(argument) && argument.length == 2) {
            const result = [];
            const innerTypes = ArgumentSerializer.splitTemplateType(innerType);
            let argumentDefinition = {
                type: innerTypes[0],
                name: 'element#0',
            }
            result.push([ ArgumentSerializer.serializeArgument(argumentDefinition, argument[0]) ]);
            argumentDefinition.type = innerTypes[1];
            argumentDefinition.name = 'element#1';
            result.push([ ArgumentSerializer.serializeArgument(argumentDefinition, argument[1]) ]);
            return result;
        }
        throw new SerializationError('argument of pair-type is not an array with two elements');
    }

    static serializeKeyValuePair(innerType, argument) {
        const splitTemplateType = ArgumentSerializer.splitTemplateType(innerType);
        if(argument['key'] === undefined)
            throw new SerializationError('argument of key value type is missing the key attribute');
        if(argument['value'] === undefined)
            throw new SerializationError('argument of key value type is missing the key attribute');
        const result = [];
        let argumentDefinition = {type: splitTemplateType[0], name: "key"};
        result.push(ArgumentSerializer.serializeArgument(argumentDefinition, argument['key']));
        argumentDefinition = {type: splitTemplateType[1], name: "value"};
        result.push(ArgumentSerializer.serializeArgument(argumentDefinition, argument['value']));
        return result;
    }

    static serializeVariant(innerType, argument) {
        const variantTypes = ArgumentSerializer.splitTemplateType(innerType);
        if(!Object.hasOwn(argument, 'variantType')) {
            throw new SerializationError('missing field \'variantType\' when serializing variant');
        }
        const variantType = argument.variantType;
        const variantIndex = variantTypes.indexOf(variantType);
        if(variantIndex == -1) {
            throw new SerializationError(`type '${ variantType }' is not valid for variant`);
        }
        if(!Object.hasOwn(argument, 'variant')) {
            throw new SerializationError('missing field \'variant\' when serializing variant');
        }
        const argumentDefinition = {
            name: 'variant',
            type: variantType
        };
        return [{value: variantIndex, type: 'uint8_t'}, ArgumentSerializer.serializeArgument(argumentDefinition, argument.variant)];
    }

    static serializeTemplate(argumentDefinition, argument) {
        const argumentType = argumentDefinition.type;
        if(argumentType.includes("<")) {
            const [ templateType, innerType ] = ArgumentSerializer.parseTemplate(argumentType);
            switch (templateType) {
                case 'RefPtr':
                case 'std::unique_ptr':
                case 'RetainPtr':
                case 'std::optional':
                case 'Optional':
                    return ArgumentSerializer.serializeOptional(innerType, argument);
                case 'Vector':
                case 'std::vector':
                case 'std::span':
                case 'ArrayReference':
                case 'Span':
                    return ArgumentSerializer.serializeVector(innerType, argument);
                case 'std::array':
                    return ArgumentSerializer.serializeStdArray(innerType, argument);
                case 'HashSet':
                    return ArgumentSerializer.serializeHashSet(innerType, argument);
                case 'std::variant':
                    return ArgumentSerializer.serializeVariant(innerType, argument);
                case 'std::pair':
                    return ArgumentSerializer.serializePair(innerType, argument);
                case 'OptionSet':
                    return ArgumentSerializer.serializeOptionSet(innerType, argumentDefinition.name, argument);
                case 'Markable':
                    return ArgumentSerializer.serializeMarkable(innerType, argument);
                case 'KeyValuePair':
                    return ArgumentSerializer.serializeKeyValuePair(innerType, argument);
                case 'HashMap':
                case 'MemoryCompactRobinHoodHashMap':
                    return ArgumentSerializer.serializeHashMap(innerType, argument);
                case 'Ref':
                case 'UniqueRef':
                    return ArgumentSerializer.serializeArgument({type: innerType, name: argumentDefinition.name}, argument);
                default:
                    throw new SerializationError(`Don't know how to serialize template '${ templateType }'`);
            }
        }
        return undefined;
    }

    static serializeIdentifier(argumentDefinition, argument) {
        if(CoreIPC.objectIdentifiers.includes(argumentDefinition.type)) {
            if(typeof argument != 'number' && typeof argument != 'bigint') {
                throw new SerializationError(`Identifier of type ${ argumentDefinition.type } is not a number`);
            }
            if(argument < MIN_INT64 || argument > MAX_INT64) {
                throw new SerializationError(`Identifier value (${ argumentDefinition.name }) of type ${ argumentDefinition.type } is out-of-bounds.`);
            }
            return {value: argument, type: 'uint64_t'};
        }
        return undefined;
    }

    static serializeOptionSet(innerType, name, argument) {
        const argumentDefinition = {
            type: innerType,
            name: name
        }
        return this.serializeEnum(argumentDefinition, argument);
    }

    static serializeEnum(argumentDefinition, argument) {
        const enumType = argumentDefinition.enum ? argumentDefinition.enum : argumentDefinition.type;
        if(enumType in CoreIPC.enumInfo) {
            const enumDefinition = CoreIPC.enumInfo[enumType];
            const enumRepresentation = ArgumentSerializer.enumSizeMap[enumDefinition.size];
            if(!enumRepresentation) {
                throw new SerializationError(`Invalid enum size '${ enumDefinition.size }' when serializing ${ enumType } of member ${ argumentDefinition.name }`);
            }
            if(!enumDefinition.isOptionSet && !enumDefinition.validValues.includes(argument)) {
                throw new SerializationError(`Invalid enum value ${ argument } when serializing ${ enumType } of member ${ argumentDefinition.name }`);
            }
            return {value: argument, type: enumRepresentation};
        }
        return undefined;
    }

    static serializeType(argumentDefinition, argument) {
        if(argumentDefinition.type in CoreIPC.typeInfo) {
            try {
                return ArgumentSerializer.serializeArguments(CoreIPC.typeInfo[argumentDefinition.type], argument);
            } catch(error) {
                if(error instanceof SerializationError) {
                    let message = `When serializing struct ${ argumentDefinition.name } of type ${ argumentDefinition.type }: ${ error.message }`;
                    throw new SerializationError(message);
                } else {
                    throw error;
                }
            }
        }
        return undefined;
    }

    static serializePrimitive(argumentDefinition, argument) {
        switch(argumentDefinition.type) {
            case 'uint8_t':
                if(typeof argument != "number") {
                    throw new SerializationError(`Primitive value of type ${ argumentDefinition.type } is not a number`);
                }
                if(argument < MIN_UINT || argument > MAX_UINT8) {
                    throw new SerializationError(`Primitive value (${ argumentDefinition.name }) of type ${ argumentDefinition.type } is out-of-bounds.`);
                }
                return {value: argument, type: argumentDefinition.type};
            case 'int8_t':
                if(typeof argument != "number") {
                    throw new SerializationError(`Primitive value of type ${ argumentDefinition.type } is not a number`);
                }
                if(argument < MIN_INT8 || argument > MAX_INT8) {
                    throw new SerializationError(`Primitive value (${ argumentDefinition.name }) of type ${ argumentDefinition.type } is out-of-bounds.`);
                }
                return {value: argument, type: argumentDefinition.type};
            case 'uint16_t':
                if(typeof argument != "number") {
                    throw new SerializationError(`Primitive value of type ${ argumentDefinition.type } is not a number`);
                }
                if(argument < MIN_UINT || argument > MAX_UINT16) {
                    throw new SerializationError(`Primitive value (${ argumentDefinition.name }) of type ${ argumentDefinition.type } is out-of-bounds.`);
                }
                return {value: argument, type: argumentDefinition.type};
            case 'int16_t':
                if(typeof argument != "number") {
                    throw new SerializationError(`Primitive value of type ${ argumentDefinition.type } is not a number`);
                }
                if(argument < MIN_INT16 || argument > MAX_INT16) {
                    throw new SerializationError(`Primitive value (${ argumentDefinition.name }) of type ${ argumentDefinition.type } is out-of-bounds.`);
                }
                return {value: argument, type: argumentDefinition.type};
            case 'uint32_t':
                if(typeof argument != "number") {
                    throw new SerializationError(`Primitive value of type ${ argumentDefinition.type } is not a number`);
                }
                if(argument < MIN_UINT || argument > MAX_UINT32) {
                    throw new SerializationError(`Primitive value (${ argumentDefinition.name }) of type ${ argumentDefinition.type } is out-of-bounds.`);
                }
                return {value: argument, type: argumentDefinition.type};
            case 'int32_t':
                if(typeof argument != "number") {
                    throw new SerializationError(`Primitive value of type ${ argumentDefinition.type } is not a number`);
                }
                if(argument < MIN_INT32 || argument > MAX_INT32) {
                    throw new SerializationError(`Primitive value (${ argumentDefinition.name }) of type ${ argumentDefinition.type } is out-of-bounds.`);
                }
                return {value: argument, type: argumentDefinition.type};
            case 'uint64_t':
                if(typeof argument != "number") {
                    throw new SerializationError(`Primitive value of type ${ argumentDefinition.type } is not a number`);
                }
                if(argument < MIN_UINT || argument > MAX_UINT64) {
                    throw new SerializationError(`Primitive value (${ argumentDefinition.name }) of type ${ argumentDefinition.type } is out-of-bounds.`);
                }
                return {value: argument, type: argumentDefinition.type};
            case 'int64_t':
                if(typeof argument != "number") {
                    throw new SerializationError(`Primitive value of type ${ argumentDefinition.type } is not a number`);
                }
                if(argument < MIN_INT64 || argument > MAX_INT64) {
                    throw new SerializationError(`Primitive value (${ argumentDefinition.name }) of type ${ argumentDefinition.type } is out-of-bounds.`);
                }
                return {value: argument, type: argumentDefinition.type};
            case 'float':
            case 'double':
                if(typeof argument != "number") {
                    throw new SerializationError(`Primitive value of type ${ argumentDefinition.type } is not a number`);
                }
                return {value: argument, type: argumentDefinition.type};
            case 'String':
                if(typeof argument != 'string') {
                    throw new SerializationError(`Primitive value is not a string`);
                }
                return {value: argument, type: argumentDefinition.type};
            case 'bool':
                if(typeof argument != 'boolean') {
                    throw new SerializationError(`Primitive value is not a bool`);
                }
                return {value: argument ? 1 : 0, type: argumentDefinition.type};
            case 'IPC::ConnectionHandle':
                if(argument instanceof StreamConnection) {
                    return {value: argument.handle, type: 'ConnectionHandle'};
                } else if(argument.open) {
                    return {value: argument, type: 'ConnectionHandle'};
                } else {
                    throw new SerializationError(`ConnectionHandle is not a connection object`);
                }
            case 'IPC::StreamServerConnectionHandle':
                if(argument instanceof StreamConnection) {
                    return {value: argument.handle, type: 'StreamServerConnectionHandle'};
                } else if(argument.open) {
                    return {value: argument, type: 'StreamServerConnectionHandle'};
                } else {
                    throw new SerializationError(`ConnectionHandle is not a connection object`);
                }
            case 'std::nullptr_t':
                if(argument === null) {
                    return [];
                } else throw new SerializationError(`std::nullptr_t is not null`);
        }
        return undefined;
    }

    static serializeArgument(argumentDefinition, argument) {
        let result;
        argumentDefinition.type = resolveAlias(argumentDefinition.type);
        if(argumentDefinition.optional === true) {
            argumentDefinition.type = `Optional<${ argumentDefinition.type }>`;
            argumentDefinition.optional = false;
        }
        if(result = ArgumentSerializer.serializeTemplate(argumentDefinition, argument))
            return result;
        if(result = ArgumentSerializer.serializeIdentifier(argumentDefinition, argument))
            return result;
        if(result = ArgumentSerializer.serializeEnum(argumentDefinition, argument))
            return result;
        if(result = ArgumentSerializer.serializePrimitive(argumentDefinition, argument))
            return result;
        if(result = ArgumentSerializer.serializeType(argumentDefinition, argument))
            return result;
        throw new Error(`Don't know how to serialize ${ argumentDefinition.name } of type ${ argumentDefinition.type }`);
    }

    static serializeArguments(methodArgumentsDefinition, methodArguments) {
        const result = [];
        for(const argument of methodArgumentsDefinition) {
            const name = ArgumentSerializer.simplifyName(argument.name);
            if(name in methodArguments) {
                try {
                    result.push(ArgumentSerializer.serializeArgument(argument, methodArguments[name]));
                } catch (error) {
                    if(error instanceof SerializationError) {
                        throw new SerializationError(`When serializing argument/field '${ name }': ` + error.message);
                    } else {
                        throw error;
                    }
                }
            } else {
                throw new SerializationError(`argument/field '${ name }' is missing`);
            }
        }
        return result;
    }

    static simplifyName(name) {
        name = name.replaceAll("()", "");
        let pos = -1;
        while((pos = name.indexOf(".")) > -1) {
            name = name.substring(0, pos) + name.charAt(pos+1).toUpperCase() + name.substring(pos+2, name.length);
        }
        return name
    }

}

export class ParserError extends Error {
    constructor(message) {
        super(message);
        this.name = 'ParserError';
    }
}

const MESSAGE_HEADER_SIZE = 0x10;

export class ArgumentParser {
    static align(position, granularity) {
        if(position%granularity) {
            position = position + granularity-(position%granularity);
        }
        return position;
    }

    static parseReply(reply, replyArguments) {
        const buffer = new DataView(reply.buffer);
        const [, typedResult, result] = ArgumentParser.parseArguments(buffer, MESSAGE_HEADER_SIZE, replyArguments);
        return [result, typedResult]
    }

    static checkOutOfBounds(buffer, position, requestedSize) {
        if(position + requestedSize > buffer.byteLength) {
            throw new ParserError('out of bounds');
        }
    }

    static parseIdentifier(buffer, position, argumentDefinition) {
        if(CoreIPC.objectIdentifiers.includes(argumentDefinition.type)) {
            position = ArgumentParser.align(position, 8);
            ArgumentParser.checkOutOfBounds(buffer, position, 8);
            return [position + 8, {parsedValue: buffer.getBigUint64(position, true), parsedType: argumentDefinition.type}];
        }
        return undefined;
    }

    static parseType(buffer, position, argumentDefinition) {
        if(argumentDefinition.type in CoreIPC.typeInfo) {
            const [newPosition, value] = this.parseArguments(
                buffer, position, CoreIPC.typeInfo[argumentDefinition.type]
            );
            return [newPosition, {parsedValue: value, parsedType: argumentDefinition.type}];
        }
        return undefined;
    }

    static parseVector(buffer, position, innerType) {
        position = ArgumentParser.align(position, 8);
        ArgumentParser.checkOutOfBounds(buffer, position, 8);
        const elementCount = buffer.getBigUint64(position, true);
        position += 8;
        const values = [];
        let element;
        const argumentDefinition = {type: innerType};
        for(let index = 0; index < elementCount; index++) {
            argumentDefinition.name = `element#${ index }`;
            try {
                [position, element] = ArgumentParser.parseArgument(buffer, position, argumentDefinition);
            } catch (error) {
                throw new ParserError(`when parsing element #${ index } of array: ${ error.message }`);
            }
            values.push(element);
        }
        return [position, values];
    }

    static parsePair(buffer, position, innerType) {
        const values = [];
        const innerTypes = ArgumentSerializer.splitTemplateType(innerType);
        let argumentDefinition = {type: innerTypes[0]};
        argumentDefinition.name = "element#0";
        let element;
        [position, element] = ArgumentParser.parseArgument(buffer, position, argumentDefinition);
        values.push(element);
        argumentDefinition.name = "element#1";
        argumentDefinition.type = innerTypes[1];
        [position, element] = ArgumentParser.parseArgument(buffer, position, argumentDefinition);
        values.push(element);
        return [position, values];
    }

    static parseOptional(buffer, position, innerType) {
        ArgumentParser.checkOutOfBounds(buffer, position, 1);
        const has = !!buffer.getUint8(position);
        position += 1;
        if(has) {
            const argumentDefinition = {name: 'optionalValue', type: innerType};
            const [newPosition, optionalValue] = ArgumentParser.parseArgument(buffer, position, argumentDefinition)
            return [newPosition, {optionalValue: optionalValue}];
        } else {
            return [position, {}]
        }
    }

    static parseMarkable(buffer, position, innerType) {
        ArgumentParser.checkOutOfBounds(buffer, position, 1);
        const isEmpty = !!buffer.getUint8(position);
        position += 1;
        if(!isEmpty) {
            const argumentDefinition = {name: 'optionalValue', type: innerType};
            return ArgumentParser.parseArgument(buffer, position, argumentDefinition);
        } else {
            return [position, {}]
        }
    }

    static parseVariant(buffer, position, innerType) {
        ArgumentParser.checkOutOfBounds(buffer, position, 1);
        const variantTypes = ArgumentSerializer.splitTemplateType(innerType);
        const variantIndex = buffer.getUint8(position, true);
        if(variantIndex > variantTypes.length - 1) {
            throw new ParserError(`invalid variant index ${ variantIndex }`)
        }
        position += 1;
        const variantType = variantTypes[variantIndex];
        const argumentDefinition = {name: 'variant', type: variantType};
        const [newPosition, value] = ArgumentParser.parseArgument(buffer, position, argumentDefinition);
        return [newPosition, {variantType: variantType, variant: value}];
    }

    static parseTemplate(buffer, position, argumentDefinition) {
        const argumentType = argumentDefinition.type;
        if(argumentType.includes("<")) {
            const [ templateType, innerType ] = ArgumentSerializer.parseTemplate(argumentType);
            switch (templateType) {
                case 'RefPtr':
                case 'std::unique_ptr':
                case 'RetainPtr':
                case 'std::optional':
                case 'Optional': {
                    const [newPosition, value] = ArgumentParser.parseOptional(buffer, position, innerType);
                    return [newPosition, {parsedType: argumentDefinition.type, parsedValue: value}]
                }
                case 'std::span':
                case 'Vector': {
                    const [newPosition, values] = ArgumentParser.parseVector(buffer, position, innerType);
                    return [newPosition, {parsedType: argumentDefinition.type, value: values}];
                }
                // TODO: HashSet and std::array and KeyValuePair and HashMap
                case 'std::variant': {
                    const [newPosition, variant] = ArgumentParser.parseVariant(buffer, position, innerType);
                    return [newPosition, {parsedType: argumentDefinition.type, parsedValue: variant}];
                }
                case 'std::pair': {
                    let [newPosition, values] = ArgumentParser.parsePair(buffer, position, innerType);
                    return [newPosition, {parsedType: argumentDefinition.type, parsedValue: values}];
                }
                case 'OptionSet': {
                    let [newPosition, optionSet] = ArgumentParser.parseEnum(buffer, position, {type: innerType, name: argumentDefinition.name});
                    return [newPosition, {parsedType: argumentDefinition.type, parsedValue: optionSet}];
                }
                case 'Markable': {
                    const [newPosition, value] = ArgumentParser.parseMarkable(buffer, position, innerType);
                    return [newPosition, {parsedType: argumentDefinition.type, parsedValue: value}]
                }
                case 'Ref':
                case 'UniqueRef': {
                    return ArgumentParser.parseArgument(buffer, position, {type: innerType, name: argumentDefinition.name});
                }
                default:
                    throw new ParserError(`Don't know how to parse template type '${ templateType }'`)
            }
        }
        return undefined;
    }

    static parseEnum(buffer, position, argumentDefinition) {
        const argumentType = argumentDefinition.type;
        if(argumentType in CoreIPC.enumInfo) {
            const enumArgumentDefintion = {
                type: ArgumentSerializer.enumSizeMap[CoreIPC.enumInfo[argumentType].size],
                name: argumentDefinition.name
            };
            return ArgumentParser.parsePrimitive(buffer, position, enumArgumentDefintion);
        }
        return undefined;
    }

    static parsePrimitive(buffer, position, argumentDefinition) {
        switch(argumentDefinition.type) {
            case 'bool':
                ArgumentParser.checkOutOfBounds(buffer, position, 1);
                return [position + 1, {parsedValue: !!buffer.getUint8(position), parsedType: argumentDefinition.type}];
            case 'uint8_t':
                ArgumentParser.checkOutOfBounds(buffer, position, 1);
                return [position + 1, {parsedValue: buffer.getUint8(position), parsedType: argumentDefinition.type}];
            case 'int8_t':
                ArgumentParser.checkOutOfBounds(buffer, position, 1);
                return [position + 1, {parsedValue: buffer.getInt8(position), parsedType: argumentDefinition.type}];
            case 'uint16_t':
                position = ArgumentParser.align(position, 2);
                ArgumentParser.checkOutOfBounds(buffer, position, 2);
                return [position + 2, {parsedValue: buffer.getUint16(position, true), parsedType: argumentDefinition.type}];
            case 'int16_t':
                position = ArgumentParser.align(position, 2);
                ArgumentParser.checkOutOfBounds(buffer, position, 2);
                return [position + 2, {parsedValue: buffer.getInt16(position, true), parsedType: argumentDefinition.type}]
            case 'uint32_t':
                position = ArgumentParser.align(position, 4);
                ArgumentParser.checkOutOfBounds(buffer, position, 4);
                return [position + 4, {parsedValue: buffer.getUint32(position, true), parsedType: argumentDefinition.type}];
            case 'int32_t':
                position = ArgumentParser.align(position, 4);
                ArgumentParser.checkOutOfBounds(buffer, position, 4);
                return [position + 4, {parsedValue: buffer.getInt32(position, true), parsedType: argumentDefinition.type}];
            case 'uint64_t':
                position = ArgumentParser.align(position, 8);
                ArgumentParser.checkOutOfBounds(buffer, position, 8);
                return [position + 8, {parsedValue: buffer.getBigUint64(position, true), parsedType: argumentDefinition.type}];
            case 'int64_t':
                position = ArgumentParser.align(position, 8);
                ArgumentParser.checkOutOfBounds(buffer, position, 8);
                return [position + 8, {parsedValue: buffer.getBigInt64(position, true), parsedType: argumentDefinition.type}];
            case 'float':
                position = ArgumentParser.align(position, 4);
                ArgumentParser.checkOutOfBounds(buffer, position, 4);
                return [position + 4, {parsedValue: buffer.getFloat32(position), parsedType: argumentDefinition.type}];
            case 'double':
                position = ArgumentParser.align(position, 8);
                ArgumentParser.checkOutOfBounds(buffer, position, 8);
                return [position + 8, {parsedValue: buffer.getFloat64(position), parsedType: argumentDefinition.type}];
            case 'String': {
                position = ArgumentParser.align(position, 4);
                ArgumentParser.checkOutOfBounds(buffer, position, 4);
                const stringLength = buffer.getUint32(position, true);
                position += 4;
                if(stringLength == 0xffffffff) {
                    // null string
                    return [position, {parsedValue: null, parsedType: 'String'}];
                }
                const is8Bit = !!buffer.getUint8(position);
                position += 1;
                let result = '';
                if(is8Bit) {
                    ArgumentParser.checkOutOfBounds(buffer, position, stringLength);
                    for(let i=0; i<stringLength; i++) {
                        result += String.fromCharCode(buffer.getUint8(position));
                        position += 1;
                    }
                } else {
                    ArgumentParser.checkOutOfBounds(buffer, position, stringLength * 2);
                    for(let i=0; i<stringLength; i++) {
                        result += String.fromCharCode(buffer.getUint16(position));
                        position += 2;
                    }
                }
                return [position, {parsedValue: result, parsedType: 'String'}];
            }
            case 'std::nullptr_t':
                return [position, {parsedValue: 'null', parsedType: 'std::nullptr_t'}];
        }
        return undefined;
    }

    static parseArgument(buffer, position, argumentDefinition) {
        let result;
        argumentDefinition.type = resolveAlias(argumentDefinition.type);
        if(argumentDefinition.optional === true) {
            argumentDefinition.type = `Optional<${ argumentDefinition.type }>`;
            argumentDefinition.optional = false;
        }
        if(result = ArgumentParser.parseTemplate(buffer, position, argumentDefinition)) {
            return result;
        }
        if(result = ArgumentParser.parseIdentifier(buffer, position, argumentDefinition)) {
            return result;
        }
        if(result = ArgumentParser.parseEnum(buffer, position, argumentDefinition)) {
            return result;
        }
        if(result = ArgumentParser.parsePrimitive(buffer, position, argumentDefinition)) {
            return result;
        }
        if(result = ArgumentParser.parseType(buffer, position, argumentDefinition)) {
            return result;
        }
        throw new ParserError(`Don't know how to parse type '${ argumentDefinition.type }'`);
    }

    static untypeResultOld(typedResult) {
        let result;
        if(Array.isArray(typedResult)) {
            result = [];
            for(const value of typedResult) {
                if(typeof(value.value) == 'object') {
                    result.push(ArgumentParser.untypeResult(value));
                } else {
                    result.push(value.value);
                }
            }
        } else {
            result = {};
            for(const [key, value] of Object.entries(typedResult)) {
                if(typeof(value.value) == 'object') {
                    result[key] = ArgumentParser.untypeResult(value.value);
                } else {
                    result[key] = value.value;
                }
            }
        }
        return result;
    }

    static untypeResult(typedResult) {
        if(typeof(typedResult)=='object') {
            if('parsedType' in typedResult) {
                if(isEnum(typedResult.parsedType) || isPrimtiveType(typedResult.parsedType) || isIdentifier(typedResult.parsedType)) {
                    return typedResult.parsedValue;
                }
                return ArgumentParser.untypeResult(typedResult.parsedValue);
            } else {
                const result = {};
                for(const [key, value] of Object.entries(typedResult)) {
                    result[key] = ArgumentParser.untypeResult(value);
                }
                return result;
            }
        }
        if(Array.isArray(typedResult)) {
            const newArray = [];
            for(const element of typedResult) {
                newArray.push(ArgumentParser.untypeResult(element));
            }
            return newArray;
        }
        return typedResult;
    }

    static parseArguments(buffer, position, replyArguments) {
        const typedResult = {}
        for(const argument of replyArguments) {
            try {
                let parseResult;
                [position, parseResult] = ArgumentParser.parseArgument(buffer, position, argument);
                typedResult[ArgumentSerializer.simplifyName(argument.name)] = parseResult;
            } catch (error) {
                if(error instanceof ParserError) {
                    throw new ParserError(`When parsing field '${ argument.name }' of type '${ argument.type }': ${ error.message }`);
                } else {
                    throw error;
                }
            }
        }
        return [position, typedResult, ArgumentParser.untypeResult(typedResult)];
    }
}

export class IPCWireTap {
    #nextTaps
    #allTaps
    #everyTaps
    #listenerFunction
    #process

    constructor(process, direction) {
        if(!['UI','GPU','Networking'].includes(process)){
            throw new Error(`WireTap: process has to be one of 'UI','GPU' or 'Networking' but is '${process}'`);
        }
        if(direction != "Outgoing" && direction != "Incoming") {
            throw new Error(`WireTap: direction has to be one of 'Outgoing' or 'Incoming' but is '${direction}'`);
        }
        this.#process = process;
        this.#allTaps = [];
        this.#everyTaps = {};
        this.#nextTaps = {};
        this.#listenerFunction = this.createListener();
        if(direction == "Outgoing") {
            IPC.addOutgoingMessageListener(process, this.#listenerFunction);
        } else {
            IPC.addIncomingMessageListener(process, this.#listenerFunction);
        }
        // self reference to avoid gc issues
        if(!window.refs) {
            window.refs = [];
        }
        window.refs.push(this);
    }

    tapNext(message, tap) {
        if(!(message in this.#nextTaps)) {
            this.#nextTaps[message] = [];
        }
        this.#nextTaps[message].push(tap);
    }

    tapEvery(message, tap) {
        if(!(message in this.#everyTaps)) {
            this.#everyTaps[message] = [];
        }
        this.#everyTaps[message].push(tap);
    }

    tapAll(tap) {
        this.#allTaps.push(tap);
    }

    parseMessage(messageName, buffer) {
        const messageArguments = CoreIPC.messageByName[messageName].arguments;
        if(!messageArguments) return [0, {}, {}];
        try {
            return ArgumentParser.parseArguments(buffer, MESSAGE_HEADER_SIZE, messageArguments);
        } catch (error) {
            if(window.$vm) {
                $vm?.print("WireTap couldn't parse message: " + error.message);
            } else {
                console.log("WireTap couldn't parse message: " + error.message);
            }
        }
    }

    isFuzzMessage(buffer) {
            if(buffer.byteLength >= 24) {
                const slice = new Int32Array(buffer.slice(buffer.byteLength - 12));
                if(slice[0] == 0x5a5a5546)
                    return true;
            }
            return false;
    }

    createListener() {
        return (tapArguments) => {
            const messageName = tapArguments.name;
            const connectionID = tapArguments.destinationID;
            const buffer = tapArguments.buffer;
            if(this.isFuzzMessage(buffer)) {
                return;
            }
            let typedResult;
            let result;
            for(const tap of this.#allTaps) {
                if(typedResult == undefined && buffer)
                    [, typedResult, result] = this.parseMessage(messageName, new DataView(buffer));
                tap(this.#process, connectionID, messageName, typedResult, result);
            }
            const everyTaps = this.#everyTaps[messageName];
            if(everyTaps) {
                for(const tap of everyTaps) {
                    if(typedResult == undefined && buffer)
                        [, typedResult, result] = this.parseMessage(messageName, new DataView(buffer));
                    tap(this.#process, connectionID, messageName, typedResult, result);
                }
            }
            const nextTaps = this.#nextTaps[messageName];
            if(nextTaps) {
                let tap;
                while(tap = nextTaps.pop()) {
                    if(typedResult == undefined && buffer)
                        [, typedResult, result] = this.parseMessage(messageName, new DataView(buffer));
                    tap(this.#process, connectionID, messageName, typedResult, result);
                    break; // TODO: only do this while fuzzing
                }
            }
        };
    }
}

export const CoreIPC = new CoreIPCClass();
