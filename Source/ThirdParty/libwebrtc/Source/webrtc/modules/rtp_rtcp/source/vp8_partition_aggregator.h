/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_VP8_PARTITION_AGGREGATOR_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_VP8_PARTITION_AGGREGATOR_H_

#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Class used to solve the VP8 aggregation problem.
class PartitionTreeNode {
 public:
  // Create a tree node.
  PartitionTreeNode(PartitionTreeNode* parent,
                    const size_t* size_vector,
                    size_t num_partitions,
                    size_t this_size);

  // Create a root node.
  static PartitionTreeNode* CreateRootNode(const size_t* size_vector,
                                           size_t num_partitions);

  ~PartitionTreeNode();

  // Calculate the cost for the node. If the node is a solution node, the cost
  // will be the actual cost associated with that solution. If not, the cost
  // will be the cost accumulated so far along the current branch (which is a
  // lower bound for any solution along the branch).
  int Cost(size_t penalty);

  // Create the two children for this node.
  bool CreateChildren(size_t max_size);

  // Get the number of packets for the configuration that this node represents.
  size_t NumPackets();

  // Find the optimal solution given a maximum packet size and a per-packet
  // penalty. The method will be recursively called while the solver is
  // probing down the tree of nodes.
  PartitionTreeNode* GetOptimalNode(size_t max_size, size_t penalty);

  // Setters and getters.
  void set_max_parent_size(int size) { max_parent_size_ = size; }
  void set_min_parent_size(int size) { min_parent_size_ = size; }
  PartitionTreeNode* parent() const { return parent_; }
  PartitionTreeNode* left_child() const { return children_[kLeftChild]; }
  PartitionTreeNode* right_child() const { return children_[kRightChild]; }
  size_t this_size() const { return this_size_; }
  bool packet_start() const { return packet_start_; }

 private:
  enum Children {
    kLeftChild = 0,
    kRightChild = 1
  };

  int this_size_int() const { return static_cast<int>(this_size_); }
  void set_packet_start(bool value) { packet_start_ = value; }

  PartitionTreeNode* parent_;
  PartitionTreeNode* children_[2];
  size_t this_size_;
  const size_t* size_vector_;
  size_t num_partitions_;
  int max_parent_size_;
  int min_parent_size_;
  bool packet_start_;

  RTC_DISALLOW_COPY_AND_ASSIGN(PartitionTreeNode);
};

// Class that calculates the optimal aggregation of VP8 partitions smaller than
// the maximum packet size.
class Vp8PartitionAggregator {
 public:
  typedef std::vector<size_t> ConfigVec;

  // Constructor. All partitions in the fragmentation header from index
  // first_partition_idx to last_partition_idx must be smaller than
  // maximum packet size to be used in FindOptimalConfiguration.
  Vp8PartitionAggregator(const RTPFragmentationHeader& fragmentation,
                         size_t first_partition_idx,
                         size_t last_partition_idx);

  ~Vp8PartitionAggregator();

  // Set the smallest and largest payload sizes produces so far.
  void SetPriorMinMax(int min_size, int max_size);

  // Find the aggregation of VP8 partitions that produces the smallest cost.
  // The result is given as a vector of the same length as the number of
  // partitions given to the constructor (i.e., last_partition_idx -
  // first_partition_idx + 1), where each element indicates the packet index
  // for that partition. Thus, the output vector starts at 0 and is increasing
  // up to the number of packets - 1.
  ConfigVec FindOptimalConfiguration(size_t max_size, size_t penalty);

  // Calculate minimum and maximum packet sizes for a given aggregation config.
  // The extreme packet sizes of the given aggregation are compared with the
  // values given in min_size and max_size, and if either of these are exceeded,
  // the new extreme value will be written to the corresponding variable.
  void CalcMinMax(const ConfigVec& config, int* min_size, int* max_size) const;

  // Calculate the number of fragments to divide a large partition into.
  // The large partition is of size large_partition_size. The payload must not
  // be larger than max_payload_size. Each fragment comes at an overhead cost
  // of penalty bytes. If the size of the fragments fall outside the range
  // [min_size, max_size], an extra cost is inflicted.
  static size_t CalcNumberOfFragments(size_t large_partition_size,
                                      size_t max_payload_size,
                                      size_t penalty,
                                      int min_size,
                                      int max_size);

 private:
  PartitionTreeNode* root_;
  size_t num_partitions_;
  size_t* size_vector_;
  size_t largest_partition_size_;

  RTC_DISALLOW_COPY_AND_ASSIGN(Vp8PartitionAggregator);
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_VP8_PARTITION_AGGREGATOR_H_
