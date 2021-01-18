/***************************************************************************
 *   Copyright (C) 2014 by Wojciech Domski                                 *
 *   wojciech.domski@gmail.com                                             *
 *                                                                         *
 *   This is a driver for jr3 sensor for Xenomai. It was based on driver   *
 *   by Mario Prats                                                        *
 *   mprats@icc.uji.es                                                     *
 *                                                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#define __MODULE__

#include <linux/module.h>

#ifdef DEBUG

	#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
	#define DBG_PRINT(fmt, args...) printk("%s:%d:%s(): " fmt, \
			__FILENAME__, __LINE__, __PRETTY_FUNCTION__, ##args)
#else
	#define DBG_PRINT(fmt, args...) do{}while(0)
#endif

#define JR3_DESC	"JR3 PCI force/torque sensor kernel module"
#define JR3_AUTHOR	"Wojciech Domski (wojciech.domski@gmail.com)"

MODULE_DESCRIPTION(JR3_DESC);
MODULE_AUTHOR(JR3_AUTHOR);
MODULE_LICENSE("GPL");

static int jr3_major = 0;

static LIST_HEAD( jr3_list);

static const char driver_name[] = "jr3";

#include <asm/uaccess.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/delay.h>

#include "jr3pci-driver.h"
#include "jr3pci-firmware.h"
#include "jr3pci-ioctl.h"
#include "jr3pci-devicename.h"

#define INIT_PCI_ENABLE				0x0001
#define INIT_PCI_REQUEST_REGIONS		0x0002
#define INIT_PCI_IOMAP				0x0004
#define INIT_PCI_REQUEST_IRQ			0x0008

int jr3_nr_devs = MAX_BOARD_COUNT;	/* maximum number of jr3 devices */

module_param(jr3_nr_devs, int, S_IRUGO);

int pci_registered; /* current number of registered jr3 devices */

static int jr3_open(struct inode *inode, struct file *filp);

static int jr3_release(struct inode *inode, struct file *filp);

static long jr3_ioctl(struct file *filp, 
	unsigned int request, unsigned long arg);

/** Board address to PCI virtual address conversion */
void __iomem *board2virtual(void __iomem *jr3_base_address, int ba) {
	return (void*)jr3_base_address+4*ba;
}

/** Read data memory */
short read_data(struct jr3_board *board, int ba, int card)
{
	return (short) readl(
		board2virtual(board->jr3_base_address, ba + card * CARD_OFFSET));
}

/** Write data memory */
void write_data(struct jr3_board *board, int ba, int data, int card)
{
	writel(data, board2virtual(board->jr3_base_address, ba + card * CARD_OFFSET));
}

void write_program(struct jr3_board *board, int pa, short data0, short data1,
	int card)
{
	writew(data0,
		board2virtual(board->jr3_base_address, pa + card * CARD_OFFSET));
	writew(data1,
		(board2virtual(board->jr3_base_address, pa + card * CARD_OFFSET)
			+ JR3_P8BIT_OFFSET));
}

/** Read program memory */
int read_program(struct jr3_board *board, int pa, int card)
{
	int r;
	r = readw(board2virtual(board->jr3_base_address, pa + card * CARD_OFFSET))
		<< 8;
	r = r
		| readb(
			board2virtual(board->jr3_base_address,
				pa + card * CARD_OFFSET) + JR3_P8BIT_OFFSET);
	return r;
}

int jr3_reset(struct jr3_board *board, int card)
{
	write_data(board, JR3_RESET_ADDRESS, 0, card);
	return 0;
}

int jr3_zero_offs(struct jr3_board *board, int card)
{
	write_data(board, JR3_COMMAND0, JR3_CMD_RESETOFFSETS, card);
	return 0;
}

int jr3_filter(struct jr3_board *board, unsigned long arg, int num_filter,
	int card)
{
	int i;
	int ret = 0;
	int axload[6];
	int address;

	for (i = 0; i < 6; i++) {
		address = JR3_FILTER0 + 0x08 * num_filter + i;
		axload[i] = (short) read_data(board, address, card);
	}

	ret = copy_to_user((void *) arg, (int *) axload, sizeof(jr3_six_axis_array));
	return ret;
}

/* Not tested */
int jr3_set_full_scales(struct jr3_board *board, unsigned long arg, int card)
{
	int fs[8];
	int ret = copy_from_user((int*) fs, (void *) arg, sizeof(jr3_force_array));
	int i;
	int address;

	for (i = 0; i < 8; i++) {
		address = JR3_FULLSCALE + i;
		write_data(board, address, fs[i], card);
	}
	write_data(board, JR3_COMMAND0, JR3_CMD_SETFULLSCALES, card);
	return ret;
}

static int jr3_open(struct inode *inode, struct file *filp)
{
	struct jr3_board *dev; /* device information */

	dev = container_of(inode->i_cdev, struct jr3_board, cdev);
	filp->private_data = dev; /* for other methods */

	return 0;          /* success */
}

static int jr3_release(struct inode *inode, struct file *filp)
{
	return 0;
}

int jr3_get_full_scales(struct jr3_board *board, unsigned long arg, int card)
{
	int i;
	int ret = 0;
	int fullscales[8];

	for (i = 0; i < 8; i++)
		fullscales[i] = read_data(board, JR3_FULLSCALE + i, card);
	ret = copy_to_user((void *) arg, (int *) fullscales, sizeof(jr3_force_array));

	return ret;
}

long jr3_ioctl(struct file *filp, unsigned int request, unsigned long arg)
{
	struct jr3_board *board = NULL;
	int err = 0;
	int ret = 0;
	int size = _IOC_SIZE(request); /* the size bitfield in cmd */
	int type = _IOC_TYPE(request);
	int nr = _IOC_NR(request);
	unsigned short pci_device = 0;

	board = (struct jr3_board *) filp->private_data;
	pci_device = board->pdev->device;

	if (type != JR3_IOC_MAGIC)
		return -ENOTTY;

	if (nr > IOCTL_JR3_MAXNR)
		return -ENOTTY;

#ifndef LINUX_20
	if (_IOC_DIR(request) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, arg, size);

	if (_IOC_DIR(request) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, arg, size);

	if (err)
		return -EFAULT;
#else
	if (_IOC_DIR(cmd) & _IOC_READ)
	err = verify_area(VERIFY_WRITE, arg, size);

	if (_IOC_DIR(cmd) & _IOC_WRITE)
	err = verify_area(VERIFY_READ, arg, size);

	if (err) return err;
#endif
	switch (request) {
	case IOCTL0_JR3_TYPE:
	case IOCTL1_JR3_TYPE:
		if (pci_device == PCI_DEVICE_ID_JR3_DOUBLE){
			ret = JR3_IOCTL_DOUBLE_BOARD;
		}else{
			ret = JR3_IOCTL_SINGLE_BOARD;
		}
		break;
	case IOCTL0_JR3_RESET:
		ret = jr3_reset(board, 0);
		break;
	case IOCTL0_JR3_ZEROOFFS:
		ret = jr3_zero_offs(board, 0);
		break;
	case IOCTL0_JR3_FILTER0:
		ret = jr3_filter(board, (unsigned long int)arg, 0,0);
		break;
	case IOCTL0_JR3_FILTER1:
		ret = jr3_filter(board, (unsigned long int)arg, 1,0);
		break;
	case IOCTL0_JR3_FILTER2:
		ret = jr3_filter(board, (unsigned long int)arg, 2,0);
		break;
	case IOCTL0_JR3_FILTER3:
		ret = jr3_filter(board, (unsigned long int)arg, 3,0);
		break;
	case IOCTL0_JR3_FILTER4:
		ret = jr3_filter(board, (unsigned long int)arg, 4,0);
		break;
	case IOCTL0_JR3_FILTER5:
		ret = jr3_filter(board, (unsigned long int)arg, 5,0);
		break;
	case IOCTL0_JR3_FILTER6:
		ret = jr3_filter(board, (unsigned long int)arg, 6,0);
		break;
	case IOCTL0_JR3_GET_FULL_SCALES:
		ret = jr3_get_full_scales(board, (unsigned long int)arg,0);
		break;
	case IOCTL1_JR3_RESET:
		if (pci_device == PCI_DEVICE_ID_JR3_DOUBLE)
			ret = jr3_reset(board, 1);
		else
			ret = -1;
		break;
	case IOCTL1_JR3_ZEROOFFS:
		if (pci_device == PCI_DEVICE_ID_JR3_DOUBLE)
			ret = jr3_zero_offs(board, 1);
		else
			ret = -1;
		break;
	case IOCTL1_JR3_FILTER0:
		if (pci_device==PCI_DEVICE_ID_JR3_DOUBLE)
		ret = jr3_filter(board, (unsigned long int)arg, 0,1);
		else ret=-1;
		break;
	case IOCTL1_JR3_FILTER1:
		if (pci_device==PCI_DEVICE_ID_JR3_DOUBLE)
		ret = jr3_filter(board, (unsigned long int)arg, 1,1);
		else ret=-1;
		break;
	case IOCTL1_JR3_FILTER2:
		if (pci_device==PCI_DEVICE_ID_JR3_DOUBLE)
		ret = jr3_filter(board, (unsigned long int)arg, 2,1);
		else ret=-1;
		break;
	case IOCTL1_JR3_FILTER3:
		if (pci_device==PCI_DEVICE_ID_JR3_DOUBLE)
		ret = jr3_filter(board, (unsigned long int)arg, 3,1);
		else ret=-1;
		break;
	case IOCTL1_JR3_FILTER4:
		if (pci_device==PCI_DEVICE_ID_JR3_DOUBLE)
		ret = jr3_filter(board, (unsigned long int)arg, 4,1);
		else ret=-1;
		break;
	case IOCTL1_JR3_FILTER5:
		if (pci_device==PCI_DEVICE_ID_JR3_DOUBLE)
		ret = jr3_filter(board, (unsigned long int)arg, 5,1);
		else ret=-1;
		break;
	case IOCTL1_JR3_FILTER6:
		if (pci_device==PCI_DEVICE_ID_JR3_DOUBLE)
		ret = jr3_filter(board, (unsigned long int)arg, 6,1);
		else ret=-1;
		break;
	case IOCTL1_JR3_GET_FULL_SCALES:
		if (pci_device==PCI_DEVICE_ID_JR3_DOUBLE)
		ret = jr3_get_full_scales(board, (unsigned long int)arg,1);
		else ret=-1;
		break;
	default:
		return -ENOTTY;
	}
	return ret;
}

static int show_copyright(struct jr3_board *board, short card)
{
	int i;
	char copyright[19];
	char units_str[16];
	int base = 0x6040;
	short day, year, units;

	for (i = 0; i < 18; i++) {
		copyright[i] = (char) (read_data(board, base, card) >> 8);
		base++;
	}

	copyright[18] = '\0';
	day = read_data(board, 0x60f6, card);
	year = read_data(board, 0x60f7, card);
	units = read_data(board, 0x60fc, card);
	if (units == 0) {
		strncpy(units_str, "lbs\0", 4);
	} else if (units == 1) {
		strncpy(units_str, "Newtons\0", 8);
	} else if (units == 2) {
		strncpy(units_str, "Kilograms-force\0", 16);
	} else if (units == 3) {
		strncpy(units_str, "1000 lbs\0", 8);
	} else {
		sprintf(units_str, "(id %d)", units);
	}
	printk("jr3pci-driver(%d): %s\n", card, copyright);
	printk("jr3pci-driver(%d): DSP Software updated day %d, year %d. Units: %s\n",
		card, day, year, units_str);
	return 0;
}

/** Download code to DSP */
static int jr3pci_init_dsp(struct jr3_board *board, int card)
{
	int i = 0;
	int count, address, value, value2;

	count = jr3_firmware[i++];
	while (count != 0xffff) {
		address = jr3_firmware[i++];

		msleep_interruptible(5);
		DBG_PRINT("jr3pci-driver(%d): DSP Code. File pos. %d, Address 0x%x, %d times\n",
			card, i, address, count);



		while (count > 0) {
			if (address & JR3_PD_DEVIDER) {
				value = jr3_firmware[i++];
				write_data(board, address, value, card);
				if (value != read_data(board, address, card)) {
					printk("data write error at address 0x%x\n", address);
				}
				count--;
			} else {
				value = jr3_firmware[i++];
				value2 = jr3_firmware[i++];
				write_program(board, address, value, value2, card);
				if (((value << 8) | (value2 & 0x00ff))
					!= read_program(board, address, card)) {
					printk("program write error at address 0x%x\n", address);
				}
				count -= 2;
			}
			address++;
		}
		count = jr3_firmware[i++];
	}
	return 0;
}

/** pci probe function */
static int jr3_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct jr3_board *board;
	int err = 0;

	DBG_PRINT("jr3pci-driver: %s\n", __PRETTY_FUNCTION__);

	// init and register jr3_board structure 
	board = kzalloc(sizeof(struct jr3_board), GFP_KERNEL);
	if (board == NULL) {
		printk("jr3pci-driver: %s: kzalloc failed\n", __PRETTY_FUNCTION__);
		err = -ENOMEM;
		goto leave;
	}
	//board = &jr3_devices[pci_registered];

	pci_set_drvdata(pdev, (void *) board);

	// init pci device 
	err = pci_enable_device(pdev);
	if (err) {
		printk("jr3pci-driver: %s: pci_enable_device failed\n", __PRETTY_FUNCTION__);
		goto leave;
	}
	board->init_flags |= INIT_PCI_ENABLE;

	// enable bus-mastering on the device
	// pci_set_master(pdev); 

	// init memory mapping 
	board->memregion = pci_resource_start(pdev, 0);
	board->size = pci_resource_len(pdev, 0);

	/*
	if (check_mem_region(board->memregion, board->size)) {
		printk("jr3pci-driver: memory already in use\n");
		return -EBUSY;
	}

*/

	if (request_mem_region(board->memregion, board->size, "JR3pci")) {
		board->jr3_base_address = ioremap(board->memregion, board->size);
		DBG_PRINT("jr3pci-driver: memory mapped successfully\n");
		err = 0;
	}
	else{
		printk("jr3pci-driver: memory not mapped\n");
		return -EIO;
	}
	board->init_flags |= INIT_PCI_REQUEST_REGIONS;

	DBG_PRINT( "jr3pci-driver: JR3 PCI card %s (%x:%x) at 0x%lx\n", 
		pci_name(pdev), pdev->vendor, pdev->device, 
		(long int)board->jr3_base_address);

	board->init_flags |= INIT_PCI_IOMAP;

	board->pdev = pdev;

	jr3_init(board);
	DBG_PRINT("jr3pci-driver: DSP code downloaded!! You can start playing  :)\n");

	list_add(&board->list, &jr3_list);

	leave:

	if (err)
		jr3_pci_remove(pdev);

	board->number = pci_registered;
	++pci_registered;

	return err;
}

//pci remove function
static void jr3_pci_remove(struct pci_dev *pdev)
{
	struct jr3_board *board = NULL;

	DBG_PRINT("jr3pci-driver: removing\n");

	board = (struct jr3_board *) pci_get_drvdata(pdev);

	if (board->init_flags & INIT_PCI_IOMAP)
		iounmap(board->jr3_base_address);

	if (board->init_flags & INIT_PCI_REQUEST_REGIONS)
		release_mem_region(board->memregion, board->size);

	if (board->init_flags & INIT_PCI_ENABLE)
		pci_disable_device(pdev);

	DBG_PRINT("jr3pci-driver: removed\n");

	--pci_registered;
}

void jr3_init(struct jr3_board *board)
{
	unsigned short pci_device = 0;

	pci_device = board->pdev->device;

	DBG_PRINT("jr3pci-driver: reset DSP\n");
	//Reset DSP
	write_data(board, JR3_RESET_ADDRESS, 0, 0);
	if (pci_device == PCI_DEVICE_ID_JR3_DOUBLE)
		write_data(board, JR3_RESET_ADDRESS, 0, 1);
	DBG_PRINT("jr3pci-driver: download DSP code\n");
	//Download DSP code
	jr3pci_init_dsp(board, 0);
	if (pci_device == PCI_DEVICE_ID_JR3_DOUBLE)
		jr3pci_init_dsp(board, 1);

	show_copyright(board, 0);
	if (pci_device == PCI_DEVICE_ID_JR3_DOUBLE)
		show_copyright(board, 1);

}


struct file_operations jr3_fops = {
	.owner =    THIS_MODULE,
	.unlocked_ioctl =    jr3_ioctl,
	.open =     jr3_open,
	.release =  jr3_release,
};

static struct pci_device_id pci_ids[] = { 
{ PCI_DEVICE(PCI_VENDOR_ID_JR3, PCI_DEVICE_ID_JR3_SINGLE) },
{ PCI_DEVICE(PCI_VENDOR_ID_JR3, PCI_DEVICE_ID_JR3_SINGLE_2) },
{ PCI_DEVICE(PCI_VENDOR_ID_JR3, PCI_DEVICE_ID_JR3_DOUBLE) },
{ 0, } };

MODULE_DEVICE_TABLE( pci, pci_ids);

static struct pci_driver jr3_pci_driver = {
	.name = (char *) driver_name, 
	
	.id_table = pci_ids, 
	
	.probe = jr3_pci_probe,

	.remove = jr3_pci_remove, };

static void jr3_pci_init(void)
{
	DBG_PRINT("jr3pci-driver: %s\n", __PRETTY_FUNCTION__);

	pci_register_driver(&jr3_pci_driver);

	printk("jr3pci-driver, registered, current number of devices: %d\n", pci_registered);
}

static void jr3_pci_cleanup(void)
{

	DBG_PRINT("jr3pci-driver: %s\n", __PRETTY_FUNCTION__);
	if (pci_registered > 0)
		pci_unregister_driver(&jr3_pci_driver);


	printk("jr3pci-driver, current number of devices: %d\n", pci_registered);
}

static void jr3_setup_cdev(struct jr3_board *dev, int index)
{
	int err, devno = MKDEV(jr3_major, index);
    
	cdev_init(&dev->cdev, &jr3_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &jr3_fops;
	err = cdev_add (&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err){
		printk(KERN_NOTICE "Error %d adding jr3%d", err, index);
	}
}

static int jr3pci_init_module(void)
{
	dev_t dev = 0;
	int err = 0;
	int index = 0;

	struct list_head *ptr;
	struct jr3_board *jr3ptr = NULL;

	printk( "jr3pci-driver: %s by %s\n", JR3_DESC, JR3_AUTHOR);

	jr3_pci_init();

	/* obtain device numbers */
	if (jr3_major) {
		dev = MKDEV(jr3_major, 0);
		err = register_chrdev_region(dev, jr3_nr_devs,
						driver_name);
	} else {
		err = alloc_chrdev_region(&dev, 0, jr3_nr_devs,
					     driver_name);
		jr3_major = MAJOR(dev);
	}
	if (err) {
		printk("jr3pci-driver: alloc_chrdev_region failed\n");
		goto cleanup_out;
	}

	list_for_each(ptr, &jr3_list)
	{
		jr3ptr = list_entry(ptr, struct jr3_board, list);

		printk("jr3pci-driver: New device: jr3%d at %s\n", index, 
		pci_name(jr3ptr->pdev));

		jr3_setup_cdev(jr3ptr, index);

		++index;
	}

	cleanup_out:

	return err;
}

static void jr3pci_exit_module(void)
{

	struct list_head *ptr, *q;
	struct jr3_board *jr3ptr = NULL;

	DBG_PRINT("jr3pci-driver: delete char devices\n");
	list_for_each(ptr, &jr3_list)
	{
		jr3ptr = list_entry(ptr, struct jr3_board, list);
		cdev_del(&jr3ptr->cdev);
	}

	DBG_PRINT("jr3pci-driver: pci cleanup\n");
	jr3_pci_cleanup();

	DBG_PRINT("jr3pci-driver: unregister char device region\n");
	unregister_chrdev_region(MKDEV(jr3_major, 0), jr3_nr_devs);

	DBG_PRINT("jr3pci-driver: memory deallocation\n");
	list_for_each_safe(ptr, q, &jr3_list)
	{
		DBG_PRINT("jr3pci-driver: freeing ...\n");
		jr3ptr = list_entry(ptr, struct jr3_board, list);

		DBG_PRINT("jr3pci-driver: list_del ...\n");
		list_del(ptr);
		DBG_PRINT("jr3pci-driver: kfree ...\n");
		kfree(jr3ptr);

	}

	printk("jr3pci-driver: driver removed\n");
}

module_init( jr3pci_init_module);
module_exit( jr3pci_exit_module);

