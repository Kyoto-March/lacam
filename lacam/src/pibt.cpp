#include "../include/pibt.hpp"

bool PIBT::SWAP = true;
bool PIBT::HINDRANCE = true;

PIBT::PIBT(const Instance *_ins, DistTable *_D, int seed)
    : ins(_ins),
      MT(seed),
      rrd(0, 1),
      N(ins->N),
      V_size(ins->G.size()),
      D(_D),
      NO_AGENT(N),
      occupied_now(V_size, NO_AGENT),
      occupied_next(V_size, NO_AGENT),
      C_next(N),
      C_indices(N)
{
}

PIBT::~PIBT() {}

bool PIBT::set_new_config(const Config &Q_from, Config &Q_to,
                          const std::vector<int> &order)
{
  bool success = true;
  // setup cache & constraints check
  for (auto i = 0; i < N; ++i) {
    // set occupied now
    occupied_now[Q_from[i]->id] = i;

    // set occupied next
    if (Q_to[i] != nullptr) {
      // vertex collision
      if (occupied_next[Q_to[i]->id] != NO_AGENT) {
        success = false;
        break;
      }
      // swap collision
      auto j = occupied_now[Q_to[i]->id];
      if (j != NO_AGENT && j != i && Q_to[j] == Q_from[i]) {
        success = false;
        break;
      }
      occupied_next[Q_to[i]->id] = i;
    }
  }

  if (success) {
    for (auto i : order) {
      if (Q_to[i] == nullptr && !funcPIBT(i, Q_from, Q_to)) {
        success = false;
        break;
      }
    }
  }

  // cleanup
  for (auto i = 0; i < N; ++i) {
    occupied_now[Q_from[i]->id] = NO_AGENT;
    if (Q_to[i] != nullptr) occupied_next[Q_to[i]->id] = NO_AGENT;
  }

  return success;
}

bool PIBT::funcPIBT(const int i, const Config &Q_from, Config &Q_to)
{
  const auto K = Q_from[i]->neighbors.size();

  // hindrance preparation
  int num_neighbor_agents = 0;
  static std::array<int, 4> neighbor_agents;
  if (HINDRANCE) {
    for (auto u : Q_from[i]->neighbors) {
      if (occupied_now[u->id] != NO_AGENT) {
        neighbor_agents[num_neighbor_agents] = occupied_now[u->id];
        ++num_neighbor_agents;
      }
    }
  }

  auto get_successor_cost = [&](Vertex *u, bool swap = false) {
    auto e = rrd(MT);
    if (swap) return std::make_tuple(-D->get(i, u), 0, e);

    int hindrance = 0;
    if (HINDRANCE) {
      for (auto k = 0; k < num_neighbor_agents; ++k) {
        auto &&j = neighbor_agents[k];
        if (Q_from[j] != u && D->get(j, u) < D->get(j, Q_from[j])) {
          hindrance += 1;
        }
      }
    }

    return std::make_tuple(D->get(i, u), hindrance, e);
  };

  // set C_next
  for (size_t k = 0; k <= K; ++k) {
    auto u = Q_from[i]->actions[k];
    C_next[i][k] = u;
    C_cost[k] = get_successor_cost(u);
  }
  // sort, note: K + 1 is sufficient
  std::iota(C_indices[i].begin(), C_indices[i].begin() + K + 1, 0);
  std::sort(C_indices[i].begin(), C_indices[i].begin() + K + 1,
            [&](const int k, const int l) { return C_cost[k] < C_cost[l]; });

  // emulate swap
  const auto swap_agent = is_swap_required_and_possible(
      i, Q_from, Q_to, C_next[i][C_indices[i][0]]);
  if (swap_agent != NO_AGENT) {
    // recompute action cost
    for (size_t k = 0; k < K + 1; ++k) {
      C_cost[k] = get_successor_cost(C_next[i][k], true);
      C_indices[i][k] = k;
    }
    std::sort(C_indices[i].begin(), C_indices[i].begin() + K + 1,
              [&](const int k, const int l) { return C_cost[k] < C_cost[l]; });
  }
  auto swap_operation = [&]() {
    if (swap_agent != NO_AGENT &&                 // swap_agent exists
        Q_to[swap_agent] == nullptr &&            // not decided
        occupied_next[Q_from[i]->id] == NO_AGENT  // free
    ) {
      // pull swap_agent
      occupied_next[Q_from[i]->id] = swap_agent;
      Q_to[swap_agent] = Q_from[i];
    }
  };

  // main loop
  for (size_t k = 0; k < K + 1; ++k) {
    auto u_idx = C_indices[i][k];
    auto u = C_next[i][u_idx];

    // avoid vertex conflicts
    if (occupied_next[u->id] != NO_AGENT) continue;

    const auto j = occupied_now[u->id];

    // avoid swap conflicts with constraints
    if (j != NO_AGENT && Q_to[j] == Q_from[i]) continue;

    // reserve next location
    occupied_next[u->id] = i;
    Q_to[i] = u;

    // priority inheritance
    if (j != NO_AGENT && u != Q_from[i] && Q_to[j] == nullptr &&
        !funcPIBT(j, Q_from, Q_to)) {
      continue;
    }

    // success to plan next one step
    if (k == 0) swap_operation();
    return true;
  }

  // failed to secure node
  occupied_next[Q_from[i]->id] = i;
  Q_to[i] = Q_from[i];
  return false;
}

int PIBT::is_swap_required_and_possible(const int i, const Config &Q_from,
                                        Config &Q_to, Vertex *v_i_target)
{
  if (!SWAP) return NO_AGENT;
  // agent-j occupying the desired vertex for agent-i
  const auto j = occupied_now[v_i_target->id];
  if (j != NO_AGENT && j != i &&  // j exists
      Q_to[j] == nullptr &&       // j does not decide next location
      is_swap_required(i, j, Q_from[i], Q_from[j]) &&  // swap required
      is_swap_possible(Q_from[j], Q_from[i])           // swap possible
  ) {
    return j;
  }

  // for clear operation, c.f., push & swap
  if (v_i_target != Q_from[i]) {
    for (auto u : Q_from[i]->neighbors) {
      const auto k = occupied_now[u->id];
      if (k != NO_AGENT &&            // k exists
          v_i_target != Q_from[k] &&  // this is for clear operation
          is_swap_required(k, i, Q_from[i],
                           v_i_target) &&  // emulating from one step ahead
          is_swap_possible(v_i_target, Q_from[i])) {
        return k;
      }
    }
  }
  return NO_AGENT;
}

bool PIBT::is_swap_required(const int pusher, const int puller,
                            Vertex *v_pusher_origin, Vertex *v_puller_origin)
{
  auto v_pusher = v_pusher_origin;
  auto v_puller = v_puller_origin;
  Vertex *tmp = nullptr;
  while (D->get(pusher, v_puller) < D->get(pusher, v_pusher)) {
    auto n = v_puller->neighbors.size();
    // remove agents who need not to move
    for (auto u : v_puller->neighbors) {
      const auto i = occupied_now[u->id];
      if (u == v_pusher ||
          (u->neighbors.size() == 1 && i != NO_AGENT && ins->goals[i] == u)) {
        --n;
      } else {
        tmp = u;
      }
    }
    if (n >= 2) return false;  // able to swap at v_l
    if (n <= 0) break;
    v_pusher = v_puller;
    v_puller = tmp;
  }

  return (D->get(puller, v_pusher) < D->get(puller, v_puller)) &&
         (D->get(pusher, v_pusher) == 0 ||
          D->get(pusher, v_puller) < D->get(pusher, v_pusher));
}

bool PIBT::is_swap_possible(Vertex *v_pusher_origin, Vertex *v_puller_origin)
{
  // simulate pull
  auto v_pusher = v_pusher_origin;
  auto v_puller = v_puller_origin;
  Vertex *tmp = nullptr;
  while (v_puller != v_pusher_origin) {  // avoid loop
    auto n = v_puller->neighbors.size();
    for (auto u : v_puller->neighbors) {
      const auto i = occupied_now[u->id];
      if (u == v_pusher ||
          (u->neighbors.size() == 1 && i != NO_AGENT && ins->goals[i] == u)) {
        --n;
      } else {
        tmp = u;
      }
    }
    if (n >= 2) return true;  // able to swap at v_next
    if (n <= 0) return false;
    v_pusher = v_puller;
    v_puller = tmp;
  }
  return false;
}
