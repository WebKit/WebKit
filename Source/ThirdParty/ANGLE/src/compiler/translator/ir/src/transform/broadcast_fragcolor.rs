// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// When EXT_draw_buffers is enabled in an ESSL 100 fragment shader, the expected behavior is that
// gl_FragColor and gl_SecondaryFragColorEXT broadcast their value to all attachments.  To
// implement this, these variables are replaced by globals which are replicated in each element of
// gl_FragData and gl_SecondaryFragDataEXT arrays respectively.
use crate::ir::*;
use crate::*;

pub struct Options {
    pub max_draw_buffers: u32,
    pub max_dual_source_draw_buffers: u32,
}

pub fn run(ir: &mut IR, options: &Options) {
    let uses_secondary = broadcast(
        ir,
        BuiltIn::SecondaryFragColorEXT,
        "angle_SecondaryFragColor",
        BuiltIn::SecondaryFragDataEXT,
        options.max_dual_source_draw_buffers,
    );
    broadcast(
        ir,
        BuiltIn::FragColor,
        "angle_FragColor",
        BuiltIn::FragData,
        if uses_secondary {
            options.max_dual_source_draw_buffers
        } else {
            options.max_draw_buffers
        },
    );
}

fn broadcast(
    ir: &mut IR,
    built_in: BuiltIn,
    name: &'static str,
    broadcast_to: BuiltIn,
    array_size: u32,
) -> bool {
    let original_built_in = ir.meta.get_built_in_variable(built_in);
    let original_built_in_exists = original_built_in.is_some();
    if let Some(original_id) = original_built_in {
        // Replace the built-in with a global variable
        let replacement = ir.meta.get_variable_mut(original_id);
        replacement.name = Name::new_temp(name);
        replacement.built_in = None;
        debug_assert!(replacement.decorations.decorations.is_empty());
        debug_assert!(replacement.scope == VariableScope::Global);
        debug_assert!(!replacement.is_const);

        let type_id = replacement.type_id;
        let precision = replacement.precision;

        // Declare a new variable with the new BuiltIn.  gl_FragColor is a `mediump vec4` and
        // gl_FragData is an array of that.  gl_SecondaryFragColorEXT and gl_SecondaryFragDataEXT
        // are similarly typed.  So it's always valid to take the type and precision of the
        // original built-in and use it to declare the array'ed built-in.
        let type_id = ir.meta.get_array_type_id(type_id, array_size);
        let (arrayed_built_in_id, arrayed_built_in) =
            ir.meta.declare_built_in_variable(type_id, precision, broadcast_to);
        ir.meta.get_variable_mut(arrayed_built_in_id).is_static_use = true;

        // Since the variable ID is unchanged, the IR does not need further modification.  The
        // transformation only needs to replicate the value of the global variable in each element
        // of the built-in array:
        //
        //     gl_*FragData[0] = angle_*FragColor;
        //     gl_*FragData[1] = angle_*FragColor;
        //     ...
        //
        // Which translates to:
        //
        //     color  = Load replacement
        //     index0 = AccessArrayElement arrayed_built_in 0
        //              Store index0 color
        //     index1 = AccessArrayElement arrayed_built_in 1
        //              Store index1 color
        //     ...
        //
        //
        let mut postamble = Block::new();

        let replacement = TypedId::from_variable_id(&ir.meta, original_id);
        let color = postamble.add_typed_instruction(instruction::load(&mut ir.meta, replacement));
        for index in 0..array_size {
            let index = ir.meta.get_constant_uint_typed(index);
            let indexed = postamble.add_typed_instruction(instruction::index(
                &mut ir.meta,
                arrayed_built_in,
                index,
            ));
            postamble.add_void_instruction(OpCode::Store(indexed, color));
        }

        ir.append_to_main(postamble);
    }

    original_built_in_exists
}
