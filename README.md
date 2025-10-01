## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Run

```bash
./build/stove_heat --mesh vver440.msh --xs xs_vver440.txt
```

- `--mesh` : Gmsh MSH v2 ASCII hálófájl
- `--xs` : Többcsoportos keresztmetszet könyvtár (lásd `xs_vver440.txt`)

## Project Structure

- `CMakeLists.txt` - CMake build configuration
- `src/` - Source code (`main.cpp`, `mesh.cpp`, `mesh.hpp`, `xs.cpp`, `xs.hpp`)
- `stove.msh` - Mesh input file
- `xs_vver440.txt` - keresztmetszet "könyvtár"
- `build/` - (Ignored) Out-of-source build directory
