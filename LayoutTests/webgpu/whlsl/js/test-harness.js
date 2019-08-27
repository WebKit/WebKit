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
    FLOAT4X4: Symbol("float4x4"),
    MAX_SIZE: 64 // This needs to be big enough to hold any singular WHLSL type.
});

function isScalar(type)
{
    switch(type) {
        case Types.FLOAT4:
        case Types.FLOAT4X4:
            return false;
        default:
            return true;
    }
}

function convertTypeToArrayType(type)
{
    switch(type) {
        case Types.BOOL:
            return Int32Array;
        case Types.INT:
            return Int32Array;
        case Types.UCHAR:
            return Uint8Array;
        case Types.UINT:
            return Uint32Array;
        case Types.FLOAT:
        case Types.FLOAT4:
        case Types.FLOAT4X4:
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
        case Types.FLOAT4X4:
            return "float4x4";
        default:
            throw new Error("Invalid TYPE provided!");
    }
}

function whlslArgumentType(type)
{
    if (type === Types.BOOL)
        return "int";
    return convertTypeToWHLSLType(type);
}

function convertToWHLSLOutputType(code, type)
{
    if (type !== Types.BOOL)
        return code;
    return `int(${code})`;
}

function convertToWHLSLInputType(code, type)
{
    if (type !== Types.BOOL)
        return code;
    return `bool(${code})`;
}

/* Harness Classes */

class WebGPUUnsupportedError extends Error {
    constructor()
    {
        super("No GPUDevice detected!");
    }
};

class Data {
    /**
     * Upload typed data to and return a wrapper of a GPUBuffer.
     * @param {Types} type - The WHLSL type to be stored in this Data.
     * @param {Number or Array[Number]} values - The raw data to be uploaded.
     */
    constructor(harness, type, values, isBuffer = false)
    {
        if (harness.device === undefined)
            return;
        // One or more scalars in an array can be accessed through an array reference.
        // However, vector types are also created via an array of scalars.
        // This ensures that buffers of just one vector are usable in a test function.
        if (Array.isArray(values))
            this._isBuffer = isScalar(type) ? true : isBuffer;
        else {
            this._isBuffer = false;
            values = [values];
        }

        this._type = type;
        this._byteLength = (convertTypeToArrayType(type)).BYTES_PER_ELEMENT * values.length;

        const [buffer, arrayBuffer] = harness.device.createBufferMapped({
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
        if (harness.device === undefined)
            throw new WebGPUUnsupportedError();

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
    get isBuffer() { return this._isBuffer; }
    get buffer() { return this._buffer; }
    get byteLength() { return this._byteLength; }
}

class Harness {
    constructor ()
    {
    }

    async requestDevice()
    {
        try {
            const adapter = await navigator.gpu.requestAdapter();
            this._device = await adapter.requestDevice();
        } catch {
            // WebGPU is not supported.
            // FIXME: Add support for GPUAdapterRequestOptions and GPUDeviceDescriptor,
            // and differentiate between descriptor validation errors and no WebGPU support.
        }
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
        if (this._device === undefined)
            throw new WebGPUUnsupportedError();

        const [argsLayouts, argsResourceBindings, argsDeclarations, functionCallArgs] = this._setUpArguments(args);

        if (this._resultBuffer) {
            this._clearResults()
        } else {
            this._resultBuffer = this.device.createBuffer({ 
                size: Types.MAX_SIZE, 
                usage: GPUBufferUsage.STORAGE | GPUBufferUsage.MAP_READ | GPUBufferUsage.TRANSFER_DST
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

        let entryPointCode;
        argsDeclarations.unshift(`device ${whlslArgumentType(type)}[] result : register(u0)`);
        let callCode = `${name}(${functionCallArgs.join(", ")})`;
        callCode = convertToWHLSLOutputType(callCode, type);
        entryPointCode = `
[numthreads(1, 1, 1)]
compute void _compute_main(${argsDeclarations.join(", ")})
{
    result[0] = ${callCode};
}
`;
        const code = functions + entryPointCode;
        await this._callFunction(code, argsLayouts, argsResourceBindings);
    
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
    callVoidFunction(functions, name, args)
    {
        if (this._device === undefined)
            return;

        const [argsLayouts, argsResourceBindings, argsDeclarations, functionCallArgs] = this._setUpArguments(args);

        let entryPointCode = `
[numthreads(1, 1, 1)]
compute void _compute_main(${argsDeclarations.join(", ")})
{
    ${name}(${functionCallArgs.join(", ")});
}`;
        const code = functions + entryPointCode;
        this._callFunction(code, argsLayouts, argsResourceBindings);
    }

    /**
     * Assert that malformed shader code does not compile.
     * @param {String} source - Custom code to be tested.
     */
    async checkCompileFail(source)
    {
        if (this._device === undefined)
            return;
        
        let entryPointCode = `
[numthreads(1, 1, 1)]
compute void _compute_main() { }`;

        const code = source + entryPointCode;

        this._device.pushErrorScope("validation");
        
        const shaders = this._device.createShaderModule({ code: code });
    
        this._device.createComputePipeline({
            computeStage: {
                module: shaders,
                entryPoint: "_compute_main"
            }
        });

        const error = await this._device.popErrorScope();
        if (!error)
            throw new Error("Compiler error: shader code did not fail to compile!");
    }

    get device() { return this._device; }

    _clearResults()
    {
        if (!this._clearBuffer) {
            this._clearBuffer = this._device.createBuffer({ 
                size: Types.MAX_SIZE, 
                usage: GPUBufferUsage.TRANSFER_SRC
            });
        }
        const commandEncoder = this._device.createCommandEncoder();
        commandEncoder.copyBufferToBuffer(this._clearBuffer, 0, this._resultBuffer, 0, Types.MAX_SIZE);
        this._device.getQueue().submit([commandEncoder.finish()]);
    }

    _setUpArguments(args)
    {
        if (!Array.isArray(args)) {
            if (args instanceof Data)
                args = [args];
            else if (!args)
                args = [];
        }

        // Expand bind group structure to represent any arguments.
        let argsDeclarations = [];
        let functionCallArgs = [];
        let argsLayouts = [];
        let argsResourceBindings = [];

        for (let i = 1; i <= args.length; ++i) {
            const arg = args[i - 1];
            argsDeclarations.push(`device ${whlslArgumentType(arg.type)}[] arg${i} : register(u${i})`);
            functionCallArgs.push(convertToWHLSLInputType(`arg${i}` + (arg.isBuffer ? "" : "[0]"), arg.type));
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

        return [argsLayouts, argsResourceBindings, argsDeclarations, functionCallArgs];
    }

    async _callFunction(code, argsLayouts, argsResourceBindings)
    {
        const bindGroupLayout = this._device.createBindGroupLayout({
            bindings: argsLayouts
        });

        const pipelineLayout = this._device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

        const bindGroup = this._device.createBindGroup({
            layout: bindGroupLayout,
            bindings: argsResourceBindings
        });

        this._device.pushErrorScope("validation");

        const shaders = this._device.createShaderModule({ code: code });
    
        const pipeline = this._device.createComputePipeline({
            layout: pipelineLayout,
            computeStage: {
                module: shaders,
                entryPoint: "_compute_main"
            }
        });

        const commandEncoder = this._device.createCommandEncoder();
        const passEncoder = commandEncoder.beginComputePass();
        passEncoder.setBindGroup(0, bindGroup);
        passEncoder.setPipeline(pipeline);
        passEncoder.dispatch(1, 1, 1);
        passEncoder.endPass();
        
        this._device.getQueue().submit([commandEncoder.finish()]);

        const error = await this._device.popErrorScope();
        if (error)
            throw new Error(error.message);
    }
}

/* Harness Setup */

const harness = new Harness();
harness.requestDevice();

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
    const results = processArrays(values, 4);
    return new Data(harness, Types.FLOAT4, results.values, results.isBuffer);
}

/**
 * @param {Array or Array[Array]} values - 1D or 2D array of float values.
 * The total number of float values must be divisible by 16.
 * A single 16-element array of floats will be treated as a single float4x4 argument in the shader.
 * This should follow the glMatrix/OpenGL method of storing 4x4 matrices,
 * where the x, y, z translation components are the 13th, 14th, and 15th elements respectively.
 */
function makeFloat4x4(values)
{
    const results = processArrays(values, 16);
    return new Data(harness, Types.FLOAT4X4, results.values, results.isBuffer);
}

function processArrays(values, minimumLength)
{
    const originalLength = values.length;
    // This works because float4 is tightly packed.
    // When implementing other vector types, add padding if needed.
    values = values.flat();
    if (values.length % minimumLength != 0)
        throw new Error("Invalid number of elements in non-scalar type!");
    
    return { values: values, isBuffer: originalLength === 1 || values.length > minimumLength };
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

async function callFloat4x4Function(functions, name, args)
{
    return (await harness.callTypedFunction(Types.FLOAT4X4, functions, name, args)).subarray(0, 16);
}

async function checkFail(source) 
{
    return (await harness.checkCompileFail(source));
}

/**
 * Does not return a Promise. To observe the results of a call, 
 * call 'getArrayBuffer' on the Data object retaining your output buffer.
 */
function callVoidFunction(functions, name, args)
{
    harness.callVoidFunction(functions, name, args);
}

const webGPUPromiseTest = (testFunc, msg) => {
    promise_test(async () => { 
        return testFunc().catch(e => {
        if (!(e instanceof WebGPUUnsupportedError))
            throw e;
        });
    }, msg);
}

function runTests(obj) {
    window.addEventListener("load", async () => {
        try {
            for (const name in obj) {
                if (!name.startsWith("_")) 
                    await webGPUPromiseTest(obj[name], name);
            }
        } catch (e) {
            if (window.testRunner)
                testRunner.notifyDone();
            
            throw e;
        }
    });
}

