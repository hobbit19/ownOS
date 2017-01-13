#include "includes.h"

isr_handler_t interrupt_handlers[256];

void register_interrupt_handler(uint8_t interrupt, isr_handler_t handler)
{
    interrupt_handlers[interrupt] = handler;
}

void isr_handler(registers_t regs)
{
	terminal_writestring("ISR");
    char *result;
	itoa(regs.int_no,*result,10);
	terminal_writestring(*result);
	if(regs.int_no == GENERAL_PROTECTION_FAULT)
    {
        //printf("General Protection Fault. Code: %d", regs.err_code);
        terminal_writestring("General Protection Fault!");
		char *result;
		itoa(regs.err_code,*result,10);
		terminal_writestring(*result);
    }

    if(interrupt_handlers[regs.int_no])
    {
        terminal_writestring("Handling!");
        interrupt_handlers[regs.int_no](regs);
    }
}

void irq_handler(registers_t regs)
{
	if(regs.int_no != IRQ0 && regs.int_no != IRQ1) {
		terminal_writestring("IRQ");
		char *result;
		itoa(regs.int_no,*result,10);
		terminal_writestring(*result);
	}
	if(regs.int_no == IRQ0) {
		terminal_writestring("Handling IRQ0");
		struct cpu_state* cpu;
		cpu->eax = regs.eax;
		cpu->ebx = regs.ebx;
		cpu->ecx = regs.ecx;
		cpu->edx = regs.edx;
		cpu->esi = regs.esi;
		cpu->edi = regs.edi;
		cpu->ebp = regs.ebp;
		cpu->intr = regs.int_no;
		cpu->error = regs.err_code;
		cpu->eip = regs.eip;
		cpu->cs = regs.cs;
		cpu->eflags = regs.eflags;
		cpu->esp = regs.useresp;
		cpu->ss = regs.ss;
		
		terminal_writestring("IRQ0 handled");
		handle_multitasking(cpu);
	}
	if(regs.int_no == IRQ1) {
		kbd_irq_handler();
	}
	//If int_no >= 40, we must reset the slave as well as the master
	if(regs.int_no >= 40)
	{
		//reset slave
		outb(SLAVE_COMMAND, PIC_RESET);
	}

	outb(MASTER_COMMAND, PIC_RESET);

	if(interrupt_handlers[regs.int_no])
	{
		interrupt_handlers[regs.int_no](regs);
	}
}