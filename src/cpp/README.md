# convertNormalToQlog (C++)

This C++ utility uses a [vcpkg](https://vcpkg.io/) manifest file to configure dependencies.

This project relies on CMake to generate project files. Please run the following command with the correct path to vcpkg:

```cmd
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
```

To build the executable:

```cmd
cmake --build build
```

For more information on using vcpkg and CMake see [Microsoft Learn](https://learn.microsoft.com/en-us/vcpkg/consume/manifest-mode?tabs=cmake%2Cbuild-cmake)