## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Run

```bash
./build/stove_heat --mesh stove.msh
```

## Project Structure

- `CMakeLists.txt` - CMake build configuration
- `src/` - Source code (`main.cpp`, `mesh.cpp`, `mesh.hpp`)
- `stove.msh` - Mesh input file
- `build/` - (Ignored) Out-of-source build directory
