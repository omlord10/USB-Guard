/// @file signature_engine.h
/// @brief Потоковый сканер байтовых (HEX) сигнатур для посекторного чтения USB.
///
/// Движок не зависит от платформы: компилируется и на ПК (gcc), и в Arduino.
/// Данные подаются блоками (например, секторами по 512 байт) через scan_feed().
/// Сигнатуры, лежащие на границе двух блоков, ловятся за счёт "хвоста" (carry).

#ifndef SIGNATURE_ENGINE_H
#define SIGNATURE_ENGINE_H

#include <stddef.h>

/// Максимальная длина одной сигнатуры в байтах.
#define SIG_MAX_LEN 32U

/// @brief Описание одной вирусной сигнатуры (последовательности байтов).
typedef struct
{
    unsigned char pattern[SIG_MAX_LEN]; ///< Байты сигнатуры.
    unsigned char length;               ///< Реальная длина сигнатуры (1..SIG_MAX_LEN).
    const char   *name;                 ///< Человекочитаемое имя угрозы.
} VirusSignature;

/// @brief Состояние потокового сканера. Хранит "хвост" предыдущего блока.
typedef struct
{
    unsigned char carry[SIG_MAX_LEN - 1U]; ///< Последние байты прошлого блока.
    unsigned int  carry_len;                ///< Сколько байт реально в carry.
} ScanState;

/// Коды ошибок scan_init().
enum Error_Codes_SI
{
    SI_SUCCESS = 0,
    SI_NULL_STATE_POINTER = 1
};

/// Коды ошибок scan_feed().
enum Error_Codes_SF
{
    SF_SUCCESS = 0,
    SF_NULL_STATE_POINTER = 1,
    SF_NULL_BLOCK_POINTER = 2,
    SF_NULL_SIGS_POINTER = 3,
    SF_NULL_FLAG_POINTER = 4,
    SF_NULL_MATCH_NAME_POINTER = 5,
    SF_ZERO_SIGNATURE_LENGTH = 6,
    SF_SIGNATURE_TOO_LONG = 7
};

int scan_init(ScanState *state);

int scan_feed(ScanState *state,
              const unsigned char *block,
              unsigned int block_len,
              const VirusSignature *sigs,
              unsigned int sig_count,
              int *virus_flag,
              const char **match_name);

#endif /* SIGNATURE_ENGINE_H */
