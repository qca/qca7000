/*====================================================================*
 *   
 *   Copyright (c) 2011, 2012, Qualcomm Atheros Communications Inc.
 *   
 *   Permission to use, copy, modify, and/or distribute this software 
 *   for any purpose with or without fee is hereby granted, provided 
 *   that the above copyright notice and this permission notice appear 
 *   in all copies.
 *   
 *   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL 
 *   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED 
 *   WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL  
 *   THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR 
 *   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 *   LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, 
 *   NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 *   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *   
 *--------------------------------------------------------------------*/

/*====================================================================*
 * 
 *   qca_debug.c
 *
 *   This file contains debugging routines for use in the QCA7K driver.
 *				
 *--------------------------------------------------------------------*/

/*====================================================================*
 *   system header files;
 *--------------------------------------------------------------------*/

#include <linux/types.h>

/*====================================================================*
 *   custom header files;
 *--------------------------------------------------------------------*/

#include "qca_spi.h"
#include "qca_7k.h"

/*
 * Dumps the contents of all SPI slave registers.
 */
void
dump_reg(char *str, struct qcaspi *qca)
{
#if 1
	struct reg {
		char *name;
		uint32_t address;
	};

	static struct reg regs[] = {
		{ "SPI_REG_BFR_SIZE", SPI_REG_BFR_SIZE },
		{ "SPI_REG_WRBUF_SPC_AVA", SPI_REG_WRBUF_SPC_AVA },
		{ "SPI_REG_RDBUF_BYTE_AVA", SPI_REG_RDBUF_BYTE_AVA },
		{ "SPI_REG_SPI_CONFIG", SPI_REG_SPI_CONFIG },
		{ "SPI_REG_INTR_CAUSE", SPI_REG_INTR_CAUSE },
		{ "SPI_REG_INTR_ENABLE", SPI_REG_INTR_ENABLE },
		{ "SPI_REG_RDBUF_WATERMARK", SPI_REG_RDBUF_WATERMARK },
		{ "SPI_REG_WRBUF_WATERMARK", SPI_REG_WRBUF_WATERMARK },
		{ "SPI_REG_SIGNATURE", SPI_REG_SIGNATURE },
		{ "SPI_REG_ACTION_CTRL", SPI_REG_ACTION_CTRL }
	};
	int i;

	for (i = 0; i < (sizeof(regs) / sizeof(struct reg)); ++i) {
		uint32_t value;
		value = qcaspi_read_register(qca, regs[i].address);
		printk(KERN_DEBUG "qcaspi: %s: %lu: dumpreg: %s(0x%04x)=0x%08x\n", str, jiffies, regs[i].name, regs[i].address, value);
	}
#endif
}
