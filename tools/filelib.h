/*
 * Copyright (c) 2010, Friedrich-Alexander University Erlangen, Germany
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
 * \addtogroup sky_bootloader
 * @{
 * \file
 *         Helper functions to create loadable kernel modules
 * \author
 *         Klaus Stengel <Klaus.Stengel@informatik.uni-erlangen.de>
 *         Moritz Strübe <Moritz.Struebe@informatik.uni-erlangen.de>
 */

#ifndef FILELIB__H_INCLUDED
#define FILELIB__H_INCLUDED

#include "minilink.h"
#include <stdio.h>

typedef struct kernel_image_st {
  unsigned char *txtdat;
  size_t txtdat_size;
  unsigned char *vectors;
} KernelImage;

#ifndef COMPILE_HOSTED_TOOLS
#define PACK __attribute__((__packed__))
#else
#define PACK
#endif


/** Operating system image information */
typedef struct {
  /** File signature, must equal MINILINK_KERNEL_SIGNATURE */
  uint16_t signature PACK;

  /** Size of kernel image (code + data) */
  uint16_t imagesize PACK;

  /** CRC32K checksum of code + data + interrupt table */
  uint32_t imagecrc PACK;

  /** Where execution should start after loading */
  uint16_t entrypoint PACK;

  /** Named memory offset */
  uint16_t nmem_start PACK;

  /** Named memory size */
  uint16_t nmem_size PACK;
} OSImageInfo;

#undef PACK


void build_kernel_header(const KernelImage *kern, OSImageInfo *head);
int convert_kernel_header(const OSImageInfo *data, unsigned char *dest,
    const size_t destspace);

int convert_symbol_header(const Minilink_SymbolHeader *sh, unsigned char *dest,
    const size_t destspace);
int convert_program_header(const Minilink_Header *mlh, unsigned char *dest,
    const size_t destspace);

int read_kernel_header(unsigned char *src, size_t srclen,
    OSImageInfo *output);

uint16_t get_le16_val(unsigned char *bytes);
int set_le16(unsigned char **dest, size_t *space, uint16_t data);

#endif
/* @} */
