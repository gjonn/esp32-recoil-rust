
#include <Arduino.h>
#include <BleMouse.h>
#include <EspUsbHost.h>
#include <math.h>
#include <string.h>

// ================== CONFIG ==================
#define BLE_NAME "Logitech G502X"
#define REQUIRE_RIGHT_HELD 1
#define REPORT_RATE_HZ 130       // 130Hz updates (approx 7.5ms)
#define MAX_BURST 3              // Max packets per tick for fast flicks
#define MMB_DEBOUNCE_MS 250      // Debounce for G502 infinite wheel

// Mouse bitmasks
static constexpr uint8_t USB_BTN_LEFT     = 1 << 0;
static constexpr uint8_t USB_BTN_RIGHT    = 1 << 1;
static constexpr uint8_t USB_BTN_MIDDLE   = 1 << 2;
static constexpr uint8_t USB_BTN_BACK     = 1 << 3;
static constexpr uint8_t USB_BTN_FORWARD  = 1 << 4;

BleMouse bleMouse(BLE_NAME, "Logitech", 100);

// ============ WEAPON DATA (ORIGINAL) ============
struct WeaponConfig {
  const char* name;
  const float* rx; const float* ry; int steps;
  float tbs_ms;
  float holoMul; float x8Mul;
  bool  patternWeapon;
};

// --- ARRAYS REVERTED TO ORIGINAL V2 ---
const float ak47_rx[] = {0,0.194914926,0.391704579,0.511817958,0.648471024,0.764900073,0.80257069,0.806110299,0.907826148,0.907167069,0.869595948,0.901561429,0.839903022,0.902936872,0.963032607,0.874347588,0.825614937,0.95416489,0.914673816,0.903444147,0.883858707,0.943581636,0.915063957,0.857727099,0.936576585,0.91080919,0.936988434,0.91679014,0.936988434,0.936576585};
const float ak47_ry[] = {-1.361448576,-1.387357074,-1.386754677,-1.362197502,-1.401340041,-1.336221468,-1.348643268,-1.337576409,-1.292502762,-1.346553873,-1.369026982,-1.329203754,-1.305987651,-1.313265861,-1.369361745,-1.293973479,-1.36908135,-1.385905402,-1.347744798,-1.349755803,-1.405980126,-1.352300463,-1.388740356,-1.333803438,-1.355569929,-1.378052181,-1.350748341,-1.339769017,-1.350748341,-1.355569929};

const float lr300_rx[] = {0,0.033517,-0.149077,-0.147054,0.057723,0.064947,0.195907,-0.109226,0.097927,-0.176122,0.039039,-0.076399,-0.054613,0.039399,-0.119098,-0.130586,0.025668,-0.039517,0.052087,-0.058688,-0.12846,-0.010727,-0.027483,-0.032145,0.033208,-0.0554,-0.020333,0.160678,0.047691,0.020668};
const float lr300_ry[] = {-1.237727,-1.14431,-1.123523,-1.169826,-1.185951,-1.136969,-1.173873,-1.133846,-1.097525,-1.202949,-1.107805,-1.138392,-1.105178,-1.09091,-1.02865,-1.076927,-1.090242,-1.059847,-1.148118,-1.187485,-1.115724,-1.043713,-1.075563,-1.137415,-1.081225,-1.159009,-1.083084,-1.202353,-1.158462,-1.158462};

const float mp5_rx[] = {0.075593,-0.060027,0.016778,-0.00827,-0.004792,0.057946,-0.0277,0.020693,-0.002393,-0.00567,0.08491,-0.009076,0.057706,-0.162595,0.000172,0.011103,0.059812,0.120394,-0.049844,0.004116,0.055301,-0.065532,0.102276,-0.023061,-0.051564,0.082176,0.118424,-0.091965,-0.12935,0.020668};
const float mp5_ry[] = {-0.634625,-0.561723,-0.575319,-0.513457,-0.645559,-0.613864,-0.479012,-0.670909,-0.560814,-0.535767,-0.585397,-0.63107,-0.518866,-0.626454,-0.506808,-0.625991,-0.513576,-0.538504,-0.644775,-0.53154,-0.694026,-0.582204,-0.662998,-0.675411,-0.528804,-0.631696,-0.627106,-0.729202,-0.576859,-0.576859};

const float thompson_rx[] = {-0.069091,0.005238,0.006218,0.03909,0.062757,-0.053135,0.054213,0.022354,0.107614,0.020906,-0.049843,0.015407,0.049696,-0.074353,0.016983,-0.07076,-0.162073,-0.032011,0.025555,0.025555};
const float thompson_ry[] = {-0.410423,-0.407989,-0.411751,-0.416881,-0.395338,-0.398239,-0.407135,-0.381472,-0.382746,-0.403674,-0.4009,-0.383888,-0.390212,-0.399249,-0.399399,-0.418165,-0.398657,-0.408528,-0.390164,-0.390164};

const float sar_rx[] = {0,0};
const float sar_ry[] = {-0.90,-0.90};

const float m39_rx[] = {0.54,0.54,};
const float m39_ry[] = {-0.95,-0.95,};

const float customsmg_rx[] = {-0.069,0.005,0.006,0.039,0.062,-0.053,0.054,0.022,0.107,0.020,-0.049,0.015,0.049,-0.074,0.017,-0.070,-0.162,-0.032,0.002,0.008,-0.006,0.026,-0.026,0.002};
const float customsmg_ry[] = {-0.410,-0.408,-0.411,-0.416,-0.395,-0.398,-0.407,-0.381,-0.382,-0.403,-0.400,-0.383,-0.390,-0.399,-0.399,-0.418,-0.398,-0.408,-0.390,-0.332,-0.332,-0.348,-0.331,-0.331};

const float hmlmg_rx[] = {0,-0.536,-0.536,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556,-0.556};
const float hmlmg_ry[] = {-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047,-1.047};

const float m249_rx[] = {0,0.393,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525,0.525};
const float m249_ry[] = {-0.81,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08,-1.08};

const float python_rx[] = {0,0};
const float python_ry[] = {-3.5,-3.5};

WeaponConfig WEAPONS[] = {
  {"AK47", ak47_rx, ak47_ry, (int)(sizeof(ak47_rx)/sizeof(float)), 133.3f, 1.2f, 7.3f, false},
  {"LR300", lr300_rx, lr300_ry, (int)(sizeof(lr300_rx)/sizeof(float)), 120.0f, 1.2f, 6.9f, false},
  {"MP5", mp5_rx, mp5_ry, (int)(sizeof(mp5_rx)/sizeof(float)), 100.0f, 1.2f, 6.9f, false},
  {"THOMPSON", thompson_rx, thompson_ry, (int)(sizeof(thompson_rx)/sizeof(float)), 130.0f, 1.48f, 8.4f, false},
  {"SAR", sar_rx, sar_ry, (int)(sizeof(sar_rx)/sizeof(float)), 175.0f, 1.2f, 7.35f, false},
  {"M39", m39_rx, m39_ry, (int)(sizeof(m39_rx)/sizeof(float)), 150.0f, 1.6f, 9.7f, false},
  {"CUSTOMSMG", customsmg_rx, customsmg_ry, (int)(sizeof(customsmg_rx)/sizeof(float)), 100.0f, 1.5f, 7.95f, false},
  {"HMLMG", hmlmg_rx, hmlmg_ry, (int)(sizeof(hmlmg_rx)/sizeof(float)), 125.0f, 1.2f, 7.2f, false},
  {"M249", m249_rx, m249_ry, (int)(sizeof(m249_rx)/sizeof(float)), 120.0f, 1.175f, 6.95f, false},
  {"PYTHON", python_rx, python_ry, (int)(sizeof(python_rx)/sizeof(float)), 150.0f, 1.6f, 9.75f, false},
};
static const int NUM_WEAPONS = sizeof(WEAPONS)/sizeof(WEAPONS[0]);

// ============ State Globals ============
static float SENSITIVITY = 0.78f, ADS_SENSITIVITY = 1.00f, FOV = 90.0f;
static float SMOOTHING_FACTOR = 0.30f, PATTERN_GAIN = 2.2f, CROUCH_MULTIPLIER = 0.50f;
static bool  is_crouching=false, att_holo=false, att_x8=false, att_sil=false, att_mb=false;

static float move_multiplier = 1.0f, shot_scale = 1.0f, cur_tbs = 133.0f;
static int   cur_weapon = 0;

static volatile bool g_recoil_active = false;
static bool g_recoil_enabled = false;
static uint32_t start_ms = 0;
static int step_idx = 0;
static float prev_cum_x=0, prev_cum_y=0, next_cum_x=0, next_cum_y=0;
static float total_moved_x=0, total_moved_y=0;
static float rec_acc_x = 0, rec_acc_y = 0;

// Shared Accumulators
static volatile int32_t shared_dx = 0;
static volatile int32_t shared_dy = 0;
static volatile int8_t  shared_wheel = 0;
static volatile uint8_t shared_buttons = 0; 
static volatile bool    shared_dirty = false;
static uint32_t last_mmb_time = 0;

// ============ Logic ============
static inline float myLerp(float a,float b,float t){ return a+(b-a)*t; }

static inline float scope_mult() {
  float m=1.0f; const auto& W = WEAPONS[cur_weapon];
  if (att_x8) m*=W.x8Mul; else if (att_holo) m*=W.holoMul;
  return m;
}

static void recompute_scale() {
  move_multiplier = 0.03f * SENSITIVITY * ADS_SENSITIVITY * scope_mult() * 3.6f * (FOV/100.0f);
  if (move_multiplier < 1e-4f) move_multiplier = 1e-4f;
  float sil_m = att_sil ? 0.9f : 1.0f;
  shot_scale = (PATTERN_GAIN * sil_m) / move_multiplier;
  cur_tbs = WEAPONS[cur_weapon].tbs_ms * (att_mb ? 0.9f : 1.0f);
  if (cur_tbs < 1.0f) cur_tbs = 1.0f;
}

static void pattern_reset(){
  step_idx=0; prev_cum_x=0; prev_cum_y=0; next_cum_x=0; next_cum_y=0;
  total_moved_x=0; total_moved_y=0; rec_acc_x=0; rec_acc_y=0;
  const auto& W = WEAPONS[cur_weapon];
  if (W.steps>0){
    float c = is_crouching ? CROUCH_MULTIPLIER : 1.0f;
    next_cum_x += (-W.rx[0]) * shot_scale * c;
    next_cum_y += (-W.ry[0]) * shot_scale * c;
  }
}

static void recoil_start(){ if(g_recoil_active) return; g_recoil_active=true; start_ms=millis(); recompute_scale(); pattern_reset(); }
static void recoil_stop(){ g_recoil_active=false; }

// ============ USB Host ============
static uint8_t last_usb_mask = 0;

class MyUsbHost: public EspUsbHost {
public:
  void onMouse(hid_mouse_report_t r, uint8_t) override {
    uint8_t bm = 0;
    if (r.buttons & USB_BTN_LEFT)    bm |= MOUSE_LEFT;
    if (r.buttons & USB_BTN_RIGHT)   bm |= MOUSE_RIGHT;
    if (r.buttons & USB_BTN_MIDDLE)  bm |= MOUSE_MIDDLE;
    if (r.buttons & USB_BTN_BACK)    bm |= MOUSE_BACK;
    if (r.buttons & USB_BTN_FORWARD) bm |= MOUSE_FORWARD;

    bool L = bm & MOUSE_LEFT;
    bool R = bm & MOUSE_RIGHT;
    bool M = bm & MOUSE_MIDDLE;
    bool old_M = last_usb_mask & MOUSE_MIDDLE;

    // Toggle on Middle Click (Debounced)
    if (!old_M && M && (millis() - last_mmb_time > MMB_DEBOUNCE_MS)) {
      g_recoil_enabled = !g_recoil_enabled;
      if (!g_recoil_enabled) recoil_stop();
      last_mmb_time = millis();
    }

    if (g_recoil_enabled) {
      #if REQUIRE_RIGHT_HELD
        bool both = L && R;
        bool old_both = (last_usb_mask & MOUSE_LEFT) && (last_usb_mask & MOUSE_RIGHT);
        if (both && !old_both) recoil_start();
        if (!both && old_both) recoil_stop();
      #else
        bool old_L = last_usb_mask & MOUSE_LEFT;
        if (L && !old_L) recoil_start();
        if (!L && old_L) recoil_stop();
      #endif
    } else {
      if (g_recoil_active) recoil_stop();
    }

    last_usb_mask = bm;

    // Accumulate, do not send yet
    if (r.x != 0) shared_dx += (int8_t)r.x;
    if (r.y != 0) shared_dy += (int8_t)r.y;
    if (r.wheel != 0) shared_wheel += (int8_t)r.wheel;
    if (shared_buttons != bm) { shared_buttons = bm; shared_dirty = true; }
    if (r.x || r.y || r.wheel) shared_dirty = true;
  }
} usb;

// ============ Serial Parsing ============
static char serialBuf[128];
static uint8_t serialPos = 0;

static void print_state() {
  const auto& W = WEAPONS[cur_weapon];
  char att[32]; att[0]='\0';
  if(att_holo) strcat(att, "HOLO,");
  if(att_x8) strcat(att, "X8,");
  if(att_sil) strcat(att, "SIL,");
  if(att_mb) strcat(att, "MB,");
  
  Serial.printf("ST: W=%s T=%.1f S=%.2f A=%.2f F=%.1f G=%.2f SM=%.2f C=%d ON=%d ATT=%s\n",
    W.name, cur_tbs, SENSITIVITY, ADS_SENSITIVITY, FOV, PATTERN_GAIN, SMOOTHING_FACTOR, 
    is_crouching, g_recoil_enabled, att);
}

static void process_command() {
  char* cmd = strtok(serialBuf, ":");
  if (!cmd) return;
  if (strcasecmp(cmd, "PING") == 0) { Serial.println("PONG"); return; }
  if (strcasecmp(cmd, "GET") == 0 || strcasecmp(cmd, "GET:ALL") == 0) { print_state(); return; }

  char* valStr = strtok(NULL, ":");
  if (!valStr) return;

  if (strcasecmp(cmd, "WEAPON") == 0) {
    for (int i=0; i<NUM_WEAPONS; i++) {
      if (strcasecmp(valStr, WEAPONS[i].name) == 0) {
        cur_weapon = i; recompute_scale(); pattern_reset(); break;
      }
    }
  }
  else if (strcasecmp(cmd, "SENS") == 0)   { SENSITIVITY = atof(valStr); recompute_scale(); }
  else if (strcasecmp(cmd, "ADS") == 0)    { ADS_SENSITIVITY = atof(valStr); recompute_scale(); }
  else if (strcasecmp(cmd, "FOV") == 0)    { FOV = atof(valStr); recompute_scale(); }
  else if (strcasecmp(cmd, "SMTH") == 0)   { SMOOTHING_FACTOR = atof(valStr); }
  else if (strcasecmp(cmd, "GAIN") == 0)   { PATTERN_GAIN = atof(valStr); recompute_scale(); }
  else if (strcasecmp(cmd, "CROUCH") == 0) { is_crouching = (atoi(valStr) != 0); }
  else if (strcasecmp(cmd, "CROUCHM") == 0){ CROUCH_MULTIPLIER = atof(valStr); }
  else if (strcasecmp(cmd, "RECOIL") == 0) { g_recoil_enabled = (atoi(valStr) != 0); if(!g_recoil_enabled) recoil_stop(); }
  else if (strcasecmp(cmd, "HOLO") == 0) { att_holo=(atoi(valStr)); if(att_holo) att_x8=0; recompute_scale(); }
  else if (strcasecmp(cmd, "X8") == 0)   { att_x8=(atoi(valStr)); if(att_x8) att_holo=0; recompute_scale(); }
  else if (strcasecmp(cmd, "SIL") == 0)  { att_sil=(atoi(valStr)); recompute_scale(); }
  else if (strcasecmp(cmd, "MB") == 0)   { att_mb=(atoi(valStr)); recompute_scale(); }
  
  print_state();
}

// ============ Main ============
void setup() {
  Serial.begin(115200);
  bleMouse.begin();
  usb.begin();
  recompute_scale();
}

void loop() {
  static uint32_t last_report_time = 0;
  uint32_t now = micros();

  usb.task();

  // --- Recoil Step ---
  if (g_recoil_active && bleMouse.isConnected()) {
    const auto& W = WEAPONS[cur_weapon];
    uint32_t dt = millis() - start_ms;
    
    // Fast forward to current step
    while (((step_idx+1) * cur_tbs) <= dt && (step_idx + 1) < W.steps) {
      prev_cum_x = next_cum_x; prev_cum_y = next_cum_y; step_idx++;
      float c = is_crouching ? CROUCH_MULTIPLIER : 1.0f;
      next_cum_x += (-W.rx[step_idx]) * shot_scale * c;
      next_cum_y += (-W.ry[step_idx]) * shot_scale * c;
    }

    float prog = (dt - (step_idx * cur_tbs)) / cur_tbs;
    if (prog > 1.0f) prog = 1.0f; else if (prog < 0.0f) prog = 0.0f;

    float target_x = myLerp(prev_cum_x, next_cum_x, prog);
    float target_y = myLerp(prev_cum_y, next_cum_y, prog);

    float step_x = (target_x - total_moved_x) * SMOOTHING_FACTOR;
    float step_y = (target_y - total_moved_y) * SMOOTHING_FACTOR;

    total_moved_x += step_x; total_moved_y += step_y;
    rec_acc_x += step_x; rec_acc_y += step_y;

    int px = (int)lroundf(rec_acc_x);
    int py = (int)lroundf(rec_acc_y);
    
    if (px || py) {
      shared_dx += px; shared_dy += py;
      rec_acc_x -= px; rec_acc_y -= py;
      shared_dirty = true;
    }
    
    if (step_idx >= (W.steps-1) && prog >= 0.99f) recoil_stop();
  }

  // --- Output Tick (130Hz) ---
  if (now - last_report_time >= (1000000 / REPORT_RATE_HZ)) {
    last_report_time = now;
    
    if (shared_dirty && bleMouse.isConnected()) {
      int burst = 0;
      while ((shared_dx!=0 || shared_dy!=0 || shared_wheel!=0 || shared_buttons!=last_usb_mask) && burst < MAX_BURST) {
        int8_t mx = (shared_dx > 127) ? 127 : ((shared_dx < -127) ? -127 : shared_dx);
        int8_t my = (shared_dy > 127) ? 127 : ((shared_dy < -127) ? -127 : shared_dy);
        
        shared_dx -= mx; shared_dy -= my;
        int8_t mw = shared_wheel; shared_wheel = 0;
        uint8_t mb = shared_buttons;

        static uint8_t lastSent = 0;
        if (mb != lastSent) {
          if ((mb&MOUSE_LEFT)!=(lastSent&MOUSE_LEFT)) { if(mb&MOUSE_LEFT) bleMouse.press(MOUSE_LEFT); else bleMouse.release(MOUSE_LEFT); }
          if ((mb&MOUSE_RIGHT)!=(lastSent&MOUSE_RIGHT)) { if(mb&MOUSE_RIGHT) bleMouse.press(MOUSE_RIGHT); else bleMouse.release(MOUSE_RIGHT); }
          if ((mb&MOUSE_MIDDLE)!=(lastSent&MOUSE_MIDDLE)) { if(mb&MOUSE_MIDDLE) bleMouse.press(MOUSE_MIDDLE); else bleMouse.release(MOUSE_MIDDLE); }
          if ((mb&MOUSE_BACK)!=(lastSent&MOUSE_BACK)) { if(mb&MOUSE_BACK) bleMouse.press(MOUSE_BACK); else bleMouse.release(MOUSE_BACK); }
          if ((mb&MOUSE_FORWARD)!=(lastSent&MOUSE_FORWARD)) { if(mb&MOUSE_FORWARD) bleMouse.press(MOUSE_FORWARD); else bleMouse.release(MOUSE_FORWARD); }
          lastSent = mb;
        }

        if (mx || my || mw) bleMouse.move(mx, my, mw);
        burst++;
        if (shared_dx == 0 && shared_dy == 0) break;
      }
      if (shared_dx==0 && shared_dy==0 && shared_wheel==0) shared_dirty = false;
    }
  }

  // --- Serial Polling ---
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialPos > 0) { serialBuf[serialPos] = 0; process_command(); serialPos = 0; }
    } else {
      if (serialPos < 127) serialBuf[serialPos++] = c;
    }
  }
}
