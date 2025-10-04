## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Run

```bash
./build/szakdolgozat --mesh vver440.msh --xs xs_vver440.txt --model model.txt --control control.txt
```

### Command-line options:

- `--mesh` / `-m` : Gmsh MSH v2 ASCII hálófájl (default: `vver440.msh`)
- `--xs` / `-x` : Többcsoportos keresztmetszet könyvtár (default: `xs_vver440.txt`)
- `--model` / `-d` : Model fájl zónákkal (default: `model.txt`)
- `--control` / `-c` : Kimenet kontroll fájl (default: `control.txt`)

## Output Control

A program kimenetét a `control.txt` fájl szabályozza. Három parser külön-külön kontrollálható:

### Verbosity szintek:

- `0` = Semmi kimenet (csendes)
- `1` = Alapvető összefoglaló (csomópontok száma, elemek száma, stb.)
- `2` = Részletes (fizikai csoportok, anyagok listája, stb.)
- `3` = Debug (minden részlet, scatter mátrix, komponensek)
- `4` = Minden adat + debug info (parsing idő, fájl méret, memória használat)
- `5` = CSAK debug info (nincs adat kimenet, csak metrikák)

### Példa control.txt:

```
$MeshOutput
verbosity 2
physical_groups on
elements_per_group on
$EndMeshOutput

$XsOutput
verbosity 1
energy_groups on
materials on
$EndXsOutput

$ModelOutput
verbosity 2
zones on
mixtures on
materials on
$EndModelOutput
```

### Elérhető flag-ek:

**Mesh:**

- `physical_groups` - Fizikai csoportok listája
- `elements_per_group` - Elemek száma fizikai csoportonként
- `boundary_nodes` - 1D elemekhez kapcsolódó csomópontok

**XS:**

- `energy_groups` - Energia csoport nevek
- `materials` - Anyag nevek
- `cross_sections` - Keresztmetszet értékek részletesen
- `scatter_matrix` - Scatter mátrix
- `boundaries` - Peremfeltételek

**Model:**

- `zones` - Zónák részletesen
- `boundaries` - Peremek részletesen
- `mixtures` - Keverékek
- `materials` - Zóna-anyag hozzárendelések
- `mixture_details` - Keverék komponensek

## Project Structure

- `CMakeLists.txt` - CMake build configuration
- `src/` - Source code
  - `main.cpp` - Fő program
  - `mesh.cpp`, `mesh.hpp` - Háló beolvasás (Gmsh MSH v2)
  - `xs.cpp`, `xs.hpp` - Keresztmetszet könyvtár beolvasás
  - `model.cpp`, `model.hpp` - Model fájl beolvasás (zónák, keverékek, anyagok)
  - `control.cpp`, `control.hpp` - Kimenet kontroll rendszer
- `vver440.msh` - Példa háló fájl
- `xs_vver440.txt` - Keresztmetszet könyvtár (anyagok + peremfeltételek)
- `model.txt` - Model fájl (zónák, keverékek, zóna-anyag hozzárendelések)
- `control.txt` - Kimenet kontroll fájl
- `build/` - (Ignored) Out-of-source build directory
