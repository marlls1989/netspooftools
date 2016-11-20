// -----------------------------------------------------------
// Developed By Marcos Luiggi Sartori
//              Leonardo Pavanatto Soares
// Pontifical Catholic University of Rio Grande do Sul (PUCRS)
// Computer Networks Laboratory - April 11, 2016
// -----------------------------------------------------------
// File: arp.c
// Description: applies arp protocol to a given payload.
// -----------------------------------------------------------
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <arpa/inet.h>

#include "arp.h"
#include "mac.h"

// Used for debugging proposes during development
void print_bytearray(uint8_t *array, unsigned length,
										 unsigned base, char separator) {
  unsigned i;
	
  for(i = 0 ; i < length ; i++) {
    if(i) putchar(separator);
    printf(base == 16 ? "%02X" : "%d", array[i]);
  }
}

void arp_print(arp_t *arp) {
  uint16_t op;
	
  switch(op = ntohs(arp->operation)) {
  case ARP_REQUEST:
    printf("ARP Request\n");
    break;
  case ARP_REPLY:
    printf("ARP Reply\n");
    break;
  default:
    printf("ARP Unknown operation 0x%04X\n", op);
  }

  printf("HWProto: 0x%04X\n", htons(arp->htype));
  printf("PPRoto: 0x%04X\n", htons(arp->ptype));

  printf("Source HWAddr: ");
  print_bytearray(arp_source_haddr(arp), arp->hlen, 16, ':');
  printf("\nSource PAddr: ");
  print_bytearray(arp_source_paddr(arp), arp->plen, 10, '.');

  printf("\nTarget HWAddr: ");
  print_bytearray(arp_target_haddr(arp), arp->hlen, 16, ':');
  printf("\nTarget PAddr: ");
  print_bytearray(arp_target_paddr(arp), arp->plen, 10, '.');
  printf("\n");
}

// Assembles an ARP Request and send it
int send_arp4_request(raw_iface_t *iface, macaddr_t src_macaddr, ipaddr_t src_ipaddr,
											macaddr_t target_macaddr, ipaddr_t target_ip) {
  arp_t arp;

  arp.htype = htons(1);
  arp.ptype = htons(IPV4_ETHERTYPE);

  arp.hlen = ETH_ALEN;
  arp.plen = IP_ALEN;

  arp.operation = htons(ARP_REQUEST);

  memcpy(arp.payload.v4.src_macaddr, src_macaddr, ETH_ALEN);
  memcpy(arp.payload.v4.src_ipaddr, src_ipaddr, IP_ALEN);
  
  bzero(arp.payload.v4.dest_macaddr, ETH_ALEN);
  memcpy(arp.payload.v4.dest_ipaddr, target_ip, IP_ALEN);

  return send_frame(iface, &arp, sizeof(arp_t), src_macaddr, target_macaddr, ARP_ETHERTYPE);
}

// Assembles an ARP reply and send it
int send_arp4_reply(raw_iface_t *iface, macaddr_t src_macaddr, ipaddr_t src_ipaddr,
		    macaddr_t target_macaddr, ipaddr_t target_ip) {
  arp_t arp;

  arp.htype = htons(1);
  arp.ptype = htons(IPV4_ETHERTYPE);

  arp.hlen = ETH_ALEN;
  arp.plen = IP_ALEN;

  arp.operation = htons(ARP_REPLY);

  memcpy(arp.payload.v4.src_macaddr, src_macaddr, ETH_ALEN);
  memcpy(arp.payload.v4.src_ipaddr, src_ipaddr, IP_ALEN);
  
  memcpy(arp.payload.v4.dest_macaddr, target_macaddr, ETH_ALEN);
  memcpy(arp.payload.v4.dest_ipaddr, target_ip, IP_ALEN);

  return send_frame(iface, &arp, sizeof(arp_t), src_macaddr, target_macaddr, ARP_ETHERTYPE);
}

// SIGALRM cappll back used to timeout the ARP lookup process
static volatile unsigned timeout = 0;
static void lookup_timeout() {
  signal(SIGALRM, SIG_IGN);
  timeout = 1;
}

// Assembles an arp request and waits for the reply, filling target_mac
int arp4_lookup(raw_iface_t *iface, ipaddr_t src_ipaddr, macaddr_t src_mac,
		ipaddr_t target_ipaddr, macaddr_t target_mac) {
  int ret;
  uint8_t buffer[sizeof(macframe_t)+sizeof(arp_t)];
  macframe_t *frame = (macframe_t*)buffer;
  arp_t *arp = (arp_t*)(frame->payload);

  // Broadcast an ARP request
  if((ret = send_arp4_request(iface, src_mac, src_ipaddr,
			      broadcast_macaddr, target_ipaddr)) < 0)
    return ret;

  // Wait 3 seconds for a reply
  timeout = 0;
  signal(SIGALRM, lookup_timeout);
  alarm(3);
  while(!timeout) {
    if((ret = recv_frame(iface, buffer, sizeof(buffer))) < 0)
      return ret;

    /* if valid frame from target_ipaddr is received
     * copy target_macaddr */
    if((ntohs(frame->ethertype) == ARP_ETHERTYPE) &&
       (!memcmp(arp->payload.v4.src_ipaddr, target_ipaddr, IP_ALEN))) {
      memcpy(target_mac, arp->payload.v4.src_macaddr, ETH_ALEN);
      alarm(0);
      signal(SIGALRM, SIG_IGN);
      return ret;
    }
  }

  return -2;
}

// Parse address string into ipaddr_t format
int parse_ipv4str(ipaddr_t ip, char *str) {
  unsigned i, j;
  uint8_t k;

  while(isspace(*str)) str++;

  for(i = 0 ; i < IP_ALEN; i++, str++) {
    k = 0;
    for(j = 0 ; (j < 3) && *str && isdigit(*str); j++, str++) {
      k *= 10;
      k += *str - '0';
    }

    ip[i] = k;
    
    if(*str && *str != '.')
      break;
  }

  return IP_ALEN - i;
}

/* Local Variables: */
/* mode: c */
/* tab-width: 2 */
/* End: */