#ifndef CONTROL_HPP
#define CONTROL_HPP

#include <string>
#include <map>

// Egy parser kimenet konfigurációja (mesh, xs, vagy model)
struct ParserOutputConfig
{
  int verbosity = 1; // 0=semmi, 1=alap, 2=részletes, 3=debug
  std::map<std::string, bool> flags; // Egyedi kapcsolók: "physical_groups", "materials", stb.

  // Helper: Flag értékének lekérdezése (ha nincs megadva, akkor false)
  bool getFlag(const std::string &name) const;
};

// Teljes control konfiguráció (az egész programhoz)
struct ControlConfig
{
  // Parser-specifikus beállítások
  ParserOutputConfig meshOutput;
  ParserOutputConfig xsOutput;
  ParserOutputConfig modelOutput;

  // Globális beállítások
  int masterVerbosity = -1;      // -1 = nincs beállítva, egyébként felülírja az összes parser verbosity-t
  std::string format = "plain";  // Kimenet formátuma (jelenleg csak "plain")

  // Helper: Effektív verbosity lekérdezése egy adott parserhez
  // Ha master_verbosity be van állítva (>=0), azt használja, különben a parser saját verbosity-ját
  int getEffectiveVerbosity(const ParserOutputConfig &config) const;
};

// Control fájl betöltése
// Ha a fájl nem létezik, akkor default értékekkel tér vissza (nincs hiba)
void loadControl(const std::string &path, ControlConfig &config);

#endif // CONTROL_HPP
