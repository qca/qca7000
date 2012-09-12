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
 *   qca_7k.c	-
 *
 *   This module implements the Qualcomm Atheros SPI protocol for
 *   kernel-based SPI device.
 *   
 *--------------------------------------------------------------------*/

/*====================================================================*
 *   system header files;
 *--------------------------------------------------------------------*/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/spi/spi.h>

/*====================================================================*
 *   custom header files;
 *--------------------------------------------------------------------*/

#include "qca_7k.h"

/*====================================================================*
 *
 *   uint16_t qcaspi_read_register(struct qcaspi *qca, uint16_t reg);
 *
 *   Read the specified register from the QCA7K
 *
 *--------------------------------------------------------------------*/

uint16_t
qcaspi_read_register(struct qcaspi *qca, uint16_t reg)
{
	uint16_t tx_data;
	uint16_t rx_data;
	struct spi_transfer transfer[2];
	struct spi_message msg;

	memset(transfer, 0, sizeof(transfer));

	spi_message_init(&msg);

	tx_data = __cpu_to_be16(QCA7K_SPI_READ | QCA7K_SPI_INTERNAL | reg);

	transfer[0].tx_buf = &tx_data;
	transfer[0].len = QCASPI_CMD_LEN;
	transfer[1].rx_buf = &rx_data;
	transfer[1].len = QCASPI_CMD_LEN;

	if (qca->legacy_mode) {
		spi_message_add_tail(&transfer[0], &msg);
		spi_sync(qca->spi_device, &msg);
		spi_message_init(&msg);
		spi_message_add_tail(&transfer[1], &msg);
		spi_sync(qca->spi_device, &msg);
	} else {
		spi_message_add_tail(&transfer[0], &msg);
		spi_message_add_tail(&transfer[1], &msg);
		spi_sync(qca->spi_device, &msg);
	}

	return __be16_to_cpu(rx_data);
}

/*====================================================================*
 *    
 *   void qcaspi_write_register(struct qcaspi *qca, uint16_t reg, uint16_t value);
 *
 *   Write a 16 bit value to the specified SPI register
 *
 *--------------------------------------------------------------------*/

void
qcaspi_write_register(struct qcaspi *qca, uint16_t reg, uint16_t value)
{
	uint16_t tx_data[2];
	struct spi_transfer transfer[2];
	struct spi_message msg;

	memset(&transfer, 0, sizeof(transfer));

	spi_message_init(&msg);

	tx_data[0] = __cpu_to_be16(QCA7K_SPI_WRITE | QCA7K_SPI_INTERNAL | reg);
	tx_data[1] = __cpu_to_be16(value);

	transfer[0].tx_buf = &tx_data[0];
	transfer[0].len = QCASPI_CMD_LEN;
	transfer[1].tx_buf = &tx_data[1];
	transfer[1].len = QCASPI_CMD_LEN;

	if (qca->legacy_mode) {
		spi_message_add_tail(&transfer[0], &msg);
		spi_sync(qca->spi_device, &msg);
		spi_message_init(&msg);
		spi_message_add_tail(&transfer[1], &msg);
		spi_sync(qca->spi_device, &msg);
	} else {
		spi_message_add_tail(&transfer[0], &msg);
		spi_message_add_tail(&transfer[1], &msg);
		spi_sync(qca->spi_device, &msg);
	}
}

/*====================================================================*
 *    
 *   void qcaspi_tx_cmd(struct qcaspi *qca, uint16_t cmd);
 *
 *   Transmits a 16 bit command.
 *
 *--------------------------------------------------------------------*/

int
qcaspi_tx_cmd(struct qcaspi *qca, uint16_t cmd)
{
	struct spi_message msg;
	struct spi_transfer transfer;

	memset(&transfer, 0, sizeof(transfer));

	spi_message_init(&msg);

	transfer.len = sizeof(cmd);
	transfer.tx_buf = &cmd;
	spi_message_add_tail(&transfer, &msg);

	cmd = __cpu_to_be16(cmd);
	spi_sync(qca->spi_device, &msg);

	if (msg.actual_length != sizeof(cmd)) {
		return -1;
	}

	return 0;
}

/*====================================================================*
 *
 *--------------------------------------------------------------------*/
