#include "mesh.hpp"
#include "xs.hpp"
#include <exception>
#include <iostream>
#include <map>
#include <set>
#include <cstring>

static std::string lookup_phys_name(const Mesh &mesh, int phys)
{
  auto it = mesh.physNames.find(phys);
  if (it != mesh.physNames.end())
  {
    return it->second;
  }
  return std::string("<nincs név>");
}

static void print_group_values(const std::string &label, const std::vector<double> &values)
{
  std::cout << "    " << label << ":";
  for (std::size_t i = 0; i < values.size(); ++i)
  {
    std::cout << " " << values[i];
  }
  std::cout << "\n";
}

static std::map<int, const XsMaterial *> build_phys_xs_map(const Mesh &mesh, const XsLibrary &library)
{
  std::map<int, const XsMaterial *> mapping;
  for (std::map<int, std::string>::const_iterator it = mesh.physNames.begin(); it != mesh.physNames.end(); ++it)
  {
    const int physId = it->first;
    const std::string &physName = it->second;
    if (physName.empty())
    {
      std::cerr << "[FIGYELMEZTETÉS] Fizikai csoport név nélkül (id=" << physId << "), kihagyom.\n";
      continue;
    }
    const XsMaterial *material = library.find_material(physName);
    if (material == nullptr)
    {
      std::cerr << "[FIGYELMEZTETÉS] Nincs keresztmetszet adat a(z) " << physName << " csoporthoz.\n";
      continue;
    }
    mapping[physId] = material;
  }
  return mapping;
}

int main(int argc, char **argv)
{
  std::string meshPath = "stove.msh";
  std::string xsPath = "xs_vver440.txt";

  for (int i = 1; i < argc; ++i)
  {
    if ((std::strcmp(argv[i], "--mesh") == 0 || std::strcmp(argv[i], "-m") == 0) && i + 1 < argc)
    {
      meshPath = argv[++i];
    }
    else if ((std::strcmp(argv[i], "--xs") == 0 || std::strcmp(argv[i], "-x") == 0) && i + 1 < argc)
    {
      xsPath = argv[++i];
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

  const std::size_t nodeCount = (M.nodes.size() > 0 ? M.nodes.size() - 1 : 0);
  std::cout << "[OK] Háló beolvasva:\n";
  std::cout << "  Csomópontok: " << nodeCount << "\n";
  std::cout << "  1D elemek: " << M.lines.size() << "\n";
  std::cout << "  Háromszögek: " << M.tris.size() << "\n";

  if (!M.physNames.empty())
  {
    std::cout << "  Fizikai csoportok (id → név):\n";
    for (const auto &entry : M.physNames)
    {
      std::cout << "    " << entry.first << " → " << entry.second << "\n";
    }
  }

  std::map<int, std::size_t> triangleCountPerPhys;
  for (const auto &tri : M.tris)
  {
    triangleCountPerPhys[tri.phys] += 1;
  }
  if (!triangleCountPerPhys.empty())
  {
    std::cout << "  Háromszög elemek fizikai csoport szerint:\n";
    for (const auto &entry : triangleCountPerPhys)
    {
      const int physId = entry.first;
      const std::size_t triCount = entry.second;
      std::cout << "    phys=" << physId << " (" << lookup_phys_name(M, physId) << ") : " << triCount << " db\n";
    }
  }

  std::map<int, std::size_t> lineCountPerPhys;
  std::map<int, std::set<int>> lineNodesPerPhys;
  for (const auto &lineElem : M.lines)
  {
    lineCountPerPhys[lineElem.phys] += 1;
    lineNodesPerPhys[lineElem.phys].insert(lineElem.a);
    lineNodesPerPhys[lineElem.phys].insert(lineElem.b);
  }
  if (!lineCountPerPhys.empty())
  {
    std::cout << "  1D (él) elemek fizikai csoport szerint:\n";
    for (const auto &entry : lineCountPerPhys)
    {
      const int physId = entry.first;
      const std::size_t lineCount = entry.second;
      const std::size_t nodeCountOnBoundary = lineNodesPerPhys[physId].size();
      std::cout << "    phys=" << physId << " (" << lookup_phys_name(M, physId) << ") : "
                << lineCount << " db él, " << nodeCountOnBoundary << " db csomópont\n";
    }
  }
  else
  {
    std::cout << "  [MEGJEGYZÉS] Nem találtam 1D elemeket, így a peremet később kell definiálni.\n";
  }

  if (!lineNodesPerPhys.empty())
  {
    std::set<int> allBoundaryNodes;
    for (const auto &entry : lineNodesPerPhys)
    {
      allBoundaryNodes.insert(entry.second.begin(), entry.second.end());
    }
    std::cout << "  Összesen " << allBoundaryNodes.size() << " db egyedi csomópont kapcsolódik 1D elemekhez.\n";
  }

  XsLibrary xsLibrary;
  try
  {
    load_xs(xsPath, xsLibrary);
    std::cout << "\n[OK] Keresztmetszet könyvtár beolvasva: " << xsLibrary.title << "\n";
    std::cout << "  Energia csoportok száma: " << xsLibrary.energyGroupCount << "\n";
    if (!xsLibrary.energyGroupNames.empty())
    {
      std::cout << "  Csoportnevek:";
      for (std::size_t i = 0; i < xsLibrary.energyGroupNames.size(); ++i)
      {
        std::cout << " " << xsLibrary.energyGroupNames[i];
      }
      std::cout << "\n";
    }
    if (!xsLibrary.materialOrder.empty())
    {
      std::cout << "  Anyagok (sorrend):";
      for (std::size_t i = 0; i < xsLibrary.materialOrder.size(); ++i)
      {
        std::cout << " " << xsLibrary.materialOrder[i];
      }
      std::cout << "\n";
    }
    for (std::size_t index = 0; index < xsLibrary.materials.size(); ++index)
    {
      const XsMaterial &mat = xsLibrary.materials[index];
      std::cout << "    [" << mat.name << "]\n";
      print_group_values("sigma_t", mat.sigma_t);
      print_group_values("sigma_a", mat.sigma_a);
      print_group_values("nu_sigma_f", mat.nu_sigma_f);
      print_group_values("chi", mat.chi);
      std::cout << "    scatter mátrix:" << "\n";
      for (std::size_t row = 0; row < mat.scatter.size(); ++row)
      {
        std::cout << "      ";
        for (std::size_t col = 0; col < mat.scatter[row].size(); ++col)
        {
          std::cout << mat.scatter[row][col];
          if (col + 1 < mat.scatter[row].size())
          {
            std::cout << " ";
          }
        }
        std::cout << "\n";
      }
    }
    std::map<int, const XsMaterial *> physToXs = build_phys_xs_map(M, xsLibrary);
    if (!physToXs.empty())
    {
      std::cout << "\n[OK] Fizikai csoport → anyag hozzárendelés:\n";
      for (std::map<int, const XsMaterial *>::const_iterator it = physToXs.begin(); it != physToXs.end(); ++it)
      {
        const int physId = it->first;
        const XsMaterial *material = it->second;
        std::cout << "  phys=" << physId << " → " << material->name << "\n";
      }
    }
  }
  catch (const XsParseError &ex)
  {
    std::cerr << "Keresztmetszet beolvasási hiba (sor " << ex.line() << "): " << ex.what() << "\n";
    return 1;
  }
  catch (const XsError &ex)
  {
    std::cerr << "Keresztmetszet beolvasási hiba: " << ex.what() << "\n";
    return 1;
  }

  std::cout << "\nKész vagyunk!\n";
  return 0;
}
