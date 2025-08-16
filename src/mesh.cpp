#include "mesh.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <vector>
#include <limits>
#include <algorithm>
#include <cctype>

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

bool load_msh2(const std::string &path, Mesh &mesh, std::ostream &log)
{
  std::ifstream in(path);
  if (!in)
  {
    log << "[HIBA] Nem tudtam megnyitni: " << path << "\n";
    return false;
  }

  mesh = Mesh{}; // reset

  enum class State
  {
    None,
    PhysicalNames,
    Nodes,
    Elements
  };
  State state = State::None;

  std::string line;
  std::size_t physToRead = 0, physRead = 0;
  std::size_t nodesToRead = 0, nodesRead = 0;
  std::size_t elemsToRead = 0, elemsRead = 0;

  while (std::getline(in, line))
  {
    trim_inplace(line); // fontos: CRLF és extra space-ek kezelése
    if (line.empty())
      continue;

    if (line[0] == '$')
    {
      if (line == "$PhysicalNames")
      {
        state = State::PhysicalNames;
        if (!std::getline(in, line))
          break; // darabszám sor
        trim_inplace(line);
        std::istringstream iss(line);
        iss >> physToRead;
        physRead = 0;
        continue;
      }
      if (line == "$EndPhysicalNames")
      {
        state = State::None;
        continue;
      }

      if (line == "$Nodes")
      {
        state = State::Nodes;
        if (!std::getline(in, line))
          break; // darabszám sor
        trim_inplace(line);
        std::istringstream iss(line);
        iss >> nodesToRead;
        mesh.nodes.assign(nodesToRead + 1, Mesh::Node{}); // 1-based
        nodesRead = 0;
        mesh.minx = std::numeric_limits<double>::infinity();
        mesh.miny = std::numeric_limits<double>::infinity();
        mesh.maxx = -std::numeric_limits<double>::infinity();
        mesh.maxy = -std::numeric_limits<double>::infinity();
        continue;
      }
      if (line == "$EndNodes")
      {
        state = State::None;
        continue;
      }

      if (line == "$Elements")
      {
        state = State::Elements;
        if (!std::getline(in, line))
          break; // darabszám sor
        trim_inplace(line);
        std::istringstream iss(line);
        iss >> elemsToRead;
        mesh.tris.clear();
        mesh.tris.reserve(elemsToRead);
        elemsRead = 0;
        continue;
      }
      if (line == "$EndElements")
      {
        state = State::None;
        continue;
      }

      // egyéb szekciók most nem kellenek
      continue;
    }

    // adatsorok
    if (state == State::PhysicalNames && physRead < physToRead)
    {
      // formátum: <dim> <physId> "Name"
      std::istringstream iss(line);
      int dim = 0, id = -1;
      (void)dim;
      iss >> dim >> id;
      std::string name;
      auto q1 = line.find('"');
      auto q2 = line.rfind('"');
      if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1)
      {
        name = line.substr(q1 + 1, q2 - q1 - 1);
      }
      mesh.physNames[id] = name;
      ++physRead;
      continue;
    }

    if (state == State::Nodes && nodesRead < nodesToRead)
    {
      // formátum: id x y z
      std::istringstream iss(line);
      int id = 0;
      double x = 0, y = 0, z = 0;
      if (iss >> id >> x >> y >> z)
      {
        if (id >= 0 && static_cast<std::size_t>(id) < mesh.nodes.size())
        {
          mesh.nodes[id].x = x;
          mesh.nodes[id].y = y;
          mesh.minx = std::min(mesh.minx, x);
          mesh.miny = std::min(mesh.miny, y);
          mesh.maxx = std::max(mesh.maxx, x);
          mesh.maxy = std::max(mesh.maxy, y);
        }
        ++nodesRead;
      }
      continue;
    }

    if (state == State::Elements && elemsRead < elemsToRead)
    {
      // formátum: eid etype ntags [tags...] n1 n2 n3 ...
      std::istringstream iss(line);
      int eid = 0, etype = 0, ntags = 0;
      if (!(iss >> eid >> etype >> ntags))
      {
        ++elemsRead;
        continue;
      }

      std::vector<int> tags;
      tags.reserve(ntags);
      for (int t = 0; t < ntags; ++t)
      {
        int v = 0;
        iss >> v;
        tags.push_back(v);
      }

      if (etype == 2)
      { // 3-csúcsú háromszög
        int n1 = 0, n2 = 0, n3 = 0;
        iss >> n1 >> n2 >> n3;
        int phys = (ntags >= 1 ? tags[0] : -1); // 1. tag: physical entity id
        mesh.tris.push_back({n1, n2, n3, phys});
      }

      ++elemsRead;
      continue;
    }
  }

  if (nodesRead == 0)
  {
    log << "[FIGYELEM] 0 csomópontot olvastam. (CRLF/whitespace gond volt? A trim mostantól kezeli.)\n";
  }
  if (mesh.tris.empty())
  {
    log << "[FIGYELEM] Nem találtam háromszög elemeket (etype=2).\n";
  }

  return true;
}