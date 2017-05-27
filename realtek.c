#include "includes.h"

pci_bdf_t addr;
uint8_t irq = 0;

struct rx_desc {
	int own : 1; // if 1 owned by nic / else owned by host
	int eor : 1; // if set end of ring
	uint16_t reserved : 16;
	uint16_t buffer_size : 14;
	uint32_t vlan;
	uint32_t addr_low;
	uint32_t addr_high;
};

struct rx_desc_status {
	int own : 1; // if 1 owned by nic / else owned by host
	int eor : 1; // if set end of ring
	int fs : 1; // if set this is the first segment of packet
	int ls : 1; // if set this is the last segment of packet
	int mar : 1; // multicast packet received
	int pam : 1; // physical packet received
	int bar : 1; // broadcast packet received
	int reserved : 2; // always 0x01
	int rwt : 1; // packet bigger than 8192 bytes
	int res : 1; // if set and ls=1 -> error (crc,runt,rwt,fae)
	int runt : 1; // packet smaller than 64 bytes
	int crc : 1; // if set -> crc-error
	int pid : 2; // protocol-ID:
				/*
				00 = IP
				01 = TCP/IP
				10 = UDP/IP
				11 = IP
				*/
	int ipf : 1; // if set -> ip checksum failure
	int udpf : 1; // if set -> udp checksum failure
	int tcpf : 1; // if set -> tcp checksum failure
	uint16_t frame_length : 14; // if own = 0 and ls = 1 -> packet length incl. crc in bytes
	uint32_t vlan;
	uint32_t addr_low;
	uint32_t addr_high;
};

void realtek_init(pci_bdf_t device) {
	addr = device;
	kprintf("Realtek...\n");
	irq = pci_config_read_8(addr,0x3C);
	kprintf("Registerig IRQ %d\n",irq);
	kprintf("MAC: %x-",pci_read_register_8(addr,0,0x00));
	kprintf("%x-",pci_read_register_8(addr,0,0x01));
	kprintf("%x-",pci_read_register_8(addr,0,0x02));
	kprintf("%x-",pci_read_register_8(addr,0,0x03));
	kprintf("%x-",pci_read_register_8(addr,0,0x04));
	kprintf("%x\n",pci_read_register_8(addr,0,0x05));
	pci_write_register_16(addr,0,0x3E,pci_read_register_16(addr,0,0x3E)); //Status zurücksetzen
	pci_write_register_16(addr,0,0x3C,0x43FF); //Activating all Interrupts
	//pci_write_register_16(addr,0,0x3C,0x20); //Nur Link-Change überwachen
}

void realtek_handle_intr(void) {
	uint16_t status = pci_read_register_16(addr,0,0x3E);
	//kprintf("Status: %b\n",status);
	if(status & 0x0001) kprintf("Receive succesfull\n");
	if(status & 0x0002) kprintf("Receive error\n");
	if(status & 0x0004) kprintf("Transmit succesfull\n");
	if(status & 0x0008) kprintf("Transmit error\n");
	if(status & 0x0010) kprintf("Receive descriptor unavailable\n");
	if(status & 0x0020) {
		kprintf("Link changed\n");
		if(pci_read_register_8(addr,0,0x06) & 0x02) {
			kprintf("Link is up\n");
		} else {
			kprintf("Link is down\n");
		}
	}
	if(status & 0x0040) kprintf("Receive FIFO overflow\n");
	if(status & 0x0080) kprintf("Transmit descriptor unavailable\n");
	if(status & 0x0100) kprintf("Software Interrupt\n");
	if(status & 0x0200) kprintf("Receive FIFO empty\n");
	if(status & 0x0400) kprintf("Unknown Status (reserved Bit 11)\n");
	if(status & 0x0800) kprintf("Unknown Status (reserved Bit 12)\n");
	if(status & 0x1000) kprintf("Unknown Status (reserved Bit 13)\n");
	if(status & 0x2000) kprintf("Unknown Status (reserved Bit 14)\n");
	if(status & 0x4000) kprintf("Timeout\n");
	if(status & 0x8000) kprintf("Unknown Status (reserved Bit 16)\n");
	pci_write_register_16(addr,0,0x3E,pci_read_register_16(addr,0,0x3E));
}