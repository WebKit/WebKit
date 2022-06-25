/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `WGSL execution test. Section: Float built-in functions`;
import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);

g.test('float_builtin_functions,acos')
  .uniqueId('3b55004d23fedacf')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
acos:
T is f32 or vecN<f32> acos(e: T ) -> T Returns the arc cosine of e. Component-wise when T is a vector. (GLSLstd450Acos)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,asin')
  .uniqueId('322c7c5ba84c257a')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
asin:
T is f32 or vecN<f32> asin(e: T ) -> T Returns the arc sine of e. Component-wise when T is a vector. (GLSLstd450Asin)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,cosh')
  .uniqueId('e4499ece6f25610d')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
cosh:
T is f32 or vecN<f32> cosh(e: T ) -> T Returns the hyperbolic cosine of e. Component-wise when T is a vector (GLSLstd450Cosh)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,vector_case_cross')
  .uniqueId('61356f087238c33c')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
vector case, cross:
T is f32 cross(e1: vec3<T> ,e2: vec3<T>) -> vec3<T> Returns the cross product of e1 and e2. (GLSLstd450Cross)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,distance')
  .uniqueId('a1459d94b9d23add')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
distance:
T is f32 or vecN<f32> distance(e1: T ,e2: T ) -> f32 Returns the distance between e1 and e2 (e.g. length(e1-e2)). (GLSLstd450Distance)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,exp')
  .uniqueId('ba1d78b3923e3ecc')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
exp:
T is f32 or vecN<f32> exp(e1: T ) -> T Returns the natural exponentiation of e1 (e.g. ee1). Component-wise when T is a vector. (GLSLstd450Exp)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,exp2')
  .uniqueId('335173647c18d7b0')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
exp2:
T is f32 or vecN<f32> exp2(e: T ) -> T Returns 2 raised to the power e (e.g. 2e). Component-wise when T is a vector. (GLSLstd450Exp2)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,faceForward')
  .uniqueId('ff98e4f5d2064a6f')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
faceForward:
T is vecN<f32> faceForward(e1: T ,e2: T ,e3: T ) -> T Returns e1 if dot(e2,e3) is negative, and -e1 otherwise. (GLSLstd450FaceForward)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,fma')
  .uniqueId('c6212635b880548b')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
fma:
T is f32 or vecN<f32> fma(e1: T ,e2: T ,e3: T ) -> T Returns e1 * e2 + e3. Component-wise when T is a vector. (GLSLstd450Fma)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,scalar_case_frexp')
  .uniqueId('c5df46977f5b77a0')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
scalar case, frexp:
T is f32 frexp(e:T) -> _frexp_result Splits e into a significand and exponent of the form significand * 2exponent. Returns the _frexp_result built-in structure, defined as: struct _frexp_result { sig : f32; // significand part exp : i32; // exponent part
}; The magnitude of the significand is in the range of [0.5, 1.0) or 0. Note: A value cannot be explicitly declared with the type _frexp_result, but a value may infer the type. (GLSLstd450FrexpStruct)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,vector_case_frexp')
  .uniqueId('69806278766b12a2')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
vector case, frexp:
T is vecN<f32> frexp(e:T) -> _frexp_result_vecN Splits the components of e into a significand and exponent of the form significand * 2exponent. Returns the _frexp_result_vecN built-in structure, defined as: struct _frexp_result_vecN { sig : vecN<f32>; // significand part exp : vecN<i32>; // exponent part
}; The magnitude of each component of the significand is in the range of [0.5, 1.0) or 0. Note: A value cannot be explicitly declared with the type _frexp_result_vecN, but a value may infer the type. (GLSLstd450FrexpStruct)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,inverseSqrt')
  .uniqueId('84fc180ad82c5618')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
inverseSqrt:
T is f32 or vecN<f32> inverseSqrt(e: T ) -> T Returns the reciprocal of sqrt(e). Component-wise when T is a vector. (GLSLstd450InverseSqrt)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,length')
  .uniqueId('0e5dba3253f9dec6')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
length:
T is f32 or vecN<f32> length(e: T ) -> f32 Returns the length of e (e.g. abs(e) if T is a scalar, or sqrt(e[0]2 + e[1]2 + ...) if T is a vector). (GLSLstd450Length)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,mix_all_same_type_operands')
  .uniqueId('f17861e71386bb59')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
mix all same type operands:
T is f32 or vecN<f32> mix(e1: T ,e2: T ,e3: T) -> T Returns the linear blend of e1 and e2 (e.g. e1*(1-e3)+e2*e3). Component-wise when T is a vector. (GLSLstd450FMix)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,vector_mix_with_scalar_blending_factor')
  .uniqueId('0a9f4a579e0c1348')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
vector mix with scalar blending factor:
T is vecN<f32> mix(e1: T ,e2: T ,e3: f32 ) -> T Returns the component-wise linear blend of e1 and e2, using scalar blending factor e3 for each component. Same as mix(e1,e2,T(e3)).

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,scalar_case_modf')
  .uniqueId('2a7234321aef021d')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
scalar case, modf:
T is f32 modf(e:T) -> _modf_result Splits e into fractional and whole number parts. Returns the _modf_result built-in structure, defined as: struct _modf_result { fract : f32; // fractional part whole : f32; // whole part
}; Note: A value cannot be explicitly declared with the type _modf_result, but a value may infer the type. (GLSLstd450ModfStruct)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,vector_case_modf')
  .uniqueId('d1426ca015843ddf')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
vector case, modf:
T is vecN<f32> modf(e:T) -> _modf_result_vecN Splits the components of e into fractional and whole number parts. Returns the _modf_result_vecN built-in structure, defined as: struct _modf_result_vecN { fract : vecN<f32>; // fractional part whole : vecN<f32>; // whole part
}; Note: A value cannot be explicitly declared with the type _modf_result_vecN, but a value may infer the type. (GLSLstd450ModfStruct)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,vector_case_normalize')
  .uniqueId('29c971aea0969a86')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
vector case, normalize:
T is f32 normalize(e: vecN<T> ) -> vecN<T> Returns a unit vector in the same direction as e. (GLSLstd450Normalize)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,pow')
  .uniqueId('a3ff963b1810c8c4')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
pow:
T is f32 or vecN<f32> pow(e1: T ,e2: T ) -> T Returns e1 raised to the power e2. Component-wise when T is a vector. (GLSLstd450Pow)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,quantize_to_f16')
  .uniqueId('ec899bfcd46a6316')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
quantize to f16:
T is f32 or vecN<f32> quantizeToF16(e: T ) -> T Quantizes a 32-bit floating point value e as if e were converted to a IEEE 754 binary16 value, and then converted back to a IEEE 754 binary32 value. See section 12.5.2 Floating point conversion. Component-wise when T is a vector. Note: The vec2<f32> case is the same as unpack2x16float(pack2x16float(e)). (OpQuantizeToF16)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,reflect')
  .uniqueId('463ddb8c59de0a98')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
reflect:
T is vecN<f32> reflect(e1: T ,e2: T ) -> T For the incident vector e1 and surface orientation e2, returns the reflection direction e1-2*dot(e2,e1)*e2. (GLSLstd450Reflect)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,refract')
  .uniqueId('8e0c0021b980cf0a')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
refract:
T is vecN<f32>I is f32 refract(e1: T ,e2: T ,e3: I ) -> T For the incident vector e1 and surface normal e2, and the ratio of indices of refraction e3, let k = 1.0 -e3*e3* (1.0 - dot(e2,e1) * dot(e2,e1)). If k < 0.0, returns the refraction vector 0.0, otherwise return the refraction vector e3*e1- (e3* dot(e2,e1) + sqrt(k)) *e2. (GLSLstd450Refract)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,round')
  .uniqueId('427d7791f5cd13dc')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
round:
T is f32 or vecN<f32> round(e: T ) -> T Result is the integer k nearest to e, as a floating point value. When e lies halfway between integers k and k+1, the result is k when k is even, and k+1 when k is odd. Component-wise when T is a vector. (GLSLstd450RoundEven)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,float_sign')
  .uniqueId('411a9acbb5411c89')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
float sign:
T is f32 or vecN<f32> sign(e: T ) -> T Returns the sign of e. Component-wise when T is a vector. (GLSLstd450FSign)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,sinh')
  .uniqueId('d1d30e0b45aabed5')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
sinh:
T is f32 or vecN<f32> sinh(e: T ) -> T Returns the hyperbolic sine of e. Component-wise when T is a vector. (GLSLstd450Sinh)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,smoothStep')
  .uniqueId('d1e9e5d30be184c0')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
smoothStep:
T is f32 or vecN<f32> smoothStep(e1: T ,e2: T ,e3: T ) -> T Returns the smooth Hermite interpolation between 0 and 1. Component-wise when T is a vector. (GLSLstd450SmoothStep)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,sqrt')
  .uniqueId('f16f8ca434c7e6d8')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
sqrt:
T is f32 or vecN<f32> sqrt(e: T ) -> T Returns the square root of e. Component-wise when T is a vector. (GLSLstd450Sqrt)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,step')
  .uniqueId('ac15bb28d3fa3032')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
step:
T is f32 or vecN<f32> step(e1: T ,e2: T ) -> T Returns 0.0 if e1 is less than e2, and 1.0 otherwise. Component-wise when T is a vector. (GLSLstd450Step)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,tan')
  .uniqueId('0229869d4d7f2702')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
tan:
T is f32 or vecN<f32> tan(e: T ) -> T Returns the tangent of e. Component-wise when T is a vector. (GLSLstd450Tan)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,tanh')
  .uniqueId('5d36803b13b3522d')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
tanh:
T is f32 or vecN<f32> tanh(e: T ) -> T Returns the hyperbolic tangent of e. Component-wise when T is a vector. (GLSLstd450Tanh)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

g.test('float_builtin_functions,trunc')
  .uniqueId('2f5ce2108f924fca')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
trunc:
T is f32 or vecN<f32> trunc(e: T ) -> T Returns the nearest whole number whose absolute value is less than or equal to e. Component-wise when T is a vector. (GLSLstd450Trunc)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();
