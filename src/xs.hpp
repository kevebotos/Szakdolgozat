#ifndef XS_HPP
#define XS_HPP

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

struct XsMaterial
{
  typedef std::shared_ptr<XsMaterial> SPtr;
  typedef std::unique_ptr<XsMaterial> UPtr;
  typedef std::weak_ptr<XsMaterial> WPtr;

  std::string name;
  std::vector<double> sigma_t;
  std::vector<double> sigma_a;
  std::vector<double> nu_sigma_f;
  std::vector<double> chi;
  std::vector<std::vector<double>> scatter;
};

struct XsBoundary
{
  std::string name;
  std::string type;
  double value = 0.0;
};

struct XsLibrary
{
  std::string title;
  int energyGroupCount = 0;
  std::vector<std::string> energyGroupNames;
  std::vector<std::string> materialOrder;
  std::vector<XsMaterial> materials;
  std::vector<XsBoundary> boundaries;

  const XsMaterial::SPtr find_material(const std::string &name) const;
};

class XsError : public std::runtime_error
{
public:
  using std::runtime_error::runtime_error;
};

class XsParseError : public XsError
{
public:
  XsParseError(std::size_t line, const std::string &message)
      : XsError(message), m_line(line)
  {
  }

  std::size_t line() const noexcept { return m_line; }

private:
  std::size_t m_line = 0;
};

void load_xs(const std::string &path, XsLibrary &library);

#endif // XS_HPP
