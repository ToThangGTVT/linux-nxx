/* nxcompat: network helpers (see include/nxcompat/net.h) */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <switch.h>
#include "nxcompat/net.h"

/* libslirp asks for every DNS query; re-query nifm at most this often */
#define DNS_CACHE_SECONDS 10

int nxc_get_dns_addr(struct in_addr *addr)
{
    static u32 cached;
    static time_t cached_at;
    static bool nifm_ready;
    time_t now = time(NULL);

    if (!cached || now - cached_at >= DNS_CACHE_SECONDS) {
        u32 ip = 0, mask = 0, gw = 0, dns1 = 0, dns2 = 0;

        if (!nifm_ready && R_SUCCEEDED(nifmInitialize(NifmServiceType_User))) {
            nifm_ready = true;
        }
        if (nifm_ready &&
            R_SUCCEEDED(nifmGetCurrentIpConfigInfo(&ip, &mask, &gw, &dns1, &dns2)) &&
            dns1) {
            cached = dns1;   /* already in network byte order */
        } else if (!cached) {
            inet_pton(AF_INET, "8.8.8.8", &cached);
        }
        cached_at = now;
    }
    addr->s_addr = cached;
    return 0;
}
