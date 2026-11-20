# TransportGraph

## Build and test on Windows

The project uses the `windows-x64-debug` and `windows-x64-release` CMake presets. Set `VCPKG_ROOT` to the root of your vcpkg installation before configuring so CMake can load the vcpkg toolchain:

```powershell
$env:VCPKG_ROOT = "<path-to-vcpkg>"

cmake --preset windows-x64-debug
cmake --build --preset windows-x64-debug
ctest --test-dir out/build/windows-x64-debug --output-on-failure

cmake --preset windows-x64-release
cmake --build --preset windows-x64-release
ctest --test-dir out/build/windows-x64-release --output-on-failure
```

`GraphSmokeTest` verifies that the sample map asset is present beside the executable after the post-build asset copy. Run the application with `out/build/windows-x64-debug/TransportGraph.exe` (or the corresponding release path).