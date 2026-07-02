// Copyright 2025 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "merkle_tree.h"

#include <cassert>
#include <cstdint>
#include <limits>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

BSSL_NAMESPACE_BEGIN

namespace {

// Returns a subtree consistency proof for `subtree` in the first `n` elements
// of `tree`. This is currently implemented recursively, matching the
// specification. If we ever need to expose it, we can implement it more
// efficiently.
std::vector<uint8_t> SubtreeConsistencyProof(const MerkleTree &mt,
                                             Subtree subtree,
                                             const Subtree &tree,
                                             bool known_hash = true) {
  BSSL_CHECK(subtree.IsValid());
  BSSL_CHECK(tree.IsValid());
  BSSL_CHECK(tree.Contains(subtree));

  if (subtree == tree) {
    if (known_hash) {
      return {};
    }
    TreeHash h = mt.SubtreeHash(subtree);
    return std::vector(h.begin(), h.end());
  }

  uint64_t k = tree.Split();
  Subtree subproof_tree, mth_tree;
  if (subtree.end <= k) {
    subproof_tree = tree.Left();
    mth_tree = tree.Right();
  } else if (subtree.start >= k) {
    mth_tree = tree.Left();
    subproof_tree = tree.Right();
  } else {
    subtree.start = k;
    mth_tree = tree.Left();
    subproof_tree = tree.Right();
    known_hash = false;
  }
  std::vector<uint8_t> subproof =
      SubtreeConsistencyProof(mt, subtree, subproof_tree, known_hash);
  TreeHash mth = mt.SubtreeHash(mth_tree);
  subproof.insert(subproof.end(), mth.begin(), mth.end());
  return subproof;
}

std::vector<std::vector<uint8_t>> MakeTestEntries(std::string_view label,
                                                  size_t n) {
  std::vector<std::vector<uint8_t>> entries;
  entries.reserve(n);
  for (size_t i = 0; i < n; i++) {
    std::vector<uint8_t> entry(label.begin(), label.end());
    for (size_t j = 0; j < 8; j++) {
      entry.push_back(static_cast<uint8_t>(i >> (j * 8)));
    }
    entries.push_back(std::move(entry));
  }
  return entries;
}

std::vector<uint8_t> ConcatProof(const std::vector<TreeHash> &proof) {
  std::vector<uint8_t> out;
  for (const auto &p : proof) {
    out.insert(out.end(), p.begin(), p.end());
  }
  return out;
}

TEST(MerkleTreeTest, SubtreeIsValid) {
  // An empty subtree is invalid.
  EXPECT_FALSE((Subtree{0, 0}.IsValid()));
  // But if the end is before start, it's invalid.
  EXPECT_FALSE((Subtree{1, 0}.IsValid()));
  // A subtree of the maximum expressible size is valid.
  EXPECT_TRUE((Subtree{0, std::numeric_limits<uint64_t>::max()}.IsValid()));

  // Subtrees don't have to start at 0.
  EXPECT_TRUE((Subtree{4, 8}.IsValid()));
  // But if they don't start at 0, there's a limit to how big they can be.
  EXPECT_FALSE((Subtree{4, 9}.IsValid()));
  // Subtrees can have a ragged right edge.
  EXPECT_TRUE((Subtree{4, 6}.IsValid()));
  EXPECT_TRUE((Subtree{0, 6}.IsValid()));
}

TEST(MerkleTreeTest, SubtreeSplit) {
  // Empty subtree.
  EXPECT_EQ((Subtree{24601, 24601}).Split(), 24601ul);
  // Single-item subtree.
  EXPECT_EQ((Subtree{1336, 1337}).Split(), 1337ul);
  // Two items in subtree.
  EXPECT_EQ((Subtree{42, 44}).Split(), 43ul);
  // Subtree size is 1 less than a power of 2.
  EXPECT_EQ((Subtree{0, 31}).Split(), 16ul);
  // Subtree size is a power of 2.
  EXPECT_EQ((Subtree{64, 128}).Split(), 96ul);
  /// Subtree size is 1 more than a power of 2.
  EXPECT_EQ((Subtree{0, 257}).Split(), 256ul);

  static const uint64_t u64_max = std::numeric_limits<uint64_t>::max();
  // Maximum size tree.
  EXPECT_EQ((Subtree{0, u64_max}).Split(), 1ull << 63);
  // Small tree, with end at maximum value.
  EXPECT_EQ((Subtree{u64_max - 3, u64_max}).Split(), u64_max - 1);
}

TEST(MerkleTreeTest, VerifySubtreeInclusionProof) {
  auto entries = MakeTestEntries("label", 847);
  MerkleTreeInMemory tree(entries);

  uint64_t index = 0;
  auto node_hash = tree.SubtreeHash({index, index + 1});
  Subtree subtree{0, 16};
  auto proof = tree.SubtreeInclusionProof(index, subtree);
  auto root_hash = EvaluateMerkleSubtreeConsistencyProof(
      subtree.end, {index, index + 1}, proof, node_hash);
  ASSERT_TRUE(root_hash.has_value());
  EXPECT_EQ(root_hash, tree.SubtreeHash(subtree));
  // Check again with EvaluateMerkleSubtreeInclusionProof
  root_hash =
      EvaluateMerkleSubtreeInclusionProof(proof, index, node_hash, subtree);
  ASSERT_TRUE(root_hash.has_value());
  EXPECT_EQ(root_hash, tree.SubtreeHash(subtree));

  // Build and verify a proof from a subtree with start != 0
  index = 845;
  node_hash = tree.SubtreeHash({index, index + 1});
  subtree = {840, 847};
  proof = tree.SubtreeInclusionProof(index, subtree);
  root_hash = EvaluateMerkleSubtreeConsistencyProof(
      subtree.Size(), {index - subtree.start, index - subtree.start + 1}, proof,
      node_hash);
  ASSERT_TRUE(root_hash.has_value());
  EXPECT_EQ(root_hash, tree.SubtreeHash(subtree));
  // Check again with EvaluateMerkleSubtreeInclusionProof
  root_hash =
      EvaluateMerkleSubtreeInclusionProof(proof, index, node_hash, subtree);
  ASSERT_TRUE(root_hash.has_value());
  EXPECT_EQ(root_hash, tree.SubtreeHash(subtree));
}

TEST(MerkleTreeTest, SubtreeInclusionProofInvalidArgs) {
  auto entries = MakeTestEntries("label", 847);
  MerkleTreeInMemory tree(entries);

  uint64_t index = 845;
  auto node_hash = tree.SubtreeHash({index, index + 1});
  Subtree subtree = {840, 847};
  auto proof = tree.SubtreeInclusionProof(index, subtree);

  // If the wrong node hash is passed in, the function will still compute a
  // root hash, but it won't match the expected value.
  auto wrong_node_hash = tree.SubtreeHash({index + 1, index + 2});
  auto root_hash = EvaluateMerkleSubtreeInclusionProof(
      proof, index, wrong_node_hash, subtree);
  ASSERT_TRUE(root_hash.has_value());
  EXPECT_NE(root_hash, tree.SubtreeHash(subtree));

  // If the subtree isn't valid, the function will fail.
  ASSERT_FALSE(
      EvaluateMerkleSubtreeInclusionProof(proof, index, node_hash, {840, 849}));

  // If the index isn't contained within the subtree, the function will fail.
  ASSERT_FALSE(
      EvaluateMerkleSubtreeInclusionProof(proof, 848, node_hash, {840, 847}));
}

// Test that the computed consistency proofs match the examples given in RFC
// 9162 section 2.1.5.
TEST(MerkleTreeTest, SubtreeConsistencyProofRFC9162) {
  auto entries = MakeTestEntries("label", 7);
  MerkleTreeInMemory tree(entries);

  // The example from section 2.1.5 has a final tree with 7 leaves.
  Subtree final_tree{0, 7};

  // The examples refer to letters representing the hashes of various subtrees
  // within that tree. a and e aren't used in any of the examples.
  auto b = tree.SubtreeHash({1, 2});
  auto c = tree.SubtreeHash({2, 3});
  auto d = tree.SubtreeHash({3, 4});
  auto f = tree.SubtreeHash({5, 6});
  auto g = tree.SubtreeHash({0, 2});
  auto h = tree.SubtreeHash({2, 4});
  auto i = tree.SubtreeHash({4, 6});
  auto j = tree.SubtreeHash({6, 7});
  auto k = tree.SubtreeHash({0, 4});
  auto l = tree.SubtreeHash({4, 7});

  // Inclusion proofs:

  // Section 2.1.5: "The inclusion proof for `d0` is `[b, h, l]`."
  auto d0_proof = tree.SubtreeInclusionProof(0, final_tree);
  EXPECT_EQ(d0_proof, ConcatProof({b, h, l}));

  // Section 2.1.5: "The inclusion proof for `d3` is `[c, g, l]`."
  auto d3_proof = tree.SubtreeInclusionProof(3, final_tree);
  EXPECT_EQ(d3_proof, ConcatProof({c, g, l}));

  // Section 2.1.5: "The inclusion proof for `d4` is `[f, j, k]`."
  auto d4_proof = tree.SubtreeInclusionProof(4, final_tree);
  EXPECT_EQ(d4_proof, ConcatProof({f, j, k}));

  // Section 2.1.5: "The inclusion proof for `d6` is `[i, k]`."
  auto d6_proof = tree.SubtreeInclusionProof(6, final_tree);
  EXPECT_EQ(d6_proof, ConcatProof({i, k}));

  // Consistency proofs:

  // The consistency proofs refer to the lettered hashes above, as well as some
  // hashes of the tree as it was incrementally built.
  Subtree hash0_subtree = {0, 3};
  Subtree hash1_subtree = {0, 4};
  auto hash1 = tree.SubtreeHash(hash1_subtree);
  ASSERT_EQ(hash1, k);
  Subtree hash2_subtree = {0, 6};

  // "The consistency proof between hash0 and hash is [c, d, g, l]."
  auto hash0_proof = SubtreeConsistencyProof(tree, hash0_subtree, final_tree);
  EXPECT_EQ(hash0_proof, ConcatProof({c, d, g, l}));

  // "The consistency proof between hash1 and hash is [l]."
  auto hash1_proof = SubtreeConsistencyProof(tree, hash1_subtree, final_tree);
  EXPECT_EQ(hash1_proof, ConcatProof({l}));

  // "The consistency proof between hash2 and hash is [i, j, k]."
  auto hash2_proof = SubtreeConsistencyProof(tree, hash2_subtree, final_tree);
  EXPECT_EQ(hash2_proof, ConcatProof({i, j, k}));
}

TEST(MerkleTreeTest, ValidProofsTest) {
  uint64_t n = 4, start = 0, end = 3;
  Subtree full_tree{0, n};
  auto entries = MakeTestEntries("label", n);
  MerkleTreeInMemory tree(entries);

  auto tree_hash = tree.SubtreeHash(full_tree);
  Subtree subtree{start, end};
  ASSERT_TRUE(subtree.IsValid());
  auto subtree_hash = tree.SubtreeHash(subtree);

  auto proof = SubtreeConsistencyProof(tree, subtree, full_tree);
  auto computed_hash =
      EvaluateMerkleSubtreeConsistencyProof(n, subtree, proof, subtree_hash);
  EXPECT_EQ(computed_hash, tree_hash);
}

TEST(MerkleTreeTest, ValidProofs) {
  // As of the time of writing this test, a run was performed with limit=257 and
  // the test passed. This value is set to 129 to balance how much of the space
  // to explore with test execution time. In particular, in an unoptimized
  // build, limit=257 takes about 4 seconds.
  for (bool incremental : {false, true}) {
    SCOPED_TRACE(incremental);

    uint64_t limit = 129;
    auto entries = MakeTestEntries("label", limit);
    MerkleTreeInMemory tree;
    if (incremental) {
      for (const auto &entry : entries) {
        tree.Append(entry);
      }
    } else {
      tree = MerkleTreeInMemory(entries);
    }

    // Exhaustively test subtree consistency proofs.
    for (uint64_t n = 1; n < limit; n++) {
      Subtree full_tree{0, n};
      auto tree_hash = tree.SubtreeHash(full_tree);
      for (uint64_t end = 0; end <= n; end++) {
        for (uint64_t start = 0; start < end; start++) {
          Subtree subtree{start, end};
          if (!subtree.IsValid()) {
            continue;
          }
          SCOPED_TRACE(testing::Message() << "Tree n=" << n << ", start: "
                                          << start << ", end: " << end);
          auto subtree_hash = tree.SubtreeHash(subtree);
          auto proof = SubtreeConsistencyProof(tree, subtree, full_tree);
          auto computed_hash = EvaluateMerkleSubtreeConsistencyProof(
              n, subtree, proof, subtree_hash);
          EXPECT_EQ(computed_hash, tree_hash);
        }
      }
    }

    // Exhaustively test subtree inclusion proofs.
    for (uint64_t end = 0; end <= limit; end++) {
      for (uint64_t start = 0; start < end; start++) {
        Subtree subtree{start, end};
        if (!subtree.IsValid()) {
          continue;
        }
        for (uint64_t index = start; index < end; index++) {
          SCOPED_TRACE(testing::Message() << "index: " << index << ", start: "
                                          << start << ", end: " << end);
          auto subtree_hash = tree.SubtreeHash(subtree);
          auto entry_hash = tree.SubtreeHash({index, index + 1});
          auto proof = tree.SubtreeInclusionProof(index, subtree);
          auto computed_hash = EvaluateMerkleSubtreeInclusionProof(
              proof, index, entry_hash, subtree);
          EXPECT_EQ(computed_hash, subtree_hash);

          // A subtree inclusion proof is a special case of a consistency proof.
          auto proof2 =
              SubtreeConsistencyProof(tree, {index, index + 1}, subtree);
          EXPECT_EQ(proof, proof2);
        }
      }
    }
  }
}

class MerkleTreeLarge : public MerkleTree {
 public:
  // Constructs a Merkle Tree over 2^64 - 1 copies of `entry`.
  explicit MerkleTreeLarge(Span<const uint8_t> entry) {
    HashLeaf(entry, levels_[0]);
    for (size_t i = 1; i < levels_.size(); i++) {
      HashNode(levels_[i - 1], levels_[i - 1], levels_[i]);
    }
  }

  uint64_t Size() const override {
    return std::numeric_limits<uint64_t>::max();
  }

  TreeHash GetNode(size_t level, uint64_t index) const override {
    BSSL_CHECK(level < 64);
    return levels_[level];
  }

 private:
  std::array<TreeHash, 64> levels_;
};

TEST(MerkleTreeTest, VeryLargeProofs) {
  MerkleTreeLarge tree(StringAsBytes("entry"));

  Subtree fullest_tree = {0, std::numeric_limits<uint64_t>::max()};
  auto root_hash = tree.SubtreeHash(fullest_tree);

  Subtree test_subtrees[] = {
      fullest_tree,
      {0, 1},
      {0, 1ull << 63},
      {1ull << 63, std::numeric_limits<uint64_t>::max()},
      {std::numeric_limits<uint64_t>::max() - 1,
       std::numeric_limits<uint64_t>::max()},
  };
  for (auto subtree : test_subtrees) {
    SCOPED_TRACE(testing::Message() << "Subtree start: " << subtree.start
                                    << ", end: " << subtree.end);

    auto proof = SubtreeConsistencyProof(tree, subtree, fullest_tree);
    auto computed_root_hash = EvaluateMerkleSubtreeConsistencyProof(
        fullest_tree.end, subtree, proof, tree.SubtreeHash(subtree));
    EXPECT_EQ(computed_root_hash, root_hash);
  }
}

}  // namespace

BSSL_NAMESPACE_END
