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

void setup() {
  Serial.begin(baud_rate);
  set_sleep_mode(SLEEP_MODE_STANDBY);

#ifdef ENABLE_OSCHF_RUNSTDBY_FOR_STABILITY
  uint8_t current_oschfctrla = CLKCTRL.OSCHFCTRLA;
  _PROTECTED_WRITE(CLKCTRL.OSCHFCTRLA, current_oschfctrla | CLKCTRL_RUNSTDBY_bm);
#endif
}

// シリアル受信バッファ
constexpr uint8_t buf_len = 16;
char buf[buf_len];
uint8_t buf_wptr = 0;

// スリープまでのタイムアウト時間
constexpr unsigned long rx_idle_timeout_us = 50ul * 1000ul;
unsigned long t0 = 0ul;

bool read_char() {
  bool is_cmd_ready = false;
  char c = Serial.read();
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
                 "  - control [R] [G] [B]\n" \
                 "    - [R/G/B] on / off\n" \
                 "  - status");
}

void control_led() {
  Serial.println("Not implemented: control_led()");
}

void show_status() {
  Serial.println("Not implemented: show_status()");
}

void execute_command() {
  bool is_valid_cmd = true;
  char* cmd = strtok(buf, " ");
  
  // コマンドを parse して実行
  if (cmd == NULL) {
    is_valid_cmd = false;
  } else {
    strlwr(cmd);
    if (strcmp(cmd, "control") == 0) {
      control_led();
    } else if (strcmp(cmd, "status") == 0) {
      show_status();
    } else if (strcmp(cmd, "help") == 0) {
      show_usage();
    } else {
      is_valid_cmd = false;
    }
  }

  if (!is_valid_cmd) {
    Serial.print("Invalid command: ");
    Serial.println(cmd);
    Serial.println("For a list of available commands, type 'help'.");
  }
}

void loop() {
  if (Serial.available()) {
    t0 = micros();
    if (read_char()) {
      execute_command();
      Serial.flush();
    }
  } else if (micros() - t0 >= rx_idle_timeout_us) {
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
    t0 = micros();
  }
}
 