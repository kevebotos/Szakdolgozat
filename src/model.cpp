#include "model.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>

static inline void trim_inplace(std::string &s)
{
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
  {
    s.erase(s.begin());
  }
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
  {
    s.pop_back();
  }
}

static inline std::string strip_comment(const std::string &line)
{
  std::string result = line;
  const std::size_t hashPos = result.find('#');
  if (hashPos != std::string::npos)
  {
    result.erase(hashPos);
  }
  return result;
}

namespace
{
  [[noreturn]] void throw_at_line(std::size_t currentLine, const std::string &message)
  {
    throw ModelParseError(currentLine, message);
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
    countLine = strip_comment(countLine);
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
}

const Zone *ModelLibrary::findZone(const std::string &name) const // randge based még mindig nem elég. std::find_if vagy std::ranges::find!!!!!!
{
  for (const Zone &zone : zones)
  {
    if (zone.name == name)
    {
      return &zone;
    }
  }
  return nullptr;
}

const Boundary *ModelLibrary::findBoundary(const std::string &name) const
{
  for (std::size_t i = 0; i < boundaries.size(); ++i)
  {
    if (boundaries[i].name == name)
    {
      return &boundaries[i];
    }
  }
  return nullptr;
}

const Mixture *ModelLibrary::findMixture(const std::string &name) const
{
  for (std::size_t i = 0; i < mixtures.size(); ++i)
  {
    if (mixtures[i].name == name)
    {
      return &mixtures[i];
    }
  }
  return nullptr;
}

void loadModel(const std::string &path, ModelLibrary &model)
{
  std::ifstream input(path);
  if (!input)
  {
    throw ModelError("Nem sikerült megnyitni a model fájlt: " + path);
  }

  ModelLibrary fresh;     // Ide töltjük be a kész modelt
  std::string line;       // Az aktuálisan olvasott sor
  std::size_t lineNo = 0; // Hibaüzenethez: hanyadik sorban járunk

  while (std::getline(input, line))
  {
    ++lineNo;
    std::string cleaned = strip_comment(line);
    trim_inplace(cleaned);
    if (cleaned.empty())
    {
      continue; // Üres sor vagy csak komment: lépjünk tovább
    }

    // --- 1) ModelInfo ---
    if (cleaned == "$ModelInfo")
    {
      if (!std::getline(input, line))
      {
        throw_at_line(lineNo + 1, "$ModelInfo blokk vége előtt elfogyott a fájl.");
      }
      ++lineNo;
      std::string titleLine = strip_comment(line);
      trim_inplace(titleLine);
      fresh.title = titleLine;

      // Blokk lezárása kötelező: $EndModelInfo
      if (!std::getline(input, line))
      {
        throw_at_line(lineNo + 1, "Hiányzik a $EndModelInfo sor.");
      }
      ++lineNo;
      std::string endLine = strip_comment(line);
      trim_inplace(endLine);
      if (endLine != "$EndModelInfo")
      {
        throw_at_line(lineNo, "A $ModelInfo blokkot $EndModelInfo sorral kell zárni.");
      }
      continue;
    }

    // --- 2) Zones ---
    if (cleaned == "$Zones")
    {
      const std::size_t zoneCount = read_count(input, lineNo, "$Zones");
      for (std::size_t i = 0; i < zoneCount; ++i)
      {
        if (!std::getline(input, line))
        {
          throw_at_line(lineNo + 1, "$Zones blokk vége előtt elfogyott a fájl.");
        }
        ++lineNo;
        std::string zoneLine = strip_comment(line);
        trim_inplace(zoneLine);
        if (zoneLine.empty())
        {
          throw_at_line(lineNo, "$Zones sor üres.");
        }

        // Formátum: ZoneName physGroupCount physGroup1 physGroup2 ...
        std::istringstream iss(zoneLine);
        std::string zoneName;
        int physGroupCount = 0;

        if (!(iss >> zoneName >> physGroupCount))
        {
          throw_at_line(lineNo, "Nem tudom kiolvasni a zone nevet és physGroupCount-ot ebből a sorból: \"" + zoneLine + "\"");
        }
        if (physGroupCount <= 0)
        {
          throw_at_line(lineNo, "A physGroupCount-nak pozitívnak kell lennie: " + std::to_string(physGroupCount));
        }

        // Physical group nevek beolvasása
        std::vector<std::string> physGroups;
        for (int pg = 0; pg < physGroupCount; ++pg)
        {
          std::string physGroup;
          if (!(iss >> physGroup))
          {
            throw_at_line(lineNo, "Hiányzik a(z) " + std::to_string(pg + 1) + ". physical group név.");
          }
          physGroups.push_back(physGroup);
        }

        // Ellenőrizzük, hogy nincs-e extra adat
        std::string extra;
        if (iss >> extra)
        {
          throw_at_line(lineNo, "Túl sok adat a zone sorban: \"" + zoneLine + "\"");
        }

        // Ellenőrizzük, hogy nincs-e duplikált zone név
        if (fresh.findZone(zoneName) != nullptr)
        {
          throw_at_line(lineNo, "Ez a zone név már szerepelt: " + zoneName);
        }

        Zone zone;
        zone.name = zoneName;
        zone.physicalGroups = physGroups;
        fresh.zones.push_back(zone);
      }

      // Blokk lezárása kötelező: $EndZones
      if (!std::getline(input, line))
      {
        throw_at_line(lineNo + 1, "Hiányzik a $EndZones sor.");
      }
      ++lineNo;
      std::string endLine = strip_comment(line);
      trim_inplace(endLine);
      if (endLine != "$EndZones")
      {
        throw_at_line(lineNo, "A $Zones blokkot $EndZones sorral kell zárni.");
      }
      continue;
    }

    // --- 3) Boundaries ---
    if (cleaned == "$Boundaries")
    {
      const std::size_t boundaryCount = read_count(input, lineNo, "$Boundaries");
      for (std::size_t i = 0; i < boundaryCount; ++i)
      {
        if (!std::getline(input, line))
        {
          throw_at_line(lineNo + 1, "$Boundaries blokk vége előtt elfogyott a fájl.");
        }
        ++lineNo;
        std::string boundLine = strip_comment(line);
        trim_inplace(boundLine);
        if (boundLine.empty())
        {
          throw_at_line(lineNo, "$Boundaries sor üres.");
        }

        // Formátum: BoundaryName physGroupCount physGroup1 physGroup2 ...
        std::istringstream iss(boundLine);
        std::string boundName;
        int physGroupCount = 0;

        if (!(iss >> boundName >> physGroupCount))
        {
          throw_at_line(lineNo, "Nem tudom kiolvasni a boundary nevet és physGroupCount-ot ebből a sorból: \"" + boundLine + "\"");
        }
        if (physGroupCount <= 0)
        {
          throw_at_line(lineNo, "A physGroupCount-nak pozitívnak kell lennie: " + std::to_string(physGroupCount));
        }

        // Physical group nevek beolvasása
        std::vector<std::string> physGroups;
        for (int pg = 0; pg < physGroupCount; ++pg)
        {
          std::string physGroup;
          if (!(iss >> physGroup))
          {
            throw_at_line(lineNo, "Hiányzik a(z) " + std::to_string(pg + 1) + ". physical group név.");
          }
          physGroups.push_back(physGroup);
        }

        // Ellenőrizzük, hogy nincs-e extra adat
        std::string extra;
        if (iss >> extra)
        {
          throw_at_line(lineNo, "Túl sok adat a boundary sorban: \"" + boundLine + "\"");
        }

        // Ellenőrizzük, hogy nincs-e duplikált boundary név
        if (fresh.findBoundary(boundName) != nullptr)
        {
          throw_at_line(lineNo, "Ez a boundary név már szerepelt: " + boundName);
        }

        Boundary boundary;
        boundary.name = boundName;
        boundary.physicalGroups = physGroups;
        fresh.boundaries.push_back(boundary);
      }

      // Blokk lezárása kötelező: $EndBoundaries
      if (!std::getline(input, line))
      {
        throw_at_line(lineNo + 1, "Hiányzik a $EndBoundaries sor.");
      }
      ++lineNo;
      std::string endLine = strip_comment(line);
      trim_inplace(endLine);
      if (endLine != "$EndBoundaries")
      {
        throw_at_line(lineNo, "A $Boundaries blokkot $EndBoundaries sorral kell zárni.");
      }
      continue;
    }

    // --- 4) Mixtures ---
    if (cleaned == "$Mixtures")
    {
      const std::size_t mixtureCount = read_count(input, lineNo, "$Mixtures");
      for (std::size_t i = 0; i < mixtureCount; ++i)
      {
        if (!std::getline(input, line))
        {
          throw_at_line(lineNo + 1, "$Mixtures blokk vége előtt elfogyott a fájl.");
        }
        ++lineNo;
        std::string mixLine = strip_comment(line);
        trim_inplace(mixLine);
        if (mixLine.empty())
        {
          throw_at_line(lineNo, "$Mixtures sor üres.");
        }

        // Formátum: MixtureName density componentCount elem1 atoms1 elem2 atoms2 ...
        std::istringstream iss(mixLine);
        std::string mixName;
        double density = 0.0;
        int componentCount = 0;

        if (!(iss >> mixName >> density >> componentCount))
        {
          throw_at_line(lineNo, "Nem tudom kiolvasni a mixture nevet, density-t és componentCount-ot ebből a sorból: \"" + mixLine + "\"");
        }
        if (density <= 0.0)
        {
          throw_at_line(lineNo, "A density-nek pozitívnak kell lennie: " + std::to_string(density));
        }
        if (componentCount <= 0)
        {
          throw_at_line(lineNo, "A componentCount-nak pozitívnak kell lennie: " + std::to_string(componentCount));
        }

        // Komponensek beolvasása
        std::vector<MixtureComponent> components;
        for (int c = 0; c < componentCount; ++c)
        {
          std::string element;
          double atoms = 0.0;
          if (!(iss >> element >> atoms))
          {
            throw_at_line(lineNo, "Hiányzik a(z) " + std::to_string(c + 1) + ". komponens elem neve vagy atom száma.");
          }
          if (atoms <= 0.0)
          {
            throw_at_line(lineNo, "Az atom számnak pozitívnak kell lennie: " + std::to_string(atoms));
          }

          MixtureComponent comp;
          comp.element = element;
          comp.atoms = atoms;
          components.push_back(comp);
        }

        // Ellenőrizzük, hogy nincs-e extra adat
        std::string extra;
        if (iss >> extra)
        {
          throw_at_line(lineNo, "Túl sok adat a mixture sorban: \"" + mixLine + "\"");
        }

        // Ellenőrizzük, hogy nincs-e duplikált mixture név
        if (fresh.findMixture(mixName) != nullptr)
        {
          throw_at_line(lineNo, "Ez a mixture név már szerepelt: " + mixName);
        }

        Mixture mixture;
        mixture.name = mixName;
        mixture.density = density;
        mixture.components = components;
        fresh.mixtures.push_back(mixture);
      }

      // Blokk lezárása kötelező: $EndMixtures
      if (!std::getline(input, line))
      {
        throw_at_line(lineNo + 1, "Hiányzik a $EndMixtures sor.");
      }
      ++lineNo;
      std::string endLine = strip_comment(line);
      trim_inplace(endLine);
      if (endLine != "$EndMixtures")
      {
        throw_at_line(lineNo, "A $Mixtures blokkot $EndMixtures sorral kell zárni.");
      }
      continue;
    }

    // --- 5) Materials ---
    if (cleaned == "$Materials")
    {
      const std::size_t materialCount = read_count(input, lineNo, "$Materials");
      for (std::size_t i = 0; i < materialCount; ++i)
      {
        if (!std::getline(input, line))
        {
          throw_at_line(lineNo + 1, "$Materials blokk vége előtt elfogyott a fájl.");
        }
        ++lineNo;
        std::string matLine = strip_comment(line);
        trim_inplace(matLine);
        if (matLine.empty())
        {
          throw_at_line(lineNo, "$Materials sor üres.");
        }

        // Formátum: ZoneName MixtureName
        std::istringstream iss(matLine);
        std::string zoneName, mixtureName;
        if (!(iss >> zoneName >> mixtureName))
        {
          throw_at_line(lineNo, "Nem tudom kiolvasni a zone és mixture nevet ebből a sorból: \"" + matLine + "\"");
        }

        // Ellenőrizzük, hogy nincs-e extra adat
        std::string extra;
        if (iss >> extra)
        {
          throw_at_line(lineNo, "Túl sok adat a material sorban: \"" + matLine + "\"");
        }

        // Validáljuk, hogy létezik-e a zóna és a mixture
        if (fresh.findZone(zoneName) == nullptr)
        {
          throw_at_line(lineNo, "Ismeretlen zóna: \"" + zoneName + "\"");
        }
        if (fresh.findMixture(mixtureName) == nullptr)
        {
          throw_at_line(lineNo, "Ismeretlen mixture: \"" + mixtureName + "\"");
        }

        Material material;
        material.zoneName = zoneName;
        material.mixtureName = mixtureName;
        fresh.materials.push_back(material);
      }

      // Blokk lezárása kötelező: $EndMaterials
      if (!std::getline(input, line))
      {
        throw_at_line(lineNo + 1, "Hiányzik a $EndMaterials sor.");
      }
      ++lineNo;
      std::string endLine = strip_comment(line);
      trim_inplace(endLine);
      if (endLine != "$EndMaterials")
      {
        throw_at_line(lineNo, "A $Materials blokkot $EndMaterials sorral kell zárni.");
      }
      continue;
    }
  }

  // Sikeres betöltés után átmásoljuk az eredményt
  model = fresh;
}
