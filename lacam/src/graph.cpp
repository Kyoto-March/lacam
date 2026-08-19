#include "../include/graph.hpp"

Vertex::Vertex(int _id, int _index, int _x, int _y)
    : id(_id), index(_index), x(_x), y(_y), neighbors()
{
}

Graph::Graph(int w, int h) : V(), U(w * h, nullptr), width(w), height(h) {}

Graph::~Graph()
{
  for (auto &v : V)
    if (v != nullptr) delete v;
  V.clear();
}

// to load graph
static const std::regex r_height = std::regex(R"(height\s(\d+))");
static const std::regex r_width = std::regex(R"(width\s(\d+))");
static const std::regex r_map = std::regex(R"(map)");

Graph::Graph(const std::string &filename) : V(Vertices()), width(0), height(0)
{
  std::ifstream file(filename);
  if (!file) {
    std::cout << "file " << filename << " is not found." << std::endl;
    return;
  }
  std::string line;
  std::smatch results;

  // read fundamental graph parameters
  while (getline(file, line)) {
    // for CRLF coding
    if (*(line.end() - 1) == 0x0d) line.pop_back();

    if (std::regex_match(line, results, r_height)) {
      height = std::stoi(results[1].str());
    }
    if (std::regex_match(line, results, r_width)) {
      width = std::stoi(results[1].str());
    }
    if (std::regex_match(line, results, r_map)) break;
  }

  U = Vertices(width * height, nullptr);

  // create vertices
  int y = 0;
  while (getline(file, line)) {
    // for CRLF coding
    if (*(line.end() - 1) == 0x0d) line.pop_back();
    for (int x = 0; x < width; ++x) {
      char s = line[x];
      if (s == 'T' or s == '@') continue;  // object
      auto index = width * y + x;
      auto v = new Vertex(V.size(), index, x, y);
      V.push_back(v);
      U[index] = v;
    }
    ++y;
  }
  file.close();

  // create edges
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      auto v = U[width * y + x];
      if (v == nullptr) continue;
      // left
      if (x > 0) {
        auto u = U[width * y + (x - 1)];
        if (u != nullptr) v->neighbors.push_back(u);
      }
      // right
      if (x < width - 1) {
        auto u = U[width * y + (x + 1)];
        if (u != nullptr) v->neighbors.push_back(u);
      }
      // up
      if (y < height - 1) {
        auto u = U[width * (y + 1) + x];
        if (u != nullptr) v->neighbors.push_back(u);
      }
      // down
      if (y > 0) {
        auto u = U[width * (y - 1) + x];
        if (u != nullptr) v->neighbors.push_back(u);
      }
      v->actions = v->neighbors;
      v->actions.push_back(v);
    }
  }
}

int Graph::size() const { return V.size(); }

void Graph::save(const std::string &output_name) const
{
  std::ofstream log;
  log.open(output_name, std::ios::out);
  log << "height " << height
      << "\n"
         "width "
      << width << "\n"
      << "map\n"
      << this;
  log.close();
}

bool is_connected(const Graph *G)
{
  auto OPEN = std::queue<Vertex *>();
  auto CLOSED = std::vector<bool>(G->size(), false);
  int cnt = 0;

  OPEN.push(G->V[0]);
  while (!OPEN.empty()) {
    auto u = OPEN.front();
    OPEN.pop();
    if (CLOSED[u->id]) continue;
    CLOSED[u->id] = true;
    ++cnt;
    for (auto v : u->neighbors) {
      if (CLOSED[v->id]) continue;
      OPEN.push(v);
    }
  }
  return cnt == G->size();
}

bool is_connected(const Graph &G) { return is_connected(&G); }

bool is_same_config(const Config &C1, const Config &C2)
{
  const auto N = C1.size();
  for (size_t i = 0; i < N; ++i) {
    if (C1[i]->id != C2[i]->id) return false;
  }
  return true;
}

uint ConfigHasher::operator()(const Config &C) const
{
  uint hash = C.size();
  for (auto &v : C) {
    hash ^= v->id + 0x9e3779b9 + (hash << 6) + (hash >> 2);
  }
  return hash;
}

std::ostream &operator<<(std::ostream &os, const Vertex *v)
{
  os << v->index;
  return os;
}

std::ostream &operator<<(std::ostream &os, const Config &Q)
{
  for (auto v : Q) os << v << ",";
  return os;
}

std::ostream &operator<<(std::ostream &os, const Paths &paths)
{
  for (size_t i = 0; i < paths.size(); ++i) {
    os << i << ":";
    for (auto &v : paths[i]) {
      os << std::setw(4) << v << "->";
    }
    std::cout << std::endl;
  }
  return os;
}

std::ostream &operator<<(std::ostream &os, const Graph &G)
{
  os << &G;
  return os;
}

std::ostream &operator<<(std::ostream &os, const Graph *G)
{
  auto i = 0;
  for (auto y = 0; y < G->height; ++y) {
    for (auto x = 0; x < G->width; ++x) {
      os << (G->U[i] == nullptr ? "@" : ".");
      ++i;
    }
    os << "\n";
  }
  return os;
}
