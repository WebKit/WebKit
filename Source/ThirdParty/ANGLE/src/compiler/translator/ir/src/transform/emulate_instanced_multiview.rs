// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// When an instanced draw call is issued involving multiview, the instances are divided up between
// the views.  If there are N views, the first N instances should receive gl_InstanceID of 0, with
// each view (indexed by gl_ViewID_OVR in [0, N-1]) receiving one instance.  The next N instances
// are similarly divided between views, and receive gl_InstanceID of 1.
//
// When instanced multiview is emulated, the input gl_InstanceID (unaware of multiview) is used as
// such:
//
//     InstanceID = gl_InstanceID / N;
//     ViewID_OVR = gl_InstanceID % N;
//
// The rest of the shader would use `InstanceID` and `ViewID_OVR` instead of gl_InstanceID and
// gl_ViewID_OVR.
//
// If required, a new uniform `multiviewBaseViewLayerIndex` is created and added to `ViewID_OVR`.
use crate::ir::*;
use crate::*;

pub struct Options {
    pub shader_type: ShaderType,
    pub select_viewport_layer: bool,
}

struct State<'a> {
    ir_meta: &'a mut IRMeta,
}

pub fn run(ir: &mut IR, options: &Options) {
    let mut state = State { ir_meta: &mut ir.meta };

    let view_id = replace_view_id(&mut state, options);
    if options.shader_type == ShaderType::Vertex {
        let (instance_id_built_in, instance_id) = replace_instance_id(&mut state);

        let preamble =
            generate_preamble(&mut state, options, instance_id_built_in, instance_id, view_id);
        ir.prepend_to_main(preamble);
    }
}

fn replace_view_id(state: &mut State, options: &Options) -> TypedId {
    let view_id_built_in = state.ir_meta.get_built_in_variable(BuiltIn::ViewIDOVR);
    let view_id_name = Name::new_exact("ViewID_OVR");

    let decoration = if options.shader_type == ShaderType::Fragment {
        Decoration::EmulatedViewIDIn
    } else {
        Decoration::EmulatedViewIDOut
    };

    // If the built-in is already used by the shader, replace it by the new variable.  Otherwise
    // create a new one.
    if let Some(view_id_built_in) = view_id_built_in {
        let view_id_variable = state.ir_meta.get_variable_mut(view_id_built_in);
        view_id_variable.name = view_id_name;
        view_id_variable.built_in = None;
        debug_assert!(view_id_variable.decorations.decorations.is_empty());
        view_id_variable.decorations.decorations.push(decoration);
        TypedId::from_variable_id(state.ir_meta, view_id_built_in)
    } else {
        state
            .ir_meta
            .declare_variable(
                view_id_name,
                TYPE_ID_UINT,
                Precision::High,
                Decorations::new(vec![decoration]),
                None,
                None,
                VariableScope::Global,
            )
            .1
    }
}

fn replace_instance_id(state: &mut State) -> (TypedId, TypedId) {
    let (replaced_instance_id, replaced_instance_id_typed) =
        state.ir_meta.get_or_declare_built_in_variable(BuiltIn::InstanceID);

    // Make a duplicate of gl_InstanceID to add to globals, and let the shader use the replacement.
    (
        state.ir_meta.declare_cached_global_for_variable(replaced_instance_id, "InstanceID").1,
        replaced_instance_id_typed,
    )
}

fn generate_preamble(
    state: &mut State,
    options: &Options,
    instance_id_built_in: TypedId,
    instance_id: TypedId,
    view_id: TypedId,
) -> Block {
    let mut preamble = Block::new();
    // Note: if multiview is enabled via #extension all, num_views may not be set.
    let num_views = state.ir_meta.get_constant_uint_typed(state.ir_meta.get_num_views().max(1));

    // Initialize InstanceID and ViewID_OVR as such:
    //
    //     InstanceID = int(uint(gl_InstanceID) / num_views);
    //     ViewID_OVR = uint(gl_InstanceID) % num_views;
    //
    // Which translates to:
    //
    //     flat_instance  = Load instance_id_built_in
    //     flat_instance' = ConstructScalarFromScalar flat_instance  // cast to uint
    //     instance       = Div flat_instance' num_views
    //     view           = IMod flat_instance' num_views
    //     instance'      = ConstructScalarFromScalar instance       // cast to int
    //                      Store instance_id instance'
    //                      Store view_id view
    let flat_instance =
        preamble.add_typed_instruction(instruction::load(state.ir_meta, instance_id_built_in));
    let flat_instance = preamble.add_typed_instruction(instruction::construct(
        state.ir_meta,
        TYPE_ID_UINT,
        vec![flat_instance],
    ));
    let instance =
        preamble.add_typed_instruction(instruction::div(state.ir_meta, flat_instance, num_views));
    let view =
        preamble.add_typed_instruction(instruction::imod(state.ir_meta, flat_instance, num_views));
    let instance = preamble.add_typed_instruction(instruction::construct(
        state.ir_meta,
        TYPE_ID_INT,
        vec![instance],
    ));

    preamble.add_void_instruction(OpCode::Store(instance_id, instance));
    preamble.add_void_instruction(OpCode::Store(view_id, view));

    // If needed, set gl_Layer as well
    if options.select_viewport_layer {
        // Create a new uniform variable for the backend to set.
        let base_layer_index = state
            .ir_meta
            .declare_variable(
                Name::new_exact("multiviewBaseViewLayerIndex"),
                TYPE_ID_INT,
                Precision::High,
                Decorations::new(vec![Decoration::Uniform]),
                None,
                None,
                VariableScope::Global,
            )
            .1;

        let layer_built_in = state.ir_meta.get_or_declare_built_in_variable(BuiltIn::LayerOut).1;

        // Set gl_Layer as such:
        //
        //     gl_Layer = int(viewId) + multiviewBaseViewLayerIndex;
        //
        // Which translates to:
        //
        //     view' = ConstructScalarFromScalar view     // cast to int
        //     base  = Load multiviewBaseViewLayerIndex
        //     layer = Add view' base
        //             Store gl_layer layer
        let view = preamble.add_typed_instruction(instruction::construct(
            state.ir_meta,
            TYPE_ID_INT,
            vec![view],
        ));
        let base =
            preamble.add_typed_instruction(instruction::load(state.ir_meta, base_layer_index));
        let layer = preamble.add_typed_instruction(instruction::add(state.ir_meta, view, base));

        preamble.add_void_instruction(OpCode::Store(layer_built_in, layer));
    }

    preamble
}
