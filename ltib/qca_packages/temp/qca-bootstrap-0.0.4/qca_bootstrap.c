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
 *   qca_bootstrap.c
 *
 *   This module allows reading of the bootstrap parameters in the proc
 *   filesystem.
 *				
 *--------------------------------------------------------------------*/

/*====================================================================*
 *   system header files;
 *--------------------------------------------------------------------*/

#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/version.h>

#include <mach/qca_bootstrap.h>

/*====================================================================*
 *   constants;
 *--------------------------------------------------------------------*/

#define QCABOOTSTRAP_VERSION "0.0.4"
#define QCABOOTSTRAP_MODNAME "qcabootstrap"

#define PROC_FILENAME "qca_bootstrap"

/*====================================================================*
 *   variables;
 *--------------------------------------------------------------------*/

static struct proc_dir_entry *proc_file;
static char output_data[512];
static int output_len;

int
qca_readproc(char *buffer, char **start, off_t offset, int count, int *peof, void *data)
{
	pr_debug("qca_readproc: offset: %lu, count: %d\n", offset, count);

	if (offset > output_len) {
		*peof = 1;
		return 0;
	}
		
	if (output_data + offset + count > output_data + output_len)
		count = output_len - offset;

	memcpy(buffer + offset, output_data + offset, count);

	if (count + offset == output_len)
		*peof = 1;

	return offset + count;
}

/*====================================================================*
 *   
 * qcabootstrap_mod_init - Module initialize function.
 *
 * This is the entry point of the kernel module.
 *
 * Return:   0 on success, else failure
 *   
 *--------------------------------------------------------------------*/

static int __init
qcabootstrap_mod_init(void) 
{
	uint32_t value;
	uint32_t mode;
	char *host_mode;

	value = qca_read_bootstrap();
	mode = (value & QCA_HOST_MODE) >> QCA_HOST_MODE_SHIFT;
	switch (mode) {
	case QCA_HOST_MODE_UART:
		host_mode = "uart";
		break;
	case QCA_HOST_MODE_SPI:
		host_mode = "spi";
		break;
	case QCA_HOST_MODE_DISABLED:
		host_mode = "disabled";
		break;
	default:
		host_mode = "unknown";
		break;
	}

	output_len = snprintf(output_data, sizeof(output_data), "QCA_SPI_UART_SELECT=\"%s\"\n"
	    "QCA_RS232_RS485_SELECT=\"%s\"\n"
	    "QCA_XCVR_ENABLED=\"%s\"\n"
	    "QCA_DISABLE_OUTPUTS=\"%s\"\n"
	    "QCA_HOST_MODE=\"%s\"\n",
	    (value & QCA_SPI_UART_SELECT) ? "uart" : "spi",
	    (value & QCA_RS232_RS485_SELECT) ? "rs485" : "rs232",
	    (value & QCA_XCVR_ENABLED) ? "yes" : "no",
	    (value & QCA_DISABLE_OUTPUTS) ? "yes" : "no",
	    host_mode);

	printk(KERN_DEBUG "qcabootstrap: output_len is %d\n", output_len);

	printk(KERN_INFO "qcabootstrap: version %s\n", QCABOOTSTRAP_VERSION);

	proc_file = create_proc_read_entry(PROC_FILENAME, 0444, NULL, qca_readproc, NULL);
	if (proc_file == NULL) {
		return -EAGAIN;
	}
	printk(KERN_INFO "qcabootstrap: created file /proc/%s.\n", PROC_FILENAME);

	return 0;
}

/*====================================================================*
 *   
 * qcabootstrap_mod_exit - Exit function of the module.
 *
 * The function is called when the module is unloaded.
 *
 * Return:   N/A
 *   
 *--------------------------------------------------------------------*/

static void __exit
qcabootstrap_mod_exit(void) 
{
	remove_proc_entry(PROC_FILENAME, NULL);
	printk(KERN_INFO "qcabootstrap: Driver unloaded.\n");
}

/*====================================================================*
 *   
 *--------------------------------------------------------------------*/

module_init(qcabootstrap_mod_init);
module_exit(qcabootstrap_mod_exit);

/*====================================================================*
 *
 *--------------------------------------------------------------------*/

MODULE_DESCRIPTION("Qualcomm Atheros Bootstrap Driver");
MODULE_AUTHOR("Qualcomm Atheros Communications");
MODULE_LICENSE("ISC");
