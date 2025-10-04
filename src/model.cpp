#include "model.hpp"
#include <fstream>
#include <sstream>
#include <cctype>
#include <vector>

namespace
{
  void trim_inplace(std::string &text)
  {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
    {
      text.erase(text.begin());
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
    {
      text.pop_back();
    }
  }

  std::string strip_comment(const std::string &line) // kommentek törlése a fileból
  {
    std::string result = line;
    const std::size_t hashPos = result.find('#');
    if (hashPos != std::string::npos)
    {
      result.erase(hashPos);
    }
    return result;
  }

  [[noreturn]] void throw_at_line(std::size_t lineNo, const std::string &message)
  {
    throw ModelParseError(lineNo, message);
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

  // Idézőjelezett string kinyerése a sorból
  std::string extract_quoted_string(const std::string &text, std::size_t &pos, std::size_t lineNo)
  {
    if (pos >= text.size() || text[pos] != '"')
    {
      throw_at_line(lineNo, "Hiányzó idézőjel a stringnél.");
    }
    ++pos; // nyitó " átugrása
    std::string result;
    while (pos < text.size() && text[pos] != '"')
    {
      result += text[pos];
      ++pos;
    }
    if (pos >= text.size())
    {
      throw_at_line(lineNo, "Hiányzó záró idézőjel.");
    }
    ++pos; // záró " átugrása
    return result;
  }

  // Következő nem-whitespace token kinyerése
  std::string extract_token(const std::string &text, std::size_t &pos)
  {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])))
    {
      ++pos;
    }
    std::string result;
    while (pos < text.size() && !std::isspace(static_cast<unsigned char>(text[pos])))
    {
      result += text[pos];
      ++pos;
    }
    return result;
  }
}

const Zone *ModelLibrary::findZone(const std::string &name) const
{
  for (std::size_t i = 0; i < zones.size(); ++i)
  {
    if (zones[i].name == name)
    {
      return &zones[i];
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

  ModelLibrary fresh;
  std::string line;
  std::size_t lineNo = 0;

  while (std::getline(input, line))
  {
    ++lineNo;
    std::string cleaned = strip_comment(line);
    trim_inplace(cleaned);
    if (cleaned.empty())
    {
      continue;
    }

    // --- ModelInfo ---
    if (cleaned == "$ModelInfo")
    {
      if (!std::getline(input, line))
      {
        throw_at_line(lineNo + 1, "Váratlan fájlvég a $ModelInfo blokkban.");
      }
      ++lineNo;
      std::string titleLine = strip_comment(line);
      trim_inplace(titleLine);
      fresh.title = titleLine;

      if (!std::getline(input, line))
      {
        throw_at_line(lineNo + 1, "Hiányzó $EndModelInfo.");
      }
      ++lineNo;
      std::string endLine = strip_comment(line);
      trim_inplace(endLine);
      if (endLine != "$EndModelInfo")
      {
        throw_at_line(lineNo, "Várt $EndModelInfo, kapott: " + endLine);
      }
      continue;
    }

    // --- Zones ---
    if (cleaned == "$Zones")
    {
      const std::size_t zoneCount = read_count(input, lineNo, "$Zones");
      for (std::size_t i = 0; i < zoneCount; ++i)
      {
        if (!std::getline(input, line))
        {
          throw_at_line(lineNo + 1, "Váratlan fájlvég a $Zones blokkban.");
        }
        ++lineNo;
        std::string zoneLine = strip_comment(line);
        trim_inplace(zoneLine);
        if (zoneLine.empty())
        {
          throw_at_line(lineNo, "Üres sor a $Zones blokkban.");
        }

        // Formátum: név "physGroup1" ["physGroup2" ...] "leírás"
        std::size_t pos = 0;
        std::string zoneName = extract_token(zoneLine, pos);
        if (zoneName.empty())
        {
          throw_at_line(lineNo, "Hiányzó zone név.");
        }

        // Beolvassuk az összes physical group-ot (legalább 1 kell)
        std::vector<std::string> physGroups;
        while (pos < zoneLine.size())
        {
          while (pos < zoneLine.size() && std::isspace(static_cast<unsigned char>(zoneLine[pos])))
          {
            ++pos;
          }
          if (pos >= zoneLine.size())
          {
            break;
          }

          // Ha idézőjel, akkor physical group vagy description
          if (zoneLine[pos] == '"')
          {
            std::string quoted = extract_quoted_string(zoneLine, pos, lineNo);
            physGroups.push_back(quoted);
          }
          else
          {
            break;
          }
        }

        if (physGroups.empty())
        {
          throw_at_line(lineNo, "Legalább egy physical group szükséges.");
        }

        // Az utolsó quoted string a description
        std::string description = physGroups.back();
        physGroups.pop_back();

        if (physGroups.empty())
        {
          throw_at_line(lineNo, "Legalább egy physical group szükséges a description mellett.");
        }

        Zone zone;
        zone.name = zoneName;
        zone.physicalGroups = physGroups;
        zone.description = description;
        fresh.zones.push_back(zone);
      }

      if (!std::getline(input, line))
      {
        throw_at_line(lineNo + 1, "Hiányzó $EndZones.");
      }
      ++lineNo;
      std::string endLine = strip_comment(line);
      trim_inplace(endLine);
      if (endLine != "$EndZones")
      {
        throw_at_line(lineNo, "Várt $EndZones, kapott: " + endLine);
      }
      continue;
    }

    // --- Boundaries ---
    if (cleaned == "$Boundaries")
    {
      const std::size_t boundaryCount = read_count(input, lineNo, "$Boundaries");
      for (std::size_t i = 0; i < boundaryCount; ++i)
      {
        if (!std::getline(input, line))
        {
          throw_at_line(lineNo + 1, "Váratlan fájlvég a $Boundaries blokkban.");
        }
        ++lineNo;
        std::string boundLine = strip_comment(line);
        trim_inplace(boundLine);
        if (boundLine.empty())
        {
          throw_at_line(lineNo, "Üres sor a $Boundaries blokkban.");
        }

        // Formátum: név "physGroup1" ["physGroup2" ...] "leírás"
        std::size_t pos = 0;
        std::string boundName = extract_token(boundLine, pos);
        if (boundName.empty())
        {
          throw_at_line(lineNo, "Hiányzó boundary név.");
        }

        // Beolvassuk az összes physical group-ot (legalább 1 kell)
        std::vector<std::string> physGroups;
        while (pos < boundLine.size())
        {
          while (pos < boundLine.size() && std::isspace(static_cast<unsigned char>(boundLine[pos])))
          {
            ++pos;
          }
          if (pos >= boundLine.size())
          {
            break;
          }

          // Ha idézőjel, akkor physical group vagy description
          if (boundLine[pos] == '"')
          {
            std::string quoted = extract_quoted_string(boundLine, pos, lineNo);
            physGroups.push_back(quoted);
          }
          else
          {
            break;
          }
        }

        if (physGroups.empty())
        {
          throw_at_line(lineNo, "Legalább egy physical group szükséges.");
        }

        // Az utolsó quoted string a description
        std::string description = physGroups.back();
        physGroups.pop_back();

        if (physGroups.empty())
        {
          throw_at_line(lineNo, "Legalább egy physical group szükséges a description mellett.");
        }

        Boundary boundary;
        boundary.name = boundName;
        boundary.physicalGroups = physGroups;
        boundary.description = description;
        fresh.boundaries.push_back(boundary);
      }

      if (!std::getline(input, line))
      {
        throw_at_line(lineNo + 1, "Hiányzó $EndBoundaries.");
      }
      ++lineNo;
      std::string endLine = strip_comment(line);
      trim_inplace(endLine);
      if (endLine != "$EndBoundaries")
      {
        throw_at_line(lineNo, "Várt $EndBoundaries, kapott: " + endLine);
      }
      continue;
    }

    // --- Mixtures ---
    if (cleaned == "$Mixtures")
    {
      const std::size_t mixtureCount = read_count(input, lineNo, "$Mixtures");
      for (std::size_t i = 0; i < mixtureCount; ++i)
      {
        if (!std::getline(input, line))
        {
          throw_at_line(lineNo + 1, "Váratlan fájlvég a $Mixtures blokkban.");
        }
        ++lineNo;
        std::string mixLine = strip_comment(line);
        trim_inplace(mixLine);
        if (mixLine.empty())
        {
          throw_at_line(lineNo, "Üres sor a $Mixtures blokkban.");
        }

        // Formátum: név sűrűség "leírás" komponensszám elem1 atom1 elem2 atom2 ...
        std::istringstream iss(mixLine);
        std::string mixName;
        double density = 0.0;
        if (!(iss >> mixName >> density))
        {
          throw_at_line(lineNo, "Hibás mixture formátum (név vagy sűrűség).");
        }

        // Leírás beolvasása idézőjelek között
        std::string remainder;
        std::getline(iss, remainder);
        trim_inplace(remainder);
        std::size_t pos = 0;
        while (pos < remainder.size() && std::isspace(static_cast<unsigned char>(remainder[pos])))
        {
          ++pos;
        }
        std::string description = extract_quoted_string(remainder, pos, lineNo);

        // Komponensszám
        while (pos < remainder.size() && std::isspace(static_cast<unsigned char>(remainder[pos])))
        {
          ++pos;
        }
        std::size_t compCountStart = pos;
        while (pos < remainder.size() && !std::isspace(static_cast<unsigned char>(remainder[pos])))
        {
          ++pos;
        }
        std::string compCountStr = remainder.substr(compCountStart, pos - compCountStart);
        int compCount = 0;
        std::istringstream compIss(compCountStr);
        if (!(compIss >> compCount) || compCount <= 0)
        {
          throw_at_line(lineNo, "Hibás komponensszám.");
        }

        // Komponensek beolvasása
        std::vector<MixtureComponent> components;
        for (int c = 0; c < compCount; ++c)
        {
          while (pos < remainder.size() && std::isspace(static_cast<unsigned char>(remainder[pos])))
          {
            ++pos;
          }
          std::string elem = extract_token(remainder, pos);
          if (elem.empty())
          {
            throw_at_line(lineNo, "Hiányzó elem név a komponensben.");
          }

          while (pos < remainder.size() && std::isspace(static_cast<unsigned char>(remainder[pos])))
          {
            ++pos;
          }
          std::string atomsStr = extract_token(remainder, pos);
          if (atomsStr.empty())
          {
            throw_at_line(lineNo, "Hiányzó atom szám a komponensben.");
          }

          double atoms = 0.0;
          std::istringstream atomIss(atomsStr);
          if (!(atomIss >> atoms))
          {
            throw_at_line(lineNo, "Hibás atom szám: " + atomsStr);
          }

          MixtureComponent comp;
          comp.element = elem;
          comp.atoms = atoms;
          components.push_back(comp);
        }

        Mixture mixture;
        mixture.name = mixName;
        mixture.density = density;
        mixture.description = description;
        mixture.components = components;
        fresh.mixtures.push_back(mixture);
      }

      if (!std::getline(input, line))
      {
        throw_at_line(lineNo + 1, "Hiányzó $EndMixtures.");
      }
      ++lineNo;
      std::string endLine = strip_comment(line);
      trim_inplace(endLine);
      if (endLine != "$EndMixtures")
      {
        throw_at_line(lineNo, "Várt $EndMixtures, kapott: " + endLine);
      }
      continue;
    }

    // --- Materials ---
    if (cleaned == "$Materials")
    {
      const std::size_t materialCount = read_count(input, lineNo, "$Materials");
      for (std::size_t i = 0; i < materialCount; ++i)
      {
        if (!std::getline(input, line))
        {
          throw_at_line(lineNo + 1, "Váratlan fájlvég a $Materials blokkban.");
        }
        ++lineNo;
        std::string matLine = strip_comment(line);
        trim_inplace(matLine);
        if (matLine.empty())
        {
          throw_at_line(lineNo, "Üres sor a $Materials blokkban.");
        }

        // Formátum: ZoneName MixtureName
        std::istringstream iss(matLine);
        std::string zoneName, mixtureName;
        if (!(iss >> zoneName >> mixtureName))
        {
          throw_at_line(lineNo, "Hibás material formátum. Várt: ZoneName MixtureName");
        }

        // Ellenőrizzük, hogy van-e extra adat a sorban
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

      if (!std::getline(input, line))
      {
        throw_at_line(lineNo + 1, "Hiányzó $EndMaterials.");
      }
      ++lineNo;
      std::string endLine = strip_comment(line);
      trim_inplace(endLine);
      if (endLine != "$EndMaterials")
      {
        throw_at_line(lineNo, "Várt $EndMaterials, kapott: " + endLine);
      }
      continue;
    }
  }

  model = fresh;
}
