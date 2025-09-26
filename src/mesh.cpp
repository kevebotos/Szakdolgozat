#include "mesh.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <vector>
#include <limits>
#include <algorithm>
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

// Egyszerű, kezdőbarát segédfüggvények a dobásokhoz és számlálósor olvasásához
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
      throw_at_line(lineNo, "Háromszög elem érvénytelen csomópont azonosítóval: " + std::to_string(nodeId));
    }
  }
}

void load_msh2(const std::string &path, Mesh &mesh, std::ostream &log)
{
  std::ifstream in(path);
  if (!in)
  {
    throw MeshError("Nem tudtam megnyitni a hálófájlt: " + path);
  }

  Mesh result;

  enum class State
  {
    None,
    PhysicalNames,
    Nodes,
    Elements
  };
  State state = State::None;

  std::string line;
  std::size_t lineNo = 0;

  std::size_t physToRead = 0, physRead = 0;
  std::size_t nodesToRead = 0, nodesRead = 0;
  std::size_t elemsToRead = 0, elemsRead = 0;
  std::vector<bool> nodeSeen;

  // Lambdák helyett egyszerű segédfüggvényeket használunk (lásd fentebb)

  while (std::getline(in, line))
  {
    ++lineNo;
    trim_inplace(line);
    if (line.empty())
      continue;

    if (line[0] == '$')
    {
      if (line == "$PhysicalNames")
      {
        if (state != State::None)
        {
          throw_at_line(lineNo, "Új blokk indult a meglévő befejezése előtt: $PhysicalNames");
        }
        state = State::PhysicalNames;
        physToRead = read_count(in, lineNo, "$PhysicalNames");
        physRead = 0;
        continue;
      }
      if (line == "$EndPhysicalNames")
      {
        if (state != State::PhysicalNames)
        {
          throw_at_line(lineNo, "Váratlan $EndPhysicalNames.");
        }
        if (physRead != physToRead)
        {
          throw_at_line(lineNo, "A $PhysicalNames blokk sorainak száma eltér a megadottól.");
        }
        state = State::None;
        continue;
      }

      if (line == "$Nodes")
      {
        if (state != State::None)
        {
          throw_at_line(lineNo, "Új blokk indult a meglévő befejezése előtt: $Nodes");
        }
        state = State::Nodes;
        nodesToRead = read_count(in, lineNo, "$Nodes");
        result.nodes.assign(nodesToRead + 1, Mesh::Node{});
        nodeSeen.assign(nodesToRead + 1, false);
        result.minx = std::numeric_limits<double>::infinity();
        result.miny = std::numeric_limits<double>::infinity();
        result.maxx = -std::numeric_limits<double>::infinity();
        result.maxy = -std::numeric_limits<double>::infinity();
        nodesRead = 0;
        continue;
      }
      if (line == "$EndNodes")
      {
        if (state != State::Nodes)
        {
          throw_at_line(lineNo, "Váratlan $EndNodes.");
        }
        if (nodesRead != nodesToRead)
        {
          throw_at_line(lineNo, "A $Nodes blokkban a megadott elemszám nem egyezik a beolvasott sorok számával.");
        }
        for (std::size_t id = 1; id < nodeSeen.size(); ++id)
        {
          if (!nodeSeen[id])
          {
            throw_at_line(lineNo, "Hiányzó csomópont azonosító: " + std::to_string(id));
          }
        }
        state = State::None;
        continue;
      }

      if (line == "$Elements")
      {
        if (state != State::None)
        {
          throw_at_line(lineNo, "Új blokk indult a meglévő befejezése előtt: $Elements");
        }
        state = State::Elements;
        elemsToRead = read_count(in, lineNo, "$Elements");
        result.tris.clear();
        result.tris.reserve(elemsToRead);
        elemsRead = 0;
        continue;
      }
      if (line == "$EndElements")
      {
        if (state != State::Elements)
        {
          throw_at_line(lineNo, "Váratlan $EndElements.");
        }
        if (elemsRead != elemsToRead)
        {
          throw_at_line(lineNo, "A $Elements blokk sorainak száma eltér a megadottól.");
        }
        state = State::None;
        continue;
      }

      if (state != State::None)
      {
        throw_at_line(lineNo, "Ismeretlen blokk kezdete egy másik blokk lezárása előtt: " + line);
      }

      const std::string sectionName = line.substr(1);
      const std::string endToken = "$End" + sectionName;
      bool foundEnd = false;
      while (std::getline(in, line))
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

    switch (state)
    {
    case State::PhysicalNames:
    {
      if (physRead >= physToRead)
      {
        throw_at_line(lineNo, "Túl sok sor a $PhysicalNames blokkban.");
      }
      std::istringstream iss(line);
      int dim = 0;
      int id = -1;
      if (!(iss >> dim >> id))
      {
        throw_at_line(lineNo, "Érvénytelen sor a $PhysicalNames blokkban: \"" + line + "\"");
      }
      if (id < 0)
      {
        throw_at_line(lineNo, "Negatív fizikai azonosító: " + std::to_string(id));
      }
      std::string name;
      auto q1 = line.find('"');
      auto q2 = line.rfind('"');
      if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1)
      {
        name = line.substr(q1 + 1, q2 - q1 - 1);
      }
      else
      {
        name.clear();
      }
      auto inserted = result.physNames.emplace(id, name);
      if (!inserted.second)
      {
        throw_at_line(lineNo, "Duplikált fizikai azonosító: " + std::to_string(id));
      }
      ++physRead;
      break;
    }
    case State::Nodes:
    {
      if (nodesRead >= nodesToRead)
      {
        throw_at_line(lineNo, "Túl sok sor a $Nodes blokkban.");
      }
      std::istringstream iss(line);
      int id = 0;
      double x = 0.0, y = 0.0, z = 0.0;
      if (!(iss >> id >> x >> y >> z))
      {
        throw_at_line(lineNo, "Érvénytelen csomópont sor: \"" + line + "\"");
      }
      if (id <= 0 || static_cast<std::size_t>(id) >= result.nodes.size())
      {
        throw_at_line(lineNo, "Csomópont azonosító tartományon kívül: " + std::to_string(id));
      }
      if (nodeSeen[static_cast<std::size_t>(id)])
      {
        throw_at_line(lineNo, "Duplikált csomópont azonosító: " + std::to_string(id));
      }
      nodeSeen[static_cast<std::size_t>(id)] = true;
      result.nodes[static_cast<std::size_t>(id)].x = x;
      result.nodes[static_cast<std::size_t>(id)].y = y;
      result.minx = std::min(result.minx, x);
      result.miny = std::min(result.miny, y);
      result.maxx = std::max(result.maxx, x);
      result.maxy = std::max(result.maxy, y);
      ++nodesRead;
      break;
    }
    case State::Elements:
    {
      if (elemsRead >= elemsToRead)
      {
        throw_at_line(lineNo, "Túl sok sor a $Elements blokkban.");
      }
      std::istringstream iss(line);
      int eid = 0;
      int etype = 0;
      int ntags = 0;
      if (!(iss >> eid >> etype >> ntags))
      {
        throw_at_line(lineNo, "Érvénytelen elem sor: \"" + line + "\"");
      }
      if (ntags < 0)
      {
        throw_at_line(lineNo, "Negatív tag szám az elem sorban: " + std::to_string(ntags));
      }
      std::vector<int> tags;
      tags.reserve(static_cast<std::size_t>(ntags));
      for (int t = 0; t < ntags; ++t)
      {
        int tagValue = 0;
        if (!(iss >> tagValue))
        {
          throw_at_line(lineNo, "Nem olvasható ki a(z) " + std::to_string(t + 1) + ". tag értéke.");
        }
        tags.push_back(tagValue);
      }

      if (etype == 2)
      {
        int n1 = 0, n2 = 0, n3 = 0;
        if (!(iss >> n1 >> n2 >> n3))
        {
          throw_at_line(lineNo, "Háromszög elemhez hiányoznak a csomópont azonosítók.");
        }
        check_node_id_or_throw(n1, result, lineNo);
        check_node_id_or_throw(n2, result, lineNo);
        check_node_id_or_throw(n3, result, lineNo);
        const int phys = (ntags >= 1 ? tags[0] : -1);
        result.tris.push_back({n1, n2, n3, phys});
      }
      ++elemsRead;
      break;
    }
    case State::None:
      throw_at_line(lineNo, "Adatsor érkezett aktív blokk nélkül.");
    }
  }

  if (state != State::None)
  {
    throw_at_line(lineNo, "A fájl vége előtt nem zárult le a blokk.");
  }

  mesh = std::move(result);

  if (nodesRead == 0)
  {
    log << "[FIGYELEM] 0 csomópontot olvastam.\n";
  }
  if (mesh.tris.empty())
  {
    log << "[FIGYELEM] Nem találtam háromszög elemeket (etype=2).\n";
  }
}
