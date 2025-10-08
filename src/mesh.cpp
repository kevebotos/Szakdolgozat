#include "mesh.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>
#include <utility>

// levágja az elejéről/végéről a whitespace-et (CRLF esetén a '\r'-t is)
static inline void trim_inplace(std::string &s)
{
  // Távolítsuk el az elejéről a whitespace karaktereket
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
  {
    s.erase(s.begin());
  }
  // Távolítsuk el a végéről a whitespace karaktereket (CRLF esetén a '\r'-t is)
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
  {
    s.pop_back();
  }
}

namespace
{
  [[noreturn]] void throw_at_line(std::size_t currentLine, const std::string &message)
  {
    throw MeshParseError(currentLine, message);
  }

  std::size_t read_count(std::istream &in, std::size_t &lineNo, const char *sectionToken)
  {
    std::string countLine;
    const std::size_t expectedLine = lineNo + 1;
    if (!std::getline(in, countLine))
    {
      throw_at_line(expectedLine, std::string("Váratlan fájlvég a(z) ") + sectionToken + " blokk elején.");
    }
    lineNo = expectedLine;
    trim_inplace(countLine);
    if (countLine.empty())
    {
      throw_at_line(lineNo, std::string("Hiányzó elemszám a(z) ") + sectionToken + " blokk elején.");
    }

    std::size_t count = 0;
    std::istringstream iss(countLine);
    if (!(iss >> count))
    {
      throw_at_line(lineNo, std::string("Érvénytelen elemszám a(z) ") + sectionToken + " blokkban: \"" + countLine + "\"");
    }
    char extra = '\0';
    if (iss >> extra)
    {
      throw_at_line(lineNo, std::string("Túl sok adat a(z) ") + sectionToken + " elemszám sorában: \"" + countLine + "\"");
    }
    return count;
  }

  void check_node_id_or_throw(int nodeId, const Mesh &mesh, std::size_t lineNo)
  {
    if (nodeId <= 0 || static_cast<std::size_t>(nodeId) >= mesh.nodes.size())
    {
      throw_at_line(lineNo, "Elem érvénytelen csomópont azonosítóval: " + std::to_string(nodeId));
    }
  }
}

void load_msh2(const std::string &path, Mesh &mesh)
{
  std::ifstream input(path);
  if (!input)
  {
    throw MeshError("Nem tudtam megnyitni a hálófájlt: " + path);
  }

  Mesh fresh;                 // Ide töltjük be a kész hálót
  std::string line;           // Az aktuálisan olvasott sor
  std::size_t lineNo = 0;     // Hibaüzenethez: hanyadik sorban járunk
  std::vector<bool> nodeSeen; // Segít figyelni, hogy minden csomópont ID egyszer szerepeljen

  while (std::getline(input, line))
  {
    ++lineNo;
    trim_inplace(line);
    if (line.empty())
    {
      continue; // Üres sor: lépjünk tovább
    }

    // --- 1) PhysicalNames ---
    if (line == "$PhysicalNames")
    {
      const std::size_t physCount = read_count(input, lineNo, "$PhysicalNames");
      for (std::size_t i = 0; i < physCount; ++i)
      {
        if (!std::getline(input, line))
        {
          throw_at_line(lineNo + 1, "$PhysicalNames blokk vége előtt elfogyott a fájl.");
        }
        ++lineNo;
        trim_inplace(line);
        if (line.empty())
        {
          throw_at_line(lineNo, "$PhysicalNames sor üres.");
        }

        std::istringstream iss(line);
        int dimension = 0;
        int physId = -1;
        if (!(iss >> dimension >> physId))
        {
          throw_at_line(lineNo, "Nem tudom kiolvasni a fizikai azonosítót ebből a sorból: \"" + line + "\"");
        }
        if (physId < 0)
        {
          throw_at_line(lineNo, "A fizikai azonosító nem lehet negatív: " + std::to_string(physId));
        }

        // A név idézőjelben van (pl. "Fuel"). Ha nincs idézőjel, marad üres string.
        std::string name;
        std::size_t firstQuote = line.find('"');
        std::size_t secondQuote = line.rfind('"');
        if (firstQuote != std::string::npos && secondQuote != std::string::npos && secondQuote > firstQuote)
        {
          name = line.substr(firstQuote + 1, secondQuote - firstQuote - 1);
        }

        if (fresh.physNames.count(physId) != 0)
        {
          throw_at_line(lineNo, "Ez a fizikai azonosító már szerepelt: " + std::to_string(physId));
        }
        fresh.physNames[physId] = name;
      }

      // Blokk lezárása kötelező: $EndPhysicalNames
      if (!std::getline(input, line))
      {
        throw_at_line(lineNo + 1, "Hiányzik a $EndPhysicalNames sor.");
      }
      ++lineNo;
      trim_inplace(line);
      if (line != "$EndPhysicalNames")
      {
        throw_at_line(lineNo, "A $PhysicalNames blokkot $EndPhysicalNames sorral kell zárni.");
      }
      continue;
    }

    // --- 2) Csomópontok ---
    if (line == "$Nodes")
    {
      const std::size_t nodeCount = read_count(input, lineNo, "$Nodes");

      // A msh v2 fájl 1-alapú azonosítókat használ, ezért +1 elemet foglalunk.
      fresh.nodes.assign(nodeCount + 1, Mesh::Node{});
      nodeSeen.assign(nodeCount + 1, false);

      for (std::size_t i = 0; i < nodeCount; ++i)
      {
        if (!std::getline(input, line))
        {
          throw_at_line(lineNo + 1, "$Nodes blokk közben elfogyott a fájl.");
        }
        ++lineNo;
        trim_inplace(line);
        if (line.empty())
        {
          throw_at_line(lineNo, "Üres sor a $Nodes blokkban.");
        }

        std::istringstream iss(line);
        int nodeId = 0;
        double x = 0.0;
        double y = 0.0;
        double z = 0.0; // a VVER háló 2D, de a formátum igényli a harmadik számot is
        if (!(iss >> nodeId >> x >> y >> z))
        {
          throw_at_line(lineNo, "Nem tudom kiolvasni a csomópont adatait ebből a sorból: \"" + line + "\"");
        }
        if (nodeId <= 0 || static_cast<std::size_t>(nodeId) >= fresh.nodes.size())
        {
          throw_at_line(lineNo, "A csomópont azonosító kívül esik a megengedett tartományon: " + std::to_string(nodeId));
        }
        if (nodeSeen[static_cast<std::size_t>(nodeId)])
        {
          throw_at_line(lineNo, "Csomópont azonosító ismétlődik: " + std::to_string(nodeId));
        }

        nodeSeen[static_cast<std::size_t>(nodeId)] = true;
        fresh.nodes[static_cast<std::size_t>(nodeId)].x = x;
        fresh.nodes[static_cast<std::size_t>(nodeId)].y = y;
      }

      // Ha hiányzik bármelyik azonosító, az hiba.
      for (std::size_t id = 1; id < nodeSeen.size(); ++id)
      {
        if (!nodeSeen[id])
        {
          throw_at_line(lineNo, "Hiányzik ez a csomópont azonosító: " + std::to_string(id));
        }
      }

      if (!std::getline(input, line))
      {
        throw_at_line(lineNo + 1, "Hiányzik a $EndNodes sor.");
      }
      ++lineNo;
      trim_inplace(line);
      if (line != "$EndNodes")
      {
        throw_at_line(lineNo, "A $Nodes blokkot $EndNodes sorral kell lezárni.");
      }
      continue;
    }

    // --- 3) Elemek (1D + 2D) ---
    if (line == "$Elements")
    {
      const std::size_t elementCount = read_count(input, lineNo, "$Elements");

      fresh.lines.clear();
      fresh.tris.clear();
      fresh.lines.reserve(elementCount);
      fresh.tris.reserve(elementCount);

      std::size_t skipped = 0; // Ha más típus is felbukkan, ezt jelezzük majd logban

      for (std::size_t i = 0; i < elementCount; ++i)
      {
        if (!std::getline(input, line))
        {
          throw_at_line(lineNo + 1, "$Elements blokk közben elfogyott a fájl.");
        }
        ++lineNo;
        trim_inplace(line);
        if (line.empty())
        {
          throw_at_line(lineNo, "Üres sor a $Elements blokkban.");
        }

        std::istringstream iss(line);
        int elemId = 0;
        int elemType = 0;
        int tagCount = 0;
        if (!(iss >> elemId >> elemType >> tagCount))
        {
          throw_at_line(lineNo, "Nem tudom kiolvasni az elem fejléct ebből a sorból: \"" + line + "\"");
        }
        if (tagCount < 0)
        {
          throw_at_line(lineNo, "A tag darabszám nem lehet negatív: " + std::to_string(tagCount));
        }

        int physicalId = -1;
        for (int t = 0; t < tagCount; ++t)
        {
          int tagValue = 0;
          if (!(iss >> tagValue))
          {
            throw_at_line(lineNo, "Nem tudom beolvasni a(z) " + std::to_string(t + 1) + ". taget az elem sorában.");
          }
          if (t == 0)
          {
            physicalId = tagValue; // Az első tag a fizikai azonosító
          }
        }

        if (elemType == 1)
        {
          int n1 = 0;
          int n2 = 0;
          if (!(iss >> n1 >> n2))
          {
            throw_at_line(lineNo, "A vonal elemhez két csomópont azonosítót várok.");
          }
          check_node_id_or_throw(n1, fresh, lineNo);
          check_node_id_or_throw(n2, fresh, lineNo);

          Mesh::Line edge;
          edge.a = n1;
          edge.b = n2;
          edge.phys = physicalId;
          fresh.lines.push_back(edge);
        }
        else if (elemType == 2)
        {
          int n1 = 0;
          int n2 = 0;
          int n3 = 0;
          if (!(iss >> n1 >> n2 >> n3))
          {
            throw_at_line(lineNo, "A háromszög elemhez három csomópont azonosítót várok.");
          }
          check_node_id_or_throw(n1, fresh, lineNo);
          check_node_id_or_throw(n2, fresh, lineNo);
          check_node_id_or_throw(n3, fresh, lineNo);

          Mesh::Tri tri;
          tri.a = n1;
          tri.b = n2;
          tri.c = n3;
          tri.phys = physicalId;
          fresh.tris.push_back(tri);
        }
        else
        {
          // Más elem-típus (pl. 3D elem). Egyszerűen átugorjuk őket.
          int unused = 0;
          while (iss >> unused)
          {
            // csak kiolvassuk a hátralévő számokat
          }
          ++skipped;
        }
      }

      if (!std::getline(input, line))
      {
        throw_at_line(lineNo + 1, "Hiányzik a $EndElements sor.");
      }
      ++lineNo;
      trim_inplace(line);
      if (line != "$EndElements")
      {
        throw_at_line(lineNo, "A $Elements blokkot $EndElements sorral kell lezárni.");
      }

      // Kihagyott elemek figyelmen kívül hagyása (nincs kiírás)
      continue;
    }

    // --- 4) Egyéb blokkok: csak olvassuk át őket ---
    if (!line.empty() && line[0] == '$')
    {
      const std::string sectionName = line.substr(1);
      const std::string endToken = "$End" + sectionName;
      bool foundEnd = false;

      while (std::getline(input, line))
      {
        ++lineNo;
        trim_inplace(line);
        if (line == endToken)
        {
          foundEnd = true;
          break;
        }
      }

      if (!foundEnd)
      {
        throw_at_line(lineNo, "Ismeretlen blokk lezárása hiányzik: " + endToken);
      }
      continue;
    }

    // Ha idáig eljutunk, a sor tartalma számunkra érthetetlen.
    throw_at_line(lineNo, "Nem ismert adat szerepel a fájlban: " + line);
  }

  mesh = std::move(fresh);
  // Kimenet most a main.cpp-ben van kontrollálva a control.txt alapján
}
