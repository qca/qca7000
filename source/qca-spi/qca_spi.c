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
 *   qca_spi.c
 *
 *   This module implements the Qualcomm Atheros SPI protocol for
 *   kernel-based SPI device;
 *				
 *--------------------------------------------------------------------*/

/*====================================================================*
 *   system header files;
 *--------------------------------------------------------------------*/

#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>		
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/netdevice.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/spi/spi.h>
#include <linux/types.h>
#include <linux/version.h>

/*====================================================================*
 *   custom header files;
 *--------------------------------------------------------------------*/

#include "qca_spi.h"
#include "qca_framing.h"
#include "qca_7k.h"
#include "qca_debug.h"

/*====================================================================*
 *   constants;
 *--------------------------------------------------------------------*/

#define QCASPI_VERSION "0.1.8"
#define QCASPI_MODNAME "qcaspi"
#define QCASPI_DEF_MAC_ADDRESS "\x00\xB0\x52\xFF\xFF\x02"

#define MAX_DMA_BURST_LEN 5000

extern int gpio_spi_intr_cfg(void);
extern int gpio_spi_intr_get_irq(void);

/*--------------------------------------------------------------------*
 *   Modules parameters
 *--------------------------------------------------------------------*/

#define QCASPI_CLK_SPEED_MIN 1039062
#define QCASPI_CLK_SPEED_MAX 16625000
#define QCASPI_CLK_SPEED 8312500
static int qcaspi_clkspeed = QCASPI_CLK_SPEED;		
module_param(qcaspi_clkspeed, int, 0);
MODULE_PARM_DESC(qcaspi_clkspeed, "SPI bus clock speed (Hz)");

#define QCASPI_LEGACY_MODE_MIN 0
#define QCASPI_LEGACY_MODE_MAX 1
static int qcaspi_legacy_mode = 0;
module_param(qcaspi_legacy_mode, int, 0);
MODULE_PARM_DESC(qcaspi_legacy_mode, "Turn on/off legacy mode.");

#define QCASPI_BURST_LEN_MIN 1
#define QCASPI_BURST_LEN_MAX MAX_DMA_BURST_LEN
static int qcaspi_burst_len = MAX_DMA_BURST_LEN;
module_param(qcaspi_burst_len, int, 0);
MODULE_PARM_DESC(qcaspi_burst_len, "Number of data bytes per burst. Use 1-5000.");

/*--------------------------------------------------------------------*
 *   SPI bus id parameter
 *--------------------------------------------------------------------*/

#define QCASPI_BUS_ID 1
#define QCASPI_BUS_MODE (SPI_CPOL | SPI_CPHA)
#define QCASPI_CS_ID 0
#define QCASPI_MTU QCAFRM_ETHMAXMTU
#define QCASPI_TX_TIMEOUT (1 * HZ)

/*====================================================================*
 *   variables;
 *--------------------------------------------------------------------*/

static struct spi_board_info qca_spi_board_info __initdata = {
	.modalias = QCASPI_MODNAME,
	.max_speed_hz = 50000000,
	.bus_num = QCASPI_BUS_ID,
	.chip_select = QCASPI_CS_ID,
	.mode = QCASPI_BUS_MODE
};

static struct net_device *qcaspi_devs;
static volatile unsigned int intReq;
static volatile unsigned int intSvc;

/*
 * Disables all SPI interrupts and returns the old value of the
 * interrupt enable register.
 */
uint32_t
disable_spi_interrupts(struct qcaspi *qca)
{
	uint32_t old_intr_enable = qcaspi_read_register(qca, SPI_REG_INTR_ENABLE);
	qcaspi_write_register(qca, SPI_REG_INTR_ENABLE, 0);
	return old_intr_enable;
}

/*
 * Enables only the SPI interrupts passed in the intr argument.
 * All others are disabled.
 * Returns the old value of the interrupt enable register.
 */
uint32_t
enable_spi_interrupts(struct qcaspi *qca, uint32_t intr_enable)
{
	uint32_t old_intr_enable = qcaspi_read_register(qca, SPI_REG_INTR_ENABLE);
	qcaspi_write_register(qca, SPI_REG_INTR_ENABLE, intr_enable);
	return old_intr_enable;
}

/*
 * Transmits a write command and len bytes of data
 * from src buffer in a single burst.
 *
 * Returns 0 if not all data could be transmitted,
 * and len if all data was transmitted.
 */
uint32_t
qcaspi_write_burst(struct qcaspi *qca, uint8_t *src, uint32_t len)
{
	uint16_t cmd;
	struct spi_message msg;
	struct spi_transfer transfer[2];

	memset(&transfer, 0, sizeof(transfer));
	spi_message_init(&msg);

	cmd = __cpu_to_be16(QCA7K_SPI_WRITE | QCA7K_SPI_EXTERNAL);
	transfer[0].tx_buf = &cmd;
	transfer[0].len = QCASPI_CMD_LEN;
	transfer[1].tx_buf = src;
	transfer[1].len = len;

	spi_message_add_tail(&transfer[0], &msg);
	spi_message_add_tail(&transfer[1], &msg);
	spi_sync(qca->spi_device, &msg);

	if (msg.actual_length != QCASPI_CMD_LEN + len) {
		return 0;
	}

	return len;
}

/*
 * Transmits len bytes of data from src buffer in
 * a single burst.
 *
 * Returns 0 if not all data could be transmitted,
 * and len if all data was transmitted.
 */
uint32_t
qcaspi_write_legacy(struct qcaspi *qca, uint8_t *src, uint32_t len)
{
	struct spi_message msg;
	struct spi_transfer transfer;

	memset(&transfer, 0, sizeof(transfer));
	spi_message_init(&msg);

	transfer.tx_buf = src;
	transfer.len = len;

	spi_message_add_tail(&transfer, &msg);
	spi_sync(qca->spi_device, &msg);

	if (msg.actual_length != len) {
		return 0;
	}

	return len;
}

/*
 * Sends a read command and then receives len
 * bytes of data from the external SPI slave into
 * the buffer at dst.
 *
 * Returns 0 if not all data could be received,
 * and len if all data was received.
 */
uint32_t
qcaspi_read_burst(struct qcaspi *qca, uint8_t *dst, uint32_t len)
{
	struct spi_message msg;
	uint16_t cmd;
	struct spi_transfer transfer[2];

	memset(&transfer, 0, sizeof(transfer));
	spi_message_init(&msg);

	cmd = __cpu_to_be16(QCA7K_SPI_READ | QCA7K_SPI_EXTERNAL);
	transfer[0].tx_buf = &cmd;
	transfer[0].len = QCASPI_CMD_LEN;
	transfer[1].rx_buf = dst;
	transfer[1].len = len;

	spi_message_add_tail(&transfer[0], &msg);
	spi_message_add_tail(&transfer[1], &msg);
	spi_sync(qca->spi_device, &msg);

	if (msg.actual_length != QCASPI_CMD_LEN + len) {
		return 0;
	}

	return len;
}

/*
 * Receives len bytes of data from the external
 * SPI slave into the buffer at dst.
 *
 * Returns 0 if not all data could be received,
 * and len if all data was received.
 */
uint32_t
qcaspi_read_legacy(struct qcaspi *qca, uint8_t *dst, uint32_t len)
{
	struct spi_message msg;
	struct spi_transfer transfer;

	memset(&transfer, 0, sizeof(transfer));
	spi_message_init(&msg);

	transfer.rx_buf = dst;
	transfer.len = len;

	spi_message_add_tail(&transfer, &msg);
	spi_sync(qca->spi_device, &msg);

	if (msg.actual_length != len) {
		return 0;
	}

	/* print_hex_dump(KERN_DEBUG, "read_legacy: ", DUMP_PREFIX_ADDRESS, 16, 1, dst, len, true); */

	return len;
}

/*
 * Transmits an sk_buff in legacy or burst mode.
 *
 * Returns -1 on failure, 0 on success.
 */
int
qcaspi_tx_frame(struct qcaspi *qca, struct sk_buff *skb)
{
	uint32_t count;
	uint32_t bytes_written;
	uint32_t offset;
	uint32_t len;

	len = skb->len;

	qcaspi_write_register(qca, SPI_REG_BFR_SIZE, len);
	if (qca->legacy_mode) {
		qcaspi_tx_cmd(qca, QCA7K_SPI_WRITE | QCA7K_SPI_EXTERNAL);
	}

	offset = 0;
	while (len) {
		count = len;
		if (count > qca->burst_len)
			count = qca->burst_len;

		if (qca->legacy_mode) {
			bytes_written = qcaspi_write_legacy(qca, skb->data + offset, count);
		} else {
			bytes_written = qcaspi_write_burst(qca, skb->data + offset, count);
		}

		if (bytes_written != count)
			return -1;

		offset += count;
		len -= count;
	}

	return 0;
}

/*
 * Transmits as many sk_buff's that will fit in
 * the SPI slave write buffer.
 *
 * Returns -1 on failure, 0 on success.
 */
int
qcaspi_transmit(struct qcaspi *qca)
{
	uint32_t available;

	available = qcaspi_read_register(qca, SPI_REG_WRBUF_SPC_AVA);

	while (qca->txq.skb[qca->txq.head] && available >= qca->txq.skb[qca->txq.head]->len + QCASPI_HW_PKT_LEN) {
		if (qcaspi_tx_frame(qca, qca->txq.skb[qca->txq.head]) == -1)
			return -1;

		qca->stats.tx_packets++;
		qca->stats.tx_bytes += qca->txq.skb[qca->txq.head]->len;
		available -= qca->txq.skb[qca->txq.head]->len + QCASPI_HW_PKT_LEN;

		/* remove the skb from the queue */
		netif_tx_lock(qca->dev);
		dev_kfree_skb(qca->txq.skb[qca->txq.head]);
		qca->txq.skb[qca->txq.head] = NULL;
		++qca->txq.head;
		if (qca->txq.head >= TX_QUEUE_LEN)
			qca->txq.head = 0;
		netif_wake_queue(qca->dev);
		netif_tx_unlock(qca->dev);
	}

	return 0;
}

/*
 * Read the number of bytes available in the SPI slave and
 * then read and process the data from the slave.
 *
 * Returns -1 on error, 0 on success.
 */
int
qcaspi_receive(struct qcaspi *qca)
{
	uint32_t available;
	uint32_t bytes_read;
	uint32_t count;
	uint8_t *cp;

	/* Allocate rx SKB if we don't have one available. */
	if (qca->rx_skb == NULL) {
		qca->rx_skb = dev_alloc_skb(qca->dev->mtu + VLAN_ETH_HLEN);
		if (qca->rx_skb == NULL) {
			printk(KERN_DEBUG "qcaspi: out of RX resources\n");
			return -1;
		}
	}

	/* Read the packet size. */
	available = qcaspi_read_register(qca, SPI_REG_RDBUF_BYTE_AVA);
	pr_debug("qcaspi: qcaspi_receive: SPI_REG_RDBUF_BYTE_AVA: Value: %08x\n", available);

	if (available == 0) {
		printk(KERN_DEBUG "qcaspi: qcaspi_receive called without any data being available!\n");
		return -1;
	}

	qcaspi_write_register(qca, SPI_REG_BFR_SIZE, available);

	if (qca->legacy_mode) {
		qcaspi_tx_cmd(qca, QCA7K_SPI_READ | QCA7K_SPI_EXTERNAL);
	}

	while (available) {
		count = available;
		if (count > qca->burst_len) {
			count = qca->burst_len;
		}

		if (qca->legacy_mode) {
			bytes_read = qcaspi_read_legacy(qca, qca->rx_buffer, count);
		} else {
			bytes_read = qcaspi_read_burst(qca, qca->rx_buffer, count);
		}
		cp = qca->rx_buffer;

		pr_debug("qcaspi: available: %d, byte read: %d\n", available, bytes_read);

		available -= bytes_read;

		while ((bytes_read--) && (qca->rx_skb)) {
			int32_t retcode = QcaFrmFsmDecode(&qca->lFrmHdl, qca->rx_skb->data, skb_tailroom(qca->rx_skb), *cp++);
			switch (retcode) {
			case QCAFRM_GATHER:
				break;

			case QCAFRM_NOHEAD:
				break;

			case QCAFRM_NOTAIL:
				pr_debug("qcaspi: no RX tail\n");
				qca->stats.rx_errors++;
				qca->stats.rx_dropped++;
				break;

			case QCAFRM_INVLEN:
				pr_debug("qcaspi: invalid RX length\n");
				qca->stats.rx_errors++;
				qca->stats.rx_dropped++;
				break;

			default:
				qca->rx_skb->dev = qca->dev;
				qca->stats.rx_packets++;
				qca->stats.rx_bytes += retcode;
				skb_put(qca->rx_skb, retcode);
				qca->rx_skb->protocol = eth_type_trans(qca->rx_skb, qca->rx_skb->dev);
				qca->rx_skb->ip_summed = CHECKSUM_UNNECESSARY;
				netif_rx_ni(qca->rx_skb);
				qca->rx_skb = dev_alloc_skb(qca->dev->mtu + VLAN_ETH_HLEN);
				if (!qca->rx_skb) {
					printk(KERN_DEBUG "qcaspi: out of RX resources\n");
					qca->stats.rx_errors++;
					break;
				}
			}
		}
	}

	return 0;
}

/*
 * Flush the tx queue. This function is only safe to
 * call from the qcaspi_spi_thread.
 */
void
qcaspi_flush_txq(struct qcaspi *qca)
{
	int i;

	netif_tx_lock(qca->dev);
	for (i = 0; i < TX_QUEUE_LEN; ++i) {
		if (qca->txq.skb[i])
			dev_kfree_skb(qca->txq.skb[i]);
		qca->txq.skb[i] = NULL;
		qca->txq.tail = 0;
		qca->txq.head = 0;
	}
	netif_tx_unlock(qca->dev);
}

/*
 * Manage synchronization with the external SPI slave.
 */
void
qcaspi_qca7k_sync(struct qcaspi *qca, int event)
{
	uint32_t signature;
	uint32_t spi_config;
	uint32_t wrbuf_space;
	static uint32_t reset_count;

	/* CPU ON occured, verify signature */
	if (event == QCASPI_SYNC_CPUON) {
		/* Read signature twice, if not valid go back to unknown state. */
		signature = qcaspi_read_register(qca, SPI_REG_SIGNATURE);
		signature = qcaspi_read_register(qca, SPI_REG_SIGNATURE);
		if (signature != QCASPI_GOOD_SIGNATURE) {
			qca->sync = QCASPI_SYNC_UNKNOWN;
			printk(KERN_DEBUG "qcaspi: sync: got CPU on, but signature was invalid, restart\n");
		} else {
			/* ensure that the WRBUF is empty */
			wrbuf_space = qcaspi_read_register(qca, SPI_REG_WRBUF_SPC_AVA);
			if (wrbuf_space != QCASPI_HW_BUF_LEN) {
				printk(KERN_DEBUG "qcaspi: sync: got CPU on, but wrbuf not empty. reset!\n");
				qca->sync = QCASPI_SYNC_UNKNOWN;
			} else {
				printk(KERN_DEBUG "qcaspi: sync: got CPU on, now in sync\n");
				qca->sync = QCASPI_SYNC_READY;
				return;
			}
		}
	}

	/* In sync. */
	if (qca->sync == QCASPI_SYNC_READY) {
		/* Don't check signature after sync in burst mode. */
		if (!qca->legacy_mode)
			return;

		signature = qcaspi_read_register(qca, SPI_REG_SIGNATURE);
		if (signature != QCASPI_GOOD_SIGNATURE) {
			qca->sync = QCASPI_SYNC_UNKNOWN;
			printk(KERN_DEBUG "qcaspi: sync: bad signature, restart\n");
			/* don't reset right away */
			return;
		}
	}

	/* Reset the device. */
	if (qca->sync == QCASPI_SYNC_UNKNOWN) {
		if (qca->legacy_mode) {
			/* use GPIO to reset QCA7000 */
		} else {
			/* Read signature, if not valid stay in unknown state */
			signature = qcaspi_read_register(qca, SPI_REG_SIGNATURE);
			if (signature != QCASPI_GOOD_SIGNATURE) {
				printk(KERN_DEBUG "qcaspi: sync: could not read signature to reset device, retry.\n");
				return;
			}

			printk(KERN_DEBUG "qcaspi: sync: resetting device.\n");
			spi_config = qcaspi_read_register(qca, SPI_REG_SPI_CONFIG);
			qcaspi_write_register(qca, SPI_REG_SPI_CONFIG, spi_config | QCASPI_SLAVE_RESET_BIT);
		}
		qca->sync = QCASPI_SYNC_RESET;
		reset_count = 0;
		return;
	}

	/* Currently waiting for CPU on to take us out of reset. */
	if (qca->sync == QCASPI_SYNC_RESET) {
		++reset_count;
		printk(KERN_DEBUG "qcaspi: sync: waiting for CPU on, count %d.\n", reset_count);
		if (reset_count >= QCASPI_RESET_TIMEOUT) {
			/* reset did not seem to take place, try again */
			qca->sync = QCASPI_SYNC_UNKNOWN;
			printk(KERN_DEBUG "qcaspi: sync: reset timeout, restarting process.\n");
		}
	}
}

/*====================================================================*
 *   
 * qcaspi_spi_thread - SPI worker thread.
 *
 * Handle interrupts and transmit requests on the SPI interface.
 *
 * Return:   0 on success, else failure
 *   
 *--------------------------------------------------------------------*/

static int
qcaspi_spi_thread(void *data) 
{
	struct qcaspi *qca = (struct qcaspi *) data;
	uint32_t vInterruptCause;
	uint32_t intr_enable;

	printk(KERN_INFO "qcaspi: SPI thread created\n");
	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (intReq == intSvc && qca->txq.skb[qca->txq.head] == NULL && qca->sync == QCASPI_SYNC_READY) {
			schedule();
		}
		__set_current_state(TASK_RUNNING);

		pr_debug("qcaspi: have work to do. int: %d, tx_skb: %p\n", intReq - intSvc, qca->txq.skb[qca->txq.head]);

		/* mantain sync with external device */
		qcaspi_qca7k_sync(qca, QCASPI_SYNC_UPDATE);

		/* not synced, awaiting reset, or unknown */
		if (qca->sync != QCASPI_SYNC_READY) {
			printk(KERN_DEBUG "qcaspi: sync: not ready, turn off carrier and flush\n");
			netif_carrier_off(qca->dev);
			qcaspi_flush_txq(qca);
			netif_wake_queue(qca->dev);
			msleep(1000);
		}

		if (intSvc != intReq) {
			intSvc = intReq;
			intr_enable = disable_spi_interrupts(qca);
			vInterruptCause = qcaspi_read_register(qca, SPI_REG_INTR_CAUSE);
			pr_debug("qcaspi: interrupts: 0x%08x\n", vInterruptCause);

			if (vInterruptCause & SPI_INT_CPU_ON) {
				qcaspi_qca7k_sync(qca, QCASPI_SYNC_CPUON);

				/* not synced. */
				if (qca->sync != QCASPI_SYNC_READY)
					continue;

				intr_enable = (SPI_INT_CPU_ON | SPI_INT_PKT_AVLBL | SPI_INT_RDBUF_ERR | SPI_INT_WRBUF_ERR);
				netif_carrier_on(qca->dev);
			}

			if (vInterruptCause & SPI_INT_RDBUF_ERR) {
				/* restart sync */
				printk(KERN_DEBUG "qcaspi: ===> rdbuf error!\n");
				qca->sync = QCASPI_SYNC_UNKNOWN;
				continue;
			}

			if (vInterruptCause & SPI_INT_WRBUF_ERR) {
				/* restart sync */
				printk(KERN_DEBUG "qcaspi: ===> wrbuf error!\n");
				qca->sync = QCASPI_SYNC_UNKNOWN;
				continue;
			}

			/* can only handle other interrupts if sync has occured */
			if (qca->sync == QCASPI_SYNC_READY) {
				if (vInterruptCause & SPI_INT_PKT_AVLBL) {
					qcaspi_receive(qca);
				}
			}

			qcaspi_write_register(qca, SPI_REG_INTR_CAUSE, vInterruptCause);
			enable_spi_interrupts(qca, intr_enable);
			pr_debug("qcaspi: acking int: 0x%08x\n", vInterruptCause);
		}

		if (qca->txq.skb[qca->txq.head] != NULL)
			qcaspi_transmit(qca);
	}
	set_current_state(TASK_RUNNING);
	printk(KERN_INFO "qcaspi: SPI thread exit\n");

	return 0;
}

/*====================================================================*
 *   
 * qcaspi_intr_handler -  Interrupt handler.
 *
 * The actual interrupt handler coming from the SPI device.
 *
 * Return:   see linux interrupt handler for more details.
 *   
 *--------------------------------------------------------------------*/

static irqreturn_t
qcaspi_intr_handler(int irq, void *data) 
{
	struct qcaspi *qca = (struct qcaspi *) data;
	intReq++;
	if (qca->spi_thread && qca->spi_thread->state != TASK_RUNNING) {
		wake_up_process(qca->spi_thread);
	}
	return IRQ_HANDLED;
}

/*====================================================================*
 *   
 * qcaspi_netdev_open - open network adapter.
 *
 * This function is use by the network stack to open the network device.
 *
 * Return:   0 on success, else failure
 *   
 *--------------------------------------------------------------------*/

int
qcaspi_netdev_open(struct net_device *dev) 
{
	struct qcaspi *qca = netdev_priv(dev);

	memset(&qca->txq, 0, sizeof(qca->txq));
	intReq = 0;
	intSvc = 0;
	qca->sync = QCASPI_SYNC_UNKNOWN;
	QcaFrmFsmInit(&qca->lFrmHdl);

	netif_start_queue(qca->dev);

	/* start main thread */
	qca->spi_thread = kthread_run((void *)qcaspi_spi_thread, qca, QCASPI_MODNAME);

	/* Attach interrupt handle & enable interrupt. */
	if (gpio_spi_intr_cfg() == 0) {
		if (!request_irq(dev->irq, qcaspi_intr_handler, 0, QCASPI_MODNAME, qca)) {
			printk(KERN_ERR "qcaspi: Irq request succeed %d\n", dev->irq);
		} else {
			printk(KERN_ERR "qcaspi: Fail to request irq %d\n", dev->irq);
		}
	} else {
		printk(KERN_ERR "qcaspi: Fail to reconfigure interrupt signal\n");
		/* XXX: should return an error here */
	}

	return 0;
}

/*====================================================================*
 *   
 * qcaspi_netdev_close - Close network device.
 *
 * This function is use by the network stack to close the network device.
 *
 * Return:   0 on success, else failure
 *   
 *--------------------------------------------------------------------*/

int
qcaspi_netdev_close(struct net_device *dev) 
{
	struct qcaspi *qca = netdev_priv(dev);

	/* Disable interrupt and free irq. */
	qcaspi_write_register(qca, SPI_REG_INTR_ENABLE, 0);
	free_irq(dev->irq, qca);

	/* stop the main thread */
	kthread_stop(qca->spi_thread);
	qca->spi_thread = NULL;

	/* can't transmit any more. */
	netif_stop_queue(dev);

	/* flush out the tx queue */
	qcaspi_flush_txq(qca);

	return 0;
}

/*====================================================================*
 *   
 * qcaspi_netdev_xmit - Transmit a packet (called by the kernel).
 *
 * This function is called by the network stack to send an Ethernet frame.
 *
 * Return:   0 on success, else failure
 *   
 *--------------------------------------------------------------------*/

netdev_tx_t
qcaspi_netdev_xmit(struct sk_buff *skb, struct net_device *dev) 
{
	uint32_t frame_len;
	uint8_t *ptmp;
	struct qcaspi *qca = netdev_priv(dev);
	uint32_t new_tail;
	struct sk_buff *tskb;
	uint8_t pad_len = 0;

	/* need padding? */
	if (skb->len < QCAFRM_ETHMINLEN) {
		pad_len = QCAFRM_ETHMINLEN - skb->len;
	}

#if 1
	/* room in our queue? should always be true */
	if (qca->txq.skb[qca->txq.tail]) {
		printk(KERN_WARNING "qcaspi: queue was unexpectedly full!\n");
		netif_stop_queue(qca->dev);
		return NETDEV_TX_BUSY;
	}
#endif

	/* not enough head, tail room, or a runt frame, must copy data */
	if (skb_headroom(skb) < QCAFRM_HEADER_LEN || skb_tailroom(skb) < QCAFRM_FOOTER_LEN + pad_len) {
		tskb = skb_copy_expand(skb, QCAFRM_HEADER_LEN, QCAFRM_FOOTER_LEN + pad_len, GFP_ATOMIC);
		if (tskb == NULL) {
			printk(KERN_DEBUG "qcaspi: could not allocate tx_buff in qcaspi_netdev_xmit\n");
			return NETDEV_TX_BUSY;
		}
		dev_kfree_skb(skb);
		skb = tskb;
	}

	/* save original frame length + padding */
	frame_len = skb->len + pad_len;

	ptmp = skb_push(skb, QCAFRM_HEADER_LEN);
	QcaFrmCreateHeader(ptmp, frame_len);

	if (pad_len) {
		ptmp = skb_put(skb, pad_len);
		memset(ptmp, 0, pad_len);
	}

	ptmp = skb_put(skb, QCAFRM_FOOTER_LEN);
	QcaFrmCreateFooter(ptmp);

	/* print_hex_dump(KERN_DEBUG, "xmit_frame: ", DUMP_PREFIX_ADDRESS, 16, 1, skb, skb->len, true); */
	pr_debug("qcaspi: Tx-ing packet: Size: 0x%08x\n", skb->len);

	new_tail = qca->txq.tail + 1;
	if (new_tail >= TX_QUEUE_LEN)
		new_tail = 0;

	/* queue is full */
	if (qca->txq.skb[new_tail]) {
		netif_stop_queue(qca->dev);
	}

	/* SKB is no longer ours. */
	qca->txq.skb[qca->txq.tail] = skb;
	qca->txq.tail = new_tail;

	dev->trans_start = jiffies;

	if (qca->spi_thread && qca->spi_thread->state != TASK_RUNNING) {
		wake_up_process(qca->spi_thread);
	}

	return NETDEV_TX_OK;
}


/*====================================================================*
 *   
 * qcaspi_netdev_tx_timeout - Transmit timeout function.
 *
 * This function deals with a transmit timeout.
 *
 * Return:   N/A
 *   
 *--------------------------------------------------------------------*/

void
qcaspi_netdev_tx_timeout(struct net_device *dev) 
{
	struct qcaspi *qca = netdev_priv(dev);
	printk(KERN_INFO "qcaspi: Transmit timeout at %ld, latency %ld\n", jiffies, jiffies - dev->trans_start);
	qca->stats.tx_errors++;
	/* wake the queue if there is room */
	if (qca->txq.skb[qca->txq.tail] == NULL) {
		netif_wake_queue(dev);
	}
}


/*====================================================================*
 *   
 * qcaspi_netdev_get_stats - Return statistics to the caller.
 *
 * This function returns the device statistics to the caller.
 *
 * Return:   Device statistics
 *   
 *--------------------------------------------------------------------*/

struct net_device_stats *
qcaspi_netdev_get_stats(struct net_device *dev) 
{
	struct qcaspi *qca = netdev_priv(dev);
	return &qca->stats;
}

/*====================================================================*
 *   
 * qcaspi_netdev_init - Netdevice register callback.
 *
 * This is the callback function for the Netdevice register function.
 *
 * Return:   0 on success, else failure
 *   
 *--------------------------------------------------------------------*/

static int
qcaspi_netdev_init(struct net_device *dev) 
{
	struct qcaspi *qca = netdev_priv(dev);
	
	/* Finish setting up the device info. */
	dev->irq = gpio_spi_intr_get_irq();
	dev->mtu = QCASPI_MTU;
	dev->type = ARPHRD_ETHER;
	qca->clkspeed = qcaspi_clkspeed;
	qca->legacy_mode = qcaspi_legacy_mode;
	qca->burst_len = qcaspi_burst_len;

	qca->buffer_size = (dev->mtu + VLAN_ETH_HLEN + QCAFRM_HEADER_LEN + QCAFRM_FOOTER_LEN + 4) * 4;

	/* Allocate RX data buffer. */
	qca->rx_buffer = kmalloc(qca->buffer_size, GFP_ATOMIC);
	if (!qca->rx_buffer) {
		return -ENOBUFS;
	}

	qca->rx_skb = dev_alloc_skb(qca->dev->mtu + VLAN_ETH_HLEN);
	if (qca->rx_skb == NULL) {
		printk(KERN_INFO "qcaspi: Failed to allocate RX sk_buff.\n");
		return -ENOBUFS;
	}

	return 0;
}

/*====================================================================*
 *   
 * qcaspi_netdev_uninit - Uninitialize the network adapter.
 *
 * This function is call to un-initialize the network adapter.
 *
 * Return:   N/A
 *   
 *--------------------------------------------------------------------*/

static void
qcaspi_netdev_uninit(struct net_device *dev) 
{
	struct qcaspi *qca = netdev_priv(dev);
	kfree(qca->rx_buffer);
	qca->buffer_size = 0;
	if (qca->rx_skb)
		dev_kfree_skb(qca->rx_skb);
}

/*====================================================================*
 *   
 * qcaspi_netdev_change_mtu - change MTU value.
 *
 * This function changes the MTU value.
 *
 * Return:   0 on success, else failure
 *   
 *--------------------------------------------------------------------*/

int
qcaspi_netdev_change_mtu(struct net_device *dev, int new_mtu) 
{
	if ((new_mtu < QCAFRM_ETHMINMTU) || (new_mtu > QCAFRM_ETHMAXMTU)) {
		return -EINVAL;
	}

	dev->mtu = new_mtu;

	return 0;
}

/*====================================================================*
 *   
 * qcaspi_netdev_set_mac_address - set the MAC address.
 *
 * This function is use to change the MAC address of the network adapter. The
 * newly MAC address is not persistent.
 *
 * Return:   0 on success, else failure
 *   
 *--------------------------------------------------------------------*/

static int
qcaspi_netdev_set_mac_address(struct net_device *dev, void *p) 
{
	struct sockaddr *addr = p;
	if (netif_running(dev)) {
		return -EBUSY;
	}
	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);
	printk("qcaspi: Setting MAC address to %pM.\n", dev->dev_addr);
	return 0;
}

static const struct net_device_ops qcaspi_netdev_ops = {
	.ndo_init = qcaspi_netdev_init,
	.ndo_uninit = qcaspi_netdev_uninit,
	.ndo_open = qcaspi_netdev_open,
	.ndo_stop = qcaspi_netdev_close,
	.ndo_start_xmit = qcaspi_netdev_xmit,
	.ndo_get_stats = qcaspi_netdev_get_stats,
	.ndo_change_mtu = qcaspi_netdev_change_mtu,
	.ndo_set_mac_address = qcaspi_netdev_set_mac_address,
	.ndo_tx_timeout = qcaspi_netdev_tx_timeout,
};

/*====================================================================*
 *   
 * qcaspi_netdev_setup - init function.
 *
 * The init function (sometimes called probe). It is invoked by
 * alloc_netdev().
 *
 *--------------------------------------------------------------------*/

void
qcaspi_netdev_setup(struct net_device *dev) 
{
	struct qcaspi *qca = NULL;

	ether_setup(dev);

	dev->netdev_ops = &qcaspi_netdev_ops;

	dev->watchdog_timeo = QCASPI_TX_TIMEOUT;
	dev->flags = IFF_MULTICAST;
	dev->tx_queue_len = 100;

	/* Default MAC address. */
	memcpy(dev->dev_addr, QCASPI_DEF_MAC_ADDRESS, dev->addr_len);

	qca = netdev_priv(dev);
	memset(qca, 0, sizeof(struct qcaspi));
}

/*====================================================================*
 *   
 * qcaspi_mod_init - Module initialize function.
 *
 * This is the entry point of the kernel module. It is called when the module
 * is loaded. It registers SPI and network devices.
 *
 * Return:   0 on success, else failure
 *   
 *--------------------------------------------------------------------*/

static int __init
qcaspi_mod_init(void) 
{
	struct spi_board_info *spi_board = NULL;
	struct spi_master *spi_master = NULL;
	struct spi_device *spi_device = NULL;
	struct qcaspi *qca = NULL;

	printk(KERN_INFO "qcaspi: version %s\n", QCASPI_VERSION);

	/* Validate module parameters. */
	if ((qcaspi_clkspeed < QCASPI_CLK_SPEED_MIN) ||
	    (qcaspi_clkspeed > QCASPI_CLK_SPEED_MAX) ||
	    (qcaspi_legacy_mode < QCASPI_LEGACY_MODE_MIN) ||
	    (qcaspi_legacy_mode > QCASPI_LEGACY_MODE_MAX) ||
	    (qcaspi_burst_len < QCASPI_BURST_LEN_MIN) ||
	    (qcaspi_burst_len > QCASPI_BURST_LEN_MAX)) 
	{
		printk(KERN_INFO "qcaspi: Invalid parameters "
		    "(clk=%dHz, legacy_mode=%d, burst_len=%d)\n",
		    qcaspi_clkspeed, qcaspi_legacy_mode, qcaspi_burst_len);
		return -EINVAL;
	}

	/* Retrieve the SPI master bus information. */
	spi_master = spi_busnum_to_master(QCASPI_BUS_ID);
	if (!spi_master) {
		kfree(spi_board);
		printk(KERN_ERR "qcaspi: Unable to locate SPI master device on bus %d\n", QCASPI_BUS_ID);
		return -ENOMEM;
	}
	printk(KERN_INFO "qcaspi: SPI bus master retrieve from bus number %d\n", QCASPI_BUS_ID);

	/* Create new SPI device */
	spi_device = spi_new_device(spi_master, &qca_spi_board_info);
	if (!spi_device) {
		printk(KERN_ERR "qcaspi: Unable to create new SPI device on bus %d\n", QCASPI_BUS_ID);
		return -ENODEV;
	}

	/* Fill new allocated spi board info. */
	spi_device->max_speed_hz = qcaspi_clkspeed;
	if (spi_setup(spi_device) < 0) {
		kfree(spi_device);
		printk(KERN_ERR "qcaspi: Unable to setup SPI bus %d\n", QCASPI_BUS_ID);
		return -EFAULT;
	}
	printk(KERN_INFO "qcaspi: SPI device create (clk=%dHz, "
	    "legacy_mode=%d, burst_len=%d)\n",
	    qcaspi_clkspeed, qcaspi_legacy_mode, qcaspi_burst_len);

	/* Allocate the devices */
	qcaspi_devs = alloc_netdev(sizeof(struct qcaspi), "qca%d", qcaspi_netdev_setup);
	if (!qcaspi_devs) {
		kfree(spi_device);
		printk(KERN_ERR "qcaspi: Unable to allocate memory for spi network device\n");
		return -ENOMEM;
	}
	qca = netdev_priv(qcaspi_devs);
	if (!qca) {
		free_netdev(qcaspi_devs);
		kfree(spi_device);
		printk(KERN_ERR "qcaspi: Fail to retrieve private structure from net device\n");
		return -ENOMEM;
	}
	qca->dev = qcaspi_devs;
	qca->spi_board = &qca_spi_board_info;
	qca->spi_master = spi_master;
	qca->spi_device = spi_device;

	netif_carrier_off(qca->dev);

	/* Register network device */
	if (register_netdev(qcaspi_devs)) {
		kfree(spi_device);
		printk(KERN_ERR "qcaspi: Unable to register network device %s\n", qcaspi_devs->name);
		free_netdev(qcaspi_devs);
		return -EFAULT;
	}
	printk(KERN_ERR "qcaspi: Driver loaded (%s)\n", qcaspi_devs->name);

	return 0;
}

/*====================================================================*
 *   
 * qcaspi_mod_exit - Exit function of the module.
 *
 * The function is called when the module is unloaded. It unregister network
 * SPI devices.
 *
 * Return:   N/A
 *   
 *--------------------------------------------------------------------*/

static void __exit
qcaspi_mod_exit(void) 
{
	struct qcaspi *qca = netdev_priv(qcaspi_devs);
	unregister_netdev(qcaspi_devs);
	printk(KERN_ERR "qcaspi: Driver unloaded (%s)\n", qcaspi_devs->name);
	spi_unregister_device(qca->spi_device);
	free_netdev(qcaspi_devs);
}

/*====================================================================*
 *   
 *--------------------------------------------------------------------*/

module_init(qcaspi_mod_init);
module_exit(qcaspi_mod_exit);

/*====================================================================*
 *
 *--------------------------------------------------------------------*/

MODULE_DESCRIPTION("Qualcomm Atheros SPI Driver");
MODULE_AUTHOR("Qualcomm Atheros Communications");
MODULE_LICENSE("GPL");
