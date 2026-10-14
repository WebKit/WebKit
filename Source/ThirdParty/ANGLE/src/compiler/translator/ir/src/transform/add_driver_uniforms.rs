// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Add a set of driver uniforms.  There are minor differences between the driver uniforms needed for
// the different generators, which is communicated via options, but the uniforms are otherwise
// largely the same between them.
use crate::ir::*;
use crate::*;

pub struct Options {
    // Declare the uniforms as a struct.  Otherwise an interface block is declared.
    pub declare_as_struct: bool,
    // Whether the transformXY graphics uniform is needed.
    pub add_transform_xy: bool,
    // Whether the xfbBufferOffsets and xfbVerticesPerInstance graphics uniforms are needed.
    pub add_xfb_emulation: bool,
    // Whether the coverageMask graphics uniform is needed.
    pub add_coverage_mask: bool,
}

// The `misc` field is packed with various bits.  The following must match
// `kDriverUniformsMisc*Offset/Mask` values in ShaderLang.h:
const MISC_SWAP_XY_OFFSET: u32 = 0;
const MISC_SWAP_XY_MASK: u32 = 0x1;
const MISC_ADVANCED_BLEND_EQUATION_OFFSET: u32 = 1;
const MISC_ADVANCED_BLEND_EQUATION_MASK: u32 = 0x1F;
const MISC_SAMPLE_COUNT_OFFSET: u32 = 6;
const MISC_SAMPLE_COUNT_MASK: u32 = 0x3F;
const MISC_ENABLED_CLIP_PLANES_OFFSET: u32 = 12;
const MISC_ENABLED_CLIP_PLANES_MASK: u32 = 0xFF;
const MISC_TRANSFORM_DEPTH_OFFSET: u32 = 20;
const MISC_TRANSFORM_DEPTH_MASK: u32 = 0x1;
const MISC_ALPHA_TO_COVERAGE_OFFSET: u32 = 21;
const MISC_ALPHA_TO_COVERAGE_MASK: u32 = 0x1;
const MISC_LAYERED_FRAMEBUFFER_OFFSET: u32 = 22;
const MISC_LAYERED_FRAMEBUFFER_MASK: u32 = 0x1;

struct DriverUniformFields {
    // Field index of the uniforms, cached during creation to avoid look up.
    depth_range: Option<u32>,
    render_area: Option<u32>,
    flip_xy: Option<u32>,
    misc: Option<u32>,
    base_instance: Option<u32>,
    acb_buffer_offsets: Option<u32>,
    transform_xy: Option<u32>,
    xfb_buffer_offsets: Option<u32>,
    xfb_vertices_per_instance: Option<u32>,
    coverage_mask: Option<u32>,
}

pub struct DriverUniforms {
    // The driver uniforms instance
    uniforms_variable_id: TypedId,
    // The type of the emulated gl_DepthRangeParams struct
    depth_range_params_type_id: TypeId,
    field_index: DriverUniformFields,
}

pub fn run(ir: &mut IR, options: &Options) -> DriverUniforms {
    // Declare the driver uniform's block based on shader type (compute vs graphics)
    let (uniforms_block_type_id, field_index) = if ir.meta.get_shader_type() == ShaderType::Compute
    {
        make_compute_driver_uniforms_type(&mut ir.meta, options)
    } else {
        make_graphics_driver_uniforms_type(&mut ir.meta, options)
    };

    // Declare the block itself.
    //
    // The uniforms are normally named `ANGLEUniforms`, but if asked to be added as a struct, the
    // variable name is `ANGLE_angleUniforms` due to the assumptions in the MSL generator in AST.
    // The names can be unified once MSL is directly generated from IR and the assumptions are
    // relaxed.  TODO(http://anglebug.com/349994211)
    let (name, decorations) = if options.declare_as_struct {
        ("ANGLE_angleUniforms", Decorations::new(vec![Decoration::Uniform]))
    } else {
        (
            "ANGLEUniforms",
            Decorations::new(vec![
                Decoration::Uniform,
                Decoration::PushConstant,
                Decoration::Block(BlockStorage::Std140),
            ]),
        )
    };
    let (uniforms_variable_id, uniforms_id) = ir.meta.declare_variable(
        Name::new_exact(name),
        uniforms_block_type_id,
        Precision::NotApplicable,
        false,
        decorations,
        None,
        None,
        VariableScope::Global,
    );
    ir.meta.get_variable_mut(uniforms_variable_id).is_static_use = true;

    let depth_range_params_type_id = declare_depth_range_struct_type(&mut ir.meta);

    DriverUniforms { uniforms_variable_id: uniforms_id, depth_range_params_type_id, field_index }
}

fn uniform_field(name: &'static str, type_id: TypeId) -> Field {
    Field {
        name: Name::new_exact_struct_field(name),
        type_id,
        // Currently, all driver uniforms are highp.  Any that have a smaller range are packed.
        precision: Precision::High,
        precise: false,
        decorations: Decorations::new_none(),
        is_static_use: true,
    }
}

fn make_graphics_driver_uniforms_type(
    ir_meta: &mut IRMeta,
    options: &Options,
) -> (TypeId, DriverUniformFields) {
    let mut fields = Vec::with_capacity(16);
    let mut next_field_index = 0;

    let mut add_field = |name, type_id, index: &mut Option<u32>| {
        let next_index = next_field_index;

        fields.push(uniform_field(name, type_id));
        *index = Some(next_index);

        next_field_index += 1;
    };

    let mut field_index = DriverUniformFields {
        depth_range: None,
        render_area: None,
        flip_xy: None,
        misc: None,
        base_instance: None,
        acb_buffer_offsets: None,
        transform_xy: None,
        xfb_buffer_offsets: None,
        xfb_vertices_per_instance: None,
        coverage_mask: None,
    };

    // Name must match kDriverUniformsBlockName in cpp until IR port is complete.
    let name = Name::new_exact("ANGLEUniformBlock");

    // The names must match those in DriverUniform.cpp until IR port is complete.
    // depthRange: Near and far depth
    add_field("depthRange", TYPE_ID_VEC2, &mut field_index.depth_range);
    // renderArea: Packed ushort2
    add_field("renderArea", TYPE_ID_UINT, &mut field_index.render_area);
    // flipXY: Packed snorm4
    add_field("flipXY", TYPE_ID_UINT, &mut field_index.flip_xy);
    // misc: Various bits of state
    add_field("misc", TYPE_ID_UINT, &mut field_index.misc);
    // baseInstance: int
    add_field("baseInstance", TYPE_ID_INT, &mut field_index.base_instance);
    // acbBufferOffsets: Packed ubyte8
    add_field("acbBufferOffsets", TYPE_ID_UVEC2, &mut field_index.acb_buffer_offsets);
    if options.add_transform_xy {
        add_field("transformXY", TYPE_ID_VEC4, &mut field_index.transform_xy);
    }
    let mut padding = 4;
    if options.add_xfb_emulation {
        add_field("xfbBufferOffsets", TYPE_ID_IVEC4, &mut field_index.xfb_buffer_offsets);
        add_field(
            "xfbVerticesPerInstance",
            TYPE_ID_INT,
            &mut field_index.xfb_vertices_per_instance,
        );
        padding -= 1;
    }
    if options.add_coverage_mask {
        add_field("coverageMask", TYPE_ID_UINT, &mut field_index.coverage_mask);
        padding -= 1;
    }
    if padding < 4 {
        debug_assert!(padding == 2 || padding == 3);
        if padding == 3 {
            // Need two fields (uint and uvec2) to avoid implicit padding.  A uvec3 would cause
            // padding to be added between it and the previous uint for std140 alignment reasons.
            fields.push(uniform_field("unused2", TYPE_ID_UINT));
        }
        fields.push(uniform_field("unused", TYPE_ID_UVEC2));
    }

    let specialization = if options.declare_as_struct {
        StructSpecialization::Struct
    } else {
        StructSpecialization::InterfaceBlock
    };

    (ir_meta.get_struct_type_id(name, fields, specialization), field_index)
}

fn make_compute_driver_uniforms_type(
    ir_meta: &mut IRMeta,
    options: &Options,
) -> (TypeId, DriverUniformFields) {
    let field_index = DriverUniformFields {
        depth_range: None,
        render_area: None,
        flip_xy: None,
        misc: None,
        base_instance: None,
        acb_buffer_offsets: Some(0),
        transform_xy: None,
        xfb_buffer_offsets: None,
        xfb_vertices_per_instance: None,
        coverage_mask: None,
    };

    // Name must match kDriverUniformsBlockName in cpp until IR port is complete.
    let name = Name::new_exact("ANGLEUniformBlock");

    // The names must match those in DriverUniform.cpp until IR port is complete.
    let fields = vec![
        // acbBufferOffsets: Packed ubyte8, 2 uints padding
        uniform_field("acbBufferOffsets", TYPE_ID_UVEC4),
    ];

    // Struct is used only by generators that don't support compute shaders.
    debug_assert!(!options.declare_as_struct);
    let specialization = StructSpecialization::InterfaceBlock;

    (ir_meta.get_struct_type_id(name, fields, specialization), field_index)
}

fn declare_depth_range_struct_type(ir_meta: &mut IRMeta) -> TypeId {
    let name = Name::new_exact("ANGLEDepthRangeParams");

    // The names must match gl_DepthRangeParams's.
    let fields = vec![
        uniform_field("near", TYPE_ID_FLOAT),
        uniform_field("far", TYPE_ID_FLOAT),
        uniform_field("diff", TYPE_ID_FLOAT),
    ];

    ir_meta.get_struct_type_id(name, fields, StructSpecialization::Struct)
}

impl DriverUniforms {
    fn load_field(
        &self,
        ir_meta: &mut IRMeta,
        field_index: u32,
        transforms: &mut Vec<traverser::Transform>,
    ) -> TypedId {
        // Generate the following:
        //
        //     field  = AccessStructField uniforms index
        //     field' = Load field
        let field = traverser::add_typed_instruction(
            transforms,
            instruction::make!(struct_field, ir_meta, self.uniforms_variable_id, field_index),
        );
        traverser::add_typed_instruction(transforms, instruction::make!(load, ir_meta, field))
    }

    pub fn depth_range(
        &self,
        ir_meta: &mut IRMeta,
        transforms: &mut Vec<traverser::Transform>,
    ) -> TypedId {
        let field = self.load_field(ir_meta, self.field_index.depth_range.unwrap(), transforms);
        // The depth range uniform is a vec2, with `near` in the first and `far` in the second
        // component.  The expected result is a struct of gl_DepthRangeParams type with these fields
        // as well as a `diff` field.
        //
        // Generate the following:
        //
        //    near  = ExtractVectorComponent field 0
        //    far   = ExtractVectorComponent field 1
        //    diff  = Sub far near
        //    range = ConstructStruct (near far diff)  // gl_DepthRangeParams
        let near = traverser::add_typed_instruction(
            transforms,
            instruction::make!(vector_component, ir_meta, field, 0),
        );
        let far = traverser::add_typed_instruction(
            transforms,
            instruction::make!(vector_component, ir_meta, field, 1),
        );
        let diff = traverser::add_typed_instruction(
            transforms,
            instruction::make!(sub, ir_meta, far, near),
        );
        traverser::add_typed_instruction(
            transforms,
            instruction::make!(
                construct,
                ir_meta,
                self.depth_range_params_type_id,
                vec![near, far, diff],
                None
            ),
        )
    }

    pub fn half_render_area(
        &self,
        ir_meta: &mut IRMeta,
        transforms: &mut Vec<traverser::Transform>,
    ) -> TypedId {
        let field = self.load_field(ir_meta, self.field_index.render_area.unwrap(), transforms);
        // The render area is packed as two ushort values in one uint.  They are extracted and
        // converted to float, then bundled in a vec2.  The result is multiplied by 0.5, because
        // that's the only use of the render area.
        //
        // Generate the following:
        //
        //    width    = BitwiseAnd field 0xFFFF
        //    height   = BitShiftRight field 16
        //    width'   = ConstructScalarFromScalar width              // float
        //    height'  = ConstructScalarFromScalar height             // float
        //    area     = ConstructVectorFromMultiple (width, height)  // vec2
        //    halfArea = VectorTimesScalar area 0.5
        let width_mask = ir_meta.get_constant_uint_typed(0xFFFF, Precision::High);
        let height_shift = ir_meta.get_constant_uint_typed(16, Precision::High);
        let width = traverser::add_typed_instruction(
            transforms,
            instruction::make!(bitwise_and, ir_meta, field, width_mask),
        );
        let height = traverser::add_typed_instruction(
            transforms,
            instruction::make!(bit_shift_right, ir_meta, field, height_shift),
        );
        let width = traverser::add_typed_instruction(
            transforms,
            instruction::make!(construct, ir_meta, TYPE_ID_FLOAT, vec![width], None),
        );
        let height = traverser::add_typed_instruction(
            transforms,
            instruction::make!(construct, ir_meta, TYPE_ID_FLOAT, vec![height], None),
        );
        let area = traverser::add_typed_instruction(
            transforms,
            instruction::make!(construct, ir_meta, TYPE_ID_VEC2, vec![width, height], None),
        );
        traverser::add_typed_instruction(
            transforms,
            instruction::make!(vector_times_scalar, ir_meta, area, TYPED_CONSTANT_ID_FLOAT_HALF),
        )
    }

    pub fn flip_xy(
        &self,
        ir_meta: &mut IRMeta,
        transforms: &mut Vec<traverser::Transform>,
    ) -> TypedId {
        let field = self.load_field(ir_meta, self.field_index.flip_xy.unwrap(), transforms);
        // The flipXY uniform is packed as four 8-bit snorm values in one uint.  The first two
        // components are used for the fragment shader and the last two for pre-rasterization
        // stages.
        //
        // Generate the following:
        //
        //    flipxy   = UnpackSnorm4x8 field
        //    // If fragment shader:
        //    flipxy   = ExtractVectorComponentMulti flipxy (0, 1)
        //    // Otherwise:
        //    flipxy   = ExtractVectorComponentMulti flipxy (2, 3)
        let flipxy = traverser::add_typed_instruction(
            transforms,
            instruction::make!(built_in_unary, ir_meta, UnaryOpCode::UnpackSnorm4x8, field),
        );
        let swizzle =
            if ir_meta.get_shader_type() == ShaderType::Fragment { vec![0, 1] } else { vec![2, 3] };
        traverser::add_typed_instruction(
            transforms,
            instruction::make!(vector_component_multi, ir_meta, flipxy, swizzle),
        )
    }

    pub fn flip_xy_upside_down(
        &self,
        ir_meta: &mut IRMeta,
        transforms: &mut Vec<traverser::Transform>,
    ) -> TypedId {
        // Same as flipXY, but with Y flipped.
        //
        // Generate the following:
        //
        //    flipxy_upside_down = Mul flipxy vec2(1,-1)
        let flipxy = self.flip_xy(ir_meta, transforms);
        let multiplier = ir_meta.get_constant_composite_typed(
            TYPE_ID_VEC2,
            vec![CONSTANT_ID_FLOAT_ONE, CONSTANT_ID_FLOAT_NEGATIVE_ONE],
            Precision::Low,
        );
        traverser::add_typed_instruction(
            transforms,
            instruction::make!(mul, ir_meta, flipxy, multiplier),
        )
    }

    fn misc(
        &self,
        ir_meta: &mut IRMeta,
        offset: u32,
        mask: u32,
        transforms: &mut Vec<traverser::Transform>,
    ) -> TypedId {
        let mut field = self.load_field(ir_meta, self.field_index.misc.unwrap(), transforms);
        // Helper to extract a uniform out of the `misc` field.
        //
        // Generate the following:
        //
        //    // Apply offset only if not zero:
        //    field'  = BitShiftRight field offset
        //    field'' = BitwiseAnd field' mask
        if offset != 0 {
            let offset = ir_meta.get_constant_uint_typed(offset, Precision::Low);
            field = traverser::add_typed_instruction(
                transforms,
                instruction::make!(bit_shift_right, ir_meta, field, offset),
            );
        }
        let mask = ir_meta.get_constant_uint_typed(mask, Precision::Low);
        traverser::add_typed_instruction(
            transforms,
            instruction::make!(bitwise_and, ir_meta, field, mask),
        )
    }

    fn misc_bool(
        &self,
        ir_meta: &mut IRMeta,
        offset: u32,
        mask: u32,
        transforms: &mut Vec<traverser::Transform>,
    ) -> TypedId {
        // Get a bit out of the `misc` field.  Similar to the `misc()` helper, but with an added
        // cast to bool.
        //
        // Generate:
        //
        //    field' = ConstructScalarFromScalar field // bool
        let field = self.misc(ir_meta, offset, mask, transforms);
        traverser::add_typed_instruction(
            transforms,
            instruction::make!(construct, ir_meta, TYPE_ID_BOOL, vec![field], None),
        )
    }

    pub fn swap_xy(
        &self,
        ir_meta: &mut IRMeta,
        transforms: &mut Vec<traverser::Transform>,
    ) -> TypedId {
        self.misc_bool(ir_meta, MISC_SWAP_XY_OFFSET, MISC_SWAP_XY_MASK, transforms)
    }
    pub fn advanced_blend_equation(
        &self,
        ir_meta: &mut IRMeta,
        transforms: &mut Vec<traverser::Transform>,
    ) -> TypedId {
        self.misc_bool(
            ir_meta,
            MISC_ADVANCED_BLEND_EQUATION_OFFSET,
            MISC_ADVANCED_BLEND_EQUATION_MASK,
            transforms,
        )
    }
    pub fn sample_count(
        &self,
        ir_meta: &mut IRMeta,
        transforms: &mut Vec<traverser::Transform>,
    ) -> TypedId {
        self.misc(ir_meta, MISC_SAMPLE_COUNT_OFFSET, MISC_SAMPLE_COUNT_MASK, transforms)
    }
    pub fn enabled_clip_planes(
        &self,
        ir_meta: &mut IRMeta,
        transforms: &mut Vec<traverser::Transform>,
    ) -> TypedId {
        self.misc(
            ir_meta,
            MISC_ENABLED_CLIP_PLANES_OFFSET,
            MISC_ENABLED_CLIP_PLANES_MASK,
            transforms,
        )
    }
    pub fn transform_depth(
        &self,
        ir_meta: &mut IRMeta,
        transforms: &mut Vec<traverser::Transform>,
    ) -> TypedId {
        self.misc_bool(ir_meta, MISC_TRANSFORM_DEPTH_OFFSET, MISC_TRANSFORM_DEPTH_MASK, transforms)
    }
    pub fn alpha_to_coverage(
        &self,
        ir_meta: &mut IRMeta,
        transforms: &mut Vec<traverser::Transform>,
    ) -> TypedId {
        self.misc_bool(
            ir_meta,
            MISC_ALPHA_TO_COVERAGE_OFFSET,
            MISC_ALPHA_TO_COVERAGE_MASK,
            transforms,
        )
    }
    pub fn layered_framebuffer(
        &self,
        ir_meta: &mut IRMeta,
        transforms: &mut Vec<traverser::Transform>,
    ) -> TypedId {
        self.misc_bool(
            ir_meta,
            MISC_LAYERED_FRAMEBUFFER_OFFSET,
            MISC_LAYERED_FRAMEBUFFER_MASK,
            transforms,
        )
    }

    pub fn base_instance(
        &self,
        ir_meta: &mut IRMeta,
        transforms: &mut Vec<traverser::Transform>,
    ) -> TypedId {
        self.load_field(ir_meta, self.field_index.base_instance.unwrap(), transforms)
    }

    pub fn acb_buffer_offsets(
        &self,
        ir_meta: &mut IRMeta,
        transforms: &mut Vec<traverser::Transform>,
    ) -> TypedId {
        self.load_field(ir_meta, self.field_index.acb_buffer_offsets.unwrap(), transforms)
    }

    pub fn transform_xy(
        &self,
        ir_meta: &mut IRMeta,
        transforms: &mut Vec<traverser::Transform>,
    ) -> TypedId {
        self.load_field(ir_meta, self.field_index.transform_xy.unwrap(), transforms)
    }

    pub fn xfb_buffer_offsets(
        &self,
        ir_meta: &mut IRMeta,
        transforms: &mut Vec<traverser::Transform>,
    ) -> TypedId {
        self.load_field(ir_meta, self.field_index.xfb_buffer_offsets.unwrap(), transforms)
    }

    pub fn xfb_vertices_per_instance(
        &self,
        ir_meta: &mut IRMeta,
        transforms: &mut Vec<traverser::Transform>,
    ) -> TypedId {
        self.load_field(ir_meta, self.field_index.xfb_vertices_per_instance.unwrap(), transforms)
    }

    pub fn coverage_mask(
        &self,
        ir_meta: &mut IRMeta,
        transforms: &mut Vec<traverser::Transform>,
    ) -> TypedId {
        self.load_field(ir_meta, self.field_index.coverage_mask.unwrap(), transforms)
    }
}
