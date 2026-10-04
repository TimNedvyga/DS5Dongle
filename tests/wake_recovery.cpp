#include <cstdint>
#include <cstdio>
#include <cstdlib>
#define ENABLE_WAKE_HID
#define ENABLE_SERIAL 0
#define DS5_BRIDGE_CONFIG_H
struct Config { bool enable_wake = true; } config;
const Config &get_config() { return config; }
uint64_t clock_us = 1;
uint64_t time_us_64() { return clock_us; }
struct critical_section_t {};
void critical_section_init(critical_section_t *) {}
void critical_section_enter_blocking(critical_section_t *) {}
void critical_section_exit(critical_section_t *) {}
bool remote_ok = true;
int disconnects = 0, forced_wakes = 0;
bool tud_remote_wakeup() { return remote_ok; }
void dcd_remote_wakeup(int) { ++forced_wakes; }
bool tud_hid_n_ready(int) { return true; }
bool tud_hid_n_report(int, int, const void *, unsigned) { return true; }
void tud_disconnect() {}
bool bt_is_connected() { return true; }
bool bt_disconnect() { ++disconnects; return true; }
void ps_shortcut_reset() {}
bool usb_keyboard_only = false, usb_reconfiguring = false;
uint8_t usb_keyboard_instance() { return usb_keyboard_only ? 0 : 1; }
int usb_reconnect_calls = 0;
void wake_note_usb_reconnect(void);
void usb_reconnect(bool keyboard_only) {
    ++usb_reconnect_calls;
    wake_note_usb_reconnect();
    usb_keyboard_only = keyboard_only;
    usb_reconfiguring = true;
}
bool mounted = true, suspended = false;
bool tud_mounted() { return mounted; }
bool tud_suspended() { return suspended; }
#include "../src/wake.cpp"

void check(bool ok, const char *message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
void reset() {
    clock_us = 1000000; disconnects = 0; forced_wakes = 0;
    remote_ok = true; config.enable_wake = true;
    mounted = true; suspended = false;
    usb_keyboard_only = false; usb_reconfiguring = false;
    host_suspended = false; host_resumed_event = false;
    suspend_at_us = 0; reconnect_until_us = 0;
    recovery_state = RECOVERY_IDLE;
    recovery_started_us = recovery_stable_since_us = recovery_last_report_us = 0;
    recovery_reconnect_after_us = 0; recovery_reconnects = 0; usb_reconnect_calls = 0;
    state = WAKE_IDLE; state_entered_us = 0; key_attempts = 0;
    wake_init();
}
void suspend_usb() { suspended = true; tud_suspend_cb(true); }
void resume_usb() { suspended = false; tud_resume_cb(); }
void advance(uint64_t us) { clock_us += us; wake_task(); }
void reports(unsigned seconds) {
    // 10Hz is deliberately much slower than real gamepad input polling.
    wake_on_usb_report_complete(0);
    for (unsigned i = 0; i < seconds * 10; ++i) {
        clock_us += 100000;
        wake_on_usb_report_complete(0);
        wake_task();
    }
}
void start_wake() { suspend_usb(); wake_on_bt_connect(); }
int main() {
    reset(); suspend_usb(); advance(2999999);
    check(disconnects == 0, "normal sleep before 3s");
    advance(1); check(disconnects == 1, "normal sleep at 3s");

    reset(); start_wake(); advance(60000000);
    check(disconnects == 0 && recovery_state != RECOVERY_IDLE, "slow wake protected beyond 15s");
    resume_usb(); tud_mount_cb(); advance(10000000);
    check(recovery_state != RECOVERY_IDLE, "mount alone is not recovery");
    wake_on_usb_report_complete(1); advance(10000000);
    check(recovery_state == RECOVERY_WAIT_USB, "keyboard reports do not prove controller recovery");
    tud_mount_cb(); // Complete any repair re-enumeration before host polling.
    reports(9); check(recovery_state == RECOVERY_VERIFY_USB, "not stable before 10s");
    reports(1); check(recovery_state == RECOVERY_IDLE && disconnects == 0, "stable report stream ends recovery");
    suspend_usb(); advance(3000000); check(disconnects == 1, "later sleep uses 3s");

    reset(); start_wake(); resume_usb(); reports(9); suspend_usb();
    check(recovery_state == RECOVERY_WAIT_USB && host_suspended, "resuspend resets evidence and updates USB state");
    advance(20000000); check(disconnects == 0, "resuspend remains protected");
    resume_usb(); reports(9); check(recovery_state != RECOVERY_IDLE, "new stability window after resuspend");
    reports(1); check(recovery_state == RECOVERY_IDLE, "new window completes");

    reset(); start_wake(); resume_usb(); reports(9); advance(1000000);
    check(recovery_state == RECOVERY_WAIT_USB, "report gap resets stability");
    reports(9); check(recovery_state != RECOVERY_IDLE, "gap needs another full window");
    tud_mount_cb(); check(recovery_state == RECOVERY_WAIT_USB, "remount resets stability");
    reports(9); mounted = false; tud_umount_cb();
    check(recovery_state == RECOVERY_WAIT_USB, "unmount resets stability");
    wake_on_usb_report_complete(0); check(recovery_state == RECOVERY_WAIT_USB, "unmounted completion ignored");
    mounted = true; reports(9); usb_reconfiguring = true; advance(1);
    check(recovery_state == RECOVERY_WAIT_USB, "reconfiguration invalidates stream");
    usb_reconfiguring = false; usb_keyboard_only = true; reports(11);
    check(recovery_state == RECOVERY_WAIT_USB, "keyboard-only enumeration does not count");

    reset(); start_wake(); const auto started = recovery_started_us;
    advance(60000000); wake_on_bt_connect();
    check(recovery_started_us == started, "wake retries do not extend emergency timeout");
    clock_us = started + WAKE_RECOVERY_TIMEOUT_US - 1; wake_task();
    check(disconnects == 0, "protected before emergency deadline");
    advance(1); check(disconnects == 1 && recovery_state == RECOVERY_IDLE, "stuck wake disconnects at 120s");

    reset(); start_wake(); resume_usb(); advance(WAKE_RECOVERY_TIMEOUT_US);
    check(disconnects == 0 && recovery_state == RECOVERY_IDLE, "timeout never disconnects awake host");
    reset(); config.enable_wake = false; start_wake(); advance(3000000);
    check(disconnects == 1 && recovery_state == RECOVERY_IDLE, "disabled wake keeps normal sleep behavior");
    reset(); remote_ok = false; request_host_wake("test awake");
    check(recovery_state == RECOVERY_IDLE, "rejected request does not protect");
    start_wake(); check(forced_wakes == 1 && recovery_state != RECOVERY_IDLE, "forced DCD wake starts recovery");
    wake_on_bt_disconnect(); check(recovery_state == RECOVERY_IDLE, "BT disconnect clears recovery");
    reset(); start_wake(); resume_usb(); usb_reconfiguring = true;
    advance(WAKE_RECOVERY_NO_REPORT_US - 1);
    check(usb_reconnect_calls == 0, "no repair before observation window");
    advance(1);
    check(usb_reconnect_calls == 1 && disconnects == 0, "repair stuck reconfiguring without dropping BT");
    tud_mount_cb(); reports(10);
    check(recovery_state == RECOVERY_IDLE && usb_reconnect_calls == 1, "reports stop repairs and complete recovery");

    reset(); start_wake(); resume_usb();
    advance(WAKE_RECOVERY_NO_REPORT_US);
    advance(WAKE_RECOVERY_RETRY_US - 1);
    check(usb_reconnect_calls == 1, "retry interval respected");
    advance(1); advance(WAKE_RECOVERY_RETRY_US); advance(WAKE_RECOVERY_RETRY_US);
    check(usb_reconnect_calls == 3, "USB repair attempts bounded");
    reset(); start_wake(); advance(30000000);
    check(usb_reconnect_calls == 0, "never reconnect a sleeping host");
    resume_usb(); mounted = false; advance(30000000);
    check(usb_reconnect_calls == 0, "never retry before enumeration");
    reset(); advance(30000000);
    check(usb_reconnect_calls == 0, "no repair outside wake recovery");
    std::puts("PASS: USB wake recovery scenarios");
}
