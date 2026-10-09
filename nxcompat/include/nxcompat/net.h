/* nxcompat: network helpers for Horizon (no libnx headers) */
#ifndef NXCOMPAT_NET_H
#define NXCOMPAT_NET_H

#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Fill @addr with the console's current primary DNS server (from nifm),
 * falling back to a public resolver. Returns 0 on success.
 */
int nxc_get_dns_addr(struct in_addr *addr);

#ifdef __cplusplus
}
#endif

#endif
