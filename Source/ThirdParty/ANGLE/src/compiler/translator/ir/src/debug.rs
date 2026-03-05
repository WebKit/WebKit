// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Debug utilities.  Notable, the IR can be dumped for inspection.

use crate::ir::*;
use crate::*;

fn register_id_str(id: RegisterId) -> String {
    format!("r{}", id.id)
}

fn constant_id_str(id: ConstantId) -> String {
    format!("c{}", id.id)
}

fn variable_id_str(id: VariableId) -> String {
    format!("v{}", id.id)
}

fn function_id_str(id: FunctionId) -> String {
    format!("f{}", id.id)
}

fn type_id_str(id: TypeId) -> String {
    format!("t{}", id.id)
}

fn precision_str(precision: Precision) -> &'static str {
    match precision {
        Precision::NotApplicable => "",
        Precision::Low => "lowp",
        Precision::Medium => "mediump",
        Precision::High => "highp",
    }
}

fn id_str(id: TypedId) -> String {
    match id.id {
        Id::Register(rid) => register_id_str(rid),
        Id::Constant(cid) => {
            // Constants normally do not have an associated precision, but if a constant replaces a
            // `const` variable, its use as an operand of an instruction would be associated with a
            // precision.  In that case, output the precision too.
            let constant_str = constant_id_str(cid);
            match id.precision {
                Precision::NotApplicable => constant_str,
                _ => format!("{constant_str}[{}]", precision_str(id.precision)),
            }
        }
        Id::Variable(vid) => variable_id_str(vid),
    }
}

fn id_list_str(ids: &[TypedId]) -> String {
    ids.iter().map(|&id| id_str(id)).collect::<Vec<_>>().join(", ")
}

fn index_list_str(ids: &[u32]) -> String {
    ids.iter().map(|index| index.to_string()).collect::<Vec<_>>().join(", ")
}

fn shader_type_str(shader_type: ShaderType) -> String {
    (match shader_type {
        ShaderType::Vertex => "Vertex Shader",
        ShaderType::TessellationControl => "Tessellation Control Shader",
        ShaderType::TessellationEvaluation => "Tessellation Evaluation Shader",
        ShaderType::Geometry => "Geometry Shader",
        ShaderType::Fragment => "Fragment Shader",
        ShaderType::Compute => "Compute Shader",
    })
    .to_string()
}

fn early_fragment_tests_str(early_fragment_tests: bool) -> String {
    (if early_fragment_tests { "[Early fragment tests]" } else { "" }).to_string()
}

fn blend_equation_advanced_str(equations: &AdvancedBlendEquations) -> String {
    if equations.all() {
        return "[Advanced Blend Equations: All]".to_string();
    }

    let mut equations_list = Vec::new();
    if equations.multiply {
        equations_list.push("Multiply");
    }
    if equations.screen {
        equations_list.push("Screen");
    }
    if equations.overlay {
        equations_list.push("Overlay");
    }
    if equations.darken {
        equations_list.push("Darken");
    }
    if equations.lighten {
        equations_list.push("Lighten");
    }
    if equations.colordodge {
        equations_list.push("Color Dodge");
    }
    if equations.colorburn {
        equations_list.push("Color Burn");
    }
    if equations.hardlight {
        equations_list.push("Hard Light");
    }
    if equations.softlight {
        equations_list.push("Soft Light");
    }
    if equations.difference {
        equations_list.push("Difference");
    }
    if equations.exclusion {
        equations_list.push("Exclusion");
    }
    if equations.hsl_hue {
        equations_list.push("HSL Hue");
    }
    if equations.hsl_saturation {
        equations_list.push("HSL Saturation");
    }
    if equations.hsl_color {
        equations_list.push("HSL Color");
    }
    if equations.hsl_luminosity {
        equations_list.push("HSL Luminosity");
    }

    if equations_list.is_empty() {
        return String::new();
    }

    let indent = "    ".to_string();
    let delim = format!(",\n{indent}");
    format!("[Advanced Blend Equations:\n{indent}{}]", equations_list.join(&delim))
}

fn tcs_vertices_str(vertices: u32) -> String {
    format!("[Vertices: {vertices}]")
}

fn tes_primitive_str(primitive: TessellationPrimitive) -> String {
    format!(
        "[Primitive: {}]",
        match primitive {
            TessellationPrimitive::Undefined => "Undefined",
            TessellationPrimitive::Triangles => "Triangles",
            TessellationPrimitive::Quads => "Quads",
            TessellationPrimitive::Isolines => "Isolines",
        }
    )
}

fn tes_vertex_spacing_str(vertex_spacing: TessellationSpacing) -> String {
    format!(
        "[Vertex Spacing: {}]",
        match vertex_spacing {
            TessellationSpacing::Undefined => "Undefined",
            TessellationSpacing::EqualSpacing => "Equal",
            TessellationSpacing::FractionalEvenSpacing => "Fractional Even",
            TessellationSpacing::FractionalOddSpacing => "Fractional Odd",
        }
    )
}

fn tes_ordering_str(ordering: TessellationOrdering) -> String {
    format!(
        "[Ordering: {}]",
        match ordering {
            TessellationOrdering::Undefined => "Undefined",
            TessellationOrdering::Cw => "CW",
            TessellationOrdering::Ccw => "CCW",
        }
    )
}

fn tes_point_mode_str(point_mode: bool) -> String {
    (if point_mode { "[Point Mode]" } else { "" }).to_string()
}

fn gs_primitive_str(primitive: GeometryPrimitive, direction: &str) -> String {
    format!(
        "[Primitive {}: {}]",
        direction,
        match primitive {
            GeometryPrimitive::Undefined => "Undefined",
            GeometryPrimitive::Points => "Points",
            GeometryPrimitive::Lines => "Lines",
            GeometryPrimitive::LinesAdjacency => "Lines Adjacency",
            GeometryPrimitive::Triangles => "Triangles",
            GeometryPrimitive::TrianglesAdjacency => "Triangles Adjacency",
            GeometryPrimitive::LineStrip => "Line Strip",
            GeometryPrimitive::TriangleStrip => "Triangle Strip",
        }
    )
}

fn gs_invocations_str(invocations: u32) -> String {
    format!("[Invocations: {invocations}]")
}

fn gs_max_vertices_str(max_vertices: u32) -> String {
    format!("[Max Vertices: {max_vertices}]")
}

fn basic_type_str(basic: BasicType) -> String {
    (match basic {
        BasicType::Void => "void",
        BasicType::Float => "float",
        BasicType::Int => "int",
        BasicType::Uint => "uint",
        BasicType::Bool => "bool",
        BasicType::AtomicCounter => "atomic_uint",
        BasicType::YuvCscStandard => "yuvCscStandardEXT",
    })
    .to_string()
}

fn image_type_str(basic_type: ImageBasicType, image_type: ImageType) -> String {
    let prefix = match basic_type {
        ImageBasicType::Float => "",
        ImageBasicType::Int => "i",
        ImageBasicType::Uint => "u",
    };
    let mut base_name = if image_type.is_sampled { "sampler" } else { "image" };
    let suffix = match image_type.dimension {
        ImageDimension::D2 => "2D",
        ImageDimension::D3 => "3D",
        ImageDimension::Cube => "Cube",
        ImageDimension::Rect => "Rect",
        ImageDimension::Buffer => "Buffer",
        ImageDimension::External => "ExternalOES",
        ImageDimension::ExternalY2Y => "External2DY2YEXT",
        ImageDimension::Video => "VideoWEBGL",
        ImageDimension::PixelLocal => {
            base_name = "pixelLocalANGLE";
            ""
        }
        ImageDimension::Subpass => {
            base_name = "subpassInput";
            ""
        }
    };
    let multisample_suffix = if image_type.is_ms { "MS" } else { "" };
    let array_suffix = if image_type.is_array { "Array" } else { "" };
    let shadow_suffix = if image_type.is_shadow { "Shadow" } else { "" };
    format!("{prefix}{base_name}{suffix}{multisample_suffix}{array_suffix}{shadow_suffix}")
}

fn name_str(name: &Name, temp_prefix: &'static str, id: u32) -> String {
    // Some names are expected to be output exactly, and are known to be unique.  Others will
    // be disambiguated with an `_N` suffix if they clash with any other name in text outputs.
    format!(
        "'{}{}{}'",
        match name.source {
            NameSource::ShaderInterface => USER_SYMBOL_PREFIX,
            NameSource::Temporary => temp_prefix,
            _ => "",
        },
        name.name,
        if name.source == NameSource::Temporary { format!("_{id}") } else { "".to_string() }
    )
}

fn block_storage_str(storage: BlockStorage) -> String {
    (match storage {
        BlockStorage::Shared => "shared",
        BlockStorage::Packed => "packed",
        BlockStorage::Std140 => "std140",
        BlockStorage::Std430 => "std430",
    })
    .to_string()
}

fn matrix_packing_str(packing: MatrixPacking) -> String {
    (match packing {
        MatrixPacking::ColumnMajor => "column_major",
        MatrixPacking::RowMajor => "row_major",
    })
    .to_string()
}

fn depth_str(depth: Depth) -> String {
    (match depth {
        Depth::Any => "depth_any",
        Depth::Greater => "depth_greater",
        Depth::Less => "depth_less",
        Depth::Unchanged => "depth_unchanged",
    })
    .to_string()
}

fn image_internal_format_str(format: ImageInternalFormat) -> String {
    (match format {
        ImageInternalFormat::RGBA32F => "rgba32f",
        ImageInternalFormat::RGBA16F => "rgba16f",
        ImageInternalFormat::R32F => "r32f",
        ImageInternalFormat::RGBA32UI => "rgba32ui",
        ImageInternalFormat::RGBA16UI => "rgba16ui",
        ImageInternalFormat::RGBA8UI => "rgba8ui",
        ImageInternalFormat::R32UI => "r32ui",
        ImageInternalFormat::RGBA32I => "rgba32i",
        ImageInternalFormat::RGBA16I => "rgba16i",
        ImageInternalFormat::RGBA8I => "rgba8i",
        ImageInternalFormat::R32I => "r32i",
        ImageInternalFormat::RGBA8 => "rgba8",
        ImageInternalFormat::RGBA8SNORM => "rgba8snorm",
    })
    .to_string()
}

fn decoration_str(decoration: Decoration) -> String {
    match decoration {
        Decoration::Invariant => "invariant".to_string(),
        Decoration::Precise => "precise".to_string(),
        Decoration::Interpolant => "interpolant".to_string(),
        Decoration::Smooth => "smooth".to_string(),
        Decoration::Flat => "flat".to_string(),
        Decoration::NoPerspective => "noperspective".to_string(),
        Decoration::Centroid => "centroid".to_string(),
        Decoration::Sample => "sample".to_string(),
        Decoration::Patch => "patch".to_string(),
        Decoration::Shared => "shared(memory)".to_string(),
        Decoration::ReadOnly => "readonly".to_string(),
        Decoration::WriteOnly => "writeonly".to_string(),
        Decoration::Coherent => "coherent".to_string(),
        Decoration::Restrict => "restrict".to_string(),
        Decoration::Volatile => "volatile".to_string(),
        Decoration::Uniform => "uniform".to_string(),
        Decoration::Buffer => "buffer".to_string(),
        Decoration::PushConstant => "push_constant".to_string(),
        Decoration::NonCoherent => "noncoherent".to_string(),
        Decoration::Yuv => "yuv".to_string(),
        Decoration::Input => "input".to_string(),
        Decoration::Output => "output".to_string(),
        Decoration::InputOutput => "input/output".to_string(),
        Decoration::Location(n) => format!("location={n}"),
        Decoration::Index(n) => format!("index={n}"),
        Decoration::InputAttachmentIndex(n) => format!("input_attachment_index={n}"),
        Decoration::SpecConst(n) => format!("constant_id={n}"),
        Decoration::Block(storage) => block_storage_str(storage),
        Decoration::Binding(n) => format!("binding={n}"),
        Decoration::Offset(n) => format!("offset={n}"),
        Decoration::MatrixPacking(packing) => matrix_packing_str(packing),
        Decoration::Depth(depth) => depth_str(depth),
        Decoration::ImageInternalFormat(format) => image_internal_format_str(format),
        Decoration::NumViews(n) => format!("num_views={n}"),
        Decoration::RasterOrdered => "raster_ordered(D3D)".to_string(),
    }
}

fn decoration_list(precision: Precision, decorations: &Decorations) -> String {
    let mut result = Vec::new();
    match precision {
        Precision::NotApplicable => {}
        _ => result.push(precision_str(precision).to_string()),
    };

    decorations.decorations.iter().for_each(|&decoration| {
        result.push(decoration_str(decoration));
    });

    result.join(", ")
}

fn append_decorations(result: &mut String, precision: Precision, decorations: &Decorations) {
    let decorations = decoration_list(precision, decorations);
    if !decorations.is_empty() {
        *result = format!("{result} [{decorations}]");
    }
}

fn field_str(field: &Field, index: usize) -> String {
    let mut result = format!(
        "{}: {}",
        name_str(&field.name, TEMP_STRUCT_FIELD_PREFIX, index as u32),
        type_id_str(field.type_id)
    );
    append_decorations(&mut result, field.precision, &field.decorations);
    result
}

fn yuv_csc_standard_str(yuv_csc: YuvCscStandard) -> String {
    (match yuv_csc {
        YuvCscStandard::Itu601 => "itu_601",
        YuvCscStandard::Itu601FullRange => "itu_601_full_range",
        YuvCscStandard::Itu709 => "itu_709",
    })
    .to_string()
}

fn built_in_str(built_in: BuiltIn) -> String {
    (match built_in {
        BuiltIn::InstanceID => "InstanceID",
        BuiltIn::VertexID => "VertexID",
        BuiltIn::Position => "Position",
        BuiltIn::PointSize => "PointSize",
        BuiltIn::BaseVertex => "BaseVertex",
        BuiltIn::BaseInstance => "BaseInstance",
        BuiltIn::DrawID => "DrawID",
        BuiltIn::FragCoord => "FragCoord",
        BuiltIn::FrontFacing => "FrontFacing",
        BuiltIn::PointCoord => "PointCoord",
        BuiltIn::HelperInvocation => "HelperInvocation",
        BuiltIn::FragColor => "FragColor",
        BuiltIn::FragData => "FragData",
        BuiltIn::FragDepth => "FragDepth",
        BuiltIn::SecondaryFragColorEXT => "SecondaryFragColorEXT",
        BuiltIn::SecondaryFragDataEXT => "SecondaryFragDataEXT",
        BuiltIn::DepthRange => "DepthRange",
        BuiltIn::ViewIDOVR => "ViewIDOVR",
        BuiltIn::ClipDistance => "ClipDistance",
        BuiltIn::CullDistance => "CullDistance",
        BuiltIn::LastFragColor => "LastFragColor",
        BuiltIn::LastFragData => "LastFragData",
        BuiltIn::LastFragDepthARM => "LastFragDepthARM",
        BuiltIn::LastFragStencilARM => "LastFragStencilARM",
        BuiltIn::ShadingRateEXT => "ShadingRateEXT",
        BuiltIn::PrimitiveShadingRateEXT => "PrimitiveShadingRateEXT",
        BuiltIn::SampleID => "SampleID",
        BuiltIn::SamplePosition => "SamplePosition",
        BuiltIn::SampleMaskIn => "SampleMaskIn",
        BuiltIn::SampleMask => "SampleMask",
        BuiltIn::NumSamples => "NumSamples",
        BuiltIn::NumWorkGroups => "NumWorkGroups",
        BuiltIn::WorkGroupSize => "WorkGroupSize",
        BuiltIn::WorkGroupID => "WorkGroupID",
        BuiltIn::LocalInvocationID => "LocalInvocationID",
        BuiltIn::GlobalInvocationID => "GlobalInvocationID",
        BuiltIn::LocalInvocationIndex => "LocalInvocationIndex",
        BuiltIn::PerVertexIn => "PerVertexIn",
        BuiltIn::PerVertexOut => "PerVertexOut",
        BuiltIn::PrimitiveIDIn => "PrimitiveIDIn",
        BuiltIn::InvocationID => "InvocationID",
        BuiltIn::PrimitiveID => "PrimitiveID",
        BuiltIn::LayerOut => "Layer(GS)",
        BuiltIn::LayerIn => "Layer(FS)",
        BuiltIn::PatchVerticesIn => "PatchVerticesIn",
        BuiltIn::TessLevelOuter => "TessLevelOuter",
        BuiltIn::TessLevelInner => "TessLevelInner",
        BuiltIn::TessCoord => "TessCoord",
        BuiltIn::BoundingBoxOES => "BoundingBoxOES",
        BuiltIn::PixelLocalEXT => "PixelLocalEXT",
    })
    .to_string()
}

fn function_param_direction_str(direction: FunctionParamDirection) -> String {
    (match direction {
        FunctionParamDirection::Input => "in",
        FunctionParamDirection::Output => "out",
        FunctionParamDirection::InputOutput => "inout",
    })
    .to_string()
}

fn function_prototype_str(id: FunctionId, function: &Function) -> String {
    let name = name_str(&function.name, TEMP_FUNCTION_PREFIX, id.id);

    let mut return_type = type_id_str(function.return_type_id);
    append_decorations(&mut return_type, function.return_precision, &function.return_decorations);

    let params = function
        .params
        .iter()
        .map(|param| {
            format!(
                "{} {}",
                function_param_direction_str(param.direction),
                variable_id_str(param.variable_id)
            )
        })
        .collect::<Vec<_>>()
        .join(", ");

    format!("{}: {name}({params}) -> {return_type}", function_id_str(id))
}

fn block_kind_str(kind: traverser::BlockKind) -> String {
    match kind {
        traverser::BlockKind::Entry => "Entry To Function:".to_string(),
        traverser::BlockKind::True => "If True Block:".to_string(),
        traverser::BlockKind::False => "If False Block:".to_string(),
        traverser::BlockKind::LoopCondition => "Loop Condition:".to_string(),
        traverser::BlockKind::LoopBody => "Loop Body:".to_string(),
        traverser::BlockKind::Continue => "Loop Continue Block:".to_string(),
        traverser::BlockKind::Case(case) => case
            .map(|constant_id| format!("Case {}:", constant_id_str(constant_id)))
            .unwrap_or("Default Case:".to_string()),
        traverser::BlockKind::Merge => "Merge Block:".to_string(),
    }
}

fn unary_opcode_str(op: UnaryOpCode) -> &'static str {
    match op {
        UnaryOpCode::ArrayLength => "ArrayLength",
        UnaryOpCode::Negate => "Negate",
        UnaryOpCode::PostfixIncrement => "PostfixIncrement",
        UnaryOpCode::PostfixDecrement => "PostfixDecrement",
        UnaryOpCode::PrefixIncrement => "PrefixIncrement",
        UnaryOpCode::PrefixDecrement => "PrefixDecrement",
        UnaryOpCode::LogicalNot => "LogicalNot",
        UnaryOpCode::BitwiseNot => "BitwiseNot",
        UnaryOpCode::Radians => "Radians",
        UnaryOpCode::Degrees => "Degrees",
        UnaryOpCode::Sin => "Sin",
        UnaryOpCode::Cos => "Cos",
        UnaryOpCode::Tan => "Tan",
        UnaryOpCode::Asin => "Asin",
        UnaryOpCode::Acos => "Acos",
        UnaryOpCode::Atan => "Atan",
        UnaryOpCode::Sinh => "Sinh",
        UnaryOpCode::Cosh => "Cosh",
        UnaryOpCode::Tanh => "Tanh",
        UnaryOpCode::Asinh => "Asinh",
        UnaryOpCode::Acosh => "Acosh",
        UnaryOpCode::Atanh => "Atanh",
        UnaryOpCode::Exp => "Exp",
        UnaryOpCode::Log => "Log",
        UnaryOpCode::Exp2 => "Exp2",
        UnaryOpCode::Log2 => "Log2",
        UnaryOpCode::Sqrt => "Sqrt",
        UnaryOpCode::Inversesqrt => "Inversesqrt",
        UnaryOpCode::Abs => "Abs",
        UnaryOpCode::Sign => "Sign",
        UnaryOpCode::Floor => "Floor",
        UnaryOpCode::Trunc => "Trunc",
        UnaryOpCode::Round => "Round",
        UnaryOpCode::RoundEven => "RoundEven",
        UnaryOpCode::Ceil => "Ceil",
        UnaryOpCode::Fract => "Fract",
        UnaryOpCode::Isnan => "Isnan",
        UnaryOpCode::Isinf => "Isinf",
        UnaryOpCode::FloatBitsToInt => "FloatBitsToInt",
        UnaryOpCode::FloatBitsToUint => "FloatBitsToUint",
        UnaryOpCode::IntBitsToFloat => "IntBitsToFloat",
        UnaryOpCode::UintBitsToFloat => "UintBitsToFloat",
        UnaryOpCode::PackSnorm2x16 => "PackSnorm2x16",
        UnaryOpCode::PackHalf2x16 => "PackHalf2x16",
        UnaryOpCode::UnpackSnorm2x16 => "UnpackSnorm2x16",
        UnaryOpCode::UnpackHalf2x16 => "UnpackHalf2x16",
        UnaryOpCode::PackUnorm2x16 => "PackUnorm2x16",
        UnaryOpCode::UnpackUnorm2x16 => "UnpackUnorm2x16",
        UnaryOpCode::PackUnorm4x8 => "PackUnorm4x8",
        UnaryOpCode::PackSnorm4x8 => "PackSnorm4x8",
        UnaryOpCode::UnpackUnorm4x8 => "UnpackUnorm4x8",
        UnaryOpCode::UnpackSnorm4x8 => "UnpackSnorm4x8",
        UnaryOpCode::Length => "Length",
        UnaryOpCode::Normalize => "Normalize",
        UnaryOpCode::Transpose => "Transpose",
        UnaryOpCode::Determinant => "Determinant",
        UnaryOpCode::Inverse => "Inverse",
        UnaryOpCode::Any => "Any",
        UnaryOpCode::All => "All",
        UnaryOpCode::Not => "Not",
        UnaryOpCode::BitfieldReverse => "BitfieldReverse",
        UnaryOpCode::BitCount => "BitCount",
        UnaryOpCode::FindLSB => "FindLSB",
        UnaryOpCode::FindMSB => "FindMSB",
        UnaryOpCode::DFdx => "DFdx",
        UnaryOpCode::DFdy => "DFdy",
        UnaryOpCode::Fwidth => "Fwidth",
        UnaryOpCode::InterpolateAtCentroid => "InterpolateAtCentroid",
        UnaryOpCode::AtomicCounter => "AtomicCounter",
        UnaryOpCode::AtomicCounterIncrement => "AtomicCounterIncrement",
        UnaryOpCode::AtomicCounterDecrement => "AtomicCounterDecrement",
        UnaryOpCode::ImageSize => "ImageSize",
        UnaryOpCode::PixelLocalLoadANGLE => "PixelLocalLoadANGLE",
    }
}

fn binary_opcode_str(op: BinaryOpCode) -> &'static str {
    match op {
        BinaryOpCode::Add => "Add",
        BinaryOpCode::Sub => "Sub",
        BinaryOpCode::Mul => "Mul",
        BinaryOpCode::VectorTimesScalar => "VectorTimesScalar",
        BinaryOpCode::MatrixTimesScalar => "MatrixTimesScalar",
        BinaryOpCode::VectorTimesMatrix => "VectorTimesMatrix",
        BinaryOpCode::MatrixTimesVector => "MatrixTimesVector",
        BinaryOpCode::MatrixTimesMatrix => "MatrixTimesMatrix",
        BinaryOpCode::Div => "Div",
        BinaryOpCode::IMod => "IMod",
        BinaryOpCode::LogicalXor => "LogicalXor",
        BinaryOpCode::Equal => "Equal",
        BinaryOpCode::NotEqual => "NotEqual",
        BinaryOpCode::LessThan => "LessThan",
        BinaryOpCode::GreaterThan => "GreaterThan",
        BinaryOpCode::LessThanEqual => "LessThanEqual",
        BinaryOpCode::GreaterThanEqual => "GreaterThanEqual",
        BinaryOpCode::BitShiftLeft => "BitShiftLeft",
        BinaryOpCode::BitShiftRight => "BitShiftRight",
        BinaryOpCode::BitwiseOr => "BitwiseOr",
        BinaryOpCode::BitwiseXor => "BitwiseXor",
        BinaryOpCode::BitwiseAnd => "BitwiseAnd",
        BinaryOpCode::Atan => "Atan[binary]",
        BinaryOpCode::Pow => "Pow",
        BinaryOpCode::Mod => "Mod",
        BinaryOpCode::Min => "Min",
        BinaryOpCode::Max => "Max",
        BinaryOpCode::Step => "Step",
        BinaryOpCode::Modf => "Modf",
        BinaryOpCode::Frexp => "Frexp",
        BinaryOpCode::Ldexp => "Ldexp",
        BinaryOpCode::Distance => "Distance",
        BinaryOpCode::Dot => "Dot",
        BinaryOpCode::Cross => "Cross",
        BinaryOpCode::Reflect => "Reflect",
        BinaryOpCode::MatrixCompMult => "MatrixCompMult",
        BinaryOpCode::OuterProduct => "OuterProduct",
        BinaryOpCode::LessThanVec => "LessThan[built-in]",
        BinaryOpCode::LessThanEqualVec => "LessThanEqual[built-in]",
        BinaryOpCode::GreaterThanVec => "GreaterThan[built-in]",
        BinaryOpCode::GreaterThanEqualVec => "GreaterThanEqual[built-in]",
        BinaryOpCode::EqualVec => "Equal[built-in]",
        BinaryOpCode::NotEqualVec => "NotEqual[built-in]",
        BinaryOpCode::InterpolateAtSample => "InterpolateAtSample",
        BinaryOpCode::InterpolateAtOffset => "InterpolateAtOffset",
        BinaryOpCode::AtomicAdd => "AtomicAdd",
        BinaryOpCode::AtomicMin => "AtomicMin",
        BinaryOpCode::AtomicMax => "AtomicMax",
        BinaryOpCode::AtomicAnd => "AtomicAnd",
        BinaryOpCode::AtomicOr => "AtomicOr",
        BinaryOpCode::AtomicXor => "AtomicXor",
        BinaryOpCode::AtomicExchange => "AtomicExchange",
    }
}

fn built_in_opcode_str(op: BuiltInOpCode) -> &'static str {
    match op {
        BuiltInOpCode::Clamp => "Clamp",
        BuiltInOpCode::Mix => "Mix",
        BuiltInOpCode::Smoothstep => "Smoothstep",
        BuiltInOpCode::Fma => "Fma",
        BuiltInOpCode::Faceforward => "Faceforward",
        BuiltInOpCode::Refract => "Refract",
        BuiltInOpCode::BitfieldExtract => "BitfieldExtract",
        BuiltInOpCode::BitfieldInsert => "BitfieldInsert",
        BuiltInOpCode::UaddCarry => "UaddCarry",
        BuiltInOpCode::UsubBorrow => "UsubBorrow",
        BuiltInOpCode::UmulExtended => "UmulExtended",
        BuiltInOpCode::ImulExtended => "ImulExtended",
        BuiltInOpCode::TextureSize => "TextureSize",
        BuiltInOpCode::TextureQueryLod => "TextureQueryLod",
        BuiltInOpCode::TexelFetch => "TexelFetch",
        BuiltInOpCode::TexelFetchOffset => "TexelFetchOffset",
        BuiltInOpCode::Rgb2Yuv => "Rgb2Yuv",
        BuiltInOpCode::Yuv2Rgb => "Yuv2Rgb",
        BuiltInOpCode::AtomicCompSwap => "AtomicCompSwap",
        BuiltInOpCode::ImageStore => "ImageStore",
        BuiltInOpCode::ImageLoad => "ImageLoad",
        BuiltInOpCode::ImageAtomicAdd => "ImageAtomicAdd",
        BuiltInOpCode::ImageAtomicMin => "ImageAtomicMin",
        BuiltInOpCode::ImageAtomicMax => "ImageAtomicMax",
        BuiltInOpCode::ImageAtomicAnd => "ImageAtomicAnd",
        BuiltInOpCode::ImageAtomicOr => "ImageAtomicOr",
        BuiltInOpCode::ImageAtomicXor => "ImageAtomicXor",
        BuiltInOpCode::ImageAtomicExchange => "ImageAtomicExchange",
        BuiltInOpCode::ImageAtomicCompSwap => "ImageAtomicCompSwap",
        BuiltInOpCode::PixelLocalStoreANGLE => "PixelLocalStoreANGLE",
        BuiltInOpCode::MemoryBarrier => "MemoryBarrier",
        BuiltInOpCode::MemoryBarrierAtomicCounter => "MemoryBarrierAtomicCounter",
        BuiltInOpCode::MemoryBarrierBuffer => "MemoryBarrierBuffer",
        BuiltInOpCode::MemoryBarrierImage => "MemoryBarrierImage",
        BuiltInOpCode::Barrier => "Barrier",
        BuiltInOpCode::MemoryBarrierShared => "MemoryBarrierShared",
        BuiltInOpCode::GroupMemoryBarrier => "GroupMemoryBarrier",
        BuiltInOpCode::EmitVertex => "EmitVertex",
        BuiltInOpCode::EndPrimitive => "EndPrimitive",
        BuiltInOpCode::SubpassLoad => "SubpassLoad",
        BuiltInOpCode::BeginInvocationInterlockNV => "BeginInvocationInterlockNV",
        BuiltInOpCode::EndInvocationInterlockNV => "EndInvocationInterlockNV",
        BuiltInOpCode::BeginFragmentShaderOrderingINTEL => "BeginFragmentShaderOrderingINTEL",
        BuiltInOpCode::BeginInvocationInterlockARB => "BeginInvocationInterlockARB",
        BuiltInOpCode::EndInvocationInterlockARB => "EndInvocationInterlockARB",
        BuiltInOpCode::NumSamples => "NumSamples",
        BuiltInOpCode::SamplePosition => "SamplePosition",
        BuiltInOpCode::InterpolateAtCenter => "InterpolateAtCenter",
        BuiltInOpCode::Saturate => "Saturate",
        BuiltInOpCode::LoopForwardProgress => "LoopForwardProgress",
    }
}

fn texture_opcode_str(op: &TextureOpCode) -> (&'static str, String) {
    match *op {
        TextureOpCode::Implicit { is_proj, offset } => (
            "",
            format!(
                "is_proj:{}{}",
                is_proj,
                offset.map(|id| format!(" offset:{}", id_str(id))).unwrap_or("".to_string())
            ),
        ),
        TextureOpCode::Compare { compare } => ("Compare", format!("compare:{}", id_str(compare))),
        TextureOpCode::Lod { is_proj, lod, offset } => (
            "Lod",
            format!(
                "is_proj:{} lod:{}{}",
                is_proj,
                id_str(lod),
                offset.map(|id| format!(" offset:{}", id_str(id))).unwrap_or("".to_string())
            ),
        ),
        TextureOpCode::CompareLod { compare, lod } => {
            ("CompareLod", format!("compare:{} lod:{}", id_str(compare), id_str(lod)))
        }
        TextureOpCode::Bias { is_proj, bias, offset } => (
            "Bias",
            format!(
                "is_proj:{} bias:{}{}",
                is_proj,
                id_str(bias),
                offset.map(|id| format!(" offset:{}", id_str(id))).unwrap_or("".to_string())
            ),
        ),
        TextureOpCode::CompareBias { compare, bias } => {
            ("CompareBias", format!("compare:{} bias:{}", id_str(compare), id_str(bias)))
        }
        TextureOpCode::Grad { is_proj, dx, dy, offset } => (
            "Grad",
            format!(
                "is_proj:{} dx:{} dy:{}{}",
                is_proj,
                id_str(dx),
                id_str(dy),
                offset.map(|id| format!(" offset:{}", id_str(id))).unwrap_or("".to_string())
            ),
        ),
        TextureOpCode::Gather { offset } => {
            ("Gather", offset.map(|id| format!(" offset:{}", id_str(id))).unwrap_or("".to_string()))
        }
        TextureOpCode::GatherComponent { component, offset } => (
            "GatherComponent",
            format!(
                "component:{}{}",
                id_str(component),
                offset.map(|id| format!(" offset:{}", id_str(id))).unwrap_or("".to_string())
            ),
        ),
        TextureOpCode::GatherRef { refz, offset } => (
            "GatherRef",
            format!(
                "refz:{}{}",
                id_str(refz),
                offset.map(|id| format!(" offset:{}", id_str(id))).unwrap_or("".to_string())
            ),
        ),
    }
}

fn opcode_str(op: &OpCode) -> String {
    match *op {
        OpCode::MergeInput => {
            panic!("Internal error: No block instruction should use MergeInput")
        }
        OpCode::Call(id, ref params) => {
            format!("Call {} With ({})", function_id_str(id), id_list_str(params))
        }
        OpCode::Discard => "Discard".to_string(),
        OpCode::Return(id) => {
            format!("Return{}", id.map(|id| format!(" {}", id_str(id))).unwrap_or("".to_string()))
        }
        OpCode::Break => "Break".to_string(),
        OpCode::Continue => "Continue".to_string(),
        OpCode::Passthrough => "Passthrough".to_string(),
        OpCode::NextBlock => "NextBlock".to_string(),
        OpCode::Merge(id) => {
            format!("Merge{}", id.map(|id| format!(" {}", id_str(id))).unwrap_or("".to_string()))
        }
        OpCode::If(id) => format!("If {}", id_str(id)),
        OpCode::Loop => "Loop".to_string(),
        OpCode::DoLoop => "DoLoop".to_string(),
        OpCode::LoopIf(id) => format!("LoopIf {}", id_str(id)),
        OpCode::Switch(id, _) => format!("Switch {}", id_str(id)),
        OpCode::ExtractVectorComponent(id, index) => {
            format!("ExtractVectorComponent {} {index}", id_str(id))
        }
        OpCode::ExtractVectorComponentMulti(id, ref indices) => {
            format!("ExtractVectorComponentMulti {} ({})", id_str(id), index_list_str(indices))
        }
        OpCode::ExtractVectorComponentDynamic(id, index) => {
            format!("ExtractVectorComponentDynamic {} {}", id_str(id), id_str(index))
        }
        OpCode::ExtractMatrixColumn(id, index) => {
            format!("ExtractMatrixColumn {} {}", id_str(id), id_str(index))
        }
        OpCode::ExtractStructField(id, index) => {
            format!("ExtractStructField {} {index}", id_str(id))
        }
        OpCode::ExtractArrayElement(id, index) => {
            format!("ExtractArrayElement {} {}", id_str(id), id_str(index))
        }
        OpCode::ConstructScalarFromScalar(id) => {
            format!("ConstructScalarFromScalar {}", id_str(id))
        }
        OpCode::ConstructVectorFromScalar(id) => {
            format!("ConstructVectorFromScalar {}", id_str(id))
        }
        OpCode::ConstructMatrixFromScalar(id) => {
            format!("ConstructMatrixFromScalar {}", id_str(id))
        }
        OpCode::ConstructMatrixFromMatrix(id) => {
            format!("ConstructMatrixFromMatrix {}", id_str(id))
        }
        OpCode::ConstructVectorFromMultiple(ref ids) => {
            format!("ConstructVectorFromMultiple ({})", id_list_str(ids))
        }
        OpCode::ConstructMatrixFromMultiple(ref ids) => {
            format!("ConstructMatrixFromMultiple ({})", id_list_str(ids))
        }
        OpCode::ConstructStruct(ref ids) => {
            format!("ConstructStruct ({})", id_list_str(ids))
        }
        OpCode::ConstructArray(ref ids) => {
            format!("ConstructArray ({})", id_list_str(ids))
        }
        OpCode::AccessVectorComponent(id, index) => {
            format!("AccessVectorComponent {} {index}", id_str(id))
        }
        OpCode::AccessVectorComponentMulti(id, ref indices) => {
            format!("AccessVectorComponentMulti {} ({})", id_str(id), index_list_str(indices))
        }
        OpCode::AccessVectorComponentDynamic(id, index) => {
            format!("AccessVectorComponentDynamic {} {}", id_str(id), id_str(index))
        }
        OpCode::AccessMatrixColumn(id, index) => {
            format!("AccessMatrixColumn {} {}", id_str(id), id_str(index))
        }
        OpCode::AccessStructField(id, index) => {
            format!("AccessStructField {} {index}", id_str(id))
        }
        OpCode::AccessArrayElement(id, index) => {
            format!("AccessArrayElement {} {}", id_str(id), id_str(index))
        }
        OpCode::Load(id) => format!("Load {}", id_str(id)),
        OpCode::Store(target, value) => {
            format!("Store {} {}", id_str(target), id_str(value))
        }
        OpCode::Alias(id) => format!("Alias {}", id_str(id)),
        OpCode::Unary(unary_op, id) => {
            format!("{} {}", unary_opcode_str(unary_op), id_str(id))
        }
        OpCode::Binary(binary_op, lhs, rhs) => {
            format!("{} {} {}", binary_opcode_str(binary_op), id_str(lhs), id_str(rhs))
        }
        OpCode::BuiltIn(built_in_op, ref ids) => {
            format!("{} ({})", built_in_opcode_str(built_in_op), id_list_str(ids))
        }
        OpCode::Texture(ref texture_op, sampler, coord) => {
            let (variant, params) = texture_opcode_str(texture_op);
            format!(
                "Texture{} sampler:{} coord:{} {}",
                variant,
                id_str(sampler),
                id_str(coord),
                params
            )
        }
    }
}

// Helper to append to the string on a new line, unless the appended string is empty.
fn append_on_new_line(result: &mut String, new: String, indent: usize) {
    if new.is_empty() {
        return;
    }

    result.push('\n');

    // The first two levels of indent are "  ", before the function blocks are output.
    result.push_str(&"  ".repeat(std::cmp::min(indent, 2)));

    // To annotate control flow, "|   " is added per block indent.
    if indent > 2 {
        result.push_str(&"|   ".repeat(indent - 2));
    }

    result.push_str(&new);
}

fn dump_shader_properties(ir_meta: &IRMeta, result: &mut String) {
    match ir_meta.get_shader_type() {
        ShaderType::Fragment => {
            append_on_new_line(
                result,
                early_fragment_tests_str(ir_meta.get_early_fragment_tests()),
                0,
            );
            append_on_new_line(
                result,
                blend_equation_advanced_str(ir_meta.get_advanced_blend_equations()),
                0,
            );
        }
        ShaderType::TessellationControl => {
            append_on_new_line(result, tcs_vertices_str(ir_meta.get_tcs_vertices()), 0);
        }
        ShaderType::TessellationEvaluation => {
            append_on_new_line(result, tes_primitive_str(ir_meta.get_tes_primitive()), 0);
            append_on_new_line(result, tes_vertex_spacing_str(ir_meta.get_tes_vertex_spacing()), 0);
            append_on_new_line(result, tes_ordering_str(ir_meta.get_tes_ordering()), 0);
            append_on_new_line(result, tes_point_mode_str(ir_meta.get_tes_point_mode()), 0);
        }
        ShaderType::Geometry => {
            append_on_new_line(result, gs_primitive_str(ir_meta.get_gs_primitive_in(), "In"), 0);
            append_on_new_line(result, gs_primitive_str(ir_meta.get_gs_primitive_out(), "Out"), 0);
            append_on_new_line(result, gs_invocations_str(ir_meta.get_gs_invocations()), 0);
            append_on_new_line(result, gs_max_vertices_str(ir_meta.get_gs_max_vertices()), 0);
        }
        _ => {}
    };
}

fn dump_types(ir_meta: &IRMeta, result: &mut String) {
    result.push_str("\n\nTypes:");
    ir_meta.all_types().iter().enumerate().for_each(|(id, t)| {
        let formatted = format!(
            "t{id}: {}",
            match t {
                &Type::Scalar(basic_type) => basic_type_str(basic_type),
                &Type::Vector(type_id, count) =>
                    format!("Vector of {}[{count}]", type_id_str(type_id)),
                &Type::Matrix(type_id, count) =>
                    format!("Matrix of {}[{count}]", type_id_str(type_id)),
                &Type::Array(type_id, count) =>
                    format!("Array of {}[{count}]", type_id_str(type_id)),
                &Type::UnsizedArray(type_id) =>
                    format!("Unsized Array of {}", type_id_str(type_id)),
                &Type::Image(basic_type, image_type) => image_type_str(basic_type, image_type),
                Type::Struct(name, _, specialization) => format!(
                    "{} {}:",
                    match specialization {
                        StructSpecialization::Struct => "Struct",
                        StructSpecialization::InterfaceBlock => "Interface Block",
                    },
                    name_str(name, TEMP_STRUCT_PREFIX, id as u32)
                ),
                &Type::Pointer(type_id) => format!("Pointer to {}", type_id_str(type_id)),
                Type::DeadCodeEliminated => {
                    return;
                }
            }
        );
        append_on_new_line(result, formatted, 1);
        if let Type::Struct(_, fields, _) = t {
            fields
                .iter()
                .enumerate()
                .for_each(|(index, field)| append_on_new_line(result, field_str(field, index), 2));
        }
    });
}

fn dump_constants(ir_meta: &IRMeta, result: &mut String) {
    result.push_str("\n\nConstants:");
    ir_meta.all_constants().iter().enumerate().for_each(|(id, c)| {
        if c.is_dead_code_eliminated {
            return;
        }
        let formatted = format!(
            "c{id} ({}): {}",
            type_id_str(c.type_id),
            match &c.value {
                &ConstantValue::Float(f) => format!("{f:?}"),
                &ConstantValue::Int(i) => i.to_string(),
                &ConstantValue::Uint(u) => u.to_string(),
                &ConstantValue::Bool(b) => b.to_string(),
                &ConstantValue::YuvCsc(yuv_csc) => yuv_csc_standard_str(yuv_csc),
                ConstantValue::Composite(elements) => format!(
                    "composite({})",
                    elements
                        .iter()
                        .map(|&element| constant_id_str(element))
                        .collect::<Vec<_>>()
                        .join(", ")
                ),
            }
        );
        append_on_new_line(result, formatted, 1);
    });
}

fn dump_variables(ir_meta: &IRMeta, result: &mut String) {
    result.push_str("\n\nVariables:");
    ir_meta.all_variables().iter().enumerate().for_each(|(id, v)| {
        if v.is_dead_code_eliminated {
            return;
        }
        let name = name_str(&v.name, TEMP_VARIABLE_PREFIX, id as u32);
        let initializer = v
            .initializer
            .map(|constant_id| format!("={}", constant_id_str(constant_id)))
            .or(ir_meta
                .variable_needs_zero_initialization(VariableId { id: id as u32 })
                .then_some("=TO_BE_ZERO_INIT".to_string()))
            .unwrap_or("".to_string());
        let built_in = v
            .built_in
            .map(|built_in| format!(" <{}>", built_in_str(built_in)))
            .unwrap_or("".to_string());

        let mut formatted =
            format!("v{id} ({}): {}{}{}", type_id_str(v.type_id), name, initializer, built_in);
        append_decorations(&mut formatted, v.precision, &v.decorations);

        append_on_new_line(result, formatted, 1);
    });

    write!(
        result,
        "\n\nGlobals: {}",
        ir_meta
            .all_global_variables()
            .iter()
            .map(|&id| variable_id_str(id))
            .collect::<Vec<_>>()
            .join(", ")
    )
    .unwrap();
}

fn dump_instruction(
    result: &mut String,
    ir_meta: &IRMeta,
    instruction: &BlockInstruction,
    indent: usize,
) {
    let (result_id, op, decorations) = match instruction {
        &BlockInstruction::Register(id) => {
            let instruction = ir_meta.get_instruction(id);
            debug_assert!(id.id == instruction.result.id.id);
            let type_id = format!("({})", type_id_str(instruction.result.type_id));
            (
                format!("{} {:>6} = ", register_id_str(id), type_id),
                opcode_str(&instruction.op),
                decoration_list(instruction.result.precision, &Decorations::new_none()),
            )
        }
        BlockInstruction::Void(op) => ("".to_string(), opcode_str(op), "".to_string()),
    };

    let mut formatted = format!("{result_id:15}{op}");

    if !decorations.is_empty() {
        formatted = format!("{formatted:60} [{decorations}]");
    }

    append_on_new_line(result, formatted, indent);
}

fn dump_block(
    result: &mut String,
    ir_meta: &IRMeta,
    kind: traverser::BlockKind,
    block: &Block,
    indent: usize,
) {
    // Start every block with a new line to separate them more easily.
    let block_kind = block_kind_str(kind);
    let block_header = "_".repeat(block_kind.len());
    let is_merge_block = matches!(kind, traverser::BlockKind::Merge);
    if is_merge_block {
        append_on_new_line(result, "|".to_string(), indent);
        append_on_new_line(
            result,
            "V".to_string() + &block_header[..block_header.len() - 1],
            indent,
        );
    } else if indent > 2 {
        append_on_new_line(result, "|".to_string(), indent - 1);
        append_on_new_line(result, "+-> ".to_string() + &block_header, indent - 1);
    } else {
        result.push('\n');
        append_on_new_line(result, block_header, indent);
    }
    append_on_new_line(result, block_kind, indent);

    block.input.inspect(|input| {
        debug_assert!(matches!(ir_meta.get_instruction(input.id).op, OpCode::MergeInput));
        debug_assert!(ir_meta.get_instruction(input.id).result.type_id == input.type_id);
        debug_assert!(ir_meta.get_instruction(input.id).result.precision == input.precision);

        let mut formatted =
            format!("Input: {} ({})", register_id_str(input.id), type_id_str(input.type_id));
        append_decorations(&mut formatted, input.precision, &Decorations::new_none());
        append_on_new_line(result, formatted, indent);
    });

    if !block.variables.is_empty() {
        let declarations = format!(
            "Declare: {}",
            block.variables.iter().map(|&id| variable_id_str(id)).collect::<Vec<_>>().join(", ")
        );
        append_on_new_line(result, declarations, indent);
    }

    for instruction in &block.instructions {
        dump_instruction(result, ir_meta, instruction, indent);
    }
}

fn dump_functions(ir_meta: &IRMeta, function_entries: &[Option<Block>], result: &mut String) {
    result.push_str("\n\nFunctions:");

    traverser::visitor::for_each_function(
        result,
        function_entries,
        |result, id| {
            result.push('\n');
            append_on_new_line(result, function_prototype_str(id, ir_meta.get_function(id)), 1);
        },
        |result, block, kind, indent| {
            dump_block(result, ir_meta, kind, block, indent + 2);
            traverser::visitor::VISIT_SUB_BLOCKS
        },
        |_, _| {},
    );
}

// Dump the IR for debug purposes.
pub fn dump(ir: &IR) {
    // TODO: verify that the following removes the binary for dump_types etc, otherwise they
    // all need to have this cfg.
    //#[cfg(debug_assertions)]
    {
        let mut result = shader_type_str(ir.meta.get_shader_type());
        dump_shader_properties(&ir.meta, &mut result);
        dump_types(&ir.meta, &mut result);
        dump_constants(&ir.meta, &mut result);
        dump_variables(&ir.meta, &mut result);
        dump_functions(&ir.meta, &ir.function_entries, &mut result);

        println!("{result}");
    }
}
