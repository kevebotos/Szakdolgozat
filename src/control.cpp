#include "control.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <iostream>

namespace
{
  // Szöveg elejéről és végéről eltávolítja a whitespace karaktereket
  void trim_inplace(std::string &text)
  {
    // Elején levő whitespace-ek törlése
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
    {
      text.erase(text.begin());
    }
    // Végén levő whitespace-ek törlése
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
    {
      text.pop_back();
    }
  }

  // Kommentek eltávolítása a sorból (# karakter után mindent töröl)
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

  // String-ből bool konverzió (on/off, true/false, 1/0)
  bool parse_bool(const std::string &value)
  {
    if (value == "on" || value == "true" || value == "1")
    {
      return true;
    }
    if (value == "off" || value == "false" || value == "0")
    {
      return false;
    }
    throw std::runtime_error("Érvénytelen bool érték: " + value + " (használj: on/off, true/false, vagy 1/0)");
  }
}

// ParserOutputConfig::getFlag implementáció
bool ParserOutputConfig::getFlag(const std::string &name) const
{
  // Megkeressük a flag-et a map-ben
  std::map<std::string, bool>::const_iterator it = flags.find(name);
  if (it != flags.end())
  {
    return it->second; // Ha megvan, visszaadjuk az értékét
  }
  return false; // Ha nincs megadva, akkor false az alapértelmezett
}

// ControlConfig::getEffectiveVerbosity implementáció
int ControlConfig::getEffectiveVerbosity(const ParserOutputConfig &config) const
{
  // Ha master_verbosity be van állítva (>=0), azt használjuk
  if (masterVerbosity >= 0)
  {
    return masterVerbosity;
  }
  // Különben a parser saját verbosity-ját használjuk
  return config.verbosity;
}

// Control fájl betöltése
void loadControl(const std::string &path, ControlConfig &config)
{
  std::ifstream input(path);
  if (!input)
  {
    // Ha nincs control fájl, default értékekkel térünk vissza
    // Ez nem hiba - a program működik control.txt nélkül is
    return;
  }

  ControlConfig fresh; // Új, üres konfiguráció
  std::string line;
  std::size_t lineNo = 0;
  std::string currentSection; // Melyik szekcióban vagyunk éppen ($MeshOutput, $XsOutput, stb.)

  while (std::getline(input, line))
  {
    ++lineNo;
    std::string cleaned = strip_comment(line);
    trim_inplace(cleaned);

    // Üres sorok átugrása
    if (cleaned.empty())
    {
      continue;
    }

    // Szekció kezdések felismerése
    if (cleaned == "$MeshOutput")
    {
      currentSection = "MeshOutput";
      continue;
    }
    if (cleaned == "$XsOutput")
    {
      currentSection = "XsOutput";
      continue;
    }
    if (cleaned == "$ModelOutput")
    {
      currentSection = "ModelOutput";
      continue;
    }
    if (cleaned == "$GlobalOutput")
    {
      currentSection = "GlobalOutput";
      continue;
    }

    // Szekció végek felismerése
    if (cleaned == "$EndMeshOutput" || cleaned == "$EndXsOutput" ||
        cleaned == "$EndModelOutput" || cleaned == "$EndGlobalOutput")
    {
      currentSection.clear(); // Kilépünk a szekcióból
      continue;
    }

    // Ha szekcióban vagyunk, akkor parsing
    if (!currentSection.empty())
    {
      std::istringstream iss(cleaned);
      std::string key, value;

      // Beolvassuk a kulcs-érték párt (pl. "verbosity 2")
      if (!(iss >> key >> value))
      {
        std::cerr << "[FIGYELMEZTETÉS] Control fájl sor " << lineNo
                  << ": Nem értelmezhető sor, kihagyom: \"" << cleaned << "\"\n";
        continue;
      }

      // Global szekció kezelése
      if (currentSection == "GlobalOutput")
      {
        if (key == "master_verbosity")
        {
          try
          {
            fresh.masterVerbosity = std::stoi(value);
          }
          catch (const std::exception &ex)
          {
            std::cerr << "[FIGYELMEZTETÉS] Control fájl sor " << lineNo
                      << ": Érvénytelen master_verbosity érték: \"" << value << "\"\n";
          }
        }
        else if (key == "format")
        {
          fresh.format = value;
        }
        else
        {
          std::cerr << "[FIGYELMEZTETÉS] Control fájl sor " << lineNo
                    << ": Ismeretlen globális beállítás: \"" << key << "\"\n";
        }
      }
      // Parser szekciók kezelése (MeshOutput, XsOutput, ModelOutput)
      else
      {
        // Melyik parser konfigurációhoz tartozik ez a szekció?
        ParserOutputConfig *targetConfig = nullptr;
        if (currentSection == "MeshOutput")
        {
          targetConfig = &fresh.meshOutput;
        }
        else if (currentSection == "XsOutput")
        {
          targetConfig = &fresh.xsOutput;
        }
        else if (currentSection == "ModelOutput")
        {
          targetConfig = &fresh.modelOutput;
        }

        if (targetConfig != nullptr)
        {
          // "verbosity" kulcs speciális kezelése
          if (key == "verbosity")
          {
            try
            {
              targetConfig->verbosity = std::stoi(value);
            }
            catch (const std::exception &ex)
            {
              std::cerr << "[FIGYELMEZTETÉS] Control fájl sor " << lineNo
                        << ": Érvénytelen verbosity érték: \"" << value << "\"\n";
            }
          }
          else
          {
            // Minden más kulcs egy flag (pl. "physical_groups on")
            try
            {
              targetConfig->flags[key] = parse_bool(value);
            }
            catch (const std::exception &ex)
            {
              std::cerr << "[FIGYELMEZTETÉS] Control fájl sor " << lineNo
                        << ": " << ex.what() << "\n";
            }
          }
        }
      }
    }
  }

  // Ha sikeresen végigmentünk a fájlon, akkor átmásoljuk az új konfigurációt
  config = fresh;
}
