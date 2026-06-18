/*
 * test_hdmi.ino — Тестовый скетч для USBGuard v5
 *
 * НЕ требует USB Host Shield.
 * Дублирует вывод в Serial (115200) и tang (9600).
 * Реагирует на команды из Serial Monitor и от Tang.
 *
 * Команды:
 *   h   = offset +100        (btn1/P26)
 *   k   = offset +10 000     (btn2/P27)
 *   m   = offset +1 000 000  (btn3/P28)
 *   s   = имитация скана     (S1/P4)
 *   res = сброс offset       (S2/P3)
 *   g/r = toggle green/red LED на Tang
 *   0-5 = toggle LED 1-6 на Tang
 *
 * Статусы (для проверки LED-паттернов):
 *   R/N/W/G/B/E в Serial Monitor -> отправит в Tang
 *
 * Протокол LED: 0x01 (ESC) + байт команды.
 * Обычные символы -> HDMI-терминал, LED не затрагиваются.
 *
 * На смещении 110 500 секторов лежит EICAR-сигнатура (для теста).
 * Максимальное смещение: 1 000 000 000 секторов.
 */

#include <SoftwareSerial.h>

extern "C" {
    #include "signature_engine.h"
}
#include "signatures.h"

SoftwareSerial tang(2, 3);   // RX=D2, TX=D3

#define DP(x)   do { Serial.print(x);   tang.print(x);   } while(0)
#define DPLN(x) do { Serial.println(x); tang.println(x); } while(0)

/* ESC-протокол: 0x01 + команда -> LED/статус на FPGA */
#define TANG_CMD_ESC  0x01

static inline void tang_cmd(uint8_t c)
{
    tang.write(TANG_CMD_ESC);
    tang.write(c);
}

static uint32_t g_offset   = 0;
static uint32_t g_capacity = 1000000000UL;  /* 1 млрд секторов */

static ScanState g_scan;

static char    ser_buf[16];
static uint8_t ser_idx = 0;
static char    tang_buf[16];
static uint8_t tang_idx = 0;

/* Сектор, на котором лежит тестовая EICAR-сигнатура */
#define EICAR_SECTOR  110500UL

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

/* ==================== Показать offset ==================== */
static void show_offset(void)
{
    log_ts();
    DP(F("offset: "));
    DP(g_offset);
    DP(F(" / "));
    DPLN(g_capacity);
}

/* ==================== Генерация фейкового сектора ==================== */
static void gen_fake_sector(uint32_t sector, uint8_t *buf)
{
    /* По умолчанию — нули */
    memset(buf, 0, 512);

    /* На секторе EICAR_SECTOR (110 500) кладём EICAR-сигнатуру */
    if (sector == EICAR_SECTOR)
    {
        const char eicar[] = "EICAR-STANDARD-ANTIVIRUS-TEST-FI"
                             "LE!$H+H*";
        memcpy(buf, eicar, strlen(eicar));
    }
}

/* ==================== Имитация скана ==================== */
static void fake_scan(void)
{
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
        return;
    }

    /* flush Serial */
    delay(50);
    while (Serial.available()) Serial.read();

    unsigned long t0 = millis();
    uint32_t sec;
    uint8_t fake_buf[512];

    for (sec = g_offset; sec < g_capacity; sec++)
    {
        /* Отмена по любой клавише из Serial Monitor */
        if (Serial.available() > 0)
        {
            while (Serial.available()) Serial.read();
            DPLN();
            log_ts();
            DP(F("ABORTED at sector "));
            DPLN(sec);
            break;
        }

        /* Отмена по любому символу от Tang */
        if (tang.available() > 0)
        {
            while (tang.available()) tang.read();
            DPLN();
            log_ts();
            DP(F("ABORTED at sector "));
            DPLN(sec);
            break;
        }

        /* Прогресс каждые 500 секторов */
        if ((sec - g_offset) % 500 == 0)
        {
            log_ts();
            DP(sec);
            DP(F(" / "));
            DPLN(g_capacity);
        }

        /* Генерируем фейковый сектор */
        gen_fake_sector(sec, fake_buf);

        /* Сканируем */
        int flag = 0;
        const char *threat = "(none)";
        int sr = scan_feed(&g_scan, fake_buf, 512U,
                           SIGNATURES, SIGNATURES_COUNT, &flag, &threat);

        if (sr != SF_SUCCESS)
        {
            log_ts(); DPLN(F("err: scan_feed"));
            send_status('E');
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
            return;
        }
    }

    /* Дошли до конца или abort */
    DPLN();
    unsigned long dt = (millis() - t0) / 1000UL;
    log_ts(); DPLN(F("CLEAN - no threats (fake)"));
    log_ts();
    DP(sec - g_offset);
    DP(F(" sectors in "));
    DP(dt);
    DPLN(F(" sec"));

    send_status('G');
}

/* ==================== Обработка команды ==================== */
static void handle_char_cmd(char c)
{
    switch (c)
    {
        case 'h':
            g_offset += 100UL;
            if (g_offset > g_capacity) g_offset = g_capacity;
            show_offset();
            break;
        case 'k':
            g_offset += 10000UL;
            if (g_offset > g_capacity) g_offset = g_capacity;
            show_offset();
            break;
        case 'm':
            g_offset += 1000000UL;
            if (g_offset > g_capacity) g_offset = g_capacity;
            show_offset();
            break;
        case 's':
            fake_scan();
            break;

        /* LED-управление — через ESC-протокол, без эха в лог */
        case 'g': case 'r':
        case '0': case '1': case '2':
        case '3': case '4': case '5':
            tang_cmd((uint8_t)c);
            log_ts();
            Serial.print(F("-> tang LED: '"));
            Serial.print(c);
            Serial.println(F("'"));
            break;

        /* Статусы — для проверки LED-паттернов */
        case 'R': case 'N': case 'W':
        case 'G': case 'B': case 'E':
            send_status((uint8_t)c);
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
        DP(F("unknown: "));
        DPLN(cmd);
    }
}

/* ==================== setup ==================== */
void setup()
{
    Serial.begin(115200);
    tang.begin(9600);

    delay(500);   // дать Tang время загрузиться

    DPLN();
    DPLN(F("================================"));
    DPLN(F("  USBGuard v5 - TEST MODE"));
    DPLN(F("  (no USB Host Shield)"));
    DPLN(F("================================"));
    DPLN(F("Commands:"));
    DPLN(F("  h   = offset +100"));
    DPLN(F("  k   = offset +10 000"));
    DPLN(F("  m   = offset +1 000 000"));
    DPLN(F("  s   = fake scan"));
    DPLN(F("  res = reset offset"));
    DPLN(F("  g/r = toggle green/red LED"));
    DPLN(F("  0-5 = toggle LEDs 1-6"));
    DPLN(F("  R/N/W/G/B/E = status LED test"));
    DPLN(F("================================"));
    DPLN(F("Capacity: 1 000 000 000 sectors"));
    DP(F("EICAR signature at sector "));
    DPLN(EICAR_SECTOR);
    DPLN(F("================================"));
    DPLN();

    show_offset();
    send_status('R');
}

/* ==================== loop ==================== */
void loop()
{
    /* Serial Monitor */
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

    /* Tang Nano */
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
