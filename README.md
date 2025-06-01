# Sdl3ComputeSample

This is a headless compute shader sample using the SDL3 GPU API. Currently works on Windows, but not on macOS.

## How to build
Make sure you have the following installed in your machine:

- CMake (through Kitware's installer or `winget` on Windows, or `brew` on macOS)
- either [vcpkg](https://github.com/microsoft/vcpkg), or install `sdl3` and `spdlog` through your package managers of choice

To configure the project:
```
$> mkdir build && cd build

# If using Ninja and vcpkg
$> cmake .. -G Ninja -DCMAKE_TOOLCHAIN_FILE=/(...)/vcpkg/scripts/buildsystems/vcpkg.cmake   
$> ninja

# If you installed sdl3 and spdlog with another package manager
$> cmake ..
$> cmake --build .
```

Make sure to run the application from the build directory so that it picks up the shader file.
