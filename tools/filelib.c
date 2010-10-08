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
 */

#include "filelib.h"
#include <crc32k.h>


#if BOOTLOADER
static void
crc32k_add_le16(uint32_t *sum, uint16_t data)
{
  unsigned char realdata[2];

  realdata[0] = (unsigned char)(data & 0xff);
  realdata[1] = (unsigned char)(data >> 8);
  crc32k_add(realdata, sizeof(realdata), sum);
}

static void
crc32k_add_le32(uint32_t *sum, uint32_t data)
{
  unsigned char realdata[4];

  realdata[0] = (unsigned char)( data        & 0xff);
  realdata[1] = (unsigned char)((data >>  8) & 0xff);
  realdata[2] = (unsigned char)((data >> 16) & 0xff);
  realdata[3] = (unsigned char)((data >> 24) & 0xff);
  crc32k_add(realdata, sizeof(realdata), sum);
}

#endif

uint16_t
get_le16_val(unsigned char *bytes)
{
  uint16_t retval;

  retval  = (uint16_t)(bytes[0]);
  retval |= (uint16_t)(bytes[1]) << 8;
  return retval;
}

uint32_t
get_le32_val(unsigned char *bytes)
{
  uint32_t retval;

  retval  = (uint32_t)(bytes[0]);
  retval |= (uint32_t)(bytes[1]) <<  8;
  retval |= (uint32_t)(bytes[2]) << 16;
  retval |= (uint32_t)(bytes[3]) << 24;
  return retval;
}

int
set_le16(unsigned char **dest, size_t *space, uint16_t data)
{
  if (*space < 2) return -1;

  *(*dest)++ = (unsigned char)(data & 0xff);
  *(*dest)++ = (unsigned char)(data >> 8);
  *space -= 2;
  return 0;
}

static int
set_le32(unsigned char **dest, size_t *space, uint32_t data)
{
  if (*space < 4) return -1;

  *(*dest)++ = (unsigned char)( data        & 0xff);
  *(*dest)++ = (unsigned char)((data >>  8) & 0xff);
  *(*dest)++ = (unsigned char)((data >> 16) & 0xff);
  *(*dest)++ = (unsigned char)((data >> 24) & 0xff);
  *space -= 4;
  return 0;
}
#if BOOTLOADER

void
build_kernel_header(const KernelImage *kern, OSImageInfo *head)
{
  head->signature = MINILINK_KERNEL_SIGNATURE;
  head->imagesize = kern->txtdat_size;

  /* --- assume image crc to be zero ---------------------- */
  crc32k_init(&head->imagecrc);

  /* --- retrieve entry point from vector table ----------- */
  head->entrypoint = get_le16_val(kern->vectors + INTR_VECTOR_TABLE_BYTES);

  /* --- sanity cleanup for named memory ------------------ */
  if (head->nmem_start == 0 || head->nmem_size == 0) {
    head->nmem_start = 0;
    head->nmem_size = 0;
  }

  /* --- calculate image header checksum ------------------ */
  crc32k_add_le16(&head->imagecrc, head->signature);
  crc32k_add_le16(&head->imagecrc, head->imagesize);
  crc32k_add_le32(&head->imagecrc, 0);
  crc32k_add_le16(&head->imagecrc, head->entrypoint);
  crc32k_add_le16(&head->imagecrc, head->nmem_start);
  crc32k_add_le16(&head->imagecrc, head->nmem_size);

  /* --- add kernel text, data and vectors ---------------- */
  crc32k_add(kern->txtdat, kern->txtdat_size, &head->imagecrc);
  crc32k_add(kern->vectors, INTR_VECTOR_TABLE_BYTES, &head->imagecrc);
}
#endif

int
read_kernel_header(unsigned char *src, size_t srclen, OSImageInfo *output) {
  if (srclen < 2+2+4+2+2+2)
    return -1;

  output->signature  = get_le16_val(src); src += 2;
  output->imagesize  = get_le16_val(src); src += 2;
  output->imagecrc   = get_le32_val(src); src += 4;
  output->entrypoint = get_le16_val(src); src += 2;
  output->nmem_start = get_le16_val(src); src += 2;
  output->nmem_size  = get_le16_val(src); src += 2;

  return 0;
}




int
convert_kernel_header(const OSImageInfo *data, unsigned char *dest,
    size_t destspace)
{

  int status;
  size_t orig_destspace = destspace;

  status = set_le16(&dest, &destspace, data->signature);
  if (status != 0) return status;
  status = set_le16(&dest, &destspace, data->imagesize);
  if (status != 0) return status;
  status = set_le32(&dest, &destspace, data->imagecrc);
  if (status != 0) return status;
  status = set_le16(&dest, &destspace, data->entrypoint);
  if (status != 0) return status;
  status = set_le16(&dest, &destspace, data->nmem_start);
  if (status != 0) return status;
  status = set_le16(&dest, &destspace, data->nmem_size);
  if (status != 0) return status;

  return orig_destspace - destspace;
}


int
convert_symbol_header(const Minilink_SymbolHeader *sh, unsigned char *dest,
    size_t destspace)
{
  int status;
  size_t orig_destspace = destspace;

  status = set_le16(&dest, &destspace, sh->common.magic);
  if (status != 0) return status;
  status = set_le32(&dest, &destspace, sh->common.crc);
  if (status != 0) return status;
  status = set_le32(&dest, &destspace, sh->kernelchksum);
  if (status != 0) return status;

  return orig_destspace - destspace;
}

int
convert_program_header(const Minilink_Header *mlh, unsigned char *dest,
    size_t destspace)
{
  int status;
  size_t orig_destspace = destspace;

  status = set_le16(&dest, &destspace, mlh->common.magic);
  if (status != 0) return status;
  status = set_le32(&dest, &destspace, mlh->common.crc);
  if (status != 0) return status;
  status = set_le16(&dest, &destspace, mlh->processoffset);
  if (status != 0) return status;
  status = set_le16(&dest, &destspace, mlh->textsize);
  if (status != 0) return status;
  status = set_le16(&dest, &destspace, mlh->datasize);
  if (status != 0) return status;
  status = set_le16(&dest, &destspace, mlh->bsssize);
  if (status != 0) return status;
  status = set_le16(&dest, &destspace, mlh->migsize);
  if (status != 0) return status;
  status = set_le16(&dest, &destspace, mlh->migptrsize);
  if (status != 0) return status;
  status = set_le16(&dest, &destspace, mlh->symentries);
  if (status != 0) return status;

  return orig_destspace - destspace;
}



/* @} */
