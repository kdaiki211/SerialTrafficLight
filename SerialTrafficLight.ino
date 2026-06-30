/**
 * @file    SerialTrafficLight.ino
 * @brief   シリアル経由で受信したコマンドに応じてミニ信号機の LED を制御する
 * @author  Daiki Komatsuda
 * @date    2026-06-28
 * ===== Arduino IDE の設定 =====
 * Board                  : DxCore > AVR DD-series (Optiboot)
 * Chip                   : AVR32DD20
 * Clock                  : 24 MHz internal
 * Bootloader Serial Port : USARTO (default pins): TX PAO, RX PA1 (int. clock only)
 * Port                   : /dev/cu.usbserial-10 (例)
 * ==============================
 */

// スリープから復帰直後に Serial 受信文字が数個欠けないようにするためスリープ中にオシレータを駆動させたままにもできるがその分スリープ中の消費電力が上がる
// #define ENABLE_OSCHF_RUNSTDBY_FOR_STABILITY

#include <avr/sleep.h>
#ifdef ENABLE_OSCHF_RUNSTDBY_FOR_STABILITY
#include <avr/io.h>
#endif

constexpr long baud_rate = 115200;
constexpr bool verbose = false;

constexpr pin_size_t pin_r = 2;
constexpr pin_size_t pin_y = 3;
constexpr pin_size_t pin_b = 4;
constexpr uint8_t led_off = LOW;
constexpr uint8_t led_on  = HIGH;

void setup() {
  Serial.begin(baud_rate);
  set_sleep_mode(SLEEP_MODE_STANDBY);

  // LED 用の pin
  pinMode(pin_r, OUTPUT);
  pinMode(pin_y, OUTPUT);
  pinMode(pin_b, OUTPUT);

#ifdef ENABLE_OSCHF_RUNSTDBY_FOR_STABILITY
  uint8_t current_oschfctrla = CLKCTRL.OSCHFCTRLA;
  _PROTECTED_WRITE(CLKCTRL.OSCHFCTRLA, current_oschfctrla | CLKCTRL_RUNSTDBY_bm);
#endif
}

// シリアル受信バッファ
constexpr uint8_t buf_len = 32;
char buf[buf_len];
uint8_t buf_wptr = 0;
const char* arg_delimiter = " ";

// スリープまでのタイムアウト時間
constexpr unsigned long rx_idle_timeout_us = 50ul * 1000ul;
unsigned long t0 = 0ul;

bool read_char() {
  bool is_cmd_ready = false;
  const char c = Serial.read();
  if (c == '\r') {
    return false; // CR は無視
  } else if (c == '\n' || c == '\0') {
    is_cmd_ready = true;
  } else {
    buf[buf_wptr++] = c;
    if (buf_wptr == buf_len - 1) {
      is_cmd_ready = true;
    }
  }
  if (is_cmd_ready) {
    buf[buf_wptr] = '\0';
    buf_wptr = 0;
  }
  return is_cmd_ready;
}

void show_usage() {
  Serial.println("Available commands:\n" \
                 "  - help\n" \
                 "  - control [red] [yellow] [blue]\n" \
                 "    - [red/yellow/blue] on / off\n" \
                 "  - status");
}

void control_led(const bool r, const bool y, const bool b) {
  digitalWrite(pin_r, r ? led_on : led_off);
  digitalWrite(pin_y, y ? led_on : led_off);
  digitalWrite(pin_b, b ? led_on : led_off);
  Serial.println("control_led() done.");
}

void show_status() {
  Serial.println("Not implemented: show_status()");
}

bool parse_on_off(bool& result) {
  char* arg = strtok(NULL, arg_delimiter);
  strlwr(arg);
  if (strcmp(arg, "on") == 0) {
    result = true;
    return true;
  } else if (strcmp(arg, "off") == 0) {
    result = false;
    return true;
  }
  return false;
}

void execute_command() {
  bool is_valid_cmd = true;
  bool is_valid_arg = true;

  // strtok に渡したバッファは書き換えられるため buf のコピーを用意
  char buf_copy[buf_len];
  strcpy(buf_copy, buf);
  char* cmd = strtok(buf_copy, arg_delimiter);
  
  // コマンドを parse して実行
  if (cmd) {
    strlwr(cmd);
    if (strcmp(cmd, "control") == 0) {
      bool r, y, b;
      is_valid_arg &= parse_on_off(r);
      is_valid_arg &= parse_on_off(y);
      is_valid_arg &= parse_on_off(b);
      if (is_valid_arg) {
        control_led(r, y, b);
      }
    } else if (strcmp(cmd, "status") == 0) {
      show_status();
    } else if (strcmp(cmd, "help") == 0) {
      show_usage();
    } else {
      is_valid_cmd = false;
    }
  }

  if (!is_valid_cmd || !is_valid_arg) {
    Serial.print("Invalid command or argument: ");
    Serial.println(buf);
    Serial.println("For a list of available commands, type 'help'.");
  }
}

void sleep() {
  // スリープへ突入
  verbose && Serial.println("Going to sleep...");
  Serial.flush();
  USART0.CTRLB |= USART_SFDEN_bm;  // SFD (Start Frame Detection) 有効化
  sleep_enable();
  sleep_cpu();

  // スリープから復帰: 復帰後は Serial の受信文字が 4 文字程度欠けることに注意。送信側が \n\n\n\n (無害な文字列) に続いてコマンドを入力する必要がある
  sleep_disable();
  USART0.CTRLB &= ~USART_SFDEN_bm;  // SFD 無効化
  USART0.STATUS = USART_RXSIF_bm;   // USART Receive Start Interrupt Flag クリア (RW1C)

  verbose && Serial.println("Woke up.");
}

void loop() {
  if (Serial.available()) {
    if (read_char()) {
      execute_command();
      Serial.flush();
    }
    t0 = micros();
  } else if (micros() - t0 >= rx_idle_timeout_us) {
    sleep();
    t0 = micros();
  }
}
 