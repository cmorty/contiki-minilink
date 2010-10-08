/*
 * Copyright (c) 2009, Friedrich-Alexander University Erlangen, Germany
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
 *         Alignment macros
 * \author
 *         Klaus Stengel <Klaus.Stengel@informatik.uni-erlangen.de>
 */

/* Macros for space efficient alignment calculation */

#include "contiki.h"

#define ALIGN_PREV(x, at) (((x) % (at)) ? (x) - ((x) % (at)) : (x));
#define ALIGN_NEXT(x, at) (((x) % (at)) ? (x) - ((x) % (at)) + (at) : (x));

#define ALIGN_WORD_NEXT(x) (x) + ((x) & 1)
#define ALIGN_WORD_PREV(x) (x) - ((x) & 1)

#if (ROM_ERASE_UNIT_SIZE & (ROM_ERASE_UNIT_SIZE - 1)) == 0
#define XXXALIGNLOWBITS(x) ((x) & (ROM_ERASE_UNIT_SIZE - 1))
#define ALIGN_ROM_PREV(x)  ((x) - XXXALIGNLOWBITS(x))
#define ALIGN_ROM_NEXT(x)  (ALIGN_ROM_PREV(x) + \
                              (XXXALIGNLOWBITS(x) ? ROM_ERASE_UNIT_SIZE : 0))
#else
#define ALIGN_ROM_PREV(x) ALIGN_PREV(x, ROM_ERASE_UNIT_SIZE)
#define ALIGN_ROM_NEXT(x) ALIGN_NEXT(x, ROM_ERASE_UNIT_SIZE)
#endif


/* @} */
