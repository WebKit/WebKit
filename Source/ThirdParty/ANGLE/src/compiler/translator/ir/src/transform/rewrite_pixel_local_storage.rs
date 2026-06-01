// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implement ANGLE_shader_pixel_local_storage on top of the desired strategy specified in the
// options; using storage images or framebuffer fetch.
//
// Depending on the backend API and device capabilities, coherency is achieved by different means
// on most platforms.
use crate::ir::*;
use crate::*;

use compile::PixelLocalStorageImpl;
use compile::PixelLocalStorageSync;

pub struct Options {
    pub pls: compile::PixelLocalStorageOptions,
    // The maximum value for the `location` qualifier, if emulated with framebuffer fetch.
    pub max_framebuffer_fetch_location: u32,
    // Driver bug workaround for unpackUnorm4x8 when given mediump input.
    pub pass_highp_to_pack_unorm_snorm_built_ins: bool,
}

#[derive(Clone)]
struct PlaneInfo {
    // How the plane is implemented.  While currently ANGLE implements all planes identically, in
    // the future we could implement coherent and non-cohernet planes differently.
    implementation: PixelLocalStorageImpl,
    format: ImageInternalFormat,
    // If emulated by framebuffer fetch, the id of the global variable that stores the content of
    // the plane during shader execution.
    access_variable: Option<TypedId>,
}

struct State<'a> {
    ir_meta: &'a mut IRMeta,
    // Indexed by VariableId, contains information about the PLS plane
    planes: Vec<PlaneInfo>,
    // Whether any plane is implemented as image2D
    has_image: bool,
    // ... and expected to be coherent.
    has_coherent_image: bool,
    // Whether any plane is implemented as inout
    has_inout: bool,
}

pub fn run(ir: &mut IR, options: &Options) {
    let mut state = State {
        ir_meta: &mut ir.meta,
        planes: vec![],
        has_image: false,
        has_coherent_image: false,
        has_inout: false,
    };

    // First, decide how to transform each PLS variable and replace it with the implementation's
    // choice.  Since monomorphize_unsupported_functions has already run and eliminated PLS planes
    // as function arguments, only the global variables need to be modified.
    let mut planes = vec![
        PlaneInfo {
            implementation: PixelLocalStorageImpl::NotSupported,
            format: ImageInternalFormat::RGBA32F,
            access_variable: None,
        };
        state.ir_meta.all_variables().len()
    ];
    let global_variables = state.ir_meta.all_global_variables().clone();
    for variable_id in &global_variables {
        if let Some(info) = transform_variable(&mut state, *variable_id, options) {
            planes[variable_id.id as usize] = info;
        }
    }
    state.planes = planes;

    let mut preamble = Block::new();
    let mut postamble = Block::new();

    // When PLS planes are implemented as image2D, gl_FragCoord is used to locate this fragment's
    // coordinates in the plane.
    let fragcoord = declare_fragcoord_global(&mut state, &mut preamble);

    traverser::transformer::for_each_instruction(
        &mut state,
        &mut ir.function_entries,
        &|state, instruction| transform_pls_load_store(state, options, instruction, fragcoord),
    );

    if state.has_image {
        add_pre_and_post_pls_code_for_image(&mut state, options, &mut preamble, &mut postamble);
    }
    if state.has_inout {
        add_pre_and_post_pls_code_for_framebuffer_fetch(
            &mut state,
            &global_variables,
            &mut preamble,
            &mut postamble,
        );
    }

    // Inject the code that needs to run before and after all PLS operations.
    //
    // TODO(http://anglebug.com/40096838): Inject these functions in a tight critical section,
    // instead of just locking the entire main() function:
    //
    //   - Monomorphize all PLS calls into main().
    //   - Insert begin/end calls around the first/last PLS calls (and outside of flow control).
    let main_id = state.ir_meta.get_main_function_id().unwrap();
    let main_entry = ir.function_entries[main_id.id as usize].as_mut().unwrap();
    main_entry.prepend_code(preamble);
    main_entry.append_code(postamble);
}

fn transform_plane_variable_to_image(
    state: &mut State,
    variable_id: VariableId,
    basic_type: ImageBasicType,
    format: ImageInternalFormat,
    options: &Options,
) {
    // Adjust the image format if emulation is needed.
    let (override_format, basic_type) =
        match (format, options.pls.supports_native_rgba8_image_formats) {
            (ImageInternalFormat::RGBA8 | ImageInternalFormat::RGBA8UI, false) => {
                (ImageInternalFormat::R32UI, ImageBasicType::Uint)
            }
            (ImageInternalFormat::RGBA8I, false) => {
                (ImageInternalFormat::R32I, ImageBasicType::Int)
            }
            _ => (format, basic_type),
        };

    // Change the type to image2D.
    let image_type = ImageType {
        dimension: ImageDimension::D2,
        is_sampled: false,
        is_array: false,
        is_ms: false,
        is_shadow: false,
    };
    let type_id = state.ir_meta.get_image_type_id(basic_type, image_type);
    let type_id = state.ir_meta.get_pointer_type_id(type_id);

    // Modify the variable in-place so it continues to be referenced by the same id.
    // Only the variable's type, precision and decorations need to be changed.
    let variable = state.ir_meta.get_variable_mut(variable_id);

    // Add decorations required for implementation as image2D.  The PLS plane's binding is reused
    // for the storage image.
    let binding = *variable
        .decorations
        .decorations
        .iter()
        .find(|decoration| matches!(decoration, Decoration::Binding(_)))
        .unwrap();
    let mut decorations = vec![
        Decoration::Uniform,
        binding,
        Decoration::ImageInternalFormat(override_format),
        Decoration::Coherent,
        Decoration::Restrict,
    ];
    // Add decorations required for coherence, unless the PLS plane is declared as non-coherent.
    let is_noncoherent =
        variable.decorations.has(Decoration::NonCoherent) && options.pls.supports_noncoherent;
    if !is_noncoherent {
        state.has_coherent_image = true;
        if matches!(
            options.pls.fragment_sync,
            PixelLocalStorageSync::RasterizerOrderViews_D3D
                | PixelLocalStorageSync::RasterOrderGroups_Metal
        ) {
            decorations.push(Decoration::RasterOrdered);
        }
    }
    // TODO(http://anglebug.com/40096838): If after transformation we notice that the PLS is never
    // loaded, we could add a writeonly qualifier at the end.  That can be easily done by tracking
    // that in `PlaneInfo`.

    variable.type_id = type_id;
    variable.decorations = Decorations::new(decorations);
    if format != override_format {
        variable.precision = Precision::High;
    }
}

fn transform_plane_variable_to_inout(
    state: &mut State,
    variable_id: VariableId,
    format: ImageInternalFormat,
    options: &Options,
) -> TypedId {
    // The PLS variable is turned into an `inout` variable.  A global variable holds the value of
    // the pixel during the execution of the shader.
    //
    // Qualcomm seems to want fragment outputs to be 4-component vectors, and produces a compile
    // error from "inout uint".  Our Metal translator also saturates color outputs to 4 components.
    // And since the spec also seems silent on how many components an output must have, we always
    // use 4.
    let (access_variable_type, inout_variable_type) = match format {
        ImageInternalFormat::RGBA8 => (TYPE_ID_VEC4, TYPE_ID_VEC4),
        ImageInternalFormat::RGBA8I => (TYPE_ID_IVEC4, TYPE_ID_IVEC4),
        ImageInternalFormat::RGBA8UI => (TYPE_ID_UVEC4, TYPE_ID_UVEC4),
        ImageInternalFormat::R32F => (TYPE_ID_FLOAT, TYPE_ID_VEC4),
        ImageInternalFormat::R32I => (TYPE_ID_INT, TYPE_ID_IVEC4),
        ImageInternalFormat::R32UI => (TYPE_ID_UINT, TYPE_ID_UVEC4),
        _ => panic!("Internal error: Unexpected pixel local storage format"),
    };
    let inout_variable_type = state.ir_meta.get_pointer_type_id(inout_variable_type);

    // Modify the variable in-place so it continues to be referenced by the same id.
    // Only the variable's type, precision and decorations need to be changed.
    let variable = state.ir_meta.get_variable_mut(variable_id);
    let precision = variable.precision;
    variable.type_id = inout_variable_type;

    // Swap the Binding decoration with Location, and add a decoration if non-coherent.
    let binding =
        variable
            .decorations
            .decorations
            .iter()
            .find_map(|decoration| {
                if let &Decoration::Binding(binding) = decoration { Some(binding) } else { None }
            })
            .unwrap();
    // PLS attachments are bound in reverse order from the rear.
    let mut decorations = vec![
        Decoration::InputOutput,
        Decoration::Location(options.max_framebuffer_fetch_location - binding),
    ];
    debug_assert!(
        options.pls.supports_noncoherent
            || options.pls.fragment_sync != PixelLocalStorageSync::NotSupported
    );
    if options.pls.supports_noncoherent {
        // EXT_shader_framebuffer_fetch_non_coherent requires the "noncoherent" qualifier if the
        // coherent version of the extension isn't supported.  The extension also allows us to opt
        // into noncoherent accesses when PLS planes are specified as noncoherent by the shader.
        //
        // (Without EXT_shader_framebuffer_fetch_non_coherent, we just ignore the noncoherent
        // qualifier, which is a perfectly valid implementation of the PLS spec.)
        if variable.decorations.has(Decoration::NonCoherent)
            || options.pls.fragment_sync == PixelLocalStorageSync::NotSupported
        {
            decorations.push(Decoration::NonCoherent);
        }
    }
    variable.decorations = Decorations::new(decorations);

    // Create a global variable to hold the contents of the PLS plane for the duration of the
    // fragment shader.
    state
        .ir_meta
        .declare_private_variable(
            Name::new_temp("pls"),
            access_variable_type,
            precision,
            None,
            VariableScope::Global,
        )
        .1
}

fn transform_plane_variable(
    state: &mut State,
    variable_id: VariableId,
    basic_type: ImageBasicType,
    options: &Options,
) -> PlaneInfo {
    let variable = state.ir_meta.get_variable(variable_id);

    let format = variable
        .decorations
        .decorations
        .iter()
        .find_map(|decoration| {
            if let &Decoration::ImageInternalFormat(format) = decoration {
                Some(format)
            } else {
                None
            }
        })
        .unwrap();

    // TODO: Decide implementation based on the properties of the plane itself, not just what is
    // supported.  http://anglebug.com/40096838
    let implementation = options.pls.implementation;

    let access_variable = match implementation {
        PixelLocalStorageImpl::ImageLoadStore => {
            state.has_image = true;
            transform_plane_variable_to_image(state, variable_id, basic_type, format, options);
            None
        }
        PixelLocalStorageImpl::FramebufferFetch => {
            state.has_inout = true;
            Some(transform_plane_variable_to_inout(state, variable_id, format, options))
        }
        _ => panic!(
            "Internal error: Encountered pixel local storage plane, but that feature is not supported"
        ),
    };

    PlaneInfo { implementation, format, access_variable }
}

fn transform_variable(
    state: &mut State,
    variable_id: VariableId,
    options: &Options,
) -> Option<PlaneInfo> {
    let variable = state.ir_meta.get_variable(variable_id);
    let type_info = state.ir_meta.get_type(state.ir_meta.get_pointee_type(variable.type_id));

    match *type_info {
        Type::Image(
            basic_type,
            ImageType {
                dimension: ImageDimension::PixelLocal,
                is_sampled,
                is_array,
                is_ms,
                is_shadow,
            },
        ) => {
            // PLS planes are always individually declared as their own special type.
            debug_assert!(!is_sampled && !is_array && !is_ms && !is_shadow);
            Some(transform_plane_variable(state, variable_id, basic_type, options))
        }
        _ => None,
    }
}

fn declare_fragcoord_global(state: &mut State, preamble: &mut Block) -> Option<TypedId> {
    let has_image = state.has_image;
    has_image.then(|| {
        let built_in_fragcoord =
            state.ir_meta.get_or_declare_built_in_variable(BuiltIn::FragCoord).1;

        // We need to use `ivec2(floor(gl_FragCoord.xy))` as coordinates of this pixel when
        // accessing the image associated with the plane.  To avoid recalculating this every time,
        // the result of this expression is cached in a global variable.
        let fragcoord = state
            .ir_meta
            .declare_private_variable(
                Name::new_temp("fragcoord"),
                TYPE_ID_IVEC2,
                Precision::High,
                None,
                VariableScope::Global,
            )
            .1;

        let built_in_fragcoord =
            preamble.add_typed_instruction(instruction::load(state.ir_meta, built_in_fragcoord));
        let built_in_fragcoord_xy = preamble.add_typed_instruction(
            instruction::vector_component_multi(state.ir_meta, built_in_fragcoord, vec![0, 1]),
        );
        let built_in_fragcoord_xy = preamble.add_typed_instruction(instruction::built_in_unary(
            state.ir_meta,
            UnaryOpCode::Floor,
            built_in_fragcoord_xy,
        ));
        let built_in_fragcoord_xy = preamble.add_typed_instruction(instruction::construct(
            state.ir_meta,
            TYPE_ID_IVEC2,
            vec![built_in_fragcoord_xy],
        ));

        preamble.add_void_instruction(OpCode::Store(fragcoord, built_in_fragcoord_xy));
        fragcoord
    })
}

fn transform_load_by_image(
    ir_meta: &mut IRMeta,
    options: &Options,
    plane: TypedId,
    plane_info: &PlaneInfo,
    result: TypedRegisterId,
    fragcoord: TypedId,
) -> Vec<traverser::Transform> {
    // Transform:
    //
    //     result = PixelLocalLoadANGLE plane
    //
    // Into:
    //
    //     coord = Load fragcoord
    //     plane' = Load plane
    //     result = ImageLoad plane' coord
    //
    // In case supports_native_rgba8_image_formats is false, the transformation is instead:
    //
    // For RGBA8:
    //
    //     coord = Load fragcoord
    //     loaded = ImageLoad plane coord
    //     r = ExtractVectorComponent loaded 0
    //     result = UnpackUnorm4x8 r
    //
    // For RGBA8I/RGBA8UI, the expression `loaded.rrrr << uvec4(24, 16, 8, 0) >> 24u` unpacks bytes
    // out of the 32-bit integer; with the shift right preserving the sign bit (if signed).
    //
    //     coord = Load fragcoord
    //     loaded = ImageLoad plane coord
    //     rrrr = ExtractVectorComponentMulti loaded [0, 0, 0, 0]
    //     partial = BitShiftLeft rrrr gvec4(24, 16, 8, 0)
    //     result = BitShiftRight partial 24
    //
    let mut transforms = vec![];
    let coord =
        traverser::add_typed_instruction(&mut transforms, instruction::load(ir_meta, fragcoord));
    let plane =
        traverser::add_typed_instruction(&mut transforms, instruction::load(ir_meta, plane));
    let mut loaded = instruction::built_in(ir_meta, BuiltInOpCode::ImageLoad, vec![plane, coord]);

    if !options.pls.supports_native_rgba8_image_formats
        && plane_info.format == ImageInternalFormat::RGBA8
    {
        let loaded = traverser::add_typed_instruction(&mut transforms, loaded);
        let r = traverser::add_typed_instruction(
            &mut transforms,
            instruction::vector_component(ir_meta, loaded, 0),
        );
        traverser::add_typed_instruction(
            &mut transforms,
            instruction::make_with_result_id!(
                built_in_unary,
                ir_meta,
                result,
                UnaryOpCode::UnpackUnorm4x8,
                r
            ),
        );
    } else if !options.pls.supports_native_rgba8_image_formats
        && (plane_info.format == ImageInternalFormat::RGBA8I
            || plane_info.format == ImageInternalFormat::RGBA8UI)
    {
        let (shift, const24) = if plane_info.format == ImageInternalFormat::RGBA8I {
            (ir_meta.get_constant_ivec4_typed(24, 16, 8, 0), ir_meta.get_constant_int_typed(24))
        } else {
            (ir_meta.get_constant_uvec4_typed(24, 16, 8, 0), ir_meta.get_constant_uint_typed(24))
        };

        let loaded = traverser::add_typed_instruction(&mut transforms, loaded);
        let rrrr = traverser::add_typed_instruction(
            &mut transforms,
            instruction::vector_component_multi(ir_meta, loaded, vec![0, 0, 0, 0]),
        );
        let partial = traverser::add_typed_instruction(
            &mut transforms,
            instruction::bit_shift_left(ir_meta, rrrr, shift),
        );
        traverser::add_typed_instruction(
            &mut transforms,
            instruction::make_with_result_id!(bit_shift_right, ir_meta, result, partial, const24),
        );
    } else {
        loaded.override_result_id(ir_meta, result);
        traverser::add_typed_instruction(&mut transforms, loaded);
    }

    transforms
}

fn transform_load_by_framebuffer_fetch(
    ir_meta: &mut IRMeta,
    plane_info: &PlaneInfo,
    result: TypedRegisterId,
) -> Vec<traverser::Transform> {
    // Transform:
    //
    //     result = PixelLocalLoadANGLE plane
    //
    // Into:
    //
    //     result = Load plane_access
    //
    // For single-channel formats, the result is expanded to four channels:
    //
    //     result' = ConstructVectorFromMultiple [result, 0, 0, 1]
    //
    let access_variable = plane_info.access_variable.unwrap();
    if matches!(
        plane_info.format,
        ImageInternalFormat::RGBA8 | ImageInternalFormat::RGBA8I | ImageInternalFormat::RGBA8UI
    ) {
        traverser::single_typed_instruction(instruction::make_with_result_id!(
            load,
            ir_meta,
            result,
            access_variable
        ))
    } else {
        let mut transforms = vec![];
        let loaded = traverser::add_typed_instruction(
            &mut transforms,
            instruction::load(ir_meta, access_variable),
        );
        match plane_info.format {
            ImageInternalFormat::R32F => {
                traverser::add_typed_instruction(
                    &mut transforms,
                    instruction::make_with_result_id!(
                        construct,
                        ir_meta,
                        result,
                        TYPE_ID_VEC4,
                        vec![
                            loaded,
                            TYPED_CONSTANT_ID_FLOAT_ZERO,
                            TYPED_CONSTANT_ID_FLOAT_ZERO,
                            TYPED_CONSTANT_ID_FLOAT_ONE
                        ]
                    ),
                );
            }
            ImageInternalFormat::R32I => {
                traverser::add_typed_instruction(
                    &mut transforms,
                    instruction::make_with_result_id!(
                        construct,
                        ir_meta,
                        result,
                        TYPE_ID_IVEC4,
                        vec![
                            loaded,
                            TYPED_CONSTANT_ID_INT_ZERO,
                            TYPED_CONSTANT_ID_INT_ZERO,
                            TYPED_CONSTANT_ID_INT_ONE
                        ]
                    ),
                );
            }
            ImageInternalFormat::R32UI => {
                traverser::add_typed_instruction(
                    &mut transforms,
                    instruction::make_with_result_id!(
                        construct,
                        ir_meta,
                        result,
                        TYPE_ID_UVEC4,
                        vec![
                            loaded,
                            TYPED_CONSTANT_ID_UINT_ZERO,
                            TYPED_CONSTANT_ID_UINT_ZERO,
                            TYPED_CONSTANT_ID_UINT_ONE
                        ]
                    ),
                );
            }
            _ => panic!("Internal error: Unhandled pixel local storage format in load"),
        }
        transforms
    }
}

fn transform_store_by_image(
    ir_meta: &mut IRMeta,
    options: &Options,
    plane: TypedId,
    plane_info: &PlaneInfo,
    value: TypedId,
    fragcoord: TypedId,
) -> Vec<traverser::Transform> {
    // Transform:
    //
    //     PixelLocalStoreANGLE plane value
    //
    // Into:
    //
    //     coord = Load fragcoord
    //     plane' = Load plane
    //     clamped = ... value ...
    //     MemoryBarrierImage
    //     ImageStore plane' coord clamped
    //     MemoryBarrierImage
    //
    // Storing to integer formats with larger-than-representable values has different behavior on
    // the various APIs (http://anglebug.com/42265993), so value may be clamped:
    //
    // For RGBA8I:
    //
    //     clamped = Clamp value -128 127
    //
    // For RGBA8UI:
    //
    //     clamped = Min value 255
    //
    // In case supports_native_rgba8_image_formats is false, the clamped value is further
    // transformed before ImageStore:
    //
    // For RGBA8:
    //
    //     packed = PackUnorm4x8 clamped
    //     clamped' = ConstructVectorFromMultiple [packed, 0, 0, 0]
    //
    // For RGBA8I/RGBA8UI, the bits beyond 8 are cut off (only necessary for RGBA8I).  Then r, g,
    // b, and a are packed into a single 32-bit (signed or unsigned) int with
    // r | (g << 8) | (b << 16) | (a << 24)
    //
    //     lowbits = BitwiseAnd clamped 0xFF
    //     r = ExtractVectorComponent lowbits 0
    //     g = ExtractVectorComponent lowbits 1
    //     b = ExtractVectorComponent lowbits 2
    //     a = ExtractVectorComponent lowbits 3
    //     g' = BitShiftLeft g 8
    //     b' = BitShiftLeft b 16
    //     a' = BitShiftLeft a 24
    //     rg = BitwiseOr r g'
    //     rgb = BitwiseOr rg b'
    //     rgba = BitwiseOr rgb a'
    //     clamped' = ConstructVectorFromMultiple [packed, 0, 0, 0]
    //

    let mut transforms = vec![];
    let coord =
        traverser::add_typed_instruction(&mut transforms, instruction::load(ir_meta, fragcoord));
    let plane =
        traverser::add_typed_instruction(&mut transforms, instruction::load(ir_meta, plane));

    let mut clamped = match plane_info.format {
        ImageInternalFormat::RGBA8I => {
            let int8_min = ir_meta.get_constant_int_typed(-128);
            let int8_max = ir_meta.get_constant_int_typed(127);
            traverser::add_typed_instruction(
                &mut transforms,
                instruction::built_in(
                    ir_meta,
                    BuiltInOpCode::Clamp,
                    vec![value, int8_min, int8_max],
                ),
            )
        }
        ImageInternalFormat::RGBA8UI => {
            let uint8_max = ir_meta.get_constant_uint_typed(255);
            traverser::add_typed_instruction(
                &mut transforms,
                instruction::built_in_binary(ir_meta, BinaryOpCode::Min, value, uint8_max),
            )
        }
        _ => value,
    };

    if !options.pls.supports_native_rgba8_image_formats {
        if plane_info.format == ImageInternalFormat::RGBA8 {
            // unpackUnorm4x8 doesn't work on Qualcomm proprietary drivers when passed
            // a mediump vec4.  Cache in an intermediate highp vec4.  http://anglebug.com/42265995.
            let to_pack = if options.pass_highp_to_pack_unorm_snorm_built_ins {
                let (highp_value_variable, highp_value) = ir_meta.declare_private_variable(
                    Name::new_temp("highp_pls_value"),
                    TYPE_ID_VEC4,
                    Precision::High,
                    None,
                    VariableScope::Local,
                );
                transforms.push(traverser::Transform::DeclareVariable(highp_value_variable));
                traverser::add_void_instruction(
                    &mut transforms,
                    instruction::store(ir_meta, highp_value, clamped),
                );
                highp_value
            } else {
                clamped
            };
            let packed = traverser::add_typed_instruction(
                &mut transforms,
                instruction::built_in_unary(ir_meta, UnaryOpCode::PackUnorm4x8, to_pack),
            );
            clamped = traverser::add_typed_instruction(
                &mut transforms,
                instruction::construct(ir_meta, TYPE_ID_UVEC4, vec![packed]),
            );
        } else if plane_info.format == ImageInternalFormat::RGBA8I
            || plane_info.format == ImageInternalFormat::RGBA8UI
        {
            let (vec_format, const8, const16, const24, const255) =
                if plane_info.format == ImageInternalFormat::RGBA8I {
                    (
                        TYPE_ID_IVEC4,
                        ir_meta.get_constant_int_typed(8),
                        ir_meta.get_constant_int_typed(16),
                        ir_meta.get_constant_int_typed(24),
                        ir_meta.get_constant_int_typed(0xFF),
                    )
                } else {
                    (
                        TYPE_ID_UVEC4,
                        ir_meta.get_constant_uint_typed(8),
                        ir_meta.get_constant_uint_typed(16),
                        ir_meta.get_constant_uint_typed(24),
                        ir_meta.get_constant_uint_typed(0xFF),
                    )
                };

            let lowbits = if plane_info.format == ImageInternalFormat::RGBA8I {
                traverser::add_typed_instruction(
                    &mut transforms,
                    instruction::bitwise_and(ir_meta, clamped, const255),
                )
            } else {
                clamped
            };

            // Since the following need to create a 32-bit value, make sure the calculation is done
            // in highp.
            let lowbits = if lowbits.precision != Precision::High {
                let (highp_value_variable, highp_value) = ir_meta.declare_private_variable(
                    Name::new_temp("highp_pls_value"),
                    lowbits.type_id,
                    Precision::High,
                    None,
                    VariableScope::Local,
                );
                transforms.push(traverser::Transform::DeclareVariable(highp_value_variable));
                traverser::add_void_instruction(
                    &mut transforms,
                    instruction::store(ir_meta, highp_value, lowbits),
                );
                traverser::add_typed_instruction(
                    &mut transforms,
                    instruction::load(ir_meta, highp_value),
                );
                highp_value
            } else {
                lowbits
            };

            let r = traverser::add_typed_instruction(
                &mut transforms,
                instruction::vector_component(ir_meta, lowbits, 0),
            );
            let g = traverser::add_typed_instruction(
                &mut transforms,
                instruction::vector_component(ir_meta, lowbits, 1),
            );
            let b = traverser::add_typed_instruction(
                &mut transforms,
                instruction::vector_component(ir_meta, lowbits, 2),
            );
            let a = traverser::add_typed_instruction(
                &mut transforms,
                instruction::vector_component(ir_meta, lowbits, 3),
            );
            let g = traverser::add_typed_instruction(
                &mut transforms,
                instruction::bit_shift_left(ir_meta, g, const8),
            );
            let b = traverser::add_typed_instruction(
                &mut transforms,
                instruction::bit_shift_left(ir_meta, b, const16),
            );
            let a = traverser::add_typed_instruction(
                &mut transforms,
                instruction::bit_shift_left(ir_meta, a, const24),
            );
            let rg = traverser::add_typed_instruction(
                &mut transforms,
                instruction::bitwise_or(ir_meta, r, g),
            );
            let rgb = traverser::add_typed_instruction(
                &mut transforms,
                instruction::bitwise_or(ir_meta, rg, b),
            );
            let rgba = traverser::add_typed_instruction(
                &mut transforms,
                instruction::bitwise_or(ir_meta, rgb, a),
            );
            clamped = traverser::add_typed_instruction(
                &mut transforms,
                instruction::construct(ir_meta, vec_format, vec![rgba]),
            );
        }
    }

    traverser::add_void_instruction(
        &mut transforms,
        instruction::built_in(ir_meta, BuiltInOpCode::MemoryBarrierImage, vec![]),
    );
    traverser::add_void_instruction(
        &mut transforms,
        instruction::built_in(ir_meta, BuiltInOpCode::ImageStore, vec![plane, coord, clamped]),
    );
    traverser::add_void_instruction(
        &mut transforms,
        instruction::built_in(ir_meta, BuiltInOpCode::MemoryBarrierImage, vec![]),
    );

    transforms
}

fn transform_store_by_framebuffer_fetch(
    ir_meta: &mut IRMeta,
    plane_info: &PlaneInfo,
    value: TypedId,
) -> Vec<traverser::Transform> {
    // Transform:
    //
    //     PixelLocalStoreANGLE plane value
    //
    // Into:
    //
    //     Store plane_access value
    //
    // For single-channel formats, the value is first swizzled to get the first channel.
    //
    //     value' = ExtractVectorComponent value 0
    //
    let access_variable = plane_info.access_variable.unwrap();
    let mut transforms = vec![];
    let value = match plane_info.format {
        ImageInternalFormat::R32F | ImageInternalFormat::R32I | ImageInternalFormat::R32UI => {
            traverser::add_typed_instruction(
                &mut transforms,
                instruction::vector_component(ir_meta, value, 0),
            )
        }
        _ => value,
    };
    traverser::add_void_instruction(
        &mut transforms,
        instruction::store(ir_meta, access_variable, value),
    );
    transforms
}

// Assuming `register_id` is the result of a `Load` operation, return the variable the load was
// done from.  PLS planes cannot be arrays, so every usage of a PLS plane must directly be the
// result of `Load` from the PLS uniform variable.
fn get_loaded_pls_variable(ir_meta: &IRMeta, register_id: RegisterId) -> TypedId {
    if let OpCode::Load(variable) = ir_meta.get_instruction(register_id).op {
        variable
    } else {
        panic!(
            "Internal error: Expected pixel local storage built-in's first parameter to be a plain variable"
        );
    }
}

fn transform_pls_load_store(
    state: &mut State,
    options: &Options,
    instruction: &BlockInstruction,
    fragcoord: Option<TypedId>,
) -> Vec<traverser::Transform> {
    let (opcode, result) = instruction.get_op_and_result(state.ir_meta);
    match opcode {
        &OpCode::Load(plane) => {
            if let Id::Variable(plane_variable_id) = plane.id
                && (plane_variable_id.id as usize) < state.planes.len()
            {
                let plane_info = &state.planes[plane_variable_id.id as usize];
                if plane_info.implementation != PixelLocalStorageImpl::NotSupported {
                    // Remove the Load instruction.  If the emulation needs it, it will Load again.
                    vec![traverser::Transform::Remove]
                } else {
                    vec![]
                }
            } else {
                vec![]
            }
        }
        &OpCode::Unary(UnaryOpCode::PixelLocalLoadANGLE, plane) => {
            let plane = get_loaded_pls_variable(state.ir_meta, plane.id.get_register());
            let plane_info = &state.planes[plane.id.get_variable().id as usize];
            if plane_info.implementation == PixelLocalStorageImpl::ImageLoadStore {
                let plane = TypedId::from_variable_id(state.ir_meta, plane.id.get_variable());
                transform_load_by_image(
                    state.ir_meta,
                    options,
                    plane,
                    plane_info,
                    result.unwrap(),
                    fragcoord.unwrap(),
                )
            } else {
                transform_load_by_framebuffer_fetch(state.ir_meta, plane_info, result.unwrap())
            }
        }
        OpCode::BuiltIn(BuiltInOpCode::PixelLocalStoreANGLE, args) => {
            let value = args[1];
            let plane = args[0];
            let plane = get_loaded_pls_variable(state.ir_meta, plane.id.get_register());
            let plane_info = &state.planes[plane.id.get_variable().id as usize];
            if plane_info.implementation == PixelLocalStorageImpl::ImageLoadStore {
                let plane = TypedId::from_variable_id(state.ir_meta, plane.id.get_variable());
                transform_store_by_image(
                    state.ir_meta,
                    options,
                    plane,
                    plane_info,
                    value,
                    fragcoord.unwrap(),
                )
            } else {
                transform_store_by_framebuffer_fetch(state.ir_meta, plane_info, value)
            }
        }
        _ => vec![],
    }
}

fn add_pre_and_post_pls_code_for_image(
    state: &mut State,
    options: &Options,
    preamble: &mut Block,
    postamble: &mut Block,
) {
    // When PLS is implemented with images, early_fragment_tests ensure that depth/stencil
    // can also block stores to PLS.
    state.ir_meta.set_early_fragment_tests(true);

    if state.has_coherent_image {
        // To make pixel local storage coherent, delimit the beginning of a per-pixel critical
        // section, if supported, by either of the following:
        //
        //     beginInvocationInterlockNV (GL_NV_fragment_shader_interlock)
        //     beginFragmentShaderOrderingINTEL (GL_INTEL_fragment_shader_ordering)
        //     beginInvocationInterlockARB (GL_ARB_fragment_shader_interlock and
        //                                  SPV_EXT_fragment_shader_interlock)
        //
        // Delimit the end of the PLS critical section similarly by:
        //
        //     endInvocationInterlockNV (GL_NV_fragment_shader_interlock)
        //     endInvocationInterlockARB (GL_ARB_fragment_shader_interlock and
        //                                SPV_EXT_fragment_shader_interlock)
        match options.pls.fragment_sync {
            PixelLocalStorageSync::RasterizerOrderViews_D3D
            | PixelLocalStorageSync::RasterOrderGroups_Metal
            | PixelLocalStorageSync::NotSupported => {
                // Raster ordered resources don't need explicit synchronization calls.
            }
            PixelLocalStorageSync::FragmentShaderInterlockNV_GL => {
                preamble.add_instruction(instruction::built_in(
                    state.ir_meta,
                    BuiltInOpCode::BeginInvocationInterlockNV,
                    vec![],
                ));
                postamble.add_instruction(instruction::built_in(
                    state.ir_meta,
                    BuiltInOpCode::EndInvocationInterlockNV,
                    vec![],
                ));
            }
            PixelLocalStorageSync::FragmentShaderInterlockARB_GL => {
                preamble.add_instruction(instruction::built_in(
                    state.ir_meta,
                    BuiltInOpCode::BeginInvocationInterlockARB,
                    vec![],
                ));
                postamble.add_instruction(instruction::built_in(
                    state.ir_meta,
                    BuiltInOpCode::EndInvocationInterlockARB,
                    vec![],
                ));
            }
            PixelLocalStorageSync::FragmentShaderOrderingINTEL_GL => {
                preamble.add_instruction(instruction::built_in(
                    state.ir_meta,
                    BuiltInOpCode::BeginFragmentShaderOrderingINTEL,
                    vec![],
                ));
                // GL_INTEL_fragment_shader_ordering doesn't have an "end()" call.
            }
            PixelLocalStorageSync::Automatic => {
                panic!(
                    "Internal error: pixel local storage implemented by storage images cannot be automatically coherent"
                );
            }
            _ => {
                panic!("Internal error: unexpected pixel local storage sync type");
            }
        }
    }
}

fn add_pre_and_post_pls_code_for_framebuffer_fetch(
    state: &mut State,
    global_variables: &[VariableId],
    preamble: &mut Block,
    postamble: &mut Block,
) {
    // [OpenGL ES Version 3.0.6, 3.9.2.3 "Shader Output"]: Any colors, or color components,
    // associated with a fragment that are not written by the fragment shader are undefined.
    //
    // [EXT_shader_framebuffer_fetch]: Prior to fragment shading, fragment outputs declared inout
    // are populated with the value last written to the framebuffer at the same(x, y, sample)
    // position.
    //
    // It's unclear from the EXT_shader_framebuffer_fetch spec whether inout fragment variables
    // become undefined if not explicitly written, but either way, when this compiles to subpass
    // loads in Vulkan, we definitely get undefined behavior if PLS variables are not written.
    //
    // To make sure every PLS variable gets written, we read them all before PLS operations, then
    // write them all back out after all PLS is complete.
    global_variables.iter().for_each(|&id| {
        let plane_info = &state.planes[id.id as usize];
        if plane_info.implementation == PixelLocalStorageImpl::FramebufferFetch {
            // In the beginning of the shader, load from the inout variable and assign to the
            // global:
            //
            //     initial_value = Load inout
            //     Store access_variable initial_value
            //
            // For single-channel formats, the inout variable is turned to gvec4 but the cached
            // global variable is still single-channel.
            //
            let inout_variable = TypedId::from_variable_id(state.ir_meta, id);
            let inout_access = if matches!(
                plane_info.format,
                ImageInternalFormat::R32F | ImageInternalFormat::R32I | ImageInternalFormat::R32UI
            ) {
                preamble.add_typed_instruction(instruction::vector_component(
                    state.ir_meta,
                    inout_variable,
                    0,
                ))
            } else {
                inout_variable
            };
            let access_variable = plane_info.access_variable.unwrap();

            let initial_value =
                preamble.add_typed_instruction(instruction::load(state.ir_meta, inout_access));
            preamble.add_instruction(instruction::store(
                state.ir_meta,
                access_variable,
                initial_value,
            ));

            // At the end of the shader, load from the global and assign back to the inout
            // variable:
            //
            //     final_value = Load access_variable
            //     Store inout final_value
            //
            // The AccessVectorComponent instruction to access the R channel for single-channel
            // formats is *not* repeated in the postamble.  For the foreseeable future, both blocks
            // will be placed in the same function.
            let final_value =
                postamble.add_typed_instruction(instruction::load(state.ir_meta, access_variable));
            postamble.add_instruction(instruction::store(state.ir_meta, inout_access, final_value));
        }
    });
}
