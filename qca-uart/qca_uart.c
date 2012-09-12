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
 *   qca_uart.c
 *
 *   This module implements the Qualcomm Atheros UART protocol for
 *   kernel-based UART device;
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
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/netdevice.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/types.h>
#include <linux/tty.h>
#include <linux/version.h>

/*====================================================================*
 *   custom header files;
 *--------------------------------------------------------------------*/

#include "qca_uart.h"
#include "qca_framing.h"

/*====================================================================*
 *   constants;
 *--------------------------------------------------------------------*/

#define QCAUART_VERSION "0.0.5"
#define QCAUART_MODNAME "qcauart"
#define QCAUART_DEF_MAC_ADDRESS "\x00\xB0\x52\xFF\xFF\x02"
#define QCAUART_MTU QCAFRM_ETHMAXMTU
#define QCAUART_TX_TIMEOUT (1 * HZ)

/*====================================================================*
 *   variables;
 *--------------------------------------------------------------------*/

static struct net_device *qcauart_devs;

void
qca_tty_receive(struct tty_struct *tty, const unsigned char *cp, char *fp, int count)
{
	struct qcauart *qca = netdev_priv(qcauart_devs);

	pr_debug("qcauart: qca_tty_receive(), %d bytes\n", count);
	/* print_hex_dump(KERN_DEBUG, "tty_receive: ", DUMP_PREFIX_ADDRESS, 16, 1, cp, count, true); */

	if (qca->rx_skb == NULL) {
		qca->rx_skb = dev_alloc_skb(qca->dev->mtu + VLAN_ETH_HLEN);
		if (qca->rx_skb == NULL) {
			printk(KERN_DEBUG "qcauart: out of RX resources\n");
			return;
		}
	}

	while (count-- && qca->rx_skb) {
		int32_t retcode = QcaFrmFsmDecode(&qca->lFrmHdl, qca->rx_skb->data, skb_tailroom(qca->rx_skb), *cp++);
		switch (retcode) {
		case QCAFRM_GATHER:
			break;

		case QCAFRM_NOHEAD:
			break;

		case QCAFRM_NOTAIL:
			pr_debug("qcauart: no RX tail\n");
			qca->stats.rx_errors++;
			qca->stats.rx_dropped++;
			break;

		case QCAFRM_INVLEN:
			pr_debug("qcauart: invalid RX length\n");
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
				printk(KERN_DEBUG "qcauart: out of RX resources\n");
				qca->stats.rx_errors++;
				break;
			}
		}
	}
}

void
qca_tty_wakeup(struct tty_struct *tty)
{
	struct qcauart *qca = netdev_priv(qcauart_devs);
	int written;

	if (qca->tx_skb->len == 0) {
		dev_kfree_skb(qca->tx_skb);
		qca->tx_skb = NULL;
		clear_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
		pr_debug("qcauart: qca_tty_wakeup() exit early\n");
		++qca->stats.tx_packets;
		netif_wake_queue(qca->dev);
		return;
	}

	written = tty->ops->write(qca->tty, qca->tx_skb->data, qca->tx_skb->len);
	qca->stats.tx_bytes += written;
	skb_pull(qca->tx_skb, written);
	pr_debug("qcauart: xmit wrote %d bytes, %d left\n", written, qca->tx_skb->len);
}

int
qca_tty_open(struct tty_struct *tty)
{
	struct qcauart *qca = netdev_priv(qcauart_devs);

	pr_debug("qcauart: qca_tty_open()\n");

	qca->tty = tty;

	netif_carrier_on(qca->dev);

	return 0;
}

void
qca_tty_close(struct tty_struct *tty)
{
	struct qcauart *qca = netdev_priv(qcauart_devs);
	pr_debug("qcauart: qca_tty_close()\n");
	netif_carrier_off(qca->dev);
}

static struct tty_ldisc_ops qca_ldisc = {
	.owner  = THIS_MODULE,
	.magic	= TTY_LDISC_MAGIC,
	.name	= "qca",
	.open	= qca_tty_open,
	.close	= qca_tty_close,
	.receive_buf = qca_tty_receive,
	.write_wakeup = qca_tty_wakeup,
};

/*====================================================================*
 *   
 * qcauart_netdev_open - open network adapter.
 *
 * This function is use by the network stack to open the network device.
 *
 * Return:   0 on success, else failure
 *   
 *--------------------------------------------------------------------*/

int
qcauart_netdev_open(struct net_device *dev) 
{
	struct qcauart *qca = netdev_priv(dev);

	qca->tx_skb = NULL;

	QcaFrmFsmInit(&qca->lFrmHdl);

	netif_start_queue(qca->dev);

	return 0;
}

/*====================================================================*
 *   
 * qcauart_netdev_close - Close network device.
 *
 * This function is use by the network stack to close the network device.
 *
 * Return:   0 on success, else failure
 *   
 *--------------------------------------------------------------------*/

int
qcauart_netdev_close(struct net_device *dev) 
{
	struct qcauart *qca = netdev_priv(dev);

	/* can't transmit any more. */
	netif_stop_queue(dev);

	if (qca->tx_skb) {
		dev_kfree_skb(qca->tx_skb);
		qca->tx_skb = NULL;
	}

	return 0;
}

/*====================================================================*
 *   
 * qcauart_netdev_xmit - Transmit a packet (called by the kernel).
 *
 * This function is called by the network stack to send an Ethernet frame.
 *
 * Return:   0 on success, else failure
 *   
 *--------------------------------------------------------------------*/

netdev_tx_t
qcauart_netdev_xmit(struct sk_buff *skb, struct net_device *dev) 
{
	uint32_t frame_len;
	uint8_t *ptmp;
	struct qcauart *qca = netdev_priv(dev);
	struct sk_buff *tskb;
	uint8_t pad_len = 0;
	int written;

	/* need padding? */
	if (skb->len < QCAFRM_ETHMINLEN) {
		pad_len = QCAFRM_ETHMINLEN - skb->len;
	}

	/* not enough head, tail room, or a runt frame, must copy data */
	if (skb_headroom(skb) < QCAFRM_HEADER_LEN || skb_tailroom(skb) < QCAFRM_FOOTER_LEN + pad_len) {
		tskb = skb_copy_expand(skb, QCAFRM_HEADER_LEN, QCAFRM_FOOTER_LEN + pad_len, GFP_ATOMIC);
		if (tskb == NULL) {
			printk(KERN_DEBUG "qcauart: could not allocate tx_buff in qcauart_netdev_xmit\n");
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

	/* print_hex_dump(KERN_DEBUG, "xmit_frame: ", DUMP_PREFIX_ADDRESS, 16, 1, skb->data, skb->len, true); */
	pr_debug("qcauart: Tx-ing packet: Size: 0x%08x\n", skb->len);

	netif_stop_queue(qca->dev);

	set_bit(TTY_DO_WRITE_WAKEUP, &qca->tty->flags);
	written = qca->tty->ops->write(qca->tty, skb->data, skb->len);
	qca->stats.tx_bytes += written;
	pr_debug(KERN_DEBUG "qcauart: xmit wrote %d bytes\n", written);
	skb_pull(skb, written);

	qca->tx_skb = skb;

	dev->trans_start = jiffies;

	return NETDEV_TX_OK;
}


/*====================================================================*
 *   
 * qcauart_netdev_tx_timeout - Transmit timeout function.
 *
 * This function deals with a transmit timeout.
 *
 * Return:   N/A
 *   
 *--------------------------------------------------------------------*/

void
qcauart_netdev_tx_timeout(struct net_device *dev) 
{
	struct qcauart *qca = netdev_priv(dev);
	printk(KERN_INFO "qcauart: Transmit timeout at %ld, latency %ld\n", jiffies, jiffies - dev->trans_start);
	qca->stats.tx_errors++;
	qca->stats.tx_dropped++;

	clear_bit(TTY_DO_WRITE_WAKEUP, &qca->tty->flags);

	if (qca->tx_skb) {
		dev_kfree_skb(qca->tx_skb);
		qca->tx_skb = NULL;
	}

	netif_wake_queue(dev);
}


/*====================================================================*
 *   
 * qcauart_netdev_get_stats - Return statistics to the caller.
 *
 * This function returns the device statistics to the caller.
 *
 * Return:   Device statistics
 *   
 *--------------------------------------------------------------------*/

struct net_device_stats *
qcauart_netdev_get_stats(struct net_device *dev) 
{
	struct qcauart *qca = netdev_priv(dev);
	return &qca->stats;
}

/*====================================================================*
 *   
 * qcauart_netdev_init - Netdevice register callback.
 *
 * This is the callback function for the Netdevice register function.
 *
 * Return:   0 on success, else failure
 *   
 *--------------------------------------------------------------------*/

static int
qcauart_netdev_init(struct net_device *dev) 
{
	struct qcauart *qca = netdev_priv(dev);
	
	/* Finish setting up the device info. */
	dev->mtu = QCAUART_MTU;
	dev->type = ARPHRD_ETHER;

	qca->rx_skb = dev_alloc_skb(qca->dev->mtu + VLAN_ETH_HLEN);
	if (qca->rx_skb == NULL) {
		printk(KERN_INFO "qcauart: Failed to allocate RX sk_buff.\n");
		return -ENOBUFS;
	}

	return 0;
}

/*====================================================================*
 *   
 * qcauart_netdev_uninit - Uninitialize the network adapter.
 *
 * This function is call to un-initialize the network adapter.
 *
 * Return:   N/A
 *   
 *--------------------------------------------------------------------*/

static void
qcauart_netdev_uninit(struct net_device *dev) 
{
	struct qcauart *qca = netdev_priv(dev);
	if (qca->rx_skb)
		dev_kfree_skb(qca->rx_skb);
}

/*====================================================================*
 *   
 * qcauart_netdev_change_mtu - change MTU value.
 *
 * This function changes the MTU value.
 *
 * Return:   0 on success, else failure
 *   
 *--------------------------------------------------------------------*/

int
qcauart_netdev_change_mtu(struct net_device *dev, int new_mtu) 
{
	if ((new_mtu < QCAFRM_ETHMINMTU) || (new_mtu > QCAFRM_ETHMAXMTU)) {
		return -EINVAL;
	}

	dev->mtu = new_mtu;

	return 0;
}

/*====================================================================*
 *   
 * qcauart_netdev_set_mac_address - set the MAC address.
 *
 * This function is use to change the MAC address of the network adapter. The
 * newly MAC address is not persistent.
 *
 * Return:   0 on success, else failure
 *   
 *--------------------------------------------------------------------*/

static int
qcauart_netdev_set_mac_address(struct net_device *dev, void *p) 
{
	struct sockaddr *addr = p;
	if (netif_running(dev)) {
		return -EBUSY;
	}
	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);
	printk("qcauart: Setting MAC address to %pM.\n", dev->dev_addr);
	return 0;
}

static const struct net_device_ops qcauart_netdev_ops = {
	.ndo_init = qcauart_netdev_init,
	.ndo_uninit = qcauart_netdev_uninit,
	.ndo_open = qcauart_netdev_open,
	.ndo_stop = qcauart_netdev_close,
	.ndo_start_xmit = qcauart_netdev_xmit,
	.ndo_get_stats = qcauart_netdev_get_stats,
	.ndo_change_mtu = qcauart_netdev_change_mtu,
	.ndo_set_mac_address = qcauart_netdev_set_mac_address,
	.ndo_tx_timeout = qcauart_netdev_tx_timeout,
};

/*====================================================================*
 *   
 * qcauart_netdev_setup - init function.
 *
 * The init function (sometimes called probe). It is invoked by
 * alloc_netdev().
 *
 *--------------------------------------------------------------------*/

void
qcauart_netdev_setup(struct net_device *dev) 
{
	struct qcauart *qca = NULL;

	ether_setup(dev);

	dev->netdev_ops = &qcauart_netdev_ops;

	dev->watchdog_timeo = QCAUART_TX_TIMEOUT;
	dev->flags = IFF_MULTICAST;
	dev->tx_queue_len = 100;

	/* Default MAC address. */
	memcpy(dev->dev_addr, QCAUART_DEF_MAC_ADDRESS, dev->addr_len);

	qca = netdev_priv(dev);
	memset(qca, 0, sizeof(struct qcauart));
}

/*====================================================================*
 *   
 * qcauart_mod_init - Module initialize function.
 *
 * This is the entry point of the kernel module. It is called when the module
 * is loaded. It registers UART and network devices.
 *
 * Return:   0 on success, else failure
 *   
 *--------------------------------------------------------------------*/

static int __init
qcauart_mod_init(void) 
{
	struct qcauart *qca = NULL;
	int status;

	printk(KERN_INFO "qcauart: version %s\n", QCAUART_VERSION);

	/* Validate module parameters. */
	if (0)
	{
		return -EINVAL;
	}

	/* Register line discipline */
	status = tty_register_ldisc(N_QCA, &qca_ldisc);
	if (status != 0) {
		printk(KERN_ERR "qcauart: can't register line discipline (err = %d)\n", status);
		return status;
	}

	/* Allocate the devices */
	qcauart_devs = alloc_netdev(sizeof(struct qcauart), "qca%d", qcauart_netdev_setup);
	if (!qcauart_devs) {
		printk(KERN_ERR "qcauart: Unable to allocate memory for UART network device\n");
		return -ENOMEM;
	}
	qca = netdev_priv(qcauart_devs);
	if (!qca) {
		free_netdev(qcauart_devs);
		printk(KERN_ERR "qcauart: Fail to retrieve private structure from net device\n");
		return -ENOMEM;
	}
	qca->dev = qcauart_devs;

	netif_carrier_off(qca->dev);

	/* Register network device */
	if (register_netdev(qcauart_devs)) {
		printk(KERN_ERR "qcauart: Unable to register network device %s\n", qcauart_devs->name);
		free_netdev(qcauart_devs);
		return -EFAULT;
	}

	printk(KERN_ERR "qcauart: Driver loaded (%s)\n", qcauart_devs->name);

	return 0;
}

/*====================================================================*
 *   
 * qcauart_mod_exit - Exit function of the module.
 *
 * The function is called when the module is unloaded. It unregister network
 * UART devices.
 *
 * Return:   N/A
 *   
 *--------------------------------------------------------------------*/

static void __exit
qcauart_mod_exit(void) 
{
	int r;
	unregister_netdev(qcauart_devs);
	r = tty_unregister_ldisc(N_QCA);
	if (r != 0)
		printk(KERN_ERR "qcauart: can't unregister line discipline (err = %d)\n", r);
	printk(KERN_ERR "qcauart: Driver unloaded (%s)\n", qcauart_devs->name);
	free_netdev(qcauart_devs);
}

/*====================================================================*
 *   
 *--------------------------------------------------------------------*/

module_init(qcauart_mod_init);
module_exit(qcauart_mod_exit);

/*====================================================================*
 *
 *--------------------------------------------------------------------*/

MODULE_DESCRIPTION("Qualcomm Atheros UART Driver");
MODULE_AUTHOR("Qualcomm Atheros Communications");
MODULE_LICENSE("ISC");
