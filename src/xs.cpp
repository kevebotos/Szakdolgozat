#include "xs.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <cctype>

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

  std::string strip_comment(const std::string &line)
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
    throw XsParseError(lineNo, message);
  }

  std::vector<std::string> parse_string_list(const std::string &value, std::size_t lineNo)
  {
    if (value.size() < 2 || value.front() != '[' || value.back() != ']')
    {
      throw_at_line(lineNo, "A lista formátuma hibás (hiányzó szögletes zárójelek).");
    }
    std::string inside = value.substr(1, value.size() - 2);
    std::vector<std::string> result;
    std::string token;
    std::string current;
    std::size_t i = 0;
    while (i < inside.size())
    {
      char ch = inside[i];
      if (ch == ',')
      {
        current = token;
        trim_inplace(current);
        if (!current.empty())
        {
          if (current.front() == '"' && current.back() == '"' && current.size() >= 2)
          {
            current = current.substr(1, current.size() - 2);
          }
          result.push_back(current);
        }
        token.clear();
        ++i;
        continue;
      }
      token.push_back(ch);
      ++i;
    }
    current = token;
    trim_inplace(current);
    if (!current.empty())
    {
      if (current.front() == '"' && current.back() == '"' && current.size() >= 2)
      {
        current = current.substr(1, current.size() - 2);
      }
      result.push_back(current);
    }
    return result;
  }

  std::vector<double> parse_double_list(const std::string &value, std::size_t lineNo)
  {
    std::vector<double> numbers;
    std::istringstream iss(value);
    double number = 0.0;
    while (iss >> number)
    {
      numbers.push_back(number);
    }
    if (numbers.empty())
    {
      throw_at_line(lineNo, "Hiányoznak a numerikus értékek.");
    }
    return numbers;
  }

  void expect_group_count(const std::vector<double> &values, int expected, const std::string &field, std::size_t lineNo)
  {
    if (expected > 0 && static_cast<int>(values.size()) != expected)
    {
      throw_at_line(lineNo, field + " mezőben " + std::to_string(expected) + " értéket várok.");
    }
  }

  void expect_scatter_shape(const std::vector<std::vector<double>> &rows, int expected, std::size_t lineNo)
  {
    if (expected <= 0)
    {
      throw_at_line(lineNo, "A szórási mátrix feldolgozásához előbb a no_energy értéket kell megadni.");
    }
    if (static_cast<int>(rows.size()) != expected)
    {
      throw_at_line(lineNo, "A szórási mátrixnak " + std::to_string(expected) + " sorból kell állnia.");
    }
    for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
      const std::vector<double> &row = rows[rowIndex];
      if (static_cast<int>(row.size()) != expected)
      {
        throw_at_line(lineNo, "A szórási mátrix sorainak " + std::to_string(expected) + " elemből kell állniuk.");
      }
    }
  }

  void finalise_material(const XsMaterial &material, int groupCount, std::size_t lineNo)
  {
    if (material.name.empty())
    {
      throw_at_line(lineNo, "Hiányzik a material név.");
    }
    expect_group_count(material.sigma_t, groupCount, "sigma_t", lineNo);
    expect_group_count(material.sigma_a, groupCount, "sigma_a", lineNo);
    expect_group_count(material.nu_sigma_f, groupCount, "nu_sigma_f", lineNo);
    expect_group_count(material.chi, groupCount, "chi", lineNo);
    expect_scatter_shape(material.scatter, groupCount, lineNo);
  }
}

const XsMaterial *XsLibrary::find_material(const std::string &name) const
{
  for (std::size_t i = 0; i < materials.size(); ++i)
  {
    if (materials[i].name == name)
    {
      return &materials[i];
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

  XsLibrary fresh;
  std::string line;
  std::size_t lineNo = 0;
  bool insideMaterial = false;
  XsMaterial currentMaterial;

  while (std::getline(input, line))
  {
    ++lineNo;
    std::string cleaned = strip_comment(line);
    trim_inplace(cleaned);
    if (cleaned.empty())
    {
      continue;
    }

    if (cleaned.front() == '[')
    {
      if (cleaned.size() < 2 || cleaned.back() != ']')
      {
        throw_at_line(lineNo, "A szekció fejléc formátuma hibás.");
      }
      if (cleaned.compare(0, 10, "[Material ") == 0)
      {
        if (insideMaterial)
        {
          finalise_material(currentMaterial, fresh.energyGroupCount, lineNo);
          fresh.materials.push_back(currentMaterial);
          currentMaterial = XsMaterial();
        }
        const std::size_t firstQuote = cleaned.find('"');
        const std::size_t lastQuote = cleaned.rfind('"');
        if (firstQuote == std::string::npos || lastQuote == firstQuote)
        {
          throw_at_line(lineNo, "A material fejlécben idézőjelek közé kell tenni a nevet.");
        }
        std::string materialName = cleaned.substr(firstQuote + 1, lastQuote - firstQuote - 1);
        if (materialName.empty())
        {
          throw_at_line(lineNo, "A material név nem lehet üres.");
        }
        for (std::size_t i = 0; i < fresh.materials.size(); ++i)
        {
          if (fresh.materials[i].name == materialName)
          {
            throw_at_line(lineNo, "A(z) " + materialName + " anyag már szerepel a fájlban.");
          }
        }
        currentMaterial = XsMaterial();
        currentMaterial.name = materialName;
        insideMaterial = true;
        continue;
      }
      else
      {
        throw_at_line(lineNo, "Ismeretlen fejléc: " + cleaned);
      }
    }

    std::size_t equalPos = cleaned.find('=');
    if (equalPos == std::string::npos)
    {
      throw_at_line(lineNo, "A sorban hiányzik az '=' jel.");
    }

    std::string key = cleaned.substr(0, equalPos);
    std::string value = cleaned.substr(equalPos + 1);
    trim_inplace(key);
    trim_inplace(value);
    if (key.empty() || value.empty())
    {
      throw_at_line(lineNo, "A kulcs vagy az érték üres.");
    }

    if (!insideMaterial)
    {
      if (key == "title")
      {
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        {
          fresh.title = value.substr(1, value.size() - 2);
        }
        else
        {
          fresh.title = value;
        }
      }
      else if (key == "no_energy")
      {
        std::istringstream iss(value);
        int count = 0;
        if (!(iss >> count) || count <= 0)
        {
          throw_at_line(lineNo, "A no_energy értékének pozitív egész számnak kell lennie.");
        }
        fresh.energyGroupCount = count;
      }
      else if (key == "energy_groups")
      {
        fresh.energyGroupNames = parse_string_list(value, lineNo);
      }
      else if (key == "materials")
      {
        fresh.materialOrder = parse_string_list(value, lineNo);
      }
      else
      {
        throw_at_line(lineNo, "Ismeretlen globális kulcs: " + key);
      }
      continue;
    }

    if (key == "sigma_t")
    {
      std::vector<double> values = parse_double_list(value, lineNo);
      expect_group_count(values, fresh.energyGroupCount, key, lineNo);
      currentMaterial.sigma_t = values;
    }
    else if (key == "sigma_a")
    {
      std::vector<double> values = parse_double_list(value, lineNo);
      expect_group_count(values, fresh.energyGroupCount, key, lineNo);
      currentMaterial.sigma_a = values;
    }
    else if (key == "nu_sigma_f")
    {
      std::vector<double> values = parse_double_list(value, lineNo);
      expect_group_count(values, fresh.energyGroupCount, key, lineNo);
      currentMaterial.nu_sigma_f = values;
    }
    else if (key == "chi")
    {
      std::vector<double> values = parse_double_list(value, lineNo);
      expect_group_count(values, fresh.energyGroupCount, key, lineNo);
      currentMaterial.chi = values;
    }
    else if (key == "scatter")
    {
      if (value != "[")
      {
        throw_at_line(lineNo, "A scatter blokkot '[' jellel kell indítani.");
      }
      if (fresh.energyGroupCount <= 0)
      {
        throw_at_line(lineNo, "A scatter blokk előtt meg kell adni a no_energy értéket.");
      }
      std::vector<std::vector<double>> rows;
      while (std::getline(input, line))
      {
        ++lineNo;
        std::string rowLine = strip_comment(line);
        trim_inplace(rowLine);
        if (rowLine.empty())
        {
          continue;
        }
        if (rowLine == "]")
        {
          break;
        }
        std::vector<double> rowValues = parse_double_list(rowLine, lineNo);
        if (static_cast<int>(rowValues.size()) != fresh.energyGroupCount)
        {
          throw_at_line(lineNo, "A szórási mátrix soraihoz " + std::to_string(fresh.energyGroupCount) + " értéket várok.");
        }
        rows.push_back(rowValues);
      }
      if (rows.empty())
      {
        throw_at_line(lineNo, "A szórási mátrix nem lehet üres.");
      }
      currentMaterial.scatter = rows;
    }
    else
    {
      throw_at_line(lineNo, "Ismeretlen kulcs az anyag blokkjában: " + key);
    }
  }

  if (insideMaterial)
  {
    finalise_material(currentMaterial, fresh.energyGroupCount, lineNo == 0 ? 1 : lineNo);
    fresh.materials.push_back(currentMaterial);
  }

  if (fresh.energyGroupCount <= 0)
  {
    throw XsError("A fájl nem tartalmaz no_energy bejegyzést.");
  }

  if (!fresh.energyGroupNames.empty() && static_cast<int>(fresh.energyGroupNames.size()) != fresh.energyGroupCount)
  {
    throw XsError("Az energy_groups lista hossza nem egyezik a no_energy értékével.");
  }

  if (!fresh.materialOrder.empty())
  {
    for (std::size_t i = 0; i < fresh.materialOrder.size(); ++i)
    {
      const std::string &name = fresh.materialOrder[i];
      if (fresh.find_material(name) == nullptr)
      {
        throw XsError("A materials listában szereplő anyag hiányzik: " + name);
      }
    }
  }

  library = fresh;
}

