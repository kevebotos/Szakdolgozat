#include "mesh.hpp"
#include <exception>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <cstring>

static bool is_burner(int phys)
{
  return phys == 101 || phys == 102 || phys == 103 || phys == 104;
}

static double compute_eps(const Mesh &m)
{
  const double dx = m.maxx - m.minx;
  const double dy = m.maxy - m.miny;
  if (!std::isfinite(dx) || !std::isfinite(dy))
  {
    return 1e-12;
  }
  const double scale = std::max(dx, dy);
  return std::max(1e-12, scale * 1e-9); // nagyon kicsi, de a bbox-hoz igazodik
}

int main(int argc, char **argv)
{
  std::string meshPath = "stove.msh"; // alapértelmezett a projekt gyökerében
  for (int i = 1; i < argc; ++i)
  {
    if ((std::strcmp(argv[i], "--mesh") == 0 || std::strcmp(argv[i], "-m") == 0) && i + 1 < argc)
    {
      meshPath = argv[++i];
    }
  }

  Mesh M;
  try
  {
    load_msh2(meshPath, M, std::cout);
  }
  catch (const MeshParseError &ex)
  {
    std::cerr << "Hálóbeolvasási hiba (sor " << ex.line() << "): " << ex.what() << "\n";
    return 1;
  }
  catch (const MeshError &ex)
  {
    std::cerr << "Hálóbeolvasási hiba: " << ex.what() << "\n";
    return 1;
  }
  catch (const std::exception &ex)
  {
    std::cerr << "Váratlan hiba: " << ex.what() << "\n";
    return 1;
  }

  const std::size_t N = (M.nodes.size() > 0 ? M.nodes.size() - 1 : 0);
  std::cout << "[OK] Háló beolvasva:\n";
  std::cout << "  Csomópontok: " << N << "\n";
  std::cout << "  Háromszögek: " << M.tris.size() << "\n";
  std::cout << "  BBox: [" << M.minx << ", " << M.miny << "] — [" << M.maxx << ", " << M.maxy << "]\n";

  // Fizikai csoport névlista
  if (!M.physNames.empty())
  {
    std::cout << "  Physical names (id → név):\n";
    for (const auto &kv : M.physNames)
    {
      std::cout << "    " << kv.first << " → " << kv.second << "\n";
    }
  }

  // Háromszög darabszám fizikai csoportonként
  std::unordered_map<int, std::size_t> triCount;
  for (const auto &t : M.tris)
    triCount[t.phys]++;

  std::cout << "  Háromszögek fizikai csoportonként:\n";
  for (const auto &kv : triCount)
  {
    int id = kv.first;
    std::size_t cnt = kv.second;
    auto it = M.physNames.find(id);
    std::string name = (it != M.physNames.end() ? it->second : std::string("<nincs név>"));
    std::cout << "    phys=" << id << " (" << name << ") : " << cnt << " db\n";
  }

  // Külső perem csomópontok (téglalap szélein)
  const double eps = compute_eps(M);
  std::unordered_set<int> outer;
  for (int id = 1; id <= static_cast<int>(N); ++id)
  {
    const auto &p = M.nodes[id];
    if (std::fabs(p.x - M.minx) <= eps || std::fabs(p.x - M.maxx) <= eps ||
        std::fabs(p.y - M.miny) <= eps || std::fabs(p.y - M.maxy) <= eps)
    {
      outer.insert(id);
    }
  }
  std::cout << "  Külső perem (geometriai) csomópontok: " << outer.size() << " db\n";

  // Égők (Burner 1..4) csomópontjai — háromszög-fizikai ID alapján
  std::unordered_set<int> burnerNodes;
  for (const auto &t : M.tris)
  {
    if (is_burner(t.phys))
    {
      burnerNodes.insert(t.a);
      burnerNodes.insert(t.b);
      burnerNodes.insert(t.c);
    }
  }
  std::cout << "  Égőkhöz tartozó (belső) csomópontok (összesen): " << burnerNodes.size() << " db\n";

  std::cout << "\n Noice!\n";
  return 0;
}
