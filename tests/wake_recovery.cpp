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
void check(bool ok, const char *msg) { if (!ok) { std::fprintf(stderr,"FAIL: %s\n",msg); std::exit(1); } }
void reset() {
 clock_us=1000000; disconnects=0; forced_wakes=0; remote_ok=true;
 config.enable_wake=true; host_suspended=false; host_resumed_event=false;
 suspend_at_us=0; reconnect_until_us=0; wake_in_progress=false;
 wake_resumed_at_us=0; suspend_debounce_us=WAKE_DISCONNECT_DEBOUNCE_US;
 state=WAKE_IDLE; state_entered_us=0; key_attempts=0; usb_reconnect_calls=0;
 usb_keyboard_only=false; usb_reconfiguring=false; wake_init();
}
void step(uint64_t us) { clock_us+=us; wake_task(); }
void start() { tud_suspend_cb(true); wake_on_bt_connect(); }
int main() {
 reset(); tud_suspend_cb(true); step(2999999); check(!disconnects,"sleep before 3s");
 step(1); check(disconnects==1,"sleep at 3s");
 reset(); start(); step(29999999); check(!disconnects,"wake before 30s");
 step(1); check(disconnects==1,"failed wake at 30s");
 reset(); start(); step(20000000); tud_resume_cb(); step(1000000); tud_suspend_cb(true);
 step(29999999); check(!disconnects,"resuspend gets full 30s even past request deadline");
 step(1); check(disconnects==1,"resuspend deadline");
 reset(); start(); step(25000000); tud_resume_cb(); tud_mount_cb();
 step(1000000); tud_suspend_cb(true); step(25000000); tud_resume_cb();
 step(1000000); tud_suspend_cb(true); step(25000000);
 check(!disconnects,"repeated resume/suspend can exceed 60s total");
 tud_resume_cb(); step(30000000); check(!wake_in_progress,"30s resumed returns to normal");
 tud_suspend_cb(true); step(3000000); check(disconnects==1,"later sleep takes 3s");
 reset(); tud_suspend_cb(true); clock_us+=3600000000ULL; wake_on_bt_connect();
 step(29999999); check(!disconnects,"old suspend reset on new wake");
 step(1); check(disconnects==1,"old suspend bounded from new wake");
 reset(); start(); step(6000000); wake_on_bt_connect(); step(24000000);
 check(disconnects==1,"retries without USB event do not extend timeout");
 reset(); start(); tud_resume_cb(); step(29000000); tud_suspend_cb(true);
 step(29999999); check(!disconnects,"late suspend retains chosen 30s debounce");
 step(1); check(disconnects==1,"late suspend expires");
 reset(); start(); wake_on_bt_disconnect(); tud_suspend_cb(true); step(3000000);
 check(disconnects==1 && !wake_in_progress,"BT disconnect resets wake mode");
 reset(); config.enable_wake=false; start(); step(3000000);
 check(disconnects==1 && !wake_in_progress,"disabled wake uses 3s");
 reset(); remote_ok=false; request_host_wake("awake"); check(!wake_in_progress,"rejected request");
 start(); check(forced_wakes==1 && wake_in_progress,"forced remote wake");
 reset(); start(); tud_resume_cb(); step(120000000);
 check(!disconnects && !usb_reconnect_calls,"no automatic USB reconnect or report dependency");
 std::puts("PASS: 3s sleep / 30s per-suspend wake debounce");
}
