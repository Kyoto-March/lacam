#include "../include/planner.hpp"

Solution solve(const Instance &ins, int verbose, const Deadline *deadline,
               int seed)
{
  // distance table
  auto D = DistTable(ins);
  info(1, verbose, deadline,
       "set distance table, multi-thread init: ", DistTable::MULTI_THREAD_INIT);

  // lacam
  auto lacam = LaCAM(&ins, &D, verbose, deadline, seed);
  info(1, verbose, deadline, "start lacam");
  return lacam.solve();
}
