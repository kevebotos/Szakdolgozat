#include "xs.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>
#include <memory>

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

// Kommentek eltávolítása a sorból (# karakter után mindent töröl)
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

// Egyszerű, kezdőbarát segédfüggvények a dobásokhoz és számlálósor olvasásához
namespace
{
  [[noreturn]] void throw_at_line(std::size_t currentLine, const std::string &message)
  {
    throw XsParseError(currentLine, message);
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

  // Vector beolvasása a sorból (pl. "1.0 2.0 3.0")
  std::vector<double> parse_vector(const std::string &line, std::size_t lineNo, int expectedSize)
  {
    std::vector<double> values;
    std::istringstream iss(line);
    double val = 0.0;
    while (iss >> val)
    {
      values.push_back(val);
    }
    if (expectedSize > 0 && static_cast<int>(values.size()) != expectedSize)
    {
      throw_at_line(lineNo, "Várt " + std::to_string(expectedSize) + " értéket, de " + std::to_string(values.size()) + " találtam.");
    }
    if (values.empty())
    {
      throw_at_line(lineNo, "Hiányzó numerikus értékek.");
    }
    return values;
  }

  // Kulcs-érték pár olvasása (pl. "sigma_t 1.0 2.0")
  bool parse_key_value(const std::string &line, std::string &key, std::string &value)
  {
    std::size_t spacePos = line.find(' ');
    if (spacePos == std::string::npos)
    {
      return false;
    }
    key = line.substr(0, spacePos);
    value = line.substr(spacePos + 1);
    trim_inplace(key);
    trim_inplace(value);
    return !key.empty() && !value.empty();
  }
}

const XsMaterial::SPtr XsLibrary::find_material(const std::string &name) const
{
  for (std::size_t i = 0; i < materials.size(); ++i)
  {
    if (materials[i].name == name)
    {
      return std::make_shared<XsMaterial>(materials[i]);
    }
  }
  return nullptr;
}

const XsBoundary *XsLibrary::find_boundary(const std::string &name) const
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

void load_xs(const std::string &path, XsLibrary &library)
{
  std::ifstream input(path);
  if (!input)
  {
    throw XsError("Nem sikerült megnyitni a keresztmetszet fájlt: " + path);
  }

  XsLibrary fresh; // Ide töltjük be a kész library-t
  std::string line; // Az aktuálisan olvasott sor
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

    // --- 1) XsInfo ---
    if (cleaned == "$XsInfo")
    {
      if (!std::getline(input, line))
      {
        throw_at_line(lineNo + 1, "$XsInfo blokk vége előtt elfogyott a fájl.");
      }
      ++lineNo;
      std::string titleLine = strip_comment(line);
      trim_inplace(titleLine);
      fresh.title = titleLine;

      // Blokk lezárása kötelező: $EndXsInfo
      if (!std::getline(input, line))
      {
        throw_at_line(lineNo + 1, "Hiányzik a $EndXsInfo sor.");
      }
      ++lineNo;
      std::string endLine = strip_comment(line);
      trim_inplace(endLine);
      if (endLine != "$EndXsInfo")
      {
        throw_at_line(lineNo, "A $XsInfo blokkot $EndXsInfo sorral kell zárni.");
      }
      continue;
    }

    // --- 2) EnergyGroups ---
    if (cleaned == "$EnergyGroups")
    {
      const std::size_t groupCount = read_count(input, lineNo, "$EnergyGroups");
      fresh.energyGroupCount = static_cast<int>(groupCount);

      // Energia csoport nevek beolvasása (opcionális, de ha van, akkor pontosan groupCount darab)
      for (std::size_t i = 0; i < groupCount; ++i)
      {
        if (!std::getline(input, line))
        {
          throw_at_line(lineNo + 1, "$EnergyGroups blokk vége előtt elfogyott a fájl.");
        }
        ++lineNo;
        std::string groupName = strip_comment(line);
        trim_inplace(groupName);
        if (groupName.empty())
        {
          throw_at_line(lineNo, "$EnergyGroups sor üres.");
        }
        fresh.energyGroupNames.push_back(groupName);
      }

      // Blokk lezárása kötelező: $EndEnergyGroups
      if (!std::getline(input, line))
      {
        throw_at_line(lineNo + 1, "Hiányzik a $EndEnergyGroups sor.");
      }
      ++lineNo;
      std::string endLine = strip_comment(line);
      trim_inplace(endLine);
      if (endLine != "$EndEnergyGroups")
      {
        throw_at_line(lineNo, "A $EnergyGroups blokkot $EndEnergyGroups sorral kell zárni.");
      }
      continue;
    }

    // --- 3) Materials ---
    if (cleaned == "$Materials")
    {
      const std::size_t materialCount = read_count(input, lineNo, "$Materials");

      std::size_t materialsRead = 0;
      while (materialsRead < materialCount)
      {
        // Material név olvasása (üres sorokat átugorjuk)
        std::string matLine;
        while (true)
        {
          if (!std::getline(input, line))
          {
            throw_at_line(lineNo + 1, "$Materials blokk vége előtt elfogyott a fájl.");
          }
          ++lineNo;
          matLine = strip_comment(line);
          trim_inplace(matLine);
          if (!matLine.empty())
          {
            break; // Nem üres sort találtunk
          }
        }

        XsMaterial mat;
        mat.name = matLine;

        // Ellenőrizzük, hogy nincs-e duplikált material név
        if (fresh.find_material(mat.name) != nullptr)
        {
          throw_at_line(lineNo, "Ez a material név már szerepelt: " + mat.name);
        }

        // sigma_t olvasása
        if (!std::getline(input, line))
        {
          throw_at_line(lineNo + 1, "Hiányzó sigma_t sor a(z) " + mat.name + " materialhoz.");
        }
        ++lineNo;
        std::string sigmaLine = strip_comment(line);
        trim_inplace(sigmaLine);
        std::string key, value;
        if (!parse_key_value(sigmaLine, key, value) || key != "sigma_t")
        {
          throw_at_line(lineNo, "Várt 'sigma_t' sort a(z) " + mat.name + " materialhoz.");
        }
        mat.sigma_t = parse_vector(value, lineNo, fresh.energyGroupCount);

        // sigma_a olvasása
        if (!std::getline(input, line))
        {
          throw_at_line(lineNo + 1, "Hiányzó sigma_a sor a(z) " + mat.name + " materialhoz.");
        }
        ++lineNo;
        sigmaLine = strip_comment(line);
        trim_inplace(sigmaLine);
        if (!parse_key_value(sigmaLine, key, value) || key != "sigma_a")
        {
          throw_at_line(lineNo, "Várt 'sigma_a' sort a(z) " + mat.name + " materialhoz.");
        }
        mat.sigma_a = parse_vector(value, lineNo, fresh.energyGroupCount);

        // nu_sigma_f olvasása
        if (!std::getline(input, line))
        {
          throw_at_line(lineNo + 1, "Hiányzó nu_sigma_f sor a(z) " + mat.name + " materialhoz.");
        }
        ++lineNo;
        sigmaLine = strip_comment(line);
        trim_inplace(sigmaLine);
        if (!parse_key_value(sigmaLine, key, value) || key != "nu_sigma_f")
        {
          throw_at_line(lineNo, "Várt 'nu_sigma_f' sort a(z) " + mat.name + " materialhoz.");
        }
        mat.nu_sigma_f = parse_vector(value, lineNo, fresh.energyGroupCount);

        // chi olvasása
        if (!std::getline(input, line))
        {
          throw_at_line(lineNo + 1, "Hiányzó chi sor a(z) " + mat.name + " materialhoz.");
        }
        ++lineNo;
        sigmaLine = strip_comment(line);
        trim_inplace(sigmaLine);
        if (!parse_key_value(sigmaLine, key, value) || key != "chi")
        {
          throw_at_line(lineNo, "Várt 'chi' sort a(z) " + mat.name + " materialhoz.");
        }
        mat.chi = parse_vector(value, lineNo, fresh.energyGroupCount);

        // $Scatter blokk olvasása
        if (!std::getline(input, line))
        {
          throw_at_line(lineNo + 1, "Hiányzó $Scatter blokk a(z) " + mat.name + " materialhoz.");
        }
        ++lineNo;
        std::string scatterStart = strip_comment(line);
        trim_inplace(scatterStart);
        if (scatterStart != "$Scatter")
        {
          throw_at_line(lineNo, "Várt '$Scatter' sort a(z) " + mat.name + " materialhoz.");
        }

        // Scatter mátrix sorai (energyGroupCount db sor kell)
        std::vector<std::vector<double>> scatterMatrix;
        for (int row = 0; row < fresh.energyGroupCount; ++row)
        {
          if (!std::getline(input, line))
          {
            throw_at_line(lineNo + 1, "$Scatter blokk vége előtt elfogyott a fájl.");
          }
          ++lineNo;
          std::string rowLine = strip_comment(line);
          trim_inplace(rowLine);
          if (rowLine.empty())
          {
            throw_at_line(lineNo, "$Scatter mátrix sor üres.");
          }
          std::vector<double> rowValues = parse_vector(rowLine, lineNo, fresh.energyGroupCount);
          scatterMatrix.push_back(rowValues);
        }
        mat.scatter = scatterMatrix;

        // $EndScatter olvasása
        if (!std::getline(input, line))
        {
          throw_at_line(lineNo + 1, "Hiányzik a $EndScatter sor.");
        }
        ++lineNo;
        std::string scatterEnd = strip_comment(line);
        trim_inplace(scatterEnd);
        if (scatterEnd != "$EndScatter")
        {
          throw_at_line(lineNo, "A $Scatter blokkot $EndScatter sorral kell zárni.");
        }

        fresh.materials.push_back(mat);
        ++materialsRead;
      }

      // Blokk lezárása kötelező: $EndMaterials (üres sorokat átugorjuk)
      while (true)
      {
        if (!std::getline(input, line))
        {
          throw_at_line(lineNo + 1, "Hiányzik a $EndMaterials sor.");
        }
        ++lineNo;
        std::string endLine = strip_comment(line);
        trim_inplace(endLine);
        if (endLine.empty())
        {
          continue; // Üres sor, próbáljuk újra
        }
        if (endLine != "$EndMaterials")
        {
          throw_at_line(lineNo, "A $Materials blokkot $EndMaterials sorral kell zárni.");
        }
        break; // Megvan az $EndMaterials
      }
      continue;
    }

    // --- 4) Boundaries ---
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

        // Formátum: BoundaryName type
        std::istringstream iss(boundLine);
        std::string boundName, boundType;
        if (!(iss >> boundName >> boundType))
        {
          throw_at_line(lineNo, "Nem tudom kiolvasni a boundary nevet és típust ebből a sorból: \"" + boundLine + "\"");
        }

        // Ellenőrizzük, hogy nincs-e extra adat
        std::string extra;
        if (iss >> extra)
        {
          throw_at_line(lineNo, "Túl sok adat a boundary sorban: \"" + boundLine + "\"");
        }

        // Validáció: csak "vacuum" vagy "interface" megengedett
        if (boundType != "vacuum" && boundType != "interface")
        {
          throw_at_line(lineNo, "A boundary type csak 'vacuum' vagy 'interface' lehet.");
        }

        // Ellenőrizzük, hogy nincs-e duplikált boundary név
        if (fresh.find_boundary(boundName) != nullptr)
        {
          throw_at_line(lineNo, "Ez a boundary név már szerepelt: " + boundName);
        }

        XsBoundary boundary;
        boundary.name = boundName;
        boundary.type = boundType;
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
  }

  // Validációk
  if (fresh.energyGroupCount <= 0)
  {
    throw XsError("A fájl nem tartalmaz $EnergyGroups blokkot vagy az energia csoportok száma 0.");
  }

  // Sikeres betöltés után átmásoljuk az eredményt
  library = fresh;
}
