/**
 * @file   pktbuf.c
 * @author Yifeng Jin <hqking@gmail.com>
 * @date   Sun Mar 24 22:20:47 2013
 * 
 * @brief  
 * 
 * 
 */
#include <stdlib.h>
#include "queue.h"
#include "pktbuf.h"

typedef unsigned char	buf_t;
typedef unsigned int	size_t;

STAILQ_HEAD(pktList, pkt);

struct pkt {
	buf_t *payload;
	size_t len;
	STAILQ_ENTRY(pkt) entries;
};

data_t * pktCreate(struct pktList *list, size_t len)
{
	data_t *buf;

	buf = (data_t *)malloc(sizeof(len));

	return buf;
}

void pktDelete(struct pktList *list, data_t *buf)
{
	free(buf);
}

int pktAppend(struct pktList *list, buf_t *buf, size_t len)
{
	struct pkt *pkt;

	pkt = (struct pkt *)malloc(sizeof(struct pkt));
	if (pkt == NULL)
		return -1;

	pkt->payload = buf;
	pkt->len = len;

	/* lock */
	if (STAILQ_EMPTY(list)) {
		STAILQ_INSERT_HEAD(list, pkt, entries);
		/* inform start send */
	} else {
		STAILQ_INSERT_TAIL(list, pkt, entries);
	}
	/* unlock */

	return 0;
}

data_t * pktFetch(struct pktList *list, size_t *len)
{
	struct pkt *pkt;
	data_t *buf;

	/* lock */
	pkt = STAILQ_FIRST(list);

	if (pkt) {
		STIALQ_REMOVE_HEAD(list, entires);
	}
	/* unlock */

	if (pkt == NULL)
		return NULL;

	*len = pkt->len;
	buf = pkt->payload;

	free(pkt);

	return buf;
}

data_t * pktPeek(struct pktList *list, size_t *len)
{
        struct pkt *pkt;

	pkt = STIALQ_FIRST(list);

	if (pkt == NULL)
		return NULL;

	*len = pkt->len;

	return pkt->payload;
}

void pktQueueInit(struct pktLIst *list)
{
	STAILQ_INIT(list);
}
