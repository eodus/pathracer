#pragma once

#include "cursor.hpp"
#include "fees.hpp"
#include "pathtree.hpp"

#include "utils/logger/logger.hpp"

#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <memory>
#include <vector>
#include <limits>

#include "depth_filter.hpp"

extern "C" {
#include "hmmer.h"
}

namespace impl {

using pathtree::PathLink;
using pathtree::PathLinkRef;



template <typename GraphCursor>
struct ScoredPLink {
  PathLinkRef<GraphCursor> plink;
  score_t score;
};

template <typename GraphCursor>
struct State {
  GraphCursor cursor;
  PathLinkRef<GraphCursor> plink;
  score_t score;
};


template <typename GraphCursor>
class DeletionStateSet : public std::unordered_map<GraphCursor, ScoredPLink<GraphCursor>> {
 public:
  std::vector<State<GraphCursor>> states() const {  // TODO implement this as a generator
    std::vector<State<GraphCursor>> states;
    for (const auto &kv : *this) {
      const auto &cursor = kv.first;
      const auto &score = kv.second.score;
      const auto &plink = kv.second.plink;

      states.push_back({cursor, plink, score});
    }

    return states;
  }

  bool update(const GraphCursor &cursor,
              score_t score,
              const PathLinkRef<GraphCursor> &plink) {
    auto it_fl = this->insert({cursor, {plink, score}});
    auto it = it_fl.first;
    bool inserted = it_fl.second;

    if (inserted) {
      return true;
    }

    const score_t &prev_score = it->second.score;
    if (prev_score > score) {
      it->second = {plink, score};
      return true;
    }
    return false;
  }

  template <typename Set>
  size_t merge(const Set &S, score_t fee = 0) {
    size_t count = 0;

    for (const auto &state : S.states()) {
      count += update(state.cursor, state.score + fee, state.plink);
    }

    return count;
  }

  void increment(score_t fee = 0) {
    for (auto &kv : *this) {
      kv.second.score += fee;
    }
  }

  template <typename Predicate>
  size_t filter(const Predicate &predicate) {
    size_t count = 0;
    for (auto it = this->begin(); it != this->end();) {
      if (predicate(*it)) {
        it = this->erase(it);
        ++count;
      } else {
        ++it;
      }
    }

    return count;
  }

  // TODO refactor it, make general code instof copy-paste
  auto scores() {
    std::vector<double> scores;
    scores.reserve(this->size());
    for (auto it = this->begin(); it != this->end(); ++it) {
      scores.push_back(it->second.score);
    }

    return scores;
  }


  size_t filter(size_t n, double score) {
    n = std::min(n, this->size());
    if (n == 0) {
      this->clear();
      return 0;
    }

    {
      auto scores = this->scores();
      std::nth_element(scores.begin(), scores.begin() + n - 1, scores.end());
      score = std::min(score, scores[n - 1]);
    }

    auto pred = [score](const auto &kv) {
      return kv.second.score > score;
    };

    return this->filter(pred);
  }
};


template <typename GraphCursor>
class StateSet : public std::unordered_map<GraphCursor, PathLinkRef<GraphCursor>> {
 public:
  std::vector<State<GraphCursor>> states() const {  // TODO implement this as a generator
    std::vector<State<GraphCursor>> states;
    for (const auto &kv : *this) {
      const auto &cursor = kv.first;
      const auto &score = kv.second->score();
      const auto &plink = kv.second;

      states.push_back({cursor, plink, score});
    }

    return states;
  }

  template <typename Container>
  std::vector<State<GraphCursor>> states(const Container &container) const {  // TODO implement this as a generator
    std::vector<State<GraphCursor>> states;
    for (const auto &cursor : container) {
      auto it = this->find(cursor);
      assert(it != this->cend());
      const auto &score = it->second->score();
      const auto &plink = it->second;

      states.push_back({cursor, plink, score});
    }

    return states;
  }

  void set_event(size_t m, EventType type) {
    for (auto &kv : *this) {
      if (!kv.first.is_empty()) {
        kv.second->event = {static_cast<unsigned int>(m), type};
      }
    }
  }

  bool check_events() const {
    for (const auto &kv : *this) {
      if (!kv.first.is_empty() && kv.second->event.type == EventType::NONE) {
        ERROR("None event on position " << kv.first);
        return false;
      }
      if (kv.first.is_empty() && kv.second->event.type != EventType::NONE) {
        ERROR("Nontrivial event on empty cursor position " << kv.first);
        return false;
      }
    }
    return true;
  }

  const PathLinkRef<GraphCursor> &get_or_create(const GraphCursor &key) {
    return this->insert({key, new PathLink<GraphCursor>()}).first->second;
  }

  StateSet clone() const {
    StateSet copy{*this};
    for (auto &kv : copy) {
      kv.second = kv.second->clone();
    }

    return copy;
  }

  bool update(const GraphCursor &key, double score, GraphCursor from,
              const PathLinkRef<GraphCursor> &traj) {
    auto it_fl = this->insert({key, new PathLink<GraphCursor>()});
    double prev = it_fl.second ? std::numeric_limits<double>::infinity() : it_fl.first->second->score();
    // double prev = it_fl.first->second->score();
    it_fl.first->second->update(from, score, traj);
    return prev > score;
  }

  // TODO implement method detecting upd of any kind
  //

  auto scores() {
    std::vector<double> scores;
    scores.reserve(this->size());
    for (auto it = this->begin(); it != this->end(); ++it) {
      scores.push_back(it->second->score());
    }

    return scores;
  }

  template <typename Predicate>
  size_t filter(const Predicate &predicate) {
    size_t count = 0;
    for (auto it = this->begin(); it != this->end();) {
      if (predicate(*it)) {
        it = this->erase(it);
        ++count;
      } else {
        ++it;
      }
    }

    return count;
  }

  size_t filter(size_t n, double score) {
    n = std::min(n, this->size());
    if (n == 0) {
      this->clear();
      return 0;
    }

    {
      auto scores = this->scores();
      std::nth_element(scores.begin(), scores.begin() + n - 1, scores.end());
      score = std::min(score, scores[n - 1]);
    }

    auto pred = [score](const auto &kv) {
      return kv.second->score() > score;
    };

    return this->filter(pred);
  }

};

template <typename GraphCursor>
PathSet<GraphCursor> find_best_path(const hmm::Fees &fees, const std::vector<GraphCursor> &initial_original) {
  const double absolute_threshold = 100.0;
  using StateSet = StateSet<GraphCursor>;
  using DeletionStateSet = DeletionStateSet<GraphCursor>;
  const auto &code = fees.code;

  INFO("pHMM size: " << fees.M);
  if (!fees.check_i_loop(0)) {
    WARN("Negative-cost insertion at the beginning");
  }
  if (!fees.check_i_loop(fees.M)) {
    WARN("Negative-cost insertion at the end");
  }

  for (size_t i = 0; i <= fees.M; ++i) {
    if (!fees.check_i_loop(i)) {
      WARN("Negative-cost insertion at position " << i);
    }
  }

  if (!fees.check_i_negative_loops()) {
    WARN("MODEL CONTAINS NEGATIVE I-LOOPS");
  }

  std::vector<GraphCursor> initial;

  auto transfer = [&code, &initial](StateSet &to, const auto &from, double transfer_fee,
                                    const std::vector<double> &emission_fees, const std::string & = "") {
    assert((void*)(&to) != (void*)(&from));
    for (const auto &state : from.states()) {
      const auto &cur = state.cursor;
      const auto &fee = state.score;
      const auto &id = state.plink;
      if (cur.is_empty()) {
        // This branch is used only during BEGIN->M, BEGIN->I and D->M transfers
        for (size_t i = 0; i < initial.size(); ++i) {
          const auto &next = initial[i];
          double cost = fee + transfer_fee + emission_fees[code(next.letter())];
          to.update(next, cost, cur, id);
        }
      } else {
        auto next_pairs = cur.next_pairs();
        for (size_t i = 0; i < next_pairs.size(); ++i) {
          const auto &next = next_pairs[i].first;
          char letter = next_pairs[i].second;
          double cost = fee + transfer_fee + emission_fees[code(letter)];
          to.update(next, cost, cur, id);
        }
      }
    }
  };

  auto transfer_upd = [&code, &initial](StateSet &to, const StateSet &from, double transfer_fee,
                                        const std::vector<double> &emission_fees, const std::string &,
                                        const std::unordered_set<GraphCursor> &keys) {
    assert(&to != &from);
    std::unordered_set<GraphCursor> updated;
    for (const auto &state : from.states(keys)) {
      const auto &cur = state.cursor;
      const auto &fee = state.score;
      const auto &id = state.plink;
      auto next_pairs = cur.next_pairs();
      for (size_t i = 0; i < next_pairs.size(); ++i) {
        const auto &next = next_pairs[i].first;
        char letter = next_pairs[i].second;
        double cost = fee + transfer_fee + emission_fees[code(letter)];
        if (to.update(next, cost, cur, id)) {
          updated.insert(next);
        }
      }
    }
    return updated;
  };

  auto i_loop_processing_negative = [&transfer_upd, &fees](StateSet &I, size_t m) {
    const size_t max_insertions = 30;

    std::unordered_set<GraphCursor> updated;
    for (const auto &kv : I) {
      updated.insert(kv.first);
    }
    I.set_event(m, EventType::INSERTION);
    StateSet Inew = I.clone();
    for (size_t i = 0; i < max_insertions; ++i) {
      updated = transfer_upd(Inew, I, fees.t[m][p7H_II], fees.ins[m], "o", updated);
      Inew.set_event(m, EventType::INSERTION);
      TRACE(updated.size() << " items updated");
      for (const auto &cur : updated) {
        I[cur] = Inew[cur]->clone();  // TODO Implement minor updation detection
      }
    }
    I = std::move(Inew);  // It is necessary to copy minorly updated states
  };

  auto i_loop_processing_non_negative = [&fees, &code, &absolute_threshold](StateSet &I, size_t m, const auto &filter) {
    const auto &emission_fees = fees.ins[m];
    const auto &transfer_fee = fees.t[m][p7H_II];

    TRACE(I.size() << " I states initially present in I-loop m = " << m);
    std::unordered_set<GraphCursor> updated;

    struct QueueElement {
      GraphCursor current_cursor;
      double score;
      GraphCursor source_cursor;
      PathLinkRef<GraphCursor> source_state;

      bool operator<(const QueueElement &other) const {
        return this->score > other.score;
      }
    };

    std::priority_queue<QueueElement> q;

    for (const auto &kv : I) {
      const auto &current_cursor = kv.first;
      auto best = kv.second->best_ancestor();
      const auto &score = best->second.first;
      if (score > absolute_threshold) {
        continue;
      }
      if (!filter(current_cursor)) {
        q.push({current_cursor, score, best->first, best->second.second});
      }
    }
    TRACE(q.size() << " I values in queue m = " << m);

    std::unordered_set<GraphCursor> processed;
    size_t taken_values = 0;
    while(!q.empty()) {
      QueueElement elt = q.top();
      q.pop();
      ++taken_values;

      if (elt.score > absolute_threshold) {
        break;
      }

      if (processed.count(elt.current_cursor)) {
        continue;
      }

      // add processed.size() limit

      processed.insert(elt.current_cursor);

      I.update(elt.current_cursor, elt.score, elt.source_cursor, elt.source_state);  // TODO return iterator to inserted/updated elt
      const auto &id = I[elt.current_cursor];
      auto next_pairs = elt.current_cursor.next_pairs();
      for (size_t i = 0; i < next_pairs.size(); ++i) {
        const auto &next = next_pairs[i].first;
        if (processed.count(next)) {
          continue;
        }
        char letter = next_pairs[i].second;
        double cost = elt.score + transfer_fee + emission_fees[code(letter)];
        if (!filter(next)) {
          q.push({next, cost, elt.current_cursor, id});
        }
      }
    }

    TRACE(processed.size() << " states processed in I-loop m = " << m);
    TRACE(taken_values << " values extracted from queue m = " << m);
    // TODO update secondary references.
    // Cycle references may appear =(
  };

  auto i_loop_processing = [&](StateSet &I, size_t m, const auto &filter) {
    return fees.is_i_loop_non_negative(m) ? i_loop_processing_non_negative(I, m, filter) : i_loop_processing_negative(I, m);
  };

  auto merge_state_set = [](StateSet &target, const StateSet &source, double transfer_fee = 0) {
    for (const auto &kv : source) {
      const auto &cur = kv.first;
      const auto &id = kv.second;
      target.get_or_create(cur)->merge_update(id.get(), transfer_fee);
    }
  };

  auto merge_state_set_best = [](StateSet &target, const StateSet &source, double transfer_fee = 0) {
    for (const auto &kv : source) {
      const auto &cur = kv.first;
      const auto &id = kv.second;
      target.get_or_create(cur)->merge_update_best(id.get(), transfer_fee);
    }
  };

  auto dm_new_bak = [&](StateSet &D, StateSet &M, const StateSet &I, size_t m) {
    StateSet Dnew;
    merge_state_set(Dnew, M, fees.t[m - 1][p7H_MD]);
    merge_state_set(Dnew, D, fees.t[m - 1][p7H_DD]);

    StateSet preM;
    // It's enough to merge only best scores here
    merge_state_set_best(preM, M, fees.t[m - 1][p7H_MM]);
    merge_state_set_best(preM, D, fees.t[m - 1][p7H_DM]);
    merge_state_set_best(preM, I, fees.t[m - 1][p7H_IM]);
    M.clear();
    transfer(M, preM, 0, fees.mat[m], "m");

    D = std::move(Dnew);
  };

  auto dm_new = [&](DeletionStateSet &D, StateSet &M, const StateSet &I, size_t m) {
    DeletionStateSet preM = D;

    D.increment(fees.t[m - 1][p7H_DD]);
    D.merge(M, fees.t[m - 1][p7H_MD]);

    preM.increment(fees.t[m - 1][p7H_DM]);
    preM.merge(M, fees.t[m - 1][p7H_MM]);
    preM.merge(I, fees.t[m - 1][p7H_IM]);

    M.clear();
    transfer(M, preM, 0, fees.mat[m], "m");
  };

  DepthAtLeast<GraphCursor> depth;

  std::unordered_set<GraphCursor> neighbourhood(initial_original.cbegin(), initial_original.cend());
  auto neighbourhood_filter_cursor = [&](const GraphCursor &cursor) -> bool {
    return !cursor.is_empty() && !neighbourhood.count(cursor);  // TODO Add empty() to neighbourhood set
  };
  auto neighbourhood_filter_pair = [&](const auto &kv) -> bool {
    const GraphCursor &cursor = kv.first;
    return neighbourhood_filter_cursor(cursor);
  };

  INFO("Original (before filtering) initial set size: " << initial_original.size());
  std::copy_if(initial_original.cbegin(), initial_original.cend(), std::back_inserter(initial),
               [&](const GraphCursor &cursor) { return depth.depth_at_least(cursor, static_cast<double>(fees.M) / 3 - 10); });  // FIXME Correct this condition for local-local matching
  INFO("Initial set size: " << initial.size());

  StateSet I, M;
  DeletionStateSet D;
  const auto empty = GraphCursor();
  auto base = PathLink<GraphCursor>::master_source();
  M[empty] = base;  // TODO Implement and use empty Trajectory() instead of Trajectory(0)

  INFO("The number of links (M): " << fees.M);

  size_t positions_left = fees.M;
  auto depth_filter_cursor = [&](const GraphCursor &cursor) -> bool {
    return !depth.depth_at_least(cursor, static_cast<double>(positions_left) / 3 - 10);
  };

  auto depth_and_neib_filter_cursor = [&](const GraphCursor &cursor) -> bool {
    return depth_filter_cursor(cursor) || neighbourhood_filter_cursor(cursor);
  };

  transfer(I, M, fees.t[0][p7H_MI], fees.ins[0], "i");
  i_loop_processing(I, 0, depth_and_neib_filter_cursor);  // Do we really need I at the beginning???
  I.set_event(0, EventType::INSERTION);
  size_t n = 1;
  for (size_t m = 1; m <= fees.M; ++m) {
    positions_left = fees.M - m;

    auto depth_filter_pair = [&](const auto &kv) -> bool {
      const GraphCursor &cursor = kv.first;
      return depth_filter_cursor(cursor);
    };

    dm_new(D, M, I, m);
    I.clear();
    transfer(I, M, fees.t[m][p7H_MI], fees.ins[m], "i");
    i_loop_processing(I, m, depth_filter_cursor);

    size_t n_of_states = D.size() + I.size() + M.size();

    TRACE("# states " << m << " => " << n_of_states);
    size_t top = n_of_states;
    if (m > 10) {
      top = 1000000;
    }
    if (m > 50) {
      top = 20000;
    }
    if (m > 500) {
      top = 10000;
    }

    if (m >= n) {
      INFO("Step #: " << m);
      INFO("# states " << m << " => " << n_of_states << ": I = " << I.size() << " M = " << M.size() << " D = " << D.size());
    }

    I.set_event(m, EventType::INSERTION);
    M.set_event(m, EventType::MATCH);

    I.filter(top, absolute_threshold);
    M.filter(top, absolute_threshold);
    D.filter(top, absolute_threshold);

    size_t depth_filtered = 0;
    depth_filtered += I.filter(depth_filter_pair);
    depth_filtered += M.filter(depth_filter_pair);
    depth_filtered += D.filter(depth_filter_pair);

    size_t neighbourhood_filtered = 0;
    neighbourhood_filtered += I.filter(neighbourhood_filter_pair);
    neighbourhood_filtered += M.filter(neighbourhood_filter_pair);
    neighbourhood_filtered += D.filter(neighbourhood_filter_pair);


    if (m >= n) {
      INFO("depth-filtered " << depth_filtered << ", positions left = " << positions_left << " states m = " << m);
      INFO("neighbourhood-filtered " << neighbourhood_filtered);
      INFO("I = " << I.size() << " M = " << M.size() << " D = " << D.size());
      auto scores = M.scores();
      std::sort(scores.begin(), scores.end());
      if (scores.size() > 100) { scores.resize(100); }
      INFO("Top scores: " << scores);
      n <<= 1;
    }
  }

  INFO("Max stack size in Depth: " << depth.max_stack_size());

  PathSet<GraphCursor> result;
  auto &terminal = result.pathlink();
  terminal.update(empty, std::numeric_limits<double>::infinity(), base);
  auto upd_terminal = [&terminal](const auto &S, double fee) {
    for (const auto &state : S.states()) {
      terminal.update(state.cursor, state.score + fee, state.plink);
    }
  };

  upd_terminal(D, fees.t[fees.M][p7H_DM]);
  upd_terminal(I, fees.t[fees.M][p7H_DM]);  // Do we really need I at the end?
  upd_terminal(M, fees.t[fees.M][p7H_MM]);

  result.clip_tails_non_aggressive();

  return result;
}

}  // namespace impl

// vim: set ts=2 sw=2 et :
