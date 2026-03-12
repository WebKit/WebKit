// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Traverser utilities for the IR.  Fundamentally, there are two types of traversers:
//
// - A visitor does a read-only traversal of the IR.
// - A transformer may mutate the IR at the instruction or block granularity.  The
//   instruction::make! and instruction::make_with_result_id! macros can be used to generate new
//   instructions during transformations.  To avoid excessive modifications to the IR, when
//   transforming an instruction that produces a given id, use instruction::make_with_result_id! on
//   the final instruction to produce the same id, leaving the rest of the IR intact.

use crate::ir::*;
use crate::*;

#[derive(Copy, Clone)]
#[cfg_attr(debug_assertions, derive(Debug))]
pub enum BlockKind {
    // Entry to the function
    Entry,
    // The true and false blocks of an if
    True,
    False,
    // The condition of a loop
    LoopCondition,
    // The body of a (do-)loop
    LoopBody,
    // The continue block of a loop
    Continue,
    // The case block of a switch, and the associated constant with the case (if any).
    Case(Option<ConstantId>),
    // Merge block of a divergent control flow.
    Merge,
}

pub mod visitor {
    use super::*;

    #[derive(PartialEq, Copy, Clone)]
    #[cfg_attr(debug_assertions, derive(Debug))]
    pub enum VisitAfter {
        SubBlocks,
        MergeBlock,
        Nothing,
    }

    pub const VISIT_SUB_BLOCKS: VisitAfter = VisitAfter::SubBlocks;
    pub const SKIP_TO_MERGE_BLOCK: VisitAfter = VisitAfter::MergeBlock;
    pub const STOP: VisitAfter = VisitAfter::Nothing;

    pub fn for_each_function<State, PreVisit, BlockVisit, PostVisit>(
        state: &mut State,
        function_entries: &Vec<Option<Block>>,
        pre_visit: PreVisit,
        block_visit: BlockVisit,
        post_visit: PostVisit,
    ) where
        PreVisit: Fn(&mut State, FunctionId),
        BlockVisit: Fn(&mut State, &Block, BlockKind, usize) -> VisitAfter,
        PostVisit: Fn(&mut State, FunctionId),
    {
        for (id, entry) in function_entries.iter().enumerate() {
            if entry.is_none() {
                // Skip over functions that have been dead-code eliminated.
                continue;
            }
            let func_id = FunctionId { id: id as u32 };
            pre_visit(state, func_id);
            visit(state, entry.as_ref().unwrap(), BlockKind::Entry, &block_visit, 0);
            // TODO(http://anglebug.com/349994211): post visit useful for functions?
            post_visit(state, func_id);
        }
    }

    fn block1_kind(parent_op: &OpCode) -> BlockKind {
        match parent_op {
            OpCode::If(_) => BlockKind::True,
            _ => BlockKind::LoopBody,
        }
    }
    fn block2_kind(parent_op: &OpCode) -> BlockKind {
        match parent_op {
            OpCode::If(_) => BlockKind::False,
            _ => BlockKind::Continue,
        }
    }

    pub fn visit<State, BlockVisit>(
        state: &mut State,
        block: &Block,
        kind: BlockKind,
        block_visit: &BlockVisit,
        indent_level: usize,
    ) where
        BlockVisit: Fn(&mut State, &Block, BlockKind, usize) -> VisitAfter,
    {
        let visit_after = block_visit(state, block, kind, indent_level);

        let visit_sub_blocks = visit_after == VISIT_SUB_BLOCKS;
        let visit_merge_block = visit_after != STOP;
        let parent_op = block.get_terminating_op();
        let sub_block_indent = indent_level + 1;

        if visit_sub_blocks {
            block.loop_condition.as_ref().inspect(|block| {
                visit(state, block, BlockKind::LoopCondition, block_visit, sub_block_indent)
            });
            block.block1.as_ref().inspect(|block| {
                visit(state, block, block1_kind(parent_op), block_visit, sub_block_indent)
            });
            block.block2.as_ref().inspect(|block| {
                visit(state, block, block2_kind(parent_op), block_visit, sub_block_indent)
            });
            if let OpCode::Switch(_, case_ids) = parent_op {
                block.case_blocks.iter().zip(case_ids).for_each(|(case, &case_id)| {
                    visit(state, case, BlockKind::Case(case_id), block_visit, sub_block_indent)
                });
            }
        }

        if visit_merge_block {
            block
                .merge_block
                .as_ref()
                .inspect(|merge| visit(state, merge, BlockKind::Merge, block_visit, indent_level));
        }
    }

    // A helper to visit the instructions (but also variables and inputs) of a block in a way
    // that is convenient for replicating what the block includes, such as when generating
    // code.
    //
    // This helper is used when something is generated out of the visited blocks.  To
    // facilitate that, `block_visit` returns a BlockResult representing the result of the
    // visit to each block.  These results are automatically aggregated when sub-blocks are
    // visited so that the caller doesn't have to do gymnastics to do the same.
    //
    // `block_visit` is used to look ahead in the block if needed, such as the variable list,
    // the input of the merge block etc, as well as mark the beginning of a new block.  Some
    // object representing the result of a visit to this block is returned.
    // `non_branch_instructions_visit` is called on all instructions of the block except the
    // branch instruction, before recursing into the sub-blocks.  The block's result object is
    // passed in.
    // `branch_instructions_visit` is called after the sub-blocks are visited and is only used
    // to visit the block's terminating branch instruction.  This instruction takes the result
    // objects for the sub-blocks to operate on.
    // `merge_result_block_chain` is called after all the blocks in a merge-chain are visited to
    // aggregate them as necessary.  The aggregated result is returned.
    pub fn visit_block_instructions<
        State,
        BlockResult,
        BlockVisit,
        NonBranchInstructionsVisit,
        BranchInstructionVisit,
        MergeResultBlockChain,
    >(
        state: &mut State,
        block: &Block,
        block_visit: &BlockVisit,
        non_branch_instructions_visit: &NonBranchInstructionsVisit,
        branch_instruction_visit: &BranchInstructionVisit,
        merge_result_block_chain: &MergeResultBlockChain,
    ) -> BlockResult
    where
        BlockVisit: Fn(&mut State, &Block) -> BlockResult,
        NonBranchInstructionsVisit: Fn(&mut State, &mut BlockResult, &[BlockInstruction]),
        BranchInstructionVisit: Fn(
            &mut State,
            &mut BlockResult,
            &OpCode,
            Option<BlockResult>,
            Option<BlockResult>,
            Option<BlockResult>,
            Vec<BlockResult>,
        ),
        MergeResultBlockChain: Fn(&mut State, Vec<BlockResult>) -> BlockResult,
    {
        let mut cur_block = block;
        let mut result_block_chain = Vec::new();
        loop {
            let mut this_block_result = block_visit(state, cur_block);

            // Visit the instructions first, except the branch instruction.  Any id that is
            // defined in this block may be used by the sub-blocks, so it must be visited
            // beforehand.
            non_branch_instructions_visit(
                state,
                &mut this_block_result,
                &cur_block.instructions[..cur_block.instructions.len() - 1],
            );

            // Generate instructions for the sub-blocks before processing the branch instruction.
            let loop_condition_block_result = cur_block.loop_condition.as_ref().map(|block| {
                visit_block_instructions(
                    state,
                    block,
                    block_visit,
                    non_branch_instructions_visit,
                    branch_instruction_visit,
                    merge_result_block_chain,
                )
            });
            let block1_result = cur_block.block1.as_ref().map(|block| {
                visit_block_instructions(
                    state,
                    block,
                    block_visit,
                    non_branch_instructions_visit,
                    branch_instruction_visit,
                    merge_result_block_chain,
                )
            });
            let block2_result = cur_block.block2.as_ref().map(|block| {
                visit_block_instructions(
                    state,
                    block,
                    block_visit,
                    non_branch_instructions_visit,
                    branch_instruction_visit,
                    merge_result_block_chain,
                )
            });
            let case_block_results = cur_block
                .case_blocks
                .iter()
                .map(|block| {
                    visit_block_instructions(
                        state,
                        block,
                        block_visit,
                        non_branch_instructions_visit,
                        branch_instruction_visit,
                        merge_result_block_chain,
                    )
                })
                .collect::<Vec<_>>();

            // Finally, visit the branch instruction after visiting the sub-blocks.
            branch_instruction_visit(
                state,
                &mut this_block_result,
                cur_block.get_terminating_op(),
                loop_condition_block_result,
                block1_result,
                block2_result,
                case_block_results,
            );
            result_block_chain.push(this_block_result);

            // Continue visiting the merge blocks.  They are logical continuations of the
            // parent block.
            if let Some(merge_block) = cur_block.merge_block.as_ref() {
                cur_block = merge_block;
            } else {
                break;
            }
        }

        merge_result_block_chain(state, result_block_chain)
    }
}

// How to transform an instruction after visiting it.
#[cfg_attr(debug_assertions, derive(Debug))]
pub enum Transform {
    // Keep the instruction unchanged.  Only use this in combination with the other transforms;
    // if `Keep` is the only transform, return an empty vector from the transformation callback
    // instead.
    Keep,
    // Remove the instruction.  Normally not needed, except if the only transformation is to
    // remove the instruction.  Otherwise, not specifying `Keep` automatically results in the
    // instruction being removed.
    Remove,
    // Add a new instruction.
    Add(BlockInstruction),
    // Add a new block.  The instructions of the new block are appended to the current block,
    // and the branch targets of the current block are set to the new block's.  The following
    // instructions are placed at the end of the merge chain.
    AddBlock(Block),
    // Declare a variable at the top of the block.  The variable itself should be added to the
    // IR's list of variables already.
    DeclareVariable(VariableId),
    // TODO(http://anglebug.com/349994211): one that removes variables? Best if transformations
    // change variables instead of deleting and adding new ones.  Ideally, removing variables
    // shouldn't be necessary.
}

// Convenience function to process an instruction::Result::Void and append it to the list of
// Transforms.
pub fn add_void_instruction(transforms: &mut Vec<Transform>, inst: instruction::Result) {
    match inst {
        instruction::Result::Void(op) => {
            transforms.push(Transform::Add(BlockInstruction::new_void(op)))
        }
        _ => panic!("Internal error: Expected instruction with no value"),
    };
}

// Convenience function to process a typed instruction::Result and append it to the list of
// Transforms.  Returns a TypedId representing the result.
pub fn add_typed_instruction(
    transforms: &mut Vec<Transform>,
    inst: instruction::Result,
) -> TypedId {
    if let instruction::Result::Register(id) = inst {
        transforms.push(Transform::Add(BlockInstruction::new_typed(id.id)));
    }

    inst.get_result_id()
}

// Variant of the above that create a single instruction, removing some boilerplate.
pub fn single_void_instruction(inst: instruction::Result) -> Vec<Transform> {
    let mut transforms = vec![];
    add_void_instruction(&mut transforms, inst);
    transforms
}
pub fn single_typed_instruction(inst: instruction::Result) -> Vec<Transform> {
    let mut transforms = vec![];
    add_typed_instruction(&mut transforms, inst);
    transforms
}

pub mod transformer {
    use super::*;

    pub fn for_each_function<State, PreVisit, TreeTransform>(
        state: &mut State,
        function_entries: &mut Vec<Option<Block>>,
        pre_visit: PreVisit,
        tree_transform: &TreeTransform,
    ) where
        PreVisit: Fn(&mut State, FunctionId),
        TreeTransform: Fn(&mut State, &mut Block),
    {
        for (id, entry) in function_entries.iter_mut().enumerate() {
            if entry.is_none() {
                // Skip over functions that have been dead-code eliminated.
                continue;
            }

            let func_id = FunctionId { id: id as u32 };
            pre_visit(state, func_id);

            // Let the transformer freely transform the function blocks as it sees fit.
            tree_transform(state, entry.as_mut().unwrap());
        }
    }

    // Run a function for each block of the tree, starting from a root node.  The function
    // receives the block to transform, which it is free to transform in any way.  It returns a
    // block for which the sub-blocks should be visited.  The block is visited once before the
    // sub-blocks are visited and once after.
    //
    // If a transformation turns one block into a tree of blocks, it may choose to make the
    // traversal skip over the new blocks by returning the merge block of the tree.  Otherwise,
    // it may choose to return the original block so the new blocks are revisited.
    pub fn for_each_block<State, BlockTransformPre, BlockTransformPost>(
        state: &mut State,
        block: &mut Block,
        block_transform_pre: &BlockTransformPre,
        block_transform_post: &BlockTransformPost,
    ) where
        for<'block> BlockTransformPre: Fn(&mut State, &'block mut Block) -> &'block mut Block,
        for<'block> BlockTransformPost: Fn(&mut State, &'block mut Block) -> &'block mut Block,
    {
        let mut cur_block = block;

        loop {
            // Visit the block.  It may be transformed to another (or a tree of blocks).  The
            // branch targets may have changed after this.
            cur_block = block_transform_pre(state, cur_block);

            // Visit the sub-blocks after transforming the root block.
            cur_block.for_each_sub_block_mut(state, &|state, sub_block| {
                for_each_block(state, sub_block, block_transform_pre, block_transform_post)
            });

            // Visit the block again after visiting the sub-blocks.  It may be transformed further.
            cur_block = block_transform_post(state, cur_block);

            // If there is a merge block, continue down the chain.
            if let Some(merge_block) = cur_block.merge_block.as_mut() {
                cur_block = merge_block;
            } else {
                break;
            }
        }
    }

    pub fn for_each_instruction<State, InstTransform>(
        state: &mut State,
        function_entries: &mut Vec<Option<Block>>,
        inst_transform: &InstTransform,
    ) where
        InstTransform: Fn(&mut State, &BlockInstruction) -> Vec<Transform>,
    {
        for_each_function(state, function_entries, |_, _| {}, &|state, entry| {
            for_each_block(
                state,
                entry,
                &|state, block| transform_block(state, block, inst_transform),
                &|_, block| block,
            )
        });
    }

    // Used for common transformations, where typically instructions are added or changed
    // (including adding complex control flow), but the original control flow of the shader is
    // not changed.  Some transformations may need the latter, such as dead-code elimintation,
    // but such transformations are likely better off directly manipulating the IR as the
    // following helper may be too limiting.
    //
    // To make sure mistakes are not made, the branch instruction in the block is not allowed
    // to be transformed for this reason.
    // TODO(http://anglebug.com/349994211): If `Transform` is sufficiently expanded (for example to
    // manipulate `block1`, `block2`, `input` etc), this might just work for complex transforms
    // like DCE too, but we'll see if ever needed.
    //
    // The transformation function is given an instruction, and it is expected to return a set
    // of transformations to be applied to it.  If an empty vector is returned, the instruction
    // is kept as is; the empty vector is used because this will be the most common result.
    pub fn transform_block<'block, State, InstTransform>(
        state: &mut State,
        block: &'block mut Block,
        inst_transform: &InstTransform,
    ) -> &'block mut Block
    where
        InstTransform: Fn(&mut State, &BlockInstruction) -> Vec<Transform>,
    {
        for (i, instruction) in block.instructions.iter().enumerate() {
            let result = inst_transform(state, instruction);
            if result.len() > 0 {
                if result.len() > 0 {
                    // Transforming branches is not yet supported, but it is allowed to return
                    // [..., Keep], i.e. it is valid to prepend to the branch instruction.
                    debug_assert!(
                        !instruction.is_branch()
                            || matches!(result.last().unwrap(), Transform::Keep)
                    );
                    // Don't return [Keep], just return [].
                    debug_assert!(!(result.len() == 1 && matches!(result[0], Transform::Keep)));
                }

                // If any changes are to be made, use a helper to recreate the block.  This
                // would move the instructions so far visited as untransformed, and starts
                // applying modifications going forward.
                return transform_block_from(state, block, inst_transform, i, result);
            }
        }

        // If the loop is not broken out of, nothing has changed and the block is not transformed.
        block
    }

    // Transform the block starting with the first instruction that was definitely transformed.
    //
    // Once the transformation is finished, the block may have turned into a tree of blocks;
    // the last block in the merge chain is returned.
    //
    // - The input to the original block becomes the input to the root block of the tree.
    // - The variables declared in the original block are also declared in the root block.
    // - The branch targets of the original block (based on the original branch instruction, which
    //   is not transformable) are set in the last block in the merge chain.
    fn transform_block_from<'block, State, InstTransform>(
        state: &mut State,
        block: &'block mut Block,
        inst_transform: &InstTransform,
        intact_count: usize,
        first_transform_result: Vec<Transform>,
    ) -> &'block mut Block
    where
        InstTransform: Fn(&mut State, &BlockInstruction) -> Vec<Transform>,
    {
        let mut new_block = Block::new();
        let mut instructions = std::mem::replace(&mut block.instructions, vec![]);

        // Inherit the block input and variables in the new block.
        new_block.input = block.input;
        new_block.variables = std::mem::replace(&mut block.variables, vec![]);
        // Keep the branch targets in a temp block for now.  If the transformation results in a
        // tree of blocks, these branch targets are set to the tail of the merge chain.
        let mut branch_targets = Block::new();
        inherit_branch_targets(&mut branch_targets, block);

        // Take the first `intact_count` instructions as-is.
        let to_transform = instructions.split_off(intact_count);
        new_block.instructions = instructions;

        let mut to_transform = to_transform.into_iter();

        {
            // The first of the non-intact instructions is already transformed, so apply the
            // transformation to it right away.
            let mut cur_block = apply_transforms(
                &mut new_block,
                to_transform.next().unwrap(),
                first_transform_result,
            );

            // Apply the transformation to the rest of the instructions.
            for instruction in to_transform {
                let result = inst_transform(state, &instruction);
                cur_block = apply_transforms(cur_block, instruction, result);
            }
        }

        // Overwrite the old block and return the the tail of the merge chain.
        *block = new_block;
        let cur_block = block.get_merge_chain_last_block_mut();
        inherit_branch_targets(cur_block, &mut branch_targets);

        cur_block
    }

    fn inherit_branch_targets(new_block: &mut Block, old_block: &mut Block) {
        debug_assert!(new_block.merge_block.is_none());
        debug_assert!(new_block.loop_condition.is_none());
        debug_assert!(new_block.block1.is_none());
        debug_assert!(new_block.block2.is_none());
        debug_assert!(new_block.case_blocks.is_empty());

        new_block.merge_block = old_block.merge_block.take();
        new_block.loop_condition = old_block.loop_condition.take();
        new_block.block1 = old_block.block1.take();
        new_block.block2 = old_block.block2.take();
        new_block.case_blocks = std::mem::replace(&mut old_block.case_blocks, vec![]);
    }

    // After visiting an instruction, apply the transformations that are requested by the
    // callback.
    fn apply_transforms<'block>(
        new_block: &'block mut Block,
        instruction: BlockInstruction,
        transform_result: Vec<Transform>,
    ) -> &'block mut Block {
        // An empty transformation result means the instruction should be kept as-is.
        if transform_result.is_empty() {
            new_block.instructions.push(instruction);
            return new_block;
        }

        let mut cur_block = new_block;
        let mut instruction = instruction;

        for transform in transform_result {
            match transform {
                Transform::Keep => {
                    // There should only be one `Keep`, but to satisfy Rust, make sure
                    // `instruction` stays valid.  If `Keep` is encountered again, the branch
                    // instruction built below will cause panics later (because it's a branch
                    // in the middle of the block).
                    cur_block.instructions.push(std::mem::replace(
                        &mut instruction,
                        BlockInstruction::new_control_flow(OpCode::NextBlock),
                    ));
                }
                Transform::Remove => {
                    // Nothing to do.  Just ignoring the instruction will do.
                }
                Transform::Add(inst) => {
                    debug_assert!(!inst.is_branch());
                    cur_block.instructions.push(inst);
                }
                Transform::DeclareVariable(variable_id) => {
                    cur_block.variables.push(variable_id);
                }
                Transform::AddBlock(mut block) => {
                    debug_assert!(block.input.is_none());

                    // Append the instructions/variables of the new block to this block, then
                    // set the branch targets to the `block`'s.  Future transformations
                    // continue in the merge block of the added block (if any).
                    cur_block
                        .instructions
                        .extend(std::mem::replace(&mut block.instructions, vec![]));
                    cur_block.variables.extend(std::mem::replace(&mut block.variables, vec![]));
                    inherit_branch_targets(cur_block, &mut block);

                    cur_block = cur_block.get_merge_chain_last_block_mut();
                }
            }
        }

        cur_block
    }
}
