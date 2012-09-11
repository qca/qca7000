/*====================================================================*
 *   
 *   Copyright (c) 2011, 2012, Atheros Communications Inc.
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
 *   QcaFraming.h
 *
 *   Atheros Ethernet framing. Every Ethernet frame is surrounded by an atheros
 *   frame while transmitted over a serial channel.
 *
 *
 *--------------------------------------------------------------------*/

#ifndef _QCAFRAMING_H
#define _QCAFRAMING_H

/*====================================================================*
 *   system header files;
 *--------------------------------------------------------------------*/

#include <linux/types.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>

/*====================================================================*
 *   constants
 *--------------------------------------------------------------------*/

/* Frame is currently being received */
#define QCAFRM_GATHER 0

/*  No header byte while expecting it */
#define QCAFRM_NOHEAD (QCAFRM_ERR_BASE - 1)

/* No tailer byte while expecting it */
#define QCAFRM_NOTAIL (QCAFRM_ERR_BASE - 2)

/* Frame length is invalid */
#define QCAFRM_INVLEN (QCAFRM_ERR_BASE - 3)

/* Frame length is invalid */
#define QCAFRM_INVFRAME (QCAFRM_ERR_BASE - 4)

/* Min/Max Ethernet MTU */
#define QCAFRM_ETHMINMTU 46
#define QCAFRM_ETHMAXMTU 1500

/* Min/Max frame lengths */
#define QCAFRM_ETHMINLEN (QCAFRM_ETHMINMTU + ETH_HLEN)
#define QCAFRM_ETHMAXLEN (QCAFRM_ETHMAXMTU + VLAN_ETH_HLEN)

/* QCA7K header len */
#define QCAFRM_HEADER_LEN 8

/* QCA7K footer len */
#define QCAFRM_FOOTER_LEN 2

/* QCA7K Framing. */
#define QCAFRM_ERR_BASE -1000

/*====================================================================*
 *   
 *   QcaFrmState
 *
 *   Various states during frame decoding.
 *
 *--------------------------------------------------------------------*/

typedef enum {
	QCAFRM_HW_LEN0 = 0x8000,
	QCAFRM_HW_LEN1 = QCAFRM_HW_LEN0 - 1,
	QCAFRM_HW_LEN2 = QCAFRM_HW_LEN1 - 1,
	QCAFRM_HW_LEN3 = QCAFRM_HW_LEN2 - 1,

	/*  Waiting first 0xAA of header */
	QCAFRM_WAIT_AA1 = QCAFRM_HW_LEN3 - 1,

	/*  Waiting second 0xAA of header */
	QCAFRM_WAIT_AA2 = QCAFRM_WAIT_AA1 - 1,

	/*  Waiting third 0xAA of header */
	QCAFRM_WAIT_AA3 = QCAFRM_WAIT_AA2 - 1,

	/*  Waiting fourth 0xAA of header */
	QCAFRM_WAIT_AA4 = QCAFRM_WAIT_AA3 - 1,

	/*  Waiting Byte 0-1 of length (litte endian) */
	QCAFRM_WAIT_LEN_BYTE0 = QCAFRM_WAIT_AA4 - 1,
	QCAFRM_WAIT_LEN_BYTE1 = QCAFRM_WAIT_AA4 - 2,

	/* Reserved bytes */
	QCAFRM_WAIT_RSVD_BYTE1 = QCAFRM_WAIT_AA4 - 3,
	QCAFRM_WAIT_RSVD_BYTE2 = QCAFRM_WAIT_AA4 - 4,

	/*  The frame length is used as the state until the end of the Ethernet frame */
	/*  Waiting for first 0x55 of footer */
	QCAFRM_WAIT_551 = 1,

	/*  Waiting for second 0x55 of footer */
	QCAFRM_WAIT_552 = QCAFRM_WAIT_551 - 1
} QcaFrmState;

/*====================================================================*
 *   
 *   QcaFrmHdl
 *
 *   Structure to maintain the frame decoding during reception.
 *
 *--------------------------------------------------------------------*/

typedef struct {
	/*  Current decoding state */
	QcaFrmState state;

	/* Offset in buffer (borrowed for length too) */
	int16_t offset;

	/* Frame length as kept by this module */
	uint16_t len;
} QcaFrmHdl;

/*====================================================================*
 *   
 *   QcaFrmCreateHeader
 *
 *   Fills a provided buffer with the QCA7K frame header.
 *
 *   Return: The number of bytes written in header.
 *   
 *--------------------------------------------------------------------*/

int32_t QcaFrmCreateHeader(uint8_t *buf, uint16_t len);

/*====================================================================*
 *   
 *   QcaFrmCreateFooter
 *
 *   Fills a provided buffer with the QCA7K frame footer.
 *
 *   Return: The number of bytes written in footer.
 *   
 *--------------------------------------------------------------------*/

int32_t QcaFrmCreateFooter(uint8_t *buf);

/*====================================================================*
 *   
 *   QcaFrmFsmInit
 *
 *   Initialize the framing handle. To be called at initialization for new
 *   QcaFrmHdl allocated.
 *
 *   
 *--------------------------------------------------------------------*/

void QcaFrmFsmInit(QcaFrmHdl *frmHdl);

/*====================================================================*
 *   
 *   QcaFrmFsmDecode
 *
 *   Gather received bytes and try to extract a full Ethernet frame by following
 *   a simple state machine.
 *
 * Return:   QCAFRM_GATHER       No Ethernet frame fully received yet.
 *           QCAFRM_NOHEAD       Header expected but not found.
 *           QCAFRM_INVLEN       QCA7K frame length is invalid
 *           QCAFRM_NOTAIL       Footer expected but not found.
 *           > 0                 Number of byte in the fully received Ethernet frame
 *   
 *--------------------------------------------------------------------*/

int32_t QcaFrmFsmDecode(QcaFrmHdl *frmHdl, uint8_t *ethBuf, uint16_t ethBufLen, uint8_t recvByte);

#endif 
