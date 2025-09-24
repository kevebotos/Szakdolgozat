#pragma once
#include <cstddef>
#include <iosfwd> // std::ostream elődeklaráció
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

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

class MeshError : public std::runtime_error
{
public:
  using std::runtime_error::runtime_error;
};

class MeshParseError : public MeshError
{
public:
  MeshParseError(std::size_t line, const std::string &message)
      : MeshError(message), m_line(line)
  {
  }

  std::size_t line() const noexcept { return m_line; }

private:
  std::size_t m_line = 0;
};

// MSH v2 ASCII beolvasás (Nodes, Elements[etype=2], PhysicalNames). Siker esetén mesh feltöltve, egyébként MeshError kivétel dobódik.
void load_msh2(const std::string &path, Mesh &mesh, std::ostream &log);
