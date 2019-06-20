/* Type Utilties */

// FIXME: Support all WHLSL scalar and vector types.
// FIXME: Support textures and samplers.
const Types = Object.freeze({
    BOOL: Symbol("bool"),
    INT: Symbol("int"),
    UCHAR: Symbol("uchar"),
    UINT: Symbol("uint"),
    FLOAT: Symbol("float"),
    FLOAT4: Symbol("float4"),
    MAX_SIZE: 16 // This needs to be big enough to hold any singular WHLSL type.
});

function isVectorType(type)
{
    switch(type) {
        case Types.FLOAT4:
            return true;
        default: 
            return false;
    }
}

function convertTypeToArrayType(type)
{
    switch(type) {
        case Types.BOOL:
            return Uint8Array;
        case Types.INT:
            return Int32Array;
        case Types.UCHAR:
            return Uint8Array;
        case Types.UINT:
            return Uint32Array;
        case Types.FLOAT:
        case Types.FLOAT4:
            return Float32Array;
        default:
            throw new Error("Invalid TYPE provided!");
    }
}

function convertTypeToWHLSLType(type)
{
    switch(type) {
        case Types.BOOL:
            return "bool";
        case Types.INT:
            return "int";
        case Types.UCHAR:
            return "uchar";
        case Types.UINT:
            return "uint";
        case Types.FLOAT:
            return "float";
        case Types.FLOAT4:
            return "float4";
        default:
            throw new Error("Invalid TYPE provided!");
    }
}

/* Harness Classes */

class Data {
    /**
     * Upload typed data to and return a wrapper of a GPUBuffer.
     * @param {Types} type - The WHLSL type to be stored in this Data.
     * @param {Number or Array[Number]} values - The raw data to be uploaded.
     */
    constructor(harness, type, values, isPointer = false)
    {
        // One or more scalars in an array can be accessed through a pointer to buffer.
        // However, vector types are also created via an array of scalars.
        // This ensures that buffers of just one vector are usable in a test function.
        if (Array.isArray(values))
            this._isPointer = isVectorType(type) ? isPointer : true;
        else {
            this._isPointer = false;
            values = [values];
        }

        this._type = type;
        this._byteLength = (convertTypeToArrayType(type)).BYTES_PER_ELEMENT * values.length;

        const [buffer, arrayBuffer] = harness._device.createBufferMapped({
            size: this._byteLength,
            usage: GPUBufferUsage.STORAGE | GPUBufferUsage.MAP_READ
        });

        const typedArray = new (convertTypeToArrayType(type))(arrayBuffer);
        typedArray.set(values);
        buffer.unmap();

        this._buffer = buffer;
    }

    /**
     * @returns An ArrayBuffer containing the contents of this Data.
     */
    async getArrayBuffer()
    {
        let result;
        try {
            result = await this._buffer.mapReadAsync();
            this._buffer.unmap();
        } catch {
            throw new Error("Data error: Unable to get ArrayBuffer!");
        }
        return result;
    }

    get type() { return this._type; }
    get isPointer() { return this._isPointer; }
    get buffer() { return this._buffer; }
    get byteLength() { return this._byteLength; }
}

class Harness {
    constructor()
    {
        this._shaderHeader = `#include <metal_stdlib>
        using namespace metal;
        `;
    }

    _initialize(callback)
    {
        callback.bind(this)();
    }

    /**
     * Return the return value of a WHLSL function.
     * @param {Types} type - The return type of the WHLSL function.
     * @param {String} functions - Custom WHLSL code to be tested.
     * @param {String} name - The name of the WHLSL function which must be present in 'functions'.
     * @param {Data or Array[Data]} args - Data arguments to be passed to the call of 'name'.
     * @returns {TypedArray} - A typed array containing the return value of the function call.
     */
    async callTypedFunction(type, functions, name, args)
    {   
        const [argsLayouts, argsResourceBindings, argsStructCode, functionCallArgs] = this._setUpArguments(args);

        if (!this._resultBuffer) {
            this._resultBuffer = this._device.createBuffer({ 
                size: Types.MAX_SIZE, 
                usage: GPUBufferUsage.STORAGE | GPUBufferUsage.MAP_READ 
            });
        }

        argsLayouts.unshift({
            binding: 0,
            visibility: GPUShaderStageBit.COMPUTE,
            type: "storage-buffer"
        });
        argsResourceBindings.unshift({
            binding: 0,
            resource: {
                buffer: this._resultBuffer,
                size: Types.MAX_SIZE
            }
        });

        const code = this._shaderHeader + functions + `
        struct _compute_args {
            device ${convertTypeToWHLSLType(type)}* result [[id(0)]];
            ${argsStructCode}};
    
        kernel void _compute_main(device _compute_args& args [[buffer(0)]]) 
        {
            *args.result = ${name}(${functionCallArgs.join(", ")});
        }
        `;
    
        this._callFunction(code, argsLayouts, argsResourceBindings);
    
        try {
            var result = await this._resultBuffer.mapReadAsync();
        } catch {
            throw new Error("Harness error: Unable to read results!");
        }
        const array = new (convertTypeToArrayType(type))(result);
        this._resultBuffer.unmap();

        return array;
    }

    /**
     * Call a WHLSL function to modify the value of argument(buffer)s.
     * @param {String} functions - Custom WHLSL code to be tested.
     * @param {String} name - The name of the WHLSL function which must be present in 'functions'.
     * @param {Data or Array[Data]} args - Data arguments to be passed to the call of 'name'.
     */
    async callVoidFunction(functions, name, args)
    {
        const [argsLayouts, argsResourceBindings, argsStructCode, functionCallArgs] = this._setUpArguments(args);

        const code = this._shaderHeader + functions + `
        struct _compute_args {
            ${argsStructCode}};

        kernel void _compute_main(device _compute_args& args [[buffer(0)]])
        {
            ${name}(${functionCallArgs.join(", ")});
        }
        `;

        this._callFunction(code, argsLayouts, argsResourceBindings);
    }

    _setUpArguments(args)
    {
        if (!Array.isArray(args)) {
            if (args instanceof Data)
                args = [args];
            else if (!args)
                args = [];
        }

        // FIXME: Replace with WHLSL.
        // Expand bind group structure to represent any arguments.
        let argsStructCode = "";
        let functionCallArgs = [];
        let argsLayouts = [];
        let argsResourceBindings = [];

        for (let i = 1; i <= args.length; ++i) {
            const arg = args[i - 1];
            argsStructCode += `device ${convertTypeToWHLSLType(arg.type)}* arg${i} [[id(${i})]];
            `;
            const optionalDeref = (!arg.isPointer) ? "*" : "";
            functionCallArgs.push(optionalDeref + `args.arg${i}`);
            argsLayouts.push({
                binding: i,
                visibility: GPUShaderStageBit.COMPUTE,
                type: "storage-buffer"
            });
            argsResourceBindings.push({
                binding: i,
                resource: {
                    buffer: arg.buffer,
                    size: arg.byteLength
                }
            });
        }

        return [argsLayouts, argsResourceBindings, argsStructCode, functionCallArgs];
    }

    _callFunction(code, argsLayouts, argsResourceBindings)
    {
        const shaders = this._device.createShaderModule({ code: code });
        // FIXME: Compile errors should be caught and reported here.
        const pipeline = this._device.createComputePipeline({
            computeStage: {
                module: shaders,
                entryPoint: "_compute_main"
            }
        });

        const layout = this._device.createBindGroupLayout({
            bindings: argsLayouts
        });
    
        const bindGroup = this._device.createBindGroup({
            layout: layout,
            bindings: argsResourceBindings
        });
        
        const commandEncoder = this._device.createCommandEncoder();
        const passEncoder = commandEncoder.beginComputePass();
        passEncoder.setBindGroup(0, bindGroup);
        passEncoder.setPipeline(pipeline);
        passEncoder.dispatch(1, 1, 1);
        passEncoder.endPass();
        
        this._device.getQueue().submit([commandEncoder.finish()]);
    }
}

/* Harness Setup */

const harness = new Harness();
harness._initialize(async () => {
    try {
        const adapter = await navigator.gpu.requestAdapter();
        harness._device = await adapter.requestDevice();
    } catch (e) {
        throw new Error("Harness error: Unable to acquire GPUDevice!");
    }
});

/* Global Helper Functions */

/**
 * The make___ functions are wrappers around the Data constructor.
 * Values passed in as an array will be passed in via a device-addressed pointer type in the shader.
 * @param {Boolean, Number, or Array} values - The data to be stored on the GPU.
 * @returns A new Data object with storage allocated to store values.
 */
function makeBool(values)
{
    return new Data(harness, Types.BOOL, values);
}

function makeInt(values)
{
    return new Data(harness, Types.INT, values);
}

function makeUchar(values)
{
    return new Data(harness, Types.UCHAR, values);
}

function makeUint(values)
{
    return new Data(harness, Types.UINT, values);
}

function makeFloat(values)
{
    return new Data(harness, Types.FLOAT, values);
}

/**
 * @param {Array or Array[Array]} values - 1D or 2D array of float values.
 * The total number of float values must be divisible by 4.
 * A single 4-element array of floats will be treated as a single float4 argument in the shader.
 */
function makeFloat4(values)
{
    const originalLength = values.length;
    // This works because float4 is tightly packed.
    // When implementing other vector types, add padding if needed.
    values = values.flat();
    if (values.length % 4 != 0)
        throw new Error("makeFloat4: Invalid number of elements!");
    return new Data(harness, Types.FLOAT4, values, originalLength === 1 || values.length > 4);
}

/**
 * @param {String} functions - Shader source code that must contain a definition for 'name'.
 * @param {String} name - The function to be called from 'functions'.
 * @param {Data or Array[Data]} args - The arguments to be passed to the call of 'name'.
 * @returns A Promise that resolves to the return value of a call to 'name' with 'args'.
 */
async function callBoolFunction(functions, name, args)
{
    return !!(await harness.callTypedFunction(Types.BOOL, functions, name, args))[0];
}

async function callIntFunction(functions, name, args)
{
    return (await harness.callTypedFunction(Types.INT, functions, name, args))[0];
}

async function callUcharFunction(functions, name, args)
{
    return (await harness.callTypedFunction(Types.UCHAR, functions, name, args))[0];
}

async function callUintFunction(functions, name, args)
{
    return (await harness.callTypedFunction(Types.UINT, functions, name, args))[0];
}

async function callFloatFunction(functions, name, args)
{
    return (await harness.callTypedFunction(Types.FLOAT, functions, name, args))[0];
}

async function callFloat4Function(functions, name, args)
{
    return (await harness.callTypedFunction(Types.FLOAT4, functions, name, args)).subarray(0, 4);
}

/**
 * Does not return a Promise. To observe the results of a call, 
 * call 'getArrayBuffer' on the Data object retaining your output buffer.
 */
function callVoidFunction(functions, name, args)
{
    harness.callVoidFunction(functions, name, args);
}