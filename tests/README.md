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

Covers the 3s ordinary sleep debounce, 30s from each wake-related suspend,
repeated USB transitions lasting more than a minute, old suspend timestamps,
retry bounds, return to normal after 30s continuously resumed, disabled wake,
and Bluetooth disconnect. There is no report-stream detector or automatic
USB reconnect in this variant. Firmware compilation and hardware tests of
Sleep, Hibernate, and Sleep-to-Hibernate are still required.
