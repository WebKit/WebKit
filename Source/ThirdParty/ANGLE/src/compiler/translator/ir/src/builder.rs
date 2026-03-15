// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The IR builder connects to ParseContext.cpp, turning the parse output to an IR.  It is not meant
// to be used by transformations, as they can generate IR more efficiently.

use crate::ir::*;
use crate::*;

// Used when building the IR from GLSL, holds an intermediate block to be filled with instructions.
#[cfg_attr(debug_assertions, derive(Debug))]
struct IntermediateBlock {
    block: Block,
    is_merge_block: bool,
    // Once the block is terminated with return, break, continue or discard, any new instructions
    // that are encountered are dead code.  Those instructions are processed as usual, but don't
    // result in any changes to the block.
    new_instructions_are_dead_code: bool,
}

impl IntermediateBlock {
    fn new() -> IntermediateBlock {
        IntermediateBlock {
            block: Block::new(),
            is_merge_block: false,
            new_instructions_are_dead_code: false,
        }
    }

    // Used with debug_assert!, ensures that the block does not have any leftovers.
    fn is_reset(&self) -> bool {
        self.block.variables.is_empty()
            && self.block.instructions.is_empty()
            && self.block.merge_block.is_none()
            && self.block.block1.is_none()
            && self.block.block2.is_none()
            && self.block.case_blocks.is_empty()
            && self.block.input.is_none()
            && !self.is_merge_block
            && !self.new_instructions_are_dead_code
    }

    fn reset(&mut self) {
        self.block.input = None;
        self.is_merge_block = false;
        self.new_instructions_are_dead_code = false;

        debug_assert!(self.is_reset());
    }

    fn split_merge_block(&mut self) -> Option<IntermediateBlock> {
        if let Some(merge_block) = self.block.merge_block.take() {
            Some(IntermediateBlock {
                block: *merge_block,
                is_merge_block: true,
                new_instructions_are_dead_code: self.new_instructions_are_dead_code,
            })
        } else {
            None
        }
    }
}

// Helper to construct the CFG of a function.
#[cfg_attr(debug_assertions, derive(Debug))]
struct CFGBuilder {
    // The current block that is being built.  Once the block is complete it will be pushed to the
    // `interm_blocks` stack if necessary
    //
    // Note that `current_block` is logically the top of `interm_block` stack, but is created
    // separately to avoid lots of `panic!`s as otherwise Rust doesn't know there is always at
    // least one element on the stack.
    current_block: IntermediateBlock,
    // The blocks of the CFG that is being built.  Once a function is complete, this stack
    // should be empty.
    interm_blocks: Vec<IntermediateBlock>,
}

impl CFGBuilder {
    fn new() -> CFGBuilder {
        CFGBuilder { current_block: IntermediateBlock::new(), interm_blocks: Vec::new() }
    }

    fn clear(&mut self) {
        let _ = std::mem::replace(&mut self.current_block, IntermediateBlock::new());
        self.interm_blocks.clear();
    }

    fn is_empty(&self) -> bool {
        self.current_block.is_reset() && self.interm_blocks.is_empty()
    }

    // When a new GLSL block is encountered that affects the control flow (body of a function, true
    // or false block of an if, etc), `current_block` starts off as a non-merge block.
    //
    // If the GLSL block finishes without divergence, `current_block` remains as non-merged.  If
    // the GLSL block diverges, `current_block` is pushed onto stack and a new non-merge
    // `current_block` is created.  Once merged (for example end of `if`) a new merge
    // `current_block` is created.  This may happen multiple times, resulting in a set of nodes on
    // the stack and one node as `current_block`.
    //
    // Note again that `current_block` is logically the top element of the stack.  Once the GLSL
    // block finishes, the stack (including `current_block`) is traversed until a non-merge block
    // is found; the blocks that are popped in the process are chained via their `merge_block`.
    // The non-merge block itself is then placed where appropriate, (such as Function::entry,
    // Block::block1 of top of stack, etc)
    //
    // For `for` and `while` loops, before the condition is processed, `current_block` is pushed to
    // the stack and a new non-merge block is created (because it needs to be executed multiple
    // times), which will contain the condition only.  For `for` loops if there is a continue
    // expression, similarly `current_block` is pushed to the stack and a new non-merge block is
    // created.  Once the loop is finished, not only is the body popped but also the continue and
    // condition blocks.
    //
    // For `do-while` loops, the only difference is that the condition block is placed on top of
    // the body block instead of under.

    fn take_block(block: &mut IntermediateBlock) -> IntermediateBlock {
        std::mem::replace(block, IntermediateBlock::new())
    }

    // Push a new non-merge block on the stack
    fn push_block(&mut self) {
        debug_assert!(self.current_block.block.is_terminated());
        self.interm_blocks.push(Self::take_block(&mut self.current_block));
        self.current_block.reset();
    }

    // Extract a chain of merge blocks until a non-merge block is reached.  The blocks are linked
    // together by their `merge_block`.
    fn pop_block(&mut self) -> Block {
        debug_assert!(self.current_block.block.is_terminated());
        let mut result = Self::take_block(&mut self.current_block);

        while result.is_merge_block {
            let mut parent_block = self.interm_blocks.pop().unwrap();
            parent_block.block.set_merge_block(result.block);
            result = parent_block;
        }

        result.block
    }

    // Restore a block as the current block, possible during constant folding.  The builder assumes
    // intermediate blocks are not yet merged into chains (if needed), but pop_block would do just
    // that.  When restoring the block as the current block, undo that chaining of blocks to keep
    // the builder logic simpler.
    fn restore_block_as_current(&mut self, mut block: IntermediateBlock) {
        debug_assert!(self.current_block.is_reset());

        while let Some(merge_block) = block.split_merge_block() {
            self.interm_blocks.push(block);
            block = merge_block;
        }
        self.current_block = block;
    }

    fn are_new_instructions_dead_code_in_current_block(&self) -> bool {
        self.current_block.new_instructions_are_dead_code
    }

    fn are_new_instructions_dead_code_in_parent_block(&self) -> bool {
        self.interm_blocks.last().unwrap().new_instructions_are_dead_code
    }

    fn add_variable_declaration(&mut self, variable_id: VariableId) {
        if !self.current_block.new_instructions_are_dead_code {
            self.current_block.block.add_variable_declaration(variable_id);
        }
    }

    // Add an instruction to the current block.  This may be a break, continue, return or discard,
    // in which case the block terminates early.  If another instruction is added after the block
    // is terminated, it is dropped as dead-code.
    fn add_void_instruction(&mut self, op: OpCode) {
        if !self.current_block.new_instructions_are_dead_code {
            if matches!(op, OpCode::Discard | OpCode::Return(_) | OpCode::Break | OpCode::Continue)
            {
                self.current_block.new_instructions_are_dead_code = true;
            }

            self.current_block.block.add_void_instruction(op);
        }
    }
    fn add_typed_instruction(&mut self, id: RegisterId) {
        if !self.current_block.new_instructions_are_dead_code {
            self.current_block.block.add_register_instruction(id);
        }
    }

    fn begin_if_true_block(&mut self, condition: TypedId) {
        // The current block should terminate with a jump based on the condition.  It is pushed to
        // the stack.
        if !self.current_block.new_instructions_are_dead_code {
            self.current_block.block.terminate(OpCode::If(condition));
        }
        self.push_block();
    }

    fn end_if_body_block_common<SetBlock>(
        &mut self,
        merge_param: Option<TypedId>,
        set_block: &SetBlock,
    ) where
        SetBlock: Fn(&mut Block, Block),
    {
        if !self.current_block.block.is_terminated() {
            self.current_block.block.terminate(OpCode::Merge(merge_param));
        } else {
            debug_assert!(!matches!(
                self.current_block.block.get_terminating_op(),
                OpCode::NextBlock
            ));
        }
        let true_or_false_block = self.pop_block();
        if !self.are_new_instructions_dead_code_in_parent_block() {
            set_block(&mut self.interm_blocks.last_mut().unwrap().block, true_or_false_block);
        }
    }

    fn end_if_true_block(&mut self, merge_param: Option<TypedId>) {
        // Take the block and set it as the true block of the previously pushed if header block.
        self.end_if_body_block_common(merge_param, &|if_block, true_block| {
            if_block.set_if_true_block(true_block)
        });
    }

    fn begin_if_false_block(&mut self) {
        // The true block of the block is already processed, so there is nothing in the
        // `current_block`, and it's ready to record the instructions of the false block already.
        debug_assert!(self.current_block.is_reset());
    }

    fn end_if_false_block(&mut self, merge_param: Option<TypedId>) {
        // Take the block and set it as the false block of the previously pushed if header block.
        self.end_if_body_block_common(merge_param, &|if_block, false_block| {
            if_block.set_if_false_block(false_block)
        });
    }

    // Return whether an already terminated if block is empty with no operations; that is it has a
    // single instruction and it is a Merge instruction with no parameter.  If there are any
    // variable declarations but no instructions, the variable declarations can safely be dropped.
    //
    // Note: we could potentially follow the chain of NextBlocks to such a block if all the
    // intermediate blocks are also empty, if such a chain is ever generated.
    fn is_no_op_merge_block(block: &Option<Box<Block>>) -> bool {
        block
            .as_ref()
            .map(|block| {
                block.instructions.len() == 1
                    && matches!(block.get_terminating_op(), OpCode::Merge(None))
            })
            .unwrap_or(true)
    }

    fn end_if(&mut self, input: Option<TypedId>) -> Option<TypedId> {
        // The true and else blocks have been processed.  The header of the if is already at the
        // top of the stack.  Prepare a merge block to continue what comes after the if.
        //
        // If the entire `if` was dead-code eliminated however, restore the previous (already
        // terminated) block
        if self.are_new_instructions_dead_code_in_parent_block() {
            self.current_block = self.interm_blocks.pop().unwrap();
            return None;
        }

        debug_assert!(self.current_block.is_reset());

        // If the condition of the if is a constant, it can be constant-folded.
        let mut if_block = self.interm_blocks.pop().unwrap();
        let if_condition = if_block.block.get_terminating_op().get_if_condition().id.get_constant();

        match if_condition {
            Some(condition) => {
                // Remove the if instruction from the block and revive it as the current block.
                if_block.block.unterminate();
                self.restore_block_as_current(if_block);

                let true_block = self.current_block.block.block1.take();
                let false_block = self.current_block.block.block2.take();

                // Append the true or false block based on condition.  At the same time,
                // replace the `input` id with the parameter of the block that is replacing the
                // if.
                self.const_fold_if_with_block(if condition == CONSTANT_ID_TRUE {
                    true_block
                } else {
                    false_block
                })
            }
            None => {
                // Not a constant, see if both the true and false blocks are empty.
                let true_block_empty = Self::is_no_op_merge_block(&if_block.block.block1);
                let false_block_empty = Self::is_no_op_merge_block(&if_block.block.block2);

                if true_block_empty && false_block_empty {
                    // If both blocks are empty, eliminate the if entirely.
                    if_block.block.unterminate();
                    self.restore_block_as_current(if_block);
                    self.current_block.block.block1.take();
                    self.current_block.block.block2.take();
                    None
                } else {
                    // Need to keep the if block as-is, push it back and make the current block a
                    // merge block.
                    self.interm_blocks.push(if_block);
                    self.current_block.is_merge_block = true;
                    self.current_block.block.input = input.map(|id| id.as_register_id());
                    None
                }
            }
        }
    }

    fn const_fold_if_with_block(&mut self, block: Option<Box<Block>>) -> Option<TypedId> {
        match block {
            Some(block) => {
                // Make the current block jump to this block.
                self.current_block.block.terminate(OpCode::NextBlock);
                self.push_block();

                self.restore_block_as_current(IntermediateBlock {
                    block: *block,
                    is_merge_block: true,
                    new_instructions_are_dead_code: self
                        .are_new_instructions_dead_code_in_parent_block(),
                });

                // If there was a merge parameter, replace the merge input with this id directly.
                let last_block = self.current_block.block.get_merge_chain_last_block_mut();
                let terminating_op = last_block.get_terminating_op();
                if matches!(terminating_op, OpCode::Merge(..)) {
                    let merge_param = terminating_op.get_merge_parameter();
                    last_block.unterminate();
                    merge_param
                } else {
                    // If the terminating op is not a merge, the rest of this block is going to be
                    // dead code.
                    self.current_block.new_instructions_are_dead_code = true;
                    None
                }
            }
            None => {
                // The if should be entirely eliminated, as there is nothing to replace it with.
                // For example, `if (false) { only_true_block(); }`.
                //
                // The if header (code leading to the if) is already set as `current_block` and
                // unterminated, so there is nothing to do.
                None
            }
        }
    }

    fn begin_loop_condition(&mut self) {
        // Loops have a condition to evaluate every time, so a new block is created for that.
        if !self.current_block.new_instructions_are_dead_code {
            self.current_block.block.terminate(OpCode::Loop);
        }
        self.push_block();
    }

    fn end_loop_condition_common(&mut self, condition: TypedId) {
        // There cannot be control flow in the condition expression, so this block must always be
        // alive.
        debug_assert!(!self.current_block.new_instructions_are_dead_code);
        self.current_block.block.terminate(OpCode::LoopIf(condition));
        let mut condition_block = self.pop_block();

        if !self.are_new_instructions_dead_code_in_parent_block() {
            // GLSL supports such a construct:
            //
            //     while (bool var = ...) { use(var); }
            //
            // To support this, pull the variable declaration to the parent scope so it's not
            // declared only in the condition block.
            let loop_block = &mut self.interm_blocks.last_mut().unwrap().block;
            loop_block.variables.append(&mut std::mem::take(&mut condition_block.variables));
            loop_block.set_loop_condition_block(condition_block);
        }
    }

    fn end_loop_condition(&mut self, condition: TypedId) {
        self.end_loop_condition_common(condition);

        // Body of the loop is recorded in the current (empty) block.
        debug_assert!(self.current_block.is_reset());
    }

    fn end_loop_continue(&mut self) {
        // There cannot be control flow in the continue expression, so this block must always be
        // alive.
        debug_assert!(!self.current_block.new_instructions_are_dead_code);
        self.current_block.block.terminate(OpCode::Continue);

        let continue_block = self.pop_block();

        if !self.are_new_instructions_dead_code_in_parent_block() {
            self.interm_blocks.last_mut().unwrap().block.set_loop_continue_block(continue_block);
        }
    }

    fn end_loop(&mut self) {
        if !self.current_block.new_instructions_are_dead_code {
            self.current_block.block.terminate(OpCode::Continue);
        }

        // Retrieve and set the body block of the loop.  The header of the loop is already at the
        // top of the stack.  Prepare a merge block to continue what comes after the loop.
        //
        // If the entire loop was dead-code eliminated however, restore the previous (already
        // terminated) block.
        let body_block = self.pop_block();

        if self.are_new_instructions_dead_code_in_parent_block() {
            self.current_block = self.interm_blocks.pop().unwrap();
            return;
        }

        debug_assert!(self.current_block.is_reset());

        // If the condition of the loop is a constant false, it can be constant-folded.
        let mut loop_block = self.interm_blocks.pop().unwrap();
        let loop_condition = loop_block
            .block
            .loop_condition
            .as_ref()
            .unwrap()
            .get_merge_chain_terminating_op()
            .get_loop_condition()
            .id
            .get_constant();

        match loop_condition {
            Some(condition) if condition == CONSTANT_ID_FALSE => {
                // Remove the loop instruction from the block and revive it as the current block.
                loop_block.block.unterminate();
                self.restore_block_as_current(loop_block);

                // Remove the condition block.
                self.current_block.block.loop_condition.take();

                // Remove the continue block that might have been added, if any.
                debug_assert!(self.current_block.block.block1.is_none());
                self.current_block.block.block2.take();
            }
            _ => {
                // Not a constant false, push the loop block back and make the current block a
                // merge block.
                loop_block.block.set_loop_body_block(body_block);

                self.interm_blocks.push(loop_block);
                self.current_block.is_merge_block = true;
            }
        }
    }

    fn begin_do_loop(&mut self) {
        // Insert a new block for the body of the do-loop.
        if !self.current_block.new_instructions_are_dead_code {
            self.current_block.block.terminate(OpCode::DoLoop);
        }
        self.push_block();
    }

    fn begin_do_loop_condition(&mut self) {
        // Retrieve and set the body block of the do-loop.  The header of the do-loop is already at
        // the top of the stack.
        if !self.current_block.block.is_terminated() {
            self.current_block.block.terminate(OpCode::Continue);
        }
        let body_block = self.pop_block();

        if !self.are_new_instructions_dead_code_in_parent_block() {
            self.interm_blocks.last_mut().unwrap().block.set_loop_body_block(body_block);
        }

        // Loops have a condition to evaluate every time, which is recorded in the current (empty)
        // block.
        debug_assert!(self.current_block.is_reset());
    }

    fn end_do_loop(&mut self, condition: TypedId) {
        self.end_loop_condition_common(condition);

        // Prepare a merge block to continue what comes after the do-loop.  If the entire do-loop
        // was dead-code eliminated however, restore the previous (already terminated) block.
        if self.are_new_instructions_dead_code_in_parent_block() {
            self.current_block = self.interm_blocks.pop().unwrap();
            return;
        }

        // Make the current block a merge block.
        debug_assert!(self.current_block.is_reset());
        self.current_block.is_merge_block = true;
    }

    fn begin_switch(&mut self, value: TypedId) {
        // Insert a new block for the first case.
        if !self.current_block.new_instructions_are_dead_code {
            self.current_block.block.terminate(OpCode::Switch(value, Vec::new()));
        }
        self.push_block();
    }

    fn is_empty_switch_block(block: &Block) -> bool {
        if let OpCode::Switch(_, cases) = block.get_terminating_op() {
            cases.is_empty()
        } else {
            false
        }
    }

    fn end_previous_case(&mut self) {
        if !self.are_new_instructions_dead_code_in_parent_block() {
            // If this was the very first `case` in the switch, there is no previous case to end.
            let is_first_block =
                Self::is_empty_switch_block(&self.interm_blocks.last().unwrap().block);

            if !is_first_block {
                // If the block is terminated, nothing to do, just add it to the list of switch's
                // case blocks.  If it isn't terminated, terminate it with a `Passthrough` first.
                //
                if !self.current_block.block.is_terminated() {
                    self.current_block.block.terminate(OpCode::Passthrough);
                }

                let mut case_block = self.pop_block();
                if !self.are_new_instructions_dead_code_in_parent_block() {
                    // GLSL supports declaring a variable in one case and using it in another:
                    //
                    //     switch (..) {
                    //     case 0:
                    //         int i = 0;
                    //     default:
                    //         output = i;
                    //     }
                    //
                    // To support this, pull the variable declaration to the parent scope so it's
                    // not declared only in one case block.
                    let switch_block = &mut self.interm_blocks.last_mut().unwrap().block;
                    switch_block.variables.append(&mut std::mem::take(&mut case_block.variables));
                    switch_block.add_switch_case_block(case_block);
                }
            }
        }
    }

    fn begin_case_impl(&mut self, value: Option<ConstantId>) {
        // Add the value to the switch's case blocks.
        if !self.are_new_instructions_dead_code_in_parent_block() {
            // But first, terminate the previous case, if any and not already.
            self.end_previous_case();

            self.interm_blocks
                .last_mut()
                .unwrap()
                .block
                .get_terminating_op_mut()
                .add_switch_case(value);
        }
    }

    fn begin_case(&mut self, value: TypedId) {
        self.begin_case_impl(value.id.get_constant());
    }

    fn begin_default(&mut self) {
        self.begin_case_impl(None);
    }

    // Check if the switch in the block at the top the stack has a matching case (e.g. if it uses a
    // constant expression) and what the matching block is (if any).
    fn switch_has_matching_case(switch_block: &mut Block) -> (bool, Option<usize>) {
        let (switch_expr, switch_cases) =
            switch_block.get_terminating_op().get_switch_expression_and_cases();

        // If there are no cases, the switch is a no-op
        if switch_cases.is_empty() {
            return (false, None);
        }

        // If the expression is constant, but no matching cases (or default) exists, the switch is
        // a no-op.
        if let Some(switch_expr) = switch_expr.id.get_constant() {
            // First check if there's an exact match
            let any_exact = switch_cases
                .iter()
                .position(|case| case.map(|case_id| case_id == switch_expr).unwrap_or(false));
            if any_exact.is_some() {
                return (true, any_exact);
            }
            // Otherwise, check if there's a `default`:
            let any_default = switch_cases.iter().position(|case| case.is_none());
            return (any_default.is_some(), any_default);
        }

        // Expression is not a constant, assume the switch is not no-op.
        (true, None)
    }

    fn end_switch(&mut self) {
        // Terminate the last case, if not already.
        self.end_previous_case();

        // The case blocks have already been processed.  The header of the switch is already at the
        // top of the stack.  Prepare a merge block to continue what comes after the switch.
        //
        // If the entire `switch` was dead-code eliminated however, restore the previous (already
        // terminated) block
        if self.are_new_instructions_dead_code_in_parent_block() {
            self.current_block = self.interm_blocks.pop().unwrap();
            return;
        }

        debug_assert!(self.current_block.is_reset());

        let mut switch_block = self.interm_blocks.pop().unwrap();

        // If the last block didn't explicitly `break`, it is terminated with `Passthrough`.  Fix
        // that up to become `Break`.
        if let Some(last_case_block) = switch_block.block.case_blocks.last_mut() {
            let last_case_block = last_case_block.get_merge_chain_last_block_mut();
            if matches!(last_case_block.get_terminating_op(), OpCode::Passthrough) {
                last_case_block.unterminate();
                last_case_block.terminate(OpCode::Break);
            }
        }

        // There are a couple of possibilities to constant fold the switch:
        //
        // - The switch has no cases: In this case, it can simply be removed.
        // - The switch has a constant expression, but none of the cases match it: In this case, the
        //   switch can be removed as well.
        // - The switch has a constant expression with a matching case: In this case, the
        //   non-matching cases (that also cannot be fallen-through) can be removed.  Note that
        //   sometimes the entire switch can be replaced with the matching blocks, but it can get
        //   tricky with break and continue in it.

        let (has_matching_case, matching_block_index) =
            Self::switch_has_matching_case(&mut switch_block.block);
        if !has_matching_case {
            // Remove the switch instruction from the block and revive it as the current block.
            switch_block.block.case_blocks.clear();
            switch_block.block.unterminate();
            self.restore_block_as_current(switch_block);
        } else {
            // If the switch expression is constant, remove the case blocks that cannot be executed
            if let Some(matching_block_index) = matching_block_index {
                let case_blocks = std::mem::take(&mut switch_block.block.case_blocks);

                // Starting from the matching block, take blocks while their terminator is
                // `Passthrough` and then the final block with a different terminator.  Note that
                // take_while stops _after_ the first element that doesn't match the predicate,
                // i.e. that element is lost.  Because of that, the first element after
                // `Passthrough` is also matched.
                let mut iter = case_blocks.into_iter().skip(matching_block_index);
                let mut matching_case_block = iter.next().unwrap();
                let mut fallthrough_chain_last_block =
                    matching_case_block.get_merge_chain_last_block_mut();

                for fallthrough_block in iter {
                    // If the previous block was fallthrough, chain this block to it. Otherwise
                    // this block and all after it are unreachable.
                    if !matches!(
                        fallthrough_chain_last_block.get_terminating_op(),
                        &OpCode::Passthrough
                    ) {
                        break;
                    }

                    fallthrough_chain_last_block.unterminate();
                    fallthrough_chain_last_block.terminate(OpCode::NextBlock);
                    fallthrough_chain_last_block.set_merge_block(fallthrough_block);
                    fallthrough_chain_last_block =
                        fallthrough_chain_last_block.get_merge_chain_last_block_mut();
                }

                let (switch_expression, switch_cases) =
                    switch_block.block.get_terminating_op().get_switch_expression_and_cases();
                let matching_case = switch_cases[matching_block_index];
                // Replace the switch with one that has only one target, that is the matching case.
                switch_block.block.unterminate();
                switch_block
                    .block
                    .terminate(OpCode::Switch(switch_expression, vec![matching_case]));
                switch_block.block.case_blocks = vec![matching_case_block];
            }

            // The switch has to remain (whether constant or not), push it back and make the
            // current block a merge block.
            self.interm_blocks.push(switch_block);
            self.current_block.is_merge_block = true;
        }
    }
}

// A helper to build the IR from scratch.  The helper is invoked while parsing the shader, and
// its main purpose is to maintain in-progress items until completed, and adapt the incoming GLSL
// syntax to the IR.
pub struct Builder {
    // The IR being built
    ir: IR,

    // Flags controlling the IR generation
    options: Options,

    // The current function that is being built (if any).
    current_function: Option<FunctionId>,

    // The CFG of the current function being built, if any.
    current_function_cfg: CFGBuilder,

    // The CFG of the initialization code of global variables.
    //
    // Global variables can either be initialized with constants or non-constant expressions.  If
    // initialized with a constant, the Variable::initializer can be used, but that's impossible
    // with non-constant initializers; there may be complex code (with multiple instructions) to
    // execute before that can happen.  To support that, when code is encountered in the global
    // scope, it is generated in `global_initializers_cfg` and when it's assigned to a variable,
    // a Store op is added to the top block of the cfg.
    //
    // At the end of parsing, this is prepended to the CFG of main().
    global_initializers_cfg: CFGBuilder,

    // Stack of intermediate values.  As the parser observes expressions, it pushes them on the
    // stack.  When these intermediate expressions are combined in an operation, the corresponding
    // operation is popped off the stack.  The exact number of ids to pop depends on the
    // instruction.
    //
    // This stack should be empty after every statement in a GLSL block is completely processed.
    // It should also be empty at the end of every Block.
    interm_ids: Vec<TypedId>,

    // gl_ClipDistance and gl_CullDistance may be sized after `.length()` is called on them.  To
    // support this, a global variable is created to hold that yet-to-be-determined constant.  When
    // the size of gl_ClipDistance and gl_CullDistance is determined, that variable is initialized
    // with it.
    gl_clip_distance_length_var_id: Option<VariableId>,
    gl_cull_distance_length_var_id: Option<VariableId>,
}

impl Builder {
    pub fn new(shader_type: ShaderType, options: Options) -> Builder {
        Builder {
            ir: IR::new(shader_type),
            options,
            current_function: None,
            current_function_cfg: CFGBuilder::new(),
            global_initializers_cfg: CFGBuilder::new(),
            interm_ids: Vec::new(),
            gl_clip_distance_length_var_id: None,
            gl_cull_distance_length_var_id: None,
        }
    }

    // Called at the end of the shader to finalize it.  Notably, this moves the cached global
    // initialization code to the beginning of `main()`.
    pub fn finish(&mut self) {
        debug_assert!(self.ir.meta.get_main_function_id().is_some());

        if !self.global_initializers_cfg.is_empty() {
            self.global_initializers_cfg.current_block.block.terminate(OpCode::NextBlock);
            let global_init_block = self.global_initializers_cfg.pop_block();
            self.ir.prepend_to_main(global_init_block);
        }

        // Instantly dead-code-eliminate unreachable functions.
        let function_decl_order =
            util::calculate_function_decl_order(&self.ir.meta, &self.ir.function_entries);
        let function_count = self.ir.function_entries.len();
        // Take all entry blocks out ...
        let mut function_entries = std::mem::take(&mut self.ir.function_entries);
        self.ir.function_entries.resize_with(function_count, || None);
        // ... and only place back the ones that are reachable from `main` (i.e. are in the DAG).
        for function_id in function_decl_order {
            let id = function_id.id as usize;
            self.ir.function_entries[id] = function_entries[id].take();
        }

        // `main()` is always reachable from `main()`!
        debug_assert!(
            self.ir.function_entries[self.ir.meta.get_main_function_id().unwrap().id as usize]
                .is_some()
        );
    }

    // Called at the end of the shader after it has failed validation.  At this point, the IR is no
    // longer going to be used and is likely in a half-baked state, discard everything.
    pub fn fail(&mut self) {
        self.current_function_cfg.clear();
        self.global_initializers_cfg.clear();
        self.interm_ids.clear();
        let shader_type = self.ir.meta.get_shader_type();
        let _ = std::mem::replace(&mut self.ir, IR::new(shader_type));
    }

    // Get a reference to the IR for direct access, such as for doing type look up etc, avoiding
    // the need for a set of forwarding functions.
    pub fn ir(&mut self) -> &mut IR {
        &mut self.ir
    }
    pub fn take_ir(&mut self) -> IR {
        std::mem::replace(&mut self.ir, IR::new(ShaderType::Vertex))
    }

    fn variable_is_private_and_can_be_initialized(
        &self,
        type_id: TypeId,
        decorations: &Decorations,
        built_in: Option<BuiltIn>,
    ) -> bool {
        let type_info = self.ir.meta.get_type(type_id);
        debug_assert!(!type_info.is_pointer());

        // Some variables cannot be initialized, like uniforms, inputs, etc.
        !(type_info.is_image()
            || type_info.is_unsized_array()
            || built_in.is_some()
            || decorations.has(Decoration::Input)
            || decorations.has(Decoration::Output)
            || decorations.has(Decoration::InputOutput)
            || decorations.has(Decoration::Uniform)
            || decorations.has(Decoration::Buffer)
            || decorations.has(Decoration::Shared))
    }

    fn built_in_is_output(&self, built_in: BuiltIn) -> bool {
        // gl_PrimitiveID is an output in geometry shaders, but input in tessellation and fragment
        // shaders.  gl_Layer is an output in geometry shaders, but input in vertex shaders (for
        // multiview).
        //
        // gl_ClipDistance and gl_CullDistance are inputs in fragment shader, output otherwise.
        //
        // gl_TessLevelOuter and gl_TessLevelInner are outputs in tessellation control shaders, but
        // input in tessellation evaluation shaders.
        match built_in {
            BuiltIn::FragColor
            | BuiltIn::FragData
            | BuiltIn::FragDepth
            | BuiltIn::SecondaryFragColorEXT
            | BuiltIn::SecondaryFragDataEXT
            | BuiltIn::SampleMask
            | BuiltIn::Position
            | BuiltIn::PointSize
            | BuiltIn::PrimitiveShadingRateEXT
            | BuiltIn::BoundingBoxOES
            | BuiltIn::PerVertexOut => true,
            BuiltIn::PrimitiveID | BuiltIn::LayerOut => {
                self.ir.meta.get_shader_type() == ShaderType::Geometry
            }
            BuiltIn::ClipDistance | BuiltIn::CullDistance => {
                self.ir.meta.get_shader_type() != ShaderType::Fragment
            }
            BuiltIn::TessLevelOuter | BuiltIn::TessLevelInner => {
                self.ir.meta.get_shader_type() == ShaderType::TessellationControl
            }
            _ => false,
        }
    }

    fn variable_is_output(&self, decorations: &Decorations, built_in: Option<BuiltIn>) -> bool {
        decorations.has(Decoration::Output)
            || built_in.is_some_and(|built_in| self.built_in_is_output(built_in))
    }

    // Internal helper to declare a new variable.
    fn declare_variable(
        &mut self,
        name: Name,
        type_id: TypeId,
        precision: Precision,
        decorations: Decorations,
        built_in: Option<BuiltIn>,
        scope: VariableScope,
    ) -> VariableId {
        // The declared variable may be uninitialized.  If the build flags indicate the need,
        // the variable is marked such that it is zero-initialized before output generation.  Note
        // that function parameters are handled in declare_function_param.
        let needs_zero_initialization = scope != VariableScope::FunctionParam
            && ((self.options.initialize_uninitialized_variables
                && self.variable_is_private_and_can_be_initialized(
                    type_id,
                    &decorations,
                    built_in,
                ))
                || (self.options.initialize_output_variables
                    && self.variable_is_output(&decorations, built_in))
                || (self.options.initialize_gl_position
                    && matches!(built_in, Some(BuiltIn::Position))));

        let variable_id = self.ir.meta.declare_variable(
            name,
            type_id,
            precision,
            decorations,
            built_in,
            None,
            scope,
        );

        // Add the variable to the list of local variables in this scope, if not global.  Function
        // parameters are part of the `Function` and so don't need to be explicitly marked as
        // needing declaration.
        if scope == VariableScope::Local {
            self.current_function_cfg.add_variable_declaration(variable_id);
        }

        if needs_zero_initialization {
            self.ir.meta.require_variable_zero_initialization(variable_id);
        }

        variable_id
    }

    // Declare a built-in variable.  Must have never been declared before.
    pub fn declare_built_in_variable(
        &mut self,
        built_in: BuiltIn,
        type_id: TypeId,
        precision: Precision,
        decorations: Decorations,
    ) -> VariableId {
        // Note: the name of the built-in is not derived.  For text-based generators, the name can
        // be derived from the built-in enum, and the string is unnecessary.  For SPIR-V, the name
        // is unused.
        //
        // Built-ins are always globally declared.
        self.declare_variable(
            Name::new_exact(""),
            type_id,
            precision,
            decorations,
            Some(built_in),
            VariableScope::Global,
        )
    }

    // Declare an interface variable.  Interface variables are special in that their name must be
    // preserved in a predictable form for text-based generators.
    pub fn declare_interface_variable(
        &mut self,
        name: &'static str,
        type_id: TypeId,
        precision: Precision,
        decorations: Decorations,
    ) -> VariableId {
        self.declare_variable(
            Name::new_interface(name),
            type_id,
            precision,
            decorations,
            None,
            VariableScope::Global,
        )
    }

    // Declare a temporary variable.  The name of this variable is kept as closely to the original
    // as possible, but without any guarantees.
    pub fn declare_temp_variable(
        &mut self,
        name: &'static str,
        type_id: TypeId,
        precision: Precision,
        decorations: Decorations,
    ) -> VariableId {
        let scope = if self.current_function.is_none() {
            VariableScope::Global
        } else {
            VariableScope::Local
        };

        self.declare_variable(Name::new_temp(name), type_id, precision, decorations, None, scope)
    }

    // Declare a const temporary variable.  The name of this variable is ultimately unused.
    pub fn declare_const_variable(&mut self, type_id: TypeId, precision: Precision) -> VariableId {
        self.ir.meta.declare_const_variable(Name::new_temp(""), type_id, precision)
    }

    // In GLSL, it's possible to mark a variable as invariant or precise in a separate line,
    // especially useful for built-ins that are otherwise not necessarily declared explicitly.
    // The following allow these declarations to add invariant and precise decorations.
    fn mark_variable_invariant(&mut self, variable_id: VariableId) {
        self.ir.meta.get_variable_mut(variable_id).decorations.add_invariant();
    }
    fn mark_variable_precise(&mut self, variable_id: VariableId) {
        self.ir.meta.get_variable_mut(variable_id).decorations.add_precise();
    }

    // When a function prototype is encountered, the following functions are called:
    //
    //     return_type function (...) @new_function;
    //
    // If that is followed by the function definition, then the following functions are called:
    //
    //     return_type function (type param, ...) @new_function/@update_function_param_names
    //                                            @begin_function
    //     {
    //         ...
    //     }
    //     @end_function
    //
    // Note that if the prototype is previously seen, `new_function` is not called again on
    // definition because the function id is already known.  Instead, `update_function_param_names`
    // is called so the parameter names reflect that of the definition.

    pub fn new_function(
        &mut self,
        name: &'static str,
        params: Vec<FunctionParam>,
        return_type_id: TypeId,
        return_precision: Precision,
        return_decorations: Decorations,
    ) -> FunctionId {
        let function =
            Function::new(name, params, return_type_id, return_precision, return_decorations);
        let is_main = name == "main";

        let id = self.ir.add_function(function);

        // If this is the main() function, remember its id.
        if is_main {
            self.ir.meta.set_main_function_id(id);
        }

        id
    }
    pub fn update_function_param_names(
        &mut self,
        id: FunctionId,
        param_names: &[&'static str],
    ) -> Vec<VariableId> {
        let function = self.ir.meta.get_function_mut(id);
        let params = function.params.iter().map(|param| param.variable_id).collect::<Vec<_>>();

        debug_assert!(params.len() == param_names.len());

        params.iter().zip(param_names.iter()).for_each(|(&param, name)| {
            self.ir.meta.get_variable_mut(param).name = Name::new_temp(name)
        });

        params
    }

    // Start a new function to build.
    pub fn begin_function(&mut self, id: FunctionId) {
        debug_assert!(self.current_function_cfg.is_empty());
        self.current_function = Some(id);
    }

    // Declare a parameter for the function that is being defined.  Called for each parameters
    // before calling `begin_function`.  This is not done while parsing the prototype because if it
    // isn't followed by a definition, there is no need to assign variable ids to them.
    pub fn declare_function_param(
        &mut self,
        name: &'static str,
        type_id: TypeId,
        precision: Precision,
        decorations: Decorations,
        direction: FunctionParamDirection,
    ) -> VariableId {
        let variable_id = self.declare_variable(
            Name::new_temp(name),
            type_id,
            precision,
            decorations,
            None,
            VariableScope::FunctionParam,
        );

        if self.options.initialize_uninitialized_variables
            && direction == FunctionParamDirection::Output
        {
            self.ir.meta.require_variable_zero_initialization(variable_id);
        }

        variable_id
    }

    // Once the entire function body is visited, `end_function` puts the graph in the function.
    pub fn end_function(&mut self) {
        // End with a `return` in case not explicitly added.
        if !self.scope().are_new_instructions_dead_code_in_current_block() {
            let return_type_id =
                self.ir.meta.get_function(self.current_function.unwrap()).return_type_id;
            if return_type_id == TYPE_ID_VOID {
                self.branch_return();
            } else {
                // In GLSL, it's possible to miss the return statement from a function that returns
                // a value.  Generate a zero node.
                let null_value = self.ir.meta.get_constant_null(return_type_id);
                self.push_constant(null_value, return_type_id);
                self.branch_return_value();
            }
        }

        self.ir.set_function_entry(
            self.current_function.unwrap(),
            self.current_function_cfg.pop_block(),
        );
        debug_assert!(self.current_function_cfg.is_empty());
        self.current_function = None;

        debug_assert!(self.interm_ids.is_empty());
    }

    // Many instructions are generated either as part of a function or the initialization code for
    // global variables.  The following function should be used so that the generated code is
    // placed in the appropriate scope (function or global).
    fn scope(&mut self) -> &mut CFGBuilder {
        match self.current_function {
            Some(_) => &mut self.current_function_cfg,
            None => &mut self.global_initializers_cfg,
        }
    }

    // Used internally, loads an id from the top of the id stack.  If that is a pointer, its value
    // is loaded before returning.
    fn load(&mut self) -> TypedId {
        let to_load = self.interm_ids.pop().unwrap();

        let result = instruction::load(&mut self.ir.meta, to_load);
        self.add_instruction(result);

        // Actually return the loaded result instead of leaving it on the stack.
        self.interm_ids.pop().unwrap()
    }

    // Store the value at the top of the id stack into the pointer under it.
    pub fn store(&mut self) {
        let value = self.load();
        // The `a = b` expression is itself a value, and we use `a` to load that value back if
        // used, so the pointer is kept on the stack.  Note that the precision of a and b may be
        // different, and if the result of a = b is used, the precision of a is the correct one
        // (already on the stack!).
        let pointer = *self.interm_ids.last().unwrap();

        let result = instruction::store(&mut self.ir.meta, pointer, value);
        self.add_instruction(result);
    }

    // Use the value at the top of the id stack as the initializer for the given variable.  If the
    // initializer is a constant, it will be set in `Variable::initializer`.  Otherwise similarly
    // to `store()`, the non-constant expression is stored with `OpCode::Store` to be deferred to
    // the beginning of `main()`.
    pub fn initialize(&mut self, id: VariableId) {
        let value = self.load();

        // For some generators, non-const global variables cannot have an initializer.
        let initializer_allowed = self.options.initializer_allowed_on_non_const_global_variables
            || self.current_function.is_some()
            || self.ir.meta.get_variable(id).is_const;

        if let Id::Constant(constant_id) = value.id
            && initializer_allowed
        {
            self.ir.meta.set_variable_initializer(id, constant_id);
        } else {
            self.ir.meta.on_variable_initialized(id);
            let id = TypedId::from_variable_id(&self.ir.meta, id);
            self.scope().add_void_instruction(OpCode::Store(id, value));
        }
    }

    // Flow control helpers.

    // For `if` support, the following functions are called at the specified points:
    //
    //     if (condition) {
    //         @begin_if_true_block
    //         ...
    //         @end_if_true_block
    //     } else {
    //         @begin_if_false_block
    //         ...
    //         @end_if_false_block
    //     }
    //     @end_if
    //
    // In `begin_if_true_block`, the result of the `condition` is found at the top of the id stack.
    pub fn begin_if_true_block(&mut self) {
        // The condition must be at the top of the stack.
        let condition = self.load();
        self.scope().begin_if_true_block(condition);
    }
    pub fn end_if_true_block(&mut self) {
        self.scope().end_if_true_block(None);
    }
    pub fn begin_if_false_block(&mut self) {
        self.scope().begin_if_false_block();
    }
    pub fn end_if_false_block(&mut self) {
        self.scope().end_if_false_block(None);
    }
    pub fn end_if(&mut self) {
        self.scope().end_if(None);
    }

    // Support for ?: is similar to if/else:
    //
    //     (condition ?
    //         @begin_ternary_true_expression
    //         ...
    //         @end_ternary_true_expression :
    //         @begin_ternary_false_expression
    //         ...
    //         @end_ternary_false_expression) @end_ternary
    //
    // In `begin_if_true_expression`, the result of the `condition` is found at the top of the id
    // stack. In `end_if_true_expression`, the merge parameter is found at the top of the id
    // stack. In `end_if_false_expression`, the merge parameter is found at the top of the id
    // stack.
    //
    // In the end, the result id is found in the input of the merge block of the if/else used to
    // implement it.
    //
    // If the expressions are `void`, then end_ternary_true_expression_void,
    // end_ternary_false_expression_void, and end_ternary_void are called.
    pub fn begin_ternary_true_expression(&mut self) {
        self.begin_if_true_block();
    }
    pub fn end_ternary_true_expression(&mut self) {
        let true_value = self.load();
        self.scope().end_if_true_block(Some(true_value));

        // The result of the merge will have the same type as either of the true or false values.
        // Push a new id with this type on the stack so that `end_ternary` would use it to declare
        // the input of the merge block.
        let new_id =
            self.ir.meta.new_register(OpCode::MergeInput, true_value.type_id, true_value.precision);
        self.interm_ids.push(TypedId::from_register_id(new_id));
    }
    pub fn end_ternary_true_expression_void(&mut self) {
        self.end_if_true_block();
    }
    pub fn begin_ternary_false_expression(&mut self) {
        self.begin_if_false_block();
    }
    pub fn end_ternary_false_expression(&mut self) {
        let false_value = self.load();
        self.scope().end_if_false_block(Some(false_value));

        // Adjust the precision of the result which was inherited from the true value.
        let result = &mut self.interm_ids.last_mut().unwrap();
        result.precision =
            instruction::precision::higher_precision(result.precision, false_value.precision);
        self.ir.meta.get_instruction_mut(result.id.get_register()).result.precision =
            result.precision;
    }
    pub fn end_ternary_false_expression_void(&mut self) {
        self.end_if_false_block();
    }
    pub fn end_ternary(&mut self) {
        let merge_input = self.interm_ids.pop().unwrap();
        let constant_folded_merge_input = self.scope().end_if(Some(merge_input));
        let result = constant_folded_merge_input.unwrap_or(merge_input);
        self.interm_ids.push(TypedId::new(result.id, merge_input.type_id, merge_input.precision));
    }
    pub fn end_ternary_void(&mut self) {
        self.end_if();
    }

    // Short circuit support is implemented over ?: and uses the same mechanism:
    //
    //     a || b == a ? true : b
    //     a && b == a ? b : false
    //
    // The following functions are called as specified below:
    //
    //     a @begin_short_circuit_or || b @end_short_circuit_or
    //     a @begin_short_circuit_and && b @end_short_circuit_and
    //
    // In `begin_short_circuit_or/and`, the result of `a` is found at the of the id stack.
    // In `end_short_circuit_or/and`, the result of `b` is found at the of the id stack.
    pub fn begin_short_circuit_or(&mut self) {
        self.begin_ternary_true_expression();
        self.push_constant_bool(true);
        self.end_ternary_true_expression();
        self.begin_ternary_false_expression();
    }
    pub fn end_short_circuit_or(&mut self) {
        self.end_ternary_false_expression();
        self.end_ternary()
    }
    pub fn begin_short_circuit_and(&mut self) {
        self.begin_ternary_true_expression();
    }
    pub fn end_short_circuit_and(&mut self) {
        self.end_ternary_true_expression();
        self.begin_ternary_false_expression();
        self.push_constant_bool(false);
        self.end_ternary_false_expression();
        self.end_ternary()
    }

    // For `while` and `for` support, the following functions are called at the specified points:
    //
    //     while (@begin_loop_condition condition @end_loop_condition) {
    //         ...
    //     }
    //     @end_loop
    //
    //     for (init;
    //          @begin_loop_condition condition @end_loop_condition;
    //          continue @end_loop_continue) {
    //         ...
    //     }
    //     @end_loop
    //
    // In `end_loop_condition`, the result of the `condition` is found at the top of the id stack.
    pub fn begin_loop_condition(&mut self) {
        self.scope().begin_loop_condition();
    }
    pub fn end_loop_condition(&mut self) {
        let condition = self.load();
        self.scope().end_loop_condition(condition);
    }
    pub fn end_loop_continue(&mut self) {
        self.scope().end_loop_continue();
    }
    pub fn end_loop(&mut self) {
        self.scope().end_loop();
    }

    // For `do-while` support, the following functions are called at the specified points:
    //
    //     do { @begin_do_loop
    //         ...
    //     @begin_do_loop_condition
    //     } while(condition);
    //     @end_do_loop
    //
    // In `end_do_loop`, the result of the `condition` is found at the top of the id stack.
    pub fn begin_do_loop(&mut self) {
        self.scope().begin_do_loop();
    }
    pub fn begin_do_loop_condition(&mut self) {
        self.scope().begin_do_loop_condition();
    }
    pub fn end_do_loop(&mut self) {
        let condition = self.load();
        self.scope().end_do_loop(condition);
    }

    // For `switch` support, the following functions are called at the specified points:
    //
    //     switch (value @begin_switch) {
    //         case N @begin_case:
    //             ...
    //         default @begin_default:
    //             ...
    //         ...
    //     }
    //     @end_switch
    //
    // In `begin_switch`, the result of the `value` is found at the top of the id stack.
    //
    // In `begin_case`, the value `N` is found at the top of the id stack.
    //
    // In `begin_case` or `end_switch`, if the previous case block is not already terminated, it
    // will terminate with `Passthrough` to the next block.
    pub fn begin_switch(&mut self) {
        let value = self.load();
        self.scope().begin_switch(value);
    }
    pub fn begin_case(&mut self) {
        let value = self.load();
        self.scope().begin_case(value);
    }
    pub fn begin_default(&mut self) {
        self.scope().begin_default();
    }
    pub fn end_switch(&mut self) {
        self.scope().end_switch();
    }

    pub fn branch_discard(&mut self) {
        self.add_instruction(instruction::branch_discard());
    }
    pub fn branch_return(&mut self) {
        self.add_instruction(instruction::branch_return(None));
    }
    pub fn branch_return_value(&mut self) {
        let value = self.load();
        self.add_instruction(instruction::branch_return(Some(value)));
    }
    pub fn branch_break(&mut self) {
        self.add_instruction(instruction::branch_break());
    }
    pub fn branch_continue(&mut self) {
        self.add_instruction(instruction::branch_continue());
    }

    // Called when a constant expression used as an array size is evaluated.  The result is found
    // on the stack, but is not used by an instruction; it's returned to the parser instead.
    pub fn pop_array_size(&mut self) -> u32 {
        let size_id = self.interm_ids.pop().unwrap().id.get_constant().unwrap();
        // Use get_index() because the expression may either be int or uint, both of which are
        // acceptable; get_index() returns a u32 for either case.
        self.ir.meta.get_constant(size_id).value.get_index()
    }

    // Called when a statement in a block is finished, but only if the statement has a value (i.e.
    // leaves an id on the stack).
    //
    //     statement; @end_statement_with_value
    //
    // or
    //
    //     expression, @end_statement_with_value expression2
    //
    pub fn end_statement_with_value(&mut self) {
        // There _must_ be a value on the stack, unwrap() will assert that.
        self.interm_ids.pop().unwrap();
    }

    fn push_id(&mut self, id: Id, type_id: TypeId, precision: Precision) {
        self.interm_ids.push(TypedId::new(id, type_id, precision));
    }

    // Called when constant scalar values are visited.
    fn push_constant(&mut self, id: ConstantId, type_id: TypeId) {
        self.push_id(Id::new_constant(id), type_id, Precision::NotApplicable);
    }
    pub fn push_constant_float(&mut self, value: f32) {
        let id = self.ir.meta.get_constant_float(value);
        self.push_constant(id, TYPE_ID_FLOAT);
    }
    pub fn push_constant_int(&mut self, value: i32) {
        let id = self.ir.meta.get_constant_int(value);
        self.push_constant(id, TYPE_ID_INT);
    }
    pub fn push_constant_uint(&mut self, value: u32) {
        let id = self.ir.meta.get_constant_uint(value);
        self.push_constant(id, TYPE_ID_UINT);
    }
    pub fn push_constant_bool(&mut self, value: bool) {
        let id = self.ir.meta.get_constant_bool(value);
        self.push_constant(id, TYPE_ID_BOOL);
    }
    pub fn push_constant_yuv_csc_standard(&mut self, value: YuvCscStandard) {
        let id = self.ir.meta.get_constant_yuv_csc_standard(value);
        self.push_constant(id, TYPE_ID_YUV_CSC_STANDARD);
    }

    // Called when a variable is visited.
    pub fn push_variable(&mut self, id: VariableId) {
        let variable = self.ir.meta.get_variable(id);
        let type_id = variable.type_id;
        let precision = variable.precision;

        debug_assert!(self.ir.meta.get_type(type_id).is_pointer());

        // If the variable is a constant, replace it with its constant value but with the
        // variable's precision.  While constants are not associated with a precision, const
        // variables are.  As a result, the resulting TypedId when this value is load()ed will be a
        // constant but with a precision, which affects the precision of other intermediate ops.
        if variable.is_const {
            debug_assert!(variable.initializer.is_some());
            debug_assert!(
                self.ir.meta.get_type(type_id).get_element_type_id().unwrap()
                    == self.ir.meta.get_constant(variable.initializer.unwrap()).type_id
            );

            let constant_id = variable.initializer.unwrap();
            let pointee_type_id = self.ir.meta.get_constant(constant_id).type_id;
            self.push_id(Id::new_constant(constant_id), pointee_type_id, precision)
        } else {
            // Mark the variable as statically used during parse.  This _has_ to be done during
            // parse because static use in dead code is still considered static use by GLSL.
            self.ir.meta.get_variable_mut(id).is_static_use = true;
            self.push_id(Id::new_variable(id), type_id, precision)
        }
    }

    // Depending on whether the instruction::* function returned a Constant, Void or Register,
    // either add a void or typed instruction (if necessary) and push a value to the stack (if
    // necessary).
    fn add_instruction(&mut self, result: instruction::Result) {
        match result {
            instruction::Result::Constant(id) => {
                self.interm_ids.push(TypedId::from_typed_constant_id(id))
            }
            instruction::Result::Void(op) => self.scope().add_void_instruction(op),
            instruction::Result::Register(id) => {
                self.scope().add_typed_instruction(id.id);
                self.interm_ids.push(TypedId::from_register_id(id))
            }
            instruction::Result::NoOp(id) => self.interm_ids.push(id),
        };
    }

    // Helper to get N arguments off the stack in the right order.
    fn get_args<Load, T>(n: usize, mut load: Load) -> Vec<T>
    where
        Load: FnMut() -> T,
    {
        let mut args: Vec<_> = (0..n).map(|_| load()).collect();
        args.reverse();
        args
    }

    // Called when a user-defined function is called
    pub fn call_function(&mut self, id: FunctionId) {
        let arg_count = self.ir.meta.get_function(id).params.len();

        // Collect the call arguments.
        //
        // Note: The function parameters are not load()ed because they may be `out` or `inout`,
        // i.e. they could be pointers.
        let args = Self::get_args(arg_count, || self.interm_ids.pop().unwrap());
        let result = instruction::call(&mut self.ir.meta, id, args);
        self.add_instruction(result);
    }

    // A generic helper to make an assignment operator with an op (like +=).  The rhs operand of
    // the binary operator is found at the top of the stack, and the lhs operand is found under it.
    fn assign_and_op<Op>(&mut self, op: Op)
    where
        Op: FnOnce(&mut Self),
    {
        // X op= Y is calculated as X = X op Y.  This is implemented by `Temp = X op Y`, followed
        // by `X = Temp`.  As a preparation, duplicate lhs on the stack so existing functions can
        // be used.
        let rhs = self.interm_ids.pop().unwrap();
        let lhs = *self.interm_ids.last().unwrap();
        self.interm_ids.push(lhs);
        self.interm_ids.push(rhs);

        // Create the op instruction followed by a store to lhs.
        op(self);
        self.store();
    }

    // Called when a component of a vector is taken, like `vector.y`.
    //
    // The vector being indexed is expected to be found at the top of the stack.
    pub fn vector_component(&mut self, component: u32) {
        let vector = self.interm_ids.pop().unwrap();
        let result = instruction::vector_component(&mut self.ir.meta, vector, component);
        self.add_instruction(result);
    }

    // Called when multiple components of a vector are taken, like `vector.yxy`.
    //
    // The vector being indexed is expected to be found at the top of the stack.
    pub fn vector_component_multi(&mut self, components: Vec<u32>) {
        let vector = self.interm_ids.pop().unwrap();

        let result = instruction::vector_component_multi(&mut self.ir.meta, vector, components);
        self.add_instruction(result);
    }

    // Called when an array-like index is taken:
    //
    // - vector[expr]
    // - matrix[expr]
    // - array[expr]
    //
    // The index is expected to be found at the top of the stack, followed by the expression being
    // indexed.
    pub fn index(&mut self) {
        let index = self.load();
        let indexed = self.interm_ids.pop().unwrap();

        let result = instruction::index(&mut self.ir.meta, indexed, index);
        self.add_instruction(result);
    }

    // Called when a field of a struct is selected, like `block.field`.
    //
    // The struct whose field is being selected is expected to be found at the top of the stack.
    pub fn struct_field(&mut self, field_index: u32) {
        let struct_id = self.interm_ids.pop().unwrap();

        let result = instruction::struct_field(&mut self.ir.meta, struct_id, field_index);
        self.add_instruction(result);
    }

    fn trim_constructor_args_to_component_count(
        &mut self,
        args: Vec<TypedId>,
        count: u32,
    ) -> Vec<TypedId> {
        // To simplify this, do a first pass where matrix arguments are split by column.  This lets
        // the following code simply deal with scalars and vectors.
        let mut matrix_expanded_args = Vec::new();
        for id in args {
            let type_info = self.ir.meta.get_type(id.type_id);

            if let &Type::Matrix(_, column_count) = type_info {
                for row in 0..column_count {
                    self.interm_ids.push(id);
                    self.interm_ids.push(TypedId::from_constant_id(
                        self.ir.meta.get_constant_uint(row),
                        TYPE_ID_UINT,
                    ));
                    self.index();
                    matrix_expanded_args.push(self.load());
                }
            } else {
                matrix_expanded_args.push(id);
            }
        }

        let mut result = Vec::new();
        let mut total = 0;

        for id in matrix_expanded_args {
            let type_info = self.ir.meta.get_type(id.type_id);

            if let &Type::Scalar(..) = type_info {
                // Scalar values add a single component, take them directly.
                result.push(id);
                total += 1;
            } else if let &Type::Vector(_, vec_size) = type_info {
                // If the vector is entirely consumed, take it directly.  Otherwise only take
                // enough components to fulfill the given `count`.
                let needed_components = count - total;
                if needed_components >= vec_size {
                    result.push(id);
                    total += vec_size;
                } else {
                    self.interm_ids.push(id);
                    if needed_components == 1 {
                        self.vector_component(0);
                    } else {
                        self.vector_component_multi((0..needed_components).collect());
                    }
                    result.push(self.load());
                    total = count;
                }
            }

            debug_assert!(total <= count);
            if total >= count {
                break;
            }
        }

        result
    }

    // Trim extra arguments passed to a constructor.  This is only possible with a vector and
    // matrix constructor.
    //
    // Constructing a matrix from another matrix is special, the input matrix may have more or
    // fewer components.  So matNxM(m) is not trimmed by this function.
    //
    // Additionally, array and struct constructors require an exact match of arguments, so no
    // stripping is done.
    fn trim_constructor_args(&mut self, type_id: TypeId, args: Vec<TypedId>) -> Vec<TypedId> {
        let type_info = self.ir.meta.get_type(type_id);

        match type_info {
            // Nothing to do for arrays and structs
            Type::Array(..) | Type::Struct(..) => {
                return args;
            }
            // Early out for matrix-from-matrix constructors.
            Type::Matrix(..) if args.len() == 1 => {
                return args;
            }
            _ => (),
        }

        let total_components = type_info.get_total_component_count(&self.ir.meta);
        self.trim_constructor_args_to_component_count(args, total_components)
    }

    // Called when a constructor is used, like `vec(a, b, c)`.  Constructors take many
    // forms, including having more parameters than needed.  This function corrects that by taking
    // the appropriate number of parameters.  The following are examples of each form:
    //
    // - float(f), vecN(vN), matNxM(mNxM): Replaced with just f, vN and mNxM respectively
    // - float(v): Replaced with float(v.x)
    // - float(m): Replaced with m[0][0]
    // - vecN(f)
    // - vecN(v1.zy, v2.xzy): If number of components exceeds N, extra components are stripped, such
    //   as vecN(v1.zy, v2.x)
    // - vecN(m)
    // - matNxM(f)
    // - matNxM(m)
    // - matNxM(v1.zy, v2.x, ...): Similarly to constructing a vector, extra components are
    //   stripped.
    // - array(elem0, elem1, ...)
    // - Struct(field0, field1, ...)
    //
    // The arguments of the constructor are found on the stack in reverse order.
    pub fn construct(&mut self, type_id: TypeId, arg_count: usize) {
        // Collect the constructor arguments.
        let args = Self::get_args(arg_count, || self.load());

        // In the case of ConstructVectorFromMultiple and ConstructMatrixFromMultiple, there may be
        // more components in the parameters than the constructor needs.  In this step, these
        // components are removed.  Other constructor kinds are unaffected.
        //
        // Note: this can affect the precision of the result if the stripped args would have
        // increased the end precision.  This is such a niche case that we're ignoring it for
        // simplicity.
        let args = self.trim_constructor_args(type_id, args);

        let result = instruction::construct(&mut self.ir.meta, type_id, args);
        self.add_instruction(result);
    }

    fn declare_clip_cull_distance_array_length_variable(
        &mut self,
        existing_id: Option<VariableId>,
        name: &'static str,
    ) -> VariableId {
        existing_id.unwrap_or_else(|| {
            let name = Name::new_temp(name);
            self.declare_variable(
                name,
                TYPE_ID_INT,
                Precision::Low,
                Decorations::new_none(),
                None,
                VariableScope::Global,
            )
        })
    }

    fn on_gl_clip_cull_distance_sized(
        &mut self,
        id: VariableId,
        length: u32,
        length_variable: Option<VariableId>,
    ) {
        let type_id = self.ir.meta.get_variable(id).type_id;
        let type_info = self.ir.meta.get_type(type_id);
        debug_assert!(type_info.is_pointer());

        let type_id = type_info.get_element_type_id().unwrap();
        let type_info = self.ir.meta.get_type(type_id);
        debug_assert!(type_info.is_unsized_array());

        let element_type_id = type_info.get_element_type_id().unwrap();
        let sized_type_id = self.ir.meta.get_array_type_id(element_type_id, length);
        let sized_type_id = self.ir.meta.get_pointer_type_id(sized_type_id);

        // Update the type of the variable.
        self.ir.meta.get_variable_mut(id).type_id = sized_type_id;

        if let Some(length_variable_id) = length_variable {
            let length = self.ir.meta.get_constant_int(length as i32);
            self.ir.meta.set_variable_initializer(length_variable_id, length);
        }
    }

    // Called when the size of gl_ClipDistance is determined.  It updates the type of the
    // gl_ClipDistance variable and initializes the temp variable used to hold its length (if any).
    pub fn on_gl_clip_distance_sized(&mut self, id: VariableId, length: u32) {
        self.on_gl_clip_cull_distance_sized(id, length, self.gl_clip_distance_length_var_id);
    }
    pub fn on_gl_cull_distance_sized(&mut self, id: VariableId, length: u32) {
        self.on_gl_clip_cull_distance_sized(id, length, self.gl_cull_distance_length_var_id);
    }

    // Called when the length() method is visited.
    pub fn array_length(&mut self) {
        let operand = self.interm_ids.pop().unwrap();
        let type_info = self.ir.meta.get_type(operand.type_id);

        // Support for gl_ClipDistance and gl_CullDistance, who may be sized later.
        let (is_clip_distance, is_cull_distance) = if let Id::Variable(variable_id) = operand.id {
            let built_in = self.ir.meta.get_variable(variable_id).built_in;
            (built_in == Some(BuiltIn::ClipDistance), built_in == Some(BuiltIn::CullDistance))
        } else {
            (false, false)
        };

        match *type_info {
            Type::Pointer(type_id) => {
                let array_type_info = self.ir.meta.get_type(type_id);
                if let &Type::Array(_, size) = array_type_info {
                    // The length is a constant, so push that on the stack.
                    self.push_constant_int(size as i32);
                } else if is_clip_distance || is_cull_distance {
                    // If gl_ClipDistance and gl_CullDistance are not yet sized, use a global
                    // variable for their size.  When they are sized, the global variable is
                    // initialized with the length.
                    let length_var = if is_clip_distance {
                        let new_var = self.declare_clip_cull_distance_array_length_variable(
                            self.gl_clip_distance_length_var_id,
                            "clip_distance_length",
                        );
                        self.gl_clip_distance_length_var_id = Some(new_var);
                        new_var
                    } else {
                        let new_var = self.declare_clip_cull_distance_array_length_variable(
                            self.gl_cull_distance_length_var_id,
                            "cull_distance_length",
                        );
                        self.gl_cull_distance_length_var_id = Some(new_var);
                        new_var
                    };

                    // Load from the (currently uninitialized) variable.  The newly added
                    // variable is initialized with the real length when determined.
                    self.push_variable(length_var);
                    let length = self.load();
                    self.interm_ids.push(length);
                } else {
                    // The array is unsized, so this should be a runtime instruction.
                    let result = instruction::array_length(&mut self.ir.meta, operand);
                    self.add_instruction(result);
                }
            }
            Type::Array(_, size) => {
                self.push_constant_int(size as i32);
            }
            _ => panic!("Internal error: length() called on non-array"),
        }
    }

    // A generic helper to make a unary operator.  The operand of the unary operator is found at
    // the top of the stack.
    fn unary_instruction_from_stack<MakeInst>(&mut self, inst: MakeInst)
    where
        MakeInst: FnOnce(&mut IRMeta, TypedId) -> instruction::Result,
    {
        // Load the operand.
        let operand = self.load();

        let result = inst(&mut self.ir.meta, operand);
        self.add_instruction(result);
    }
    fn unary_instruction_from_stack_with_pointer<MakeInst>(&mut self, inst: MakeInst)
    where
        MakeInst: FnOnce(&mut IRMeta, TypedId) -> instruction::Result,
    {
        // The operand is expected to be a pointer.
        let operand = self.interm_ids.pop().unwrap();

        let result = inst(&mut self.ir.meta, operand);
        self.add_instruction(result);
    }

    // A generic helper to make a binary operator.  The rhs operand of the binary operator is found
    // at the top of the stack, and the lhs operand is found under it.
    fn binary_instruction_from_stack<MakeInst>(&mut self, inst: MakeInst)
    where
        MakeInst: FnOnce(&mut IRMeta, TypedId, TypedId) -> instruction::Result,
    {
        // Load the operands.
        let rhs = self.load();
        let lhs = self.load();

        let result = inst(&mut self.ir.meta, lhs, rhs);
        self.add_instruction(result);
    }

    // Arithmetic operations.
    pub fn negate(&mut self) {
        self.unary_instruction_from_stack(instruction::negate);
    }
    pub fn postfix_increment(&mut self) {
        self.unary_instruction_from_stack_with_pointer(instruction::postfix_increment);
    }
    pub fn postfix_decrement(&mut self) {
        self.unary_instruction_from_stack_with_pointer(instruction::postfix_decrement);
    }
    pub fn prefix_increment(&mut self) {
        self.unary_instruction_from_stack_with_pointer(instruction::prefix_increment);
    }
    pub fn prefix_decrement(&mut self) {
        self.unary_instruction_from_stack_with_pointer(instruction::prefix_decrement);
    }
    pub fn add(&mut self) {
        self.binary_instruction_from_stack(instruction::add);
    }
    pub fn add_assign(&mut self) {
        self.assign_and_op(Self::add);
    }
    pub fn sub(&mut self) {
        self.binary_instruction_from_stack(instruction::sub);
    }
    pub fn sub_assign(&mut self) {
        self.assign_and_op(Self::sub);
    }
    pub fn mul(&mut self) {
        self.binary_instruction_from_stack(instruction::mul);
    }
    pub fn mul_assign(&mut self) {
        self.assign_and_op(Self::mul);
    }
    pub fn vector_times_scalar(&mut self) {
        self.binary_instruction_from_stack(instruction::vector_times_scalar);
    }
    pub fn vector_times_scalar_assign(&mut self) {
        self.assign_and_op(Self::vector_times_scalar);
    }
    pub fn matrix_times_scalar(&mut self) {
        self.binary_instruction_from_stack(instruction::matrix_times_scalar);
    }
    pub fn matrix_times_scalar_assign(&mut self) {
        self.assign_and_op(Self::matrix_times_scalar);
    }
    pub fn vector_times_matrix(&mut self) {
        self.binary_instruction_from_stack(instruction::vector_times_matrix);
    }
    pub fn vector_times_matrix_assign(&mut self) {
        self.assign_and_op(Self::vector_times_matrix);
    }
    pub fn matrix_times_vector(&mut self) {
        self.binary_instruction_from_stack(instruction::matrix_times_vector);
    }
    pub fn matrix_times_matrix(&mut self) {
        self.binary_instruction_from_stack(instruction::matrix_times_matrix);
    }
    pub fn matrix_times_matrix_assign(&mut self) {
        self.assign_and_op(Self::matrix_times_matrix);
    }
    pub fn div(&mut self) {
        self.binary_instruction_from_stack(instruction::div);
    }
    pub fn div_assign(&mut self) {
        self.assign_and_op(Self::div);
    }
    pub fn imod(&mut self) {
        self.binary_instruction_from_stack(instruction::imod);
    }
    pub fn imod_assign(&mut self) {
        self.assign_and_op(Self::imod);
    }

    // Logical operations.
    pub fn logical_not(&mut self) {
        self.unary_instruction_from_stack(instruction::logical_not);
    }
    pub fn logical_xor(&mut self) {
        self.binary_instruction_from_stack(instruction::logical_xor);
    }

    // Comparisons.
    pub fn equal(&mut self) {
        self.binary_instruction_from_stack(instruction::equal);
    }
    pub fn not_equal(&mut self) {
        self.binary_instruction_from_stack(instruction::not_equal);
    }
    pub fn less_than(&mut self) {
        self.binary_instruction_from_stack(instruction::less_than);
    }
    pub fn greater_than(&mut self) {
        self.binary_instruction_from_stack(instruction::greater_than);
    }
    pub fn less_than_equal(&mut self) {
        self.binary_instruction_from_stack(instruction::less_than_equal);
    }
    pub fn greater_than_equal(&mut self) {
        self.binary_instruction_from_stack(instruction::greater_than_equal);
    }

    // Bit operations.
    pub fn bitwise_not(&mut self) {
        self.unary_instruction_from_stack(instruction::bitwise_not);
    }
    pub fn bit_shift_left(&mut self) {
        self.binary_instruction_from_stack(instruction::bit_shift_left);
    }
    pub fn bit_shift_left_assign(&mut self) {
        self.assign_and_op(Self::bit_shift_left);
    }
    pub fn bit_shift_right(&mut self) {
        self.binary_instruction_from_stack(instruction::bit_shift_right);
    }
    pub fn bit_shift_right_assign(&mut self) {
        self.assign_and_op(Self::bit_shift_right);
    }
    pub fn bitwise_or(&mut self) {
        self.binary_instruction_from_stack(instruction::bitwise_or);
    }
    pub fn bitwise_or_assign(&mut self) {
        self.assign_and_op(Self::bitwise_or);
    }
    pub fn bitwise_xor(&mut self) {
        self.binary_instruction_from_stack(instruction::bitwise_xor);
    }
    pub fn bitwise_xor_assign(&mut self) {
        self.assign_and_op(Self::bitwise_xor);
    }
    pub fn bitwise_and(&mut self) {
        self.binary_instruction_from_stack(instruction::bitwise_and);
    }
    pub fn bitwise_and_assign(&mut self) {
        self.assign_and_op(Self::bitwise_and);
    }

    // Generic helpers to make a built-in operator.  The operands are found in reverse order in
    // the stack.
    fn built_in_unary_instruction_from_stack(&mut self, built_in_op: UnaryOpCode) {
        self.unary_instruction_from_stack(|ir_meta, operand| {
            instruction::built_in_unary(ir_meta, built_in_op, operand)
        });
    }
    fn built_in_unary_instruction_from_stack_with_pointer(&mut self, built_in_op: UnaryOpCode) {
        self.unary_instruction_from_stack_with_pointer(|ir_meta, operand| {
            instruction::built_in_unary(ir_meta, built_in_op, operand)
        });
    }
    fn built_in_binary_instruction_from_stack_with_pointer_lhs(
        &mut self,
        built_in_op: BinaryOpCode,
    ) {
        // Load the operands.
        let rhs = self.load();
        let lhs = self.interm_ids.pop().unwrap();

        let result = instruction::built_in_binary(&mut self.ir.meta, built_in_op, lhs, rhs);
        self.add_instruction(result);
    }
    fn built_in_binary_instruction_from_stack_with_pointer_rhs(
        &mut self,
        built_in_op: BinaryOpCode,
    ) {
        // Load the operands.
        let rhs = self.interm_ids.pop().unwrap();
        let lhs = self.load();

        let result = instruction::built_in_binary(&mut self.ir.meta, built_in_op, lhs, rhs);
        self.add_instruction(result);
    }
    fn built_in_binary_instruction_from_stack(&mut self, built_in_op: BinaryOpCode) {
        self.binary_instruction_from_stack(|ir_meta, lhs, rhs| {
            instruction::built_in_binary(ir_meta, built_in_op, lhs, rhs)
        });
    }
    fn built_in_instruction_from_stack_with_pointer_args(
        &mut self,
        built_in_op: BuiltInOpCode,
        first_pointer_arg_count: usize,
        load_arg_count: usize,
        last_pointer_arg_count: usize,
    ) {
        // Load the operands.  Note that pointer arguments are always either the first or last
        // arguments in GLSL built-ins.
        let mut args =
            (0..last_pointer_arg_count).map(|_| self.interm_ids.pop().unwrap()).collect::<Vec<_>>();
        args.extend((0..load_arg_count).map(|_| self.load()));
        args.extend((0..first_pointer_arg_count).map(|_| self.interm_ids.pop().unwrap()));
        args.reverse();

        let result = instruction::built_in(&mut self.ir.meta, built_in_op, args);
        self.add_instruction(result);
    }
    fn built_in_instruction_from_stack(&mut self, built_in_op: BuiltInOpCode, arg_count: usize) {
        self.built_in_instruction_from_stack_with_pointer_args(built_in_op, 0, arg_count, 0);
    }

    // Built-in operations
    pub fn built_in_radians(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Radians);
    }
    pub fn built_in_degrees(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Degrees);
    }
    pub fn built_in_sin(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Sin);
    }
    pub fn built_in_cos(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Cos);
    }
    pub fn built_in_tan(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Tan);
    }
    pub fn built_in_asin(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Asin);
    }
    pub fn built_in_acos(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Acos);
    }
    pub fn built_in_atan(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Atan);
    }
    pub fn built_in_sinh(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Sinh);
    }
    pub fn built_in_cosh(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Cosh);
    }
    pub fn built_in_tanh(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Tanh);
    }
    pub fn built_in_asinh(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Asinh);
    }
    pub fn built_in_acosh(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Acosh);
    }
    pub fn built_in_atanh(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Atanh);
    }
    pub fn built_in_atan_binary(&mut self) {
        self.built_in_binary_instruction_from_stack(BinaryOpCode::Atan);
    }
    pub fn built_in_pow(&mut self) {
        self.built_in_binary_instruction_from_stack(BinaryOpCode::Pow);
    }
    pub fn built_in_exp(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Exp);
    }
    pub fn built_in_log(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Log);
    }
    pub fn built_in_exp2(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Exp2);
    }
    pub fn built_in_log2(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Log2);
    }
    pub fn built_in_sqrt(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Sqrt);
    }
    pub fn built_in_inversesqrt(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Inversesqrt);
    }
    pub fn built_in_abs(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Abs);
    }
    pub fn built_in_sign(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Sign);
    }
    pub fn built_in_floor(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Floor);
    }
    pub fn built_in_trunc(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Trunc);
    }
    pub fn built_in_round(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Round);
    }
    pub fn built_in_roundeven(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::RoundEven);
    }
    pub fn built_in_ceil(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Ceil);
    }
    pub fn built_in_fract(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Fract);
    }
    pub fn built_in_mod(&mut self) {
        self.built_in_binary_instruction_from_stack(BinaryOpCode::Mod);
    }
    pub fn built_in_min(&mut self) {
        self.built_in_binary_instruction_from_stack(BinaryOpCode::Min);
    }
    pub fn built_in_max(&mut self) {
        self.built_in_binary_instruction_from_stack(BinaryOpCode::Max);
    }
    pub fn built_in_clamp(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::Clamp, 3);
    }
    pub fn built_in_mix(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::Mix, 3);
    }
    pub fn built_in_step(&mut self) {
        self.built_in_binary_instruction_from_stack(BinaryOpCode::Step);
    }
    pub fn built_in_smoothstep(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::Smoothstep, 3);
    }
    pub fn built_in_modf(&mut self) {
        // modf() takes two parameters, the second of which is a pointer.
        self.built_in_binary_instruction_from_stack_with_pointer_rhs(BinaryOpCode::Modf);
    }
    pub fn built_in_isnan(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Isnan);
    }
    pub fn built_in_isinf(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Isinf);
    }
    pub fn built_in_floatbitstoint(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::FloatBitsToInt);
    }
    pub fn built_in_floatbitstouint(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::FloatBitsToUint);
    }
    pub fn built_in_intbitstofloat(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::IntBitsToFloat);
    }
    pub fn built_in_uintbitstofloat(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::UintBitsToFloat);
    }
    pub fn built_in_fma(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::Fma, 3);
    }
    pub fn built_in_frexp(&mut self) {
        // frexp() takes two parameters, the second of which is a pointer.
        self.built_in_binary_instruction_from_stack_with_pointer_rhs(BinaryOpCode::Frexp);
    }
    pub fn built_in_ldexp(&mut self) {
        self.built_in_binary_instruction_from_stack(BinaryOpCode::Ldexp);
    }
    pub fn built_in_packsnorm2x16(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::PackSnorm2x16);
    }
    pub fn built_in_packhalf2x16(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::PackHalf2x16);
    }
    pub fn built_in_unpacksnorm2x16(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::UnpackSnorm2x16);
    }
    pub fn built_in_unpackhalf2x16(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::UnpackHalf2x16);
    }
    pub fn built_in_packunorm2x16(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::PackUnorm2x16);
    }
    pub fn built_in_unpackunorm2x16(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::UnpackUnorm2x16);
    }
    pub fn built_in_packunorm4x8(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::PackUnorm4x8);
    }
    pub fn built_in_packsnorm4x8(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::PackSnorm4x8);
    }
    pub fn built_in_unpackunorm4x8(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::UnpackUnorm4x8);
    }
    pub fn built_in_unpacksnorm4x8(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::UnpackSnorm4x8);
    }
    pub fn built_in_length(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Length);
    }
    pub fn built_in_distance(&mut self) {
        self.built_in_binary_instruction_from_stack(BinaryOpCode::Distance);
    }
    pub fn built_in_dot(&mut self) {
        self.built_in_binary_instruction_from_stack(BinaryOpCode::Dot);
    }
    pub fn built_in_cross(&mut self) {
        self.built_in_binary_instruction_from_stack(BinaryOpCode::Cross);
    }
    pub fn built_in_normalize(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Normalize);
    }
    pub fn built_in_faceforward(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::Faceforward, 3);
    }
    pub fn built_in_reflect(&mut self) {
        self.built_in_binary_instruction_from_stack(BinaryOpCode::Reflect);
    }
    pub fn built_in_refract(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::Refract, 3);
    }
    pub fn built_in_matrixcompmult(&mut self) {
        self.built_in_binary_instruction_from_stack(BinaryOpCode::MatrixCompMult);
    }
    pub fn built_in_outerproduct(&mut self) {
        self.built_in_binary_instruction_from_stack(BinaryOpCode::OuterProduct);
    }
    pub fn built_in_transpose(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Transpose);
    }
    pub fn built_in_determinant(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Determinant);
    }
    pub fn built_in_inverse(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Inverse);
    }
    pub fn built_in_lessthan(&mut self) {
        self.built_in_binary_instruction_from_stack(BinaryOpCode::LessThanVec);
    }
    pub fn built_in_lessthanequal(&mut self) {
        self.built_in_binary_instruction_from_stack(BinaryOpCode::LessThanEqualVec);
    }
    pub fn built_in_greaterthan(&mut self) {
        self.built_in_binary_instruction_from_stack(BinaryOpCode::GreaterThanVec);
    }
    pub fn built_in_greaterthanequal(&mut self) {
        self.built_in_binary_instruction_from_stack(BinaryOpCode::GreaterThanEqualVec);
    }
    pub fn built_in_equal(&mut self) {
        self.built_in_binary_instruction_from_stack(BinaryOpCode::EqualVec);
    }
    pub fn built_in_notequal(&mut self) {
        self.built_in_binary_instruction_from_stack(BinaryOpCode::NotEqualVec);
    }
    pub fn built_in_any(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Any);
    }
    pub fn built_in_all(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::All);
    }
    pub fn built_in_not(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Not);
    }
    pub fn built_in_bitfieldextract(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::BitfieldExtract, 3);
    }
    pub fn built_in_bitfieldinsert(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::BitfieldInsert, 4);
    }
    pub fn built_in_bitfieldreverse(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::BitfieldReverse);
    }
    pub fn built_in_bitcount(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::BitCount);
    }
    pub fn built_in_findlsb(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::FindLSB);
    }
    pub fn built_in_findmsb(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::FindMSB);
    }
    pub fn built_in_uaddcarry(&mut self) {
        // uaddCarry() takes three parameters, the last of which is a pointer.
        self.built_in_instruction_from_stack_with_pointer_args(BuiltInOpCode::UaddCarry, 0, 2, 1);
    }
    pub fn built_in_usubborrow(&mut self) {
        // usubBurrow() takes three parameters, the last of which is a pointer.
        self.built_in_instruction_from_stack_with_pointer_args(BuiltInOpCode::UsubBorrow, 0, 2, 1);
    }
    pub fn built_in_umulextended(&mut self) {
        // umulExtended() takes four parameters, the last two of which are pointers.
        self.built_in_instruction_from_stack_with_pointer_args(
            BuiltInOpCode::UmulExtended,
            0,
            2,
            2,
        );
    }
    pub fn built_in_imulextended(&mut self) {
        // imulExtended() takes four parameters, the last two of which are pointers.
        self.built_in_instruction_from_stack_with_pointer_args(
            BuiltInOpCode::ImulExtended,
            0,
            2,
            2,
        );
    }
    pub fn built_in_texturesize(&mut self, with_lod: bool) {
        self.built_in_instruction_from_stack(
            BuiltInOpCode::TextureSize,
            if with_lod { 2 } else { 1 },
        );
    }
    pub fn built_in_texturequerylod(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::TextureQueryLod, 2);
    }
    pub fn built_in_texelfetch(&mut self, with_lod_or_sample: bool) {
        self.built_in_instruction_from_stack(
            BuiltInOpCode::TexelFetch,
            if with_lod_or_sample { 3 } else { 2 },
        );
    }
    pub fn built_in_texelfetchoffset(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::TexelFetchOffset, 4);
    }
    pub fn built_in_rgb_2_yuv(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::Rgb2Yuv, 2);
    }
    pub fn built_in_yuv_2_rgb(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::Yuv2Rgb, 2);
    }
    pub fn built_in_dfdx(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::DFdx);
    }
    pub fn built_in_dfdy(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::DFdy);
    }
    pub fn built_in_fwidth(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::Fwidth);
    }
    pub fn built_in_interpolateatcentroid(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::InterpolateAtCentroid);
    }
    pub fn built_in_interpolateatsample(&mut self) {
        self.built_in_binary_instruction_from_stack(BinaryOpCode::InterpolateAtSample);
    }
    pub fn built_in_interpolateatoffset(&mut self) {
        self.built_in_binary_instruction_from_stack(BinaryOpCode::InterpolateAtOffset);
    }
    pub fn built_in_atomiccounter(&mut self) {
        self.built_in_unary_instruction_from_stack_with_pointer(UnaryOpCode::AtomicCounter);
    }
    pub fn built_in_atomiccounterincrement(&mut self) {
        self.built_in_unary_instruction_from_stack_with_pointer(
            UnaryOpCode::AtomicCounterIncrement,
        );
    }
    pub fn built_in_atomiccounterdecrement(&mut self) {
        self.built_in_unary_instruction_from_stack_with_pointer(
            UnaryOpCode::AtomicCounterDecrement,
        );
    }
    pub fn built_in_atomicadd(&mut self) {
        // atomicAdd's first argument is a pointer.
        self.built_in_binary_instruction_from_stack_with_pointer_lhs(BinaryOpCode::AtomicAdd);
    }
    pub fn built_in_atomicmin(&mut self) {
        // atomicMin's first argument is a pointer.
        self.built_in_binary_instruction_from_stack_with_pointer_lhs(BinaryOpCode::AtomicMin);
    }
    pub fn built_in_atomicmax(&mut self) {
        // atomicMax's first argument is a pointer.
        self.built_in_binary_instruction_from_stack_with_pointer_lhs(BinaryOpCode::AtomicMax);
    }
    pub fn built_in_atomicand(&mut self) {
        // atomicAnd's first argument is a pointer.
        self.built_in_binary_instruction_from_stack_with_pointer_lhs(BinaryOpCode::AtomicAnd);
    }
    pub fn built_in_atomicor(&mut self) {
        // atomicOr's first argument is a pointer.
        self.built_in_binary_instruction_from_stack_with_pointer_lhs(BinaryOpCode::AtomicOr);
    }
    pub fn built_in_atomicxor(&mut self) {
        // atomicXor's first argument is a pointer.
        self.built_in_binary_instruction_from_stack_with_pointer_lhs(BinaryOpCode::AtomicXor);
    }
    pub fn built_in_atomicexchange(&mut self) {
        // atomicExchange's first argument is a pointer.
        self.built_in_binary_instruction_from_stack_with_pointer_lhs(BinaryOpCode::AtomicExchange);
    }
    pub fn built_in_atomiccompswap(&mut self) {
        // atomicCompSwap's first argument is a pointer.
        self.built_in_instruction_from_stack_with_pointer_args(
            BuiltInOpCode::AtomicCompSwap,
            1,
            2,
            0,
        );
    }
    pub fn built_in_imagesize(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::ImageSize);
    }
    pub fn built_in_imagestore(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::ImageStore, 3);
    }
    pub fn built_in_imageload(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::ImageLoad, 2);
    }
    pub fn built_in_imageatomicadd(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::ImageAtomicAdd, 3);
    }
    pub fn built_in_imageatomicmin(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::ImageAtomicMin, 3);
    }
    pub fn built_in_imageatomicmax(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::ImageAtomicMax, 3);
    }
    pub fn built_in_imageatomicand(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::ImageAtomicAnd, 3);
    }
    pub fn built_in_imageatomicor(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::ImageAtomicOr, 3);
    }
    pub fn built_in_imageatomicxor(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::ImageAtomicXor, 3);
    }
    pub fn built_in_imageatomicexchange(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::ImageAtomicExchange, 3);
    }
    pub fn built_in_imageatomiccompswap(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::ImageAtomicCompSwap, 4);
    }
    pub fn built_in_pixellocalloadangle(&mut self) {
        self.built_in_unary_instruction_from_stack(UnaryOpCode::PixelLocalLoadANGLE);
    }
    pub fn built_in_pixellocalstoreangle(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::PixelLocalStoreANGLE, 2);
    }
    pub fn built_in_memorybarrier(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::MemoryBarrier, 0);
    }
    pub fn built_in_memorybarrieratomiccounter(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::MemoryBarrierAtomicCounter, 0);
    }
    pub fn built_in_memorybarrierbuffer(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::MemoryBarrierBuffer, 0);
    }
    pub fn built_in_memorybarrierimage(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::MemoryBarrierImage, 0);
    }
    pub fn built_in_barrier(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::Barrier, 0);
    }
    pub fn built_in_memorybarriershared(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::MemoryBarrierShared, 0);
    }
    pub fn built_in_groupmemorybarrier(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::GroupMemoryBarrier, 0);
    }
    pub fn built_in_emitvertex(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::EmitVertex, 0);
    }
    pub fn built_in_endprimitive(&mut self) {
        self.built_in_instruction_from_stack(BuiltInOpCode::EndPrimitive, 0);
    }

    // A generic helper to make a texture* built-in operator.  The coord operand of the texture
    // built-in is found at the top of the stack, and the sampler operand is found under it.
    // Any other operand is expected to be already popped, and passed in with `texture_op`.
    fn built_in_texture_instruction_from_stack(&mut self, texture_op: TextureOpCode) {
        // Load the operands.
        let coord = self.load();
        let sampler = self.load();

        let result = instruction::built_in_texture(&mut self.ir.meta, texture_op, sampler, coord);
        self.add_instruction(result);
    }

    // Built-in texture* operations
    pub fn built_in_texture(&mut self, with_compare: bool) {
        let texture_op = if with_compare {
            let compare = self.load();
            TextureOpCode::Compare { compare }
        } else {
            TextureOpCode::Implicit { is_proj: false, offset: None }
        };
        self.built_in_texture_instruction_from_stack(texture_op);
    }
    pub fn built_in_textureproj(&mut self) {
        self.built_in_texture_instruction_from_stack(TextureOpCode::Implicit {
            is_proj: true,
            offset: None,
        });
    }
    pub fn built_in_texturelod(&mut self, with_compare: bool) {
        let lod = self.load();
        let texture_op = if with_compare {
            let compare = self.load();
            TextureOpCode::CompareLod { compare, lod }
        } else {
            TextureOpCode::Lod { is_proj: false, lod, offset: None }
        };
        self.built_in_texture_instruction_from_stack(texture_op);
    }
    pub fn built_in_textureprojlod(&mut self) {
        let lod = self.load();
        self.built_in_texture_instruction_from_stack(TextureOpCode::Lod {
            is_proj: true,
            lod,
            offset: None,
        });
    }
    pub fn built_in_texturebias(&mut self, with_compare: bool) {
        let bias = self.load();
        let texture_op = if with_compare {
            let compare = self.load();
            TextureOpCode::CompareBias { compare, bias }
        } else {
            TextureOpCode::Bias { is_proj: false, bias, offset: None }
        };

        self.built_in_texture_instruction_from_stack(texture_op);
    }
    pub fn built_in_textureprojbias(&mut self) {
        let bias = self.load();
        self.built_in_texture_instruction_from_stack(TextureOpCode::Bias {
            is_proj: true,
            bias,
            offset: None,
        });
    }
    pub fn built_in_textureoffset(&mut self) {
        let offset = self.load();
        self.built_in_texture_instruction_from_stack(TextureOpCode::Implicit {
            is_proj: false,
            offset: Some(offset),
        });
    }
    pub fn built_in_textureprojoffset(&mut self) {
        let offset = self.load();
        self.built_in_texture_instruction_from_stack(TextureOpCode::Implicit {
            is_proj: true,
            offset: Some(offset),
        });
    }
    pub fn built_in_texturelodoffset(&mut self) {
        let offset = self.load();
        let lod = self.load();
        self.built_in_texture_instruction_from_stack(TextureOpCode::Lod {
            is_proj: false,
            lod,
            offset: Some(offset),
        });
    }
    pub fn built_in_textureprojlodoffset(&mut self) {
        let offset = self.load();
        let lod = self.load();
        self.built_in_texture_instruction_from_stack(TextureOpCode::Lod {
            is_proj: true,
            lod,
            offset: Some(offset),
        });
    }
    pub fn built_in_textureoffsetbias(&mut self) {
        let bias = self.load();
        let offset = self.load();
        self.built_in_texture_instruction_from_stack(TextureOpCode::Bias {
            is_proj: false,
            bias,
            offset: Some(offset),
        });
    }
    pub fn built_in_textureprojoffsetbias(&mut self) {
        let bias = self.load();
        let offset = self.load();
        self.built_in_texture_instruction_from_stack(TextureOpCode::Bias {
            is_proj: true,
            bias,
            offset: Some(offset),
        });
    }
    pub fn built_in_texturegrad(&mut self) {
        let dy = self.load();
        let dx = self.load();
        self.built_in_texture_instruction_from_stack(TextureOpCode::Grad {
            is_proj: false,
            dx,
            dy,
            offset: None,
        });
    }
    pub fn built_in_textureprojgrad(&mut self) {
        let dy = self.load();
        let dx = self.load();
        self.built_in_texture_instruction_from_stack(TextureOpCode::Grad {
            is_proj: true,
            dx,
            dy,
            offset: None,
        });
    }
    pub fn built_in_texturegradoffset(&mut self) {
        let offset = self.load();
        let dy = self.load();
        let dx = self.load();
        self.built_in_texture_instruction_from_stack(TextureOpCode::Grad {
            is_proj: false,
            dx,
            dy,
            offset: Some(offset),
        });
    }
    pub fn built_in_textureprojgradoffset(&mut self) {
        let offset = self.load();
        let dy = self.load();
        let dx = self.load();
        self.built_in_texture_instruction_from_stack(TextureOpCode::Grad {
            is_proj: true,
            dx,
            dy,
            offset: Some(offset),
        });
    }
    pub fn built_in_texturegather(&mut self) {
        self.built_in_texture_instruction_from_stack(TextureOpCode::Gather { offset: None });
    }
    pub fn built_in_texturegathercomp(&mut self) {
        let component = self.load();
        self.built_in_texture_instruction_from_stack(TextureOpCode::GatherComponent {
            component,
            offset: None,
        });
    }
    pub fn built_in_texturegatherref(&mut self) {
        let refz = self.load();
        self.built_in_texture_instruction_from_stack(TextureOpCode::GatherRef {
            refz,
            offset: None,
        });
    }
    pub fn built_in_texturegatheroffset(&mut self) {
        let offset = self.load();
        self.built_in_texture_instruction_from_stack(TextureOpCode::Gather {
            offset: Some(offset),
        });
    }
    pub fn built_in_texturegatheroffsetcomp(&mut self) {
        let component = self.load();
        let offset = self.load();
        self.built_in_texture_instruction_from_stack(TextureOpCode::GatherComponent {
            component,
            offset: Some(offset),
        });
    }
    pub fn built_in_texturegatheroffsetref(&mut self) {
        let offset = self.load();
        let refz = self.load();
        self.built_in_texture_instruction_from_stack(TextureOpCode::GatherRef {
            refz,
            offset: Some(offset),
        });
    }
}

#[cxx::bridge(namespace = "sh::ir::ffi")]
pub mod ffi {
    // Flags controlling the way the IR is built, applying transformations during IR generation.
    struct BuildOptions {
        // Whether uninitialized local and global variables should be zero-initialized.
        initialize_uninitialized_variables: bool,
        // Whether non-const global variables are allowed to have an initializer.
        initializer_allowed_on_non_const_global_variables: bool,
        // Whether output variables should be zero-initialized.
        initialize_output_variables: bool,
        // Whether gl_Position should be zero-initialized.
        initialize_gl_position: bool,
    }

    // The following enums and types must be identical to what's found in BaseTypes.h.  This
    // duplication is not ideal, but necessary during the transition to IR.  Once the translator
    // switches over to IR completely, it can directly use the types exported from here (or better
    // yet, switch to more IR friendly types) and remove them from BaseTypes.h.
    #[derive(Copy, Clone)]
    #[repr(u32)]
    enum ASTPrecision {
        Undefined,
        Low,
        Medium,
        High,
    }

    #[derive(Copy, Clone)]
    #[repr(u32)]
    enum ASTBasicType {
        Void,
        Float,
        Int,
        UInt,
        Bool,
        AtomicCounter,
        YuvCscStandardEXT,
        Sampler2D,
        Sampler3D,
        SamplerCube,
        Sampler2DArray,
        SamplerExternalOES,
        SamplerExternal2DY2YEXT,
        Sampler2DRect,
        Sampler2DMS,
        Sampler2DMSArray,
        ISampler2D,
        ISampler3D,
        ISamplerCube,
        ISampler2DArray,
        ISampler2DMS,
        ISampler2DMSArray,
        USampler2D,
        USampler3D,
        USamplerCube,
        USampler2DArray,
        USampler2DMS,
        USampler2DMSArray,
        Sampler2DShadow,
        SamplerCubeShadow,
        Sampler2DArrayShadow,
        SamplerBuffer,
        SamplerCubeArray,
        SamplerCubeArrayShadow,
        ISampler2DRect,
        ISamplerBuffer,
        ISamplerCubeArray,
        USampler2DRect,
        USamplerBuffer,
        USamplerCubeArray,
        SamplerVideoWEBGL,
        Image2D,
        Image3D,
        Image2DArray,
        ImageCube,
        ImageCubeArray,
        ImageBuffer,
        IImage2D,
        IImage3D,
        IImage2DArray,
        IImageCube,
        IImageCubeArray,
        IImageBuffer,
        UImage2D,
        UImage3D,
        UImage2DArray,
        UImageCube,
        UImageCubeArray,
        UImageBuffer,
        PixelLocalANGLE,
        IPixelLocalANGLE,
        UPixelLocalANGLE,
        SubpassInput,
        ISubpassInput,
        USubpassInput,
        Struct,
        InterfaceBlock,
    }

    #[derive(Copy, Clone)]
    #[repr(u32)]
    enum ASTQualifier {
        Temporary,
        Global,
        Const,
        Attribute,
        VaryingIn,
        VaryingOut,
        Uniform,
        Buffer,
        Patch,
        VertexIn,
        FragmentOut,
        VertexOut,
        FragmentIn,
        FragmentInOut,
        ParamIn,
        ParamOut,
        ParamInOut,
        ParamConst,
        InstanceID,
        VertexID,
        Position,
        PointSize,
        BaseVertex,
        BaseInstance,
        DrawID,
        FragCoord,
        FrontFacing,
        PointCoord,
        HelperInvocation,
        FragColor,
        FragData,
        FragDepth,
        SecondaryFragColorEXT,
        SecondaryFragDataEXT,
        ViewIDOVR,
        ClipDistance,
        CullDistance,
        LastFragColor,
        LastFragData,
        LastFragDepth,
        LastFragStencil,
        DepthRange,
        Smooth,
        Flat,
        NoPerspective,
        Centroid,
        Sample,
        NoPerspectiveCentroid,
        NoPerspectiveSample,
        SmoothOut,
        FlatOut,
        NoPerspectiveOut,
        CentroidOut,
        SampleOut,
        NoPerspectiveCentroidOut,
        NoPerspectiveSampleOut,
        SmoothIn,
        FlatIn,
        NoPerspectiveIn,
        CentroidIn,
        SampleIn,
        NoPerspectiveCentroidIn,
        NoPerspectiveSampleIn,
        ShadingRateEXT,
        PrimitiveShadingRateEXT,
        SampleID,
        SamplePosition,
        SampleMaskIn,
        SampleMask,
        NumSamples,
        Shared,
        ComputeIn,
        NumWorkGroups,
        WorkGroupSize,
        WorkGroupID,
        LocalInvocationID,
        GlobalInvocationID,
        LocalInvocationIndex,
        ReadOnly,
        WriteOnly,
        Coherent,
        Restrict,
        Volatile,
        GeometryIn,
        GeometryOut,
        PerVertexIn,
        PrimitiveIDIn,
        InvocationID,
        PrimitiveID,
        LayerOut,
        LayerIn,
        PatchIn,
        PatchOut,
        TessControlIn,
        TessControlOut,
        PerVertexOut,
        PatchVerticesIn,
        TessLevelOuter,
        TessLevelInner,
        BoundingBox,
        TessEvaluationIn,
        TessEvaluationOut,
        TessCoord,
        SpecConst,
        PixelLocalEXT,
    }

    #[derive(Copy, Clone)]
    #[repr(u32)]
    enum ASTLayoutImageInternalFormat {
        Unspecified,
        RGBA32F,
        RGBA16F,
        R32F,
        RGBA32UI,
        RGBA16UI,
        RGBA8UI,
        R32UI,
        RGBA32I,
        RGBA16I,
        RGBA8I,
        R32I,
        RGBA8,
        RGBA8SNORM,
    }

    #[derive(Copy, Clone)]
    #[repr(u32)]
    enum ASTLayoutMatrixPacking {
        Unspecified,
        RowMajor,
        ColumnMajor,
    }

    #[derive(Copy, Clone)]
    #[repr(u32)]
    enum ASTLayoutBlockStorage {
        Unspecified,
        Shared,
        Packed,
        Std140,
        Std430,
    }

    #[derive(Copy, Clone)]
    #[repr(u32)]
    enum ASTLayoutDepth {
        Unspecified,
        Any,
        Greater,
        Less,
        Unchanged,
    }

    #[derive(Copy, Clone)]
    #[repr(u32)]
    enum ASTYuvCscStandardEXT {
        Undefined,
        Itu601,
        Itu601FullRange,
        Itu709,
    }

    #[derive(Copy, Clone)]
    #[repr(u32)]
    enum ASTLayoutPrimitiveType {
        Undefined,
        Points,
        Lines,
        LinesAdjacency,
        Triangles,
        TrianglesAdjacency,
        LineStrip,
        TriangleStrip,
    }

    #[derive(Copy, Clone)]
    #[repr(u32)]
    enum ASTLayoutTessEvaluationType {
        Undefined,
        Triangles,
        Quads,
        Isolines,
        EqualSpacing,
        FractionalEvenSpacing,
        FractionalOddSpacing,
        Cw,
        Ccw,
        PointMode,
    }

    #[derive(Copy, Clone)]
    #[repr(u32)]
    enum ASTShaderType {
        Vertex,
        TessControl,
        TessEvaluation,
        Geometry,
        Fragment,
        Compute,
    }

    // The AST structs don't have to be identical to TLayoutQualifier/etc, it's too risky to assume
    // identical layout so the fields are copied from TLayoutQualifier/etc instead.
    struct ASTLayoutQualifier {
        location: i32,
        matrix_packing: ASTLayoutMatrixPacking,
        block_storage: ASTLayoutBlockStorage,
        binding: i32,
        offset: i32,
        depth: ASTLayoutDepth,
        image_internal_format: ASTLayoutImageInternalFormat,
        num_views: i32,
        yuv: bool,
        index: i32,
        noncoherent: bool,

        // The following fields are only needed while output/legacy.rs is needed, as they may be
        // set in the IR and need to translate back to AST, but can never be set when parsing GLSL.
        push_constant: bool,
        input_attachment_index: i32,
        raster_ordered: bool,
    }

    struct ASTMemoryQualifier {
        readonly: bool,
        writeonly: bool,
        coherent: bool,
        restrict_qualifier: bool,
        volatile_qualifier: bool,
    }

    struct ASTType {
        type_id: TypeId,
        qualifier: ASTQualifier,
        precision: ASTPrecision,
        layout_qualifier: ASTLayoutQualifier,
        memory_qualifier: ASTMemoryQualifier,
        invariant: bool,
        precise: bool,
        interpolant: bool,
    }

    struct ASTField {
        name: &'static str,
        ast_type: ASTType,
    }

    struct ASTStruct<'a> {
        name: &'static str,
        fields: &'a [ASTField],
        is_interface_block: bool,
        is_internal: bool,
        is_at_global_scope: bool,
    }

    // IR ids that the parser would have to know about.
    #[derive(Copy, Clone)]
    struct TypeId {
        id: u32,
    }
    #[derive(Copy, Clone)]
    struct FunctionId {
        id: u32,
    }
    #[derive(Copy, Clone)]
    struct VariableId {
        id: u32,
    }
    #[derive(Copy, Clone)]
    struct ConstantId {
        id: u32,
    }

    extern "Rust" {
        type BuilderWrapper;
        #[derive(ExternType)]
        type IR;

        fn builder_new(shader_type: ASTShaderType, options: BuildOptions) -> Box<BuilderWrapper>;
        fn builder_finish(mut builder: Box<BuilderWrapper>) -> Box<IR>;
        fn builder_fail(mut builder: Box<BuilderWrapper>) -> Box<IR>;

        // Helpers to get a TypeId out of a TType.
        fn get_basic_type_id(
            self: &mut BuilderWrapper,
            basic_type: ASTBasicType,
            primary_size: u32,
            secondary_size: u32,
        ) -> TypeId;
        fn get_struct_type_id(self: &mut BuilderWrapper, struct_info: &ASTStruct) -> TypeId;
        fn get_array_type_id(
            self: &mut BuilderWrapper,
            element_type_id: TypeId,
            array_sizes: &[u32],
        ) -> TypeId;

        // Helpers to set global metadata.
        fn set_early_fragment_tests(self: &mut BuilderWrapper, value: bool);
        fn set_advanced_blend_equations(self: &mut BuilderWrapper, value: u32);
        fn set_tcs_vertices(self: &mut BuilderWrapper, value: u32);
        fn set_tes_primitive(self: &mut BuilderWrapper, value: ASTLayoutTessEvaluationType);
        fn set_tes_vertex_spacing(self: &mut BuilderWrapper, value: ASTLayoutTessEvaluationType);
        fn set_tes_ordering(self: &mut BuilderWrapper, value: ASTLayoutTessEvaluationType);
        fn set_tes_point_mode(self: &mut BuilderWrapper, value: ASTLayoutTessEvaluationType);
        fn set_gs_primitive_in(self: &mut BuilderWrapper, value: ASTLayoutPrimitiveType);
        fn set_gs_primitive_out(self: &mut BuilderWrapper, value: ASTLayoutPrimitiveType);
        fn set_gs_invocations(self: &mut BuilderWrapper, value: u32);
        fn set_gs_max_vertices(self: &mut BuilderWrapper, value: u32);

        fn declare_interface_variable(
            self: &mut BuilderWrapper,
            name: &'static str,
            ast_type: &ASTType,
            is_declaration_internal: bool,
        ) -> VariableId;
        fn declare_temp_variable(
            self: &mut BuilderWrapper,
            name: &'static str,
            ast_type: &ASTType,
        ) -> VariableId;
        fn mark_variable_invariant(self: &mut BuilderWrapper, variable_id: VariableId);
        fn mark_variable_precise(self: &mut BuilderWrapper, variable_id: VariableId);
        fn new_function(
            self: &mut BuilderWrapper,
            name: &'static str,
            params: &[VariableId],
            param_directions: &[ASTQualifier],
            return_type_id: TypeId,
            return_ast_type: &ASTType,
        ) -> FunctionId;
        fn update_function_param_names(
            self: &mut BuilderWrapper,
            id: FunctionId,
            param_names: &[&'static str],
            param_ids: &mut [VariableId],
        );
        fn declare_function_param(
            self: &mut BuilderWrapper,
            name: &'static str,
            type_id: TypeId,
            ast_type: &ASTType,
            direction: ASTQualifier,
        ) -> VariableId;
        fn begin_function(self: &mut BuilderWrapper, id: FunctionId);
        fn end_function(self: &mut BuilderWrapper);
        fn store(self: &mut BuilderWrapper);
        fn initialize(self: &mut BuilderWrapper, id: VariableId);
        fn begin_if_true_block(self: &mut BuilderWrapper);
        fn end_if_true_block(self: &mut BuilderWrapper);
        fn begin_if_false_block(self: &mut BuilderWrapper);
        fn end_if_false_block(self: &mut BuilderWrapper);
        fn end_if(self: &mut BuilderWrapper);
        fn begin_ternary_true_expression(self: &mut BuilderWrapper);
        fn end_ternary_true_expression(self: &mut BuilderWrapper, is_void: bool);
        fn begin_ternary_false_expression(self: &mut BuilderWrapper);
        fn end_ternary_false_expression(self: &mut BuilderWrapper, is_void: bool);
        fn end_ternary(self: &mut BuilderWrapper, is_void: bool);
        fn begin_short_circuit_or(self: &mut BuilderWrapper);
        fn end_short_circuit_or(self: &mut BuilderWrapper);
        fn begin_short_circuit_and(self: &mut BuilderWrapper);
        fn end_short_circuit_and(self: &mut BuilderWrapper);
        fn begin_loop_condition(self: &mut BuilderWrapper);
        fn end_loop_condition(self: &mut BuilderWrapper);
        fn end_loop_continue(self: &mut BuilderWrapper);
        fn end_loop(self: &mut BuilderWrapper);
        fn begin_do_loop(self: &mut BuilderWrapper);
        fn begin_do_loop_condition(self: &mut BuilderWrapper);
        fn end_do_loop(self: &mut BuilderWrapper);
        fn begin_switch(self: &mut BuilderWrapper);
        fn begin_case(self: &mut BuilderWrapper);
        fn begin_default(self: &mut BuilderWrapper);
        fn end_switch(self: &mut BuilderWrapper);
        fn branch_discard(self: &mut BuilderWrapper);
        fn branch_return(self: &mut BuilderWrapper);
        fn branch_return_value(self: &mut BuilderWrapper);
        fn branch_break(self: &mut BuilderWrapper);
        fn branch_continue(self: &mut BuilderWrapper);
        fn pop_array_size(self: &mut BuilderWrapper) -> u32;
        fn end_statement_with_value(self: &mut BuilderWrapper);
        fn push_constant_float(self: &mut BuilderWrapper, value: f32);
        fn push_constant_int(self: &mut BuilderWrapper, value: i32);
        fn push_constant_uint(self: &mut BuilderWrapper, value: u32);
        fn push_constant_bool(self: &mut BuilderWrapper, value: bool);
        fn push_constant_yuv_csc_standard(self: &mut BuilderWrapper, value: ASTYuvCscStandardEXT);
        fn push_variable(self: &mut BuilderWrapper, id: VariableId);
        fn call_function(self: &mut BuilderWrapper, id: FunctionId);
        fn vector_component(self: &mut BuilderWrapper, component: u32);
        fn vector_component_multi(self: &mut BuilderWrapper, components: &[u32]);
        fn index(self: &mut BuilderWrapper);
        fn struct_field(self: &mut BuilderWrapper, field_index: u32);
        fn construct(self: &mut BuilderWrapper, type_id: TypeId, arg_count: usize);
        fn on_gl_clip_distance_sized(self: &mut BuilderWrapper, id: VariableId, length: u32);
        fn on_gl_cull_distance_sized(self: &mut BuilderWrapper, id: VariableId, length: u32);
        fn array_length(self: &mut BuilderWrapper);
        fn negate(self: &mut BuilderWrapper);
        fn postfix_increment(self: &mut BuilderWrapper);
        fn postfix_decrement(self: &mut BuilderWrapper);
        fn prefix_increment(self: &mut BuilderWrapper);
        fn prefix_decrement(self: &mut BuilderWrapper);
        fn add(self: &mut BuilderWrapper);
        fn add_assign(self: &mut BuilderWrapper);
        fn sub(self: &mut BuilderWrapper);
        fn sub_assign(self: &mut BuilderWrapper);
        fn mul(self: &mut BuilderWrapper);
        fn mul_assign(self: &mut BuilderWrapper);
        fn vector_times_scalar(self: &mut BuilderWrapper);
        fn vector_times_scalar_assign(self: &mut BuilderWrapper);
        fn matrix_times_scalar(self: &mut BuilderWrapper);
        fn matrix_times_scalar_assign(self: &mut BuilderWrapper);
        fn vector_times_matrix(self: &mut BuilderWrapper);
        fn vector_times_matrix_assign(self: &mut BuilderWrapper);
        fn matrix_times_vector(self: &mut BuilderWrapper);
        fn matrix_times_matrix(self: &mut BuilderWrapper);
        fn matrix_times_matrix_assign(self: &mut BuilderWrapper);
        fn div(self: &mut BuilderWrapper);
        fn div_assign(self: &mut BuilderWrapper);
        fn imod(self: &mut BuilderWrapper);
        fn imod_assign(self: &mut BuilderWrapper);
        fn logical_not(self: &mut BuilderWrapper);
        fn logical_xor(self: &mut BuilderWrapper);
        fn equal(self: &mut BuilderWrapper);
        fn not_equal(self: &mut BuilderWrapper);
        fn less_than(self: &mut BuilderWrapper);
        fn greater_than(self: &mut BuilderWrapper);
        fn less_than_equal(self: &mut BuilderWrapper);
        fn greater_than_equal(self: &mut BuilderWrapper);
        fn bitwise_not(self: &mut BuilderWrapper);
        fn bit_shift_left(self: &mut BuilderWrapper);
        fn bit_shift_left_assign(self: &mut BuilderWrapper);
        fn bit_shift_right(self: &mut BuilderWrapper);
        fn bit_shift_right_assign(self: &mut BuilderWrapper);
        fn bitwise_or(self: &mut BuilderWrapper);
        fn bitwise_or_assign(self: &mut BuilderWrapper);
        fn bitwise_xor(self: &mut BuilderWrapper);
        fn bitwise_xor_assign(self: &mut BuilderWrapper);
        fn bitwise_and(self: &mut BuilderWrapper);
        fn bitwise_and_assign(self: &mut BuilderWrapper);
        fn built_in_radians(self: &mut BuilderWrapper);
        fn built_in_degrees(self: &mut BuilderWrapper);
        fn built_in_sin(self: &mut BuilderWrapper);
        fn built_in_cos(self: &mut BuilderWrapper);
        fn built_in_tan(self: &mut BuilderWrapper);
        fn built_in_asin(self: &mut BuilderWrapper);
        fn built_in_acos(self: &mut BuilderWrapper);
        fn built_in_atan(self: &mut BuilderWrapper);
        fn built_in_sinh(self: &mut BuilderWrapper);
        fn built_in_cosh(self: &mut BuilderWrapper);
        fn built_in_tanh(self: &mut BuilderWrapper);
        fn built_in_asinh(self: &mut BuilderWrapper);
        fn built_in_acosh(self: &mut BuilderWrapper);
        fn built_in_atanh(self: &mut BuilderWrapper);
        fn built_in_atan_binary(self: &mut BuilderWrapper);
        fn built_in_pow(self: &mut BuilderWrapper);
        fn built_in_exp(self: &mut BuilderWrapper);
        fn built_in_log(self: &mut BuilderWrapper);
        fn built_in_exp2(self: &mut BuilderWrapper);
        fn built_in_log2(self: &mut BuilderWrapper);
        fn built_in_sqrt(self: &mut BuilderWrapper);
        fn built_in_inversesqrt(self: &mut BuilderWrapper);
        fn built_in_abs(self: &mut BuilderWrapper);
        fn built_in_sign(self: &mut BuilderWrapper);
        fn built_in_floor(self: &mut BuilderWrapper);
        fn built_in_trunc(self: &mut BuilderWrapper);
        fn built_in_round(self: &mut BuilderWrapper);
        fn built_in_roundeven(self: &mut BuilderWrapper);
        fn built_in_ceil(self: &mut BuilderWrapper);
        fn built_in_fract(self: &mut BuilderWrapper);
        fn built_in_mod(self: &mut BuilderWrapper);
        fn built_in_min(self: &mut BuilderWrapper);
        fn built_in_max(self: &mut BuilderWrapper);
        fn built_in_clamp(self: &mut BuilderWrapper);
        fn built_in_mix(self: &mut BuilderWrapper);
        fn built_in_step(self: &mut BuilderWrapper);
        fn built_in_smoothstep(self: &mut BuilderWrapper);
        fn built_in_modf(self: &mut BuilderWrapper);
        fn built_in_isnan(self: &mut BuilderWrapper);
        fn built_in_isinf(self: &mut BuilderWrapper);
        fn built_in_floatbitstoint(self: &mut BuilderWrapper);
        fn built_in_floatbitstouint(self: &mut BuilderWrapper);
        fn built_in_intbitstofloat(self: &mut BuilderWrapper);
        fn built_in_uintbitstofloat(self: &mut BuilderWrapper);
        fn built_in_fma(self: &mut BuilderWrapper);
        fn built_in_frexp(self: &mut BuilderWrapper);
        fn built_in_ldexp(self: &mut BuilderWrapper);
        fn built_in_packsnorm2x16(self: &mut BuilderWrapper);
        fn built_in_packhalf2x16(self: &mut BuilderWrapper);
        fn built_in_unpacksnorm2x16(self: &mut BuilderWrapper);
        fn built_in_unpackhalf2x16(self: &mut BuilderWrapper);
        fn built_in_packunorm2x16(self: &mut BuilderWrapper);
        fn built_in_unpackunorm2x16(self: &mut BuilderWrapper);
        fn built_in_packunorm4x8(self: &mut BuilderWrapper);
        fn built_in_packsnorm4x8(self: &mut BuilderWrapper);
        fn built_in_unpackunorm4x8(self: &mut BuilderWrapper);
        fn built_in_unpacksnorm4x8(self: &mut BuilderWrapper);
        fn built_in_length(self: &mut BuilderWrapper);
        fn built_in_distance(self: &mut BuilderWrapper);
        fn built_in_dot(self: &mut BuilderWrapper);
        fn built_in_cross(self: &mut BuilderWrapper);
        fn built_in_normalize(self: &mut BuilderWrapper);
        fn built_in_faceforward(self: &mut BuilderWrapper);
        fn built_in_reflect(self: &mut BuilderWrapper);
        fn built_in_refract(self: &mut BuilderWrapper);
        fn built_in_matrixcompmult(self: &mut BuilderWrapper);
        fn built_in_outerproduct(self: &mut BuilderWrapper);
        fn built_in_transpose(self: &mut BuilderWrapper);
        fn built_in_determinant(self: &mut BuilderWrapper);
        fn built_in_inverse(self: &mut BuilderWrapper);
        fn built_in_lessthan(self: &mut BuilderWrapper);
        fn built_in_lessthanequal(self: &mut BuilderWrapper);
        fn built_in_greaterthan(self: &mut BuilderWrapper);
        fn built_in_greaterthanequal(self: &mut BuilderWrapper);
        fn built_in_equal(self: &mut BuilderWrapper);
        fn built_in_notequal(self: &mut BuilderWrapper);
        fn built_in_any(self: &mut BuilderWrapper);
        fn built_in_all(self: &mut BuilderWrapper);
        fn built_in_not(self: &mut BuilderWrapper);
        fn built_in_bitfieldextract(self: &mut BuilderWrapper);
        fn built_in_bitfieldinsert(self: &mut BuilderWrapper);
        fn built_in_bitfieldreverse(self: &mut BuilderWrapper);
        fn built_in_bitcount(self: &mut BuilderWrapper);
        fn built_in_findlsb(self: &mut BuilderWrapper);
        fn built_in_findmsb(self: &mut BuilderWrapper);
        fn built_in_uaddcarry(self: &mut BuilderWrapper);
        fn built_in_usubborrow(self: &mut BuilderWrapper);
        fn built_in_umulextended(self: &mut BuilderWrapper);
        fn built_in_imulextended(self: &mut BuilderWrapper);
        fn built_in_texturesize(self: &mut BuilderWrapper, with_lod: bool);
        fn built_in_texturequerylod(self: &mut BuilderWrapper);
        fn built_in_texelfetch(self: &mut BuilderWrapper, with_lod_or_sample: bool);
        fn built_in_texelfetchoffset(self: &mut BuilderWrapper);
        fn built_in_rgb_2_yuv(self: &mut BuilderWrapper);
        fn built_in_yuv_2_rgb(self: &mut BuilderWrapper);
        fn built_in_dfdx(self: &mut BuilderWrapper);
        fn built_in_dfdy(self: &mut BuilderWrapper);
        fn built_in_fwidth(self: &mut BuilderWrapper);
        fn built_in_interpolateatcentroid(self: &mut BuilderWrapper);
        fn built_in_interpolateatsample(self: &mut BuilderWrapper);
        fn built_in_interpolateatoffset(self: &mut BuilderWrapper);
        fn built_in_atomiccounter(self: &mut BuilderWrapper);
        fn built_in_atomiccounterincrement(self: &mut BuilderWrapper);
        fn built_in_atomiccounterdecrement(self: &mut BuilderWrapper);
        fn built_in_atomicadd(self: &mut BuilderWrapper);
        fn built_in_atomicmin(self: &mut BuilderWrapper);
        fn built_in_atomicmax(self: &mut BuilderWrapper);
        fn built_in_atomicand(self: &mut BuilderWrapper);
        fn built_in_atomicor(self: &mut BuilderWrapper);
        fn built_in_atomicxor(self: &mut BuilderWrapper);
        fn built_in_atomicexchange(self: &mut BuilderWrapper);
        fn built_in_atomiccompswap(self: &mut BuilderWrapper);
        fn built_in_imagesize(self: &mut BuilderWrapper);
        fn built_in_imagestore(self: &mut BuilderWrapper);
        fn built_in_imageload(self: &mut BuilderWrapper);
        fn built_in_imageatomicadd(self: &mut BuilderWrapper);
        fn built_in_imageatomicmin(self: &mut BuilderWrapper);
        fn built_in_imageatomicmax(self: &mut BuilderWrapper);
        fn built_in_imageatomicand(self: &mut BuilderWrapper);
        fn built_in_imageatomicor(self: &mut BuilderWrapper);
        fn built_in_imageatomicxor(self: &mut BuilderWrapper);
        fn built_in_imageatomicexchange(self: &mut BuilderWrapper);
        fn built_in_imageatomiccompswap(self: &mut BuilderWrapper);
        fn built_in_pixellocalloadangle(self: &mut BuilderWrapper);
        fn built_in_pixellocalstoreangle(self: &mut BuilderWrapper);
        fn built_in_memorybarrier(self: &mut BuilderWrapper);
        fn built_in_memorybarrieratomiccounter(self: &mut BuilderWrapper);
        fn built_in_memorybarrierbuffer(self: &mut BuilderWrapper);
        fn built_in_memorybarrierimage(self: &mut BuilderWrapper);
        fn built_in_barrier(self: &mut BuilderWrapper);
        fn built_in_memorybarriershared(self: &mut BuilderWrapper);
        fn built_in_groupmemorybarrier(self: &mut BuilderWrapper);
        fn built_in_emitvertex(self: &mut BuilderWrapper);
        fn built_in_endprimitive(self: &mut BuilderWrapper);
        fn built_in_texture(self: &mut BuilderWrapper, with_compare: bool);
        fn built_in_textureproj(self: &mut BuilderWrapper);
        fn built_in_texturelod(self: &mut BuilderWrapper, with_compare: bool);
        fn built_in_textureprojlod(self: &mut BuilderWrapper);
        fn built_in_texturebias(self: &mut BuilderWrapper, with_compare: bool);
        fn built_in_textureprojbias(self: &mut BuilderWrapper);
        fn built_in_textureoffset(self: &mut BuilderWrapper);
        fn built_in_textureprojoffset(self: &mut BuilderWrapper);
        fn built_in_texturelodoffset(self: &mut BuilderWrapper);
        fn built_in_textureprojlodoffset(self: &mut BuilderWrapper);
        fn built_in_textureoffsetbias(self: &mut BuilderWrapper);
        fn built_in_textureprojoffsetbias(self: &mut BuilderWrapper);
        fn built_in_texturegrad(self: &mut BuilderWrapper);
        fn built_in_textureprojgrad(self: &mut BuilderWrapper);
        fn built_in_texturegradoffset(self: &mut BuilderWrapper);
        fn built_in_textureprojgradoffset(self: &mut BuilderWrapper);
        fn built_in_texturegather(self: &mut BuilderWrapper);
        fn built_in_texturegathercomp(self: &mut BuilderWrapper);
        fn built_in_texturegatherref(self: &mut BuilderWrapper);
        fn built_in_texturegatheroffset(self: &mut BuilderWrapper);
        fn built_in_texturegatheroffsetcomp(self: &mut BuilderWrapper);
        fn built_in_texturegatheroffsetref(self: &mut BuilderWrapper);
    }
}

pub use ffi::BuildOptions as Options;

impl From<TypeId> for ffi::TypeId {
    fn from(id: TypeId) -> Self {
        ffi::TypeId { id: id.id }
    }
}

impl From<ffi::TypeId> for TypeId {
    fn from(id: ffi::TypeId) -> Self {
        TypeId { id: id.id }
    }
}

impl From<ConstantId> for ffi::ConstantId {
    fn from(id: ConstantId) -> Self {
        ffi::ConstantId { id: id.id }
    }
}

impl From<ffi::ConstantId> for ConstantId {
    fn from(id: ffi::ConstantId) -> Self {
        ConstantId { id: id.id }
    }
}

impl From<VariableId> for ffi::VariableId {
    fn from(id: VariableId) -> Self {
        ffi::VariableId { id: id.id }
    }
}

impl From<ffi::VariableId> for VariableId {
    fn from(id: ffi::VariableId) -> Self {
        VariableId { id: id.id }
    }
}

impl From<FunctionId> for ffi::FunctionId {
    fn from(id: FunctionId) -> Self {
        ffi::FunctionId { id: id.id }
    }
}

impl From<ffi::FunctionId> for FunctionId {
    fn from(id: ffi::FunctionId) -> Self {
        FunctionId { id: id.id }
    }
}

impl From<ffi::ASTPrecision> for Precision {
    fn from(precision: ffi::ASTPrecision) -> Self {
        match precision {
            ffi::ASTPrecision::Low => Precision::Low,
            ffi::ASTPrecision::Medium => Precision::Medium,
            ffi::ASTPrecision::High => Precision::High,
            _ => Precision::NotApplicable,
        }
    }
}

impl From<ffi::ASTYuvCscStandardEXT> for YuvCscStandard {
    fn from(value: ffi::ASTYuvCscStandardEXT) -> Self {
        match value {
            ffi::ASTYuvCscStandardEXT::Itu601 => YuvCscStandard::Itu601,
            ffi::ASTYuvCscStandardEXT::Itu601FullRange => YuvCscStandard::Itu601FullRange,
            ffi::ASTYuvCscStandardEXT::Itu709 => YuvCscStandard::Itu709,
            _ => panic!("Internal error: Impossible yuv csc standard value"),
        }
    }
}

impl From<ffi::ASTLayoutTessEvaluationType> for TessellationPrimitive {
    fn from(value: ffi::ASTLayoutTessEvaluationType) -> Self {
        match value {
            ffi::ASTLayoutTessEvaluationType::Undefined => TessellationPrimitive::Undefined,
            ffi::ASTLayoutTessEvaluationType::Triangles => TessellationPrimitive::Triangles,
            ffi::ASTLayoutTessEvaluationType::Quads => TessellationPrimitive::Quads,
            ffi::ASTLayoutTessEvaluationType::Isolines => TessellationPrimitive::Isolines,
            _ => panic!("Internal error: Unexpected tessellation evaluation layout"),
        }
    }
}

impl From<ffi::ASTLayoutTessEvaluationType> for TessellationSpacing {
    fn from(value: ffi::ASTLayoutTessEvaluationType) -> Self {
        match value {
            ffi::ASTLayoutTessEvaluationType::Undefined => TessellationSpacing::Undefined,
            ffi::ASTLayoutTessEvaluationType::EqualSpacing => TessellationSpacing::EqualSpacing,
            ffi::ASTLayoutTessEvaluationType::FractionalEvenSpacing => {
                TessellationSpacing::FractionalEvenSpacing
            }
            ffi::ASTLayoutTessEvaluationType::FractionalOddSpacing => {
                TessellationSpacing::FractionalOddSpacing
            }
            _ => panic!("Internal error: Unexpected tessellation evaluation layout"),
        }
    }
}

impl From<ffi::ASTLayoutTessEvaluationType> for TessellationOrdering {
    fn from(value: ffi::ASTLayoutTessEvaluationType) -> Self {
        match value {
            ffi::ASTLayoutTessEvaluationType::Undefined => TessellationOrdering::Undefined,
            ffi::ASTLayoutTessEvaluationType::Cw => TessellationOrdering::Cw,
            ffi::ASTLayoutTessEvaluationType::Ccw => TessellationOrdering::Ccw,
            _ => panic!("Internal error: Unexpected tessellation evaluation layout"),
        }
    }
}

impl From<ffi::ASTLayoutPrimitiveType> for GeometryPrimitive {
    fn from(value: ffi::ASTLayoutPrimitiveType) -> Self {
        match value {
            ffi::ASTLayoutPrimitiveType::Undefined => GeometryPrimitive::Undefined,
            ffi::ASTLayoutPrimitiveType::Points => GeometryPrimitive::Points,
            ffi::ASTLayoutPrimitiveType::Lines => GeometryPrimitive::Lines,
            ffi::ASTLayoutPrimitiveType::LinesAdjacency => GeometryPrimitive::LinesAdjacency,
            ffi::ASTLayoutPrimitiveType::Triangles => GeometryPrimitive::Triangles,
            ffi::ASTLayoutPrimitiveType::TrianglesAdjacency => {
                GeometryPrimitive::TrianglesAdjacency
            }
            ffi::ASTLayoutPrimitiveType::LineStrip => GeometryPrimitive::LineStrip,
            ffi::ASTLayoutPrimitiveType::TriangleStrip => GeometryPrimitive::TriangleStrip,
            _ => panic!("Internal error: Unexpected geometry primitive layout"),
        }
    }
}

fn builder_new(shader_type: ffi::ASTShaderType, options: ffi::BuildOptions) -> Box<BuilderWrapper> {
    let shader_type = match shader_type {
        ffi::ASTShaderType::Vertex => ShaderType::Vertex,
        ffi::ASTShaderType::TessControl => ShaderType::TessellationControl,
        ffi::ASTShaderType::TessEvaluation => ShaderType::TessellationEvaluation,
        ffi::ASTShaderType::Geometry => ShaderType::Geometry,
        ffi::ASTShaderType::Fragment => ShaderType::Fragment,
        ffi::ASTShaderType::Compute => ShaderType::Compute,
        _ => panic!("Internal error: Impossible shader type enum value"),
    };
    Box::new(BuilderWrapper { builder: Builder::new(shader_type, options) })
}

fn builder_finish(mut builder: Box<BuilderWrapper>) -> Box<IR> {
    builder.builder.finish();

    // Propagate precision to constant
    let mut ir = builder.builder.take_ir();
    transform::propagate_precision::run(&mut ir);

    Box::new(ir)
}

fn builder_fail(mut builder: Box<BuilderWrapper>) -> Box<IR> {
    builder.builder.fail();
    Box::new(builder.builder.take_ir())
}

struct BuilderWrapper {
    builder: Builder,
}

impl BuilderWrapper {
    fn apply_primary_secondary_dimensions(
        builder: &mut Builder,
        base_type_id: TypeId,
        primary_size: u32,
        secondary_size: u32,
    ) -> TypeId {
        if secondary_size > 1 {
            debug_assert!(base_type_id == TYPE_ID_FLOAT);
            builder.ir().meta.get_matrix_type_id(primary_size, secondary_size)
        } else if primary_size > 1 {
            builder.ir().meta.get_vector_type_id_from_element_id(base_type_id, primary_size)
        } else {
            base_type_id
        }
    }

    fn get_basic_type_id(
        &mut self,
        basic_type: ffi::ASTBasicType,
        primary_size: u32,
        secondary_size: u32,
    ) -> ffi::TypeId {
        let ir_base_type = match basic_type {
            ffi::ASTBasicType::Void => TYPE_ID_VOID,
            ffi::ASTBasicType::Float => TYPE_ID_FLOAT,
            ffi::ASTBasicType::Int => TYPE_ID_INT,
            ffi::ASTBasicType::UInt => TYPE_ID_UINT,
            ffi::ASTBasicType::Bool => TYPE_ID_BOOL,
            ffi::ASTBasicType::AtomicCounter => TYPE_ID_ATOMIC_COUNTER,
            ffi::ASTBasicType::YuvCscStandardEXT => TYPE_ID_YUV_CSC_STANDARD,
            _ => {
                debug_assert!(primary_size <= 1 && secondary_size <= 1);

                let image_basic_type = if matches!(
                    basic_type,
                    ffi::ASTBasicType::Sampler2D
                        | ffi::ASTBasicType::Sampler3D
                        | ffi::ASTBasicType::SamplerCube
                        | ffi::ASTBasicType::Sampler2DArray
                        | ffi::ASTBasicType::SamplerExternalOES
                        | ffi::ASTBasicType::SamplerExternal2DY2YEXT
                        | ffi::ASTBasicType::Sampler2DRect
                        | ffi::ASTBasicType::Sampler2DMS
                        | ffi::ASTBasicType::Sampler2DMSArray
                        | ffi::ASTBasicType::Sampler2DShadow
                        | ffi::ASTBasicType::SamplerCubeShadow
                        | ffi::ASTBasicType::Sampler2DArrayShadow
                        | ffi::ASTBasicType::SamplerBuffer
                        | ffi::ASTBasicType::SamplerCubeArray
                        | ffi::ASTBasicType::SamplerCubeArrayShadow
                        | ffi::ASTBasicType::SamplerVideoWEBGL
                        | ffi::ASTBasicType::Image2D
                        | ffi::ASTBasicType::Image3D
                        | ffi::ASTBasicType::Image2DArray
                        | ffi::ASTBasicType::ImageCube
                        | ffi::ASTBasicType::ImageCubeArray
                        | ffi::ASTBasicType::ImageBuffer
                        | ffi::ASTBasicType::PixelLocalANGLE
                        | ffi::ASTBasicType::SubpassInput
                ) {
                    ImageBasicType::Float
                } else if matches!(
                    basic_type,
                    ffi::ASTBasicType::ISampler2D
                        | ffi::ASTBasicType::ISampler3D
                        | ffi::ASTBasicType::ISamplerCube
                        | ffi::ASTBasicType::ISampler2DArray
                        | ffi::ASTBasicType::ISampler2DMS
                        | ffi::ASTBasicType::ISampler2DMSArray
                        | ffi::ASTBasicType::ISampler2DRect
                        | ffi::ASTBasicType::ISamplerBuffer
                        | ffi::ASTBasicType::ISamplerCubeArray
                        | ffi::ASTBasicType::IImage2D
                        | ffi::ASTBasicType::IImage3D
                        | ffi::ASTBasicType::IImage2DArray
                        | ffi::ASTBasicType::IImageCube
                        | ffi::ASTBasicType::IImageCubeArray
                        | ffi::ASTBasicType::IImageBuffer
                        | ffi::ASTBasicType::IPixelLocalANGLE
                        | ffi::ASTBasicType::ISubpassInput
                ) {
                    ImageBasicType::Int
                } else {
                    debug_assert!(matches!(
                        basic_type,
                        ffi::ASTBasicType::USampler2D
                            | ffi::ASTBasicType::USampler3D
                            | ffi::ASTBasicType::USamplerCube
                            | ffi::ASTBasicType::USampler2DArray
                            | ffi::ASTBasicType::USampler2DMS
                            | ffi::ASTBasicType::USampler2DMSArray
                            | ffi::ASTBasicType::USampler2DRect
                            | ffi::ASTBasicType::USamplerBuffer
                            | ffi::ASTBasicType::USamplerCubeArray
                            | ffi::ASTBasicType::UImage2D
                            | ffi::ASTBasicType::UImage3D
                            | ffi::ASTBasicType::UImage2DArray
                            | ffi::ASTBasicType::UImageCube
                            | ffi::ASTBasicType::UImageCubeArray
                            | ffi::ASTBasicType::UImageBuffer
                            | ffi::ASTBasicType::UPixelLocalANGLE
                            | ffi::ASTBasicType::USubpassInput
                    ));
                    ImageBasicType::Uint
                };

                let dimension = if matches!(
                    basic_type,
                    ffi::ASTBasicType::Sampler2D
                        | ffi::ASTBasicType::Sampler2DArray
                        | ffi::ASTBasicType::Sampler2DMS
                        | ffi::ASTBasicType::Sampler2DMSArray
                        | ffi::ASTBasicType::Sampler2DShadow
                        | ffi::ASTBasicType::Sampler2DArrayShadow
                        | ffi::ASTBasicType::ISampler2D
                        | ffi::ASTBasicType::ISampler2DArray
                        | ffi::ASTBasicType::ISampler2DMS
                        | ffi::ASTBasicType::ISampler2DMSArray
                        | ffi::ASTBasicType::USampler2D
                        | ffi::ASTBasicType::USampler2DArray
                        | ffi::ASTBasicType::USampler2DMS
                        | ffi::ASTBasicType::USampler2DMSArray
                        | ffi::ASTBasicType::Image2D
                        | ffi::ASTBasicType::Image2DArray
                        | ffi::ASTBasicType::IImage2D
                        | ffi::ASTBasicType::IImage2DArray
                        | ffi::ASTBasicType::UImage2D
                        | ffi::ASTBasicType::UImage2DArray
                ) {
                    ImageDimension::D2
                } else if matches!(
                    basic_type,
                    ffi::ASTBasicType::Sampler3D
                        | ffi::ASTBasicType::ISampler3D
                        | ffi::ASTBasicType::USampler3D
                        | ffi::ASTBasicType::Image3D
                        | ffi::ASTBasicType::IImage3D
                        | ffi::ASTBasicType::UImage3D
                ) {
                    ImageDimension::D3
                } else if matches!(
                    basic_type,
                    ffi::ASTBasicType::SamplerCube
                        | ffi::ASTBasicType::SamplerCubeShadow
                        | ffi::ASTBasicType::SamplerCubeArray
                        | ffi::ASTBasicType::SamplerCubeArrayShadow
                        | ffi::ASTBasicType::ISamplerCube
                        | ffi::ASTBasicType::ISamplerCubeArray
                        | ffi::ASTBasicType::USamplerCube
                        | ffi::ASTBasicType::USamplerCubeArray
                        | ffi::ASTBasicType::ImageCube
                        | ffi::ASTBasicType::ImageCubeArray
                        | ffi::ASTBasicType::IImageCube
                        | ffi::ASTBasicType::IImageCubeArray
                        | ffi::ASTBasicType::UImageCube
                        | ffi::ASTBasicType::UImageCubeArray
                ) {
                    ImageDimension::Cube
                } else if matches!(basic_type, ffi::ASTBasicType::SamplerExternalOES) {
                    ImageDimension::External
                } else if matches!(basic_type, ffi::ASTBasicType::SamplerExternal2DY2YEXT) {
                    ImageDimension::ExternalY2Y
                } else if matches!(
                    basic_type,
                    ffi::ASTBasicType::Sampler2DRect
                        | ffi::ASTBasicType::ISampler2DRect
                        | ffi::ASTBasicType::USampler2DRect
                ) {
                    ImageDimension::Rect
                } else if matches!(
                    basic_type,
                    ffi::ASTBasicType::SamplerBuffer
                        | ffi::ASTBasicType::ISamplerBuffer
                        | ffi::ASTBasicType::USamplerBuffer
                        | ffi::ASTBasicType::ImageBuffer
                        | ffi::ASTBasicType::IImageBuffer
                        | ffi::ASTBasicType::UImageBuffer
                ) {
                    ImageDimension::Buffer
                } else if matches!(basic_type, ffi::ASTBasicType::SamplerVideoWEBGL) {
                    ImageDimension::Video
                } else if matches!(
                    basic_type,
                    ffi::ASTBasicType::PixelLocalANGLE
                        | ffi::ASTBasicType::IPixelLocalANGLE
                        | ffi::ASTBasicType::UPixelLocalANGLE
                ) {
                    ImageDimension::PixelLocal
                } else {
                    debug_assert!(matches!(
                        basic_type,
                        ffi::ASTBasicType::SubpassInput
                            | ffi::ASTBasicType::ISubpassInput
                            | ffi::ASTBasicType::USubpassInput
                    ));
                    ImageDimension::Subpass
                };

                let is_sampled = matches!(
                    basic_type,
                    ffi::ASTBasicType::Sampler2D
                        | ffi::ASTBasicType::Sampler3D
                        | ffi::ASTBasicType::SamplerCube
                        | ffi::ASTBasicType::Sampler2DArray
                        | ffi::ASTBasicType::SamplerExternalOES
                        | ffi::ASTBasicType::SamplerExternal2DY2YEXT
                        | ffi::ASTBasicType::Sampler2DRect
                        | ffi::ASTBasicType::Sampler2DMS
                        | ffi::ASTBasicType::Sampler2DMSArray
                        | ffi::ASTBasicType::Sampler2DShadow
                        | ffi::ASTBasicType::SamplerCubeShadow
                        | ffi::ASTBasicType::Sampler2DArrayShadow
                        | ffi::ASTBasicType::SamplerBuffer
                        | ffi::ASTBasicType::SamplerCubeArray
                        | ffi::ASTBasicType::SamplerCubeArrayShadow
                        | ffi::ASTBasicType::SamplerVideoWEBGL
                        | ffi::ASTBasicType::ISampler2D
                        | ffi::ASTBasicType::ISampler3D
                        | ffi::ASTBasicType::ISamplerCube
                        | ffi::ASTBasicType::ISampler2DArray
                        | ffi::ASTBasicType::ISampler2DMS
                        | ffi::ASTBasicType::ISampler2DMSArray
                        | ffi::ASTBasicType::ISampler2DRect
                        | ffi::ASTBasicType::ISamplerBuffer
                        | ffi::ASTBasicType::ISamplerCubeArray
                        | ffi::ASTBasicType::USampler2D
                        | ffi::ASTBasicType::USampler3D
                        | ffi::ASTBasicType::USamplerCube
                        | ffi::ASTBasicType::USampler2DArray
                        | ffi::ASTBasicType::USampler2DMS
                        | ffi::ASTBasicType::USampler2DMSArray
                        | ffi::ASTBasicType::USampler2DRect
                        | ffi::ASTBasicType::USamplerBuffer
                        | ffi::ASTBasicType::USamplerCubeArray
                );

                let is_array = matches!(
                    basic_type,
                    ffi::ASTBasicType::Sampler2DArray
                        | ffi::ASTBasicType::Sampler2DMSArray
                        | ffi::ASTBasicType::Sampler2DArrayShadow
                        | ffi::ASTBasicType::SamplerCubeArray
                        | ffi::ASTBasicType::SamplerCubeArrayShadow
                        | ffi::ASTBasicType::Image2DArray
                        | ffi::ASTBasicType::ImageCubeArray
                        | ffi::ASTBasicType::ISampler2DArray
                        | ffi::ASTBasicType::ISampler2DMSArray
                        | ffi::ASTBasicType::ISamplerCubeArray
                        | ffi::ASTBasicType::IImage2DArray
                        | ffi::ASTBasicType::IImageCubeArray
                        | ffi::ASTBasicType::USampler2DArray
                        | ffi::ASTBasicType::USampler2DMSArray
                        | ffi::ASTBasicType::USamplerCubeArray
                        | ffi::ASTBasicType::UImage2DArray
                        | ffi::ASTBasicType::UImageCubeArray
                );

                let is_ms = matches!(
                    basic_type,
                    ffi::ASTBasicType::Sampler2DMS
                        | ffi::ASTBasicType::Sampler2DMSArray
                        | ffi::ASTBasicType::ISampler2DMS
                        | ffi::ASTBasicType::ISampler2DMSArray
                        | ffi::ASTBasicType::USampler2DMS
                        | ffi::ASTBasicType::USampler2DMSArray
                );

                let is_shadow = matches!(
                    basic_type,
                    ffi::ASTBasicType::Sampler2DShadow
                        | ffi::ASTBasicType::SamplerCubeShadow
                        | ffi::ASTBasicType::Sampler2DArrayShadow
                        | ffi::ASTBasicType::SamplerCubeArrayShadow
                );

                self.builder.ir().meta.get_image_type_id(
                    image_basic_type,
                    ImageType { dimension, is_sampled, is_array, is_ms, is_shadow },
                )
            }
        };

        Self::apply_primary_secondary_dimensions(
            &mut self.builder,
            ir_base_type,
            primary_size,
            secondary_size,
        )
        .into()
    }

    fn ast_type_built_in(ast_type: &ffi::ASTType) -> Option<BuiltIn> {
        match ast_type.qualifier {
            ffi::ASTQualifier::InstanceID => Some(BuiltIn::InstanceID),
            ffi::ASTQualifier::VertexID => Some(BuiltIn::VertexID),
            ffi::ASTQualifier::Position => Some(BuiltIn::Position),
            ffi::ASTQualifier::PointSize => Some(BuiltIn::PointSize),
            ffi::ASTQualifier::BaseVertex => Some(BuiltIn::BaseVertex),
            ffi::ASTQualifier::BaseInstance => Some(BuiltIn::BaseInstance),
            ffi::ASTQualifier::DrawID => Some(BuiltIn::DrawID),
            ffi::ASTQualifier::FragCoord => Some(BuiltIn::FragCoord),
            ffi::ASTQualifier::FrontFacing => Some(BuiltIn::FrontFacing),
            ffi::ASTQualifier::PointCoord => Some(BuiltIn::PointCoord),
            ffi::ASTQualifier::HelperInvocation => Some(BuiltIn::HelperInvocation),
            ffi::ASTQualifier::FragColor => Some(BuiltIn::FragColor),
            ffi::ASTQualifier::FragData => Some(BuiltIn::FragData),
            ffi::ASTQualifier::FragDepth => Some(BuiltIn::FragDepth),
            ffi::ASTQualifier::SecondaryFragColorEXT => Some(BuiltIn::SecondaryFragColorEXT),
            ffi::ASTQualifier::SecondaryFragDataEXT => Some(BuiltIn::SecondaryFragDataEXT),
            ffi::ASTQualifier::ViewIDOVR => Some(BuiltIn::ViewIDOVR),
            ffi::ASTQualifier::ClipDistance => Some(BuiltIn::ClipDistance),
            ffi::ASTQualifier::CullDistance => Some(BuiltIn::CullDistance),
            ffi::ASTQualifier::LastFragColor => Some(BuiltIn::LastFragColor),
            ffi::ASTQualifier::LastFragData => Some(BuiltIn::LastFragData),
            ffi::ASTQualifier::LastFragDepth => Some(BuiltIn::LastFragDepthARM),
            ffi::ASTQualifier::LastFragStencil => Some(BuiltIn::LastFragStencilARM),
            ffi::ASTQualifier::DepthRange => Some(BuiltIn::DepthRange),
            ffi::ASTQualifier::ShadingRateEXT => Some(BuiltIn::ShadingRateEXT),
            ffi::ASTQualifier::PrimitiveShadingRateEXT => Some(BuiltIn::PrimitiveShadingRateEXT),
            ffi::ASTQualifier::SampleID => Some(BuiltIn::SampleID),
            ffi::ASTQualifier::SamplePosition => Some(BuiltIn::SamplePosition),
            ffi::ASTQualifier::SampleMaskIn => Some(BuiltIn::SampleMaskIn),
            ffi::ASTQualifier::SampleMask => Some(BuiltIn::SampleMask),
            ffi::ASTQualifier::NumSamples => Some(BuiltIn::NumSamples),
            ffi::ASTQualifier::NumWorkGroups => Some(BuiltIn::NumWorkGroups),
            ffi::ASTQualifier::WorkGroupSize => Some(BuiltIn::WorkGroupSize),
            ffi::ASTQualifier::WorkGroupID => Some(BuiltIn::WorkGroupID),
            ffi::ASTQualifier::LocalInvocationID => Some(BuiltIn::LocalInvocationID),
            ffi::ASTQualifier::GlobalInvocationID => Some(BuiltIn::GlobalInvocationID),
            ffi::ASTQualifier::LocalInvocationIndex => Some(BuiltIn::LocalInvocationIndex),
            ffi::ASTQualifier::PerVertexIn => Some(BuiltIn::PerVertexIn),
            ffi::ASTQualifier::PerVertexOut => Some(BuiltIn::PerVertexOut),
            ffi::ASTQualifier::PrimitiveIDIn => Some(BuiltIn::PrimitiveIDIn),
            ffi::ASTQualifier::InvocationID => Some(BuiltIn::InvocationID),
            ffi::ASTQualifier::PrimitiveID => Some(BuiltIn::PrimitiveID),
            ffi::ASTQualifier::LayerOut => Some(BuiltIn::LayerOut),
            ffi::ASTQualifier::LayerIn => Some(BuiltIn::LayerIn),
            ffi::ASTQualifier::PatchVerticesIn => Some(BuiltIn::PatchVerticesIn),
            ffi::ASTQualifier::TessLevelOuter => Some(BuiltIn::TessLevelOuter),
            ffi::ASTQualifier::TessLevelInner => Some(BuiltIn::TessLevelInner),
            ffi::ASTQualifier::TessCoord => Some(BuiltIn::TessCoord),
            ffi::ASTQualifier::BoundingBox => Some(BuiltIn::BoundingBoxOES),
            ffi::ASTQualifier::PixelLocalEXT => Some(BuiltIn::PixelLocalEXT),
            _ => None,
        }
    }

    fn ast_type_decorations(ast_type: &ffi::ASTType) -> Decorations {
        let mut decorations = Decorations::new(match ast_type.qualifier {
            ffi::ASTQualifier::Temporary
            | ffi::ASTQualifier::Global
            | ffi::ASTQualifier::Const
            | ffi::ASTQualifier::InstanceID
            | ffi::ASTQualifier::VertexID
            | ffi::ASTQualifier::Position
            | ffi::ASTQualifier::PointSize
            | ffi::ASTQualifier::BaseVertex
            | ffi::ASTQualifier::BaseInstance
            | ffi::ASTQualifier::DrawID
            | ffi::ASTQualifier::FragCoord
            | ffi::ASTQualifier::FrontFacing
            | ffi::ASTQualifier::PointCoord
            | ffi::ASTQualifier::HelperInvocation
            | ffi::ASTQualifier::FragColor
            | ffi::ASTQualifier::FragData
            | ffi::ASTQualifier::FragDepth
            | ffi::ASTQualifier::SecondaryFragColorEXT
            | ffi::ASTQualifier::SecondaryFragDataEXT
            | ffi::ASTQualifier::ViewIDOVR
            | ffi::ASTQualifier::ClipDistance
            | ffi::ASTQualifier::CullDistance
            | ffi::ASTQualifier::LastFragColor
            | ffi::ASTQualifier::LastFragData
            | ffi::ASTQualifier::LastFragDepth
            | ffi::ASTQualifier::LastFragStencil
            | ffi::ASTQualifier::DepthRange
            | ffi::ASTQualifier::ShadingRateEXT
            | ffi::ASTQualifier::PrimitiveShadingRateEXT
            | ffi::ASTQualifier::SampleID
            | ffi::ASTQualifier::SamplePosition
            | ffi::ASTQualifier::SampleMaskIn
            | ffi::ASTQualifier::SampleMask
            | ffi::ASTQualifier::NumSamples
            | ffi::ASTQualifier::NumWorkGroups
            | ffi::ASTQualifier::WorkGroupSize
            | ffi::ASTQualifier::WorkGroupID
            | ffi::ASTQualifier::LocalInvocationID
            | ffi::ASTQualifier::GlobalInvocationID
            | ffi::ASTQualifier::LocalInvocationIndex
            | ffi::ASTQualifier::PerVertexIn
            | ffi::ASTQualifier::PrimitiveIDIn
            | ffi::ASTQualifier::InvocationID
            | ffi::ASTQualifier::PrimitiveID
            | ffi::ASTQualifier::LayerOut
            | ffi::ASTQualifier::LayerIn
            | ffi::ASTQualifier::PerVertexOut
            | ffi::ASTQualifier::PatchVerticesIn
            | ffi::ASTQualifier::TessLevelOuter
            | ffi::ASTQualifier::TessLevelInner
            | ffi::ASTQualifier::TessCoord
            | ffi::ASTQualifier::BoundingBox => vec![],

            // Parameter directions are separately tracked, the Input/Output decorations are not
            // used for function parameters.
            ffi::ASTQualifier::ParamIn
            | ffi::ASTQualifier::ParamOut
            | ffi::ASTQualifier::ParamInOut
            | ffi::ASTQualifier::ParamConst => vec![],

            ffi::ASTQualifier::Attribute
            | ffi::ASTQualifier::VaryingIn
            | ffi::ASTQualifier::VertexIn
            | ffi::ASTQualifier::FragmentIn
            | ffi::ASTQualifier::GeometryIn
            | ffi::ASTQualifier::TessControlIn
            | ffi::ASTQualifier::TessEvaluationIn => vec![Decoration::Input],

            ffi::ASTQualifier::VaryingOut
            | ffi::ASTQualifier::FragmentOut
            | ffi::ASTQualifier::VertexOut
            | ffi::ASTQualifier::GeometryOut
            | ffi::ASTQualifier::TessControlOut
            | ffi::ASTQualifier::TessEvaluationOut => vec![Decoration::Output],

            ffi::ASTQualifier::FragmentInOut => vec![Decoration::InputOutput],

            ffi::ASTQualifier::Uniform => vec![Decoration::Uniform],
            ffi::ASTQualifier::Buffer => vec![Decoration::Buffer],
            ffi::ASTQualifier::Shared => vec![Decoration::Shared],

            ffi::ASTQualifier::Smooth => vec![Decoration::Smooth],
            ffi::ASTQualifier::Flat => vec![Decoration::Flat],
            ffi::ASTQualifier::NoPerspective => vec![Decoration::NoPerspective],
            ffi::ASTQualifier::Centroid => vec![Decoration::Centroid],
            ffi::ASTQualifier::Sample => vec![Decoration::Sample],
            ffi::ASTQualifier::NoPerspectiveCentroid => {
                vec![Decoration::NoPerspective, Decoration::Centroid]
            }
            ffi::ASTQualifier::NoPerspectiveSample => {
                vec![Decoration::NoPerspective, Decoration::Sample]
            }
            ffi::ASTQualifier::Patch => vec![Decoration::Patch],

            ffi::ASTQualifier::SmoothOut => vec![Decoration::Output, Decoration::Smooth],
            ffi::ASTQualifier::FlatOut => vec![Decoration::Output, Decoration::Flat],
            ffi::ASTQualifier::NoPerspectiveOut => {
                vec![Decoration::Output, Decoration::NoPerspective]
            }
            ffi::ASTQualifier::CentroidOut => vec![Decoration::Output, Decoration::Centroid],
            ffi::ASTQualifier::SampleOut => vec![Decoration::Output, Decoration::Sample],
            ffi::ASTQualifier::NoPerspectiveCentroidOut => {
                vec![Decoration::Output, Decoration::NoPerspective, Decoration::Centroid]
            }
            ffi::ASTQualifier::NoPerspectiveSampleOut => {
                vec![Decoration::Output, Decoration::NoPerspective, Decoration::Sample]
            }
            ffi::ASTQualifier::PatchOut => vec![Decoration::Output, Decoration::Patch],

            ffi::ASTQualifier::SmoothIn => vec![Decoration::Input, Decoration::Smooth],
            ffi::ASTQualifier::FlatIn => vec![Decoration::Input, Decoration::Flat],
            ffi::ASTQualifier::NoPerspectiveIn => {
                vec![Decoration::Input, Decoration::NoPerspective]
            }
            ffi::ASTQualifier::CentroidIn => vec![Decoration::Input, Decoration::Centroid],
            ffi::ASTQualifier::SampleIn => vec![Decoration::Input, Decoration::Sample],
            ffi::ASTQualifier::NoPerspectiveCentroidIn => {
                vec![Decoration::Input, Decoration::NoPerspective, Decoration::Centroid]
            }
            ffi::ASTQualifier::NoPerspectiveSampleIn => {
                vec![Decoration::Input, Decoration::NoPerspective, Decoration::Sample]
            }
            ffi::ASTQualifier::PatchIn => vec![Decoration::Input, Decoration::Patch],

            ffi::ASTQualifier::PixelLocalEXT => unimplemented!(),
            _ => panic!("Internal error: Unexpected qualifier"),
        });

        if ast_type.invariant {
            decorations.decorations.push(Decoration::Invariant);
        }
        if ast_type.precise {
            decorations.decorations.push(Decoration::Precise);
        }
        if ast_type.interpolant {
            decorations.decorations.push(Decoration::Interpolant);
        }

        if ast_type.memory_qualifier.readonly {
            decorations.decorations.push(Decoration::ReadOnly);
        }
        if ast_type.memory_qualifier.writeonly {
            decorations.decorations.push(Decoration::WriteOnly);
        }
        if ast_type.memory_qualifier.coherent {
            decorations.decorations.push(Decoration::Coherent);
        }
        if ast_type.memory_qualifier.restrict_qualifier {
            decorations.decorations.push(Decoration::Restrict);
        }
        if ast_type.memory_qualifier.volatile_qualifier {
            decorations.decorations.push(Decoration::Volatile);
        }

        if ast_type.layout_qualifier.location >= 0 {
            decorations
                .decorations
                .push(Decoration::Location(ast_type.layout_qualifier.location as u32));
        }
        if ast_type.layout_qualifier.index >= 0 {
            decorations.decorations.push(Decoration::Index(ast_type.layout_qualifier.index as u32));
        }
        if ast_type.layout_qualifier.binding >= 0 {
            decorations
                .decorations
                .push(Decoration::Binding(ast_type.layout_qualifier.binding as u32));
        }
        if ast_type.layout_qualifier.offset >= 0 {
            decorations
                .decorations
                .push(Decoration::Offset(ast_type.layout_qualifier.offset as u32));
        }
        if ast_type.layout_qualifier.num_views >= 0 {
            decorations
                .decorations
                .push(Decoration::NumViews(ast_type.layout_qualifier.num_views as u32));
        }
        if ast_type.layout_qualifier.yuv {
            decorations.decorations.push(Decoration::Yuv);
        }
        if ast_type.layout_qualifier.noncoherent {
            decorations.decorations.push(Decoration::NonCoherent);
        }

        match ast_type.layout_qualifier.matrix_packing {
            ffi::ASTLayoutMatrixPacking::RowMajor => {
                decorations.decorations.push(Decoration::MatrixPacking(MatrixPacking::RowMajor))
            }
            ffi::ASTLayoutMatrixPacking::ColumnMajor => {
                decorations.decorations.push(Decoration::MatrixPacking(MatrixPacking::ColumnMajor))
            }
            ffi::ASTLayoutMatrixPacking::Unspecified => (),
            _ => panic!("Internal error: Unexpected matrix packing enum value"),
        };

        match ast_type.layout_qualifier.block_storage {
            ffi::ASTLayoutBlockStorage::Shared => {
                decorations.decorations.push(Decoration::Block(BlockStorage::Shared))
            }
            ffi::ASTLayoutBlockStorage::Packed => {
                decorations.decorations.push(Decoration::Block(BlockStorage::Packed))
            }
            ffi::ASTLayoutBlockStorage::Std140 => {
                decorations.decorations.push(Decoration::Block(BlockStorage::Std140))
            }
            ffi::ASTLayoutBlockStorage::Std430 => {
                decorations.decorations.push(Decoration::Block(BlockStorage::Std430))
            }
            ffi::ASTLayoutBlockStorage::Unspecified => (),
            _ => panic!("Internal error: Unexpected block storage enum value"),
        };

        match ast_type.layout_qualifier.depth {
            ffi::ASTLayoutDepth::Any => decorations.decorations.push(Decoration::Depth(Depth::Any)),
            ffi::ASTLayoutDepth::Greater => {
                decorations.decorations.push(Decoration::Depth(Depth::Greater))
            }
            ffi::ASTLayoutDepth::Less => {
                decorations.decorations.push(Decoration::Depth(Depth::Less))
            }
            ffi::ASTLayoutDepth::Unchanged => {
                decorations.decorations.push(Decoration::Depth(Depth::Unchanged))
            }
            ffi::ASTLayoutDepth::Unspecified => (),
            _ => panic!("Internal error: Unexpected depth enum value"),
        };

        match ast_type.layout_qualifier.image_internal_format {
            ffi::ASTLayoutImageInternalFormat::RGBA32F => decorations
                .decorations
                .push(Decoration::ImageInternalFormat(ImageInternalFormat::RGBA32F)),
            ffi::ASTLayoutImageInternalFormat::RGBA16F => decorations
                .decorations
                .push(Decoration::ImageInternalFormat(ImageInternalFormat::RGBA16F)),
            ffi::ASTLayoutImageInternalFormat::R32F => decorations
                .decorations
                .push(Decoration::ImageInternalFormat(ImageInternalFormat::R32F)),
            ffi::ASTLayoutImageInternalFormat::RGBA32UI => decorations
                .decorations
                .push(Decoration::ImageInternalFormat(ImageInternalFormat::RGBA32UI)),
            ffi::ASTLayoutImageInternalFormat::RGBA16UI => decorations
                .decorations
                .push(Decoration::ImageInternalFormat(ImageInternalFormat::RGBA16UI)),
            ffi::ASTLayoutImageInternalFormat::RGBA8UI => decorations
                .decorations
                .push(Decoration::ImageInternalFormat(ImageInternalFormat::RGBA8UI)),
            ffi::ASTLayoutImageInternalFormat::R32UI => decorations
                .decorations
                .push(Decoration::ImageInternalFormat(ImageInternalFormat::R32UI)),
            ffi::ASTLayoutImageInternalFormat::RGBA32I => decorations
                .decorations
                .push(Decoration::ImageInternalFormat(ImageInternalFormat::RGBA32I)),
            ffi::ASTLayoutImageInternalFormat::RGBA16I => decorations
                .decorations
                .push(Decoration::ImageInternalFormat(ImageInternalFormat::RGBA16I)),
            ffi::ASTLayoutImageInternalFormat::RGBA8I => decorations
                .decorations
                .push(Decoration::ImageInternalFormat(ImageInternalFormat::RGBA8I)),
            ffi::ASTLayoutImageInternalFormat::R32I => decorations
                .decorations
                .push(Decoration::ImageInternalFormat(ImageInternalFormat::R32I)),
            ffi::ASTLayoutImageInternalFormat::RGBA8 => decorations
                .decorations
                .push(Decoration::ImageInternalFormat(ImageInternalFormat::RGBA8)),
            ffi::ASTLayoutImageInternalFormat::RGBA8SNORM => decorations
                .decorations
                .push(Decoration::ImageInternalFormat(ImageInternalFormat::RGBA8SNORM)),
            ffi::ASTLayoutImageInternalFormat::Unspecified => (),
            _ => panic!("Internal error: Unexpected image internal format enum value"),
        };

        decorations
    }

    fn get_struct_type_id(&mut self, struct_info: &ffi::ASTStruct) -> ffi::TypeId {
        let fields = struct_info
            .fields
            .iter()
            .map(|field| {
                Field::new(
                    if struct_info.is_internal {
                        Name::new_exact(field.name)
                    } else {
                        Name::new_temp(field.name)
                    },
                    field.ast_type.type_id.into(),
                    field.ast_type.precision.into(),
                    Self::ast_type_decorations(&field.ast_type),
                )
            })
            .collect::<Vec<_>>();

        let (name, specialization) = if struct_info.is_interface_block {
            (
                if struct_info.is_internal {
                    Name::new_exact(struct_info.name)
                } else {
                    Name::new_interface(struct_info.name)
                },
                StructSpecialization::InterfaceBlock,
            )
        } else {
            (
                if struct_info.is_internal {
                    Name::new_exact(struct_info.name)
                } else if struct_info.is_at_global_scope {
                    Name::new_interface(struct_info.name)
                } else {
                    Name::new_temp(struct_info.name)
                },
                StructSpecialization::Struct,
            )
        };

        self.builder.ir().meta.get_struct_type_id(name, fields, specialization).into()
    }

    fn get_array_type_id(
        &mut self,
        element_type_id: ffi::TypeId,
        array_sizes: &[u32],
    ) -> ffi::TypeId {
        array_sizes
            .iter()
            .fold(element_type_id.into(), |element_type_id, &array_size| {
                if array_size == 0 {
                    self.builder.ir().meta.get_unsized_array_type_id(element_type_id)
                } else {
                    self.builder.ir().meta.get_array_type_id(element_type_id, array_size)
                }
            })
            .into()
    }

    fn set_early_fragment_tests(&mut self, value: bool) {
        self.builder.ir().meta.set_early_fragment_tests(value);
    }

    fn set_advanced_blend_equations(&mut self, value: u32) {
        // Identical to gl::BlendEquationType, only including advanced blend equations.
        const BLEND_MULTIPLY: u32 = 1 << 6;
        const BLEND_SCREEN: u32 = 1 << 7;
        const BLEND_OVERLAY: u32 = 1 << 8;
        const BLEND_DARKEN: u32 = 1 << 9;
        const BLEND_LIGHTEN: u32 = 1 << 10;
        const BLEND_COLORDODGE: u32 = 1 << 11;
        const BLEND_COLORBURN: u32 = 1 << 12;
        const BLEND_HARDLIGHT: u32 = 1 << 13;
        const BLEND_SOFTLIGHT: u32 = 1 << 14;
        const BLEND_DIFFERENCE: u32 = 1 << 16;
        const BLEND_EXCLUSION: u32 = 1 << 18;
        const BLEND_HSL_HUE: u32 = 1 << 19;
        const BLEND_HSL_SATURATION: u32 = 1 << 20;
        const BLEND_HSL_COLOR: u32 = 1 << 21;
        const BLEND_HSL_LUMINOSITY: u32 = 1 << 22;

        let mut equations = AdvancedBlendEquations::new();

        if value & BLEND_MULTIPLY != 0 {
            equations.multiply = true;
        }
        if value & BLEND_SCREEN != 0 {
            equations.screen = true;
        }
        if value & BLEND_OVERLAY != 0 {
            equations.overlay = true;
        }
        if value & BLEND_DARKEN != 0 {
            equations.darken = true;
        }
        if value & BLEND_LIGHTEN != 0 {
            equations.lighten = true;
        }
        if value & BLEND_COLORDODGE != 0 {
            equations.colordodge = true;
        }
        if value & BLEND_COLORBURN != 0 {
            equations.colorburn = true;
        }
        if value & BLEND_HARDLIGHT != 0 {
            equations.hardlight = true;
        }
        if value & BLEND_SOFTLIGHT != 0 {
            equations.softlight = true;
        }
        if value & BLEND_DIFFERENCE != 0 {
            equations.difference = true;
        }
        if value & BLEND_EXCLUSION != 0 {
            equations.exclusion = true;
        }
        if value & BLEND_HSL_HUE != 0 {
            equations.hsl_hue = true;
        }
        if value & BLEND_HSL_SATURATION != 0 {
            equations.hsl_saturation = true;
        }
        if value & BLEND_HSL_COLOR != 0 {
            equations.hsl_color = true;
        }
        if value & BLEND_HSL_LUMINOSITY != 0 {
            equations.hsl_luminosity = true;
        }

        self.builder.ir().meta.add_advanced_blend_equations(equations);
    }

    fn set_tcs_vertices(&mut self, value: u32) {
        self.builder.ir().meta.set_tcs_vertices(value);
    }

    fn set_tes_primitive(&mut self, value: ffi::ASTLayoutTessEvaluationType) {
        self.builder.ir().meta.set_tes_primitive(value.into());
    }

    fn set_tes_vertex_spacing(&mut self, value: ffi::ASTLayoutTessEvaluationType) {
        self.builder.ir().meta.set_tes_vertex_spacing(value.into());
    }

    fn set_tes_ordering(&mut self, value: ffi::ASTLayoutTessEvaluationType) {
        self.builder.ir().meta.set_tes_ordering(value.into());
    }

    fn set_tes_point_mode(&mut self, value: ffi::ASTLayoutTessEvaluationType) {
        self.builder
            .ir()
            .meta
            .set_tes_point_mode(value == ffi::ASTLayoutTessEvaluationType::PointMode);
    }

    fn set_gs_primitive_in(&mut self, value: ffi::ASTLayoutPrimitiveType) {
        self.builder.ir().meta.set_gs_primitive_in(value.into());
    }

    fn set_gs_primitive_out(&mut self, value: ffi::ASTLayoutPrimitiveType) {
        self.builder.ir().meta.set_gs_primitive_out(value.into());
    }

    fn set_gs_invocations(&mut self, value: u32) {
        self.builder.ir().meta.set_gs_invocations(value);
    }

    fn set_gs_max_vertices(&mut self, value: u32) {
        self.builder.ir().meta.set_gs_max_vertices(value);
    }

    // `is_declaration_internal` is used to know if a built-in is redeclared or not, which matters
    // for some backends when generating output.
    fn declare_interface_variable(
        &mut self,
        name: &'static str,
        ast_type: &ffi::ASTType,
        is_declaration_internal: bool,
    ) -> ffi::VariableId {
        if let Some(built_in) = Self::ast_type_built_in(ast_type) {
            let variable_id = self.builder.declare_built_in_variable(
                built_in,
                ast_type.type_id.into(),
                ast_type.precision.into(),
                Self::ast_type_decorations(ast_type),
            );

            if !is_declaration_internal {
                match ast_type.qualifier {
                    ffi::ASTQualifier::PerVertexIn => {
                        self.builder.ir().meta.on_per_vertex_in_redeclaration()
                    }
                    ffi::ASTQualifier::PerVertexOut => {
                        self.builder.ir().meta.on_per_vertex_out_redeclaration()
                    }
                    _ => {}
                };
            }

            variable_id
        } else {
            debug_assert!(!is_declaration_internal);
            self.builder.declare_interface_variable(
                name,
                ast_type.type_id.into(),
                ast_type.precision.into(),
                Self::ast_type_decorations(ast_type),
            )
        }
        .into()
    }

    fn declare_temp_variable(
        &mut self,
        name: &'static str,
        ast_type: &ffi::ASTType,
    ) -> ffi::VariableId {
        if ast_type.qualifier == ffi::ASTQualifier::Const {
            self.builder.declare_const_variable(ast_type.type_id.into(), ast_type.precision.into())
        } else {
            self.builder.declare_temp_variable(
                name,
                ast_type.type_id.into(),
                ast_type.precision.into(),
                Self::ast_type_decorations(ast_type),
            )
        }
        .into()
    }

    fn mark_variable_invariant(&mut self, variable_id: ffi::VariableId) {
        self.builder.mark_variable_invariant(variable_id.into());
    }

    fn mark_variable_precise(&mut self, variable_id: ffi::VariableId) {
        self.builder.mark_variable_precise(variable_id.into());
    }

    fn function_param_direction(qualifier: ffi::ASTQualifier) -> FunctionParamDirection {
        match qualifier {
            ffi::ASTQualifier::ParamIn | ffi::ASTQualifier::ParamConst => {
                FunctionParamDirection::Input
            }
            ffi::ASTQualifier::ParamOut => FunctionParamDirection::Output,
            ffi::ASTQualifier::ParamInOut => FunctionParamDirection::InputOutput,
            _ => panic!("Internal error: Unexpected qualifier on function parameter"),
        }
    }

    fn new_function(
        &mut self,
        name: &'static str,
        params: &[ffi::VariableId],
        param_directions: &[ffi::ASTQualifier],
        return_type_id: ffi::TypeId,
        return_ast_type: &ffi::ASTType,
    ) -> ffi::FunctionId {
        let params = params
            .iter()
            .map(|&id| id.into())
            .zip(
                param_directions.iter().map(|&qualifier| Self::function_param_direction(qualifier)),
            )
            .map(|(id, dir)| FunctionParam::new(id, dir))
            .collect();
        self.builder
            .new_function(
                name,
                params,
                return_type_id.into(),
                return_ast_type.precision.into(),
                Self::ast_type_decorations(return_ast_type),
            )
            .into()
    }

    fn update_function_param_names(
        self: &mut BuilderWrapper,
        id: ffi::FunctionId,
        param_names: &[&'static str],
        param_ids: &mut [ffi::VariableId],
    ) {
        let ids = self.builder.update_function_param_names(id.into(), param_names);
        debug_assert!(ids.len() == param_ids.len());

        ids.iter().zip(param_ids.iter_mut()).for_each(|(id, out)| *out = (*id).into());
    }

    fn declare_function_param(
        &mut self,
        name: &'static str,
        type_id: ffi::TypeId,
        ast_type: &ffi::ASTType,
        direction: ffi::ASTQualifier,
    ) -> ffi::VariableId {
        self.builder
            .declare_function_param(
                name,
                type_id.into(),
                ast_type.precision.into(),
                Self::ast_type_decorations(ast_type),
                Self::function_param_direction(direction),
            )
            .into()
    }

    fn begin_function(&mut self, id: ffi::FunctionId) {
        self.builder.begin_function(id.into());
    }

    fn end_function(&mut self) {
        self.builder.end_function();
    }

    fn store(&mut self) {
        self.builder.store();
    }

    fn initialize(&mut self, id: ffi::VariableId) {
        self.builder.initialize(id.into());
    }

    fn begin_if_true_block(&mut self) {
        self.builder.begin_if_true_block();
    }

    fn end_if_true_block(&mut self) {
        self.builder.end_if_true_block();
    }

    fn begin_if_false_block(&mut self) {
        self.builder.begin_if_false_block();
    }

    fn end_if_false_block(&mut self) {
        self.builder.end_if_false_block();
    }

    fn end_if(&mut self) {
        self.builder.end_if();
    }

    fn begin_ternary_true_expression(&mut self) {
        self.builder.begin_ternary_true_expression();
    }

    fn end_ternary_true_expression(&mut self, is_void: bool) {
        if is_void {
            self.builder.end_ternary_true_expression_void();
        } else {
            self.builder.end_ternary_true_expression();
        }
    }

    fn begin_ternary_false_expression(&mut self) {
        self.builder.begin_ternary_false_expression();
    }

    fn end_ternary_false_expression(&mut self, is_void: bool) {
        if is_void {
            self.builder.end_ternary_false_expression_void();
        } else {
            self.builder.end_ternary_false_expression();
        }
    }

    fn end_ternary(&mut self, is_void: bool) {
        if is_void {
            self.builder.end_ternary_void();
        } else {
            self.builder.end_ternary();
        }
    }

    fn begin_short_circuit_or(&mut self) {
        self.builder.begin_short_circuit_or();
    }

    fn end_short_circuit_or(&mut self) {
        self.builder.end_short_circuit_or();
    }

    fn begin_short_circuit_and(&mut self) {
        self.builder.begin_short_circuit_and();
    }

    fn end_short_circuit_and(&mut self) {
        self.builder.end_short_circuit_and();
    }

    fn begin_loop_condition(&mut self) {
        self.builder.begin_loop_condition();
    }

    fn end_loop_condition(&mut self) {
        self.builder.end_loop_condition();
    }

    fn end_loop_continue(&mut self) {
        self.builder.end_loop_continue();
    }

    fn end_loop(&mut self) {
        self.builder.end_loop();
    }

    fn begin_do_loop(&mut self) {
        self.builder.begin_do_loop();
    }

    fn begin_do_loop_condition(&mut self) {
        self.builder.begin_do_loop_condition();
    }

    fn end_do_loop(&mut self) {
        self.builder.end_do_loop();
    }

    fn begin_switch(&mut self) {
        self.builder.begin_switch();
    }

    fn begin_case(&mut self) {
        self.builder.begin_case();
    }

    fn begin_default(&mut self) {
        self.builder.begin_default();
    }

    fn end_switch(&mut self) {
        self.builder.end_switch();
    }

    fn branch_discard(&mut self) {
        self.builder.branch_discard();
    }

    fn branch_return(&mut self) {
        self.builder.branch_return();
    }

    fn branch_return_value(&mut self) {
        self.builder.branch_return_value();
    }

    fn branch_break(&mut self) {
        self.builder.branch_break();
    }

    fn branch_continue(&mut self) {
        self.builder.branch_continue();
    }

    fn pop_array_size(&mut self) -> u32 {
        self.builder.pop_array_size()
    }

    fn end_statement_with_value(&mut self) {
        self.builder.end_statement_with_value();
    }

    fn push_constant_float(&mut self, value: f32) {
        self.builder.push_constant_float(value);
    }

    fn push_constant_int(&mut self, value: i32) {
        self.builder.push_constant_int(value);
    }

    fn push_constant_uint(&mut self, value: u32) {
        self.builder.push_constant_uint(value);
    }

    fn push_constant_bool(&mut self, value: bool) {
        self.builder.push_constant_bool(value);
    }

    fn push_constant_yuv_csc_standard(&mut self, value: ffi::ASTYuvCscStandardEXT) {
        self.builder.push_constant_yuv_csc_standard(value.into());
    }

    fn push_variable(&mut self, id: ffi::VariableId) {
        self.builder.push_variable(id.into());
    }

    fn call_function(&mut self, id: ffi::FunctionId) {
        self.builder.call_function(id.into());
    }

    fn vector_component(&mut self, component: u32) {
        self.builder.vector_component(component);
    }

    fn vector_component_multi(&mut self, components: &[u32]) {
        self.builder.vector_component_multi(components.to_vec());
    }

    fn index(&mut self) {
        self.builder.index();
    }

    fn struct_field(&mut self, field_index: u32) {
        self.builder.struct_field(field_index);
    }

    fn construct(&mut self, type_id: ffi::TypeId, arg_count: usize) {
        self.builder.construct(type_id.into(), arg_count);
    }

    fn on_gl_clip_distance_sized(&mut self, id: ffi::VariableId, length: u32) {
        self.builder.on_gl_clip_distance_sized(id.into(), length);
    }

    fn on_gl_cull_distance_sized(&mut self, id: ffi::VariableId, length: u32) {
        self.builder.on_gl_cull_distance_sized(id.into(), length);
    }

    fn array_length(&mut self) {
        self.builder.array_length();
    }

    fn negate(&mut self) {
        self.builder.negate();
    }

    fn postfix_increment(&mut self) {
        self.builder.postfix_increment();
    }

    fn postfix_decrement(&mut self) {
        self.builder.postfix_decrement();
    }

    fn prefix_increment(&mut self) {
        self.builder.prefix_increment();
    }

    fn prefix_decrement(&mut self) {
        self.builder.prefix_decrement();
    }

    fn add(&mut self) {
        self.builder.add();
    }

    fn add_assign(&mut self) {
        self.builder.add_assign();
    }

    fn sub(&mut self) {
        self.builder.sub();
    }

    fn sub_assign(&mut self) {
        self.builder.sub_assign();
    }

    fn mul(&mut self) {
        self.builder.mul();
    }

    fn mul_assign(&mut self) {
        self.builder.mul_assign();
    }

    fn vector_times_scalar(&mut self) {
        self.builder.vector_times_scalar();
    }

    fn vector_times_scalar_assign(&mut self) {
        self.builder.vector_times_scalar_assign();
    }

    fn matrix_times_scalar(&mut self) {
        self.builder.matrix_times_scalar();
    }

    fn matrix_times_scalar_assign(&mut self) {
        self.builder.matrix_times_scalar_assign();
    }

    fn vector_times_matrix(&mut self) {
        self.builder.vector_times_matrix();
    }

    fn vector_times_matrix_assign(&mut self) {
        self.builder.vector_times_matrix_assign();
    }

    fn matrix_times_vector(&mut self) {
        self.builder.matrix_times_vector();
    }

    fn matrix_times_matrix(&mut self) {
        self.builder.matrix_times_matrix();
    }

    fn matrix_times_matrix_assign(&mut self) {
        self.builder.matrix_times_matrix_assign();
    }

    fn div(&mut self) {
        self.builder.div();
    }

    fn div_assign(&mut self) {
        self.builder.div_assign();
    }

    fn imod(&mut self) {
        self.builder.imod();
    }

    fn imod_assign(&mut self) {
        self.builder.imod_assign();
    }

    fn logical_not(&mut self) {
        self.builder.logical_not();
    }

    fn logical_xor(&mut self) {
        self.builder.logical_xor();
    }

    fn equal(&mut self) {
        self.builder.equal();
    }

    fn not_equal(&mut self) {
        self.builder.not_equal();
    }

    fn less_than(&mut self) {
        self.builder.less_than();
    }

    fn greater_than(&mut self) {
        self.builder.greater_than();
    }

    fn less_than_equal(&mut self) {
        self.builder.less_than_equal();
    }

    fn greater_than_equal(&mut self) {
        self.builder.greater_than_equal();
    }

    fn bitwise_not(&mut self) {
        self.builder.bitwise_not();
    }

    fn bit_shift_left(&mut self) {
        self.builder.bit_shift_left();
    }

    fn bit_shift_left_assign(&mut self) {
        self.builder.bit_shift_left_assign();
    }

    fn bit_shift_right(&mut self) {
        self.builder.bit_shift_right();
    }

    fn bit_shift_right_assign(&mut self) {
        self.builder.bit_shift_right_assign();
    }

    fn bitwise_or(&mut self) {
        self.builder.bitwise_or();
    }

    fn bitwise_or_assign(&mut self) {
        self.builder.bitwise_or_assign();
    }

    fn bitwise_xor(&mut self) {
        self.builder.bitwise_xor();
    }

    fn bitwise_xor_assign(&mut self) {
        self.builder.bitwise_xor_assign();
    }

    fn bitwise_and(&mut self) {
        self.builder.bitwise_and();
    }

    fn bitwise_and_assign(&mut self) {
        self.builder.bitwise_and_assign();
    }

    fn built_in_radians(&mut self) {
        self.builder.built_in_radians();
    }

    fn built_in_degrees(&mut self) {
        self.builder.built_in_degrees();
    }

    fn built_in_sin(&mut self) {
        self.builder.built_in_sin();
    }

    fn built_in_cos(&mut self) {
        self.builder.built_in_cos();
    }

    fn built_in_tan(&mut self) {
        self.builder.built_in_tan();
    }

    fn built_in_asin(&mut self) {
        self.builder.built_in_asin();
    }

    fn built_in_acos(&mut self) {
        self.builder.built_in_acos();
    }

    fn built_in_atan(&mut self) {
        self.builder.built_in_atan();
    }

    fn built_in_sinh(&mut self) {
        self.builder.built_in_sinh();
    }

    fn built_in_cosh(&mut self) {
        self.builder.built_in_cosh();
    }

    fn built_in_tanh(&mut self) {
        self.builder.built_in_tanh();
    }

    fn built_in_asinh(&mut self) {
        self.builder.built_in_asinh();
    }

    fn built_in_acosh(&mut self) {
        self.builder.built_in_acosh();
    }

    fn built_in_atanh(&mut self) {
        self.builder.built_in_atanh();
    }

    fn built_in_atan_binary(&mut self) {
        self.builder.built_in_atan_binary();
    }

    fn built_in_pow(&mut self) {
        self.builder.built_in_pow();
    }

    fn built_in_exp(&mut self) {
        self.builder.built_in_exp();
    }

    fn built_in_log(&mut self) {
        self.builder.built_in_log();
    }

    fn built_in_exp2(&mut self) {
        self.builder.built_in_exp2();
    }

    fn built_in_log2(&mut self) {
        self.builder.built_in_log2();
    }

    fn built_in_sqrt(&mut self) {
        self.builder.built_in_sqrt();
    }

    fn built_in_inversesqrt(&mut self) {
        self.builder.built_in_inversesqrt();
    }

    fn built_in_abs(&mut self) {
        self.builder.built_in_abs();
    }

    fn built_in_sign(&mut self) {
        self.builder.built_in_sign();
    }

    fn built_in_floor(&mut self) {
        self.builder.built_in_floor();
    }

    fn built_in_trunc(&mut self) {
        self.builder.built_in_trunc();
    }

    fn built_in_round(&mut self) {
        self.builder.built_in_round();
    }

    fn built_in_roundeven(&mut self) {
        self.builder.built_in_roundeven();
    }

    fn built_in_ceil(&mut self) {
        self.builder.built_in_ceil();
    }

    fn built_in_fract(&mut self) {
        self.builder.built_in_fract();
    }

    fn built_in_mod(&mut self) {
        self.builder.built_in_mod();
    }

    fn built_in_min(&mut self) {
        self.builder.built_in_min();
    }

    fn built_in_max(&mut self) {
        self.builder.built_in_max();
    }

    fn built_in_clamp(&mut self) {
        self.builder.built_in_clamp();
    }

    fn built_in_mix(&mut self) {
        self.builder.built_in_mix();
    }

    fn built_in_step(&mut self) {
        self.builder.built_in_step();
    }

    fn built_in_smoothstep(&mut self) {
        self.builder.built_in_smoothstep();
    }

    fn built_in_modf(&mut self) {
        self.builder.built_in_modf();
    }

    fn built_in_isnan(&mut self) {
        self.builder.built_in_isnan();
    }

    fn built_in_isinf(&mut self) {
        self.builder.built_in_isinf();
    }

    fn built_in_floatbitstoint(&mut self) {
        self.builder.built_in_floatbitstoint();
    }

    fn built_in_floatbitstouint(&mut self) {
        self.builder.built_in_floatbitstouint();
    }

    fn built_in_intbitstofloat(&mut self) {
        self.builder.built_in_intbitstofloat();
    }

    fn built_in_uintbitstofloat(&mut self) {
        self.builder.built_in_uintbitstofloat();
    }

    fn built_in_fma(&mut self) {
        self.builder.built_in_fma();
    }

    fn built_in_frexp(&mut self) {
        self.builder.built_in_frexp();
    }

    fn built_in_ldexp(&mut self) {
        self.builder.built_in_ldexp();
    }

    fn built_in_packsnorm2x16(&mut self) {
        self.builder.built_in_packsnorm2x16();
    }

    fn built_in_packhalf2x16(&mut self) {
        self.builder.built_in_packhalf2x16();
    }

    fn built_in_unpacksnorm2x16(&mut self) {
        self.builder.built_in_unpacksnorm2x16();
    }

    fn built_in_unpackhalf2x16(&mut self) {
        self.builder.built_in_unpackhalf2x16();
    }

    fn built_in_packunorm2x16(&mut self) {
        self.builder.built_in_packunorm2x16();
    }

    fn built_in_unpackunorm2x16(&mut self) {
        self.builder.built_in_unpackunorm2x16();
    }

    fn built_in_packunorm4x8(&mut self) {
        self.builder.built_in_packunorm4x8();
    }

    fn built_in_packsnorm4x8(&mut self) {
        self.builder.built_in_packsnorm4x8();
    }

    fn built_in_unpackunorm4x8(&mut self) {
        self.builder.built_in_unpackunorm4x8();
    }

    fn built_in_unpacksnorm4x8(&mut self) {
        self.builder.built_in_unpacksnorm4x8();
    }

    fn built_in_length(&mut self) {
        self.builder.built_in_length();
    }

    fn built_in_distance(&mut self) {
        self.builder.built_in_distance();
    }

    fn built_in_dot(&mut self) {
        self.builder.built_in_dot();
    }

    fn built_in_cross(&mut self) {
        self.builder.built_in_cross();
    }

    fn built_in_normalize(&mut self) {
        self.builder.built_in_normalize();
    }

    fn built_in_faceforward(&mut self) {
        self.builder.built_in_faceforward();
    }

    fn built_in_reflect(&mut self) {
        self.builder.built_in_reflect();
    }

    fn built_in_refract(&mut self) {
        self.builder.built_in_refract();
    }

    fn built_in_matrixcompmult(&mut self) {
        self.builder.built_in_matrixcompmult();
    }

    fn built_in_outerproduct(&mut self) {
        self.builder.built_in_outerproduct();
    }

    fn built_in_transpose(&mut self) {
        self.builder.built_in_transpose();
    }

    fn built_in_determinant(&mut self) {
        self.builder.built_in_determinant();
    }

    fn built_in_inverse(&mut self) {
        self.builder.built_in_inverse();
    }

    fn built_in_lessthan(&mut self) {
        self.builder.built_in_lessthan();
    }

    fn built_in_lessthanequal(&mut self) {
        self.builder.built_in_lessthanequal();
    }

    fn built_in_greaterthan(&mut self) {
        self.builder.built_in_greaterthan();
    }

    fn built_in_greaterthanequal(&mut self) {
        self.builder.built_in_greaterthanequal();
    }

    fn built_in_equal(&mut self) {
        self.builder.built_in_equal();
    }

    fn built_in_notequal(&mut self) {
        self.builder.built_in_notequal();
    }

    fn built_in_any(&mut self) {
        self.builder.built_in_any();
    }

    fn built_in_all(&mut self) {
        self.builder.built_in_all();
    }

    fn built_in_not(&mut self) {
        self.builder.built_in_not();
    }

    fn built_in_bitfieldextract(&mut self) {
        self.builder.built_in_bitfieldextract();
    }

    fn built_in_bitfieldinsert(&mut self) {
        self.builder.built_in_bitfieldinsert();
    }

    fn built_in_bitfieldreverse(&mut self) {
        self.builder.built_in_bitfieldreverse();
    }

    fn built_in_bitcount(&mut self) {
        self.builder.built_in_bitcount();
    }

    fn built_in_findlsb(&mut self) {
        self.builder.built_in_findlsb();
    }

    fn built_in_findmsb(&mut self) {
        self.builder.built_in_findmsb();
    }

    fn built_in_uaddcarry(&mut self) {
        self.builder.built_in_uaddcarry();
    }

    fn built_in_usubborrow(&mut self) {
        self.builder.built_in_usubborrow();
    }

    fn built_in_umulextended(&mut self) {
        self.builder.built_in_umulextended();
    }

    fn built_in_imulextended(&mut self) {
        self.builder.built_in_imulextended();
    }

    fn built_in_texturesize(&mut self, with_lod: bool) {
        self.builder.built_in_texturesize(with_lod);
    }

    fn built_in_texturequerylod(&mut self) {
        self.builder.built_in_texturequerylod();
    }

    fn built_in_texelfetch(&mut self, with_lod_or_sample: bool) {
        self.builder.built_in_texelfetch(with_lod_or_sample);
    }

    fn built_in_texelfetchoffset(&mut self) {
        self.builder.built_in_texelfetchoffset();
    }

    fn built_in_rgb_2_yuv(&mut self) {
        self.builder.built_in_rgb_2_yuv();
    }

    fn built_in_yuv_2_rgb(&mut self) {
        self.builder.built_in_yuv_2_rgb();
    }

    fn built_in_dfdx(&mut self) {
        self.builder.built_in_dfdx();
    }

    fn built_in_dfdy(&mut self) {
        self.builder.built_in_dfdy();
    }

    fn built_in_fwidth(&mut self) {
        self.builder.built_in_fwidth();
    }

    fn built_in_interpolateatcentroid(&mut self) {
        self.builder.built_in_interpolateatcentroid();
    }

    fn built_in_interpolateatsample(&mut self) {
        self.builder.built_in_interpolateatsample();
    }

    fn built_in_interpolateatoffset(&mut self) {
        self.builder.built_in_interpolateatoffset();
    }

    fn built_in_atomiccounter(&mut self) {
        self.builder.built_in_atomiccounter();
    }

    fn built_in_atomiccounterincrement(&mut self) {
        self.builder.built_in_atomiccounterincrement();
    }

    fn built_in_atomiccounterdecrement(&mut self) {
        self.builder.built_in_atomiccounterdecrement();
    }

    fn built_in_atomicadd(&mut self) {
        self.builder.built_in_atomicadd();
    }

    fn built_in_atomicmin(&mut self) {
        self.builder.built_in_atomicmin();
    }

    fn built_in_atomicmax(&mut self) {
        self.builder.built_in_atomicmax();
    }

    fn built_in_atomicand(&mut self) {
        self.builder.built_in_atomicand();
    }

    fn built_in_atomicor(&mut self) {
        self.builder.built_in_atomicor();
    }

    fn built_in_atomicxor(&mut self) {
        self.builder.built_in_atomicxor();
    }

    fn built_in_atomicexchange(&mut self) {
        self.builder.built_in_atomicexchange();
    }

    fn built_in_atomiccompswap(&mut self) {
        self.builder.built_in_atomiccompswap();
    }

    fn built_in_imagesize(&mut self) {
        self.builder.built_in_imagesize();
    }

    fn built_in_imagestore(&mut self) {
        self.builder.built_in_imagestore();
    }

    fn built_in_imageload(&mut self) {
        self.builder.built_in_imageload();
    }

    fn built_in_imageatomicadd(&mut self) {
        self.builder.built_in_imageatomicadd();
    }

    fn built_in_imageatomicmin(&mut self) {
        self.builder.built_in_imageatomicmin();
    }

    fn built_in_imageatomicmax(&mut self) {
        self.builder.built_in_imageatomicmax();
    }

    fn built_in_imageatomicand(&mut self) {
        self.builder.built_in_imageatomicand();
    }

    fn built_in_imageatomicor(&mut self) {
        self.builder.built_in_imageatomicor();
    }

    fn built_in_imageatomicxor(&mut self) {
        self.builder.built_in_imageatomicxor();
    }

    fn built_in_imageatomicexchange(&mut self) {
        self.builder.built_in_imageatomicexchange();
    }

    fn built_in_imageatomiccompswap(&mut self) {
        self.builder.built_in_imageatomiccompswap();
    }

    fn built_in_pixellocalloadangle(&mut self) {
        self.builder.built_in_pixellocalloadangle();
    }

    fn built_in_pixellocalstoreangle(&mut self) {
        self.builder.built_in_pixellocalstoreangle();
    }

    fn built_in_memorybarrier(&mut self) {
        self.builder.built_in_memorybarrier();
    }

    fn built_in_memorybarrieratomiccounter(&mut self) {
        self.builder.built_in_memorybarrieratomiccounter();
    }

    fn built_in_memorybarrierbuffer(&mut self) {
        self.builder.built_in_memorybarrierbuffer();
    }

    fn built_in_memorybarrierimage(&mut self) {
        self.builder.built_in_memorybarrierimage();
    }

    fn built_in_barrier(&mut self) {
        self.builder.built_in_barrier();
    }

    fn built_in_memorybarriershared(&mut self) {
        self.builder.built_in_memorybarriershared();
    }

    fn built_in_groupmemorybarrier(&mut self) {
        self.builder.built_in_groupmemorybarrier();
    }

    fn built_in_emitvertex(&mut self) {
        self.builder.built_in_emitvertex();
    }

    fn built_in_endprimitive(&mut self) {
        self.builder.built_in_endprimitive();
    }

    // Built-in texture* operations
    fn built_in_texture(&mut self, with_compare: bool) {
        self.builder.built_in_texture(with_compare);
    }

    fn built_in_textureproj(&mut self) {
        self.builder.built_in_textureproj();
    }

    fn built_in_texturelod(&mut self, with_compare: bool) {
        self.builder.built_in_texturelod(with_compare);
    }

    fn built_in_textureprojlod(&mut self) {
        self.builder.built_in_textureprojlod();
    }

    fn built_in_texturebias(&mut self, with_compare: bool) {
        self.builder.built_in_texturebias(with_compare);
    }

    fn built_in_textureprojbias(&mut self) {
        self.builder.built_in_textureprojbias();
    }

    fn built_in_textureoffset(&mut self) {
        self.builder.built_in_textureoffset();
    }

    fn built_in_textureprojoffset(&mut self) {
        self.builder.built_in_textureprojoffset();
    }

    fn built_in_texturelodoffset(&mut self) {
        self.builder.built_in_texturelodoffset();
    }

    fn built_in_textureprojlodoffset(&mut self) {
        self.builder.built_in_textureprojlodoffset();
    }

    fn built_in_textureoffsetbias(&mut self) {
        self.builder.built_in_textureoffsetbias();
    }

    fn built_in_textureprojoffsetbias(&mut self) {
        self.builder.built_in_textureprojoffsetbias();
    }

    fn built_in_texturegrad(&mut self) {
        self.builder.built_in_texturegrad();
    }

    fn built_in_textureprojgrad(&mut self) {
        self.builder.built_in_textureprojgrad();
    }

    fn built_in_texturegradoffset(&mut self) {
        self.builder.built_in_texturegradoffset();
    }

    fn built_in_textureprojgradoffset(&mut self) {
        self.builder.built_in_textureprojgradoffset();
    }

    fn built_in_texturegather(&mut self) {
        self.builder.built_in_texturegather();
    }

    fn built_in_texturegathercomp(&mut self) {
        self.builder.built_in_texturegathercomp();
    }

    fn built_in_texturegatherref(&mut self) {
        self.builder.built_in_texturegatherref();
    }

    fn built_in_texturegatheroffset(&mut self) {
        self.builder.built_in_texturegatheroffset();
    }

    fn built_in_texturegatheroffsetcomp(&mut self) {
        self.builder.built_in_texturegatheroffsetcomp();
    }

    fn built_in_texturegatheroffsetref(&mut self) {
        self.builder.built_in_texturegatheroffsetref();
    }
}
