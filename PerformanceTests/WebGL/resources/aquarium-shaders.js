/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// The GLSL ES 1.00 shaders of the WebGL Aquarium, from aquarium/aquarium.html in
// https://github.com/WebGLSamples/WebGLSamples.github.io at 425fa919b1abf5ba0824bcffa6d711ce6c6f3d9e.
// The shader sources are unmodified.

const aquariumShaders = {
    fishVertexShader: `uniform vec3 lightWorldPos;
uniform mat4 viewInverse;
uniform mat4 viewProjection;
uniform vec3 worldPosition;
uniform vec3 nextPosition;
uniform float scale;
uniform float time;
uniform float fishLength;
uniform float fishWaveLength;
uniform float fishBendAmount;
attribute vec4 position;
attribute vec3 normal;
attribute vec2 texCoord;
attribute vec3 tangent;  // #normalMap
attribute vec3 binormal;  // #normalMap
varying vec4 v_position;
varying vec2 v_texCoord;
varying vec3 v_tangent;  // #normalMap
varying vec3 v_binormal;  // #normalMap
varying vec3 v_normal;
varying vec3 v_surfaceToLight;
varying vec3 v_surfaceToView;
void main() {
  vec3 vz = normalize(worldPosition - nextPosition);
  vec3 vx = normalize(cross(vec3(0,1,0), vz));
  vec3 vy = cross(vz, vx);
  mat4 orientMat = mat4(
    vec4(vx, 0),
    vec4(vy, 0),
    vec4(vz, 0),
    vec4(worldPosition, 1));
  mat4 scaleMat = mat4(
    vec4(scale, 0, 0, 0),
    vec4(0, scale, 0, 0),
    vec4(0, 0, scale, 0),
    vec4(0, 0, 0, 1));
  mat4 world = orientMat * scaleMat;
  mat4 worldInverseTranspose = world;

  v_texCoord = texCoord;
  // NOTE:If you change this you need to change the laser code to match!
  float mult = position.z > 0.0 ?
      (position.z / fishLength) :
      (-position.z / fishLength * 2.0);
  float s = sin(time + mult * fishWaveLength);
  float a = sign(s);
  float offset = pow(mult, 2.0) * s * fishBendAmount;
  v_position = (
      viewProjection * world *
      (position +
       vec4(offset, 0, 0, 0)));
  v_normal = (worldInverseTranspose * vec4(normal, 0)).xyz;
  v_surfaceToLight = lightWorldPos - (world * position).xyz;
  v_surfaceToView = (viewInverse[3] - (world * position)).xyz;
  v_binormal = (worldInverseTranspose * vec4(binormal, 0)).xyz;  // #normalMap
  v_tangent = (worldInverseTranspose * vec4(tangent, 0)).xyz;  // #normalMap
  gl_Position = v_position;
}

`,
    fishNormalMapFragmentShader: `#ifdef GL_FRAGMENT_PRECISION_HIGH
  precision highp float;
#else
  precision mediump float;
#endif

uniform vec4 lightColor;
varying vec4 v_position;
varying vec2 v_texCoord;
varying vec3 v_tangent;  // #normalMap
varying vec3 v_binormal;  // #normalMap
varying vec3 v_normal;
varying vec3 v_surfaceToLight;
varying vec3 v_surfaceToView;

uniform vec4 ambient;
uniform sampler2D diffuse;
uniform vec4 specular;
uniform sampler2D normalMap;  // #normalMap
uniform float shininess;
uniform float specularFactor;
// #fogUniforms

vec4 lit(float l ,float h, float m) {
  return vec4(1.0,
              max(l, 0.0),
              (l > 0.0) ? pow(max(0.0, h), m) : 0.0,
              1.0);
}
void main() {
  vec4 diffuseColor = texture2D(diffuse, v_texCoord);
  mat3 tangentToWorld = mat3(v_tangent,  // #normalMap
                             v_binormal,  // #normalMap
                             v_normal);  // #normalMap
  vec4 normalSpec = texture2D(normalMap, v_texCoord.xy);  // #normalMap
  vec4 normalSpec = vec4(0,0,0,0);  // #noNormalMap
  vec3 tangentNormal = normalSpec.xyz - vec3(0.5, 0.5, 0.5);  // #normalMap
  tangentNormal = normalize(tangentNormal + vec3(0, 0, 2));  // #normalMap
  vec3 normal = (tangentToWorld * tangentNormal);  // #normalMap
  normal = normalize(normal);  // #normalMap
  vec3 normal = normalize(v_normal);   // #noNormalMap
  vec3 surfaceToLight = normalize(v_surfaceToLight);
  vec3 surfaceToView = normalize(v_surfaceToView);
  vec3 halfVector = normalize(surfaceToLight + surfaceToView);
  vec4 litR = lit(dot(normal, surfaceToLight),
                    dot(normal, halfVector), shininess);
  vec4 outColor = vec4(
    (lightColor * (diffuseColor * litR.y + diffuseColor * ambient +
                  specular * litR.z * specularFactor * normalSpec.a)).rgb,
      diffuseColor.a);
  // #fogCode
  gl_FragColor = outColor;
}
`,
    fishReflectionFragmentShader: `#ifdef GL_FRAGMENT_PRECISION_HIGH
  precision highp float;
#else
  precision mediump float;
#endif

uniform vec4 lightColor;
varying vec4 v_position;
varying vec2 v_texCoord;
varying vec3 v_tangent;  // #normalMap
varying vec3 v_binormal;  // #normalMap
varying vec3 v_normal;
varying vec3 v_surfaceToLight;
varying vec3 v_surfaceToView;

uniform vec4 ambient;
uniform sampler2D diffuse;
uniform vec4 specular;
uniform sampler2D normalMap;
uniform sampler2D reflectionMap; // #reflection
uniform samplerCube skybox; // #reflecton
uniform float shininess;
uniform float specularFactor;
// #fogUniforms

vec4 lit(float l ,float h, float m) {
  return vec4(1.0,
              max(l, 0.0),
              (l > 0.0) ? pow(max(0.0, h), m) : 0.0,
              1.0);
}
void main() {
  vec4 diffuseColor = texture2D(diffuse, v_texCoord);
  mat3 tangentToWorld = mat3(v_tangent,  // #normalMap
                             v_binormal,  // #normalMap
                             v_normal);  // #normalMap
  vec4 normalSpec = texture2D(normalMap, v_texCoord.xy);  // #normalMap
  vec4 normalSpec = vec4(0,0,0,0);  // #noNormalMap
  vec4 reflection = texture2D(reflectionMap, v_texCoord.xy); // #reflection
  vec4 reflection = vec4(0,0,0,0);  // #noReflection
  vec3 tangentNormal = normalSpec.xyz - vec3(0.5, 0.5, 0.5);  // #normalMap
  vec3 normal = (tangentToWorld * tangentNormal);  // #normalMap
  normal = normalize(normal);  // #normalMap
  vec3 normal = normalize(v_normal); // #noNormalMap
  vec3 surfaceToLight = normalize(v_surfaceToLight);
  vec3 surfaceToView = normalize(v_surfaceToView);
  vec4 skyColor = textureCube(skybox, -reflect(surfaceToView, normal));  // #reflection
  vec4 skyColor = vec4(0.5,0.5,1,1);  // #noReflection

  vec3 halfVector = normalize(surfaceToLight + surfaceToView);
  vec4 litR = lit(dot(normal, surfaceToLight),
                    dot(normal, halfVector), shininess);
  vec4 outColor = vec4(mix(
      skyColor,
      lightColor * (diffuseColor * litR.y + diffuseColor * ambient +
                    specular * litR.z * specularFactor * normalSpec.a),
      1.0 - reflection.r).rgb,
      diffuseColor.a);
  // #fogCode
  gl_FragColor = outColor;
}
`,
    seaweedVertexShader: `uniform mat4 world;
uniform mat4 viewProjection;
uniform vec3 lightWorldPos;
uniform mat4 viewInverse;
uniform mat4 worldInverseTranspose;
uniform float time;
attribute vec4 position;
attribute vec3 normal;
attribute vec2 texCoord;
varying vec4 v_position;
varying vec2 v_texCoord;
varying vec3 v_normal;
varying vec3 v_surfaceToLight;
varying vec3 v_surfaceToView;
void main() {
  vec3 toCamera = normalize(viewInverse[3].xyz - world[3].xyz);
  vec3 yAxis = vec3(0, 1, 0);
  vec3 xAxis = cross(yAxis, toCamera);
  vec3 zAxis = cross(xAxis, yAxis);

  mat4 newWorld = mat4(
      vec4(xAxis, 0),
      vec4(yAxis, 0),
      vec4(xAxis, 0),
      world[3]);

  v_texCoord = texCoord;
  v_position = position + vec4(
      sin(time * 0.5) * pow(position.y * 0.07, 2.0) * 1.0,
      -4,  // TODO(gman): remove this hack
      0,
      0);
  v_position = (viewProjection * newWorld) * v_position;
  v_normal = (newWorld * vec4(normal, 0)).xyz;
  v_surfaceToLight = lightWorldPos - (world * position).xyz;
  v_surfaceToView = (viewInverse[3] - (world * position)).xyz;
  gl_Position = v_position;
}

`,
    seaweedFragmentShader: `#ifdef GL_FRAGMENT_PRECISION_HIGH
  precision highp float;
#else
  precision mediump float;
#endif

uniform vec4 lightColor;
varying vec4 v_position;
varying vec2 v_texCoord;
varying vec3 v_normal;
varying vec3 v_surfaceToLight;
varying vec3 v_surfaceToView;

uniform vec4 ambient;
uniform sampler2D diffuse;
uniform vec4 specular;
uniform float shininess;
uniform float specularFactor;
// #fogUniforms

vec4 lit(float l ,float h, float m) {
  return vec4(1.0,
              max(l, 0.0),
              (l > 0.0) ? pow(max(0.0, h), m) : 0.0,
              1.0);
}
void main() {
  vec4 diffuseColor = texture2D(diffuse, v_texCoord);
  if (diffuseColor.a < 0.3) {
    discard;
  }
  vec3 normal = normalize(v_normal);
  vec3 surfaceToLight = normalize(v_surfaceToLight);
  vec3 surfaceToView = normalize(v_surfaceToView);
  vec3 halfVector = normalize(surfaceToLight + surfaceToView);
  vec4 litR = lit(dot(normal, surfaceToLight),
                    dot(normal, halfVector), shininess);
  vec4 outColor = vec4((
  lightColor * (diffuseColor * litR.y + diffuseColor * ambient +
                specular * litR.z * specularFactor)).rgb,
      diffuseColor.a);
  // #fogCode
  gl_FragColor = outColor;
}
`,
    diffuseVertexShader: `uniform mat4 viewProjection;
uniform vec3 lightWorldPos;
uniform mat4 world;
uniform mat4 viewInverse;
uniform mat4 worldInverseTranspose;
attribute vec4 position;
attribute vec3 normal;
attribute vec2 texCoord;
varying vec4 v_position;
varying vec2 v_texCoord;
varying vec3 v_normal;
varying vec3 v_surfaceToLight;
varying vec3 v_surfaceToView;
void main() {
  v_texCoord = texCoord;
  v_position = (viewProjection * world * position);
  v_normal = (worldInverseTranspose * vec4(normal, 0)).xyz;
  v_surfaceToLight = lightWorldPos - (world * position).xyz;
  v_surfaceToView = (viewInverse[3] - (world * position)).xyz;
  gl_Position = v_position;
}

`,
    diffuseFragmentShader: `#ifdef GL_FRAGMENT_PRECISION_HIGH
  precision highp float;
#else
  precision mediump float;
#endif

uniform vec4 lightColor;
varying vec4 v_position;
varying vec2 v_texCoord;
varying vec3 v_normal;
varying vec3 v_surfaceToLight;
varying vec3 v_surfaceToView;

uniform vec4 ambient;
uniform sampler2D diffuse;
uniform vec4 specular;
uniform float shininess;
uniform float specularFactor;
// #fogUniforms

vec4 lit(float l ,float h, float m) {
  return vec4(1.0,
              max(l, 0.0),
              (l > 0.0) ? pow(max(0.0, h), m) : 0.0,
              1.0);
}
void main() {
  vec4 diffuseColor = texture2D(diffuse, v_texCoord);
  vec3 normal = normalize(v_normal);
  vec3 surfaceToLight = normalize(v_surfaceToLight);
  vec3 surfaceToView = normalize(v_surfaceToView);
  vec3 halfVector = normalize(surfaceToLight + surfaceToView);
  vec4 litR = lit(dot(normal, surfaceToLight),
                    dot(normal, halfVector), shininess);
  vec4 outColor = vec4((
  lightColor * (diffuseColor * litR.y + diffuseColor * ambient +
                specular * litR.z * specularFactor)).rgb,
      diffuseColor.a);
  // #fogCode
  gl_FragColor = outColor;
}
`,
    normalMapVertexShader: `uniform mat4 viewProjection;
uniform vec3 lightWorldPos;
uniform mat4 world;
uniform mat4 viewInverse;
uniform mat4 worldInverseTranspose;
attribute vec4 position;
attribute vec3 normal;
attribute vec2 texCoord;
attribute vec3 tangent;  // #normalMap
attribute vec3 binormal;  // #normalMap
varying vec4 v_position;
varying vec2 v_texCoord;
varying vec3 v_tangent;  // #normalMap
varying vec3 v_binormal;  // #normalMap
varying vec3 v_normal;
varying vec3 v_surfaceToLight;
varying vec3 v_surfaceToView;
void main() {
  v_texCoord = texCoord;
  v_position = (viewProjection * world * position);
  v_normal = (worldInverseTranspose * vec4(normal, 0)).xyz;
  v_surfaceToLight = lightWorldPos - (world * position).xyz;
  v_surfaceToView = (viewInverse[3] - (world * position)).xyz;
  v_binormal = (worldInverseTranspose * vec4(binormal, 0)).xyz;  // #normalMap
  v_tangent = (worldInverseTranspose * vec4(tangent, 0)).xyz;  // #normalMap
  gl_Position = v_position;
}

`,
    normalMapFragmentShader: `#ifdef GL_FRAGMENT_PRECISION_HIGH
  precision highp float;
#else
  precision mediump float;
#endif

uniform vec4 lightColor;
varying vec4 v_position;
varying vec2 v_texCoord;
varying vec3 v_tangent;  // #normalMap
varying vec3 v_binormal;  // #normalMap
varying vec3 v_normal;
varying vec3 v_surfaceToLight;
varying vec3 v_surfaceToView;

uniform vec4 ambient;
uniform sampler2D diffuse;
uniform vec4 specular;
uniform sampler2D normalMap;  // #normalMap
uniform float shininess;
uniform float specularFactor;
// #fogUniforms

vec4 lit(float l ,float h, float m) {
  return vec4(1.0,
              max(l, 0.0),
              (l > 0.0) ? pow(max(0.0, h), m) : 0.0,
              1.0);
}
void main() {
  vec4 diffuseColor = texture2D(diffuse, v_texCoord);
  mat3 tangentToWorld = mat3(v_tangent,  // #normalMap
                             v_binormal,  // #normalMap
                             v_normal);  // #normalMap
  vec4 normalSpec = texture2D(normalMap, v_texCoord.xy);  // #normalMap
  vec4 normalSpec = vec4(0,0,0,0);  // #noNormalMap
  vec3 tangentNormal = normalSpec.xyz -  // #normalMap
                                 vec3(0.5, 0.5, 0.5);  // #normalMap
  vec3 normal = (tangentToWorld * tangentNormal);  // #normalMap
  normal = normalize(normal);  // #normalMap
  vec3 normal = normalize(v_normal);   // #noNormalMap
  vec3 surfaceToLight = normalize(v_surfaceToLight);
  vec3 surfaceToView = normalize(v_surfaceToView);
  vec3 halfVector = normalize(surfaceToLight + surfaceToView);
  vec4 litR = lit(dot(normal, surfaceToLight),
                    dot(normal, halfVector), shininess);
  vec4 outColor = vec4(
     (lightColor * (diffuseColor * litR.y + diffuseColor * ambient +
                    specular * litR.z * specularFactor * normalSpec.a)).rgb,
      diffuseColor.a);
  // #fogCode
  gl_FragColor = outColor;
}
`,
    reflectionMapVertexShader: `uniform mat4 viewProjection;
uniform vec3 lightWorldPos;
uniform mat4 world;
uniform mat4 viewInverse;
uniform mat4 worldInverseTranspose;
attribute vec4 position;
attribute vec3 normal;
attribute vec2 texCoord;
attribute vec3 tangent;
attribute vec3 binormal;
varying vec4 v_position;
varying vec2 v_texCoord;
varying vec3 v_tangent;
varying vec3 v_binormal;
varying vec3 v_normal;
varying vec3 v_surfaceToLight;
varying vec3 v_surfaceToView;
void main() {
  v_texCoord = texCoord;
  v_position = (viewProjection * world * position);
  v_normal = (worldInverseTranspose * vec4(normal, 0)).xyz;
  v_surfaceToLight = lightWorldPos - (world * position).xyz;
  v_surfaceToView = (viewInverse[3] - (world * position)).xyz;
  v_binormal = (worldInverseTranspose * vec4(binormal, 0)).xyz;
  v_tangent = (worldInverseTranspose * vec4(tangent, 0)).xyz;
  gl_Position = v_position;
}

`,
    reflectionMapFragmentShader: `#ifdef GL_FRAGMENT_PRECISION_HIGH
  precision highp float;
#else
  precision mediump float;
#endif

uniform vec4 lightColor;
varying vec4 v_position;
varying vec2 v_texCoord;
varying vec3 v_tangent;
varying vec3 v_binormal;
varying vec3 v_normal;
varying vec3 v_surfaceToLight;
varying vec3 v_surfaceToView;

uniform vec4 ambient;
uniform sampler2D diffuse;
uniform vec4 specular;
uniform sampler2D normalMap;
uniform sampler2D reflectionMap;
uniform samplerCube skybox;
uniform float shininess;
uniform float specularFactor;
// #fogUniforms

vec4 lit(float l ,float h, float m) {
  return vec4(1.0,
              max(l, 0.0),
              (l > 0.0) ? pow(max(0.0, h), m) : 0.0,
              1.0);
}
void main() {
  vec4 diffuseColor = texture2D(diffuse, v_texCoord);
  mat3 tangentToWorld = mat3(v_tangent,
                             v_binormal,
                             v_normal);
  vec4 normalSpec = texture2D(normalMap, v_texCoord.xy);
  vec4 reflection = texture2D(reflectionMap, v_texCoord.xy);
  vec3 tangentNormal = normalSpec.xyz - vec3(0.5, 0.5, 0.5);
  vec3 normal = (tangentToWorld * tangentNormal);
  normal = normalize(normal);
  vec3 surfaceToLight = normalize(v_surfaceToLight);
  vec3 surfaceToView = normalize(v_surfaceToView);
  vec4 skyColor = textureCube(skybox, -reflect(surfaceToView, normal));
  vec3 halfVector = normalize(surfaceToLight + surfaceToView);
  vec4 litR = lit(dot(normal, surfaceToLight),
                    dot(normal, halfVector), shininess);
  vec4 outColor = vec4(mix(
      skyColor,
      lightColor * (diffuseColor * litR.y + diffuseColor * ambient +
                    specular * litR.z * specularFactor * normalSpec.a),
      1.0 - reflection.r).rgb,
      diffuseColor.a);
  // #fogCode
  gl_FragColor = outColor;
}
`,
    innerRefractionMapVertexShader: `uniform mat4 viewProjection;
uniform vec3 lightWorldPos;
uniform mat4 world;
uniform mat4 viewInverse;
uniform mat4 worldInverseTranspose;
attribute vec4 position;
attribute vec3 normal;
attribute vec2 texCoord;
attribute vec3 tangent;
attribute vec3 binormal;
varying vec4 v_position;
varying vec2 v_texCoord;
varying vec3 v_tangent;  // #normalMap
varying vec3 v_binormal;  // #normalMap
varying vec3 v_normal;
varying vec3 v_surfaceToLight;
varying vec3 v_surfaceToView;
void main() {
  v_texCoord = texCoord;
  v_position = (viewProjection * world * position);
  v_normal = (worldInverseTranspose * vec4(normal, 0)).xyz;
  v_surfaceToLight = lightWorldPos - (world * position).xyz;
  v_surfaceToView = (viewInverse[3] - (world * position)).xyz;
  v_binormal = (worldInverseTranspose * vec4(binormal, 0)).xyz;  // #normalMap
  v_tangent = (worldInverseTranspose * vec4(tangent, 0)).xyz;  // #normalMap
  gl_Position = v_position;
}

`,
    innerRefractionMapFragmentShader: `#ifdef GL_FRAGMENT_PRECISION_HIGH
  precision highp float;
#else
  precision mediump float;
#endif

uniform vec4 lightColor;
varying vec4 v_position;
varying vec2 v_texCoord;
varying vec3 v_tangent;  // #normalMap
varying vec3 v_binormal;  // #normalMap
varying vec3 v_normal;
varying vec3 v_surfaceToLight;
varying vec3 v_surfaceToView;

uniform sampler2D diffuse;
uniform vec4 specular;
uniform sampler2D normalMap;  // #normalMap
uniform sampler2D reflectionMap;
uniform samplerCube skybox;
uniform float shininess;
uniform float specularFactor;
uniform float refractionFudge;
uniform float eta;
uniform float tankColorFudge;
// #fogUniforms

vec4 lit(float l ,float h, float m) {
  return vec4(1.0,
              max(l, 0.0),
              (l > 0.0) ? pow(max(0.0, h), m) : 0.0,
              1.0);
}
void main() {
  vec4 diffuseColor = texture2D(diffuse, v_texCoord) +
      vec4(tankColorFudge, tankColorFudge, tankColorFudge, 1);
  mat3 tangentToWorld = mat3(v_tangent,  // #normalMap
                             v_binormal,  // #normalMap
                             v_normal);  // #normalMap
  vec4 normalSpec = texture2D(normalMap, v_texCoord.xy);  // #normalMap
  vec4 normalSpec = vec4(0,0,0,0);  // #noNormalMap
  vec4 refraction = texture2D(reflectionMap, v_texCoord.xy);
  vec3 tangentNormal = normalSpec.xyz - vec3(0.5, 0.5, 0.5);  // #normalMap
  tangentNormal = normalize(tangentNormal + vec3(0,0,refractionFudge));  // #normalMap
  vec3 normal = (tangentToWorld * tangentNormal);  // #normalMap
  normal = normalize(normal);  // #normalMap
  vec3 normal = normalize(v_normal);   // #noNormalMap

  vec3 surfaceToLight = normalize(v_surfaceToLight);
  vec3 surfaceToView = normalize(v_surfaceToView);

  vec3 refractionVec = refract(surfaceToView, normal, eta);

  vec4 skyColor = textureCube(skybox, refractionVec);

//  vec4 bumpSkyColor = textureCube(skybox, refractionVec);
//  vec4 nonBumpSkyColor = textureCube(
//      skybox,
//      refract(surfaceToView, normalize(v_normal), eta));
//  vec4 skyColor = mix(nonBumpSkyColor, bumpSkyColor, normalSpec.a);
  vec4 outColor = vec4(
      mix(skyColor * diffuseColor, diffuseColor, refraction.r).rgb,
      diffuseColor.a);
  // #fogCode
  gl_FragColor = outColor;
}
`,
    outerRefractionMapVertexShader: `uniform mat4 viewProjection;
uniform vec3 lightWorldPos;
uniform mat4 world;
uniform mat4 viewInverse;
uniform mat4 worldInverseTranspose;
attribute vec4 position;
attribute vec3 normal;
attribute vec2 texCoord;
attribute vec3 tangent;
attribute vec3 binormal;
varying vec4 v_position;
varying vec2 v_texCoord;
varying vec3 v_tangent;  // #normalMap
varying vec3 v_binormal;  // #normalMap
varying vec3 v_normal;
varying vec3 v_surfaceToLight;
varying vec3 v_surfaceToView;
void main() {
  v_texCoord = texCoord;
  v_position = (viewProjection * world * position);
  v_normal = (worldInverseTranspose * vec4(normal, 0)).xyz;
  v_surfaceToLight = lightWorldPos - (world * position).xyz;
  v_surfaceToView = (viewInverse[3] - (world * position)).xyz;
  v_binormal = (worldInverseTranspose * vec4(binormal, 0)).xyz;  // #normalMap
  v_tangent = (worldInverseTranspose * vec4(tangent, 0)).xyz;  // #normalMap
  gl_Position = v_position;
}

`,
    outerRefractionMapFragmentShader: `#ifdef GL_FRAGMENT_PRECISION_HIGH
  precision highp float;
#else
  precision mediump float;
#endif

uniform vec4 lightColor;
varying vec4 v_position;
varying vec2 v_texCoord;
varying vec3 v_tangent;  // #normalMap
varying vec3 v_binormal;  // #normalMap
varying vec3 v_normal;
varying vec3 v_surfaceToLight;
varying vec3 v_surfaceToView;

uniform sampler2D diffuse;
uniform vec4 specular;
uniform sampler2D normalMap;  // #normalMap
uniform sampler2D reflectionMap;
uniform samplerCube skybox;
uniform float shininess;
uniform float specularFactor;

vec4 lit(float l ,float h, float m) {
  return vec4(1.0,
              max(l, 0.0),
              (l > 0.0) ? pow(max(0.0, h), m) : 0.0,
              1.0);
}
void main() {
  vec4 diffuseColor = texture2D(diffuse, v_texCoord);
  mat3 tangentToWorld = mat3(v_tangent,  // #normalMap
                             v_binormal,  // #normalMap
                             v_normal);  // #normalMap
  vec4 normalSpec = texture2D(normalMap, v_texCoord.xy);  // #normalMap
  vec4 normalSpec = vec4(0,0,0,0);  // #noNormalMap
  vec4 reflection = texture2D(reflectionMap, v_texCoord.xy);
  vec3 tangentNormal = normalSpec.xyz - vec3(0.5, 0.5, 0.5);  // #normalMap
//  tangentNormal = normalize(tangentNormal + vec3(0,0,refractionFudge));
  vec3 normal = (tangentToWorld * tangentNormal);  // #normalMap
  normal = normalize(normal);  // #normalMap
  vec3 normal = normalize(v_normal);   // #noNormalMap

  vec3 surfaceToLight = normalize(v_surfaceToLight);
  vec3 surfaceToView = normalize(v_surfaceToView);

  vec4 skyColor = textureCube(skybox, -reflect(surfaceToView, normal));
  float fudgeAmount = 1.1;
  vec3 fudge = skyColor.rgb * vec3(fudgeAmount, fudgeAmount, fudgeAmount);
  float bright = min(1.0, fudge.r * fudge.g * fudge.b);
  vec4 reflectColor =
      mix(vec4(skyColor.rgb, bright),
          diffuseColor,
          (1.0 - reflection.r));
  float r = abs(dot(surfaceToView, normal));
  gl_FragColor = vec4(mix(
      skyColor,
      reflectColor,
      ((r + 0.3) * (reflection.r))).rgb, 1.0 - r);
}
`,
    laserVertexShader: `attribute vec4 position;
attribute vec2 texCoord;
varying vec2 v_texCoord;
uniform mat4 world;
uniform mat4 viewProjection;
void main() {
  v_texCoord = texCoord;
  gl_Position = (viewProjection * world * position);
}
`,
    laserFragmentShader: `precision mediump float;

varying vec2 v_texCoord;
uniform vec4 colorMult;
uniform sampler2D colorMap;
void main() {
  gl_FragColor = texture2D(colorMap, v_texCoord) * colorMult;
}
`,
    texVertexShader: `attribute vec4 position;
attribute vec2 texCoord;
varying vec2 v_texCoord;
uniform mat4 world;
uniform mat4 viewProjection;
void main() {
  v_texCoord = texCoord;
  gl_Position = (viewProjection * world * position);
}
`,
    texFragmentShader: `precision mediump float;

varying vec2 v_texCoord;
uniform vec4 colorMult;
uniform sampler2D colorMap;
void main() {
  gl_FragColor = texture2D(colorMap, v_texCoord) * colorMult;
}
`,
};

// The programs that the aquarium compiles with its default settings, as
// [vertex shader, fragment shader, fog].
const aquariumPrograms = [
    ["fishVertexShader", "fishReflectionFragmentShader", true],
    ["fishVertexShader", "fishNormalMapFragmentShader", true],
    ["diffuseVertexShader", "diffuseFragmentShader", true],
    ["diffuseVertexShader", "diffuseFragmentShader", false],
    ["normalMapVertexShader", "normalMapFragmentShader", true],
    ["reflectionMapVertexShader", "reflectionMapFragmentShader", true],
    ["innerRefractionMapVertexShader", "innerRefractionMapFragmentShader", true],
    ["outerRefractionMapVertexShader", "outerRefractionMapFragmentShader", true],
    ["seaweedVertexShader", "seaweedFragmentShader", true],
    ["laserVertexShader", "laserFragmentShader", false],
    ["texVertexShader", "texFragmentShader", false],
];

// The fragment shader substitutions of createProgramFromTags() in aquarium/aquarium.js, with
// reflections and normal maps enabled.
function aquariumFragmentShader(id, fog)
{
    let fs = aquariumShaders[id];
    if (fog) {
        fs = fs.replace('// #fogUniforms', 'uniform float fogPower;\nuniform float fogMult;\nuniform float fogOffset;\nuniform vec4 fogColor;\n');
        fs = fs.replace('// #fogCode', 'outColor = mix(outColor, vec4(fogColor.rgb, diffuseColor.a),\n   clamp(pow((v_position.z / v_position.w), fogPower) * fogMult - fogOffset,0.0,1.0));\n');
    }
    fs = fs.replace(/^.*?\/\/ #noReflection$/gm, "");
    fs = fs.replace(/^.*?\/\/ #noNormalMap$/gm, "");
    return fs;
}
