#include "omnigraph_wrapper.hpp"

#include "cursor.hpp"
#include "hmmpath.hpp"
#include "utils.hpp"

#include "debug_assert/debug_assert.hpp"

#include "assembly_graph/core/graph.hpp"
#include "assembly_graph/components/graph_component.hpp"

using namespace debruijn_graph;

struct omnigraph_assert : debug_assert::default_handler,
                          debug_assert::set_level<1> {};

std::vector<DebruijnGraphCursor> DebruijnGraphCursor::prev() const {
    // Case 1: edge is a tip and we're inside the terminal vertex
    if (position_ == 0) {
        // assert(pg_->ingoing_[edge_id_].size() == 0);
        return {};
    }

    // Case 2: move backwards possibly going inside the terminal vertex of a
    // tip
    if (position_ != g_->k() ||
        g_->IncomingEdgeCount(g_->EdgeStart(e_)) == 0) {
        return { DebruijnGraphCursor(g_, e_, position_ - 1) };
    }

    // Case 3: go into incoming edges
    DEBUG_ASSERT(position_ == g_->k(), omnigraph_assert{});
    DEBUG_ASSERT(g_->IncomingEdgeCount(g_->EdgeStart(e_)), omnigraph_assert{});
    std::vector<DebruijnGraphCursor> result;
    for (EdgeId in : g_->IncomingEdges(g_->EdgeStart(e_)))
        result.emplace_back(g_, in, g_->length(in) + g_->k() - 1);

    return result;
}


inline std::vector<DebruijnGraphCursor> DebruijnGraphCursor::next() const {
    // Common case: we have not reached the end of the edge (in nucls)
    if (position_ + 1 < g_->length(e_) + g_->k())
        return { DebruijnGraphCursor(g_, e_, position_ + 1) };

    // Otherwise we're inside the vertex and need to go out of it
    DEBUG_ASSERT(position_ + 1 == g_->length(e_) + g_->k(), omnigraph_assert{});
    std::vector<DebruijnGraphCursor> result;
    result.reserve(4);
    for (EdgeId out : g_->OutgoingEdges(g_->EdgeEnd(e_)))
        result.emplace_back(g_, out, g_->k());  // Vertices are k-mers

    return result;
}

std::vector<DebruijnComponentCursor> DebruijnComponentCursor::prev() const {
    const ConjugateDeBruijnGraph &g = c_->g();

    // Case 1: edge is a tip and we're inside the terminal vertex
    if (position_ == 0) {
        // assert(pc_->g().ingoing_[edge_id_].size() == 0);
        return {};
    }

    // Case 2: move backwards possibly going inside the terminal vertex of a
    // tip
    if (position_ != g.k() ||
        (g.IncomingEdgeCount(g.EdgeStart(e_)) == 0)) {
        return { DebruijnComponentCursor(c_, e_, position_ - 1) };
    }

    // Case 3: go into incoming edges
    DEBUG_ASSERT(position_ == g.k(), omnigraph_assert{});
    DEBUG_ASSERT(g.IncomingEdgeCount(g.EdgeStart(e_)), omnigraph_assert{});
    std::vector<DebruijnComponentCursor> result;
    for (EdgeId in : g.IncomingEdges(g.EdgeStart(e_))) {
        if (!c_->contains(in))
            continue; // We need to stay in component
        result.emplace_back(c_, in, g.length(in) + g.k() - 1);
    }

    return result;
}


inline std::vector<DebruijnComponentCursor> DebruijnComponentCursor::next() const {
    const ConjugateDeBruijnGraph &g = c_->g();

    // Common case: we have not reached the end of the edge (in nucls)
    if (position_ + 1 < g.length(e_) + g.k())
        return { DebruijnComponentCursor(c_, e_, position_ + 1) };

    // Otherwise we're inside the vertex and need to go out of it
    DEBUG_ASSERT(position_ + 1 == g.length(e_) + g.k(), omnigraph_assert{});
    std::vector<DebruijnComponentCursor> result;
    result.reserve(4);
    for (EdgeId out : g.OutgoingEdges(g.EdgeEnd(e_))) {
        if (!c_->contains(out))
            continue; // We need to stay in component
        result.emplace_back(c_, out, g.k());  // Vertices are k-mers
    }

    return result;
}


std::vector<DebruijnGraphCursor> DebruijnGraphCursor::all(const ConjugateDeBruijnGraph &g) {
    std::vector<DebruijnGraphCursor> result;
    for (auto it = g.ConstEdgeBegin(); !it.IsEnd(); ++it) {
        EdgeId e = *it;
        for (size_t pos = 0; pos < g.length(e) + g.k(); ++pos) {
            DebruijnGraphCursor cursor{&g, e, pos};
            cursor.normalize_prefix_to_suffix();
            result.push_back(std::move(cursor));
        }
    }
    return result;
}

std::vector<DebruijnComponentCursor> DebruijnComponentCursor::all(const omnigraph::GraphComponent<ConjugateDeBruijnGraph> &c) {
    std::vector<DebruijnComponentCursor> result;
    for (auto it = c.e_begin(); it != c.e_end(); ++it) {
        EdgeId e = *it;
        for (size_t pos = 0; pos < c.g().length(e) + c.g().k(); ++pos) {
            DebruijnComponentCursor cursor{&c, e, pos};
            cursor.normalize_prefix_to_suffix();
            result.push_back(std::move(cursor));
        }
    }
    return result;
}

PathSet<DebruijnGraphCursor>
find_best_path(const hmm::Fees &fees,
               const std::vector<DebruijnGraphCursor> &initial) {
  return impl::find_best_path(fees, initial);
}

PathSet<RestrictedGraphCursor<DebruijnGraphCursor>>
find_best_path(const hmm::Fees &fees,
               const std::vector<RestrictedGraphCursor<DebruijnGraphCursor>> &initial) {
  return impl::find_best_path(fees, initial);
}

PathSet<DebruijnComponentCursor>
find_best_path(const hmm::Fees &fees,
               const std::vector<DebruijnComponentCursor> &initial) {
  return impl::find_best_path(fees, initial);
}

PathSet<ReversalGraphCursor<DebruijnGraphCursor>>
find_best_path_rev(const hmm::Fees &fees,
                   const std::vector<ReversalGraphCursor<DebruijnGraphCursor>> &initial) {
  return impl::find_best_path(fees, initial);
}

PathSet<ReversalGraphCursor<DebruijnComponentCursor>>
find_best_path_rev(const hmm::Fees &fees,
                   const std::vector<ReversalGraphCursor<DebruijnComponentCursor>> &initial) {
  return impl::find_best_path(fees, initial);
}

PathSet<AAGraphCursor<DebruijnComponentCursor>>
find_best_path(const hmm::Fees &fees,
               const std::vector<AAGraphCursor<DebruijnComponentCursor>> &initial) {
  return impl::find_best_path(fees, initial);
}

PathSet<AAGraphCursor<DebruijnGraphCursor>>
find_best_path(const hmm::Fees &fees,
               const std::vector<AAGraphCursor<DebruijnGraphCursor>> &initial) {
  return impl::find_best_path(fees, initial);
}

PathSet<AAGraphCursor<RestrictedGraphCursor<DebruijnGraphCursor>>>
find_best_path(const hmm::Fees &fees,
               const std::vector<AAGraphCursor<RestrictedGraphCursor<DebruijnGraphCursor>>> &initial) {
  return impl::find_best_path(fees, initial);
}
