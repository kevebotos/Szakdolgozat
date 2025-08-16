#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <limits>
#include <iosfwd> // std::ostream elődeklaráció

struct Mesh
{
  struct Node
  {
    double x = 0.0, y = 0.0;
  };
  struct Tri
  {
    int a = 0, b = 0, c = 0;
    int phys = -1;
  };

  // 1-based indexelés: nodes[0] nem használt; nodes[1..N]
  std::vector<Node> nodes;
  std::vector<Tri> tris;

  // Physical ID -> név (a $PhysicalNames szekcióból)
  std::unordered_map<int, std::string> physNames;

  // Gyors bbox
  double minx = std::numeric_limits<double>::infinity();
  double miny = std::numeric_limits<double>::infinity();
  double maxx = -std::numeric_limits<double>::infinity();
  double maxy = -std::numeric_limits<double>::infinity();
};

// MSH v2 ASCII beolvasás (Nodes, Elements[etype=2], PhysicalNames). true = siker
bool load_msh2(const std::string &path, Mesh &mesh, std::ostream &log);