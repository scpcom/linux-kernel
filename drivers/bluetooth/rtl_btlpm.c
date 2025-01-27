/*
 *  TI Bluesleep driver
 *	Kernel module responsible for Wake up of Host
 *  Copyright (C) 2009-2010 Texas Instruments


 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * Copyright (C) 2006-2007 - Motorola
 * Copyright (c) 2008-2009, Code Aurora Forum. All rights reserved.
 *
 *  Date         Author           Comment
 * -----------  --------------   --------------------------------
 * 2006-Apr-28  Motorola         The kernel module for running the Bluetooth(R)
 *                               Sleep-Mode Protocol from the Host side
 * 2006-Sep-08  Motorola         Added workqueue for handling sleep work.
 * 2007-Jan-24  Motorola         Added mbm_handle_ioi() call to ISR.
 * 2009-Aug-10  Motorola         Changed "add_timer" to "mod_timer" to solve
 *                               race when flurry of queued work comes in.
*/
#define DEBUG

#include <linux/module.h>       /* kernel module definitions */
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/proc_fs.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/pm_wakeirq.h>

#include <net/bluetooth/bluetooth.h>

/*
 * #define BT_SLEEP_DBG
 */
#undef BT_DBG
#undef BT_ERR
#ifdef BT_SLEEP_DBG
#define BT_DBG(fmt, arg...) pr_debug("[BT_LPM] %s: " fmt "\n",\
				__func__, ## arg)
#else
#define BT_DBG(fmt, arg...)
#endif
#define BT_ERR(fmt, arg...) pr_err("[BT_LPM] %s: " fmt "\n",\
				__func__, ## arg)

/*
 * Defines
 */
#define VERSION	 "1.2.3"
#define PROC_DIR	"bluetooth/sleep"

#define DEFAULT_UART_INDEX   1


static void bluesleep_stop(void);
static int bluesleep_start(void);

struct bluesleep_info {
	unsigned int wakeup_enable;
	unsigned host_wake;
	unsigned ext_wake;
	unsigned host_wake_irq;
	struct wakeup_source *ws;
	struct uart_port *uport;
	unsigned host_wake_assert:1;
	unsigned ext_wake_assert:1;
	struct platform_device *pdev;
};

/* work function */
static void bluesleep_sleep_work(struct work_struct *work);

/* work queue */
DECLARE_DELAYED_WORK(sleep_workqueue, bluesleep_sleep_work);

/* Macros for handling sleep work */
#define bluesleep_rx_busy()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_tx_busy()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_rx_idle()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_tx_idle()     schedule_delayed_work(&sleep_workqueue, 0)

/* 10 second timeout */
#define TX_TIMER_INTERVAL	10

/* state variable names and bit positions */
#define BT_PROTO	0x01
#define BT_TXDATA	0x02
#define BT_ASLEEP	0x04

/* variable use indicate lpm modle */
static bool has_lpm_enabled;

/* struct use save platform_device from uart */
static struct platform_device *bluesleep_uart_dev;

static struct bluesleep_info *bsi;

/* module usage */
static atomic_t open_count = ATOMIC_INIT(1);

/*
 * Global variables
 */

/** Global state flags */
static unsigned long flags;

/** Tasklet to respond to change in hostwake line */
static struct tasklet_struct hostwake_task;

/** Transmission timer */
static struct timer_list tx_timer;

/** Lock for state transitions */
static spinlock_t rw_lock;


struct proc_dir_entry *bluetooth_dir, *sleep_dir;

/*
 * Local functions
 */

/**
 * @return 1 if the Host can go to sleep, 0 otherwise.
 */
static inline int bluesleep_can_sleep(void)
{
	/* check if BT_WAKE_HOST_GPIO deasserted */
	return (gpio_get_value(bsi->host_wake) != bsi->host_wake_assert) &&
		(bsi->uport != NULL);
}

/*
 * after bt wakeup should clean BT_ASLEEP flag and start time.
 */
void bluesleep_sleep_wakeup(void)
{
	if (test_bit(BT_ASLEEP, &flags)) {
		BT_DBG("waking up...");
		/* Start the timer */
		mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
		clear_bit(BT_ASLEEP, &flags);
	}
}

/**
 * @brief@  main sleep work handling function which update the flags
 * and activate and deactivate UART ,check FIFO.
 */
static void bluesleep_sleep_work(struct work_struct *work)
{
	if (bluesleep_can_sleep()) {
		/* already asleep, this is an error case */
		if (test_bit(BT_ASLEEP, &flags)) {
			BT_DBG("already asleep");
			return;
		}
		if (bsi->uport->ops->tx_empty(bsi->uport)) {
			BT_DBG("going to sleep...");
			set_bit(BT_ASLEEP, &flags);
			__pm_wakeup_event(bsi->ws, HZ / 2);
		} else {
			mod_timer(&tx_timer,
					jiffies + (TX_TIMER_INTERVAL * HZ));
			return;
		}
	} else if (!test_bit(BT_ASLEEP, &flags)) {
		mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
	} else {
		bluesleep_sleep_wakeup();
	}
}

/**
 * A tasklet function that runs in tasklet context and reads the value
 * of the HOST_WAKE GPIO pin and further defer the work.
 * @param data Not used.
 */
static void bluesleep_hostwake_task(unsigned long data)
{
	BT_DBG("hostwake line change");
	spin_lock(&rw_lock);

	if (gpio_get_value(bsi->host_wake) == bsi->host_wake_assert)
		bluesleep_rx_busy();
	else
		bluesleep_rx_idle();

	spin_unlock(&rw_lock);
}

static struct uart_port *bluesleep_get_uart_port(void)
{
	struct uart_port *uport = NULL;

	if (bluesleep_uart_dev) {
		uport = platform_get_drvdata(bluesleep_uart_dev);
		if (uport)
			BT_DBG(
			"%s get uart_port from blusleep_uart_dev: %s, port irq: %d",
					__func__, bluesleep_uart_dev->name,
					uport->irq);
	}
	return uport;
}

static int bluesleep_lpm_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "lpm enable: %d\n", has_lpm_enabled);
	return 0;
}

static int bluesleep_lpm_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bluesleep_lpm_proc_show, NULL);
}

static ssize_t bluesleep_write_proc_lpm(struct file *file,
				const char __user *buffer,
				size_t count, loff_t *pos)
{
	char b;

	if (count < 1)
		return -EINVAL;

	if (copy_from_user(&b, buffer, 1))
		return -EFAULT;

	if (b == '0') {
		/* HCI_DEV_UNREG */
		bluesleep_stop();
		has_lpm_enabled = false;
		bsi->uport = NULL;
	} else {
		/* HCI_DEV_REG */
		if (!has_lpm_enabled) {
			has_lpm_enabled = true;
			if (bluesleep_uart_dev)
				bsi->uport = bluesleep_get_uart_port();

			/* if bluetooth started, start bluesleep*/
			bluesleep_start();
		}
	}

	return count;
}

static const struct proc_ops lpm_fops = {
	.proc_open		= bluesleep_lpm_proc_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release,
	.proc_write		= bluesleep_write_proc_lpm,
};

/**
 * Handles transmission timer expiration.
 * @param data Not used.
 */
static void bluesleep_tx_timer_expire(struct timer_list *t)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&rw_lock, irq_flags);

	BT_DBG("Tx timer expired");

	/* were we silent during the last timeout */
	if (!test_bit(BT_TXDATA, &flags)) {
		BT_DBG("Tx has been idle");
		bluesleep_tx_idle();
	} else {
		BT_DBG("Tx data during last period");
		mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL*HZ));
	}

	/* clear the incoming data flag */
	clear_bit(BT_TXDATA, &flags);

	spin_unlock_irqrestore(&rw_lock, irq_flags);
}

/**
 * Schedules a tasklet to run when receiving an interrupt on the
 * <code>HOST_WAKE</code> GPIO pin.
 * @param irq Not used.
 * @param dev_id Not used.
 */
static irqreturn_t bluesleep_hostwake_isr(int irq, void *dev_id)
{
	/* schedule a tasklet to handle the change in the host wake line */
	tasklet_schedule(&hostwake_task);
	__pm_stay_awake(bsi->ws);
	return IRQ_HANDLED;
}

/**
 * Starts the Sleep-Mode Protocol on the Host.
 * @return On success, 0. On error, -1, and <code>errno</code> is set
 * appropriately.
 */
static int bluesleep_start(void)
{
	int retval;
	unsigned long irq_flags;

	spin_lock_irqsave(&rw_lock, irq_flags);

	if (test_bit(BT_PROTO, &flags)) {
		spin_unlock_irqrestore(&rw_lock, irq_flags);
		return 0;
	}

	spin_unlock_irqrestore(&rw_lock, irq_flags);

	if (!atomic_dec_and_test(&open_count)) {
		atomic_inc(&open_count);
		return -EBUSY;
	}

	/* start the timer */
	mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL*HZ));

	retval = request_irq(bsi->host_wake_irq, bluesleep_hostwake_isr,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				"bluetooth hostwake", &bsi->pdev->dev);
	if (retval  < 0) {
		BT_ERR("Couldn't acquire BT_HOST_WAKE IRQ");
		goto fail;
	}

	set_bit(BT_PROTO, &flags);
	__pm_stay_awake(bsi->ws);

	return 0;
fail:
	del_timer(&tx_timer);
	atomic_inc(&open_count);

	return retval;
}

/**
 * Stops the Sleep-Mode Protocol on the Host.
 */
static void bluesleep_stop(void)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&rw_lock, irq_flags);

	if (!test_bit(BT_PROTO, &flags)) {
		spin_unlock_irqrestore(&rw_lock, irq_flags);
		return;
	}

	del_timer(&tx_timer);
	clear_bit(BT_PROTO, &flags);

	if (test_bit(BT_ASLEEP, &flags))
		clear_bit(BT_ASLEEP, &flags);

	atomic_inc(&open_count);

	spin_unlock_irqrestore(&rw_lock, irq_flags);
	free_irq(bsi->host_wake_irq, &bsi->pdev->dev);
	__pm_wakeup_event(bsi->ws, HZ / 2);
}

static int assert_level = -1;
module_param(assert_level, int, S_IRUGO);
MODULE_PARM_DESC(assert_level, "BT_LPM hostwake/btwake assert level");

static struct platform_device *sw_uart_get_pdev(int id)
{
	struct device_node *np;
	char match[20];
	sprintf(match, "uart%d", id);
	np = of_find_node_by_type(NULL, match);
	return of_find_device_by_node(np);
}

static int __init bluesleep_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	enum of_gpio_flags config;
	int ret, uart_index;
	u32 val;

	bsi = devm_kzalloc(&pdev->dev, sizeof(struct bluesleep_info),
			GFP_KERNEL);
	if (!bsi)
		return -ENOMEM;

	bsi->host_wake = of_get_named_gpio_flags(np, "bt_hostwake", 0, &config);
	if (!gpio_is_valid(bsi->host_wake)) {
		BT_ERR("get gpio bt_hostwake failed\n");
		return -EINVAL;
	}

	/* set host_wake_assert */
	bsi->host_wake_assert = (config == OF_GPIO_ACTIVE_LOW) ? 0 : 1;
	BT_DBG("bt_hostwake gpio=%d assert=%d\n", bsi->host_wake, bsi->host_wake_assert);

	if (assert_level != -1) {
		bsi->host_wake_assert = (assert_level & 0x02) > 0;
		BT_DBG("override host_wake assert to %d", bsi->host_wake_assert);
	}

	ret = devm_gpio_request(dev, bsi->host_wake, "bt_hostwake");
	if (ret < 0) {
		BT_ERR("can't request bt_hostwake gpio %d\n",
			bsi->host_wake);
		return ret;
	}
	ret = gpio_direction_input(bsi->host_wake);
	if (ret < 0) {
		BT_ERR("can't request input direction bt_wake gpio %d\n",
			bsi->host_wake);
		return ret;
	}

	if (!of_property_read_bool(np, "wakeup-source")) {
		BT_DBG("wakeup source is disabled!\n");
	} else {
		ret = device_init_wakeup(dev, true);
		if (ret < 0) {
			BT_ERR("device init wakeup failed!\n");
			return ret;
		}
		ret = dev_pm_set_wake_irq(dev, gpio_to_irq(bsi->host_wake));
		if (ret < 0) {
			BT_ERR("can't enable wakeup src for bt_hostwake %d\n",
				bsi->host_wake);
			return ret;
		}
		bsi->wakeup_enable = 1;
	}

	bsi->ext_wake = of_get_named_gpio_flags(np, "bt_wake",
			0, (enum of_gpio_flags *)&config);
	if (!gpio_is_valid(bsi->ext_wake)) {
		BT_ERR("get gpio bt_wake failed\n");
		return -EINVAL;
	}

	ret = devm_gpio_request(dev, bsi->ext_wake, "bt_wake");
	if (ret < 0) {
		BT_ERR("can't request bt_wake gpio %d\n",
			bsi->ext_wake);
		return ret;
	}

	/* set ext_wake_assert */
	bsi->ext_wake_assert = (config == OF_GPIO_ACTIVE_LOW) ? 0 : 1;
	BT_DBG("bt_wake gpio=%d assert=%d\n", bsi->ext_wake, bsi->ext_wake_assert);

	if (assert_level != -1) {
		bsi->ext_wake_assert = (assert_level & 0x01) > 0;
		BT_DBG("override ext_wake assert to %d", bsi->ext_wake_assert);
	}

	/* 1.set bt_wake as output and the level is assert, assert bt wake */
	ret = gpio_direction_output(bsi->ext_wake, bsi->ext_wake_assert);
	if (ret < 0) {
		BT_ERR("can't request output direction bt_wake gpio %d\n",
			bsi->ext_wake);
		return ret;
	}

	/* 2.get bt_host_wake gpio irq */
	bsi->host_wake_irq = gpio_to_irq(bsi->host_wake);
	if (bsi->host_wake_irq < 0) {
		BT_ERR("map gpio [%d] to virq failed, errno = %d\n",
				bsi->host_wake, bsi->host_wake_irq);
		ret = -ENODEV;
		return ret;
	}

	uart_index = DEFAULT_UART_INDEX;
	if (!of_property_read_u32(np, "uart_index", &val)) {
		switch (val) {
		case 0:
		case 1:
		case 2:
			uart_index = val;
			break;
		default:
			BT_ERR("unsupported uart_index (%u)\n", val);
		}
	}
	BT_DBG("uart_index (%u)\n", uart_index);
	bluesleep_uart_dev = sw_uart_get_pdev(uart_index);

	bsi->ws = wakeup_source_register(dev, "bluesleep");
	bsi->pdev = pdev;
	return 0;
}

static int bluesleep_remove(struct platform_device *pdev)
{
	if (test_bit(BT_PROTO, &flags)) {
		if (disable_irq_wake(bsi->host_wake_irq))
			BT_ERR("Couldn't disable hostwake IRQ wakeup mode\n");
		free_irq(bsi->host_wake_irq, &bsi->pdev->dev);
		del_timer(&tx_timer);
	}

	wakeup_source_unregister(bsi->ws);
	if (bsi->wakeup_enable) {
		BT_DBG("Deinit wakeup source");
		device_init_wakeup(&pdev->dev, false);
		dev_pm_clear_wake_irq(&pdev->dev);
	}

	return 0;
}

static int bluesleep_resume(struct platform_device *pdev)
{
	BT_DBG("%s", __func__);

	return 0;
}

static int bluesleep_suspend(struct platform_device *pdev, pm_message_t state)
{
	BT_DBG("%s", __func__);

	return 0;
}

static const struct of_device_id sunxi_btlpm_ids[] = {
	{ .compatible = "allwinner,sunxi-btlpm" },
	{ /* Sentinel */ }
};

static struct platform_driver bluesleep_driver = {
	.remove	= bluesleep_remove,
	.suspend = bluesleep_suspend,
	.resume = bluesleep_resume,
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= "sunxi-btlpm",
		.of_match_table	= sunxi_btlpm_ids,
	},
};

/**
 * Initializes the module.
 * @return On success, 0. On error, -1, and <code>errno</code> is set
 * appropriately.
 */
static int __init bluesleep_init(void)
{
	int retval;
	struct proc_dir_entry *ent;

	BT_DBG("BlueSleep Mode Driver Ver %s", VERSION);

	retval = platform_driver_probe(&bluesleep_driver, bluesleep_probe);
	if (retval)
		return retval;

	bluetooth_dir = proc_mkdir("bluetooth", NULL);
	if (bluetooth_dir == NULL) {
		BT_ERR("Unable to create /proc/bluetooth directory");
		return -ENOMEM;
	}

	sleep_dir = proc_mkdir("sleep", bluetooth_dir);
	if (sleep_dir == NULL) {
		BT_ERR("Unable to create /proc/%s directory", PROC_DIR);
		return -ENOMEM;
	}
	/* read/write proc entries */
	ent = proc_create("lpm", 0660, sleep_dir, &lpm_fops);
	if (ent == NULL) {
		BT_ERR("Unable to create /proc/%s/lpm entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}

	flags = 0; /* clear all status bits */

	/* Initialize spinlock. */
	spin_lock_init(&rw_lock);

	/* Initialize timer */
	timer_setup(&tx_timer, bluesleep_tx_timer_expire, 0);

	/* initialize host wake tasklet */
	tasklet_init(&hostwake_task, bluesleep_hostwake_task, 0);

	return 0;

fail:
	remove_proc_entry("lpm", sleep_dir);
	remove_proc_entry("sleep", bluetooth_dir);
	remove_proc_entry("bluetooth", 0);
	return retval;
}

/**
 * Cleans up the module.
 */
static void __exit bluesleep_exit(void)
{
	platform_driver_unregister(&bluesleep_driver);

	remove_proc_entry("lpm", sleep_dir);
	remove_proc_entry("sleep", bluetooth_dir);
	remove_proc_entry("bluetooth", 0);
}

module_init(bluesleep_init);
module_exit(bluesleep_exit);

MODULE_DESCRIPTION("Bluetooth Sleep Mode Driver ver %s " VERSION);
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
