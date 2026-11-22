# Building Sorrow Integration

## GitHub Actions (recommended, builds all platforms)
Push this folder to a GitHub repo. The included workflow (`.github/workflows/build.yml`)
builds Windows, macOS, Android 64, Android 32 and iOS on every push and uploads the
finished `.geode` files as artifacts. No local setup needed.

## Manual Android 64 build (aarch64 Linux host)
The CI builds against the NDK normally. On an aarch64 Linux box (where the NDK's
bundled x86_64 clang cannot run), the following works:

1. Install clang-20/lld-20, cmake >= 3.25, ninja.
2. Download the Android NDK r28c (sysroot is host-independent).
3. Use a toolchain file that includes the NDK's own
   `build/cmake/android.toolchain.cmake`, then overrides the compilers with the
   host-native clang:

       set(CMAKE_ANDROID_NDK /opt/android-ndk-r28c)
       set(ANDROID_ABI arm64-v8a)
       set(ANDROID_PLATFORM android-24)
       set(ANDROID_STL c++_shared)
       include(/opt/android-ndk-r28c/build/cmake/android.toolchain.cmake)
       set(CMAKE_C_COMPILER /usr/lib/llvm-20/bin/clang)
       ... (see /tmp/a64-toolchain.cmake in the working session)

4. Place the official `Geode.android64.so` loader binary from the Geode release
   into `<sdk>/bin/<version>/Geode.android64.so`.
5. Linker shims: copies of the NDK's libclang_rt.builtins-aarch64-android.a named
   libgcc.a and libunwind.a on the linker search path.

## Vendored dependency notes (NDK r27/r28, LLVM 18/19 headers)
- `arc` uses std::atomic_ref (LLVM 20 libc++): under LLVM 18/19 headers the two
  uses in arc Runtime.cpp can be replaced with __atomic_load_n/__atomic_store_n.
- `std23/move_only_function.h` relies on <type_traits> arriving transitively;
  older libc++ needs an explicit #include <type_traits>.
Neither is needed with the newest NDK on CI.
