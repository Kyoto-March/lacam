#include <cassert>
#include <planner.hpp>

int main()
{
  {
    const std::string filename = "../assets/random-32-32-10.map";
    auto G = Graph(filename);
    assert(G.size() == 922);
    assert(G.V[0]->neighbors.size() == 2);
    assert(G.V[0]->neighbors[0]->id == 1);
    assert(G.V[0]->neighbors[1]->id == 28);
    assert(G.width == 32);
    assert(G.height == 32);
  }

  return 0;
}
