#pragma once

#include "pathtrie.hpp"
#include "object_counter.hpp"

#include "utils/logger/logger.hpp"

#include <llvm/ADT/IntrusiveRefCntPtr.h>
#include <debug_assert/debug_assert.hpp>

#include <memory>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>

#include "fees.hpp"

enum class EventType { NONE, MATCH, INSERTION };
using score_t = double;

struct pathtree_assert : debug_assert::default_handler,
                         debug_assert::set_level<1> {};

namespace pathtree {

// TODO Pack Event (shoud weigth 4 bytes only!)
struct Event {
  unsigned int m;  // We do not need size_t here
  EventType type;
};

template <typename GraphCursor>
class PathLink : public llvm::RefCountedBase<PathLink<GraphCursor>>,
                 public AtomicObjectCounter<PathLink<GraphCursor>> {
  using This = PathLink<GraphCursor>;
  using ThisRef = llvm::IntrusiveRefCntPtr<This>;

  void* operator new (size_t sz) {
      return ::operator new(sz);
  }

 public:
  auto best_ancestor() const {
    DEBUG_ASSERT(scores_.size(), pathtree_assert{});
    return std::min_element(scores_.cbegin(), scores_.cend(),
                            [](const auto &e1, const auto &e2) { return e1.second.first < e2.second.first; });
  }

  double score() const {
    if (scores_.size() == 0) {
      return std::numeric_limits<double>::infinity();
    }

    return best_ancestor()->second.first;
  }

  bool update(GraphCursor gp, double score, const ThisRef &pl) {
    auto val = std::make_pair(score, pl);
    auto it_fl = scores_.insert({gp, val});
    bool inserted = it_fl.second;
    if (!inserted) {
      const auto &it = it_fl.first;
      if (it->second.first > score) {
        it->second = std::move(val);
        DEBUG_ASSERT(scores_.size(), pathtree_assert{});
        return true;
      } else {
        DEBUG_ASSERT(scores_.size(), pathtree_assert{});
        return false;
      }
    } else {
      DEBUG_ASSERT(scores_.size(), pathtree_assert{});
      return true;
    }
  }

  static ThisRef create() { return new This(); }
  ThisRef clone() const { return new This(*this); }

  static ThisRef master_source() {
    ThisRef result = create();
    result->scores_[GraphCursor()] = std::make_pair(0, nullptr);  // master_source score should be 0
    return result;
  }

  struct AnnotatedPath {
    std::vector<GraphCursor> path;
    double score;
    std::vector<Event> events;

    bool empty() const { return path.empty(); }

    size_t size() const {
      DEBUG_ASSERT(path.size() == events.size(), pathtree_assert{});
      return path.size();
    }
  };

  std::vector<AnnotatedPath> top_k(size_t k) const {
    struct Event {
      GraphCursor gp; // PathLink does not know its own position!
      const This *path_link;
    };

    using EventPath = pathtrie::NodeRef<Event>;
    struct QueueElement {
      EventPath path;
      double cost;  // TODO Use scores as in the paper
    };

    struct Comp {
      bool operator()(const QueueElement &e1, const QueueElement &e2) const {
        return e1.cost > e2.cost;
      }
    };

    std::priority_queue<QueueElement, std::vector<QueueElement>, Comp> q;

    std::vector<AnnotatedPath> result;
    auto SinkPath = pathtrie::make_root<Event>({GraphCursor(), this});
    q.push({SinkPath, score()});

    auto get_annotated_path = [&](const EventPath &epath, double cost) -> AnnotatedPath {
      std::vector<GraphCursor> path;
      std::vector<pathtree::Event> events;

      for (const auto &event : epath->collect()) {
        if (event.gp.is_empty()) {
          continue;  // TODO Think about a better way to exclude empty cursors
        }
        path.push_back(event.gp);
        events.push_back(event.path_link->event);
      }

      return AnnotatedPath{path, cost, events};
    };

    std::unordered_map<const This*, std::unordered_map<GraphCursor, const This*>> best_edges;

    while (!q.empty() && result.size() < k) {
      auto qe = q.top();
      q.pop();
      const GraphCursor &gp = qe.path->data().gp;
      const This *path_link = qe.path->data().path_link;
      const double &cost = qe.cost;

      TRACE("Extracting path with cost " << cost);
      // Check
      if (!qe.path->is_root()) {
        const GraphCursor &prev_gp = qe.path->parent()->data().gp;
        const This *prev_path_link = qe.path->parent()->data().path_link;
        auto &be = best_edges[path_link];
        // Trimming
        if (prev_gp.is_empty()) {
          be[prev_gp] = prev_path_link;  // Strong trimming!
          // Check has non-empty cursor
          if (std::any_of(be.cbegin(), be.cend(), [](const auto& x){ return !x.first.is_empty(); })) {
            continue;
          }
        } else {
          if (be.count(GraphCursor())) {
            continue;
          }
        }

        // Collapsing
        auto it = be.find(prev_gp);
        if (it != be.cend() && it->second != prev_path_link) {
          continue;
        }
        if (it == be.cend()) {
          be[prev_gp] = prev_path_link;
        }
      }

      // Check if path started with SOURCE: TODO implement it properly
      if (gp.is_empty() && !qe.path->is_root()) {
        // Report path
        auto annotated_path = get_annotated_path(qe.path, qe.cost);
        if (annotated_path.empty()) {
          // We should stop after the first empty path found
          // FIXME is it possible to obtain an empty path at all? --- Yes, it is, but it means that there are no proper paths
          WARN("Empty path reconstructed by top_k algorithm!");
          break;
        }
        result.push_back(get_annotated_path(qe.path, qe.cost));
        continue;
      }

      for (const auto &cdp : path_link->get_cursor_delta_trimmed_left()) {
        Event new_event{cdp.first, const_cast<const This *>(cdp.second.second.get())};
        auto new_path = qe.path->child(new_event);
        q.push({new_path, cost + cdp.second.first});
      }
    }

    return result;
  }

  Event event;
 private:
  std::unordered_map<GraphCursor, std::pair<double, ThisRef>> scores_;

  template <typename Function>
  auto apply(const Function &function) {
    std::unordered_set<const This *> checked;

    std::queue<const This *> q;
    q.push(this);
    while (!q.empty()) {
      const This *current = q.front();
      q.pop();
      if (!current || checked.count(current)) {
        continue;
      }

      function(*current);

      checked.insert(current);

      for (const auto &kv : current->scores_) {
        const This *p = kv.second.second.get();
        if (p) {
          q.push(p);
        }
      }
    }
  }

  auto get_cursor_delta_trimmed_left() const {
    // TODO Check and fix this description
    // the idea is in the following:
    // if a path is a prefix or suffix of another one,
    // we do not consider them as DIFFERENT paths. We leave only one (the best) of them
    // In order to implement this we do not allow to link to be "left-terminal" (contain backref to "empty cursor") and
    // "transit" (contain backrefs to other nontrivial links).
    // Here we just remove ref to empty cursor or refs to nontrivial links dependent on what is better

    // If terminal ref is present:
    // 1) Remove all other refs worse than it
    // 2) In case of some non-terminal refs still present, remove terminal ref as well
    // We add some tolerance. If two paths have almost equal scores we keep them both
    // const double eps = 1e-7;
    std::vector<std::pair<GraphCursor, std::pair<double, ThisRef>>> result(scores_.cbegin(), scores_.cend());
    std::sort(result.begin(), result.end(), [](const auto &p1, const auto &p2) { return p1.second.first < p2.second.first; });

    for (size_t i = 0; i < result.size(); ++i) {
      if (result[i].first.is_empty()) {
        size_t new_len = i + 1;
        if (new_len > 1) {
          --new_len;
        }
        result.resize(new_len);
        break;
      }
    }

    if (result.size()) {
      const double best_score = result[0].second.first;
      for (size_t i = 0; i < result.size(); ++i) {
        // delta >= 0
        result[i].second.first -= best_score;
      }
    }

    return result;
  }

};

template <class GraphCursor>
using PathLinkRef = llvm::IntrusiveRefCntPtr<PathLink<GraphCursor>>;
}  // namespace pathtree

template <class GraphCursor>
class PathSet {
 public:
  using AnnotatedPath = typename pathtree::PathLink<GraphCursor>::AnnotatedPath;
  class path_container {
   public:
    path_container(const pathtree::PathLink<GraphCursor> &paths, size_t k) : paths_(paths.top_k(k)) {}

    auto begin() const { return paths_.begin(); }
    auto end() const { return paths_.end(); }
    size_t size() const { return paths_.size(); }
    bool empty() const { return paths_.empty(); }
    auto operator[](size_t n) const { return paths_[n]; }

    template <class Cursor>
    static std::string str(const std::vector<Cursor> &path,
                           typename Cursor::Context context) {
      std::string s;
      for (size_t i = 0; i < path.size(); ++i) {
        DEBUG_ASSERT(!path[i].is_empty(), pathtree_assert{});

        s += path[i].letter(context);
      }
      return s;
    }

    static std::string compress_alignment(const std::string &s) {
      size_t count = 0;
      char prev_c = '\0';
      std::string result;
      for (size_t i = 0; i <= s.size(); ++i) {
        char c = s[i];
        if (c == prev_c) {
          ++count;
        } else {
          if (prev_c != '\0') {
            result += std::to_string(count);
            if (prev_c == '-') {
              prev_c = 'D';
            }
            result += prev_c;
          }
          count = 1;
        }
        prev_c = c;
      }

      return result;
    }

    static std::string alignment(const AnnotatedPath &apath, const hmm::Fees &fees,
                                 typename GraphCursor::Context context) {
      size_t m = fees.M;
      std::string s;
      size_t prev_position = 0;
      DEBUG_ASSERT(apath.path.size() == apath.events.size(), pathtree_assert{});
      for (size_t i = 0; i < apath.path.size(); ++i) {
        DEBUG_ASSERT(!apath.path[i].is_empty(), pathtree_assert{});

        if (apath.events[i].type == EventType::NONE) {
          ERROR("Position: " << i);
          ERROR("Path: " << str(apath.path, context));
          ERROR("Alignment:" << s);
        }
        DEBUG_ASSERT(apath.events[i].type != EventType::NONE, pathtree_assert{});
        for (size_t j = prev_position + 1; j < apath.events[i].m; ++j) {
          s += '-';
        }
        prev_position = apath.events[i].m;
        s += apath.events[i].type == EventType::MATCH ? (fees.consensus[apath.events[i].m - 1] == apath.path[i].letter(context) ? 'M' : 'X') : 'I';
      }

      // Add trailing gaps (-)
      for (size_t j = prev_position + 1; j <= m; ++j) {
          s += '-';
      }
      return s;
    }

    std::string str(size_t n, typename GraphCursor::Context context) const { return str(paths_[n].path, context); }
    std::string alignment(size_t n, const hmm::Fees &fees, typename GraphCursor::Context context) const { return alignment(paths_[n], fees, context); }

   private:
    std::vector<AnnotatedPath> paths_;
  };

  pathtree::PathLink<GraphCursor> &pathlink() { return pathlink_; }
  const pathtree::PathLink<GraphCursor> &pathlink() const { return pathlink_; }

  double best_score() const { return pathlink_.score(); }
  AnnotatedPath best_path() const { return top_k(1)[0]; }
  std::string best_path_string() const { return path_container::str(best_path().path); }

  path_container top_k(size_t k) const { return path_container(pathlink_, k); }

 private:
  pathtree::PathLink<GraphCursor> pathlink_;
};

// vim: set ts=2 sw=2 et :
