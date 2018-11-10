/*
 * aes-efm32.c - AES driver for EFM32HG
 *
 * Copyright (C) 2018 Sergei Glushchenko
 * Author: Sergei Glushchenko <gl.sergei@gmail.com>
 *
 * This file is a part of Chpostx port to EFM32HG
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As additional permission under GNU GPL version 3 section 7, you may
 * distribute non-source form of the Program without the copy of the
 * GNU GPL normally required by section 4, provided you inform the
 * recipients of GNU GPL by a written offer.
 *
 */

#include <stdint.h>
#include "efm32hg.h"

static void aes_decryption_key_128 (uint32_t *d_key, const uint32_t *key)
{
  int i;

  /* Generate decryption key */
  for (i = 3; i >= 0; i--)
    AES->KEYLA = __builtin_bswap32(key[i]);

  AES->CTRL = 0;
  AES->CMD = AES_CMD_START;

  while (AES->STATUS & AES_STATUS_RUNNING)
    ;

  for (i = 3; i >= 0; i--)
    d_key[i] = __builtin_bswap32(AES->KEYLA);
}

void aes_cbc128_encrypt (uint8_t *out, const uint8_t *in, unsigned int len,
                         const uint8_t *key, const uint8_t *iv)
{
  uint32_t *out32 = (uint32_t *)out;
  const uint32_t *in32 = (const uint32_t *)in;
  const uint32_t *key32 = (const uint32_t *)key;
  const uint32_t *iv32 = (const uint32_t *)iv;
  int i;

  /* Enable AES clock */
  CMU->HFCORECLKEN0 |= CMU_HFCORECLKEN0_AES;

  len /= 16;

  AES->CTRL = AES_CTRL_XORSTART;

  /* Load IV */
  for (i = 3; i >= 0; i--)
    AES->DATA = __builtin_bswap32(iv32[i]);

  /* Encrypt data */
  while (len--)
    {
      /* Load key */
      for (i = 3; i >= 0; i--)
        AES->KEYLA = __builtin_bswap32(key32[i]);

      /* XOR and encrypt */
      for (i = 3; i >= 0; i--)
        AES->XORDATA = __builtin_bswap32(in32[i]);
      in32 += 4;

      /* Wait for completion */
      while (AES->STATUS & AES_STATUS_RUNNING)
        ;

      /* Save encrypted */
      for (i = 3; i >= 0; i--)
        out32[i] = __builtin_bswap32(AES->DATA);
      out32 += 4;
    }

  /* Disable AES clock */
  CMU->HFCORECLKEN0 &= ~CMU_HFCORECLKEN0_AES;
}

void aes_cbc128_decrypt (uint8_t *out, const uint8_t *in, unsigned int len,
                         const uint8_t *key, const uint8_t *iv)
{
  uint32_t *out32 = (uint32_t *)out;
  const uint32_t *in32  = (const uint32_t *)in;
  const uint32_t *key32 = (const uint32_t *)key;
  const uint32_t *iv32  = (const uint32_t *)iv;
  int i;
  uint32_t prev[4];
  uint32_t dk[4];

  /* Enable AES clock */
  CMU->HFCORECLKEN0 |= CMU_HFCORECLKEN0_AES;

  aes_decryption_key_128 (dk, key32);

  /* Number of blocks to process */
  len /= 16;

  /* Select decryption mode */
  AES->CTRL = AES_CTRL_DECRYPT | AES_CTRL_DATASTART;

  /* Copy init vector to previous buffer to avoid special handling */
  for (i = 0; i < 4; i++)
    prev[i] = iv32[i];

  /* Decrypt data */
  while (len--)
    {
      /* Load key */
      for (i = 3; i >= 0; i--)
        AES->KEYLA = __builtin_bswap32(dk[i]);

      /* Load data and trigger decryption */
      for (i = 3; i >= 0; i--)
        AES->DATA = __builtin_bswap32(in32[i]);

      /* Wait for completion */
      while (AES->STATUS & AES_STATUS_RUNNING)
        ;

      /* Make AES module to XOR plaintext with the previous block */
      for (i = 3; i >= 0; i--)
        {
          AES->XORDATA = __builtin_bswap32(prev[i]);
          prev[i] = in32[i];
        }
      in32 += 4;

      /* Fetch result */
      for (i = 3; i >= 0; i--)
        out32[i] = __builtin_bswap32(AES->DATA);
      out32 += 4;
    }

  /* Disable AES clock */
  CMU->HFCORECLKEN0 &= ~CMU_HFCORECLKEN0_AES;
}

