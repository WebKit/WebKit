if (!navigator.gpu || GPUBufferUsage.COPY_SRC === undefined)
    document.body.className = "error";

const isChrome = navigator.userAgent.indexOf("Chrome") >= 0;

const numParticles = 2;
const numThreadsInThreadGroup = 2;
const numThreadGroups = numParticles / numThreadsInThreadGroup;

const computeShaderGLSL = `#version 450
struct Particle {
    vec2 pos;
    vec2 vel;
};

layout(std140, set = 0, binding = 0) uniform SimParams {
    float deltaT;
    float rule1Distance;
    float rule2Distance;
    float rule3Distance;
    float rule1Scale;
    float rule2Scale;
    float rule3Scale;
} params;

layout(std140, set = 0, binding = 1) buffer ParticlesA {
    Particle particles[${numParticles}];
} particlesA;

layout(std140, set = 0, binding = 2) buffer ParticlesB {
    Particle particles[${numParticles}];
} particlesB;

layout(local_size_x = ${numThreadsInThreadGroup}, local_size_y = 1, local_size_z = 1) in;

void main() {

    // https://github.com/austinEng/Project6-Vulkan-Flocking/blob/master/data/shaders/computeparticles/particle.comp

    uint index = gl_GlobalInvocationID.x;
    if (index >= ${numParticles}) {
        return;
    }

    vec2 vPos = particlesA.particles[index].pos;
    vec2 vVel = particlesA.particles[index].vel;

    vec2 cMass = vec2(0.0, 0.0);
    vec2 cVel = vec2(0.0, 0.0);
    vec2 colVel = vec2(0.0, 0.0);
    int cMassCount = 0;
    int cVelCount = 0;

    //float rand = ${Math.random()};
    float rand = 0;

    vec2 pos;
    vec2 vel;
    for (int i = 0; i < ${numParticles}; ++i) {
        if (i == index) { continue; }
        pos = particlesA.particles[i].pos.xy;
        vel = particlesA.particles[i].vel.xy;

        if (distance(pos, vPos) < params.rule1Distance) {
            cMass += pos;
            cMassCount++;
        }
        if (distance(pos, vPos) < params.rule2Distance) {
            colVel -= (pos - vPos);
        }
        if (distance(pos, vPos) < params.rule3Distance) {
            cVel += vel;
            cVelCount++;
        }
    }
    if (cMassCount > 0) {
        cMass = cMass / cMassCount - vPos;
    }
    if (cVelCount > 0) {
        cVel = cVel / cVelCount;
    }

    vVel += cMass * params.rule1Scale + colVel * params.rule2Scale + cVel * params.rule3Scale;

    // clamp velocity for a more pleasing simulation.
    vVel = normalize(vVel) * clamp(length(vVel), 0.0, 0.1);

    // kinematic update
    vPos += vVel * params.deltaT;

    // Wrap around boundary
    if (vPos.x < -1.0) vPos.x = 1.0;
    if (vPos.x > 1.0) vPos.x = -1.0;
    if (vPos.y < -1.0) vPos.y = 1.0;
    if (vPos.y > 1.0) vPos.y = -1.0;

    particlesB.particles[index].pos = vPos;
    //particlesB.particles[index].vel = vVel;
    particlesB.particles[index].vel = vVel + rand;
}
`;

const computeShaderWHLSL = `
struct Particle {
    float2 pos;
    float2 vel;
}

struct SimParams {
    float deltaT;
    float rule1Distance;
    float rule2Distance;
    float rule3Distance;
    float rule1Scale;
    float rule2Scale;
    float rule3Scale;
}

[numthreads(${numThreadsInThreadGroup}, 1, 1)]
compute void main(device SimParams[] paramsBuffer : register(u0), device Particle[] particlesA : register(u1), device Particle[] particlesB : register(u2), float3 threadID : SV_DispatchThreadID) {
    uint index = uint(threadID.x);

    SimParams params = paramsBuffer[0];

    if (index >= ${numParticles}) {
        return;
    }

    float2 vPos = particlesA[index].pos;
    float2 vVel = particlesA[index].vel;

    float2 cMass = float2(0.0, 0.0);
    float2 cVel = float2(0.0, 0.0);
    float2 colVel = float2(0.0, 0.0);
    float cMassCount = 0.0;
    float cVelCount = 0.0;

    //float rand = 25;

    float2 pos;
    float2 vel;
    for (uint i = 0; i < ${numParticles}; ++i) {
        if (i == index) { continue; }
        pos = particlesA[i].pos.xy;
        vel = particlesA[i].vel.xy;

        if (distance(pos, vPos) < params.rule1Distance) {
            cMass += pos;
            cMassCount++;
        }
        if (distance(pos, vPos) < params.rule2Distance) {
            colVel -= (pos - vPos);
        }
        if (distance(pos, vPos) < params.rule3Distance) {
            cVel += vel;
            cVelCount++;
        }
    }
    if (cMassCount > 0.0) {
        cMass = cMass / cMassCount - vPos;
    }
    if (cVelCount > 0.0) {
        cVel = cVel / cVelCount;
    }

    vVel += cMass * params.rule1Scale + colVel * params.rule2Scale + cVel * params.rule3Scale;

    // clamp velocity for a more pleasing simulation.
    vVel = normalize(vVel) * clamp(length(vVel), 0.0, 0.1);

    // kinematic update
    vPos += vVel * params.deltaT;

    // Wrap around boundary
    if (vPos.x < -1.0) vPos.x = 1.0;
    if (vPos.x > 1.0) vPos.x = -1.0;
    if (vPos.y < -1.0) vPos.y = 1.0;
    if (vPos.y > 1.0) vPos.y = -1.0;

    particlesB[index].pos = vPos;
    particlesB[index].vel = vVel;
}
`;

async function setData(buffer, data)
{
    if (isChrome) {
        buffer.setSubData(0, data);
        return;
    }

    const bufferArrayBuffer = await buffer.mapWriteAsync();
    const array = new Float32Array(bufferArrayBuffer);
    for (let i = 0; i < array.length; ++i)
        array[i] = data[i];
    buffer.unmap();
}

function appendMessage(msg) {
    let d = document.getElementById("results");
    let p = document.createElement('p');
    p.innerHTML = msg;
    d.append(p);
}

async function init() {
    if (!isChrome) {
        GPUBufferUsage.COPY_DST = GPUBufferUsage.COPY_DST;
        GPUBufferUsage.COPY_SRC = GPUBufferUsage.COPY_SRC;
    }

    const adapter = await navigator.gpu.requestAdapter();
    const device = await adapter.requestDevice();
    await Utils.ready;

    const canvas = document.querySelector('canvas');
    const context = canvas.getContext('gpupresent');

    let computeBindGroupLayout;
    if (isChrome) {
        computeBindGroupLayout = device.createBindGroupLayout({
            bindings: [
                { binding: 0, visibility: GPUShaderStage.COMPUTE, type: "uniform-buffer" },
                { binding: 1, visibility: GPUShaderStage.COMPUTE, type: "storage-buffer" },
                { binding: 2, visibility: GPUShaderStage.COMPUTE, type: "storage-buffer" },
            ],
        });
    } else {
        computeBindGroupLayout = device.createBindGroupLayout({
            bindings: [
                { binding: 0, visibility: GPUShaderStage.COMPUTE, type: "storage-buffer" },
                { binding: 1, visibility: GPUShaderStage.COMPUTE, type: "storage-buffer" },
                { binding: 2, visibility: GPUShaderStage.COMPUTE, type: "storage-buffer" },
            ],
        });
    }

    const computePipelineLayout = device.createPipelineLayout({
        bindGroupLayouts: [computeBindGroupLayout],
    });

    let computeCode;

    if (isChrome)
        computeCode = Utils.compile("c", computeShaderGLSL);
    else
        computeCode = computeShaderWHLSL;

    const startTime = Date.now();

    const computePipeline = device.createComputePipeline({
        layout: computePipelineLayout,
        computeStage: {
            module: device.createShaderModule({
                code: computeCode,
                isWHLSL: isChrome ? false : true,
            }),
            entryPoint: "main",
        }
    });

    const simParamData = new Float32Array([0.04, 0.1, 0.025, 0.025, 0.02, 0.05, 0.005]);
    let simParamBuffer
    if (isChrome) {
        simParamBuffer = device.createBuffer({
            size: simParamData.byteLength,
            usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
        });
    } else {
        simParamBuffer = device.createBuffer({
            size: simParamData.byteLength,
            usage: GPUBufferUsage.MAP_WRITE | GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST,
        });
    }
    await setData(simParamBuffer, simParamData);

    const initialParticleData = new Float32Array(numParticles * 4);
    for (let i = 0; i < numParticles; ++i) {
        //initialParticleData[4 * i + 0] = 2 * (Math.random() - 0.5);
        //initialParticleData[4 * i + 1] = 2 * (Math.random() - 0.5);
        //initialParticleData[4 * i + 2] = 2 * (Math.random() - 0.5) * 0.1;
        //initialParticleData[4 * i + 3] = 2 * (Math.random() - 0.5) * 0.1;
        initialParticleData[4 * i + 0] = 5;
        initialParticleData[4 * i + 1] = 5;
        initialParticleData[4 * i + 2] = 5;
        initialParticleData[4 * i + 3] = 5;
    }

    const particleBuffers = new Array(2);
    particleBuffers[0] = device.createBuffer({
        size: initialParticleData.byteLength,
        usage: (!isChrome ? GPUBufferUsage.MAP_WRITE : 0) | GPUBufferUsage.COPY_DST | GPUBufferUsage.STORAGE,
    });

    if (isChrome) {
        particleBuffers[1] = device.createBuffer({
            size: initialParticleData.byteLength,
            usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC,
        });
    } else {
        particleBuffers[1] = device.createBuffer({
            size: initialParticleData.byteLength,
            usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
        });
    }

    await setData(particleBuffers[0], initialParticleData);

    const bindGroup = device.createBindGroup({
        layout: computeBindGroupLayout,
        bindings: [{
            binding: 0,
            resource: {
                buffer: simParamBuffer,
                offset: 0,
                size: simParamData.byteLength
            },
        }, {
            binding: 1,
            resource: {
                buffer: particleBuffers[0],
                offset: 0,
                size: initialParticleData.byteLength,
            },
        }, {
            binding: 2,
            resource: {
                buffer: particleBuffers[1],
                offset: 0,
                size: initialParticleData.byteLength,
            },
        }],
    });

    async function foo() {
        let result = device.createBuffer({
            size: initialParticleData.byteLength,
            usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
        });
        const commandEncoder = device.createCommandEncoder({});
        const passEncoder = commandEncoder.beginComputePass();
        passEncoder.setPipeline(computePipeline);
        passEncoder.setBindGroup(0, bindGroup);
        passEncoder.dispatch(numThreadGroups, 1, 1);
        passEncoder.endPass();

        if (isChrome)
            commandEncoder.copyBufferToBuffer(particleBuffers[1], 0, result, 0, initialParticleData.byteLength);

        device.getQueue().submit([commandEncoder.finish()]);

        let resultsArrayBuffer;
        if (isChrome) {
            resultsArrayBuffer = await result.mapReadAsync();
        } else {
            resultsArrayBuffer = await particleBuffers[1].mapReadAsync();
        }

        appendMessage(`total time: ${Date.now() - startTime} ms`);
        const resultsArray = new Float32Array(resultsArrayBuffer);

        //{
        //    try {
        //        const error = await device.popErrorScope();
        //        if (error)
        //            appendMessage("Error: " + error);
        //    } catch(e) {
        //        console.log(e);
        //    }
        //}

        try {
            if (resultsArray.length !== numParticles*4)
                throw new Error("Bad length");
            for (let i = 0; i < resultsArray.length; i += 4) {
                if (resultsArray[i] !== -1)
                    throw new Error("not -1, is: " + resultsArray[i]);
                if (resultsArray[i + 1] !== -1)
                    throw new Error("not -1, is: " + resultsArray[i + 1]);
                if (resultsArray[i + 2] !== 0.0707106813788414)
                    throw new Error("not fraction");
                if (resultsArray[i + 3] !== 0.0707106813788414)
                    throw new Error("not fraction");
            }

            appendMessage("Success.");
        } catch (e) {
            appendMessage(` error: ${e}`);
        }
    }

    foo();
}

window.onload = init;
