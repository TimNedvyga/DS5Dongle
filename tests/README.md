# Release-based wake recovery tests

Build from the repository root with a host C++20 compiler:

```sh
mkdir -p build
c++ -std=c++20 -Itests/wake_stubs tests/wake_recovery.cpp -o build/wake-test
./build/wake-test
```

The tests include the production wake.cpp with simulated time, USB, and Bluetooth.
They cover ordinary 3s power-off, Sleep-to-Hibernate without a controller wake,
slow wake, repeated suspend, stable controller transfers, bounded USB-only retry,
120s failed-wake timeout, and disabled wake. They cannot verify host firmware or
Windows power transitions; test Sleep, Hibernate, and Sleep-to-Hibernate on hardware.

This variant is based on v0.7.2-hotfix, retaining its power-off command, USB
descriptors and dependencies. Recovery starts only on a successful controller
wake request. It ends after 10s of completed controller reports with no gap of
1s, or after a 120s safety timeout. An enumerated, awake USB device without
controller reports for 5s may be reconnected up to 3 times, at least 10s apart.
The known-working constant-30s variant remains on release-v0.7.2-hotfix-30s.
