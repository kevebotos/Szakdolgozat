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
  auto isws = [](unsigned char c)
  { return std::isspace(c) != 0; };
  // eleje
  auto it = std::find_if_not(s.begin(), s.end(), isws);
  s.erase(s.begin(), it);
  // vége
  while (!s.empty() && isws(static_cast<unsigned char>(s.back())))
    s.pop_back();
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

  auto throw_parse = [&](std::size_t errorLine, const std::string &message) -> void
  {
    throw MeshParseError(errorLine, message);
  };

  auto throw_current = [&](const std::string &message) -> void
  {
    throw_parse(lineNo, message);
  };

  auto read_count = [&](const char *sectionToken) -> std::size_t
  {
    std::string countLine;
    const std::size_t expectedLine = lineNo + 1;
    if (!std::getline(in, countLine))
    {
      throw_parse(expectedLine, std::string("Váratlan fájlvég a(z) ") + sectionToken + " blokk elején.");
    }
    lineNo = expectedLine;
    trim_inplace(countLine);
    if (countLine.empty())
    {
      throw_current(std::string("Hiányzó elemszám a(z) ") + sectionToken + " blokk elején.");
    }

    std::size_t count = 0;
    std::istringstream iss(countLine);
    if (!(iss >> count))
    {
      throw_current(std::string("Érvénytelen elemszám a(z) ") + sectionToken + " blokkban: \"" + countLine + "\"");
    }
    char extra = '\0';
    if (iss >> extra)
    {
      throw_current(std::string("Túl sok adat a(z) ") + sectionToken + " elemszám sorában: \"" + countLine + "\"");
    }
    return count;
  };

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
          throw_current("Új blokk indult a meglévő befejezése előtt: $PhysicalNames");
        }
        state = State::PhysicalNames;
        physToRead = read_count("$PhysicalNames");
        physRead = 0;
        continue;
      }
      if (line == "$EndPhysicalNames")
      {
        if (state != State::PhysicalNames)
        {
          throw_current("Váratlan $EndPhysicalNames.");
        }
        if (physRead != physToRead)
        {
          throw_current("A $PhysicalNames blokk sorainak száma eltér a megadottól.");
        }
        state = State::None;
        continue;
      }

      if (line == "$Nodes")
      {
        if (state != State::None)
        {
          throw_current("Új blokk indult a meglévő befejezése előtt: $Nodes");
        }
        state = State::Nodes;
        nodesToRead = read_count("$Nodes");
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
          throw_current("Váratlan $EndNodes.");
        }
        if (nodesRead != nodesToRead)
        {
          throw_current("A $Nodes blokkban a megadott elemszám nem egyezik a beolvasott sorok számával.");
        }
        for (std::size_t id = 1; id < nodeSeen.size(); ++id)
        {
          if (!nodeSeen[id])
          {
            throw_current("Hiányzó csomópont azonosító: " + std::to_string(id));
          }
        }
        state = State::None;
        continue;
      }

      if (line == "$Elements")
      {
        if (state != State::None)
        {
          throw_current("Új blokk indult a meglévő befejezése előtt: $Elements");
        }
        state = State::Elements;
        elemsToRead = read_count("$Elements");
        result.tris.clear();
        result.tris.reserve(elemsToRead);
        elemsRead = 0;
        continue;
      }
      if (line == "$EndElements")
      {
        if (state != State::Elements)
        {
          throw_current("Váratlan $EndElements.");
        }
        if (elemsRead != elemsToRead)
        {
          throw_current("A $Elements blokk sorainak száma eltér a megadottól.");
        }
        state = State::None;
        continue;
      }

      if (state != State::None)
      {
        throw_current("Ismeretlen blokk kezdete egy másik blokk lezárása előtt: " + line);
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
        throw_current("Ismeretlen blokk lezárása hiányzik: " + endToken);
      }
      continue;
    }

    switch (state)
    {
    case State::PhysicalNames:
    {
      if (physRead >= physToRead)
      {
        throw_current("Túl sok sor a $PhysicalNames blokkban.");
      }
      std::istringstream iss(line);
      int dim = 0;
      int id = -1;
      if (!(iss >> dim >> id))
      {
        throw_current("Érvénytelen sor a $PhysicalNames blokkban: \"" + line + "\"");
      }
      if (id < 0)
      {
        throw_current("Negatív fizikai azonosító: " + std::to_string(id));
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
        throw_current("Duplikált fizikai azonosító: " + std::to_string(id));
      }
      ++physRead;
      break;
    }
    case State::Nodes:
    {
      if (nodesRead >= nodesToRead)
      {
        throw_current("Túl sok sor a $Nodes blokkban.");
      }
      std::istringstream iss(line);
      int id = 0;
      double x = 0.0, y = 0.0, z = 0.0;
      if (!(iss >> id >> x >> y >> z))
      {
        throw_current("Érvénytelen csomópont sor: \"" + line + "\"");
      }
      if (id <= 0 || static_cast<std::size_t>(id) >= result.nodes.size())
      {
        throw_current("Csomópont azonosító tartományon kívül: " + std::to_string(id));
      }
      if (nodeSeen[static_cast<std::size_t>(id)])
      {
        throw_current("Duplikált csomópont azonosító: " + std::to_string(id));
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
        throw_current("Túl sok sor a $Elements blokkban.");
      }
      std::istringstream iss(line);
      int eid = 0;
      int etype = 0;
      int ntags = 0;
      if (!(iss >> eid >> etype >> ntags))
      {
        throw_current("Érvénytelen elem sor: \"" + line + "\"");
      }
      if (ntags < 0)
      {
        throw_current("Negatív tag szám az elem sorban: " + std::to_string(ntags));
      }
      std::vector<int> tags;
      tags.reserve(static_cast<std::size_t>(ntags));
      for (int t = 0; t < ntags; ++t)
      {
        int tagValue = 0;
        if (!(iss >> tagValue))
        {
          throw_current("Nem olvasható ki a(z) " + std::to_string(t + 1) + ". tag értéke.");
        }
        tags.push_back(tagValue);
      }

      if (etype == 2)
      {
        int n1 = 0, n2 = 0, n3 = 0;
        if (!(iss >> n1 >> n2 >> n3))
        {
          throw_current("Háromszög elemhez hiányoznak a csomópont azonosítók.");
        }
        auto checkNodeId = [&](int nodeId)
        {
          if (nodeId <= 0 || static_cast<std::size_t>(nodeId) >= result.nodes.size())
          {
            throw_current("Háromszög elem érvénytelen csomópont azonosítóval: " + std::to_string(nodeId));
          }
        };
        checkNodeId(n1);
        checkNodeId(n2);
        checkNodeId(n3);
        const int phys = (ntags >= 1 ? tags[0] : -1);
        result.tris.push_back({n1, n2, n3, phys});
      }
      ++elemsRead;
      break;
    }
    case State::None:
      throw_current("Adatsor érkezett aktív blokk nélkül.");
    }
  }

  if (state != State::None)
  {
    throw_current("A fájl vége előtt nem zárult le a blokk.");
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
