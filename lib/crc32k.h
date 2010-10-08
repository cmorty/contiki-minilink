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
 * \addtogroup lib
 * @{
 * \file
 *         Koopman CRC32 definitions.
 * \author
 *         Klaus Stengel <Klaus.Stengel@informatik.uni-erlangen.de>
 */
#ifndef CRC32K_H_INCLUDED
#define CRC32K_H_INCLUDED

#include <stdint.h>
#include <stdlib.h>

/** \brief Initialize CRC32 (Koopman) calculation
 * \param crc Pointer to memory to initialize
*/
static __inline__ void crc32k_init(uint32_t *crc) { *crc = 0xfffffffful; }

/** \brief Append data to CRC32 (Koopman) checksum
 * \param bits   Data to add
 * \param length Number of bytes to append
 * \param crc    Checksum value to update
*/
void crc32k_add(const void * const bits, size_t length, uint32_t * const crc);

#endif /* CRC32K_H_INCLUDED */
