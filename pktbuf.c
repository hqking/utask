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
#include "types.h"
#include "pktbuf.h"

struct pkt {
	buf_t *payload;
	size_t len;
	STAILQ_ENTRY(pkt) entries;
};

buf_t * bufCreate(struct pktList *list, size_t len)
{
	return (buf_t *)malloc(sizeof(len));
}

void bufDelete(struct pktList *list, buf_t *buf)
{
	free(buf);
}

int pktAppend(struct pktList *list, buf_t *buf, size_t len)
{
	struct pktHead *head;
	struct pkt *pkt;

	pkt = (struct pkt *)malloc(sizeof(struct pkt));
	if (pkt == NULL)
		return -1;

	head = &list->head;

	pkt->payload = buf;
	pkt->len = len;

	/* lock */
	if (STAILQ_EMPTY(head)) {
		STAILQ_INSERT_HEAD(head, pkt, entries);
		/* unlock */
		
		if (list->start)
			list->start(list, list->arg);
	} else {
		STAILQ_INSERT_TAIL(head, pkt, entries);
		/* unlock */
	}

	return 0;
}

buf_t * pktFetch(struct pktList *list, size_t *len)
{
	struct pktHead *head;
	struct pkt *pkt;
	buf_t *buf;

	head = &list->head;

	/* lock */
	pkt = STAILQ_FIRST(head);

	if (pkt) {
		STAILQ_REMOVE_HEAD(head, entries);
	}
	/* unlock */

	if (pkt == NULL) {
		if (list->end)
			list->end(list, list->arg);
	
		return NULL;
	}

	*len = pkt->len;
	buf = pkt->payload;

	free(pkt);

	return buf;
}

buf_t * pktPeek(struct pktList *list, size_t *len)
{
	struct pktHead *head;
    struct pkt *pkt;

    head = &list->head;

	pkt = STAILQ_FIRST(head);

	if (pkt == NULL)
		return NULL;

	*len = pkt->len;

	return pkt->payload;
}

int pktHasData(struct pktList *list)
{
	return STAILQ_EMPTY(&list->head) ? 0 : 1;
}

void pktQueueInit(struct pktList *list, pktEvent start, pktEvent end, void *arg)
{
	STAILQ_INIT(&list->head);
	list->start = start;
	list->end = end;
	list->arg = arg;
}
