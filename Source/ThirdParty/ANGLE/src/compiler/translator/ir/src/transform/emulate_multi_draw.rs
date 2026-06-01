// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Transform gl_DrawID from ANGLE_multi_draw to a user uniform.
//
// Transform gl_BaseVertex and gl_BaseInstance from ANGLE_base_vertex_base_instance_shader_builtin
// to user uniforms.
//
// As a driver bug workaround, add gl_BaseVertex to gl_VertexID if required.
use crate::ir::*;
use crate::*;

pub struct Options {
    pub emulate_draw_id: bool,
    pub emulate_base_vertex_instance: bool,
    pub add_base_vertex_to_vertex_id: bool,
}

pub fn run(ir: &mut IR, options: &Options) {
    if options.emulate_draw_id {
        replace_with_uniform(&mut ir.meta, BuiltIn::DrawID, "angle_DrawID");
    }
    if options.emulate_base_vertex_instance {
        if options.add_base_vertex_to_vertex_id {
            add_base_vertex_to_vertex_id(ir);
        }
        replace_with_uniform(&mut ir.meta, BuiltIn::BaseVertex, "angle_BaseVertex");
        replace_with_uniform(&mut ir.meta, BuiltIn::BaseInstance, "angle_BaseInstance");
    }
}

fn replace_with_uniform(ir_meta: &mut IRMeta, built_in: BuiltIn, name: &'static str) {
    if let Some(var_id) = ir_meta.get_built_in_variable(built_in) {
        let var = ir_meta.get_variable_mut(var_id);
        var.name = Name::new_exact(name);
        var.built_in = None;
        debug_assert!(var.decorations.decorations.is_empty());
        var.decorations.decorations.push(Decoration::Uniform);
    }
}

fn add_base_vertex_to_vertex_id(ir: &mut IR) {
    if let Some(vertex_id) = ir.meta.get_built_in_variable(BuiltIn::VertexID) {
        let replaced_vertex_id = TypedId::from_variable_id(&ir.meta, vertex_id);
        let vertex_id = ir.meta.declare_cached_global_for_variable(vertex_id, "VertexID").1;

        let base_vertex = ir.meta.get_or_declare_built_in_variable(BuiltIn::BaseVertex).1;

        // Initialize VertexID as such:
        //
        //     VertexID = gl_VertexID + angle_BaseVertex;
        //
        // Which translates to:
        //
        //     vertex_id  = Load vertex_id_built_in
        //     offset     = Load base_vertex
        //     adjusted   = Add vertex_id offset
        //                  Store vertex_id adjusted

        let mut preamble = Block::new();
        let id = preamble.add_typed_instruction(instruction::load(&mut ir.meta, vertex_id));
        let base = preamble.add_typed_instruction(instruction::load(&mut ir.meta, base_vertex));
        let adjusted = preamble.add_typed_instruction(instruction::add(&mut ir.meta, id, base));
        preamble.add_void_instruction(OpCode::Store(replaced_vertex_id, adjusted));

        ir.prepend_to_main(preamble);
    }
}
