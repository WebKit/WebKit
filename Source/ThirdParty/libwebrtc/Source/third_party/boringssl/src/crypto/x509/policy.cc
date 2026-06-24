// Copyright 2022 The BoringSSL Authors
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

#include <openssl/base.h>
#include <openssl/x509.h>

#include <assert.h>

#include <algorithm>
#include <optional>
#include <utility>

#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/span.h>
#include <openssl/stack.h>

#include "../internal.h"
#include "../mem_internal.h"
#include "internal.h"


BSSL_NAMESPACE_BEGIN
namespace {

// This file computes the X.509 policy graph, as described in RFC 9618.
// Implementation notes:
//
//  (1) It does not track "qualifier_set". This is not needed as it is not
//      output by this implementation.
//
//  (2) "expected_policy_set" is not tracked explicitly and built temporarily
//      as part of building the graph.
//
//  (3) anyPolicy nodes are not tracked explicitly.
//
//  (4) Some pruning steps are deferred to when policies are evaluated, as a
//      reachability pass.

bool is_any_policy(const ASN1_OBJECT *obj) {
  return OBJ_obj2nid(obj) == NID_any_policy;
}

// An X509PolicyNode is a node in the policy graph. It corresponds to a node
// from RFC 5280, section 6.1.2, step (a), but we store some fields differently.
struct X509PolicyNode {
  static std::optional<X509PolicyNode> Create(const ASN1_OBJECT *policy) {
    assert(!is_any_policy(policy));
    X509PolicyNode node;
    node.policy.reset(OBJ_dup(policy));
    if (node.policy == nullptr) {
      return std::nullopt;
    }
    return node;
  }

  bool operator<(const X509PolicyNode &other) const {
    return OBJ_cmp(policy.get(), other.policy.get()) < 0;
  }

  bool parent_is_any_policy() const { return parent_policies.empty(); }

  // policy is the "valid_policy" field from RFC 5280.
  UniquePtr<ASN1_OBJECT> policy;

  // parent_policies, if non-empty, is the list of "valid_policy" values for all
  // nodes which are a parent of this node. In this case, no entry in this list
  // will be anyPolicy. This list is in no particular order and may contain
  // duplicates if the corresponding certificate had duplicate mappings.
  //
  // If empty, this node has a single parent, anyPolicy. The node is then a root
  // policy, and is in authorities-constrained-policy-set if it has a path to a
  // leaf node.
  //
  // Note it is not possible for a policy to have both anyPolicy and a
  // concrete policy as a parent. Section 6.1.3, step (d.1.ii) only runs if
  // there was no match in step (d.1.i). We do not need to represent a parent
  // list of, say, {anyPolicy, OID1, OID2}.
  Vector<UniquePtr<ASN1_OBJECT>> parent_policies;

  // mapped is whether this node matches a policy mapping in the certificate.
  bool mapped = false;

  // reachable is whether this node is reachable from some valid policy in the
  // end-entity certificate. It is computed during |has_explicit_policy|.
  bool reachable = false;
};

// An X509PolicyLevel is the collection of nodes at the same depth in the
// policy graph. This structure can also be used to represent a level's
// "expected_policy_set" values. See |process_policy_mappings|.
class X509PolicyLevel {
 public:
  bool has_any_policy() const { return has_any_policy_; }
  bool is_empty() const { return !has_any_policy_ && nodes_.empty(); }
  Span<const X509PolicyNode> nodes() const { return nodes_; }

  void set_has_any_policy(bool v) { has_any_policy_ = v; }

  // Although this is mutable, callers may not modify the node's policy.
  Span<X509PolicyNode> nodes() { return Span(nodes_); }

  void Clear() {
    has_any_policy_ = false;
    nodes_.clear();
  }

  // Find returns the node corresponding to |policy|, or nullptr if none exists.
  X509PolicyNode *Find(const ASN1_OBJECT *policy) {
    // The list is sorted, so we can binary search.
    auto it = std::lower_bound(
        nodes_.begin(), nodes_.end(), policy,
        [](const X509PolicyNode &node, const ASN1_OBJECT *obj) {
          return OBJ_cmp(node.policy.get(), obj) < 0;
        });
    if (it == nodes_.end() || OBJ_cmp(it->policy.get(), policy) != 0) {
      return nullptr;
    }
    return &*it;
  }

  // AddNodes adds the nodes in |nodes|. It returns true on success and false on
  // error. No policy in |nodes| may already be present. This method leaves
  // the objects in |nodes| in a moved-from state.
  //
  // This method re-sorts the nodes, so it runs in time proportional to the
  // total size of the level. However, each level is only added to three times
  // in the course of policy validation.
  bool AddNodes(Span<X509PolicyNode> nodes) {
    if (!nodes_.AppendMove(nodes)) {
      return false;
    }
    std::sort(nodes_.begin(), nodes_.end());
#if !defined(NDEBUG)
    // There should be no duplicate nodes.
    for (size_t i = 1; i < nodes_.size(); i++) {
      assert(OBJ_cmp(nodes_[i - 1].policy.get(), nodes_[i].policy.get()) != 0);
    }
#endif
    return true;
  }

  // EraseNodesIf removes all nodes that satisfy the predicate |pred|.
  template <typename Pred>
  void EraseNodesIf(Pred pred) {
    nodes_.EraseIf(pred);
  }

 private:
  // nodes is the list of nodes at this depth, except for the anyPolicy node, if
  // any. This list is sorted by policy OID for efficient lookup.
  Vector<X509PolicyNode> nodes_;

  // has_any_policy is whether there is an anyPolicy node at this depth.
  bool has_any_policy_ = false;
};

int policyinfo_cmp(const POLICYINFO *const *a, const POLICYINFO *const *b) {
  return OBJ_cmp((*a)->policyid, (*b)->policyid);
}

// process_certificate_policies updates |level| to incorporate |x509|'s
// certificate policies extension. This implements steps (d) and (e) of RFC
// 5280, section 6.1.3. |level| must contain the previous level's
// "expected_policy_set" information. For all but the top-most level, this is
// the output of |process_policy_mappings|. |any_policy_allowed| specifies
// whether anyPolicy is allowed or inhibited, taking into account the exception
// for self-issued certificates.
bool process_certificate_policies(const X509 *x509, X509PolicyLevel *level,
                                  int any_policy_allowed) {
  int critical;
  UniquePtr<CERTIFICATEPOLICIES> policies(
      reinterpret_cast<CERTIFICATEPOLICIES *>(X509_get_ext_d2i(
          x509, NID_certificate_policies, &critical, nullptr)));
  if (policies == nullptr) {
    if (critical != -1) {
      return false;  // Syntax error in the extension.
    }

    // RFC 5280, section 6.1.3, step (e).
    level->Clear();
    return true;
  }

  // certificatePolicies may not be empty. See RFC 5280, section 4.2.1.4.
  // TODO(https://crbug.com/boringssl/443): Move this check into the parser.
  if (sk_POLICYINFO_num(policies.get()) == 0) {
    OPENSSL_PUT_ERROR(X509, X509_R_INVALID_POLICY_EXTENSION);
    return false;
  }

  sk_POLICYINFO_set_cmp_func(policies.get(), policyinfo_cmp);
  sk_POLICYINFO_sort(policies.get());
  bool cert_has_any_policy = false;
  for (size_t i = 0; i < sk_POLICYINFO_num(policies.get()); i++) {
    const POLICYINFO *policy = sk_POLICYINFO_value(policies.get(), i);
    if (is_any_policy(policy->policyid)) {
      cert_has_any_policy = true;
    }
    if (i > 0 && OBJ_cmp(sk_POLICYINFO_value(policies.get(), i - 1)->policyid,
                         policy->policyid) == 0) {
      // Per RFC 5280, section 4.2.1.4, |policies| may not have duplicates.
      OPENSSL_PUT_ERROR(X509, X509_R_INVALID_POLICY_EXTENSION);
      return false;
    }
  }

  // This does the same thing as RFC 5280, section 6.1.3, step (d), though in
  // a slightly different order. |level| currently contains
  // "expected_policy_set" values of the previous level. See
  // |process_policy_mappings| for details.
  const bool previous_level_has_any_policy = level->has_any_policy();

  // First, we handle steps (d.1.i) and (d.2). The net effect of these two
  // steps is to intersect |level| with |policies|, ignoring anyPolicy if it
  // is inhibited.
  if (!cert_has_any_policy || !any_policy_allowed) {
    level->EraseNodesIf([&](const X509PolicyNode &node) {
      assert(sk_POLICYINFO_is_sorted(policies.get()));
      // Erase the node if it not present in the current certificate.
      POLICYINFO info;
      info.policyid = node.policy.get();
      return !sk_POLICYINFO_find(policies.get(), nullptr, &info);
    });
    level->set_has_any_policy(false);
  }

  // Step (d.1.ii) may attach new nodes to the previous level's anyPolicy
  // node.
  if (previous_level_has_any_policy) {
    Vector<X509PolicyNode> new_nodes;
    for (const POLICYINFO *policy : policies.get()) {
      // Though we've reordered the steps slightly, |policy| is in |level| if
      // and only if it would have been a match in step (d.1.ii).
      if (!is_any_policy(policy->policyid) &&
          level->Find(policy->policyid) == nullptr) {
        auto node = X509PolicyNode::Create(policy->policyid);
        if (!node.has_value() || !new_nodes.Push(*std::move(node))) {
          return false;
        }
      }
    }
    if (!level->AddNodes(Span(new_nodes))) {
      return false;
    }
  }

  return true;
}

int compare_issuer_policy(const POLICY_MAPPING *const *a,
                          const POLICY_MAPPING *const *b) {
  return OBJ_cmp((*a)->issuerDomainPolicy, (*b)->issuerDomainPolicy);
}

int compare_subject_policy(const POLICY_MAPPING *const *a,
                           const POLICY_MAPPING *const *b) {
  return OBJ_cmp((*a)->subjectDomainPolicy, (*b)->subjectDomainPolicy);
}

// process_policy_mappings processes the policy mappings extension of |cert|,
// whose corresponding graph level is |level|. |mapping_allowed| specifies
// whether policy mapping is inhibited at this point. On success, it returns an
// |X509PolicyLevel| containing the "expected_policy_set" for |level|. On error,
// it returns std::nullopt. This implements steps (a) and (b) of RFC 5280,
// section 6.1.4.
//
// We represent the "expected_policy_set" as an |X509PolicyLevel|.
// |has_any_policy| indicates whether there is an anyPolicy node with
// "expected_policy_set" of {anyPolicy}. If a node with policy oid P1 contains
// P2 in its "expected_policy_set", the level will contain a node of policy P2
// with P1 in |parent_policies|.
//
// This is equivalent to the |X509PolicyLevel| that would result if the next
// certificates contained anyPolicy. |process_certificate_policies| will filter
// this result down to compute the actual level.
std::optional<X509PolicyLevel> process_policy_mappings(const X509 *cert,
                                                       X509PolicyLevel *level,
                                                       bool mapping_allowed) {
  int critical;
  UniquePtr<POLICY_MAPPINGS> mappings(reinterpret_cast<POLICY_MAPPINGS *>(
      X509_get_ext_d2i(cert, NID_policy_mappings, &critical, nullptr)));
  if (mappings == nullptr && critical != -1) {
    // Syntax error in the policy mappings extension.
    return std::nullopt;
  }

  if (mappings != nullptr) {
    // PolicyMappings may not be empty. See RFC 5280, section 4.2.1.5.
    // TODO(https://crbug.com/boringssl/443): Move this check into the parser.
    if (sk_POLICY_MAPPING_num(mappings.get()) == 0) {
      OPENSSL_PUT_ERROR(X509, X509_R_INVALID_POLICY_EXTENSION);
      return std::nullopt;
    }

    // RFC 5280, section 6.1.4, step (a).
    for (const POLICY_MAPPING *mapping : mappings.get()) {
      if (is_any_policy(mapping->issuerDomainPolicy) ||
          is_any_policy(mapping->subjectDomainPolicy)) {
        return std::nullopt;
      }
    }

    // Sort to group by issuerDomainPolicy.
    sk_POLICY_MAPPING_set_cmp_func(mappings.get(), compare_issuer_policy);
    sk_POLICY_MAPPING_sort(mappings.get());

    if (mapping_allowed) {
      // Mark nodes as mapped, and add any nodes to |level| which may be
      // needed as part of RFC 5280, section 6.1.4, step (b.1).
      Vector<X509PolicyNode> new_nodes;
      const ASN1_OBJECT *last_policy = nullptr;
      for (const POLICY_MAPPING *mapping : mappings.get()) {
        // There may be multiple mappings with the same |issuerDomainPolicy|.
        if (last_policy != nullptr &&
            OBJ_cmp(mapping->issuerDomainPolicy, last_policy) == 0) {
          continue;
        }
        last_policy = mapping->issuerDomainPolicy;

        X509PolicyNode *node = level->Find(mapping->issuerDomainPolicy);
        if (node != nullptr) {
          node->mapped = true;
        } else {
          if (!level->has_any_policy()) {
            continue;
          }
          auto new_node = X509PolicyNode::Create(mapping->issuerDomainPolicy);
          if (!new_node.has_value()) {
            return std::nullopt;
          }
          new_node->mapped = true;
          if (!new_nodes.Push(*std::move(new_node))) {
            return std::nullopt;
          }
        }
      }
      if (!level->AddNodes(Span(new_nodes))) {
        return std::nullopt;
      }
    } else {
      // RFC 5280, section 6.1.4, step (b.2). If mapping is inhibited, delete
      // all mapped nodes.
      level->EraseNodesIf([&](const X509PolicyNode &node) {
        // |mappings| must have been sorted by |compare_issuer_policy|.
        assert(sk_POLICY_MAPPING_is_sorted(mappings.get()));
        // Check if the node was mapped.
        POLICY_MAPPING mapping;
        mapping.issuerDomainPolicy = node.policy.get();
        return sk_POLICY_MAPPING_find(mappings.get(), /*out_index=*/nullptr,
                                      &mapping);
      });
      // Dropping the mappings.
      mappings = nullptr;
    }
  }

  // If a node was not mapped, it retains the original "explicit_policy_set"
  // value, itself. Add those to |mappings|.
  if (mappings == nullptr) {
    mappings.reset(sk_POLICY_MAPPING_new_null());
    if (mappings == nullptr) {
      return std::nullopt;
    }
  }
  for (const X509PolicyNode &node : level->nodes()) {
    if (!node.mapped) {
      UniquePtr<POLICY_MAPPING> mapping(POLICY_MAPPING_new());
      if (mapping == nullptr) {
        return std::nullopt;
      }
      mapping->issuerDomainPolicy = OBJ_dup(node.policy.get());
      mapping->subjectDomainPolicy = OBJ_dup(node.policy.get());
      if (mapping->issuerDomainPolicy == nullptr ||
          mapping->subjectDomainPolicy == nullptr ||
          !PushToStack(mappings.get(), std::move(mapping))) {
        return std::nullopt;
      }
    }
  }

  // Sort to group by subjectDomainPolicy.
  sk_POLICY_MAPPING_set_cmp_func(mappings.get(), compare_subject_policy);
  sk_POLICY_MAPPING_sort(mappings.get());

  // Convert |mappings| to our "expected_policy_set" representation.
  Vector<X509PolicyNode> next_nodes;
  for (POLICY_MAPPING *mapping : mappings.get()) {
    // Skip mappings where |issuerDomainPolicy| does not appear in the graph.
    if (!level->has_any_policy() &&
        level->Find(mapping->issuerDomainPolicy) == nullptr) {
      continue;
    }

    if (next_nodes.empty() || OBJ_cmp(next_nodes.back().policy.get(),
                                      mapping->subjectDomainPolicy) != 0) {
      auto new_node = X509PolicyNode::Create(mapping->subjectDomainPolicy);
      if (!new_node.has_value() || !next_nodes.Push(*std::move(new_node))) {
        return std::nullopt;
      }
    }

    // |mapping| is going to be destroyed, so steal its policy object.
    UniquePtr<ASN1_OBJECT> policy(
        std::exchange(mapping->issuerDomainPolicy, nullptr));
    if (!next_nodes.back().parent_policies.Push(std::move(policy))) {
      return std::nullopt;
    }
  }

  X509PolicyLevel next;
  next.set_has_any_policy(level->has_any_policy());
  if (!next.AddNodes(Span(next_nodes))) {
    return std::nullopt;
  }
  return next;
}

// apply_skip_certs, if |skip_certs| is non-NULL, sets |*value| to the minimum
// of its current value and |skip_certs|. It returns true on success and false
// if |skip_certs| is negative.
bool apply_skip_certs(const ASN1_INTEGER *skip_certs, size_t *value) {
  if (skip_certs == nullptr) {
    return true;
  }

  // TODO(https://crbug.com/boringssl/443): Move this check into the parser.
  if (skip_certs->type & V_ASN1_NEG) {
    OPENSSL_PUT_ERROR(X509, X509_R_INVALID_POLICY_EXTENSION);
    return false;
  }

  // If |skip_certs| does not fit in |uint64_t|, it must exceed |*value|.
  uint64_t u64;
  if (ASN1_INTEGER_get_uint64(&u64, skip_certs) && u64 < *value) {
    *value = (size_t)u64;
  }
  ERR_clear_error();
  return true;
}

// process_policy_constraints updates |*explicit_policy|, |*policy_mapping|, and
// |*inhibit_any_policy| according to |x509|'s policy constraints and inhibit
// anyPolicy extensions. It returns one on success and zero on error. This
// implements steps (i) and (j) of RFC 5280, section 6.1.4.
bool process_policy_constraints(const X509 *x509, size_t *explicit_policy,
                                size_t *policy_mapping,
                                size_t *inhibit_any_policy) {
  int critical;
  UniquePtr<POLICY_CONSTRAINTS> constraints(
      reinterpret_cast<POLICY_CONSTRAINTS *>(
          X509_get_ext_d2i(x509, NID_policy_constraints, &critical, nullptr)));
  if (constraints == nullptr && critical != -1) {
    return false;
  }
  if (constraints != nullptr) {
    if (constraints->requireExplicitPolicy == nullptr &&
        constraints->inhibitPolicyMapping == nullptr) {
      // Per RFC 5280, section 4.2.1.11, at least one of the fields must be
      // present.
      OPENSSL_PUT_ERROR(X509, X509_R_INVALID_POLICY_EXTENSION);
      return false;
    }
    if (!apply_skip_certs(constraints->requireExplicitPolicy,
                          explicit_policy) ||
        !apply_skip_certs(constraints->inhibitPolicyMapping, policy_mapping)) {
      return false;
    }
  }

  UniquePtr<ASN1_INTEGER> inhibit_any_policy_ext(
      reinterpret_cast<ASN1_INTEGER *>(
          X509_get_ext_d2i(x509, NID_inhibit_any_policy, &critical, nullptr)));
  if (inhibit_any_policy_ext == nullptr && critical != -1) {
    return false;
  }
  return apply_skip_certs(inhibit_any_policy_ext.get(), inhibit_any_policy);
}

// has_explicit_policy returns true if the set of authority-space policy OIDs
// |levels| has some non-empty intersection with |user_policies|, and false
// otherwise. This mirrors the logic in RFC 5280, section 6.1.5, step (g). This
// function modifies |levels| and should only be called at the end of policy
// evaluation.
bool has_explicit_policy(Span<X509PolicyLevel> levels,
                         const STACK_OF(ASN1_OBJECT) *user_policies) {
  assert(user_policies == nullptr || sk_ASN1_OBJECT_is_sorted(user_policies));

  // Step (g.i). If the policy graph is empty, the intersection is empty.
  if (levels.empty() || levels.back().is_empty()) {
    return false;
  }

  // Step (g.ii). If the policy graph is not empty and the user set contains
  // anyPolicy, the intersection is the entire (non-empty) graph.
  //
  // If |user_policies| is empty, we interpret it as having a single anyPolicy
  // value. The caller may also have supplied anyPolicy explicitly.
  if (sk_ASN1_OBJECT_num(user_policies) == 0) {
    return true;
  }
  for (const ASN1_OBJECT *user_policy : user_policies) {
    if (is_any_policy(user_policy)) {
      return true;
    }
  }

  // Step (g.iii) does not delete anyPolicy nodes, so if the graph has
  // anyPolicy, some explicit policy will survive. The actual intersection may
  // synthesize some nodes in step (g.iii.3), but we do not return the policy
  // list itself, so we skip actually computing this.
  if (levels.back().has_any_policy()) {
    return true;
  }

  // We defer pruning the tree, so as we look for nodes with parent anyPolicy,
  // step (g.iii.1), we must limit to nodes reachable from the bottommost level.
  // Start by marking each of those nodes as reachable.
  for (X509PolicyNode &node : levels.back().nodes()) {
    node.reachable = true;
  }

  const size_t num_levels = levels.size();
  for (size_t i = num_levels - 1; i < num_levels; i--) {
    X509PolicyLevel &level = levels[i];
    for (X509PolicyNode &node : level.nodes()) {
      if (!node.reachable) {
        continue;
      }
      if (node.parent_is_any_policy()) {
        // |node|'s parent is anyPolicy and is part of "valid_policy_node_set".
        // If it exists in |user_policies|, the intersection is non-empty and we
        // can return immediately.
        if (sk_ASN1_OBJECT_find(user_policies, /*out_index=*/nullptr,
                                node.policy.get())) {
          return true;
        }
      } else if (i > 0) {
        // |node|'s parents are concrete policies. Mark the parents reachable,
        // to be inspected by the next loop iteration.
        X509PolicyLevel &prev = levels[i - 1];
        for (const auto &parent_policy : node.parent_policies) {
          X509PolicyNode *parent_node = prev.Find(parent_policy.get());
          if (parent_node != nullptr) {
            parent_node->reachable = true;
          }
        }
      }
    }
  }

  return false;
}

int asn1_object_cmp(const ASN1_OBJECT *const *a, const ASN1_OBJECT *const *b) {
  return OBJ_cmp(*a, *b);
}

}  // namespace

int X509_policy_check(const STACK_OF(X509) *certs,
                      const STACK_OF(ASN1_OBJECT) *user_policies,
                      unsigned long flags, X509 **out_current_cert) {
  *out_current_cert = nullptr;

  // Skip policy checking if the chain is just the trust anchor.
  const size_t num_certs = sk_X509_num(certs);
  if (num_certs <= 1) {
    return X509_V_OK;
  }

  // See RFC 5280, section 6.1.2, steps (d) through (f).
  size_t explicit_policy =
      (flags & X509_V_FLAG_EXPLICIT_POLICY) ? 0 : num_certs + 1;
  size_t inhibit_any_policy =
      (flags & X509_V_FLAG_INHIBIT_ANY) ? 0 : num_certs + 1;
  size_t policy_mapping = (flags & X509_V_FLAG_INHIBIT_MAP) ? 0 : num_certs + 1;

  Vector<X509PolicyLevel> levels;
  std::optional<X509PolicyLevel> level;
  for (size_t i = num_certs - 2; i < num_certs; i--) {
    X509 *cert = sk_X509_value(certs, i);
    uint32_t ex_flags = X509_get_extension_flags(cert);
    if (ex_flags & EXFLAG_INVALID) {
      return X509_V_ERR_OUT_OF_MEM;
    }
    const bool is_self_issued = (ex_flags & EXFLAG_SI) != 0;

    // In all but the first iteration, the previous iteration will have prepared
    // "expected_policy_set" for us as a staging level.
    if (!level.has_value()) {
      assert(i == num_certs - 2);
      level.emplace();
      level->set_has_any_policy(true);
    }

    // RFC 5280, section 6.1.3, steps (d) and (e). |any_policy_allowed| is
    // computed as in step (d.2).
    const int any_policy_allowed =
        inhibit_any_policy > 0 || (i > 0 && is_self_issued);
    if (!process_certificate_policies(cert, &*level, any_policy_allowed)) {
      *out_current_cert = cert;
      return X509_V_ERR_INVALID_POLICY_EXTENSION;
    }

    // RFC 5280, section 6.1.3, step (f).
    if (explicit_policy == 0 && level->is_empty()) {
      return X509_V_ERR_NO_EXPLICIT_POLICY;
    }

    // Insert the completed level into the list.
    if (!levels.Push(*std::exchange(level, std::nullopt))) {
      return X509_V_ERR_OUT_OF_MEM;
    }
    level = std::nullopt;

    // If this is not the leaf certificate, we go to section 6.1.4. If it
    // is the leaf certificate, we go to section 6.1.5 instead.
    if (i != 0) {
      // RFC 5280, section 6.1.4, steps (a) and (b).
      level = process_policy_mappings(cert, &levels.back(), policy_mapping > 0);
      if (!level.has_value()) {
        *out_current_cert = cert;
        return X509_V_ERR_INVALID_POLICY_EXTENSION;
      }
    }

    // RFC 5280, section 6.1.4, step (h-j) for non-leaves, and section 6.1.5,
    // step (a-b) for leaves. In the leaf case, RFC 5280 says only to update
    // |explicit_policy|, but |policy_mapping| and |inhibit_any_policy| are no
    // longer read at this point, so we use the same process.
    if (i == 0 || !is_self_issued) {
      if (explicit_policy > 0) {
        explicit_policy--;
      }
      if (policy_mapping > 0) {
        policy_mapping--;
      }
      if (inhibit_any_policy > 0) {
        inhibit_any_policy--;
      }
    }
    if (!process_policy_constraints(cert, &explicit_policy, &policy_mapping,
                                    &inhibit_any_policy)) {
      *out_current_cert = cert;
      return X509_V_ERR_INVALID_POLICY_EXTENSION;
    }
  }

  // RFC 5280, section 6.1.5, step (g). We do not output the policy set, so it
  // is only necessary to check if the user-constrained-policy-set is not empty.
  if (explicit_policy == 0) {
    // Build a sorted copy of |user_policies| for more efficient lookup.
    STACK_OF(ASN1_OBJECT) *user_policies_sorted = nullptr;
    // |user_policies_sorted|'s contents are owned by |user_policies|, so we do
    // not use |sk_ASN1_OBJECT_pop_free|.
    Cleanup cleanup = [&] { sk_ASN1_OBJECT_free(user_policies_sorted); };
    if (user_policies != nullptr) {
      user_policies_sorted = sk_ASN1_OBJECT_dup(user_policies);
      if (user_policies_sorted == nullptr) {
        return X509_V_ERR_OUT_OF_MEM;
      }
      sk_ASN1_OBJECT_set_cmp_func(user_policies_sorted, asn1_object_cmp);
      sk_ASN1_OBJECT_sort(user_policies_sorted);
    }

    if (!has_explicit_policy(Span(levels), user_policies_sorted)) {
      return X509_V_ERR_NO_EXPLICIT_POLICY;
    }
  }

  return X509_V_OK;
}

BSSL_NAMESPACE_END
