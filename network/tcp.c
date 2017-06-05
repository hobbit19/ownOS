#include "includes.h"

/*
TODO:
	-> Timeout-Timer
*/

void sendTCPpacket(struct ether_header* ether, struct ip_header* ip, struct tcp_header* tcp, uint32_t options[], int options_count, uint8_t *data, int data_length);
bool register_tcp_listener(int port, void *callback_pointer);
void sendData(struct tcp_callback cb);
void closeCon(struct tcp_callback cb);
bool tcp_open_con(int port, void *callback_pointer);

uint32_t last_seq = 0;
uint32_t last_ack = 0;
bool con_est = false;

struct clients {
	uint32_t client_id;
	bool con_est;
	uint32_t last_ack;
	uint32_t last_seq;
	uint32_t fin_seq;
	uint32_t fin_ack;
	struct clients *next;
};

struct listeners {
	struct tcp_callback tcp_listener;
	struct clients **clients;
};

struct listeners listeners[65536];

struct server_con {
	int remote_port;
	bool con_est;
	bool in_use;
	uint32_t last_ack;
	uint32_t last_seq;
	uint32_t fin_seq;
	uint32_t fin_ack;
};

struct server_con connections[65536];

//bool listener_enabled[65536];
//struct tcp_callback tcp_listeners[65536][51];

void (*callback_func)(struct tcp_callback);

struct clients *find_client(uint32_t client_id, uint16_t port) {
	struct clients *client = listeners[port].clients;
	if(client == NULL) return NULL;
	while(client != NULL) {
		if(client->client_id == client_id) return client;
		client = client->next;
	}
	return NULL;
}

void show_clients(uint16_t port) {
	struct clients *client1 = listeners[port].clients;
	while(client1 != NULL) {
		kprintf("0x%x -> ",client1->client_id);
		client1 = client1->next;
	}
	kprintf("\n");
}

struct clients *add_client(uint32_t client_id, uint16_t port) {
	struct clients *client;
	client = pmm_alloc();
	client->client_id = client_id;
	client->con_est = false;
	client->last_ack = 0;
	client->last_seq = 0;
	client->fin_ack = 0;
	client->fin_seq = 0;
	client->next = NULL;
	struct clients *client1 = listeners[port].clients;
	if(client1 == NULL) {
		listeners[port].clients = client;
		return client;
	}
	while(client1->next != NULL) {
		client1 = client1->next;
	}
	client1->next = client;
	//show_clients(port);
	return client;
}

bool del_client(uint32_t client_id, uint16_t port) {
	struct clients *client = listeners[port].clients;
	if(client->client_id == client_id) {
		//kprintf("11\n");
		struct clients *client_temp = client;
		//kprintf("12\n");
		listeners[port].clients = client->next;
		//kprintf("Deleted1: 0x%x\n",client_temp->client_id);
		pmm_free(client_temp);
		//show_clients(port);
		return true;		
	}
	while(client->next != NULL) {
		if(client->next->client_id == client_id) {
			//kprintf("21\n");
			struct clients *client_temp = client->next;
			//kprintf("22\n");
			client->next = client->next->next;
			kprintf("Deleted: 0x%x\n",client_temp->client_id);
			pmm_free(client_temp);
			//show_clients(port);
			return true;			
		}
	}
	return false;
}

bool check_tcp_flags(struct tcp_flags flags, unsigned f) {//, unsigned syn, unsigned rst, unsigned psh, unsigned ack, unsigned urg, unsigned ece, unsigned cwr) {
	last_message = "init check";
	uint8_t flags1 __attribute__((packed));
	last_message = "c1";
	memcpy(&flags1,&flags,8);
	last_message = "c2";
	if(flags1 == f) {
		last_message = "c3";
		return true;
	}
	last_message = "c4";
	return false;
	last_message = "c5";
}

void tcp_handle(struct ip_header* ip, struct ether_header* ether) {
	struct tcp_header* tcp = pmm_alloc();
	for(int i=0;i<20/*ip->data_length*/;i++) {
		((uint8_t*)tcp)[i] = ip->data[i];
	}
	for(int i=20;i<(tcp->headerlen * 4);i++) {
		tcp->options[i - 20] = ip->data[i];
	}
	uint8_t *tcp_data = pmm_alloc();
	for(int i=(tcp->headerlen * 4);i< (tcp->headerlen * 4) + (ip->packetsize - (ip->headerlen * 4) - (tcp->headerlen * 4));i++) {
		tcp_data[i - (tcp->headerlen * 4)] = ip->data[i];
	}
	
	uint16_t temp_port = tcp->destination_port;
	tcp->destination_port = tcp->source_port;
	tcp->source_port = temp_port;

	ip->destinationIP = ip->sourceIP;
	ip->sourceIP = my_ip;
	ip->fragment = HTONS(ip->fragment);
	ip->id = HTONS(ip->id);
	
	last_message = "socketID tcp_handle";
	uint32_t socketID = (ip->sourceIP.ip1) +
						(ip->sourceIP.ip2) +
						(ip->sourceIP.ip3) +
						(ip->sourceIP.ip4) +
						(HTONS(tcp->destination_port)) +
						checksum(ip->sourceIP,4) +
						checksum(tcp->destination_port,2);
	//kprintf("Socket-ID: 0x%x\n",socketID);
			if(check_tcp_flags(tcp->flags, syn)) { //asking for connection
				kprintf("abc");
			} else {
				kprintf("def");
			}
			sleep(5000);
	if(listeners[HTONS(temp_port)].tcp_listener.enabled) {
		listeners[HTONS(temp_port)].tcp_listener.data = tcp_data;
		listeners[HTONS(temp_port)].tcp_listener.data_length = ip->packetsize - (ip->headerlen * 4) - (tcp->headerlen * 4);
		listeners[HTONS(temp_port)].tcp_listener.tcp = tcp;
		listeners[HTONS(temp_port)].tcp_listener.ip = ip;
		listeners[HTONS(temp_port)].tcp_listener.ether = ether;
		
		struct clients *client = find_client(socketID,HTONS(temp_port));
		if(client == NULL) { //no socketID
			//kprintf("No client found\n");
			if(check_tcp_flags(tcp->flags, syn)) { //asking for connection
				tcp->flags.syn = 1;
				tcp->flags.ack = 1;
				tcp->ack_number = HTONL(HTONL(tcp->sequence_number) + 1);
				tcp->sequence_number = HTONL(tcp->sequence_number);
				client = add_client(socketID,HTONS(temp_port));
				//kprintf("New connection: 0x%x\n",client->client_id);
				client->last_seq = HTONL(tcp->sequence_number);
				client->last_ack = HTONL(tcp->ack_number);
				sendTCPpacket(ether, ip, tcp, tcp->options, 0, tcp->data, 0);
			}
		} else {
			//kprintf("Client found: 0x%x\n",client->client_id);
			if(client->con_est) {
				if(check_tcp_flags(tcp->flags, fin | psh) &&
							tcp->ack_number != HTONL(client->fin_seq + 1) &&
							tcp->sequence_number != HTONL(client->fin_ack)) { //got FIN-Packet
					tcp->flags.fin = 1;
					tcp->flags.ack = 1;
					uint32_t ack_temp = tcp->ack_number;
					tcp->ack_number = HTONL(HTONL(tcp->sequence_number) + 1);
					tcp->sequence_number = ack_temp;
					client->fin_seq = HTONL(tcp->sequence_number);
					client->fin_ack = HTONL(tcp->ack_number);
					sendTCPpacket(ether, ip, tcp, tcp->options, 0, tcp->data, 0);
				} else if((check_tcp_flags(tcp->flags, ack) &&
							tcp->ack_number == HTONL(client->fin_seq + 1) &&
							tcp->sequence_number == HTONL(client->fin_ack)) || check_tcp_flags(tcp->flags, rst)) { // got FIN-ACK
					client->fin_seq = 0;
					client->fin_ack = 0;
					uint32_t ack_temp = tcp->ack_number;
					tcp->ack_number = HTONL(HTONL(tcp->sequence_number) + 1);
					tcp->sequence_number = ack_temp;
					client->con_est = false;
					if(del_client(client->client_id,HTONS(temp_port))) {
						//kprintf("closed\n");
					} else {
						//kprintf("error\n");
					}
					sendTCPpacket(ether, ip, tcp, tcp->options, 0, tcp->data, 0);
				} else if(check_tcp_flags(tcp->flags, ack | psh)) { //got packet
					//ACK received packet
					tcp->flags.psh = 0;
					uint32_t ack_temp = tcp->ack_number;
					tcp->ack_number = HTONL(HTONL(tcp->sequence_number) + listeners[HTONS(temp_port)].tcp_listener.data_length);
					tcp->sequence_number = ack_temp;
					client->last_seq = HTONL(tcp->sequence_number);
					client->last_ack = HTONL(tcp->ack_number);
					sendTCPpacket(ether, ip, tcp, tcp->options, 0, listeners[HTONS(temp_port)].tcp_listener.data, 0);
					
					callback_func = listeners[HTONS(temp_port)].tcp_listener.callback_pointer;
					callback_func(listeners[HTONS(temp_port)].tcp_listener);
				}
			} else {
				if(check_tcp_flags(tcp->flags, ack) && !client->con_est) { //ack connection
					if(client->last_ack == HTONL(tcp->sequence_number) && HTONL(tcp->ack_number) == client->last_seq + 1) {
						client->con_est = true;
						tcp_data[0] = 0xff;
						tcp_data[1] = 0xff;
						tcp_data[2] = 0xff;
						listeners[HTONS(temp_port)].tcp_listener.data = tcp_data;
						listeners[HTONS(temp_port)].tcp_listener.data_length = 3;
						callback_func = listeners[HTONS(temp_port)].tcp_listener.callback_pointer;
						callback_func(listeners[HTONS(temp_port)].tcp_listener);
					}
				}
			}
		}
	}
	pmm_free(tcp_data);
	pmm_free(tcp);
}

bool register_tcp_listener(int port, void *callback_pointer) {
	if(listeners[port].tcp_listener.enabled == true) {
		return false;
	} else {
		struct tcp_callback cb = {
			.callback_pointer = callback_pointer,
			.port = port,
			.con_est = false,
			.enabled = true,
			.data = pmm_alloc()
		};
		if(cb.callback_pointer == NULL) {
			kprintf("Callback not set\n");
			return false;
		}
		listeners[port].tcp_listener = cb;
		listeners[port].tcp_listener.enabled = true;
		return true;
	}
}

bool show_close = false;

void closeCon(struct tcp_callback cb) {
	last_message = "socketID closeCon";
	uint32_t socketID = (cb.ip->sourceIP.ip1) +
						(cb.ip->sourceIP.ip2) +
						(cb.ip->sourceIP.ip3) +
						(cb.ip->sourceIP.ip4) +
						(HTONS(cb.tcp->destination_port)) +
						checksum(cb.ip->sourceIP,4) +
						checksum(cb.tcp->destination_port,2);
	kprintf("trying to close 0x%x\n",socketID);
	if(find_client(socketID,cb.port) != NULL) {
		kprintf("socketID found\n");
		struct clients *client = find_client(socketID,cb.port);
		if(listeners[cb.port].tcp_listener.enabled && client->con_est) {
			kprintf("connection alive\n");
			sleep(1000);
			uint32_t ack_temp = cb.tcp->sequence_number;
			uint32_t seq_temp = HTONL(HTONL(cb.tcp->ack_number) + cb.data_length);
			cb.tcp->sequence_number = seq_temp;
			cb.tcp->ack_number = ack_temp;
			cb.tcp->flags.ack = 1;
			cb.tcp->flags.psh = 0;
			cb.tcp->flags.rst = 0;
			cb.tcp->flags.syn = 0;
			cb.tcp->flags.fin = 1;
			cb.tcp->flags.urg = 0;
			cb.tcp->flags.ece = 0;
			cb.tcp->flags.cwr = 0;
			cb.fin_ack = HTONL(ack_temp) + 2;
			cb.fin_seq = HTONL(seq_temp);
			client->fin_ack = cb.fin_ack;
			client->fin_seq = cb.fin_seq;
			show_close = true;
			kprintf("sending FIN-Packet\n");
			sendTCPpacket(cb.ether, cb.ip, cb.tcp, cb.tcp->options, 0, &cb.data, 0);
		}
	}
}

void sendData(struct tcp_callback cb) {
	last_message = "socketID sendData";
	uint32_t socketID = (cb.ip->sourceIP.ip1) +
						(cb.ip->sourceIP.ip2) +
						(cb.ip->sourceIP.ip3) +
						(cb.ip->sourceIP.ip4) +
						(HTONS(cb.tcp->destination_port)) +
						checksum(cb.ip->sourceIP,4) +
						checksum(cb.tcp->destination_port,2);
	if(find_client(socketID,cb.port) != NULL) {
		struct clients *client = find_client(socketID,cb.port);
		if(listeners[cb.port].tcp_listener.enabled && client->con_est) {
			uint32_t ack_temp = cb.tcp->sequence_number;
			uint32_t seq_temp = HTONL(HTONL(cb.tcp->ack_number));
			cb.tcp->sequence_number = seq_temp;
			cb.tcp->ack_number = ack_temp;
			cb.tcp->flags.ack = 1;
			cb.tcp->flags.psh = 1;
			cb.tcp->flags.rst = 0;
			cb.tcp->flags.syn = 0;
			cb.tcp->flags.fin = 0;
			cb.tcp->flags.urg = 0;
			cb.tcp->flags.ece = 0;
			cb.tcp->flags.cwr = 0;
			sendTCPpacket(cb.ether, cb.ip, cb.tcp, cb.tcp->options, 0, &cb.data, cb.data_length);
		}
	}
}

void sendTCPpacket(struct ether_header* ether, struct ip_header* ip, struct tcp_header* tcp, uint32_t options[], int options_count, uint8_t *data, int data_length) {
	uint16_t packetsize = 20 + 20 + options_count + data_length;
	int pos = 0;
	int pos1 = 0;
	uint8_t *temp;
	
	ip->checksum = 0;
	ip->packetsize = HTONS(packetsize);
	ip->headerlen = 5;
	ip->version = 4;
	tcp->checksum = 0;
	tcp->headerlen = (packetsize - data_length - 20) / 4;
	
	last_message = "sendTCPpacket ip";
	ip->checksum = HTONS(checksum(ip,20));
	
	struct tcp_pseudo_header head = {
		.sourceIP = ip->sourceIP,
		.destinationIP = ip->destinationIP,
		.protocol = 6,
		.tcp_length = HTONS((uint16_t)packetsize - 20)
	};
	
	uint8_t tcpChecksum[12 + packetsize - 20];
	temp = &head;
	for(int i = 0; i < 12; i++) {
		tcpChecksum[pos1] = temp[i];
		pos1++;
	}
	temp = tcp;
	for(int i = 0; i < 20; i++) { //tcp_header
		tcpChecksum[pos1] = temp[i];
		pos1++;
	}
	temp = &options;
	for(int i = 0; i < options_count; i++) { //tcp_options
		tcpChecksum[pos1] = temp[i];
		pos1++;
	}
	//temp = data;
	for(int i = 0; i < data_length; i++) { //tcp_data
		tcpChecksum[pos1] = data[i];
		pos1++;
	}
	//pos1--;
	last_message = "sendTCPpacket tcp";
	tcp->checksum = HTONS(checksum(&tcpChecksum, pos1));
	
	//uint8_t buffer[packetsize];
	uint8_t *buffer = pmm_alloc();
	temp = ip;
	for(int i = 0; i < 20; i++) { //ip_header
		buffer[pos] = temp[i];
		pos++;
	}
	temp = tcp;
	for(int i = 0; i < 20; i++) { //tcp_header
		buffer[pos] = temp[i];
		pos++;
	}
	temp = &options;
	for(int i = 0; i < options_count; i++) { //tcp_options
		buffer[pos] = temp[i];
		pos++;
	}
	//temp = data;
	for(int i = 0; i < data_length; i++) { //tcp_data
		buffer[pos] = data[i];
		pos++;
	}
	//pos--;
	sendPacket(ether,buffer,pos);
	pmm_free(buffer);
}

/*bool tcp_open_con(int port, void *callback_pointer) {
	int i;
	for(i = 60000; i < 65536; i++) {
		if(connections[i].in_use == false) {
			break;
		}
	}
	connections[i].in_use = true;
	connections[i].remote_port = port;
	register_tcp_listener(port,callback_pointer);
}*/