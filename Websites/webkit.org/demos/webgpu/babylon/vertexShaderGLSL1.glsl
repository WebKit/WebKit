#version 450

precision highp float;

layout(std140,column_major) uniform;

layout(set = 0, binding = 0) uniform Scene {
    mat4 viewProjection;
    mat4 view;
};

layout(set = 1, binding = 0) uniform Material {
    vec2 vAlbedoInfos;
    vec4 vAmbientInfos;
    vec2 vOpacityInfos;
    vec2 vEmissiveInfos;
    vec2 vLightmapInfos;
    vec3 vReflectivityInfos;
    vec2 vMicroSurfaceSamplerInfos;
    vec2 vReflectionInfos;
    vec3 vReflectionPosition;
    vec3 vReflectionSize;
    vec3 vBumpInfos;
    mat4 albedoMatrix;
    mat4 ambientMatrix;
    mat4 opacityMatrix;
    mat4 emissiveMatrix;
    mat4 lightmapMatrix;
    mat4 reflectivityMatrix;
    mat4 microSurfaceSamplerMatrix;
    mat4 bumpMatrix;
    vec2 vTangentSpaceParams;
    mat4 reflectionMatrix;
    vec3 vReflectionColor;
    vec4 vAlbedoColor;
    vec4 vLightingIntensity;
    vec3 vReflectionMicrosurfaceInfos;
    float pointSize;
    vec4 vReflectivityColor;
    vec3 vEmissiveColor;
    vec4 vEyePosition;
    vec3 vAmbientColor;
    vec2 vDebugMode;
    vec2 vClearCoatParams;
    vec4 vClearCoatRefractionParams;
    vec2 vClearCoatInfos;
    mat4 clearCoatMatrix;
    vec2 vClearCoatBumpInfos;
    vec2 vClearCoatTangentSpaceParams;
    mat4 clearCoatBumpMatrix;
    vec4 vClearCoatTintParams;
    float clearCoatColorAtDistance;
    vec2 vClearCoatTintInfos;
    mat4 clearCoatTintMatrix;
    vec3 vAnisotropy;
    vec2 vAnisotropyInfos;
    mat4 anisotropyMatrix;
    vec4 vSheenColor;
    vec2 vSheenInfos;
    mat4 sheenMatrix;
    vec3 vRefractionMicrosurfaceInfos;
    vec4 vRefractionInfos;
    mat4 refractionMatrix;
    vec2 vThicknessInfos;
    mat4 thicknessMatrix;
    vec2 vThicknessParam;
    vec3 vDiffusionDistance;
    vec4 vTintColor;
    vec3 vSubSurfaceIntensity;
    vec3 vSphericalL00;
    vec3 vSphericalL1_1;
    vec3 vSphericalL10;
    vec3 vSphericalL11;
    vec3 vSphericalL2_2;
    vec3 vSphericalL2_1;
    vec3 vSphericalL20;
    vec3 vSphericalL21;
    vec3 vSphericalL22;
    vec3 vSphericalX;
    vec3 vSphericalY;
    vec3 vSphericalZ;
    vec3 vSphericalXX_ZZ;
    vec3 vSphericalYY_ZZ;
    vec3 vSphericalZZ;
    vec3 vSphericalXY;
    vec3 vSphericalYZ;
    vec3 vSphericalZX;
};

layout(set = 1, binding = 1) uniform Mesh {
    mat4 world;
    float visibility;
};

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

mat3 transposeMat3(mat3 inMatrix) {
    vec3 i0 = inMatrix[0];
    vec3 i1 = inMatrix[1];
    vec3 i2 = inMatrix[2];
    mat3 outMatrix = mat3(
        vec3(i0.x, i1.x, i2.x),
        vec3(i0.y, i1.y, i2.y),
        vec3(i0.z, i1.z, i2.z)
    );
    return outMatrix;
}

mat3 inverseMat3(mat3 inMatrix) {
    float a00 = inMatrix[0][0], a01 = inMatrix[0][1], a02 = inMatrix[0][2];
    float a10 = inMatrix[1][0], a11 = inMatrix[1][1], a12 = inMatrix[1][2];
    float a20 = inMatrix[2][0], a21 = inMatrix[2][1], a22 = inMatrix[2][2];
    float b01 = a22 * a11 - a12 * a21;
    float b11 = -a22 * a10 + a12 * a20;
    float b21 = a21 * a10 - a11 * a20;
    float det = a00 * b01 + a01 * b11 + a02 * b21;
    return mat3(b01, -a22 * a01 + a02 * a21, a12 * a01 - a02 * a11,
        b11, a22 * a00 - a02 * a20, -a12 * a00 + a02 * a10,
        b21, -a21 * a00 + a01 * a20, a11 * a00 - a01 * a10) / det;
}

vec3 toLinearSpace(vec3 color) {
    return pow(color, vec3(2.2));
}

vec3 toGammaSpace(vec3 color) {
    return pow(color, vec3(1.0 / 2.2));
}

float square(float value) {
    return value * value;
}

float pow5(float value) {
    float sq = value * value;
    return sq * sq * value;
}

float getLuminance(vec3 color) {
    return clamp(dot(color, vec3(0.2126, 0.7152, 0.0722)), 0., 1.);
}

float getRand(vec2 seed) {
    return fract(sin(dot(seed.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

float dither(vec2 seed, float varianceAmount) {
    float rand = getRand(seed);
    float dither = mix(-varianceAmount / 255.0, varianceAmount / 255.0, rand);
    return dither;
}

vec4 toRGBD(vec3 color) {
    float maxRGB = max(max(color.r, max(color.g, color.b)), 0.0000001);
    float D = max(255.0 / maxRGB, 1.);
    D = clamp(floor(D) / 255.0, 0., 1.);
    vec3 rgb = color.rgb * D;
    rgb = toGammaSpace(rgb);
    return vec4(rgb, D);
}

vec3 fromRGBD(vec4 rgbd) {
    rgbd.rgb = toLinearSpace(rgbd.rgb);
    return rgbd.rgb / rgbd.a;
}

layout(location = 0) out vec3 vPositionW;
layout(location = 1) out vec3 vNormalW;
layout(location = 2) out vec3 vEnvironmentIrradiance;

vec3 computeEnvironmentIrradiance(vec3 normal) {
    return vSphericalL00
        + vSphericalL1_1 * (normal.y)
        + vSphericalL10 * (normal.z)
        + vSphericalL11 * (normal.x)
        + vSphericalL2_2 * (normal.y * normal.x)
        + vSphericalL2_1 * (normal.y * normal.z)
        + vSphericalL20 * ((3.0 * normal.z * normal.z) - 1.0)
        + vSphericalL21 * (normal.z * normal.x)
        + vSphericalL22 * (normal.x * normal.x - (normal.y * normal.y));
}

void main(void) {
    vec3 positionUpdated = position;
    vec3 normalUpdated = normal;
    mat4 finalWorld = world;
    gl_Position = viewProjection * finalWorld * vec4(positionUpdated, 1.0);
    vec4 worldPos = finalWorld * vec4(positionUpdated, 1.0);
    vPositionW = vec3(worldPos);
    mat3 normalWorld = mat3(finalWorld);
    vNormalW = normalize(normalWorld * normalUpdated);
    vec3 reflectionVector = vec3(reflectionMatrix * vec4(vNormalW, 0)).xyz;
    vEnvironmentIrradiance = computeEnvironmentIrradiance(reflectionVector);
    vec2 uvUpdated = vec2(0., 0.);
    vec2 uv2 = vec2(0., 0.);
    gl_Position.y *= -1.;
}
