#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "ipv4.h"
#include "mac.h"
#include "checksum.h"

void *ipv4_payload(ipv4_t *pkg) {
  unsigned offset = ((pkg->version & 0x0F)<<2)-sizeof(ipv4_t);
  
  return pkg->payload+offset;
}

uint16_t ipv4_checksum(ipv4_t *pkg) {
  unsigned len;

  len = ((pkg->version & 0x0F)<<2);

  return checksum16(pkg, len);
}

int get_ipv4_addr(raw_iface_t *iface, struct in_addr *addr) {
  struct ifreq ifr;
  int ret;
  
  strncpy(ifr.ifr_name, iface->ifname, IFNAMSIZ-1);
  ifr.ifr_addr.sa_family = AF_INET;
  
  if((ret = ioctl(iface->fd, SIOCGIFADDR, &ifr)) < 0) {
    perror("SIOCGIFADDR");
    return ret;
  }
  
  *addr = (((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
  
  return ret;
}

/* basic ipv4 assembler
 * payload should not be greater than IP_MAXLEN
 * since we are not implementing fragmentation */
int send_ipv4(raw_iface_t *iface,
              macaddr_t src_mac, ipaddr_t src_ip,
              macaddr_t dst_mac, ipaddr_t dst_ip,
              void *payload, size_t len, uint8_t proto, uint8_t ttl) {
  uint8_t buffer[IP_MAXLEN];
  
  ipv4_t *pkg = (ipv4_t*)buffer;
  
  if(len > IP_MAXLEN) return -1;
  bzero(pkg, sizeof(ipv4_t));
  
  len += sizeof(ipv4_t);
  pkg->version = 0x45;
  pkg->total_len = htons(len);
  pkg->fragmentation = htons(0x4000);
  pkg->ttl = ttl;
  pkg->proto = proto;
  pkg->src_ip = src_ip;
  pkg->dst_ip = dst_ip;
  pkg->header_checksum = ipv4_checksum(pkg);

  memcpy(ipv4_payload(pkg), payload, len-sizeof(ipv4_t));
  
  return send_frame(iface, pkg, len, src_mac , dst_mac, ETH_P_IP);
}

/* Local Variables: */
/* mode: c */
/* tab-width: 2 */
/* indent-tabs-mode: nil */
/* End: */
