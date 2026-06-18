/// @file signatures.h
/// @brief Таблица вирусных сигнатур. EICAR — стандартный безвредный тест-образец.

#ifndef SIGNATURES_H
#define SIGNATURES_H

#include "signature_engine.h"

/* EICAR-ядро (32 байта): срез стандартной строки EICAR Anti-Virus Test File.
   Полная строка безвредна и используется во всём мире для проверки антивирусов. */
static const VirusSignature SIGNATURES[] =
{
    {
        { 'E','I','C','A','R','-','S','T','A','N','D','A','R','D','-','A',
          'N','T','I','V','I','R','U','S','-','T','E','S','T','-','F','I' },
        32U,
        "EICAR-Test-File"
    },
    /* Пример пользовательской сигнатуры: байты MZ + произвольный маркер.
       Замени/добавь свои HEX-шаблоны здесь. */
    {
        { 0x4D,0x5A,0xDE,0xAD,0xBE,0xEF,0,0,0,0,0,0,0,0,0,0,
          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
        6U,
        "Demo-MZ-DEADBEEF"
    }
};

#define SIGNATURES_COUNT ((unsigned int)(sizeof(SIGNATURES) / sizeof(SIGNATURES[0])))

#endif /* SIGNATURES_H */
