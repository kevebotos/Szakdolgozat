**AGENTS.md**

````markdown
# Projekt áttekintés - VVER-440 Neutron Diffúzió Szimuláció

## Mi a projekt célja?

Végeselem módszerrel (FEM) megoldani a stacionárius, kétcsoportos neutron diffúzió egyenletet egy VVER-440 típusú reaktor kazettán.

**Végső cél:**

- Megtalálni a k_eff effektív multiplikációs tényezőt (kritikalitás)
- Kiszámolni a neutron flux eloszlást a reaktorban

## Jelenlegi állapot

### Működő komponensek:

**1. Háló beolvasás** (`mesh.cpp`, `mesh.hpp`)

- GMSH MSH v2 ASCII formátum
- 2D háromszög elemek + 1D vonal elemek (határok)
- Fizikai csoportok (pl. "Fuel", "Moderator", "Outer-Boundary")
- 1-based indexelés a csomópontoknál

**2. Keresztmetszet könyvtár** (`xs.cpp`, `xs.hpp`)

- Kétcsoportos makroszkopikus hatáskeresztmetszetek
- Anyagonként: sigma_t, sigma_a, nu_sigma_f, chi, scatter mátrix
- Fizikai csoport név → anyag leképezés

**3. Model fájl** (`model.cpp`, `model.hpp`)

- Zónák, keverékek, anyag hozzárendelések
- Peremfeltételek kezelése

**4. Kimenet kontroll** (`control.cpp`, `control.hpp`)

- Verbosity szintek parsenként (mesh, xs, model)
- Flag-ek a kimenet finomhangolásához
- Automatikus validációk (háttérben mindig futnak, nem konfigurálhatók)

**5. Main program** (`main.cpp`)

- Parancssori argumentumok (--mesh, --xs, --model, --control)
- Beolvasás és statisztikák kiírása
- Hibakezelés (try-catch)

### Fő adatstruktúrák:

```cpp
Mesh::Node {double x, y}              // 2D csomópont
Mesh::Tri {int a, b, c; int phys}     // Háromszög elem
Mesh::Line {int a, b; int phys}       // Vonal elem (határ)
XsMaterial {sigma_t, sigma_a, ...}    // Anyag adatok
```
````

## Jövőbeli lépések (még NEM implementálva)

1. **Peremfeltételek** - következő feladat (lásd TASK.md)
2. **Alakfüggvények** (shape functions) - lokális bázisfüggvények háromszögekhez
3. **Elem mátrixok** - lokális merevségi és tömegmátrixok számítása
4. **Globális mátrix összeállítás** - A és M mátrixok
5. **Peremfeltételek alkalmazása** - boundary conditions beépítése
6. **Eigenvalue solver** - k_eff és flux megoldása (Eigen library)

## Kódolási szabályok

**Nyelv:**

- Kód (változók, függvények): **angol**
- Kommentek: magyar
- Hibaüzenetek: magyar

**Stílus:**

- Egyszerű, kezdőbarát C++17
- `std::vector`, `std::map`, `std::string` használata OK
- Smart pointerek csak ha szükséges
- Lambdák csak ha abszolút kell a hatékonysághoz
- Kerüld: komplex template-ek, raw pointerek, makrók

**Hibakezelés:**

- Meglévő `MeshError`, `XsError` osztályok használata
- `throw_at_line(lineNo, "...")` helper függvény
- Érthető magyar hibaüzenetek sorszámmal

## Fontos megjegyzések

- **Nodes indexelés:** 1-based (nodes[0] üres)
- **Még NEM használunk Eigen library-t** (csak később a solvernél)
- **Magyar nyelvű konzol kimenetek**
- Minden file művelet előtt hibakezelés

## Build és futtatás

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/szakdolgozat --mesh vver440.msh --xs xs_vver440.txt
```

## Input fájlok

- `vver440.geo` - Gmsh geometria definíció
- `vver440.msh` - Generált háló (gmsh-sal)
- `xs_vver440.txt` - Hatáskeresztmetszet könyvtár

```

```
