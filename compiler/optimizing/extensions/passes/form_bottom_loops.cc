/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2015, Intel Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to the
 * source code ("Material") are owned by Intel Corporation or its suppliers or
 * licensors. Title to the Material remains with Intel Corporation or its suppliers
 * and licensors. The Material contains trade secrets and proprietary and
 * confidential information of Intel or its suppliers and licensors. The Material
 * is protected by worldwide copyright and trade secret laws and treaty provisions.
 * No part of the Material may be used, copied, reproduced, modified, published,
 * uploaded, posted, transmitted, distributed, or disclosed in any way without
 * Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery of
 * the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be express
 * and approved by Intel in writing.
 *
 */
#include "form_bottom_loops.h"

#include "cloning.h"
#include "ext_utility.h"
#include "graph_x86.h"
#include "loop_iterators.h"

// For debugging purposes.
#ifdef HAVE_ANDROID_OS
#include "cutils/properties.h"
#endif

namespace art {

void HFormBottomLoops::Run() {
  HGraph_X86* graph = GRAPH_TO_GRAPH_X86(graph_);
  HLoopInformation_X86* graph_loop = graph->GetLoopInformation();

  PRINT_PASS_OSTREAM_MESSAGE(this, "Begin: " << GetMethodName(graph));

  // Walk all the inner loops in the graph.
  bool changed = false;
  for (HOnlyInnerLoopIterator it(graph_loop); !it.Done(); it.Advance()) {
    HLoopInformation_X86* loop = it.Current();
    HBasicBlock* pre_header = loop->GetPreHeader();

    PRINT_PASS_OSTREAM_MESSAGE(this, "Visit " << loop->GetHeader()->GetBlockId()
                                     << ", preheader = " << pre_header->GetBlockId());

    // The exit block from the loop.
    HBasicBlock* loop_header = loop->GetHeader();
    HBasicBlock* exit_block = loop->GetExitBlock();

    if (!ShouldTransformLoop(loop, loop_header, exit_block)) {
      // Loop is already correct, or not valid for rewrite.
      continue;
    }

    RewriteLoop(loop, loop_header, exit_block);
    MaybeRecordStat(MethodCompilationStat::kIntelFormBottomLoop);
    changed = true;
  }

  if (changed) {
    // Rebuild the loop data structures.
    graph->RebuildDomination();
  }
  PRINT_PASS_OSTREAM_MESSAGE(this, "End: " << GetMethodName(graph));
}

static bool IsSingleGotoBackEdge(HBasicBlock* bb) {
  HLoopInformation* loop = bb->GetLoopInformation();
  return bb->IsSingleGoto()
         && (loop != nullptr && loop->IsBackEdge(*bb));
}

bool HFormBottomLoops::ShouldTransformLoop(HLoopInformation_X86* loop,
                                           HBasicBlock* loop_header,
                                           HBasicBlock* exit_block) {
  if (exit_block == nullptr) {
    // We need exactly 1 exit block from the loop.
    PRINT_PASS_MESSAGE(this, "Too many or too few exit blocks");
    return false;
  }

  // Exit block is alone and it always has one predecessor due to
  // critical edge elimination.
  DCHECK_EQ(exit_block->GetPredecessors().size(), static_cast<size_t>(1));
  HBasicBlock* loop_to_exit = exit_block->GetPredecessors()[0];

  HBasicBlock* first_back_edge = loop->GetBackEdges()[0];

  // Is this a top tested loop?
  HInstruction* last_insn = loop_header->GetLastInstruction();
  DCHECK(last_insn != nullptr);
  if (!last_insn->IsIf()) {
    PRINT_PASS_MESSAGE(this, "Loop header doesn't end with HIf");

    // Perhaps this loop is already bottom tested.
    // We must ensure that:
    // 1) loop_to_exit ends up with if.
    // 2) There is only one back edge.
    // 3) back edge is a successor of loop_to_exit.
    // 4) back edge is single goto.
    bool is_bottom_tested =
      (loop_to_exit->GetLastInstruction()->IsIf()) &&
      (loop->NumberOfBackEdges() == 1) &&
      (std::find(first_back_edge->GetPredecessors().begin(),
                  first_back_edge->GetPredecessors().end(),
                  loop_to_exit) != first_back_edge->GetPredecessors().end()) &&
      (IsSingleGotoBackEdge(first_back_edge));

    if (is_bottom_tested) {
      PRINT_PASS_MESSAGE(this, "Loop is already bottom tested");
      loop->SetBottomTested(true);
    }

    return false;
  }

  // We don't know how to rewrite a loop with multiple back edges at this time.
  if (loop->NumberOfBackEdges() != 1) {
    PRINT_PASS_MESSAGE(this, "More than one back edge");
    return false;
  }

  // Does the loop header exit the loop, making it top tested?
  if (loop_to_exit == loop_header) {
    // Perhaps this loop is already bottom tested.
    // We must ensure that:
    // 1) loop_to_exit ends up with if - already done.
    // 2) There is only one back edge - checked above.
    // 3) back edge is a successor of loop_to_exit.
    // 4) back edge is single goto.
    bool is_bottom_tested =
        (std::find(first_back_edge->GetPredecessors().begin(),
                    first_back_edge->GetPredecessors().end(),
                    loop_to_exit) != first_back_edge->GetPredecessors().end()) &&
        (IsSingleGotoBackEdge(first_back_edge));
    if (is_bottom_tested) {
      // More complicated single block loop.
      PRINT_PASS_MESSAGE(this, "Complex Loop is already bottom tested (after gotos)");
      loop->SetBottomTested(true);
      return false;
    }
  } else {
    PRINT_PASS_MESSAGE(this, "Loop header doesn't exit the loop");
    return false;
  }

  // Are the instructions in the header okay?
  if (!CheckLoopHeader(loop_header)) {
    return false;
  }

  // Leave support for debugging the transformation in debug builds only.
  // Only works with -j1.
  if (kIsDebugBuild) {
    static int max = -1;
    static int count = 0;
    if (max == -1) {
#ifdef HAVE_ANDROID_OS
      char buff[PROPERTY_VALUE_MAX];
      if (property_get("dex2oat.bottom.max", buff, "1000000") > 0) {
        max = atoi(buff);
      }
#else
      char* p = getenv("BOTTOM_MAX");
      max = p ? atoi(p) : 1000000;
#endif
    }
    if (++count > max) {
      PRINT_PASS_MESSAGE(this, "MAX transform count exceeded");
      return false;
    }
  }

  PRINT_PASS_MESSAGE(this, "Loop gate passed");
  // Looks fine to go.
  return true;
}

void HFormBottomLoops::RewriteLoop(HLoopInformation_X86* loop,
                                   HBasicBlock* loop_header,
                                   HBasicBlock* exit_block) {
  // Paranoia: we have ensured it in gate.
  DCHECK(loop != nullptr);
  DCHECK(loop_header != nullptr);
  DCHECK(exit_block != nullptr);

  HGraph_X86* graph = GRAPH_TO_GRAPH_X86(graph_);
  HBasicBlock* pre_header = loop->GetPreHeader();

  PRINT_PASS_OSTREAM_MESSAGE(this, "Rewrite loop " << loop_header->GetBlockId()
                                    << ", preheader = " << pre_header->GetBlockId());
  PRINT_PASS_OSTREAM_MESSAGE(this, "Exit block = " << exit_block->GetBlockId());

  // Find the first block in the loop after the loop header.  There must be
  // one due to our pre-checks.
  DCHECK(loop_header->GetLastInstruction() != nullptr);
  HIf* if_insn = loop_header->GetLastInstruction()->AsIf();
  DCHECK(if_insn != nullptr);
  bool first_successor_is_exit = if_insn->IfTrueSuccessor() == exit_block;

  HBasicBlock* first_block = first_successor_is_exit
                               ? if_insn->IfFalseSuccessor()
                               : if_insn->IfTrueSuccessor();

  DCHECK(first_block != nullptr);

  // Move the SuspendCheck from the loop header to the first block.
  HSuspendCheck* suspend_check = loop->GetSuspendCheck();
  if (suspend_check != nullptr) {
    PRINT_PASS_OSTREAM_MESSAGE(this, "Moving suspend check to block "
                                     << first_block->GetBlockId());
    suspend_check->MoveBefore(first_block->GetFirstInstruction());
  }

  // Get loop back edge.
  HBasicBlock* back_block = loop->GetBackEdges()[0];

  // Fiil the context information that will be required in future.
  FBLContext context(loop,
                     pre_header,
                     loop_header,
                     first_block,
                     back_block,
                     exit_block);

  // Initialize all maps.
  PrepareForNewLoop();

  // Trahsform the CFG.
  DoCFGTransformation(graph, context, first_successor_is_exit);

  // First, clone all instructions to the end of back edge.
  HInstructionCloner cloner(graph);
  CloneInstructions(context, cloner);

  // After this transformation, the structure of the loop is following:
  //   [Preheader]
  //        |
  //        V
  //       [H]---> Exit
  //        |
  //        V
  //  ---->[B]  // Was back edge before.
  //  |     |
  //  |     V
  //  -----[C]---> Exit
  // Here H denotes former Header,
  //      B denotes former Back Edge,
  //      C denotes Clone of Header.
}


bool HFormBottomLoops::CheckLoopHeader(HBasicBlock* loop_header) {
  // Walk through the instructions in the loop, looking for instructions that aren't
  // clonable, or that have results that are used outside of this block.
  HInstructionCloner cloner(GRAPH_TO_GRAPH_X86(graph_), false);

  for (HInstructionIterator inst_it(loop_header->GetInstructions());
       !inst_it.Done();
       inst_it.Advance()) {
    HInstruction* insn = inst_it.Current();

    PRINT_PASS_OSTREAM_MESSAGE(this, "Look at: " << insn);

    switch (insn->GetKind()) {
      case HInstruction::kSuspendCheck:
        // Special case SuspendCheck.  We don't care about it.
        continue;
      case HInstruction::kIf:
        // If the instruction is HIf, then we must have checked it in the gate.
        continue;
      default:
        if (insn->IsControlFlow() && !insn->CanThrow()) {
          // We can't handle any control flow except an HIf.
          PRINT_PASS_MESSAGE(this, "Instruction has control flow");
          return false;
        }

        // Can we clone this instruction?
        insn->Accept(&cloner);
        if (!cloner.AllOkay()) {
          PRINT_PASS_MESSAGE(this, "Unable to clone");
          return false;
        }
        break;
    }
  }

  // Cycled Phis like:
  //
  //   phi_1 = Phi(x, phi_2);
  //   phi_2 = Phi(y, phi_1);
  //
  // Cause us problems. We cannot add them correctly.
  // So for now we reject loops where this situation
  // is possible. TODO: Consider covering it in future.
  std::unordered_set<HInstruction*> seen_phis;
  bool looks_phis_forward = false;
  bool looks_phis_backward = false;

  // Make sure that Phis don't form cycles.
  for (HInstructionIterator inst_it(loop_header->GetPhis());
       !inst_it.Done();
       inst_it.Advance()) {
    HInstruction* phi = inst_it.Current();
    if (phi->InputCount() == 2u) {
      HInstruction* in_2 = phi->InputAt(1)->AsPhi();
      if (in_2 != nullptr && in_2->GetBlock() == loop_header) {
        if (seen_phis.find(in_2) == seen_phis.end()) {
          looks_phis_forward = true;
        } else {
          looks_phis_backward = true;
        }
      }
    }

    if (looks_phis_forward && looks_phis_backward) {
      PRINT_PASS_MESSAGE(this, "Rejecting due to cycled Phis");
      return false;
    }

    seen_phis.insert(phi);
  }

  // All instructions are okay.
  return true;
}

void HFormBottomLoops::DoCFGTransformation(HGraph_X86* graph,
                                           FBLContext& context,
                                           bool first_successor_is_exit) {
  // Retrieve all information from the context.
  HLoopInformation_X86* loop = context.loop_;
  HBasicBlock* pre_header = context.pre_header_block_;
  HBasicBlock* loop_header = context.header_block_;
  HBasicBlock* first_block = context.first_block_;
  HBasicBlock* back_block = context.back_block_;
  HBasicBlock* exit_block = context.exit_block_;


  PRINT_PASS_OSTREAM_MESSAGE(this, "Back block = " << back_block->GetBlockId());

  // Fix the successors of the block.
  if (first_successor_is_exit) {
    PRINT_PASS_OSTREAM_MESSAGE(this, "First successor is exit");
    back_block->ReplaceSuccessor(loop_header, exit_block);
    back_block->AddSuccessor(first_block);
  } else {
    PRINT_PASS_OSTREAM_MESSAGE(this, "Second successor is exit");
    back_block->ReplaceSuccessor(loop_header, first_block);
    back_block->AddSuccessor(exit_block);
  }

  HLoopInformation_X86* pre_header_loop =
    LOOPINFO_TO_LOOPINFO_X86(pre_header->GetLoopInformation());

  // Ensure that the exit block doesn't get messed up by SplitCriticalEdge.
  HBasicBlock* split_exit_block = graph->CreateNewBasicBlock();
  PRINT_PASS_OSTREAM_MESSAGE(this, "New loop exit block " << split_exit_block->GetBlockId());
  split_exit_block->AddInstruction(new (graph->GetArena()) HGoto());
  if (pre_header_loop != nullptr) {
    pre_header_loop->AddToAll(split_exit_block);
  }
  split_exit_block->InsertBetween(back_block, exit_block);

  // We also need to ensure that the branch around the loop isn't a critical edge.
  HBasicBlock* around_block = graph->CreateNewBasicBlock();
  PRINT_PASS_OSTREAM_MESSAGE(this, "New around to exit block " << around_block->GetBlockId());
  around_block->AddInstruction(new (graph->GetArena()) HGoto());
  if (pre_header_loop != nullptr) {
    pre_header_loop->AddToAll(around_block);
  }
  around_block->InsertBetween(loop_header, exit_block);

  // We also need to ensure that the branch to the top of the loop isn't a critical edge.
  HBasicBlock* top_block = graph->CreateNewBasicBlock();
  PRINT_PASS_OSTREAM_MESSAGE(this, "New around to top block " << top_block->GetBlockId());
  top_block->AddInstruction(new (graph->GetArena()) HGoto());
  loop->AddToAll(top_block);
  top_block->InsertBetween(back_block, first_block);
  loop->ReplaceBackEdge(back_block, top_block);
  DCHECK(top_block->GetLoopInformation() == loop);

  // Ensure that a new loop header doesn't get messed up by SplitCriticalEdge.
  if (loop_header->GetSuccessors().size() > 1 &&
      first_block->GetPredecessors().size() > 1) {
    HBasicBlock* new_block = graph->CreateNewBasicBlock();
    PRINT_PASS_OSTREAM_MESSAGE(this, "Fixing up loop pre-header for " <<
                                     first_block->GetBlockId() <<
                                     ", new block = " << new_block->GetBlockId());
    new_block->AddInstruction(new (graph->GetArena()) HGoto());
    if (pre_header_loop != nullptr) {
      pre_header_loop->AddToAll(new_block);
    }
    new_block->InsertBetween(loop_header, first_block);
  }

  // Fix the loop.
  loop->SetHeader(first_block);
  loop->Remove(loop_header);
  loop_header->SetLoopInformation(pre_header_loop);
  loop->SetBottomTested(true);

  // This is a required condition because we may create Phis
  // in the exit block that use instructions from Header.
  DCHECK(loop_header->Dominates(exit_block));
}

HPhi* HFormBottomLoops::NewPhi(HInstruction* in_1,
                               HInstruction* in_2,
                               HBasicBlock* block) {
  auto arena = graph_->GetArena();
  HPhi* phi =  new (arena) HPhi(arena,
                                graph_->GetNextInstructionId(),
                                2u,
                                HPhi::ToPhiType(in_1->GetType()));

  phi->SetRawInputAt(0, in_1);

  if (in_2 != nullptr && block != nullptr) {
    phi->SetRawInputAt(1, in_2);
    block->AddPhi(phi);
    PRINT_PASS_OSTREAM_MESSAGE(this, "Created new " << phi
                                     << " in block " << block->GetBlockId());
  }

  return phi;
}

void HFormBottomLoops::CloneInstructions(FBLContext& context,
                                         HInstructionCloner& cloner) {
  HBasicBlock* loop_header = context.header_block_;
  HBasicBlock* back_block = context.back_block_;

  PRINT_PASS_OSTREAM_MESSAGE(this, "Cloning instructions from header block #"
                                   << loop_header->GetBlockId()
                                   << " to the end of back edge "
                                   << back_block->GetBlockId());

  // Find the Goto at the end of the back block.
  HInstruction* goto_insn = back_block->GetLastInstruction();
  DCHECK(goto_insn != nullptr);
  DCHECK(goto_insn->IsGoto());
  DCHECK_EQ(back_block->GetSuccessors().size(), 2u);

  PRINT_PASS_OSTREAM_MESSAGE(this, "Removing GOTO instruction " << goto_insn);
  back_block->RemoveInstruction(goto_insn);

  // Create the cloned instructions.
  for (HInstructionIterator inst_it(loop_header->GetInstructions());
                            !inst_it.Done();
                            inst_it.Advance()) {
    HInstruction* insn = inst_it.Current();

    // Special case for HLoadClass, we want to clone it to back branch and
    // the original node will go to pre-header which dominates the back
    // branch, so instead of cloning we can use just original HLoadClass node.
    if (insn->IsLoadClass()) {
      cloner.AddCloneManually(insn, insn);
    } else {
      // Clone the instruction.
      insn->Accept(&cloner);
      HInstruction* cloned_insn = cloner.GetClone(insn);

      // Add the cloned instruction to the back block.
      if (cloned_insn != nullptr) {
        if (cloned_insn->GetBlock() == nullptr) {
          back_block->AddInstruction(cloned_insn);
          clones_.insert(cloned_insn);
          FixHeaderInsnUses(insn, cloned_insn, context);
        }

        PRINT_PASS_OSTREAM_MESSAGE(this, "Clone " << insn << " to " << cloned_insn);
      }
    }
  }

  HInstruction* if_insn = back_block->GetLastInstruction();
  DCHECK(if_insn != nullptr);
  // Check that in the end we have an IF instruction.
  DCHECK(if_insn->IsIf()) << "The last instruction of the back"
                         << " block is expected to be IF ("
                         << "found " <<  if_insn << ")";

  std::vector<HPhi*> to_remove;
  // Now move Phi nodes to the first block.
  for (HInstructionIterator inst_it(loop_header->GetPhis());
                            !inst_it.Done();
                            inst_it.Advance()) {
    HPhi* phi = inst_it.Current()->AsPhi();

    DCHECK_EQ(phi->InputCount(), 2u);
    FixHeaderPhisUses(phi, context);
    to_remove.push_back(phi);
  }

  for (auto phi : to_remove) {
    // We don't check !HasUses here because obsolete Phis
    // may still use each other.
    PRINT_PASS_OSTREAM_MESSAGE(this, "Removing obsolete phi " << phi);
    // Since they can use one another, we should prepare them
    // for deleting to avoid crashes in future making this hack.
    phi->ReplaceInput(phi->InputAt(0), 1);
  }

  for (auto phi : to_remove) {
    loop_header->RemovePhi(phi);
  }
}

void HFormBottomLoops::FixHeaderInsnUses(HInstruction* insn,
                                         HInstruction* clone,
                                         FBLContext& context) {
  HLoopInformation_X86* loop = context.loop_;
  HBasicBlock* loop_header = context.header_block_;
  HBasicBlock* first_block = context.first_block_;
  HBasicBlock* exit_block = context.exit_block_;
  bool has_header_phi_use = false;

  PRINT_PASS_OSTREAM_MESSAGE(this, "Fix uses for header instruction " << insn);
  // Fixup uses of instructions in header according to the following rules:
  // 1. use in header -> Do nothing;
  // 2. use in first block -> Replace with Phi(insn, clone) in first block;
  // 3. use is a clone -> Replace with clone;
  // 4. use is in/after exit -> Replace with Phi(insn, clone) in exit block.
  for (HUseIterator<HInstruction*> use_it(insn->GetUses());
                                   !use_it.Done();
                                   use_it.Advance()) {
    auto use = use_it.Current();
    HInstruction* user = use->GetUser();
    size_t index = use->GetIndex();
    HBasicBlock* user_block = user->GetBlock();
    HInstruction* new_input = nullptr;

    // There is no point to do something with uses in the same block.
    if (user_block != loop_header) {
      if (loop->Contains(*user_block)) {
        // Back edge.
        DCHECK(clones_.find(user) == clones_.end());
        new_input = GetHeaderFixup(insn, clone, header_fixup_inside_, first_block);
      } else {
        // Exit.
        new_input = GetHeaderFixup(insn, clone, header_fixup_outside_, exit_block);
      }
    } else if (user->IsPhi()) {
      has_header_phi_use = true;
    }

    if (new_input != nullptr) {
      ReplaceInput(user, new_input, index);
    }
  }

  for (HUseIterator<HEnvironment*> use_it(insn->GetEnvUses());
                                   !use_it.Done();
                                   use_it.Advance()) {
    auto use = use_it.Current();
    HEnvironment* env_user = use->GetUser();
    size_t index = use->GetIndex();
    HInstruction* user = env_user->GetHolder();
    HBasicBlock* user_block = user->GetBlock();
    HInstruction* new_input = nullptr;

    // There is no point to do something with uses in the same block.
    if (user_block != loop_header) {
      if (loop->Contains(*user_block)) {
        // Back edge.
        new_input = GetHeaderFixup(insn, clone, header_fixup_inside_, first_block);
      } else {
        // Exit.
        new_input = GetHeaderFixup(insn, clone, header_fixup_outside_, exit_block);
      }
    } else if (user->IsPhi()) {
      has_header_phi_use = true;
    }

    if (new_input != nullptr) {
      ReplaceEnvInput(env_user, new_input, index);
    }
  }

  if (has_header_phi_use) {
    // We will need this for Phi fixup in future.
    GetHeaderFixup(insn, clone, header_fixup_inside_, first_block);
  }
}

void HFormBottomLoops::FixHeaderPhisUses(HPhi* phi,
                                         FBLContext& context) {
  HLoopInformation_X86* loop = context.loop_;
  HBasicBlock* loop_header = context.header_block_;
  HBasicBlock* first_block = context.first_block_;
  HBasicBlock* exit_block = context.exit_block_;

  PRINT_PASS_OSTREAM_MESSAGE(this, "Fix uses for header phi " << phi);
  // Fixup uses of phis in header according to the following rules:
  // 1. use in header -> Do nothing for phi, replace with phi(0) for insn;
  // 2. use in first block -> Replace with Phi(phi(0), phi') in first block;
  // 3. use is a clone -> Replace with phi';
  // 4. use is in/after exit -> Replace with Phi(phi(0), phi') in exit block.
  for (HUseIterator<HInstruction*> use_it(phi->GetUses());
                                   !use_it.Done();
                                   use_it.Advance()) {
    auto use = use_it.Current();
    HInstruction* user = use->GetUser();
    size_t index = use->GetIndex();
    HBasicBlock* user_block = user->GetBlock();
    HInstruction* new_input = nullptr;

    if (user_block != loop_header) {
      if (clones_.find(user) == clones_.end()) {
        if (loop->Contains(*user_block)) {
          // Back edge.
          new_input = GetPhiInterlaceFixup(phi, context, interlace_phi_fixup_inside_, first_block);
        } else {
          // Exit.
          new_input = GetPhiInterlaceFixup(phi, context, interlace_phi_fixup_outside_, exit_block);
        }
      } else {
        // Clone.
        new_input = GetPhiFixup(phi, context);
      }
    } else if (!user->IsPhi()) {
      // Header.
      new_input = phi->InputAt(0);
    }

    if (new_input != nullptr) {
      ReplaceInput(user, new_input, index);
    }
  }

  for (HUseIterator<HEnvironment*> use_it(phi->GetEnvUses());
                                   !use_it.Done();
                                   use_it.Advance()) {
    auto use = use_it.Current();
    HEnvironment* env_user = use->GetUser();
    size_t index = use->GetIndex();
    HInstruction* user = env_user->GetHolder();
    HBasicBlock* user_block = user->GetBlock();
    HInstruction* new_input = nullptr;

    if (user_block != loop_header) {
      if (clones_.find(user) == clones_.end()) {
        if (loop->Contains(*user_block)) {
          // Back edge.
          new_input = GetPhiInterlaceFixup(phi, context, interlace_phi_fixup_inside_, first_block);
        } else {
          // Exit.
          new_input = GetPhiInterlaceFixup(phi, context, interlace_phi_fixup_outside_, exit_block);
        }
      } else {
        // Clone.
        new_input = GetPhiFixup(phi, context);
      }
    } else if (!user->IsPhi()) {
      // Header.
      new_input  = phi->InputAt(0);
    }

    if (new_input != nullptr) {
      ReplaceEnvInput(env_user, new_input, index);
    }
  }
}

HInstruction* HFormBottomLoops::GetPhiFixup(HPhi* phi,
                                            FBLContext& context) {
  auto iter = phi_fixup_.find(phi);

  if (iter != phi_fixup_.end()) {
    return iter->second;
  }

  HBasicBlock* loop_header = context.header_block_;
  HBasicBlock* first_block = context.first_block_;

  HInstruction* fixup = nullptr;
  HInstruction* phi_1 = phi->InputAt(1);

  // Phi fixup phi' is defined by following rules:
  //  phi' = phi(1),
  //    if phi(1) in old back edge or old preheader;
  //  phi' = Phi(phi(1), clone(phi(1))),
  //    if phi(1) is an insn from old header;
  //  phi' = Phi(phi(1)(0), phi(1)'),
  //    if phi(1) is a Phi from old header.
  if (phi_1->GetBlock() == loop_header) {
    if (phi_1->IsPhi()) {
      // Header phi.
      fixup = GetPhiInterlaceFixup(phi_1->AsPhi(), context, interlace_phi_fixup_inside_, first_block);
    } else {
      // Header.
      auto iter_2 = header_fixup_inside_.find(phi_1);
      DCHECK(iter_2 != header_fixup_inside_.end());
      fixup = iter_2->second;
    }
  } else {
    // Back edge & PreHeader.
    fixup = phi_1;
  }

  phi_fixup_[phi] = fixup;
  return fixup;
}

HInstruction* HFormBottomLoops::GetPhiInterlaceFixup(HPhi* phi,
                                                     FBLContext& context,
                                                     InstrToInstrMap& mapping,
                                                     HBasicBlock* block) {
  auto iter = mapping.find(phi);

  if (iter != mapping.end()) {
    return iter->second;
  }

  HPhi* fixup = NewPhi(phi->InputAt(0));
  mapping[phi] = fixup;

  HInstruction* in_2 = GetPhiFixup(phi, context);

  DCHECK(in_2->GetBlock() != nullptr);
  fixup->SetRawInputAt(1, in_2);
  block->AddPhi(fixup);

  PRINT_PASS_OSTREAM_MESSAGE(this, "Created interlace phi fixup for " << phi
                                    << " in block " << block->GetBlockId());
  return fixup;
}

HInstruction* HFormBottomLoops::GetHeaderFixup(HInstruction* insn,
                                               HInstruction* clone,
                                               InstrToInstrMap& mapping,
                                               HBasicBlock* block) {
  auto iter = mapping.find(insn);

  if (iter != mapping.end()) {
    return iter->second;
  }

  HPhi* fixup = NewPhi(insn, clone, block);
  mapping[insn] = fixup;

  PRINT_PASS_OSTREAM_MESSAGE(this, "Created header fixup for " << insn
                                   << " in block " << block->GetBlockId());
  return fixup;
}

}  // namespace art
