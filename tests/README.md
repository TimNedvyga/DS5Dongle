# Wake recovery host regression test

The test compiles the production `src/wake.cpp` with simulated USB, Bluetooth,
and clock functions. No Pico SDK or controller is required.

From the repository root, with a C++20 compiler:

```sh
mkdir -p build
c++ -std=c++20 -Itests/wake_stubs tests/wake_recovery.cpp -o build/wake-recovery-test
./build/wake-recovery-test
```

Or from a Visual Studio developer command prompt:

```bat
if not exist build mkdir build
cl /nologo /EHsc /std:c++20 /Itests\wake_stubs tests\wake_recovery.cpp /Febuild\wake-recovery-test.exe /Fobuild\wake-recovery-test.obj
build\wake-recovery-test.exe
```

Covers ordinary sleep, slow wake, repeated suspend/resume, remount/unmount,
interrupted report streams, keyboard-only enumeration, failed wake timeout,
disabled wake, Bluetooth disconnect, and bounded USB-only repair of an active
interface with no controller reports (including a stuck reconfiguration flag).
Checks that repair never runs while suspended, unmounted, or outside recovery.
USB transfer completion is simulated;
firmware compilation and real hardware testing are still required.
