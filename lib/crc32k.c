/*
 * Copyright (c) 2008, Friedrich-Alexander University Erlangen, Germany
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \addtogroup crc32k
 * @{
 * \file
 *         Koopman CRC32 implementation.
 * \author
 *         Klaus Stengel <Klaus.Stengel@informatik.uni-erlangen.de>
 */

#include "crc32k.h"

void
crc32k_add(const void * bits, size_t length, uint32_t * const crc) {
  const unsigned char *mbits = bits;
  uint_fast8_t i, tst;
  uint32_t localcrc = *crc;

  while(length) {
    localcrc ^= *mbits++;
    for (i = 0; i < 8; i++) {
      tst = localcrc;
      localcrc >>= 1;
      if (tst & 1) {
#if 0
	localcrc ^= 0xEDB88320ul; /* IEEE 802.3 */
	localcrc ^= 0x82F63B78ul; /* iSCSI */
#endif
	localcrc ^= 0xEB31D82Eu; /* Koopman */
      }
    }
    length--;
  }
  *crc = localcrc;
}
/* @} */
