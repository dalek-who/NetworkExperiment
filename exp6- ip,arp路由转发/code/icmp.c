#include "include/icmp.h"
#include "include/ip.h"
#include "include/rtable.h"
#include "include/arp.h"
#include "include/base.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// send icmp packet
void icmp_send_packet(const char *in_pkt, int len, u8 type, u8 code)
{
    // ip analysis
    struct iphdr *in_ip_hdr = packet_to_ip_hdr(in_pkt);
    u32 out_daddr = ntohl(in_ip_hdr->saddr);
    u32 out_saddr = longest_prefix_match(out_daddr)->iface->ip;

    // construct icmp: space might be too large
    long tot_size = 0;
    char *out_pkt = NULL;
    if (type != ICMP_ECHOREPLY) {
        tot_size = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + ICMP_HDR_SIZE +
            IP_HDR_SIZE(in_ip_hdr) + 8;
    } else {
        tot_size = len - IP_HDR_SIZE(in_ip_hdr) + IP_BASE_HDR_SIZE;
    }

    out_pkt = (char*) malloc(tot_size);
    if (!out_pkt) {
        exit(-1);
    }

    struct icmphdr *icmp = (struct icmphdr *)(out_pkt + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);

    // ether header
    struct ether_header *eh = (struct ether_header *)out_pkt;
    eh->ether_type = htons(ETH_P_IP);

    // ip header
    struct iphdr *out_ip_hdr = packet_to_ip_hdr(out_pkt);
    ip_init_hdr(out_ip_hdr,
                out_saddr,
                out_daddr,
                tot_size - ETHER_HDR_SIZE,
                IPPROTO_ICMP);

    // icmp header
    memset(icmp, 0, ICMP_HDR_SIZE);
    icmp->code = code;
    icmp->type = type;

    // icmp rest
    if (type != ICMP_ECHOREPLY) {
        memcpy(((char*)icmp + ICMP_HDR_SIZE),
               (char*)in_ip_hdr,
               IP_HDR_SIZE(in_ip_hdr) + 8);
    } else {
        memcpy(((char*)icmp) + ICMP_HDR_SIZE - 4,
               in_pkt + ETHER_HDR_SIZE + IP_HDR_SIZE(in_ip_hdr) + 4,
               len - ETHER_HDR_SIZE - IP_HDR_SIZE(in_ip_hdr) - 4);
    }
    icmp->checksum = icmp_checksum(icmp, tot_size-ETHER_HDR_SIZE-IP_BASE_HDR_SIZE);
    ip_send_packet(out_pkt, tot_size);
}
