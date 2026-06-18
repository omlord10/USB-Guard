/*
 * arduino_main.ino — USBGuard v5 (combined)
 *
 * Посекторный скан USB-флешки + управление смещением через
 * кнопки Tang Nano или через Serial Monitor.
 *
 * Вывод дублируется в Serial (PC) и SoftwareSerial (Tang/HDMI).
 *
 * Управление (Serial Monitor или кнопки Tang):
 *   'h'   (btn0/P26) -> offset += 100
 *   'k'   (btn1/P27) -> offset += 10 000
 *   'm'   (btn2/P28) -> offset += 1 000 000
 *   's'   (S1/P4)    -> начать скан с текущего offset
 *   "res" (S2/P3)    -> сброс offset = 0
 *   'g'              -> toggle green LED (Tang)
 *   'r'              -> toggle red LED   (Tang)
 *   '0'..'5'         -> toggle LEDs 1-6  (Tang)
 *   (любая клавиша во время скана = отмена)
 *
 * Протокол LED: 0x01 (ESC) + байт команды.
 * Обычные символы -> HDMI-терминал (LED не затрагиваются).
 *
 * Статусы -> Tang:
 *   'R' READY  'N' NO_MEDIA  'W' WORKING
 *   'G' GOOD   'B' BAD       'E' ERROR
 *
 * Пины: D0/D1 Serial(PC,115200), D2/D3 SoftSerial(Tang,9600), D9-D13 USB Host
 */

#include <SoftwareSerial.h>
#include <Usb.h>
#include <masstorage.h>

extern "C" {
    #include "signature_engine.h"
}
#include "signatures.h"

/* ============ Макрос дублированного вывода ============ */
#define DP(x)   do { Serial.print(x);   tang.print(x);   } while(0)
#define DPLN(x) do { Serial.println(x); tang.println(x); } while(0)

/* ESC-протокол: 0x01 + команда -> LED/статус на FPGA */
#define TANG_CMD_ESC  0x01

static inline void tang_cmd(uint8_t c)
{
    tang.write(TANG_CMD_ESC);
    tang.write(c);
}

/* ==================== Объекты ==================== */
SoftwareSerial tang(2, 3);   // RX=D2, TX=D3

USB      Usb;
BulkOnly bulk(&Usb);

static uint8_t   g_buf[512];
static ScanState g_scan;

/* Состояние */
static uint32_t g_offset   = 0;
static uint32_t g_capacity = 0;
static bool     g_detected = false;
static bool     g_scanning = false;

/* Буферы команд (линейные, до \n) */
static char    ser_buf[16];
static uint8_t ser_idx = 0;
static char    tang_buf[16];
static uint8_t tang_idx = 0;

/* ==================== Таймстамп ==================== */
static void log_ts(void)
{
    unsigned long s = millis() / 1000UL;
    uint8_t mm = (uint8_t)((s / 60UL) % 100UL);
    uint8_t ss = (uint8_t)(s % 60UL);
    DP('[');
    if (mm < 10) DP('0');
    DP(mm);
    DP(':');
    if (ss < 10) DP('0');
    DP(ss);
    DP(F("] "));
}

/* ==================== Статус -> Tang (через ESC-протокол) ==================== */
static void send_status(uint8_t s)
{
    tang_cmd(s);
    log_ts();
    DP(F(">> status: "));
    DPLN((char)s);
}

/* ==================== Отобразить offset ==================== */
static void show_offset(void)
{
    log_ts();
    DP(F("offset: "));
    DP(g_offset);
    DP(F(" / "));
    DPLN(g_capacity);
}

/* ==================== Скан ==================== */
static void do_scan(void)
{
    g_scanning = true;

    log_ts();
    DP(F("SCAN from sector "));
    DP(g_offset);
    DP(F(" / "));
    DPLN(g_capacity);

    send_status('W');

    if (scan_init(&g_scan) != SI_SUCCESS)
    {
        log_ts(); DPLN(F("err: scan_init"));
        send_status('E');
        g_scanning = false;
        return;
    }

    /* flush Serial (остатки от Enter) */
    delay(50);
    while (Serial.available()) Serial.read();

    unsigned long t0 = millis();
    uint32_t sec;

    for (sec = g_offset; sec < g_capacity; sec++)
    {
        /* отмена по любой клавише из Serial Monitor */
        if (Serial.available() > 0)
        {
            while (Serial.available()) Serial.read();
            DPLN();
            log_ts();
            DP(F("ABORTED at sector "));
            DPLN(sec);
            break;
        }

        /* отмена по любому символу от Tang */
        if (tang.available() > 0)
        {
            while (tang.available()) tang.read();
            DPLN();
            log_ts();
            DP(F("ABORTED at sector "));
            DPLN(sec);
            break;
        }

        /* прогресс каждые 500 секторов */
        if ((sec - g_offset) % 500 == 0)
        {
            log_ts();
            DP(sec);
            DP(F(" / "));
            DPLN(g_capacity);
        }

        /* чтение сектора */
        uint8_t rc = bulk.Read(0, sec, 512, 1, g_buf);
        if (rc != 0)
        {
            log_ts();
            DP(F("read err sector "));
            DP(sec);
            DP(F(" rc="));
            DPLN(rc);
            continue;
        }

        /* скан */
        int flag = 0;
        const char *threat = "(none)";
        int sr = scan_feed(&g_scan, g_buf, 512U,
                           SIGNATURES, SIGNATURES_COUNT, &flag, &threat);

        if (sr != SF_SUCCESS)
        {
            log_ts(); DPLN(F("err: scan_feed"));
            send_status('E');
            g_scanning = false;
            return;
        }

        if (flag == 1)
        {
            DPLN();
            log_ts(); DPLN(F("=============================="));
            log_ts(); DP(F("!! THREAT at sector "));  DPLN(sec);
            log_ts(); DP(F("!! Signature: "));         DPLN(threat);
            log_ts(); DPLN(F("=============================="));
            DPLN();

            unsigned long dt = (millis() - t0) / 1000UL;
            log_ts();
            DP(sec - g_offset + 1);
            DP(F(" sectors in "));
            DP(dt);
            DPLN(F(" sec"));

            send_status('B');
            g_scanning = false;
            return;
        }
    }

    /* дошли до конца или abort */
    DPLN();
    unsigned long dt = (millis() - t0) / 1000UL;
    log_ts(); DPLN(F("CLEAN - no threats"));
    log_ts();
    DP(sec - g_offset);
    DP(F(" sectors in "));
    DP(dt);
    DPLN(F(" sec"));

    send_status('G');
    g_scanning = false;
}

/* ==================== Обработка однобайтовой команды ==================== */
static void handle_char_cmd(char c)
{
    switch (c)
    {
        case 'h':
            g_offset += 100UL;
            if (g_capacity > 0 && g_offset > g_capacity) g_offset = g_capacity;
            show_offset();
            break;
        case 'k':
            g_offset += 10000UL;
            if (g_capacity > 0 && g_offset > g_capacity) g_offset = g_capacity;
            show_offset();
            break;
        case 'm':
            g_offset += 1000000UL;
            if (g_capacity > 0 && g_offset > g_capacity) g_offset = g_capacity;
            show_offset();
            break;
        case 's':
            if (g_detected && !g_scanning) do_scan();
            else {
                log_ts();
                if (!g_detected) DPLN(F("No USB drive"));
                else             DPLN(F("Already scanning"));
            }
            break;

        /* LED-управление — через ESC-протокол */
        case 'g': case 'r':
        case '0': case '1': case '2':
        case '3': case '4': case '5':
            tang_cmd((uint8_t)c);
            break;
    }
}

/* ==================== Обработка строковой команды ==================== */
static void process_cmd(const char *cmd)
{
    if (strcmp_P(cmd, PSTR("res")) == 0)
    {
        g_offset = 0;
        log_ts(); DPLN(F("offset RESET -> 0"));
    }
    else if (strlen(cmd) == 1)
    {
        handle_char_cmd(cmd[0]);
    }
    else
    {
        log_ts();
        DP(F("unknown cmd: "));
        DPLN(cmd);
    }
}

/* ==================== setup ==================== */
void setup()
{
    Serial.begin(115200);
    tang.begin(9600);

    DPLN();
    DPLN(F("================================"));
    DPLN(F("  USBGuard v5 - Sector Scanner"));
    DPLN(F("================================"));
    DPLN(F("Commands:"));
    DPLN(F("  h   = offset +100       (btn1)"));
    DPLN(F("  k   = offset +10 000    (btn2)"));
    DPLN(F("  m   = offset +1 000 000 (btn3)"));
    DPLN(F("  s   = SCAN from offset  (S1)"));
    DPLN(F("  res = reset offset to 0 (S2)"));
    DPLN(F("  g/r = toggle green/red LED"));
    DPLN(F("  0-5 = toggle LEDs 1-6"));
    DPLN(F("  (any key during scan = abort)"));
    DPLN(F("================================"));
    DPLN();

    if (Usb.Init() == -1)
    {
        log_ts(); DPLN(F("ERR: USB Host Shield init"));
        send_status('E');
        while (1) ;
    }

    log_ts(); DPLN(F("USB Host Shield OK"));
    log_ts(); DPLN(F("Insert USB flash drive..."));
    send_status('N');
}

/* ==================== loop ==================== */
void loop()
{
    Usb.Task();

    bool present = (bulk.GetAddress() != 0) && bulk.LUNIsGood(0);

    /* флешка появилась */
    if (present && !g_detected)
    {
        g_detected = true;
        g_capacity = bulk.GetCapacity(0);
        g_offset   = 0;

        log_ts(); DPLN(F("USB drive detected"));
        log_ts();
        DP(F("Capacity: "));
        DP(g_capacity);
        DP(F(" sectors ("));
        DP(g_capacity / 2048UL);
        DPLN(F(" MB)"));

        show_offset();
        log_ts(); DPLN(F("Set offset, then 's' to scan"));

        send_status('R');
    }

    /* флешка вытащена */
    if (!present && g_detected)
    {
        g_detected = false;
        g_capacity = 0;
        g_offset   = 0;
        log_ts(); DPLN(F("USB drive removed"));
        send_status('N');
    }

    /* команды из Serial Monitor (буферизация до \n) */
    while (Serial.available() > 0)
    {
        char c = (char)Serial.read();
        if (c == '\r') continue;
        if (c == '\n')
        {
            ser_buf[ser_idx] = '\0';
            if (ser_idx > 0) process_cmd(ser_buf);
            ser_idx = 0;
        }
        else
        {
            if (ser_idx < sizeof(ser_buf) - 1)
                ser_buf[ser_idx++] = c;
        }
    }

    /* команды от Tang Nano (буферизация до \n) */
    while (tang.available() > 0)
    {
        char c = (char)tang.read();
        if (c == '\r') continue;
        if (c == '\n')
        {
            tang_buf[tang_idx] = '\0';
            if (tang_idx > 0) process_cmd(tang_buf);
            tang_idx = 0;
        }
        else
        {
            if (tang_idx < sizeof(tang_buf) - 1)
                tang_buf[tang_idx++] = c;
        }
    }
}
