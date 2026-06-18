/// @file signature_engine.c
/// @brief Реализация потокового сканера сигнатур (см. signature_engine.h).

#include "signature_engine.h"

/// @brief Возвращает байт по сквозному индексу в склейке (carry + block).
/// @param state     Состояние сканера (источник carry). Не NULL (гарантирует вызывающий).
/// @param block     Текущий блок данных. Не NULL (гарантирует вызывающий).
/// @param block_len Длина блока.
/// @param index     Сквозной индекс: [0..carry_len) -> carry, дальше -> block.
/// @return Значение байта по индексу.
/// @note Внутренняя функция, входные указатели проверены в scan_feed().
static unsigned char byte_at(const ScanState *state,
                             const unsigned char *block,
                             unsigned int block_len,
                             unsigned int index)
{
    (void)block_len;
    if (index < state->carry_len)
    {
        return state->carry[index];
    }
    return block[index - state->carry_len];
}

/// @brief Инициализирует состояние сканера (сбрасывает хвост).
/// @param state Указатель на состояние. Не должен быть NULL.
/// @return SI_SUCCESS при успехе, иначе значение Error_Codes_SI.
int scan_init(ScanState *state)
{
    if (state == NULL)
    {
        return SI_NULL_STATE_POINTER;
    }

    state->carry_len = 0U;
    return SI_SUCCESS;
}

/// @brief Скармливает сканеру один блок данных и ищет в нём сигнатуры.
/// @param state      Состояние сканера. Не должен быть NULL.
/// @param block      Блок данных (например, сектор 512 байт). Не должен быть NULL.
/// @param block_len  Длина блока в байтах.
/// @param sigs       Таблица сигнатур. Не должна быть NULL.
/// @param sig_count  Количество сигнатур в таблице.
/// @param virus_flag Выход: 1 если найдена сигнатура, иначе 0. Не должен быть NULL.
/// @param match_name Выход: имя найденной угрозы (или не трогается, если чисто). Не NULL.
/// @return SF_SUCCESS при успехе, иначе значение Error_Codes_SF.
/// @note Сигнатуры на стыке блоков ловятся через carry предыдущего вызова.
/// @warning Длина каждой сигнатуры должна быть в диапазоне 1..SIG_MAX_LEN.
int scan_feed(ScanState *state,
              const unsigned char *block,
              unsigned int block_len,
              const VirusSignature *sigs,
              unsigned int sig_count,
              int *virus_flag,
              const char **match_name)
{
    unsigned int total;
    unsigned int pos;
    unsigned int s;
    unsigned int k;
    unsigned int new_carry;
    unsigned int copy_start;
    unsigned int i;

    if (state == NULL)
    {
        return SF_NULL_STATE_POINTER;
    }
    if (block == NULL)
    {
        return SF_NULL_BLOCK_POINTER;
    }
    if (sigs == NULL)
    {
        return SF_NULL_SIGS_POINTER;
    }
    if (virus_flag == NULL)
    {
        return SF_NULL_FLAG_POINTER;
    }
    if (match_name == NULL)
    {
        return SF_NULL_MATCH_NAME_POINTER;
    }

    for (s = 0U; s < sig_count; s = s + 1U)
    {
        if (sigs[s].length == 0U)
        {
            return SF_ZERO_SIGNATURE_LENGTH;
        }
        if (sigs[s].length > SIG_MAX_LEN)
        {
            return SF_SIGNATURE_TOO_LONG;
        }
    }

    *virus_flag = 0;

    total = state->carry_len + block_len;

    /* Проверяем каждую стартовую позицию в склейке carry+block. */
    for (pos = 0U; pos < total; pos = pos + 1U)
    {
        for (s = 0U; s < sig_count; s = s + 1U)
        {
            unsigned int len = (unsigned int)sigs[s].length;

            if ((pos + len) > total)
            {
                continue; /* сигнатура не помещается с этой позиции */
            }

            for (k = 0U; k < len; k = k + 1U)
            {
                if (byte_at(state, block, block_len, pos + k) != sigs[s].pattern[k])
                {
                    break;
                }
            }

            if (k == len)
            {
                *virus_flag = 1;
                *match_name = sigs[s].name;
                /* Угроза найдена — состояние carry не важно, выходим сразу. */
                return SF_SUCCESS;
            }
        }
    }

    /* Сохраняем последние (SIG_MAX_LEN-1) байт как хвост для следующего блока. */
    new_carry = SIG_MAX_LEN - 1U;
    if (new_carry > total)
    {
        new_carry = total;
    }
    copy_start = total - new_carry;

    for (i = 0U; i < new_carry; i = i + 1U)
    {
        state->carry[i] = byte_at(state, block, block_len, copy_start + i);
    }
    state->carry_len = new_carry;

    return SF_SUCCESS;
}
