#include <cstdint>
#include <cstdio>
#include <cstdlib>
#define ENABLE_WAKE_HID
#define DS5_BRIDGE_CONFIG_H
struct Config { bool enable_wake = true; } config;
const Config &get_config() { return config; }
uint64_t clock_us = 1000000;
uint64_t time_us_64() { return clock_us; }
void sleep_ms(unsigned ms) { clock_us += ms * 1000ULL; }
struct critical_section_t {};
void critical_section_init(critical_section_t *) {}
void critical_section_enter_blocking(critical_section_t *) {}
void critical_section_exit(critical_section_t *) {}
bool mounted = true, suspended = false, connected = true, remote_ok = true;
int poweroffs = 0, usb_disconnects = 0, usb_connects = 0, forced_wakes = 0;
bool tud_mounted() { return mounted; }
bool tud_suspended() { return suspended; }
bool tud_remote_wakeup() { return remote_ok; }
void dcd_remote_wakeup(int) { ++forced_wakes; }
bool tud_hid_n_ready(int) { return true; }
bool tud_hid_n_report(int, int, const void *, unsigned) { return true; }
void tud_disconnect() { ++usb_disconnects; mounted = false; }
void tud_connect() { ++usb_connects; }
bool bt_is_connected() { return connected; }
void bt_power_off_controller() { ++poweroffs; }
void ps_shortcut_reset() {}
#include "../src/wake.cpp"
void check(bool ok, const char *message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
void reset() {
    clock_us = 1000000; mounted = true; suspended = false; connected = true;
    remote_ok = true; config.enable_wake = true;
    poweroffs = usb_disconnects = usb_connects = forced_wakes = 0;
    host_suspended = false; host_resumed_event = false;
    suspend_at_us = reconnect_until_us = 0;
    recovery_state = RECOVERY_IDLE;
    recovery_started_us = recovery_stable_since_us = recovery_last_report_us = 0;
    recovery_reconnect_after_us = 0; recovery_reconnects = 0;
    state = WAKE_IDLE; state_entered_us = 0; key_attempts = 0;
    wake_init();
}
void suspend_usb() { suspended = true; tud_suspend_cb(true); }
void resume_usb() { suspended = false; tud_resume_cb(); }
void mount_usb() { mounted = true; suspended = false; tud_mount_cb(); }
void advance(uint64_t us) { clock_us += us; wake_task(); }
void start() { suspend_usb(); wake_on_bt_connect(); }
void reports(unsigned seconds) {
    wake_on_usb_report_complete(0);
    for (unsigned i = 0; i < seconds * 10; ++i) {
        clock_us += 100000; wake_on_usb_report_complete(0); wake_task();
    }
}
int main() {
    reset(); suspend_usb(); advance(2999999); check(!poweroffs, "sleep before 3s");
    advance(1); check(poweroffs == 1, "normal sleep powers off at 3s");
    connected = false; wake_on_bt_disconnect(); advance(60000000);
    resume_usb(); advance(10000000); suspend_usb(); advance(60000000);
    check(recovery_state == RECOVERY_IDLE && !usb_disconnects,
          "Sleep to Hibernate without a controller wake never starts recovery");

    reset(); start(); advance(60000000);
    check(!poweroffs && !usb_disconnects, "slow wake protects BT without USB reconnect while asleep");
    resume_usb(); reports(9); check(recovery_state == RECOVERY_VERIFY_USB, "need full stability interval");
    reports(1); check(recovery_state == RECOVERY_IDLE && !usb_disconnects, "healthy stream completes without reconnect");
    suspend_usb(); advance(3000000); check(poweroffs == 1, "later sleep uses 3s");

    reset(); start(); resume_usb(); reports(9); suspend_usb();
    advance(20000000); check(!poweroffs && recovery_state == RECOVERY_WAIT_USB, "resuspend keeps protection");
    resume_usb(); reports(9); check(recovery_state != RECOVERY_IDLE, "resuspend resets evidence");
    reports(1); check(recovery_state == RECOVERY_IDLE, "new stream completes recovery");

    reset(); start(); resume_usb(); reports(9); advance(1000000);
    check(recovery_state == RECOVERY_WAIT_USB, "report gap resets stability");
    reports(9); tud_umount_cb(); mounted = false;
    wake_on_usb_report_complete(0); check(recovery_state == RECOVERY_WAIT_USB, "unmounted completion ignored");
    advance(10000000); check(!usb_disconnects, "no repair before enumeration");

    reset(); start(); resume_usb(); advance(4999999);
    check(!usb_disconnects, "repair waits five seconds");
    advance(1); check(usb_disconnects == 1 && usb_connects == 1 && !poweroffs, "USB-only repair");
    mount_usb(); reports(10);
    check(recovery_state == RECOVERY_IDLE && usb_disconnects == 1, "successful repair stops retries");

    reset(); start(); resume_usb(); advance(5000000); mount_usb();
    advance(9000000); check(usb_disconnects == 1, "repair backoff");
    advance(1000000); mount_usb(); advance(10000000); mount_usb(); advance(10000000);
    check(usb_disconnects == 3, "maximum three attempts");

    reset(); start(); resume_usb(); connected = false; advance(10000000);
    check(!usb_disconnects, "no repair without BT connection");
    wake_on_bt_disconnect(); check(recovery_state == RECOVERY_IDLE, "BT disconnect cancels recovery");
    reset(); start(); advance(6000000); wake_on_bt_connect();
    clock_us = recovery_started_us + WAKE_RECOVERY_TIMEOUT_US - 1; wake_task();
    check(!poweroffs, "retries do not reset timeout");
    advance(1); check(poweroffs == 1 && recovery_state == RECOVERY_IDLE, "failed wake bounded at 120s");

    reset(); config.enable_wake = false; start(); advance(3000000);
    check(poweroffs == 1 && recovery_state == RECOVERY_IDLE, "disabled wake retains power saving");
    reset(); remote_ok = false; request_host_wake("awake");
    check(recovery_state == RECOVERY_IDLE, "rejected request ignored");
    start(); check(forced_wakes == 1 && recovery_state != RECOVERY_IDLE, "forced DCD wake protects");
    reset(); start(); resume_usb();
    for (int i=0; i<4; ++i) { wake_on_usb_report_complete(1); advance(1000000); }
    check(recovery_state == RECOVERY_WAIT_USB, "keyboard completions are not controller reports");
    std::puts("PASS: release-based recovery, power-off and USB retry scenarios");
}
