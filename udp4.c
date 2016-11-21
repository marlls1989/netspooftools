#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "udp4.h"
#include "checksum.h"

typedef struct udp4_pheader_t {
  ipaddr_t src_addr;
  ipaddr_t dst_addr;
  uint8_t padding;
  uint8_t proto;
  uint16_t length;
} udp4_pheader_t;

uint16_t udp4_checksum(udp_t *frame, ipaddr_t src_ip, ipaddr_t dst_ip) {
  udp4_pheader_t ph;
  uint32_t sum = 0;
  size_t len = ntohs(frame->length);
  
  memcpy(ph.src_addr, src_ip, IP_ALEN);
  memcpy(ph.dst_addr, dst_ip, IP_ALEN);
  ph.padding = 0;
  ph.proto = UDP_PROTO;
  ph.length = frame->length;

  sum = ~checksum16(&ph, sizeof(udp4_pheader_t));
  sum += ~checksum16(frame, len);

  while(sum >> 16)
    sum = (sum & 0xFFFF) + (sum >> 16);

  return ~sum;
}

int send_udp4(raw_iface_t *iface,
	      macaddr_t src_mac, ipaddr_t src_ip, uint16_t src_port,
	      macaddr_t dst_mac, ipaddr_t dst_ip, uint16_t dst_port,
	      void *payload, size_t len, uint8_t ttl) {
  uint8_t buffer[IP_MAXLEN];
  udp_t *frame = (udp_t*)buffer;

  if(len > UDP4_MAXLEN) return -1;

  memcpy(frame->payload, payload, len);
  len += sizeof(udp_t);
  
  frame->src_port = htons(src_port);
  frame->dst_port = htons(dst_port);
  frame->length = htons(len);
  frame->checksum = 0; //Checksumming udp over ipv4 is optional

  frame->checksum = udp4_checksum(frame, src_ip, dst_ip);

  return send_ipv4(iface,
	    src_mac, src_ip,
	    dst_mac, dst_ip,
	    frame, len, UDP_PROTO, ttl);
}