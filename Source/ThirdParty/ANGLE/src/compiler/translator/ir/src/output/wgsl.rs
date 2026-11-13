// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use super::utils::{self, BinaryOpInfo, Precedence};
use crate::ir::*;
use crate::*;
use std::collections::HashMap;
use std::fmt::Write;

#[cfg_attr(debug_assertions, derive(Debug))]
/// Formatted WGSL literal or composite constructor for an IR `ConstantId`.
struct WgslConstant {
    /// WGSL expression text for the constant (e.g. `"1.0f"`, `"42i"`, `"vec2<f32>(1.0f, 2.0f)"`).
    text: String,
    precedence: Precedence,
}

#[cfg_attr(debug_assertions, derive(Debug))]
/// Metadata and declaration text for an IR `VariableId`.
struct WgslVariable {
    /// Full WGSL declaration statement (e.g. `"var _utmp : f32 = 0.0f;"` or `"var<private> _uv :
    /// vec4<f32>;"`).
    declaration_text: String,
    /// WGSL identifier used to reference the variable in expressions (e.g. `"_ux"`,
    /// `"gl_Position"`).
    name: String,
    /// IR `TypeId` of the variable (typically a `Type::Pointer` to the value type).
    type_id: TypeId,
    /// True if the variable is emitted at module scope (`global_variables`) rather than inside a
    /// function body.
    skip_declaration: bool,
}

#[cfg_attr(debug_assertions, derive(Debug))]
/// Signature metadata for an IR `FunctionId`.
struct WgslFunction {
    /// WGSL function header without body braces (e.g. `"fn _uf(p0: f32) -> vec2<f32>"`).
    declaration_text: String,
    /// WGSL function identifier (e.g. `"main"`, `"tfmyFunc_1"`).
    name: String,
}

#[derive(Clone)]
#[cfg_attr(debug_assertions, derive(Debug))]
/// Tracks the formatted WGSL expression for an SSA `RegisterId` or operand.
struct ExprInfo {
    /// Formatted WGSL expression string (e.g. `"(_ua + _ub)"`, `"r1.xy"`).
    text: String,
    precedence: Precedence,
}

#[derive(Clone)]
#[cfg_attr(debug_assertions, derive(Debug))]
/// Represents a shader stage input or output entry in `ANGLE_Input_Annotated` /
/// `ANGLE_Output_Annotated`.
struct PipelineVar {
    /// WGSL identifier of the backing private variable and struct field (e.g. `"gl_Position"`,
    /// `"_ucolor"`).
    name: String,
    /// WGSL type string in the pipeline I/O struct (e.g. `"vec4<f32>"`, `"u32"`).
    type_str: String,
    /// WGSL attribute annotation on the struct member (e.g. `"@builtin(position)"`,
    /// `"@location(0)"`).
    annotation: String,
    /// Built-in variable kind, if this pipeline I/O entry corresponds to a GLSL built-in.
    built_in: Option<BuiltIn>,
}

/// AST-target code generator that lowers ANGLE IR into WGSL source text.
pub struct WgslOutputGenerator {
    types: HashMap<TypeId, String>,
    constants: HashMap<ConstantId, WgslConstant>,
    variables: HashMap<VariableId, WgslVariable>,
    functions: HashMap<FunctionId, WgslFunction>,

    type_declarations: String,
    global_variables: String,
    function_declarations: String,

    expressions: HashMap<RegisterId, ExprInfo>,

    pipeline_inputs: Vec<PipelineVar>,
    pipeline_outputs: Vec<PipelineVar>,
    shader_type: ShaderType,
}

pub type Generator = WgslOutputGenerator;

impl WgslOutputGenerator {
    pub fn new() -> WgslOutputGenerator {
        WgslOutputGenerator {
            types: HashMap::new(),
            constants: HashMap::new(),
            variables: HashMap::new(),
            functions: HashMap::new(),
            type_declarations: String::new(),
            global_variables: String::new(),
            function_declarations: String::new(),
            expressions: HashMap::new(),
            pipeline_inputs: Vec::new(),
            pipeline_outputs: Vec::new(),
            shader_type: ShaderType::Vertex,
        }
    }

    fn basic_type_str(id: TypeId) -> String {
        match id {
            TYPE_ID_VOID => "void",
            TYPE_ID_FLOAT => "f32",
            TYPE_ID_INT => "i32",
            TYPE_ID_UINT => "u32",
            TYPE_ID_BOOL => "bool",
            TYPE_ID_ATOMIC_COUNTER => "atomic<u32>",
            TYPE_ID_YUV_CSC_STANDARD => "u32",
            TYPE_ID_VEC2 => "vec2<f32>",
            TYPE_ID_VEC3 => "vec3<f32>",
            TYPE_ID_VEC4 => "vec4<f32>",
            TYPE_ID_IVEC2 => "vec2<i32>",
            TYPE_ID_IVEC3 => "vec3<i32>",
            TYPE_ID_IVEC4 => "vec4<i32>",
            TYPE_ID_UVEC2 => "vec2<u32>",
            TYPE_ID_UVEC3 => "vec3<u32>",
            TYPE_ID_UVEC4 => "vec4<u32>",
            TYPE_ID_BVEC2 => "vec2<bool>",
            TYPE_ID_BVEC3 => "vec3<bool>",
            TYPE_ID_BVEC4 => "vec4<bool>",
            TYPE_ID_MAT2 => "mat2x2<f32>",
            TYPE_ID_MAT2X3 => "mat2x3<f32>",
            TYPE_ID_MAT2X4 => "mat2x4<f32>",
            TYPE_ID_MAT3X2 => "mat3x2<f32>",
            TYPE_ID_MAT3 => "mat3x3<f32>",
            TYPE_ID_MAT3X4 => "mat3x4<f32>",
            TYPE_ID_MAT4X2 => "mat4x2<f32>",
            TYPE_ID_MAT4X3 => "mat4x3<f32>",
            TYPE_ID_MAT4 => "mat4x4<f32>",
            _ => unreachable!(),
        }
        .to_string()
    }

    fn image_type_str(basic_type: ImageBasicType, image_type: ImageType) -> String {
        assert!(image_type.is_sampled);
        let component = match basic_type {
            ImageBasicType::Float => "f32",
            ImageBasicType::Int => "i32",
            ImageBasicType::Uint => "u32",
        };
        if image_type.is_shadow {
            match image_type.dimension {
                ImageDimension::D2 => {
                    if image_type.is_array {
                        "texture_depth_2d_array".to_string()
                    } else {
                        "texture_depth_2d".to_string()
                    }
                }
                ImageDimension::Cube => {
                    if image_type.is_array {
                        "texture_depth_cube_array".to_string()
                    } else {
                        "texture_depth_cube".to_string()
                    }
                }
                _ => unreachable!(),
            }
        } else {
            let base = match image_type.dimension {
                ImageDimension::D2 => {
                    if image_type.is_ms {
                        "texture_multisampled_2d"
                    } else if image_type.is_array {
                        "texture_2d_array"
                    } else {
                        "texture_2d"
                    }
                }
                ImageDimension::D3 => "texture_3d",
                ImageDimension::Cube => {
                    if image_type.is_array {
                        "texture_cube_array"
                    } else {
                        "texture_cube"
                    }
                }
                ImageDimension::Buffer => "texture_storage_1d",
                _ => "texture_2d",
            };
            format!("{base}<{component}>")
        }
    }

    fn name_str(name: &Name, temp_prefix: &'static str, id: u32) -> String {
        format!(
            "{}{}{}",
            match name.source {
                NameSource::ShaderInterface if !name.name.is_empty() => USER_VARIABLE_PREFIX,
                NameSource::Temporary => temp_prefix,
                _ => "",
            },
            name.name,
            if name.source == NameSource::Temporary { format!("_{id}") } else { "".to_string() }
        )
    }

    fn builtin_annotation_str(built_in: BuiltIn) -> String {
        match built_in {
            BuiltIn::Position | BuiltIn::FragCoord => "@builtin(position)",
            BuiltIn::VertexIndex | BuiltIn::VertexID => "@builtin(vertex_index)",
            BuiltIn::InstanceIndex | BuiltIn::InstanceID => "@builtin(instance_index)",
            BuiltIn::FrontFacing => "@builtin(front_facing)",
            BuiltIn::FragDepth => "@builtin(frag_depth)",
            BuiltIn::GlobalInvocationID => "@builtin(global_invocation_id)",
            BuiltIn::LocalInvocationID => "@builtin(local_invocation_id)",
            BuiltIn::WorkGroupID => "@builtin(workgroup_id)",
            BuiltIn::LocalInvocationIndex => "@builtin(local_invocation_index)",
            BuiltIn::NumWorkGroups => "@builtin(num_workgroups)",
            BuiltIn::SampleID => "@builtin(sample_index)",
            BuiltIn::SampleMask => "@builtin(sample_mask)",
            _ => unreachable!(),
        }
        .to_string()
    }

    /// Resolves an IR operand (`TypedId`) to its formatted WGSL expression.
    fn get_expr(&self, id: TypedId) -> ExprInfo {
        match &id.id {
            Id::Register(id) => self.expressions[id].clone(),
            Id::Constant(id) => {
                let constant = &self.constants[id];
                ExprInfo { text: constant.text.clone(), precedence: constant.precedence }
            }
            Id::Variable(id) => {
                let var = &self.variables[id];
                ExprInfo { text: var.name.clone(), precedence: Precedence::Primary }
            }
        }
    }

    fn get_expression(&self, id: TypedId) -> String {
        self.get_expr(id).text
    }

    fn get_expressions(&self, ids: &[TypedId]) -> String {
        ids.iter().map(|&id| self.get_expression(id)).collect::<Vec<_>>().join(", ")
    }

    fn indent_block(block: String, times: usize) -> String {
        if block.is_empty() {
            return "".to_string();
        }
        let indent = "    ".repeat(times);
        block
            .lines()
            .map(|line| {
                if line.is_empty() {
                    "".to_string()
                } else {
                    format!(
                        "{indent}{line}
"
                    )
                }
            })
            .collect()
    }

    fn unary_builtin_str(op: UnaryOpCode) -> Option<&'static str> {
        match op {
            UnaryOpCode::Radians => Some("radians"),
            UnaryOpCode::Degrees => Some("degrees"),
            UnaryOpCode::Sin => Some("sin"),
            UnaryOpCode::Cos => Some("cos"),
            UnaryOpCode::Tan => Some("tan"),
            UnaryOpCode::Asin => Some("asin"),
            UnaryOpCode::Acos => Some("acos"),
            UnaryOpCode::Atan => Some("atan"),
            UnaryOpCode::Sinh => Some("sinh"),
            UnaryOpCode::Cosh => Some("cosh"),
            UnaryOpCode::Tanh => Some("tanh"),
            UnaryOpCode::Asinh => Some("asinh"),
            UnaryOpCode::Acosh => Some("acosh"),
            UnaryOpCode::Atanh => Some("atanh"),
            UnaryOpCode::Exp => Some("exp"),
            UnaryOpCode::Log => Some("log"),
            UnaryOpCode::Exp2 => Some("exp2"),
            UnaryOpCode::Log2 => Some("log2"),
            UnaryOpCode::Sqrt => Some("sqrt"),
            UnaryOpCode::Inversesqrt => Some("inverseSqrt"),
            UnaryOpCode::Abs => Some("abs"),
            UnaryOpCode::Sign => Some("sign"),
            UnaryOpCode::Floor => Some("floor"),
            UnaryOpCode::Trunc => Some("trunc"),
            UnaryOpCode::Round | UnaryOpCode::RoundEven => Some("round"),
            UnaryOpCode::Ceil => Some("ceil"),
            UnaryOpCode::Fract => Some("fract"),
            UnaryOpCode::Length => Some("length"),
            UnaryOpCode::Normalize => Some("normalize"),
            UnaryOpCode::Transpose => Some("transpose"),
            UnaryOpCode::Determinant => Some("determinant"),
            UnaryOpCode::Any => Some("any"),
            UnaryOpCode::All => Some("all"),
            UnaryOpCode::DFdx => Some("dpdx"),
            UnaryOpCode::DFdy => Some("dpdy"),
            UnaryOpCode::Fwidth => Some("fwidth"),
            UnaryOpCode::UnpackSnorm4x8 => Some("unpack4x8snorm"),
            UnaryOpCode::UnpackUnorm4x8 => Some("unpack4x8unorm"),
            _ => None,
        }
    }
}

impl ast::Target for WgslOutputGenerator {
    type BlockResult = String;

    fn begin(&mut self) {}

    fn end(&mut self) -> String {
        let mut output = String::new();
        // GLSL allows derivatives to be calculated as long as control flow is dynamically uniform.
        // WGSL triggers a derivative_uniformity diagnostic whenever it cannot statically
        // determine that control flow is uniform, which is by default an error. Since this
        // compiler must implement GLSL semantics, use a global diagnostic filter to turn
        // derivative_uniformity diagnostics into warnings instead of the default error.
        // See https://github.com/gpuweb/gpuweb/issues/3479 and the spec:
        // https://www.w3.org/TR/WGSL/#uniformity
        output.push_str("diagnostic(warning,derivative_uniformity);\n");

        if self.shader_type == ShaderType::Vertex
            && !self.pipeline_outputs.iter().any(|v| v.built_in == Some(BuiltIn::Position))
        {
            self.global_variables.push_str("var<private> gl_Position : vec4<f32>;\n");
            self.pipeline_outputs.push(PipelineVar {
                name: "gl_Position".to_string(),
                type_str: "vec4<f32>".to_string(),
                annotation: "@builtin(position)".to_string(),
                built_in: Some(BuiltIn::Position),
            });
        }

        if !self.global_variables.is_empty() {
            output.push_str(&self.global_variables);
            output.push('\n');
        }

        if !self.pipeline_inputs.is_empty() {
            output.push_str("struct ANGLE_Input_Annotated {\n");
            for v in &self.pipeline_inputs {
                writeln!(output, "  {} {} : {},", v.annotation, v.name, v.type_str).unwrap();
            }
            output.push_str("};\n\n");
        }

        if !self.pipeline_outputs.is_empty() {
            output.push_str("struct ANGLE_Output_Annotated {\n");
            for v in &self.pipeline_outputs {
                writeln!(output, "  {} {} : {},", v.annotation, v.name, v.type_str).unwrap();
            }
            output.push_str("};\n\n");
        }

        if !self.type_declarations.is_empty() {
            output.push_str(&self.type_declarations);
            output.push('\n');
        }
        if !self.function_declarations.is_empty() {
            output.push_str(&self.function_declarations);
        }

        let stage_attr = match self.shader_type {
            ShaderType::Vertex => "@vertex",
            ShaderType::Fragment => "@fragment",
            ShaderType::Compute => unimplemented!(),
            _ => unreachable!(),
        };

        let has_inputs = !self.pipeline_inputs.is_empty();
        let has_outputs = !self.pipeline_outputs.is_empty();

        let param_str =
            if has_inputs { "ANGLE_input_annotated : ANGLE_Input_Annotated" } else { "" };
        let ret_str = if has_outputs { " -> ANGLE_Output_Annotated" } else { "" };

        writeln!(output, "{stage_attr}\nfn wgslMain({param_str}){ret_str}\n{{").unwrap();
        if has_inputs {
            for input_var in &self.pipeline_inputs {
                let vname = &input_var.name;
                let expr = if matches!(
                    input_var.built_in,
                    Some(
                        BuiltIn::VertexIndex
                            | BuiltIn::VertexID
                            | BuiltIn::InstanceIndex
                            | BuiltIn::InstanceID
                    )
                ) {
                    format!("i32(ANGLE_input_annotated.{vname})")
                } else {
                    format!("ANGLE_input_annotated.{vname}")
                };
                writeln!(output, "  {vname} = {expr};").unwrap();
            }
        }
        output.push_str("  main();\n");
        if has_outputs {
            output.push_str("  var ANGLE_output_annotated : ANGLE_Output_Annotated;\n");
            for output_var in &self.pipeline_outputs {
                let vname = &output_var.name;
                writeln!(output, "  ANGLE_output_annotated.{vname} = {vname};").unwrap();
            }
            output.push_str("  return ANGLE_output_annotated;\n");
        }
        output.push_str("}\n\n");
        output
    }

    fn new_type(&mut self, _ir_meta: &IRMeta, id: TypeId, type_info: &Type) {
        let type_str = match type_info {
            Type::Scalar(..) | Type::Vector(..) | Type::Matrix(..) => Self::basic_type_str(id),
            Type::Array(type_id, count) => {
                let base_type = &self.types[type_id];
                format!("array<{base_type}, {count}>")
            }
            Type::UnsizedArray(type_id) => {
                let base_type = &self.types[type_id];
                format!("array<{base_type}>")
            }
            Type::Image(basic_type, image_type) => Self::image_type_str(*basic_type, *image_type),
            Type::Struct(name, fields, _specialization) => {
                let struct_name = Self::name_str(name, TEMP_STRUCT_PREFIX, id.id);

                let mut struct_def = format!(
                    "struct {struct_name}
{{
"
                );
                for (index, field) in fields.iter().enumerate() {
                    let field_type_str = &self.types[&field.type_id];
                    let field_name =
                        Self::name_str(&field.name, TEMP_STRUCT_FIELD_PREFIX, index as u32);

                    writeln!(struct_def, "  {field_name} : {field_type_str},").unwrap();
                }
                struct_def.push_str(
                    "};

",
                );
                self.type_declarations.push_str(&struct_def);
                struct_name
            }
            Type::Pointer(pointee_type_id) => self.types[pointee_type_id].clone(),
            Type::DeadCodeEliminated => return,
        };

        self.types.insert(id, type_str);
    }

    fn new_constant(&mut self, _ir_meta: &IRMeta, id: ConstantId, constant: &Constant) {
        let type_info = &self.types[&constant.type_id];
        let (constant_text, precedence) = match &constant.value {
            &ConstantValue::Float(f) => (
                format!("{f:?}f"),
                if f.is_sign_negative() { Precedence::Negate } else { Precedence::Primary },
            ),
            &ConstantValue::Int(i) => {
                (format!("{i}i"), if i < 0 { Precedence::Negate } else { Precedence::Primary })
            }
            &ConstantValue::Uint(u) => (format!("{u}u"), Precedence::Primary),
            &ConstantValue::Bool(b) => (b.to_string(), Precedence::Primary),
            &ConstantValue::YuvCsc(_) => ("0".to_string(), Precedence::Primary),
            ConstantValue::Composite(elements) => (
                format!(
                    "{type_info}({})",
                    elements
                        .iter()
                        .map(|element| self.constants[element].text.as_str())
                        .collect::<Vec<_>>()
                        .join(", ")
                ),
                Precedence::Primary,
            ),
        };
        self.constants.insert(id, WgslConstant { text: constant_text, precedence });
    }

    fn new_variable(&mut self, ir_meta: &IRMeta, id: VariableId, variable: &Variable) {
        self.shader_type = ir_meta.get_shader_type();

        let name = if let Some(built_in) = variable.built_in {
            match built_in {
                BuiltIn::Position => "gl_Position".to_string(),
                BuiltIn::FragCoord => "gl_FragCoord".to_string(),
                BuiltIn::FragColor => "gl_FragColor".to_string(),
                BuiltIn::FragDepth => "gl_FragDepth".to_string(),
                BuiltIn::FrontFacing => "gl_FrontFacing".to_string(),
                BuiltIn::PointCoord => "gl_PointCoord".to_string(),
                BuiltIn::VertexID => "gl_VertexID".to_string(),
                BuiltIn::InstanceID => "gl_InstanceID".to_string(),
                BuiltIn::VertexIndex => "gl_VertexIndex".to_string(),
                BuiltIn::InstanceIndex => "gl_InstanceIndex".to_string(),
                BuiltIn::GlobalInvocationID => "gl_GlobalInvocationID".to_string(),
                BuiltIn::LocalInvocationID => "gl_LocalInvocationID".to_string(),
                BuiltIn::WorkGroupID => "gl_WorkGroupID".to_string(),
                BuiltIn::LocalInvocationIndex => "gl_LocalInvocationIndex".to_string(),
                BuiltIn::NumWorkGroups => "gl_NumWorkGroups".to_string(),
                BuiltIn::SampleID => "gl_SampleID".to_string(),
                BuiltIn::SampleMask => "gl_SampleMask".to_string(),
                _ => "UnsupportedBuiltIn".to_string(),
            }
        } else {
            Self::name_str(&variable.name, TEMP_VARIABLE_PREFIX, id.id)
        };
        let type_info = &self.types[&variable.type_id];
        let mut decl_text = String::new();

        let mut binding = None;

        for &dec in &variable.decorations.decorations {
            match dec {
                Decoration::Binding(b) => binding = Some(b),
                Decoration::Buffer | Decoration::Shared => unimplemented!(),
                _ => {}
            }
        }

        let is_global = variable.is_interface_variable() || variable.scope == VariableScope::Global;
        if is_global {
            if let Some(binding_val) = binding {
                write!(decl_text, "@group(1) @binding({binding_val}) ").unwrap();
            }

            if let Some(init) = variable.initializer {
                let init_expr = &self.constants[&init].text;
                write!(decl_text, "var<private> {name} : {type_info} = {init_expr};").unwrap();
            } else {
                write!(decl_text, "var<private> {name} : {type_info};").unwrap();
            }
            self.global_variables.push_str(&decl_text);
            self.global_variables.push('\n');
            // Record pipeline inputs and outputs
            if let Some(built_in) = variable.built_in {
                match (built_in, self.shader_type) {
                    (BuiltIn::Position, ShaderType::Vertex) => {
                        // WGSL only allows @invariant on the vertex position output.
                        let annotation = if variable.decorations.has(Decoration::Invariant) {
                            "@builtin(position) @invariant".to_string()
                        } else {
                            Self::builtin_annotation_str(built_in)
                        };
                        self.pipeline_outputs.push(PipelineVar {
                            name: name.clone(),
                            type_str: type_info.clone(),
                            annotation,
                            built_in: Some(built_in),
                        });
                    }
                    (BuiltIn::FragDepth, ShaderType::Fragment) => {
                        self.pipeline_outputs.push(PipelineVar {
                            name: name.clone(),
                            type_str: type_info.clone(),
                            annotation: Self::builtin_annotation_str(built_in),
                            built_in: Some(built_in),
                        });
                    }
                    (BuiltIn::FragColor, ShaderType::Fragment) => {
                        self.pipeline_outputs.push(PipelineVar {
                            name: name.clone(),
                            type_str: type_info.clone(),
                            annotation: "@location(0)".to_string(),
                            built_in: Some(built_in),
                        });
                    }
                    (BuiltIn::FragCoord | BuiltIn::FrontFacing, ShaderType::Fragment) => {
                        self.pipeline_inputs.push(PipelineVar {
                            name: name.clone(),
                            type_str: type_info.clone(),
                            annotation: Self::builtin_annotation_str(built_in),
                            built_in: Some(built_in),
                        });
                    }
                    (
                        BuiltIn::VertexIndex
                        | BuiltIn::VertexID
                        | BuiltIn::InstanceIndex
                        | BuiltIn::InstanceID,
                        ShaderType::Vertex,
                    ) => {
                        self.pipeline_inputs.push(PipelineVar {
                            name: name.clone(),
                            type_str: "u32".to_string(),
                            annotation: Self::builtin_annotation_str(built_in),
                            built_in: Some(built_in),
                        });
                    }
                    _ => {}
                }
            } else if variable.is_interface_variable()
                && !variable.decorations.has(Decoration::Uniform)
            {
                let is_in = variable.decorations.has(Decoration::Input)
                    || (self.shader_type == ShaderType::Vertex
                        && !variable.decorations.has(Decoration::Output));
                let is_out = variable.decorations.has(Decoration::Output)
                    || (self.shader_type == ShaderType::Fragment
                        && !variable.decorations.has(Decoration::Input));
                if is_in {
                    self.pipeline_inputs.push(PipelineVar {
                        name: name.clone(),
                        type_str: type_info.clone(),
                        annotation: "@location(@@@@@@)".to_string(),
                        built_in: None,
                    });
                } else if is_out {
                    self.pipeline_outputs.push(PipelineVar {
                        name: name.clone(),
                        type_str: type_info.clone(),
                        annotation: "@location(@@@@@@)".to_string(),
                        built_in: None,
                    });
                }
            }
        } else if let Some(init) = variable.initializer {
            let init_expr = &self.constants[&init].text;
            write!(decl_text, "var {name} : {type_info} = {init_expr};").unwrap();
        } else {
            write!(decl_text, "var {name} : {type_info};").unwrap();
        }

        self.variables.insert(
            id,
            WgslVariable {
                declaration_text: decl_text,
                name,
                type_id: variable.type_id,
                skip_declaration: is_global,
            },
        );
    }

    fn new_function(&mut self, _ir_meta: &IRMeta, id: FunctionId, function: &Function) {
        let return_type = &self.types[&function.return_type_id];
        let name = Self::name_str(&function.name, TEMP_FUNCTION_PREFIX, id.id);
        let ret_str = if function.return_type_id == TYPE_ID_VOID {
            "".to_string()
        } else {
            format!(" -> {return_type}")
        };

        let params_str = function
            .params
            .iter()
            .map(|param| {
                let param_var = &self.variables[&param.variable_id];
                let type_info = &self.types[&param_var.type_id];
                format!("{}: {type_info}", param_var.name)
            })
            .collect::<Vec<_>>()
            .join(", ");

        let declaration_text = format!("fn {name}({params_str}){ret_str}");
        self.functions.insert(id, WgslFunction { declaration_text, name });
    }

    fn global_scope(&mut self, _ir_meta: &IRMeta) {}

    fn begin_block(
        &mut self,
        _ir_meta: &IRMeta,
        variables: &[VariableId],
        for_loop_variable: Option<VariableId>,
    ) -> String {
        let mut block = String::new();
        for &id in variables {
            let var = &self.variables[&id];
            if !var.skip_declaration && for_loop_variable != Some(id) {
                writeln!(block, "{}", var.declaration_text).unwrap();
            }
        }
        block
    }

    fn merge_blocks(&mut self, blocks: Vec<String>) -> String {
        blocks.join("")
    }

    fn swizzle_single(&mut self, _block: &mut String, result: RegisterId, id: TypedId, index: u32) {
        let base = self.get_expr(id);
        let res_expr = format!(
            "{}.{}",
            base.precedence.parenthesize_if_needed(&base.text, Precedence::Primary),
            utils::get_swizzle(index)
        );
        self.expressions
            .insert(result, ExprInfo { text: res_expr, precedence: Precedence::Primary });
    }

    fn swizzle_multi(
        &mut self,
        _block: &mut String,
        result: RegisterId,
        id: TypedId,
        indices: &[u32],
    ) {
        let base = self.get_expr(id);
        let res_expr = format!(
            "{}.{}",
            base.precedence.parenthesize_if_needed(&base.text, Precedence::Primary),
            utils::get_swizzle_multi(indices)
        );
        self.expressions
            .insert(result, ExprInfo { text: res_expr, precedence: Precedence::Primary });
    }

    fn index(&mut self, _block: &mut String, result: RegisterId, id: TypedId, index: TypedId) {
        let base = self.get_expr(id);
        let idx = self.get_expr(index);

        self.expressions.insert(
            result,
            ExprInfo {
                text: format!(
                    "{}[{}]",
                    base.precedence.parenthesize_if_needed(&base.text, Precedence::Primary),
                    idx.text
                ),
                precedence: Precedence::Primary,
            },
        );
    }

    fn select_field(
        &mut self,
        _block: &mut String,
        result: RegisterId,
        id: TypedId,
        index: u32,
        field: &Field,
    ) {
        let base = self.get_expr(id);
        let field_name = Self::name_str(&field.name, TEMP_STRUCT_FIELD_PREFIX, index);

        let text = format!(
            "{}.{field_name}",
            base.precedence.parenthesize_if_needed(&base.text, Precedence::Primary)
        );
        self.expressions.insert(result, ExprInfo { text, precedence: Precedence::Primary });
    }

    fn construct_single(
        &mut self,
        _block: &mut String,
        result: RegisterId,
        type_id: TypeId,
        id: TypedId,
    ) {
        let type_name = Self::basic_type_str(type_id);
        let expr = self.get_expression(id);
        self.expressions.insert(
            result,
            ExprInfo { text: format!("{type_name}({expr})"), precedence: Precedence::Primary },
        );
    }

    fn construct_multi(
        &mut self,
        _block: &mut String,
        result: RegisterId,
        type_id: TypeId,
        ids: &[TypedId],
    ) {
        let type_name =
            self.types.get(&type_id).cloned().unwrap_or_else(|| Self::basic_type_str(type_id));
        let args = self.get_expressions(ids);
        self.expressions.insert(
            result,
            ExprInfo { text: format!("{type_name}({args})"), precedence: Precedence::Primary },
        );
    }

    fn load(&mut self, _block: &mut String, result: RegisterId, pointer: TypedId) {
        let ptr = self.get_expr(pointer);
        self.expressions.insert(result, ExprInfo { text: ptr.text, precedence: ptr.precedence });
    }

    fn store(&mut self, block: &mut String, pointer: TypedId, value: TypedId) {
        let ptr_expr = self.get_expression(pointer);
        let val_expr = self.get_expression(value);
        writeln!(block, "{ptr_expr} = {val_expr};").unwrap();
    }

    fn call(
        &mut self,
        block: &mut String,
        result: Option<RegisterId>,
        function_id: FunctionId,
        params: &[TypedId],
        has_side_effect_with_unused_result: bool,
    ) {
        let func = &self.functions[&function_id];
        let args = self.get_expressions(params);
        let call_expr = format!("{}({args})", func.name);
        if let Some(res) = result {
            if has_side_effect_with_unused_result {
                writeln!(block, "{call_expr};").unwrap();
            } else {
                self.expressions
                    .insert(res, ExprInfo { text: call_expr, precedence: Precedence::Primary });
            }
        } else {
            writeln!(block, "{call_expr};").unwrap();
        }
    }

    fn unary(
        &mut self,
        block: &mut String,
        result: RegisterId,
        unary_op: UnaryOpCode,
        id: TypedId,
        has_side_effect_with_unused_result: bool,
    ) {
        let expr = self.get_expr(id);
        let (res_expr, stmt, precedence) = if let Some(builtin) = Self::unary_builtin_str(unary_op)
        {
            (format!("{builtin}({})", expr.text), None, Precedence::Primary)
        } else {
            match unary_op {
                UnaryOpCode::Negate => (
                    format!(
                        "-{}",
                        expr.precedence.parenthesize_if_needed(&expr.text, Precedence::Unary)
                    ),
                    None,
                    Precedence::Negate,
                ),
                UnaryOpCode::LogicalNot | UnaryOpCode::Not => (
                    format!(
                        "!{}",
                        expr.precedence.parenthesize_if_needed(&expr.text, Precedence::Unary)
                    ),
                    None,
                    Precedence::Unary,
                ),
                UnaryOpCode::BitwiseNot => (
                    format!(
                        "~{}",
                        expr.precedence.parenthesize_if_needed(&expr.text, Precedence::Unary)
                    ),
                    None,
                    Precedence::Unary,
                ),
                // Note: This implementation is buggy (postfix returns the incremented/decremented
                // value instead of the original value, and `+ 1`/`- 1` uses an untyped integer
                // literal). Increments and decrements will be implemented properly in a later
                // commit by lowering them in the IR.
                // The assignment is emitted as a statement above, and the resulting expression
                // value used in outer expressions is the variable/lvalue itself
                // (`expr.text`), which retains `expr.precedence` (typically
                // `Precedence::Primary`).
                UnaryOpCode::PrefixIncrement | UnaryOpCode::PostfixIncrement => {
                    let stmt = format!("{} = {} + 1;", expr.text, expr.text);
                    (expr.text, Some(stmt), expr.precedence)
                }
                UnaryOpCode::PrefixDecrement | UnaryOpCode::PostfixDecrement => {
                    let stmt = format!("{} = {} - 1;", expr.text, expr.text);
                    (expr.text, Some(stmt), expr.precedence)
                }
                UnaryOpCode::ArrayLength
                | UnaryOpCode::Isnan
                | UnaryOpCode::Isinf
                | UnaryOpCode::FloatBitsToInt
                | UnaryOpCode::FloatBitsToUint
                | UnaryOpCode::IntBitsToFloat
                | UnaryOpCode::UintBitsToFloat
                | UnaryOpCode::PackSnorm2x16
                | UnaryOpCode::PackHalf2x16
                | UnaryOpCode::UnpackSnorm2x16
                | UnaryOpCode::UnpackHalf2x16
                | UnaryOpCode::PackUnorm2x16
                | UnaryOpCode::UnpackUnorm2x16
                | UnaryOpCode::PackUnorm4x8
                | UnaryOpCode::PackSnorm4x8
                | UnaryOpCode::Inverse
                | UnaryOpCode::BitfieldReverse
                | UnaryOpCode::BitCount
                | UnaryOpCode::FindLSB
                | UnaryOpCode::FindMSB
                | UnaryOpCode::InterpolateAtCentroid
                | UnaryOpCode::AtomicCounter
                | UnaryOpCode::AtomicCounterIncrement
                | UnaryOpCode::AtomicCounterDecrement
                | UnaryOpCode::ImageSize
                | UnaryOpCode::PixelLocalLoadANGLE => {
                    // Placeholder for unsupported or unimplemented unary operations that are
                    // translated in subsequent commits.
                    (format!("UNIMPLEMENTED_OP({})", expr.text), None, Precedence::Primary)
                }
                _ => {
                    unreachable!(
                        "Internal error: Unary built-ins are handled by unary_builtin_str above"
                    )
                }
            }
        };
        if let Some(s) = stmt {
            writeln!(block, "{s}").unwrap();
        } else if has_side_effect_with_unused_result {
            writeln!(block, "{res_expr};").unwrap();
        }
        self.expressions.insert(result, ExprInfo { text: res_expr, precedence });
    }

    fn binary(
        &mut self,
        block: &mut String,
        result: RegisterId,
        binary_op: BinaryOpCode,
        lhs: TypedId,
        rhs: TypedId,
        has_side_effect_with_unused_result: bool,
    ) {
        let l = self.get_expr(lhs);
        let r = self.get_expr(rhs);
        let info = match binary_op {
            BinaryOpCode::Add => Some(BinaryOpInfo::new("+", Precedence::Additive)),
            BinaryOpCode::Sub => Some(BinaryOpInfo::new("-", Precedence::Additive)),
            BinaryOpCode::Mul
            | BinaryOpCode::VectorTimesScalar
            | BinaryOpCode::MatrixTimesScalar
            | BinaryOpCode::VectorTimesMatrix
            | BinaryOpCode::MatrixTimesVector
            | BinaryOpCode::MatrixTimesMatrix => {
                Some(BinaryOpInfo::new("*", Precedence::Multiplicative))
            }
            BinaryOpCode::Div => Some(BinaryOpInfo::new("/", Precedence::Multiplicative)),
            BinaryOpCode::IMod | BinaryOpCode::Mod => {
                Some(BinaryOpInfo::new("%", Precedence::Multiplicative))
            }
            BinaryOpCode::BitShiftLeft => Some(BinaryOpInfo::new("<<", Precedence::Shift)),
            BinaryOpCode::BitShiftRight => Some(BinaryOpInfo::new(">>", Precedence::Shift)),
            BinaryOpCode::LessThan | BinaryOpCode::LessThanVec => {
                Some(BinaryOpInfo::new("<", Precedence::Relational))
            }
            BinaryOpCode::GreaterThan | BinaryOpCode::GreaterThanVec => {
                Some(BinaryOpInfo::new(">", Precedence::Relational))
            }
            BinaryOpCode::LessThanEqual | BinaryOpCode::LessThanEqualVec => {
                Some(BinaryOpInfo::new("<=", Precedence::Relational))
            }
            BinaryOpCode::GreaterThanEqual | BinaryOpCode::GreaterThanEqualVec => {
                Some(BinaryOpInfo::new(">=", Precedence::Relational))
            }
            BinaryOpCode::Equal | BinaryOpCode::EqualVec => {
                Some(BinaryOpInfo::new("==", Precedence::Relational))
            }
            BinaryOpCode::NotEqual | BinaryOpCode::NotEqualVec | BinaryOpCode::LogicalXor => {
                Some(BinaryOpInfo::new("!=", Precedence::Relational))
            }
            BinaryOpCode::BitwiseOr => Some(BinaryOpInfo::new("|", Precedence::BitwiseOr)),
            BinaryOpCode::BitwiseXor => Some(BinaryOpInfo::new("^", Precedence::BitwiseXor)),
            BinaryOpCode::BitwiseAnd => Some(BinaryOpInfo::new("&", Precedence::BitwiseAnd)),
            _ => None,
        };

        let (res_expr, precedence) = if let Some(info) = info {
            info.format_expr(&l.text, l.precedence, &r.text, r.precedence)
        } else {
            let fn_call = match binary_op {
                BinaryOpCode::Atan => format!("atan2({}, {})", l.text, r.text),
                BinaryOpCode::Pow => format!("pow({}, {})", l.text, r.text),
                BinaryOpCode::Min => format!("min({}, {})", l.text, r.text),
                BinaryOpCode::Max => format!("max({}, {})", l.text, r.text),
                BinaryOpCode::Step => format!("step({}, {})", l.text, r.text),
                BinaryOpCode::Distance => format!("distance({}, {})", l.text, r.text),
                BinaryOpCode::Dot => format!("dot({}, {})", l.text, r.text),
                BinaryOpCode::Cross => format!("cross({}, {})", l.text, r.text),
                BinaryOpCode::Reflect => format!("reflect({}, {})", l.text, r.text),
                BinaryOpCode::Modf
                | BinaryOpCode::Frexp
                | BinaryOpCode::Ldexp
                | BinaryOpCode::MatrixCompMult
                | BinaryOpCode::OuterProduct
                | BinaryOpCode::InterpolateAtSample
                | BinaryOpCode::InterpolateAtOffset
                | BinaryOpCode::AtomicAdd
                | BinaryOpCode::AtomicMin
                | BinaryOpCode::AtomicMax
                | BinaryOpCode::AtomicAnd
                | BinaryOpCode::AtomicOr
                | BinaryOpCode::AtomicXor
                | BinaryOpCode::AtomicExchange => {
                    // Placeholder for unsupported or unimplemented binary operations that are
                    // translated in subsequent commits.
                    format!("UNIMPLEMENTED_OP({}, {})", l.text, r.text)
                }
                _ => {
                    unreachable!(
                        "Internal error: Binary operators are handled by BinaryOpInfo above"
                    )
                }
            };
            (fn_call, Precedence::Primary)
        };
        if has_side_effect_with_unused_result {
            writeln!(block, "{res_expr};").unwrap();
        } else {
            self.expressions.insert(result, ExprInfo { text: res_expr, precedence });
        }
    }

    fn built_in(
        &mut self,
        block: &mut String,
        result: Option<RegisterId>,
        built_in_op: BuiltInOpCode,
        args: &[TypedId],
        has_side_effect_with_unused_result: bool,
    ) {
        let args_str = self.get_expressions(args);
        let res_expr = match built_in_op {
            BuiltInOpCode::Clamp => format!("clamp({args_str})"),
            BuiltInOpCode::Mix => format!("mix({args_str})"),
            BuiltInOpCode::Smoothstep => format!("smoothstep({args_str})"),
            BuiltInOpCode::Fma => format!("fma({args_str})"),
            BuiltInOpCode::Faceforward => format!("faceForward({args_str})"),
            BuiltInOpCode::Refract => format!("refract({args_str})"),
            _ => format!("/* UNIMPLEMENTED */({args_str})"),
        };

        if let Some(res) = result {
            if has_side_effect_with_unused_result {
                writeln!(block, "{res_expr};").unwrap();
            } else {
                self.expressions
                    .insert(res, ExprInfo { text: res_expr, precedence: Precedence::Primary });
            }
        } else {
            writeln!(block, "{res_expr};").unwrap();
        }
    }

    fn texture(
        &mut self,
        _ir_meta: &IRMeta,
        _block: &mut String,
        result: RegisterId,
        _sampler: TypedId,
        _coord: TypedId,
        _is_proj: bool,
        _offset: Option<TypedId>,
    ) {
        // Placeholder: texture lookup codegen is implemented in a later commit.
        self.expressions.insert(
            result,
            ExprInfo { text: "/* UNIMPLEMENTED */".to_string(), precedence: Precedence::Primary },
        );
    }

    fn texture_compare(
        &mut self,
        _ir_meta: &IRMeta,
        _block: &mut String,
        result: RegisterId,
        _sampler: TypedId,
        _coord: TypedId,
        _compare: TypedId,
    ) {
        // Placeholder: texture lookup codegen is implemented in a later commit.
        self.expressions.insert(
            result,
            ExprInfo { text: "/* UNIMPLEMENTED */".to_string(), precedence: Precedence::Primary },
        );
    }

    fn texture_lod(
        &mut self,
        _ir_meta: &IRMeta,
        _block: &mut String,
        result: RegisterId,
        _sampler: TypedId,
        _coord: TypedId,
        _is_proj: bool,
        _lod: TypedId,
        _offset: Option<TypedId>,
    ) {
        // Placeholder: texture lookup codegen is implemented in a later commit.
        self.expressions.insert(
            result,
            ExprInfo { text: "/* UNIMPLEMENTED */".to_string(), precedence: Precedence::Primary },
        );
    }

    fn texture_compare_lod(
        &mut self,
        _ir_meta: &IRMeta,
        _block: &mut String,
        result: RegisterId,
        _sampler: TypedId,
        _coord: TypedId,
        _compare: TypedId,
        _lod: TypedId,
    ) {
        // Placeholder: texture lookup codegen is implemented in a later commit.
        self.expressions.insert(
            result,
            ExprInfo { text: "/* UNIMPLEMENTED */".to_string(), precedence: Precedence::Primary },
        );
    }

    fn texture_bias(
        &mut self,
        _ir_meta: &IRMeta,
        _block: &mut String,
        result: RegisterId,
        _sampler: TypedId,
        _coord: TypedId,
        _is_proj: bool,
        _bias: TypedId,
        _offset: Option<TypedId>,
    ) {
        // Placeholder: texture lookup codegen is implemented in a later commit.
        self.expressions.insert(
            result,
            ExprInfo { text: "/* UNIMPLEMENTED */".to_string(), precedence: Precedence::Primary },
        );
    }

    fn texture_compare_bias(
        &mut self,
        _ir_meta: &IRMeta,
        _block: &mut String,
        result: RegisterId,
        _sampler: TypedId,
        _coord: TypedId,
        _compare: TypedId,
        _bias: TypedId,
    ) {
        // Placeholder: texture lookup codegen is implemented in a later commit.
        self.expressions.insert(
            result,
            ExprInfo { text: "/* UNIMPLEMENTED */".to_string(), precedence: Precedence::Primary },
        );
    }

    fn texture_grad(
        &mut self,
        _ir_meta: &IRMeta,
        _block: &mut String,
        result: RegisterId,
        _sampler: TypedId,
        _coord: TypedId,
        _is_proj: bool,
        _dx: TypedId,
        _dy: TypedId,
        _offset: Option<TypedId>,
    ) {
        // Placeholder: texture lookup codegen is implemented in a later commit.
        self.expressions.insert(
            result,
            ExprInfo { text: "/* UNIMPLEMENTED */".to_string(), precedence: Precedence::Primary },
        );
    }

    fn texture_gather(
        &mut self,
        _ir_meta: &IRMeta,
        _block: &mut String,
        result: RegisterId,
        _sampler: TypedId,
        _coord: TypedId,
        _offset: Option<TypedId>,
    ) {
        // Placeholder: texture lookup codegen is implemented in a later commit.
        self.expressions.insert(
            result,
            ExprInfo { text: "/* UNIMPLEMENTED */".to_string(), precedence: Precedence::Primary },
        );
    }

    fn texture_gather_component(
        &mut self,
        _ir_meta: &IRMeta,
        _block: &mut String,
        result: RegisterId,
        _sampler: TypedId,
        _coord: TypedId,
        _component: TypedId,
        _offset: Option<TypedId>,
    ) {
        // Placeholder: texture lookup codegen is implemented in a later commit.
        self.expressions.insert(
            result,
            ExprInfo { text: "/* UNIMPLEMENTED */".to_string(), precedence: Precedence::Primary },
        );
    }

    fn texture_gather_ref(
        &mut self,
        _ir_meta: &IRMeta,
        _block: &mut String,
        result: RegisterId,
        _sampler: TypedId,
        _coord: TypedId,
        _refz: TypedId,
        _offset: Option<TypedId>,
    ) {
        // Placeholder: texture lookup codegen is implemented in a later commit.
        self.expressions.insert(
            result,
            ExprInfo { text: "/* UNIMPLEMENTED */".to_string(), precedence: Precedence::Primary },
        );
    }

    fn branch_discard(&mut self, block: &mut String) {
        writeln!(block, "discard;").unwrap();
    }

    fn branch_return(&mut self, block: &mut String, value: Option<TypedId>) {
        if let Some(val) = value {
            let expr = self.get_expression(val);
            writeln!(block, "return {expr};").unwrap();
        } else {
            writeln!(block, "return;").unwrap();
        }
    }

    fn branch_break(&mut self, block: &mut String) {
        writeln!(block, "break;").unwrap();
    }

    fn branch_continue(&mut self, block: &mut String) {
        writeln!(block, "continue;").unwrap();
    }

    fn branch_if(
        &mut self,
        block: &mut String,
        condition: TypedId,
        true_block: Option<String>,
        false_block: Option<String>,
    ) {
        let cond = self.get_expression(condition);
        writeln!(block, "if ({cond}) {{").unwrap();
        if let Some(tb) = true_block {
            block.push_str(&Self::indent_block(tb, 1));
        }
        if let Some(fb) = false_block
            && !fb.is_empty()
        {
            writeln!(block, "}} else {{").unwrap();
            block.push_str(&Self::indent_block(fb, 1));
        }
        writeln!(block, "}}").unwrap();
    }

    /// Emits a WGSL structured `loop { ... }` construct.
    ///
    /// In WGSL, `loop` is the canonical structured loop statement. It repeatedly executes its
    /// body until an explicit `break` (or `return`/`discard`) is encountered.
    /// For general loops (`OpCode::Loop`), `loop_condition_block` is placed at the head of the
    /// loop body and terminates with `branch_loop_if`, which evaluates the continuation condition
    /// and breaks out when false (`if (!cond) { break; }`).
    fn branch_loop(
        &mut self,
        block: &mut String,
        loop_condition_block: Option<String>,
        body_block: Option<String>,
    ) {
        writeln!(block, "loop {{").unwrap();
        if let Some(lcb) = loop_condition_block {
            block.push_str(&Self::indent_block(lcb, 1));
        }
        if let Some(bb) = body_block {
            block.push_str(&Self::indent_block(bb, 1));
        }
        writeln!(block, "}}").unwrap();
    }

    /// Emits a WGSL `loop { ... }` construct for a do-while loop.
    ///
    /// The body executes first, and the loop continuation condition (lowered into
    /// a conditional break at the bottom of the body) breaks out of the loop when false.
    fn branch_do_loop(&mut self, block: &mut String, body_block: Option<String>) {
        writeln!(block, "loop {{").unwrap();
        if let Some(bb) = body_block {
            block.push_str(&Self::indent_block(bb, 1));
        }
        writeln!(block, "}}").unwrap();
    }

    fn branch_for_loop(
        &mut self,
        block: &mut String,
        info: &util::TrivialLoopInfo,
        body_block: Option<String>,
    ) {
        let var_name = &self.variables[&info.loop_variable].name;
        let init = &self.constants[&info.variable_initializer].text;
        let cond_op = match info.condition_op {
            BinaryOpCode::LessThan => "<",
            BinaryOpCode::LessThanEqual => "<=",
            BinaryOpCode::GreaterThan => ">",
            BinaryOpCode::GreaterThanEqual => ">=",
            BinaryOpCode::NotEqual => "!=",
            BinaryOpCode::Equal => "==",
            _ => unreachable!(),
        };
        let limit = &self.constants[&info.condition_comparator].text;
        let step_str = if let Some(step_id) = info.increment_step {
            let step = &self.constants[&step_id].text;
            if info.ascending {
                format!("{var_name} += {step}")
            } else {
                format!("{var_name} -= {step}")
            }
        } else if info.ascending {
            format!("{var_name} += 1")
        } else {
            format!("{var_name} -= 1")
        };

        writeln!(
            block,
            "for (var {var_name} = {init}; {var_name} {cond_op} {limit}; {step_str}) {{"
        )
        .unwrap();
        if let Some(bb) = body_block {
            block.push_str(&Self::indent_block(bb, 1));
        }
        writeln!(block, "}}").unwrap();
    }

    /// Emits a loop exit condition check (`if (!({cond})) { break; }`).
    ///
    /// This terminates a loop condition block, causing execution to break out of the
    /// enclosing WGSL `loop { ... }` whenever the loop continuation condition evaluates to false.
    fn branch_loop_if(&mut self, block: &mut String, condition: TypedId) {
        let cond = self.get_expression(condition);
        writeln!(block, "if (!({cond})) {{ break; }}").unwrap();
    }

    fn branch_switch(
        &mut self,
        block: &mut String,
        value: TypedId,
        case_ids: &[Option<ConstantId>],
        case_blocks: Vec<String>,
    ) {
        let val = self.get_expression(value);
        writeln!(block, "switch ({val}) {{").unwrap();

        let mut has_default = false;
        for (i, &case_id) in case_ids.iter().enumerate() {
            let case_body = case_blocks.get(i).cloned().unwrap_or_default();
            if let Some(cid) = case_id {
                let const_val = &self.constants[&cid].text;
                writeln!(block, "    case {const_val}: {{").unwrap();
            } else {
                has_default = true;
                writeln!(block, "    default: {{").unwrap();
            }
            block.push_str(&Self::indent_block(case_body, 2));
            writeln!(block, "    }}").unwrap();
        }

        if !has_default {
            writeln!(block, "    default: {{}}").unwrap();
        }

        writeln!(block, "}}").unwrap();
    }

    fn end_function(&mut self, block: String, id: FunctionId) {
        let func = &self.functions[&id];
        let decl = &func.declaration_text;
        let mut func_text = format!(
            "{decl}
{{
"
        );
        func_text.push_str(&Self::indent_block(block, 1));
        func_text.push_str(
            "}

",
        );
        self.function_declarations.push_str(&func_text);
    }
}

pub fn generate(ir: &mut IR, _options: &compile::Options) {
    {
        let transform_options = transform::add_driver_uniforms::Options {
            // Only needed for SPIR-V
            add_transform_xy: false,
            add_xfb_emulation: false,
            // Only needed for MSL
            declare_as_struct: false,
            add_coverage_mask: false,
        };
        transform::run!(add_driver_uniforms, ir, &transform_options);
    }

    {
        let transform_options = transform::monomorphize_unsupported_functions::Options {
            struct_containing_samplers: true,
            image: false,
            atomic_counter: false,
            array_of_array_of_sampler_or_image: false,
            pixel_local_storage: false,
        };
        transform::run!(monomorphize_unsupported_functions, ir, &transform_options);
    }
}
