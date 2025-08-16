# Stove Heat Mini Project

A small C++ / CMake project that appears to load a mesh file (`stove.msh`) and perform a heat-related simulation.

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

## Contributing
Feel free to fork and open pull requests.

## License
Specify a license (e.g., MIT) if you plan to open source this.
