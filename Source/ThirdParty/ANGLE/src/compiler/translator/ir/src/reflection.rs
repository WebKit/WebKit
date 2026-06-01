// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Reflection info is gathered from the shader, which is a list of shader interface variables and
// their properties (similar to sh::ShaderVariable and sh::InterfaceBlock).

use crate::ir::*;
use crate::*;

use compile::BlockLayout;
use compile::BlockType;
pub use compile::InterfaceBlock;
use compile::Interpolation;
pub use compile::ShaderVariable;

pub struct Options {
    // Some built-ins have a different name in ES100.
    pub is_es1: bool,
    pub transform_float_uniform_to_fp16: bool,
}

// All reflection info bundled together
pub struct Info {
    // Either attributes or input varyings based on shader stage
    pub inputs: Vec<ShaderVariable>,
    // Either varying outputs or fragment output based on shader stage
    pub outputs: Vec<ShaderVariable>,
    pub uniforms: Vec<ShaderVariable>,
    pub shared: Vec<ShaderVariable>,
    pub uniform_blocks: Vec<InterfaceBlock>,
    pub storage_blocks: Vec<InterfaceBlock>,
}

impl Info {
    pub fn new() -> Info {
        Info {
            inputs: vec![],
            outputs: vec![],
            uniforms: vec![],
            shared: vec![],
            uniform_blocks: vec![],
            storage_blocks: vec![],
        }
    }
}

pub fn collect_info(ir: &IR, options: &Options, active_variables: &HashSet<VariableId>) -> Info {
    // Go over the interface variables and extract their reflection info.
    let mut info = Info {
        inputs: vec![],
        outputs: vec![],
        uniforms: vec![],
        shared: vec![],
        uniform_blocks: vec![],
        storage_blocks: vec![],
    };
    for &variable_id in ir.meta.all_global_variables() {
        let variable = ir.meta.get_variable(variable_id);
        // If the variable has no decorations and isn't a built-in, it's a private global variable
        // that does not need to be collected.
        if let Some(built_in) = variable.built_in {
            collect_built_in_variable(
                &ir.meta,
                &mut info,
                options,
                active_variables,
                variable_id,
                variable,
                built_in,
            );
        } else if !variable.decorations.decorations.is_empty() {
            collect_user_variable(
                &ir.meta,
                &mut info,
                options,
                active_variables,
                variable_id,
                variable,
            );
        }
    }

    info
}

fn collect_built_in_variable(
    ir_meta: &IRMeta,
    info: &mut Info,
    options: &Options,
    active_variables: &HashSet<VariableId>,
    variable_id: VariableId,
    variable: &Variable,
    built_in: BuiltIn,
) {
    // In AST-based CollectVariables(), any built-in that is redeclared is considered active even if
    // unused.  The backends currently can't handle removal of some of these built-ins correctly,
    // especially when gl_PerVertex members are removed (like gl_ClipDistance).  Those are
    // considered active unconditionally.
    //
    // A future optimization could look into removing these after fixing the backends.
    let force_active = matches!(
        built_in,
        BuiltIn::Position | BuiltIn::PointSize | BuiltIn::ClipDistance | BuiltIn::CullDistance
    );
    let active = active_variables.contains(&variable_id) || force_active;

    // Inactive Built-ins are otherwise not collected at all.
    if !active {
        return;
    }

    let mut shader_variable = new_shader_variable(ir_meta, options, variable, active);
    // Because `active` is forced to true, also mark `static_use` for consistency.
    if force_active {
        shader_variable.static_use = true;
    }

    // Set the built-in name, and adjust built-in implicit details
    let name = match built_in {
        BuiltIn::InstanceID => "gl_InstanceID",
        BuiltIn::VertexID => "gl_VertexID",
        BuiltIn::InstanceIndex | BuiltIn::VertexIndex => {
            panic!("InternalError: gl_VertexIndex / gl_InstanceIndex are not expected in the input")
        }
        BuiltIn::Position => "gl_Position",
        BuiltIn::PointSize => "gl_PointSize",
        BuiltIn::BaseVertex => "gl_BaseVertex",
        BuiltIn::BaseInstance => "gl_BaseInstance",
        BuiltIn::DrawID => "gl_DrawID",
        BuiltIn::FragCoord => "gl_FragCoord",
        BuiltIn::FrontFacing => "gl_FrontFacing",
        BuiltIn::PointCoord => "gl_PointCoord",
        BuiltIn::HelperInvocation => "gl_HelperInvocation",
        BuiltIn::FragColor => "gl_FragColor",
        BuiltIn::FragData => "gl_FragData",
        BuiltIn::FragDepth => {
            if options.is_es1 {
                "gl_FragDepthEXT"
            } else {
                "gl_FragDepth"
            }
        }
        BuiltIn::SecondaryFragColorEXT => "gl_SecondaryFragColorEXT",
        BuiltIn::SecondaryFragDataEXT => "gl_SecondaryFragDataEXT",
        BuiltIn::DepthRange => "gl_DepthRange",
        BuiltIn::ViewIDOVR => "gl_ViewIDOVR",
        BuiltIn::ClipDistance => "gl_ClipDistance",
        BuiltIn::CullDistance => "gl_CullDistance",
        BuiltIn::LastFragColor => "gl_LastFragColor",
        BuiltIn::LastFragData => "gl_LastFragData",
        BuiltIn::LastFragDepthARM => "gl_LastFragDepthARM",
        BuiltIn::LastFragStencilARM => "gl_LastFragStencilARM",
        BuiltIn::ShadingRateEXT => "gl_ShadingRateEXT",
        BuiltIn::PrimitiveShadingRateEXT => "gl_PrimitiveShadingRateEXT",
        BuiltIn::SampleID => "gl_SampleID",
        BuiltIn::SamplePosition => "gl_SamplePosition",
        BuiltIn::SampleMaskIn => "gl_SampleMaskIn",
        BuiltIn::SampleMask => "gl_SampleMask",
        BuiltIn::NumSamples => "gl_NumSamples",
        BuiltIn::NumWorkGroups => "gl_NumWorkGroups",
        BuiltIn::WorkGroupID => "gl_WorkGroupID",
        BuiltIn::LocalInvocationID => "gl_LocalInvocationID",
        BuiltIn::GlobalInvocationID => "gl_GlobalInvocationID",
        BuiltIn::LocalInvocationIndex => "gl_LocalInvocationIndex",
        BuiltIn::PerVertexIn => {
            set_io_block(&mut shader_variable);
            "gl_in"
        }
        BuiltIn::PerVertexOut => {
            set_io_block(&mut shader_variable);
            "gl_out"
        }
        BuiltIn::PrimitiveIDIn => "gl_PrimitiveIDIn",
        BuiltIn::InvocationID => "gl_InvocationID",
        BuiltIn::PrimitiveID => "gl_PrimitiveID",
        BuiltIn::LayerOut => "gl_Layer",
        BuiltIn::LayerIn => "gl_Layer",
        BuiltIn::PatchVerticesIn => "gl_PatchVerticesIn",
        BuiltIn::TessLevelOuter => {
            shader_variable.is_patch = true;
            "gl_TessLevelOuter"
        }
        BuiltIn::TessLevelInner => {
            shader_variable.is_patch = true;
            "gl_TessLevelInner"
        }
        BuiltIn::TessCoord => "gl_TessCoord",
        BuiltIn::BoundingBoxOES => {
            shader_variable.is_patch = true;
            "gl_BoundingBoxOES"
        }
    };
    shader_variable.name = name.to_string();
    shader_variable.mapped_name = shader_variable.name.clone();

    // Add the built-in to the right list
    let shader_type = ir_meta.get_shader_type();
    match built_in {
        // Inputs
        BuiltIn::FragCoord
        | BuiltIn::FrontFacing
        | BuiltIn::HelperInvocation
        | BuiltIn::PointCoord
        | BuiltIn::NumWorkGroups
        | BuiltIn::WorkGroupID
        | BuiltIn::LocalInvocationID
        | BuiltIn::GlobalInvocationID
        | BuiltIn::LocalInvocationIndex
        | BuiltIn::InstanceID
        | BuiltIn::VertexID
        | BuiltIn::DrawID
        | BuiltIn::LastFragColor
        | BuiltIn::LastFragData
        | BuiltIn::LastFragDepthARM
        | BuiltIn::LastFragStencilARM
        | BuiltIn::PrimitiveIDIn
        | BuiltIn::InvocationID
        | BuiltIn::LayerIn
        | BuiltIn::ShadingRateEXT
        | BuiltIn::SampleID
        | BuiltIn::SamplePosition
        | BuiltIn::SampleMaskIn
        | BuiltIn::PatchVerticesIn
        | BuiltIn::TessCoord
        | BuiltIn::PerVertexIn => {
            info.inputs.push(shader_variable);
        }
        // Note: mixing if guards is an experimental feature, so the body is duplicated because
        // currently the if guard has to be at the end and applies to all match arm patterns.
        BuiltIn::PrimitiveID if shader_type != ShaderType::Geometry => {
            info.inputs.push(shader_variable);
        }
        BuiltIn::ClipDistance | BuiltIn::CullDistance if shader_type == ShaderType::Fragment => {
            info.inputs.push(shader_variable);
        }
        BuiltIn::TessLevelOuter | BuiltIn::TessLevelInner
            if shader_type == ShaderType::TessellationEvaluation =>
        {
            info.inputs.push(shader_variable);
        }

        // Outputs
        BuiltIn::Position
        | BuiltIn::PointSize
        | BuiltIn::FragColor
        | BuiltIn::FragData
        | BuiltIn::FragDepth
        | BuiltIn::SecondaryFragColorEXT
        | BuiltIn::SecondaryFragDataEXT
        | BuiltIn::PrimitiveID
        | BuiltIn::ClipDistance
        | BuiltIn::CullDistance
        | BuiltIn::PrimitiveShadingRateEXT
        | BuiltIn::SampleMask
        | BuiltIn::TessLevelOuter
        | BuiltIn::TessLevelInner
        | BuiltIn::BoundingBoxOES
        | BuiltIn::PerVertexOut => {
            info.outputs.push(shader_variable);
        }
        BuiltIn::LayerOut if shader_type == ShaderType::Geometry => {
            info.outputs.push(shader_variable);
        }

        // Uniforms
        BuiltIn::NumSamples => {
            info.uniforms.push(shader_variable);
        }
        BuiltIn::DepthRange => {
            info.uniforms.push(shader_variable);
        }

        // Misc

        // gl_LayerOut outside geometry shaders is only used to implement OVR_multiview/2, and not
        // reflected to the API.
        BuiltIn::LayerOut => (),

        // The following are not captured
        //
        // * BaseVertex, BaseInstance and ViewIDOVR: They are sometimes emulated, unclear why not
        //   captured, potentially a bug.  http://anglebug.com/496616810
        // * WorkGroup
        BuiltIn::BaseVertex => {}
        BuiltIn::BaseInstance => {}
        BuiltIn::ViewIDOVR => {}

        BuiltIn::InstanceIndex | BuiltIn::VertexIndex => {
            panic!("InternalError: gl_VertexIndex / gl_InstanceIndex are not expected in the input")
        }
    }
}

fn collect_user_variable(
    ir_meta: &IRMeta,
    info: &mut Info,
    options: &Options,
    active_variables: &HashSet<VariableId>,
    variable_id: VariableId,
    variable: &Variable,
) {
    // No interface variable should have a temp name
    debug_assert!(variable.name.source != NameSource::Temporary);
    // Ignore ANGLE-internal variables, except for angle_DrawID, angle_BaseVertex and
    // angle_BaseInstance.  Currently, these are specially handled by the generators which look for
    // these specific names, and the collected names are later switched to gl_*.  Once all code
    // generation is done in IR, the correct name can be collected right here.
    if variable.name.source == NameSource::Internal
        && !has_decoration!(variable.decorations, Decoration::EmulatedMultiDrawBuiltIn)
    {
        return;
    }

    let active = active_variables.contains(&variable_id);

    if has_decoration!(variable.decorations, Decoration::Block) {
        let interface_block = new_interface_block(ir_meta, options, variable, active);
        if variable.decorations.has(Decoration::Buffer) {
            info.storage_blocks.push(interface_block);
        } else {
            debug_assert!(variable.decorations.has(Decoration::Uniform));
            info.uniform_blocks.push(interface_block);
        }
    } else {
        let shader_variable = new_shader_variable(ir_meta, options, variable, active);
        if variable.decorations.has(Decoration::Input) {
            info.inputs.push(shader_variable);
        } else if variable.decorations.has(Decoration::Output)
            || variable.decorations.has(Decoration::InputOutput)
        {
            info.outputs.push(shader_variable);
        } else if variable.decorations.has(Decoration::Shared) {
            info.shared.push(shader_variable);
        } else {
            debug_assert!(variable.decorations.has(Decoration::Uniform));
            info.uniforms.push(shader_variable);
        }
    }
}

fn mapped_name(name: &Name) -> String {
    // GLSL ES 3.00.6 section 3.9: the maximum length of an identifier is 1024 characters.
    const MAX_ESSL_IDENTIFIER_LENGTH: usize = 1024;

    let prefix = match name.source {
        // Make sure unnamed interface blocks remain unnamed.
        // Also, if the identifier length is already close to the limit, we can't prefix it.  This
        // is not a problem since there are no builtins or ANGLE's internal variables that would
        // have as long names and could conflict.
        NameSource::ShaderInterface
            if !name.name.is_empty()
                && name.name.len() + USER_SYMBOL_PREFIX.len() <= MAX_ESSL_IDENTIFIER_LENGTH =>
        {
            USER_SYMBOL_PREFIX
        }
        NameSource::Temporary => panic!(
            "Internal error: Should not collect reflection info for shader-private variables and \
             types"
        ),
        _ => "",
    };
    format!("{}{}", prefix, name.name)
}

fn to_gl_type(
    ir_meta: &IRMeta,
    type_id: TypeId,
    precision: Precision,
) -> (gl::Enum, gl::Enum, Vec<u32>) {
    let (gl_type, gl_precision_type) = match type_id {
        TYPE_ID_FLOAT => (gl::FLOAT, gl::FLOAT),
        TYPE_ID_INT => (gl::INT, gl::INT),
        TYPE_ID_UINT => (gl::UNSIGNED_INT, gl::INT),
        TYPE_ID_BOOL => (gl::BOOL, gl::NONE),
        TYPE_ID_ATOMIC_COUNTER => (gl::UNSIGNED_INT_ATOMIC_COUNTER, gl::NONE),
        TYPE_ID_YUV_CSC_STANDARD => (gl::UNSIGNED_INT, gl::NONE),

        TYPE_ID_VEC2 => (gl::FLOAT_VEC2, gl::FLOAT),
        TYPE_ID_VEC3 => (gl::FLOAT_VEC3, gl::FLOAT),
        TYPE_ID_VEC4 => (gl::FLOAT_VEC4, gl::FLOAT),
        TYPE_ID_IVEC2 => (gl::INT_VEC2, gl::INT),
        TYPE_ID_IVEC3 => (gl::INT_VEC3, gl::INT),
        TYPE_ID_IVEC4 => (gl::INT_VEC4, gl::INT),
        TYPE_ID_UVEC2 => (gl::UNSIGNED_INT_VEC2, gl::INT),
        TYPE_ID_UVEC3 => (gl::UNSIGNED_INT_VEC3, gl::INT),
        TYPE_ID_UVEC4 => (gl::UNSIGNED_INT_VEC4, gl::INT),
        TYPE_ID_BVEC2 => (gl::BOOL_VEC2, gl::NONE),
        TYPE_ID_BVEC3 => (gl::BOOL_VEC3, gl::NONE),
        TYPE_ID_BVEC4 => (gl::BOOL_VEC4, gl::NONE),

        TYPE_ID_MAT2 => (gl::FLOAT_MAT2, gl::FLOAT),
        TYPE_ID_MAT2X3 => (gl::FLOAT_MAT2X3, gl::FLOAT),
        TYPE_ID_MAT2X4 => (gl::FLOAT_MAT2X4, gl::FLOAT),
        TYPE_ID_MAT3X2 => (gl::FLOAT_MAT3X2, gl::FLOAT),
        TYPE_ID_MAT3 => (gl::FLOAT_MAT3, gl::FLOAT),
        TYPE_ID_MAT3X4 => (gl::FLOAT_MAT3X4, gl::FLOAT),
        TYPE_ID_MAT4X2 => (gl::FLOAT_MAT4X2, gl::FLOAT),
        TYPE_ID_MAT4X3 => (gl::FLOAT_MAT4X3, gl::FLOAT),
        TYPE_ID_MAT4 => (gl::FLOAT_MAT4, gl::FLOAT),

        _ => {
            let type_info = ir_meta.get_type(type_id);
            match *type_info {
                Type::DeadCodeEliminated => {
                    panic!("Internal error: Unexpected dead-code-eliminated type for variable")
                }
                Type::Pointer(pointee_type_id) => {
                    return to_gl_type(ir_meta, pointee_type_id, precision);
                }
                Type::Array(element_type_id, _) | Type::UnsizedArray(element_type_id) => {
                    let (gl_type, gl_precision, mut array_sizes) =
                        to_gl_type(ir_meta, element_type_id, precision);
                    array_sizes.push(if let Type::Array(_, count) = *type_info {
                        count
                    } else {
                        0
                    });
                    return (gl_type, gl_precision, array_sizes);
                }
                Type::Image(image_basic_type, image_type) => {
                    match image_type.dimension {
                        ImageDimension::D2 => match image_basic_type {
                            ImageBasicType::Float => {
                                if image_type.is_sampled {
                                    if image_type.is_shadow {
                                        if image_type.is_array {
                                            (gl::SAMPLER_2D_ARRAY_SHADOW, gl::FLOAT)
                                        } else {
                                            (gl::SAMPLER_2D_SHADOW, gl::FLOAT)
                                        }
                                    } else {
                                        match (image_type.is_ms, image_type.is_array) {
                                            (false, false) => (gl::SAMPLER_2D, gl::FLOAT),
                                            (false, true) => (gl::SAMPLER_2D_ARRAY, gl::FLOAT),
                                            (true, false) => {
                                                (gl::SAMPLER_2D_MULTISAMPLE, gl::FLOAT)
                                            }
                                            (true, true) => {
                                                (gl::SAMPLER_2D_MULTISAMPLE_ARRAY, gl::FLOAT)
                                            }
                                        }
                                    }
                                } else {
                                    // Multisampled storage images are a desktop GLSL feature
                                    debug_assert!(!image_type.is_ms);
                                    if image_type.is_array {
                                        (gl::IMAGE_2D_ARRAY, gl::FLOAT)
                                    } else {
                                        (gl::IMAGE_2D, gl::FLOAT)
                                    }
                                }
                            }
                            ImageBasicType::Int => {
                                if image_type.is_sampled {
                                    match (image_type.is_ms, image_type.is_array) {
                                        (false, false) => (gl::INT_SAMPLER_2D, gl::INT),
                                        (false, true) => (gl::INT_SAMPLER_2D_ARRAY, gl::INT),
                                        (true, false) => (gl::INT_SAMPLER_2D_MULTISAMPLE, gl::INT),
                                        (true, true) => {
                                            (gl::INT_SAMPLER_2D_MULTISAMPLE_ARRAY, gl::INT)
                                        }
                                    }
                                } else {
                                    debug_assert!(!image_type.is_ms);
                                    if image_type.is_array {
                                        (gl::INT_IMAGE_2D_ARRAY, gl::INT)
                                    } else {
                                        (gl::INT_IMAGE_2D, gl::INT)
                                    }
                                }
                            }
                            ImageBasicType::Uint => {
                                if image_type.is_sampled {
                                    match (image_type.is_ms, image_type.is_array) {
                                        (false, false) => (gl::UNSIGNED_INT_SAMPLER_2D, gl::INT),
                                        (false, true) => {
                                            (gl::UNSIGNED_INT_SAMPLER_2D_ARRAY, gl::INT)
                                        }
                                        (true, false) => {
                                            (gl::UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE, gl::INT)
                                        }
                                        (true, true) => {
                                            (gl::UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY, gl::INT)
                                        }
                                    }
                                } else if image_type.is_array {
                                    (gl::UNSIGNED_INT_IMAGE_2D_ARRAY, gl::INT)
                                } else {
                                    (gl::UNSIGNED_INT_IMAGE_2D, gl::INT)
                                }
                            }
                        },
                        ImageDimension::D3 => match image_basic_type {
                            ImageBasicType::Float => {
                                if image_type.is_sampled {
                                    (gl::SAMPLER_3D, gl::FLOAT)
                                } else {
                                    (gl::IMAGE_3D, gl::FLOAT)
                                }
                            }
                            ImageBasicType::Int => {
                                if image_type.is_sampled {
                                    (gl::INT_SAMPLER_3D, gl::INT)
                                } else {
                                    (gl::INT_IMAGE_3D, gl::INT)
                                }
                            }
                            ImageBasicType::Uint => {
                                if image_type.is_sampled {
                                    (gl::UNSIGNED_INT_SAMPLER_3D, gl::INT)
                                } else {
                                    (gl::UNSIGNED_INT_IMAGE_3D, gl::INT)
                                }
                            }
                        },
                        ImageDimension::Cube => match image_basic_type {
                            ImageBasicType::Float => {
                                if image_type.is_sampled {
                                    if image_type.is_shadow {
                                        if image_type.is_array {
                                            (gl::SAMPLER_CUBE_MAP_ARRAY_SHADOW, gl::FLOAT)
                                        } else {
                                            (gl::SAMPLER_CUBE_SHADOW, gl::FLOAT)
                                        }
                                    } else if image_type.is_array {
                                        (gl::SAMPLER_CUBE_MAP_ARRAY, gl::FLOAT)
                                    } else {
                                        (gl::SAMPLER_CUBE, gl::FLOAT)
                                    }
                                } else if image_type.is_array {
                                    (gl::IMAGE_CUBE_MAP_ARRAY, gl::FLOAT)
                                } else {
                                    (gl::IMAGE_CUBE, gl::FLOAT)
                                }
                            }
                            ImageBasicType::Int => {
                                if image_type.is_sampled {
                                    if image_type.is_array {
                                        (gl::INT_SAMPLER_CUBE_MAP_ARRAY, gl::INT)
                                    } else {
                                        (gl::INT_SAMPLER_CUBE, gl::INT)
                                    }
                                } else if image_type.is_array {
                                    (gl::INT_IMAGE_CUBE_MAP_ARRAY, gl::INT)
                                } else {
                                    (gl::INT_IMAGE_CUBE, gl::INT)
                                }
                            }
                            ImageBasicType::Uint => {
                                if image_type.is_sampled {
                                    if image_type.is_array {
                                        (gl::UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY, gl::INT)
                                    } else {
                                        (gl::UNSIGNED_INT_SAMPLER_CUBE, gl::INT)
                                    }
                                } else if image_type.is_array {
                                    (gl::UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY, gl::INT)
                                } else {
                                    (gl::UNSIGNED_INT_IMAGE_CUBE, gl::INT)
                                }
                            }
                        },
                        ImageDimension::Rect => {
                            // Only float rect samplers are exposed via GL_ANGLE_texture_rectangle.
                            debug_assert!(image_basic_type == ImageBasicType::Float);
                            // Rect storage images are a desktop GLSL feature
                            debug_assert!(image_type.is_sampled);
                            (gl::SAMPLER_2D_RECT_ANGLE, gl::FLOAT)
                        }
                        ImageDimension::Buffer => match image_basic_type {
                            ImageBasicType::Float => {
                                if image_type.is_sampled {
                                    (gl::SAMPLER_BUFFER, gl::FLOAT)
                                } else {
                                    (gl::IMAGE_BUFFER, gl::FLOAT)
                                }
                            }
                            ImageBasicType::Int => {
                                if image_type.is_sampled {
                                    (gl::INT_SAMPLER_BUFFER, gl::INT)
                                } else {
                                    (gl::INT_IMAGE_BUFFER, gl::INT)
                                }
                            }
                            ImageBasicType::Uint => {
                                if image_type.is_sampled {
                                    (gl::UNSIGNED_INT_SAMPLER_BUFFER, gl::INT)
                                } else {
                                    (gl::UNSIGNED_INT_IMAGE_BUFFER, gl::INT)
                                }
                            }
                        },
                        ImageDimension::External => (gl::SAMPLER_EXTERNAL_OES, gl::FLOAT),
                        ImageDimension::ExternalY2Y => (gl::SAMPLER_EXTERNAL_2D_Y2Y_EXT, gl::FLOAT),
                        ImageDimension::Video => (gl::SAMPLER_VIDEO_IMAGE_WEBGL, gl::FLOAT),
                        ImageDimension::PixelLocal => panic!(
                            "Internal error: Pixel local storage should be transformed already"
                        ),
                        ImageDimension::Subpass => panic!(
                            "Internal error: Subpass input should not be generated before \
                             reflection info is collected"
                        ),
                    }
                }

                _ => (gl::NONE, gl::NONE),
            }
        }
    };

    let gl_precision = match (gl_precision_type, precision) {
        (gl::FLOAT, Precision::High) => gl::HIGH_FLOAT,
        (gl::FLOAT, Precision::Medium) => gl::MEDIUM_FLOAT,
        (gl::FLOAT, Precision::Low) => gl::LOW_FLOAT,
        (gl::INT, Precision::High) => gl::HIGH_INT,
        (gl::INT, Precision::Medium) => gl::MEDIUM_INT,
        (gl::INT, Precision::Low) => gl::LOW_INT,
        _ => gl::NONE,
    };

    (gl_type, gl_precision, vec![])
}

fn get_base_type(ir_meta: &IRMeta, type_id: TypeId) -> &Type {
    let type_info = ir_meta.get_type(type_id);
    // Pointers and unsized arrays cannot be nested, so get their element type with a single `if`.
    let type_info = if let Type::Pointer(pointee_type_id) = *type_info {
        ir_meta.get_type(pointee_type_id)
    } else {
        type_info
    };
    let mut type_info = if let Type::UnsizedArray(element_type_id) = *type_info {
        ir_meta.get_type(element_type_id)
    } else {
        type_info
    };
    // Skip all array-ness to get to the base type
    while let Type::Array(element_type_id, _) = *type_info {
        type_info = ir_meta.get_type(element_type_id);
    }

    type_info
}

#[derive(Copy, Clone)]
struct Inherit {
    active: bool,
    is_patch: bool,
    is_default_uniform: bool,
}

impl Inherit {
    fn new(active: bool, is_default_uniform: bool) -> Inherit {
        Inherit { active, is_patch: false, is_default_uniform }
    }

    fn accumulate_is_patch(&self, is_patch: bool) -> Inherit {
        Inherit { is_patch: self.is_patch || is_patch, ..*self }
    }
}

fn new_shader_variable(
    ir_meta: &IRMeta,
    options: &Options,
    variable: &Variable,
    active: bool,
) -> ShaderVariable {
    new_common_shader_variable(
        ir_meta,
        options,
        &variable.name,
        variable.type_id,
        variable.precision,
        &variable.decorations,
        variable.is_static_use,
        Inherit::new(active, variable.decorations.has(Decoration::Uniform)),
    )
}

fn new_field(
    ir_meta: &IRMeta,
    options: &Options,
    field: &Field,
    inherit: Inherit,
) -> ShaderVariable {
    new_common_shader_variable(
        ir_meta,
        options,
        &field.name,
        field.type_id,
        field.precision,
        &field.decorations,
        field.is_static_use || inherit.active,
        inherit,
    )
}

fn new_common_shader_variable(
    ir_meta: &IRMeta,
    options: &Options,
    name: &Name,
    type_id: TypeId,
    precision: Precision,
    decorations: &Decorations,
    static_use: bool,
    inherit: Inherit,
) -> ShaderVariable {
    let (gl_type, gl_precision, array_sizes) = to_gl_type(ir_meta, type_id, precision);

    let is_eligible_for_fp16 = matches!(
        gl_type,
        gl::FLOAT
            | gl::FLOAT_VEC2
            | gl::FLOAT_VEC3
            | gl::FLOAT_VEC4
            | gl::FLOAT_MAT2
            | gl::FLOAT_MAT2X3
            | gl::FLOAT_MAT2X4
            | gl::FLOAT_MAT3X2
            | gl::FLOAT_MAT3
            | gl::FLOAT_MAT3X4
            | gl::FLOAT_MAT4X2
            | gl::FLOAT_MAT4X3
            | gl::FLOAT_MAT4
    );
    let is_float16 = options.transform_float_uniform_to_fp16
        && inherit.is_default_uniform
        && is_eligible_for_fp16
        && gl_precision != gl::HIGH_FLOAT;

    let mut var = ShaderVariable {
        gl_type,
        gl_precision,
        name: name.name.to_string(),
        mapped_name: mapped_name(name),
        struct_or_block_name: "".to_string(),
        mapped_struct_or_block_name: "".to_string(),
        fields: vec![],
        array_sizes,
        static_use,
        active: inherit.active,
        is_row_major: false,
        location: -1,
        is_location_implicit: false,
        interpolation: Interpolation::Smooth,
        is_invariant: false,
        is_io_block: false,
        is_patch: inherit.is_patch,
        binding: -1,
        gl_image_unit_format: gl::NONE,
        offset: -1,
        raster_ordered: false,
        readonly: false,
        writeonly: false,
        // TODO(http://anglebug.com/349994211): While gathering active variables, also find out
        // which samplers are used in texel fetch.  Currently, there is an AST pass for this, which
        // is not correct for samplers in structs.  It's also incorrect for sampler arrays, which
        // indicates that returning a bool in ShaderVariable is not sufficient information.
        texel_fetch_static_use: false,
        is_float16,
        is_fragment_inout: false,
        index: -1,
        yuv: false,
        id: 0,
    };

    let type_info = get_base_type(ir_meta, type_id);
    let interface_block_fields = if let Type::Struct(struct_name, fields, specialization) =
        type_info
    {
        var.struct_or_block_name = struct_name.name.to_string();
        // Mapped name is only used for interface blocks
        if *specialization == StructSpecialization::InterfaceBlock {
            var.mapped_struct_or_block_name = mapped_name(struct_name);
        }
        let field_inherit = inherit.accumulate_is_patch(decorations.has(Decoration::Patch));
        var.fields =
            fields.iter().map(|field| new_field(ir_meta, options, field, field_inherit)).collect();
        (*specialization == StructSpecialization::InterfaceBlock).then_some(fields)
    } else {
        None
    };

    for decoration in decorations.decorations.iter() {
        match *decoration {
            Decoration::Invariant => var.is_invariant = true,
            Decoration::Smooth => {
                debug_assert!(var.interpolation == Interpolation::Smooth);
            }
            Decoration::Flat => {
                debug_assert!(var.interpolation == Interpolation::Smooth);
                var.interpolation = Interpolation::Flat;
            }
            Decoration::NoPerspective => {
                var.interpolation = match var.interpolation {
                    Interpolation::Centroid => Interpolation::NoPerspectiveCentroid,
                    Interpolation::Sample => Interpolation::NoPerspectiveSample,
                    _ => {
                        debug_assert!(var.interpolation == Interpolation::Smooth);
                        Interpolation::NoPerspective
                    }
                };
            }
            Decoration::Centroid => {
                var.interpolation = match var.interpolation {
                    Interpolation::NoPerspective => Interpolation::NoPerspectiveCentroid,
                    _ => {
                        debug_assert!(var.interpolation == Interpolation::Smooth);
                        Interpolation::Centroid
                    }
                };
            }
            Decoration::Sample => {
                var.interpolation = match var.interpolation {
                    Interpolation::NoPerspective => Interpolation::NoPerspectiveSample,
                    _ => {
                        debug_assert!(var.interpolation == Interpolation::Smooth);
                        Interpolation::Sample
                    }
                };
            }
            Decoration::Patch => {
                // The auxiliary storage qualifier patch is not used for interpolation it is a
                // compile-time error to use interpolation qualifiers with patch.
                debug_assert!(var.interpolation == Interpolation::Smooth);
                var.interpolation = Interpolation::Flat;
                var.is_patch = true;
            }
            Decoration::ReadOnly => var.readonly = true,
            Decoration::WriteOnly => var.writeonly = true,
            Decoration::Yuv => var.yuv = true,
            // If there's no GL type for this variable but it's an input/output, it would be an I/O
            // block if it's also an interface block.
            Decoration::Input | Decoration::Output => {
                if let Some(fields) = interface_block_fields {
                    debug_assert!(var.gl_type == gl::NONE);
                    set_io_block(&mut var);
                    set_field_locations(
                        ir_meta,
                        fields,
                        &mut var.fields,
                        get_decoration_value!(decorations, Decoration::Location),
                    );
                }
            }
            Decoration::InputOutput => var.is_fragment_inout = true,
            Decoration::Location(location) => var.location = location as i32,
            Decoration::Index(index) => var.index = index as i32,
            Decoration::Binding(binding) => var.binding = binding as i32,
            Decoration::Offset(offset) => var.offset = offset as i32,
            Decoration::MatrixPacking(packing) => {
                var.is_row_major = packing == MatrixPacking::RowMajor
            }
            Decoration::ImageInternalFormat(format) => {
                var.gl_image_unit_format = match format {
                    ImageInternalFormat::RGBA32F => gl::RGBA32F,
                    ImageInternalFormat::RGBA16F => gl::RGBA16F,
                    ImageInternalFormat::R32F => gl::R32F,
                    ImageInternalFormat::RGBA32UI => gl::RGBA32UI,
                    ImageInternalFormat::RGBA16UI => gl::RGBA16UI,
                    ImageInternalFormat::RGBA8UI => gl::RGBA8UI,
                    ImageInternalFormat::R32UI => gl::R32UI,
                    ImageInternalFormat::RGBA32I => gl::RGBA32I,
                    ImageInternalFormat::RGBA16I => gl::RGBA16I,
                    ImageInternalFormat::RGBA8I => gl::RGBA8I,
                    ImageInternalFormat::R32I => gl::R32I,
                    ImageInternalFormat::RGBA8 => gl::RGBA8,
                    ImageInternalFormat::RGBA8SNORM => gl::RGBA8_SNORM,
                }
            }
            Decoration::RasterOrdered => var.raster_ordered = true,
            // Some information is either internal, pertains only to InterfaceBlocks or not reported
            // at the API level.
            Decoration::Precise
            | Decoration::Interpolant
            | Decoration::Shared
            | Decoration::Coherent
            | Decoration::Restrict
            | Decoration::Volatile
            | Decoration::Uniform
            | Decoration::Buffer
            | Decoration::PushConstant
            | Decoration::NonCoherent
            | Decoration::InputAttachmentIndex(_)
            | Decoration::SpecConst(_)
            | Decoration::Block(_)
            | Decoration::Depth(_)
            | Decoration::EmulatedViewIDOut
            | Decoration::EmulatedViewIDIn
            | Decoration::EmulatedMultiDrawBuiltIn(_) => (),
        }
    }

    var
}

fn get_block_type(variable: &Variable) -> BlockType {
    if variable.decorations.has(Decoration::Buffer) {
        BlockType::Buffer
    } else {
        debug_assert!(variable.decorations.has(Decoration::Uniform));
        BlockType::Uniform
    }
}

fn get_block_layout(variable: &Variable) -> BlockLayout {
    let storage = get_decoration_value!(variable.decorations, Decoration::Block).unwrap();
    match storage {
        BlockStorage::Shared => BlockLayout::Shared,
        BlockStorage::Packed => BlockLayout::Packed,
        BlockStorage::Std140 => BlockLayout::Std140,
        BlockStorage::Std430 => BlockLayout::Std430,
    }
}

fn get_location_count(ir_meta: &IRMeta, type_id: TypeId) -> i32 {
    let type_info = ir_meta.get_type(type_id);
    debug_assert!(!type_info.is_pointer());

    match type_info {
        Type::Struct(_, fields, _) => fields
            .iter()
            .fold(0, |acc, field| acc.saturating_add(get_location_count(ir_meta, field.type_id))),
        &Type::Array(element_id, count) => {
            get_location_count(ir_meta, element_id).saturating_mul(count as i32)
        }
        _ => 1,
    }
}

fn set_io_block(var: &mut ShaderVariable) {
    var.is_io_block = true;
    for field in var.fields.iter_mut() {
        set_io_block(field);
    }
}

fn set_field_locations(
    ir_meta: &IRMeta,
    fields: &[Field],
    field_variables: &mut [ShaderVariable],
    block_location: Option<u32>,
) {
    // If the I/O block has a location, the fields start at that location.  Otherwise the
    // location is implicit and starts at 0.
    let is_block_implicit_location = block_location.is_none();
    let mut location = block_location.unwrap_or(0) as i32;

    fields.iter().zip(field_variables.iter_mut()).for_each(|(field, variable)| {
        // If the field has an explicit location, it gets that.  If the next field does not have
        // an explicit location, it is implicitly assigned the next location.
        if variable.location >= 0 {
            location = variable.location;
        } else {
            variable.is_location_implicit = is_block_implicit_location;
            variable.location = location;
        }
        location += get_location_count(ir_meta, field.type_id);
    });
}

fn new_interface_block(
    ir_meta: &IRMeta,
    options: &Options,
    variable: &Variable,
    active: bool,
) -> InterfaceBlock {
    // Per GLSL ES 3.10 section 4.3.9, interface blocks cannot be arrays of arrays, so a single
    // nesting level is sufficient to get to the struct if arrayed.
    let mut struct_type_info = ir_meta.get_type(ir_meta.get_pointee_type(variable.type_id));
    let array_size = if let Type::Array(struct_type_id, count) = *struct_type_info {
        struct_type_info = ir_meta.get_type(struct_type_id);
        count
    } else {
        0
    };
    let (block_name, fields) = if let Type::Struct(struct_name, fields, _) = struct_type_info {
        (struct_name, fields)
    } else {
        panic!("Internal error: Expected struct type for interface block");
    };

    let mut var = InterfaceBlock {
        name: block_name.name.to_string(),
        mapped_name: mapped_name(block_name),
        instance_name: variable.name.name.to_string(),
        fields: vec![],
        static_use: variable.is_static_use,
        active,
        array_size,
        block_type: get_block_type(variable),
        block_layout: get_block_layout(variable),
        binding: get_decoration_value!(variable.decorations, Decoration::Binding)
            .map(|binding| binding as i32)
            .unwrap_or(-1),
        readonly: true,
        id: 0,
    };

    var.fields = fields
        .iter()
        .map(|field| {
            // While iterating the fields, also gather the following information:
            //
            // * If the interface block is nameless, static_use is derived from the fields
            // * If the interface block is an SSBO, readonly is derived from the fields
            if field.is_static_use {
                var.static_use = true;
            }
            if !field.decorations.has(Decoration::ReadOnly) {
                var.readonly = false;
            }

            new_field(ir_meta, options, field, Inherit::new(active, false))
        })
        .collect();

    var
}
