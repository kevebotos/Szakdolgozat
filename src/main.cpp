#include "mesh.hpp"
#include "xs.hpp"
#include "model.hpp"
#include "control.hpp"
#include <exception>
#include <iostream>
#include <map>
#include <set>
#include <cstring>
#include <chrono>
#include <fstream>
#include <iomanip>

// Helper: Fájl méret lekérdezése MB-ban
static double getFileSizeMB(const std::string &path)
{
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file)
    return 0.0;
  const double bytes = static_cast<double>(file.tellg());
  return bytes / (1024.0 * 1024.0);
}

// Helper: Memória becsült méret MB-ban
static double estimateMemoryMB(const Mesh &mesh)
{
  std::size_t bytes = 0;
  bytes += mesh.nodes.size() * sizeof(Mesh::Node);
  bytes += mesh.tris.size() * sizeof(Mesh::Tri);
  bytes += mesh.lines.size() * sizeof(Mesh::Line);
  return static_cast<double>(bytes) / (1024.0 * 1024.0);
}

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

static std::map<int, XsMaterial::SPtr> build_phys_xs_map(const Mesh &mesh, const XsLibrary &library)
{
  std::map<int, XsMaterial::SPtr> mapping;
  for (std::map<int, std::string>::const_iterator it = mesh.physNames.begin(); it != mesh.physNames.end(); ++it)
  {
    const int physId = it->first;
    const std::string &physName = it->second;
    if (physName.empty())
    {
      std::cerr << "[FIGYELMEZTETÉS] Fizikai csoport név nélkül (id=" << physId << "), kihagyom.\n";
      continue;
    }
    XsMaterial::SPtr material = library.find_material(physName);
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
  std::string meshPath = "vver440.msh";
  std::string xsPath = "xs_vver440.txt";
  std::string modelPath = "model.txt";
  std::string controlPath = "control.txt";

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
    else if ((std::strcmp(argv[i], "--model") == 0 || std::strcmp(argv[i], "-d") == 0) && i + 1 < argc)
    {
      modelPath = argv[++i];
    }
    else if ((std::strcmp(argv[i], "--control") == 0 || std::strcmp(argv[i], "-c") == 0) && i + 1 < argc)
    {
      controlPath = argv[++i];
    }
  }

  // Control fájl betöltése (ha nincs ilyen fájl, default értékekkel működik)
  ControlConfig control;
  loadControl(controlPath, control);

  // Mesh parsing időmérés kezdés
  auto meshStart = std::chrono::steady_clock::now();

  Mesh M;
  try
  {
    load_msh2(meshPath, M);
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

  // Mesh parsing időmérés vége
  auto meshEnd = std::chrono::steady_clock::now();
  auto meshDuration = std::chrono::duration_cast<std::chrono::milliseconds>(meshEnd - meshStart);

  // Mesh verbosity lekérdezése
  const int meshVerbosity = control.getEffectiveVerbosity(control.meshOutput);

  // HA mesh verbosity >= 1, akkor kezdünk kiírni dolgokat
  if (meshVerbosity >= 1 && meshVerbosity <= 4)
  {
    // Szeparátor
    std::cout << "[1/3] ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << "      MESH PARSING\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";

    const std::size_t nodeCount = (M.nodes.size() > 0 ? M.nodes.size() - 1 : 0);

    // Verbosity >= 1: Alapvető összefoglaló
    std::cout << "[OK] Háló beolvasva:\n";
    std::cout << "  Csomópontok: " << nodeCount << "\n";
    std::cout << "  1D elemek: " << M.lines.size() << "\n";
    std::cout << "  Háromszögek: " << M.tris.size() << "\n";

  // Verbosity >= 2 VAGY physical_groups flag: Fizikai csoportok listája
  if ((meshVerbosity >= 2 || control.meshOutput.getFlag("physical_groups")) && !M.physNames.empty())
  {
    std::cout << "  Fizikai csoportok (id → név):\n";
    for (const auto &entry : M.physNames)
    {
      std::cout << "    " << entry.first << " → " << entry.second << "\n";
    }
  }

  // Verbosity >= 2 VAGY elements_per_group flag: Háromszögek fizikai csoport szerint
  std::map<int, std::size_t> triangleCountPerPhys;
  for (const auto &tri : M.tris)
  {
    triangleCountPerPhys[tri.phys] += 1;
  }
  if ((meshVerbosity >= 2 || control.meshOutput.getFlag("elements_per_group")) && !triangleCountPerPhys.empty())
  {
    std::cout << "  Háromszög elemek fizikai csoport szerint:\n";
    for (const auto &entry : triangleCountPerPhys)
    {
      const int physId = entry.first;
      const std::size_t triCount = entry.second;
      std::cout << "    phys=" << physId << " (" << lookup_phys_name(M, physId) << ") : " << triCount << " db\n";
    }
  }

  // 1D elemek feldolgozása
  std::map<int, std::size_t> lineCountPerPhys;
  std::map<int, std::set<int>> lineNodesPerPhys;
  for (const auto &lineElem : M.lines)
  {
    lineCountPerPhys[lineElem.phys] += 1;
    lineNodesPerPhys[lineElem.phys].insert(lineElem.a);
    lineNodesPerPhys[lineElem.phys].insert(lineElem.b);
  }

  // Verbosity >= 2: 1D elemek részletei
  if (meshVerbosity >= 2)
  {
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
  }

  // Verbosity >= 3 VAGY boundary_nodes flag: Összesített boundary nodes
  if ((meshVerbosity >= 3 || control.meshOutput.getFlag("boundary_nodes")) && !lineNodesPerPhys.empty())
  {
    std::set<int> allBoundaryNodes;
    for (const auto &entry : lineNodesPerPhys)
    {
      allBoundaryNodes.insert(entry.second.begin(), entry.second.end());
    }
    std::cout << "  Összesen " << allBoundaryNodes.size() << " db egyedi csomópont kapcsolódik 1D elemekhez.\n";
  }

  // VALIDÁCIÓ 1: Fizikai csoport ellenőrzés háromszögekre
  if (control.meshOutput.getFlag("validate_physical_groups"))
  {
    int orphanCount = 0;
    std::set<int> orphanPhysIds;
    for (const auto &tri : M.tris)
    {
      if (M.physNames.find(tri.phys) == M.physNames.end())
      {
        orphanCount++;
        orphanPhysIds.insert(tri.phys);
      }
    }
    if (orphanCount > 0)
    {
      std::cout << "\n[VALIDÁCIÓS HIBA] " << orphanCount
                << " háromszög nincs definiált fizikai csoportban!\n";
      std::cout << "  Ismeretlen fizikai csoport ID-k:";
      for (int physId : orphanPhysIds)
      {
        std::cout << " " << physId;
      }
      std::cout << "\n";
    }
    else
    {
      std::cout << "\n[VALIDÁCIÓ OK] Minden háromszög fizikai csoportban van.\n";
    }
  }

  // Verbosity >= 4: Debug információk
  if (meshVerbosity >= 4)
  {
    std::cout << "\n[DEBUG] Mesh parsing részletek:\n";
    std::cout << "  Parsing idő: " << meshDuration.count() << " ms\n";
    std::cout << "  Fájl méret: " << std::fixed << std::setprecision(2) << getFileSizeMB(meshPath) << " MB\n";
    std::cout << "  Becsült memória használat: " << std::fixed << std::setprecision(2) << estimateMemoryMB(M) << " MB\n";
  }
  } // Mesh verbosity >= 1 && <= 4 vége

  // Verbosity == 5: CSAK debug információk
  if (meshVerbosity == 5)
  {
    std::cout << "[1/3] ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << "      MESH PARSING [DEBUG ONLY]\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << "[DEBUG] Mesh parsing részletek:\n";
    std::cout << "  Parsing idő: " << meshDuration.count() << " ms\n";
    std::cout << "  Fájl méret: " << std::fixed << std::setprecision(2) << getFileSizeMB(meshPath) << " MB\n";
    std::cout << "  Becsült memória használat: " << std::fixed << std::setprecision(2) << estimateMemoryMB(M) << " MB\n";
    const std::size_t nodeCount = (M.nodes.size() > 0 ? M.nodes.size() - 1 : 0);
    std::cout << "  Elemek: " << nodeCount << " nodes, " << M.tris.size() << " triangles, " << M.lines.size() << " lines\n";
  }

  // XS parsing időmérés kezdés
  auto xsStart = std::chrono::steady_clock::now();

  XsLibrary xsLibrary;
  try
  {
    load_xs(xsPath, xsLibrary);

    // XS parsing időmérés vége
    auto xsEnd = std::chrono::steady_clock::now();
    auto xsDuration = std::chrono::duration_cast<std::chrono::milliseconds>(xsEnd - xsStart);

    // XS verbosity lekérdezése
    const int xsVerbosity = control.getEffectiveVerbosity(control.xsOutput);

    // HA XS verbosity >= 1, akkor kezdünk kiírni dolgokat
    if (xsVerbosity >= 1 && xsVerbosity <= 4)
    {
      // Szeparátor
      std::cout << "\n[2/3] ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
      std::cout << "      CROSS-SECTION LIBRARY\n";
      std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";

      // Verbosity >= 1: Alapvető összefoglaló
      std::cout << "[OK] Keresztmetszet könyvtár beolvasva: " << xsLibrary.title << "\n";
      std::cout << "  Energia csoportok száma: " << xsLibrary.energyGroupCount << "\n";

    // Energia csoport nevek (verbosity >= 2)
    if (xsVerbosity >= 2 && !xsLibrary.energyGroupNames.empty())
    {
      std::cout << "  Csoportnevek:";
      for (std::size_t i = 0; i < xsLibrary.energyGroupNames.size(); ++i)
      {
        std::cout << " " << xsLibrary.energyGroupNames[i];
      }
      std::cout << "\n";
    }

    // Anyag nevek (verbosity >= 2)
    if (xsVerbosity >= 2 && !xsLibrary.materials.empty())
    {
      std::cout << "  Anyagok:";
      for (std::size_t i = 0; i < xsLibrary.materials.size(); ++i)
      {
        std::cout << " " << xsLibrary.materials[i].name;
      }
      std::cout << "\n";
    }

    // Verbosity >= 2 VAGY cross_sections flag: Keresztmetszet értékek részletesen
    if (xsVerbosity >= 2 || control.xsOutput.getFlag("cross_sections"))
    {
      for (std::size_t index = 0; index < xsLibrary.materials.size(); ++index)
      {
        const XsMaterial &mat = xsLibrary.materials[index];
        std::cout << "    [" << mat.name << "]\n";
        print_group_values("sigma_t", mat.sigma_t);
        print_group_values("sigma_a", mat.sigma_a);
        print_group_values("nu_sigma_f", mat.nu_sigma_f);
        print_group_values("chi", mat.chi);

        // Verbosity >= 3 VAGY scatter_matrix flag: Scatter mátrix
        if (xsVerbosity >= 3 || control.xsOutput.getFlag("scatter_matrix"))
        {
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
      }
    }

    // Peremfeltételek (verbosity >= 2)
    if (xsVerbosity >= 2 && !xsLibrary.boundaries.empty())
    {
      std::cout << "\n  Peremfeltételek:\n";
      for (std::size_t i = 0; i < xsLibrary.boundaries.size(); ++i)
      {
        const XsBoundary &bound = xsLibrary.boundaries[i];
        std::cout << "    [" << bound.name << "]\n";
        std::cout << "      type: " << bound.type << "\n";
      }
    }

    // Verbosity >= 2: Fizikai csoport → anyag hozzárendelés
    std::map<int, XsMaterial::SPtr> physToXs = build_phys_xs_map(M, xsLibrary);
    if (xsVerbosity >= 2 && !physToXs.empty())
    {
      std::cout << "\n[OK] Fizikai csoport → anyag hozzárendelés:\n";
      for (std::map<int, XsMaterial::SPtr>::const_iterator it = physToXs.begin(); it != physToXs.end(); ++it)
      {
        const int physId = it->first;
        const XsMaterial::SPtr &material = it->second;
        std::cout << "  phys=" << physId << " → " << material->name << "\n";
      }
    }

    // VALIDÁCIÓ 2: Anyag hozzárendelés ellenőrzés
    if (control.meshOutput.getFlag("validate_material_assignment"))
    {
      int missingCount = 0;
      std::vector<std::string> missingPhysGroups;

      for (const auto &entry : M.physNames)
      {
        int physId = entry.first;
        const std::string &physName = entry.second;

        // Csak 2D fizikai csoportokat ellenőrizzük (háromszögek)
        bool hasTriangles = false;
        for (const auto &tri : M.tris)
        {
          if (tri.phys == physId)
          {
            hasTriangles = true;
            break;
          }
        }

        if (hasTriangles && physToXs.find(physId) == physToXs.end())
        {
          missingCount++;
          missingPhysGroups.push_back(physName + " (id=" + std::to_string(physId) + ")");
        }
      }

      if (missingCount > 0)
      {
        std::cout << "\n[VALIDÁCIÓS HIBA] " << missingCount
                  << " fizikai csoportnak nincs anyag hozzárendelve!\n";
        std::cout << "  Hiányzó anyagok:\n";
        for (const auto &name : missingPhysGroups)
        {
          std::cout << "    - " << name << "\n";
        }
      }
      else
      {
        std::cout << "\n[VALIDÁCIÓ OK] Minden 2D fizikai csoportnak van anyaga.\n";
      }
    }

    // Verbosity >= 4: Debug információk
    if (xsVerbosity >= 4)
    {
      std::cout << "\n[DEBUG] XS parsing részletek:\n";
      std::cout << "  Parsing idő: " << xsDuration.count() << " ms\n";
      std::cout << "  Fájl méret: " << std::fixed << std::setprecision(2) << getFileSizeMB(xsPath) << " MB\n";
      std::cout << "  Anyagok száma: " << xsLibrary.materials.size() << "\n";
      std::cout << "  Peremfeltételek száma: " << xsLibrary.boundaries.size() << "\n";
    }
    } // XS verbosity >= 1 && <= 4 vége

    // Verbosity == 5: CSAK debug információk
    if (xsVerbosity == 5)
    {
      std::cout << "\n[2/3] ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
      std::cout << "      CROSS-SECTION LIBRARY [DEBUG ONLY]\n";
      std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
      std::cout << "[DEBUG] XS parsing részletek:\n";
      std::cout << "  Parsing idő: " << xsDuration.count() << " ms\n";
      std::cout << "  Fájl méret: " << std::fixed << std::setprecision(2) << getFileSizeMB(xsPath) << " MB\n";
      std::cout << "  Anyagok száma: " << xsLibrary.materials.size() << "\n";
      std::cout << "  Peremfeltételek száma: " << xsLibrary.boundaries.size() << "\n";
      std::cout << "  Energia csoportok száma: " << xsLibrary.energyGroupCount << "\n";
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

  // Model parsing időmérés kezdés
  auto modelStart = std::chrono::steady_clock::now();

  ModelLibrary modelLibrary;
  try
  {
    loadModel(modelPath, modelLibrary);

    // Model parsing időmérés vége
    auto modelEnd = std::chrono::steady_clock::now();
    auto modelDuration = std::chrono::duration_cast<std::chrono::milliseconds>(modelEnd - modelStart);

    // Model verbosity lekérdezése
    const int modelVerbosity = control.getEffectiveVerbosity(control.modelOutput);

    // HA Model verbosity >= 1, akkor kezdünk kiírni dolgokat
    if (modelVerbosity >= 1 && modelVerbosity <= 4)
    {
      // Szeparátor
      std::cout << "\n[3/3] ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
      std::cout << "      MODEL LIBRARY\n";
      std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";

      // Verbosity >= 1: Alapvető összefoglaló
      std::cout << "[OK] Model fájl beolvasva: " << modelLibrary.title << "\n";
      std::cout << "  Zónák száma: " << modelLibrary.zones.size() << "\n";
      std::cout << "  Peremek száma: " << modelLibrary.boundaries.size() << "\n";
      std::cout << "  Keverékek száma: " << modelLibrary.mixtures.size() << "\n";
      if (!modelLibrary.materials.empty())
      {
        std::cout << "  Zóna-anyag hozzárendelések száma: " << modelLibrary.materials.size() << "\n";
      }

    // Verbosity >= 2 VAGY zones flag: Zónák részletesen
    if ((modelVerbosity >= 2 || control.modelOutput.getFlag("zones")) && !modelLibrary.zones.empty())
    {
      for (std::size_t i = 0; i < modelLibrary.zones.size(); ++i)
      {
        const Zone &zone = modelLibrary.zones[i];
        std::cout << "    [Zone: " << zone.name << "]\n";
        std::cout << "      physical_groups (2D):";
        for (std::size_t j = 0; j < zone.physicalGroups.size(); ++j)
        {
          std::cout << " " << zone.physicalGroups[j];
        }
        std::cout << "\n";
      }
    }

    // Verbosity >= 2 VAGY boundaries flag: Peremek részletesen
    if (modelVerbosity >= 2 || control.modelOutput.getFlag("boundaries"))
    {
      for (std::size_t i = 0; i < modelLibrary.boundaries.size(); ++i)
      {
        const Boundary &boundary = modelLibrary.boundaries[i];
        std::cout << "    [Boundary: " << boundary.name << "]\n";
        std::cout << "      physical_groups (1D):";
        for (std::size_t j = 0; j < boundary.physicalGroups.size(); ++j)
        {
          std::cout << " " << boundary.physicalGroups[j];
        }
        std::cout << "\n";
      }
    }

    // Verbosity >= 2 VAGY mixtures flag: Keverékek részletesen
    if (modelVerbosity >= 2 || control.modelOutput.getFlag("mixtures"))
    {
      for (std::size_t i = 0; i < modelLibrary.mixtures.size(); ++i)
      {
        const Mixture &mixture = modelLibrary.mixtures[i];
        std::cout << "    [Mixture: " << mixture.name << "]\n";
        std::cout << "      density: " << mixture.density << " g/cm³\n";

        // Verbosity >= 3 VAGY mixture_details flag: Komponensek részletesen
        if (modelVerbosity >= 3 || control.modelOutput.getFlag("mixture_details"))
        {
          std::cout << "      components:\n";
          for (std::size_t j = 0; j < mixture.components.size(); ++j)
          {
            const MixtureComponent &comp = mixture.components[j];
            std::cout << "        " << comp.element << " = " << comp.atoms << "\n";
          }
        }
      }
    }

    // Verbosity >= 2 VAGY materials flag: Zóna-anyag hozzárendelések
    if ((modelVerbosity >= 2 || control.modelOutput.getFlag("materials")) && !modelLibrary.materials.empty())
    {
      std::cout << "\n  Zóna-anyag hozzárendelések:\n";
      for (std::size_t i = 0; i < modelLibrary.materials.size(); ++i)
      {
        const Material &mat = modelLibrary.materials[i];
        std::cout << "    " << mat.zoneName << " → " << mat.mixtureName << "\n";
      }
    }

    // VALIDÁCIÓ 3: Perem ellenőrzés
    if (control.modelOutput.getFlag("validate_boundaries"))
    {
      int missingCount = 0;
      std::vector<std::string> missingBoundaries;

      for (const auto &boundary : modelLibrary.boundaries)
      {
        bool found = false;

        // Ellenőrizzük, hogy a boundary minden fizikai csoportja létezik-e a mesh-ben
        for (const auto &physGroupName : boundary.physicalGroups)
        {
          // Keressük meg a fizikai csoport nevét a mesh-ben
          for (const auto &meshPhys : M.physNames)
          {
            if (meshPhys.second == physGroupName)
            {
              // Ellenőrizzük, hogy van-e 1D elem ezzel a fizikai ID-vel
              for (const auto &line : M.lines)
              {
                if (line.phys == meshPhys.first)
                {
                  found = true;
                  break;
                }
              }
              if (found) break;
            }
          }
          if (found) break;
        }

        if (!found)
        {
          missingCount++;
          std::string groupsList;
          for (std::size_t i = 0; i < boundary.physicalGroups.size(); ++i)
          {
            if (i > 0) groupsList += ", ";
            groupsList += boundary.physicalGroups[i];
          }
          missingBoundaries.push_back(boundary.name + " (csoportok: " + groupsList + ")");
        }
      }

      if (missingCount > 0)
      {
        std::cout << "\n[VALIDÁCIÓS HIBA] " << missingCount
                  << " definiált perem nem található a mesh-ben!\n";
        std::cout << "  Hiányzó peremek:\n";
        for (const auto &name : missingBoundaries)
        {
          std::cout << "    - " << name << "\n";
        }
      }
      else
      {
        std::cout << "\n[VALIDÁCIÓ OK] Minden definiált perem megtalálható a mesh-ben.\n";
      }
    }

    // Verbosity >= 4: Debug információk
    if (modelVerbosity >= 4)
    {
      std::cout << "\n[DEBUG] Model parsing részletek:\n";
      std::cout << "  Parsing idő: " << modelDuration.count() << " ms\n";
      std::cout << "  Fájl méret: " << std::fixed << std::setprecision(2) << getFileSizeMB(modelPath) << " MB\n";
      std::cout << "  Zónák száma: " << modelLibrary.zones.size() << "\n";
      std::cout << "  Peremek száma: " << modelLibrary.boundaries.size() << "\n";
      std::cout << "  Keverékek száma: " << modelLibrary.mixtures.size() << "\n";
      std::cout << "  Anyag hozzárendelések száma: " << modelLibrary.materials.size() << "\n";
    }
    } // Model verbosity >= 1 && <= 4 vége

    // Verbosity == 5: CSAK debug információk
    if (modelVerbosity == 5)
    {
      std::cout << "\n[3/3] ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
      std::cout << "      MODEL LIBRARY [DEBUG ONLY]\n";
      std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
      std::cout << "[DEBUG] Model parsing részletek:\n";
      std::cout << "  Parsing idő: " << modelDuration.count() << " ms\n";
      std::cout << "  Fájl méret: " << std::fixed << std::setprecision(2) << getFileSizeMB(modelPath) << " MB\n";
      std::cout << "  Zónák száma: " << modelLibrary.zones.size() << "\n";
      std::cout << "  Peremek száma: " << modelLibrary.boundaries.size() << "\n";
      std::cout << "  Keverékek száma: " << modelLibrary.mixtures.size() << "\n";
      std::cout << "  Anyag hozzárendelések száma: " << modelLibrary.materials.size() << "\n";
    }
  }
  catch (const ModelParseError &ex)
  {
    std::cerr << "Model beolvasási hiba (sor " << ex.line() << "): " << ex.what() << "\n";
    return 1;
  }
  catch (const ModelError &ex)
  {
    std::cerr << "Model beolvasási hiba: " << ex.what() << "\n";
    return 1;
  }

  // Ha bármelyik parser verbosity >= 1, akkor "Kész" üzenet szeparátorral
  if (control.getEffectiveVerbosity(control.meshOutput) >= 1 ||
      control.getEffectiveVerbosity(control.xsOutput) >= 1 ||
      control.getEffectiveVerbosity(control.modelOutput) >= 1)
  {
    std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << "✅ PARSING COMPLETE\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
  }
  return 0;
}
