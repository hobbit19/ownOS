#include "includes.h"

struct timer* timers = NULL;

void (*timer_cb)(void *arguments);

void pit_init(void) {
	int freq=1000;
	int counter = 1193182 / freq;
	outb(0x43, 0x34);
	outb(0x40,counter & 0xFF);
	outb(0x40,counter >> 8);
	timers = NULL;
}

void show_timers(void) {
	struct timer* timer_temp = timers;
	kprintf("0x%x -> ",timer_temp);
	while(timer_temp != NULL) {
		kprintf("0x%x -> ",timer_temp->next);
		timer_temp = timer_temp->next;
		sleep(100);
	}
	kprintf("\n");
}

struct timer* register_timer(void* callback, uint32_t timeout, bool remove_after_event, void *arguments) {
	struct timer* timer_temp = pmm_alloc();
	struct timer* timer_temp1 = timers;
	while(timer_temp1 != NULL) {
		if(timer_temp1 == timer_temp) {
			timer_temp = pmm_alloc();
			break;
		}
		timer_temp1 = timer_temp1->next;
	}
	timer_temp->callback = callback;
	timer_temp->ticks = 0;
	timer_temp->timeout = timeout;
	timer_temp->arguments = arguments;
	timer_temp->remove_after_event = remove_after_event;
	timer_temp->next = timers;
	if(timer_temp->callback == NULL) {
		kprintf("Error setting callback\n");
	}
	timers = timer_temp;
	kprintf("timer.c: 37 0x%x\n",timer_temp);
	show_timers();
	return timer_temp;
}

bool unregister_timer(struct timer* timer) {
	kprintf("timer.c: 43 unregister at 0x%x   0x%x\n",timer,timers);
	struct timer* timer_temp = timers;
	if(timer_temp == NULL) {
		kprintf("No timers available\n");
		return false;
	}
	if(timers == timer) {
		timers = timers->next;
		pmm_free(timer->arguments);
		pmm_free(timer);
		kprintf("timer.c: 64 Unregistering Timer done\n");
		return true;
	}
	while(timer_temp->next != NULL) {
		if(timer_temp->next == timer) {
			timer_temp->next = timer_temp->next->next;
			pmm_free(timer->arguments);
			pmm_free(timer);
			kprintf("timer.c: 73 Unregistering Timer done\n");
			return true;
		}
		timer_temp = timer_temp->next;
	}
	kprintf("Timer not found\n");
	return false;
}

bool unregister_timer_by_arguments(void* arguments) {
	struct timer* timer_temp = timers;
	struct timer* timer_del = NULL;
	if(timer_temp == NULL) return false;
	if(timer_temp->arguments == arguments) {
		timer_del = timer_temp;
		timers = timer_temp->next;
		pmm_free(timer_del->arguments);
		pmm_free(timer_del);
		kprintf("timer.c: 86 Unregistering Timer by arguments done\n");
		return true;
	}
	while(timer_temp->next != NULL) {
		kprintf("Args: 0x%x\n",timer_temp->next->arguments);
		if(timer_temp->next->arguments == arguments) {
			timer_del = timer_temp->next;
			timer_temp->next = timer_temp->next->next;
			pmm_free(timer_del->arguments);
			pmm_free(timer_del);
			kprintf("timer.c: 95 Unregistering Timer by arguments done\n");
			return true;
		}
		timer_temp = timer_temp->next;
	}
	return false;
}

void handle_timer(void) {
	struct timer* timer_temp = timers;
	while(timer_temp != NULL) {
		timer_temp->ticks++;
		if(timer_temp->ticks >= timer_temp->timeout) {
			timer_cb = timer_temp->callback;
			last_message = "timer callback";
			timer_cb(timer_temp->arguments);
			last_message = "timer callback done";
			if(timer_temp->remove_after_event) {
				last_message = "unregister timer";
				unregister_timer(timer_temp);
			} else {
				last_message = "reset timer";
				timer_temp->ticks = 0;
			}
		}
		last_message = "next timer";
		if(timer_temp->next == NULL) break;
		timer_temp = timer_temp->next;
	}
	last_message = "end timer_handle";
}