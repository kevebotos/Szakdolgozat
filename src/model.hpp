#ifndef MODEL_HPP
#define MODEL_HPP

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

struct Zone
{
  std::string name;
  std::vector<std::string> physicalGroups; // 2D (térfogati) mesh physical group nevek
};

struct Boundary
{
  std::string name;
  std::vector<std::string> physicalGroups; // 1D (felületi/él) mesh physical group nevek
};

// Anyag keverék komponens
struct MixtureComponent
{
  std::string element; // "H", "O", "U235", stb.
  double atoms;        // atom szám (pl. H2O-nál H=2, O=1)
};

// Keverék definíció
struct Mixture
{
  std::string name;
  double density;                           // g/cm³
  std::vector<MixtureComponent> components;
};

// Anyag hozzárendelés: melyik zónában milyen keverék van
struct Material
{
  std::string zoneName;    // Melyik zónához tartozik
  std::string mixtureName; // Melyik anyagkeverék van benne
};

struct ModelLibrary
{
  std::string title;
  std::vector<Zone> zones;
  std::vector<Boundary> boundaries;
  std::vector<Mixture> mixtures;
  std::vector<Material> materials; // Zóna-anyag hozzárendelések

  // Helper: zóna keresése név alapján
  const Zone *findZone(const std::string &name) const;
  // Helper: perem keresése név alapján
  const Boundary *findBoundary(const std::string &name) const;
  // Helper: keverék keresése név alapján
  const Mixture *findMixture(const std::string &name) const;
};

class ModelError : public std::runtime_error
{
public:
  using std::runtime_error::runtime_error;
};

class ModelParseError : public ModelError
{
public:
  ModelParseError(std::size_t line, const std::string &message)
      : ModelError(message), m_line(line)
  {
  }

  std::size_t line() const noexcept { return m_line; }

private:
  std::size_t m_line = 0;
};

void loadModel(const std::string &path, ModelLibrary &model);

#endif // MODEL_HPP
