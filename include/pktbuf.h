/**
 * @file   pktbuf.c
 * @author Yifeng Jin <hqking@gmail.com>
 * @date   Sun Mar 26 14:32:05 2013
 * 
 * @brief  
 * 
 * 
 */
#ifndef __PKTBUF_C__
#define __PKTBUF_C__

#include "queue.h"

struct pktList;

typedef void (*pktEvent)(struct pktList *, void *arg);

STAILQ_HEAD(pktHead, pkt);

struct pktList {
	struct pktHead head;
	void *arg;
	pktEvent start;
	pktEvent end;
};

int pktAppend(struct pktList *list, buf_t *buf, size_t len);
buf_t * pktFetch(struct pktList *list, size_t *len);
buf_t * pktPeek(struct pktList *list, size_t *len);
int pktHasData(struct pktList *list); 
void pktQueueInit(struct pktList *list, pktEvent start, pktEvent end, void *arg);
#endif /* __PKTBUF_C__ */
