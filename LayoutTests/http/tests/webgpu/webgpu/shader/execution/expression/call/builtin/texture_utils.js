/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/import { assert, range, unreachable } from '../../../../../../common/util/util.js';import { float32ToUint32 } from '../../../../../util/conversion.js';
import { align, clamp, hashU32, lerp, quantizeToF32 } from '../../../../../util/math.js';
import {
  kTexelRepresentationInfo } from


'../../../../../util/texture/texel_data.js';
import { TexelView } from '../../../../../util/texture/texel_view.js';
import { createTextureFromTexelView } from '../../../../../util/texture.js';
import { reifyExtent3D } from '../../../../../util/unions.js';

function getLimitValue(v) {
  switch (v) {
    case Number.POSITIVE_INFINITY:
      return 1000;
    case Number.NEGATIVE_INFINITY:
      return -1000;
    default:
      return v;
  }
}

function getValueBetweenMinAndMaxTexelValueInclusive(
rep,
normalized)
{
  return lerp(
    getLimitValue(rep.numericRange.min),
    getLimitValue(rep.numericRange.max),
    normalized
  );
}

/**
 * Creates a TexelView filled with random values.
 */
export function createRandomTexelView(info)


{
  const rep = kTexelRepresentationInfo[info.format];
  const generator = (coords) => {
    const texel = {};
    for (const component of rep.componentOrder) {
      const rnd = hashU32(coords.x, coords.y, coords.z, component.charCodeAt(0));
      const normalized = clamp(rnd / 0xffffffff, { min: 0, max: 1 });
      texel[component] = getValueBetweenMinAndMaxTexelValueInclusive(rep, normalized);
    }
    return quantize(texel, rep);
  };
  return TexelView.fromTexelsAsColors(info.format, generator);
}







const kTextureCallArgNames = [
'coords',
'mipLevel',
'arrayIndex',
'ddx',
'ddy',
'offset'];
















function toArray(coords) {
  if (coords instanceof Array) {
    return coords;
  }
  return [coords];
}

function quantize(texel, repl) {
  return repl.bitsToNumber(repl.unpackBits(new Uint8Array(repl.pack(repl.encode(texel)))));
}

function apply(a, b, op) {
  assert(a.length === b.length, `apply(${a}, ${b}): arrays must have same length`);
  return a.map((v, i) => op(v, b[i]));
}

const add = (a, b) => apply(a, b, (x, y) => x + y);






/**
 * Returns the expect value for a WGSL builtin texture function
 */
export function expected(
call,
texture,
sampler)
{
  const rep = kTexelRepresentationInfo[texture.texels.format];
  const textureExtent = reifyExtent3D(texture.descriptor.size);
  const textureSize = [textureExtent.width, textureExtent.height, textureExtent.depthOrArrayLayers];
  const addressMode = [
  sampler.addressModeU ?? 'clamp-to-edge',
  sampler.addressModeV ?? 'clamp-to-edge',
  sampler.addressModeW ?? 'clamp-to-edge'];


  const load = (at) =>
  texture.texels.color({
    x: Math.floor(at[0]),
    y: Math.floor(at[1] ?? 0),
    z: Math.floor(at[2] ?? 0)
  });

  switch (call.builtin) {
    case 'textureSample':{
        const coords = toArray(call.coords);

        // convert normalized to absolute texel coordinate
        // ┌───┬───┬───┬───┐
        // │ a │   │   │   │  norm: a = 1/8, b = 7/8
        // ├───┼───┼───┼───┤   abs: a = 0,   b = 3
        // │   │   │   │   │
        // ├───┼───┼───┼───┤
        // │   │   │   │   │
        // ├───┼───┼───┼───┤
        // │   │   │   │ b │
        // └───┴───┴───┴───┘
        let at = coords.map((v, i) => v * textureSize[i] - 0.5);

        // Apply offset in whole texel units
        if (call.offset !== undefined) {
          at = add(at, toArray(call.offset));
        }

        const samples = [];

        const filter = sampler.minFilter;
        switch (filter) {
          case 'linear':{
              // 'p0' is the lower texel for 'at'
              const p0 = at.map((v) => Math.floor(v));
              // 'p1' is the higher texel for 'at'
              const p1 = p0.map((v) => v + 1);

              // interpolation weights for p0 and p1
              const p1W = at.map((v, i) => v - p0[i]);
              const p0W = p1W.map((v) => 1 - v);

              switch (coords.length) {
                case 1:
                  samples.push({ at: p0, weight: p0W[0] });
                  samples.push({ at: p1, weight: p1W[0] });
                  break;
                case 2:{
                    samples.push({ at: p0, weight: p0W[0] * p0W[1] });
                    samples.push({ at: [p1[0], p0[1]], weight: p1W[0] * p0W[1] });
                    samples.push({ at: [p0[0], p1[1]], weight: p0W[0] * p1W[1] });
                    samples.push({ at: p1, weight: p1W[0] * p1W[1] });
                    break;
                  }
              }
              break;
            }
          case 'nearest':{
              const p = at.map((v) => Math.round(quantizeToF32(v)));
              samples.push({ at: p, weight: 1 });
              break;
            }
          default:
            unreachable();
        }

        const out = {};
        const ss = [];
        for (const sample of samples) {
          // Apply sampler address mode
          const c = sample.at.map((v, i) => {
            switch (addressMode[i]) {
              case 'clamp-to-edge':
                return clamp(v, { min: 0, max: textureSize[i] - 1 });
              case 'mirror-repeat':{
                  const n = Math.floor(v / textureSize[i]);
                  v = v - n * textureSize[i];
                  return (n & 1) !== 0 ? textureSize[i] - v - 1 : v;
                }
              case 'repeat':
                return v - Math.floor(v / textureSize[i]) * textureSize[i];
              default:
                unreachable();
            }
          });
          const v = load(c);
          ss.push(v);
          for (const component of rep.componentOrder) {
            out[component] = (out[component] ?? 0) + v[component] * sample.weight;
          }
        }

        return out;
      }
    case 'textureLoad':{
        return load(toArray(call.coords));
      }
  }
}

/**
 * Puts random data in a texture, generates a shader that implements `calls`
 * such that each call's result is written to the next consecutive texel of
 * a rgba32float texture. It then checks the result of each call matches
 * the expected result.
 */
export async function putDataInTextureThenDrawAndCheckResults(
device,
texture,
sampler,
calls)
{
  const results = await doTextureCalls(device, texture, sampler, calls);
  const errs = [];
  const rep = kTexelRepresentationInfo[texture.texels.format];
  for (let callIdx = 0; callIdx < calls.length; callIdx++) {
    const call = calls[callIdx];
    const got = results[callIdx];
    const expect = expected(call, texture, sampler);

    const gULP = rep.bitsToULPFromZero(rep.numberToBits(got));
    const eULP = rep.bitsToULPFromZero(rep.numberToBits(expect));
    for (const component of rep.componentOrder) {
      const g = got[component];
      const e = expect[component];
      const absDiff = Math.abs(g - e);
      const ulpDiff = Math.abs(gULP[component] - eULP[component]);
      const relDiff = absDiff / Math.max(Math.abs(g), Math.abs(e));
      if (ulpDiff > 3 && relDiff > 0.03) {
        const desc = describeTextureCall(call);
        errs.push(`component was not as expected:
      call: ${desc}
 component: ${component}
       got: ${g}
  expected: ${e}
  abs diff: ${absDiff.toFixed(4)}
  rel diff: ${(relDiff * 100).toFixed(2)}%
  ulp diff: ${ulpDiff}
  sample points:
`);
        const expectedSamplePoints = [
        'expected:',
        ...(await identifySamplePoints(texture.descriptor, (texels) => {
          return Promise.resolve(
            expected(call, { texels, descriptor: texture.descriptor }, sampler)
          );
        }))];

        const gotSamplePoints = [
        'got:',
        ...(await identifySamplePoints(
          texture.descriptor,
          async (texels) =>
          (
          await doTextureCalls(device, { texels, descriptor: texture.descriptor }, sampler, [
          call]
          ))[
          0]
        ))];

        errs.push(layoutTwoColumns(expectedSamplePoints, gotSamplePoints).join('\n'));
        errs.push('', '');
      }
    }
  }

  return errs.length > 0 ? new Error(errs.join('\n')) : undefined;
}

/**
 * Generates a text art grid showing which texels were sampled
 * followed by a list of the samples and the weights used for each
 * component.
 *
 * Example:
 *
 *     0   1   2   3   4   5   6   7
 *   ┌───┬───┬───┬───┬───┬───┬───┬───┐
 * 0 │   │   │   │   │   │   │   │   │
 *   ├───┼───┼───┼───┼───┼───┼───┼───┤
 * 1 │   │   │   │   │   │   │   │ a │
 *   ├───┼───┼───┼───┼───┼───┼───┼───┤
 * 2 │   │   │   │   │   │   │   │ b │
 *   ├───┼───┼───┼───┼───┼───┼───┼───┤
 * 3 │   │   │   │   │   │   │   │   │
 *   ├───┼───┼───┼───┼───┼───┼───┼───┤
 * 4 │   │   │   │   │   │   │   │   │
 *   ├───┼───┼───┼───┼───┼───┼───┼───┤
 * 5 │   │   │   │   │   │   │   │   │
 *   ├───┼───┼───┼───┼───┼───┼───┼───┤
 * 6 │   │   │   │   │   │   │   │   │
 *   ├───┼───┼───┼───┼───┼───┼───┼───┤
 * 7 │   │   │   │   │   │   │   │   │
 *   └───┴───┴───┴───┴───┴───┴───┴───┘
 * a: at: [7, 1], weights: [R: 0.75000]
 * b: at: [7, 2], weights: [R: 0.25000]
 */
async function identifySamplePoints(
info,
run)
{
  const textureSize = reifyExtent3D(info.size);
  const numTexels = textureSize.width * textureSize.height;
  const rep = kTexelRepresentationInfo[info.format];

  // Identify all the texels that are sampled, and their weights.
  const sampledTexelWeights = new Map();
  const unclassifiedStack = [new Set(range(numTexels, (v) => v))];
  while (unclassifiedStack.length > 0) {
    // Pop the an unclassified texels stack
    const unclassified = unclassifiedStack.pop();

    // Split unclassified texels evenly into two new sets
    const setA = new Set();
    const setB = new Set();
    [...unclassified.keys()].forEach((t, i) => ((i & 1) === 0 ? setA : setB).add(t));

    // Push setB to the unclassified texels stack
    if (setB.size > 0) {
      unclassifiedStack.push(setB);
    }

    // See if any of the texels in setA were sampled.
    const results = await run(
      TexelView.fromTexelsAsColors(
        info.format,
        (coords) => {
          const isCandidate = setA.has(coords.x + coords.y * textureSize.width);
          const texel = {};
          for (const component of rep.componentOrder) {
            texel[component] = isCandidate ? 1 : 0;
          }
          return texel;
        }
      )
    );
    if (rep.componentOrder.some((c) => results[c] !== 0)) {
      // One or more texels of setA were sampled.
      if (setA.size === 1) {
        // We identified a specific texel was sampled.
        // As there was only one texel in the set, results holds the sampling weights.
        setA.forEach((texel) => sampledTexelWeights.set(texel, results));
      } else {
        // More than one texel in the set. Needs splitting.
        unclassifiedStack.push(setA);
      }
    }
  }

  // ┌───┬───┬───┬───┐
  // │ a │   │   │   │
  // ├───┼───┼───┼───┤
  // │   │   │   │   │
  // ├───┼───┼───┼───┤
  // │   │   │   │   │
  // ├───┼───┼───┼───┤
  // │   │   │   │ b │
  // └───┴───┴───┴───┘
  const letter = (idx) => String.fromCharCode(97 + idx); // 97: 'a'
  const orderedTexelIndices = [];
  const lines = [];
  {
    let line = '  ';
    for (let x = 0; x < textureSize.width; x++) {
      line += `  ${x} `;
    }
    lines.push(line);
  }
  {
    let line = '  ┌';
    for (let x = 0; x < textureSize.width; x++) {
      line += x === textureSize.width - 1 ? '───┐' : '───┬';
    }
    lines.push(line);
  }
  for (let y = 0; y < textureSize.height; y++) {
    {
      let line = `${y} │`;
      for (let x = 0; x < textureSize.width; x++) {
        const texelIdx = x + y * textureSize.height;
        const weight = sampledTexelWeights.get(texelIdx);
        if (weight !== undefined) {
          line += ` ${letter(orderedTexelIndices.length)} │`;
          orderedTexelIndices.push(texelIdx);
        } else {
          line += '   │';
        }
      }
      lines.push(line);
    }
    if (y < textureSize.height - 1) {
      let line = '  ├';
      for (let x = 0; x < textureSize.width; x++) {
        line += x === textureSize.width - 1 ? '───┤' : '───┼';
      }
      lines.push(line);
    }
  }
  {
    let line = '  └';
    for (let x = 0; x < textureSize.width; x++) {
      line += x === textureSize.width - 1 ? '───┘' : '───┴';
    }
    lines.push(line);
  }

  orderedTexelIndices.forEach((texelIdx, i) => {
    const weights = sampledTexelWeights.get(texelIdx);
    const y = Math.floor(texelIdx / textureSize.width);
    const x = texelIdx - y * textureSize.height;
    const w = rep.componentOrder.map((c) => `${c}: ${weights[c]?.toFixed(5)}`).join(', ');
    lines.push(`${letter(i)}: at: [${x}, ${y}], weights: [${w}]`);
  });
  return lines;
}

function layoutTwoColumns(columnA, columnB) {
  const widthA = Math.max(...columnA.map((l) => l.length));
  const lines = Math.max(columnA.length, columnB.length);
  const out = new Array(lines);
  for (let line = 0; line < lines; line++) {
    const a = columnA[line] ?? '';
    const b = columnB[line] ?? '';
    out[line] = `${a}${' '.repeat(widthA - a.length)} | ${b}`;
  }
  return out;
}

export const kSamplePointMethods = ['texel-centre', 'spiral'];


/**
 * Generates an array of coordinates at which to sample a texture.
 */
export function generateSamplePoints(
n,
nearest,
args)












{
  const out = [];
  switch (args.method) {
    case 'texel-centre':{
        for (let i = 0; i < n; i++) {
          const r = hashU32(i);
          const x = Math.floor(lerp(0, args.textureWidth - 1, (r & 0xffff) / 0xffff)) + 0.5;
          const y = Math.floor(lerp(0, args.textureHeight - 1, (r >>> 16) / 0xffff)) + 0.5;
          out.push([x / args.textureWidth, y / args.textureHeight]);
        }
        break;
      }
    case 'spiral':{
        for (let i = 0; i < n; i++) {
          const f = i / (Math.max(n, 2) - 1);
          const r = (args.radius ?? 1.5) * f;
          const a = (args.loops ?? 2) * 2 * Math.PI * f;
          out.push([0.5 + r * Math.cos(a), 0.5 + r * Math.sin(a)]);
        }
        break;
      }
  }
  // Samplers across devices use different methods to interpolate.
  // Quantizing the texture coordinates seems to hit coords that produce
  // comparable results to our computed results.
  // Note: This value works with 8x8 textures. Other sizes have not been tested.
  // Values that worked for reference:
  // Win 11, NVidia 2070 Super: 16
  // Linux, AMD Radeon Pro WX 3200: 256
  // MacOS, M1 Mac: 256
  const kSubdivisionsPerTexel = 4;
  const q = [args.textureWidth * kSubdivisionsPerTexel, args.textureHeight * kSubdivisionsPerTexel];
  return out.map(
    (c) =>
    c.map((v, i) => {
      // Quantize to kSubdivisionsPerPixel
      const v1 = Math.floor(v * q[i]);
      // If it's nearest and we're on the edge of a texel then move us off the edge
      // since the edge could choose one texel or another in nearest mode
      const v2 = nearest && v1 % kSubdivisionsPerTexel === 0 ? v1 + 1 : v1;
      // Convert back to texture coords
      return v2 / q[i];
    })
  );
}

function wgslTypeFor(data, type) {
  if (data instanceof Array) {
    switch (data.length) {
      case 2:
        return `vec2${type}`;
      case 3:
        return `vec3${type}`;
    }
  }
  return '${type}32';
}

function wgslExpr(data) {
  if (data instanceof Array) {
    switch (data.length) {
      case 2:
        return `vec2(${data.map((v) => v.toString()).join(', ')})`;
      case 3:
        return `vec3(${data.map((v) => v.toString()).join(', ')})`;
    }
  }
  return data.toString();
}

function binKey(call) {
  const keys = [];
  for (const name of kTextureCallArgNames) {
    const value = call[name];
    if (value !== undefined) {
      if (name === 'offset') {
        // offset must be a constant expression
        keys.push(`${name}: ${wgslExpr(value)}`);
      } else {
        keys.push(`${name}: ${wgslTypeFor(value, call.coordType)}`);
      }
    }
  }
  return `${call.builtin}(${keys.join(', ')})`;
}

function buildBinnedCalls(calls) {
  const args = ['T']; // All texture builtins take the texture as the first argument
  const fields = [];
  const data = [];

  const prototype = calls[0];
  if (prototype.builtin.startsWith('textureSample')) {
    // textureSample*() builtins take a sampler as the second argument
    args.push('S');
  }

  for (const name of kTextureCallArgNames) {
    const value = prototype[name];
    if (value !== undefined) {
      if (name === 'offset') {
        args.push(`/* offset */ ${wgslExpr(value)}`);
      } else {
        args.push(`args.${name}`);
        fields.push(`@align(16) ${name} : ${wgslTypeFor(value, prototype.coordType)}`);
      }
    }
  }

  for (const call of calls) {
    for (const name of kTextureCallArgNames) {
      const value = call[name];
      assert(
        prototype[name] === undefined === (value === undefined),
        'texture calls are not binned correctly'
      );
      if (value !== undefined && name !== 'offset') {
        const bitcastToU32 = (value) => {
          if (calls[0].coordType === 'f') {
            return float32ToUint32(value);
          }
          return value;
        };
        if (value instanceof Array) {
          for (const c of value) {
            data.push(bitcastToU32(c));
          }
        } else {
          data.push(bitcastToU32(value));
        }
        // All fields are aligned to 16 bytes.
        while ((data.length & 3) !== 0) {
          data.push(0);
        }
      }
    }
  }

  const expr = `${prototype.builtin}(${args.join(', ')})`;

  return { expr, fields, data };
}

function binCalls(calls) {
  const map = new Map(); // key to bin index
  const bins = [];
  calls.forEach((call, callIdx) => {
    const key = binKey(call);
    const binIdx = map.get(key);
    if (binIdx === undefined) {
      map.set(key, bins.length);
      bins.push([callIdx]);
    } else {
      bins[binIdx].push(callIdx);
    }
  });
  return bins;
}

export function describeTextureCall(call) {
  const args = ['texture: T'];
  if (call.builtin.startsWith('textureSample')) {
    args.push('sampler: S');
  }
  for (const name of kTextureCallArgNames) {
    const value = call[name];
    if (value !== undefined) {
      args.push(`${name}: ${wgslExpr(value)}`);
    }
  }
  return `${call.builtin}(${args.join(', ')})`;
}

/**
 * Given a list of "calls", each one of which has a texture coordinate,
 * generates a fragment shader that uses the fragment position as an index
 * (position.y * 256 + position.x) That index is then used to look up a
 * coordinate from a storage buffer which is used to call the WGSL texture
 * function to read/sample the texture, and then write to an rgba32float
 * texture.  We then read the rgba32float texture for the per "call" results.
 *
 * Calls are "binned" by call parameters. Each bin has its own structure and
 * field in the storage buffer. This allows the calls to be non-homogenous and
 * each have their own data type for coordinates.
 */
export async function doTextureCalls(
device,
texture,
sampler,
calls)
{
  let structs = '';
  let body = '';
  let dataFields = '';
  const data = [];
  let callCount = 0;
  const binned = binCalls(calls);
  binned.forEach((binCalls, binIdx) => {
    const b = buildBinnedCalls(binCalls.map((callIdx) => calls[callIdx]));
    structs += `struct Args${binIdx} {
  ${b.fields.join(',  \n')}
}
`;
    dataFields += `  args${binIdx} : array<Args${binIdx}, ${binCalls.length}>,
`;
    body += `
  {
    let is_active = (frag_idx >= ${callCount}) & (frag_idx < ${callCount + binCalls.length});
    let args = data.args${binIdx}[frag_idx - ${callCount}];
    let call = ${b.expr};
    result = select(result, call, is_active);
  }
`;
    callCount += binCalls.length;
    data.push(...b.data);
  });

  const dataBuffer = device.createBuffer({
    size: data.length * 4,
    usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.STORAGE
  });
  device.queue.writeBuffer(dataBuffer, 0, new Uint32Array(data));

  const rtWidth = 256;
  const renderTarget = device.createTexture({
    format: 'rgba32float',
    size: { width: rtWidth, height: Math.ceil(calls.length / rtWidth) },
    usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT
  });

  const code = `
${structs}

struct Data {
${dataFields}
}

@vertex
fn vs_main(@builtin(vertex_index) vertex_index : u32) -> @builtin(position) vec4f {
  let positions = array(
    vec4f(-1,  1, 0, 1), vec4f( 1,  1, 0, 1),
    vec4f(-1, -1, 0, 1), vec4f( 1, -1, 0, 1),
  );
  return positions[vertex_index];
}

@group(0) @binding(0) var          T    : texture_2d<f32>;
@group(0) @binding(1) var          S    : sampler;
@group(0) @binding(2) var<storage> data : Data;

@fragment
fn fs_main(@builtin(position) frag_pos : vec4f) -> @location(0) vec4f {
  let frag_idx = u32(frag_pos.x) + u32(frag_pos.y) * ${renderTarget.width};
  var result : vec4f;
${body}
  return result;
}
`;
  const shaderModule = device.createShaderModule({ code });

  const pipeline = device.createRenderPipeline({
    layout: 'auto',
    vertex: { module: shaderModule, entryPoint: 'vs_main' },
    fragment: {
      module: shaderModule,
      entryPoint: 'fs_main',
      targets: [{ format: renderTarget.format }]
    },
    primitive: { topology: 'triangle-strip', cullMode: 'none' }
  });

  const gpuTexture = createTextureFromTexelView(device, texture.texels, texture.descriptor);
  const gpuSampler = device.createSampler(sampler);

  const bindGroup = device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: [
    { binding: 0, resource: gpuTexture.createView() },
    { binding: 1, resource: gpuSampler },
    { binding: 2, resource: { buffer: dataBuffer } }]

  });

  const bytesPerRow = align(16 * renderTarget.width, 256);
  const resultBuffer = device.createBuffer({
    size: renderTarget.height * bytesPerRow,
    usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ
  });
  const encoder = device.createCommandEncoder();

  const renderPass = encoder.beginRenderPass({
    colorAttachments: [{ view: renderTarget.createView(), loadOp: 'clear', storeOp: 'store' }]
  });

  renderPass.setPipeline(pipeline);
  renderPass.setBindGroup(0, bindGroup);
  renderPass.draw(4);
  renderPass.end();
  encoder.copyTextureToBuffer(
    { texture: renderTarget },
    { buffer: resultBuffer, bytesPerRow },
    { width: renderTarget.width, height: renderTarget.height }
  );
  device.queue.submit([encoder.finish()]);

  await resultBuffer.mapAsync(GPUMapMode.READ);

  const view = TexelView.fromTextureDataByReference(
    renderTarget.format,
    new Uint8Array(resultBuffer.getMappedRange()),
    {
      bytesPerRow,
      rowsPerImage: renderTarget.height,
      subrectOrigin: [0, 0, 0],
      subrectSize: [renderTarget.width, renderTarget.height]
    }
  );

  let outIdx = 0;
  const out = new Array(calls.length);
  for (const bin of binned) {
    for (const callIdx of bin) {
      const x = outIdx % rtWidth;
      const y = Math.floor(outIdx / rtWidth);
      out[callIdx] = view.color({ x, y, z: 0 });
      outIdx++;
    }
  }

  renderTarget.destroy();
  gpuTexture.destroy();
  resultBuffer.destroy();

  return out;
}