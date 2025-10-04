## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Run

```bash
./build/szakdolgozat --mesh vver440.msh --xs xs_vver440.txt --model model.txt
```

### Command-line options:

- `--mesh` / `-m` : Gmsh MSH v2 ASCII hálófájl (default: `stove.msh`)
- `--xs` / `-x` : Többcsoportos keresztmetszet könyvtár (default: `xs_vver440.txt`)
- `--model` / `-d` : Model fájl zónákkal (default: `model.txt`)

## Project Structure

- `CMakeLists.txt` - CMake build configuration
- `src/` - Source code
  - `main.cpp` - Fő program
  - `mesh.cpp`, `mesh.hpp` - Háló beolvasás (Gmsh MSH v2)
  - `xs.cpp`, `xs.hpp` - Keresztmetszet könyvtár beolvasás
  - `model.cpp`, `model.hpp` - Model fájl beolvasás (zónák)
- `stove.msh` - Példa háló fájl
- `xs_vver440.txt` - Keresztmetszet könyvtár (anyagok + peremfeltételek)
- `model.txt` - Model fájl (zónák definíciói)
- `build/` - (Ignored) Out-of-source build directory
