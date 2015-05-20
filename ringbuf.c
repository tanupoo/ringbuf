/**
 * @brief pre-allocated ring buffer
 */
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>

#include "ringbuf.h"

struct ringbuf_entry *
ringbuf_find_entry(struct ringbuf_holder *holder, char *key)
{
	struct ringbuf_entry *p;
	int len = strlen(key);

	LIST_FOREACH(p, &holder->entry, link) {
		if (strncmp(p->key, key, len) == 0)
			return p;
	}

	return NULL;
}

/**
 * provide the index to be read next to the current value of the index.
 * @param idx current index in the list.
 * @return index in the list to be accessed next of the param "idx".
 */
static int
ringbuf_next_index(struct ringbuf_entry *entry, int idx)
{
	if (idx < 0)
		errx(1, "panic idx < 0");

	if (idx > 0)
		return idx - 1;

	if (entry->filled) {
		entry->filled--;
		return entry->n_items - 1;
	}

	return -1;	/* no data in the list */
}

/**
 * @brief output the amount of data and pass to the callback.
 * @param max_data the number of data to get.
 * @param cb callback function to output the data.
 * @param ctx context parameter for the callback.
 * @return total bytes of provided data.
 * @note "cb" must return the length of the data chunk.
 */
int
ringbuf_get_item(struct ringbuf_holder *holder, char *key,
    int max_data, int (*cb)(void *, void *), void *ctx)
{
	struct ringbuf_entry *entry;
	int idx;
	int t_size, count;

	/* find an entry */
	if ((entry = ringbuf_find_entry(holder, key)) == NULL) {
		if (holder->debug > 0)
			warnx("ERROR: %s: no such entry for key=%s\n",
			    __FUNCTION__, key);
		return -1;	/* ignore */
	}

	if (holder->debug > 1) {
		printf("DEBUG: %s: try to get %d items key=%s\n",
		    __FUNCTION__, max_data, key);
	}

	if (holder->debug > 0 && !cb)
		printf("DEBUG: %s: cb is not specified.\n", __FUNCTION__);

	/* lock the entry */
	entry->lock_entry = 1;

	if ((idx = ringbuf_next_index(entry, entry->index)) == -1) {
		/* nothing to output */
		if (holder->debug > 1) {
			printf("DEBUG: %s: no entries key=%s\n",
			    __FUNCTION__, key);
		}
		entry->lock_entry = 0;
		return 0;
	}

	t_size = 0;
	for (count = 0; count < max_data; count++) {
		/* is there a data to be read ? */
		if (!entry->item[idx].valued) {
			entry->lock_entry = 0;
			return t_size;
		}
		/*
		 * call the callback
		 */
		if (cb) {
			t_size += cb(entry->item[idx].data, ctx);
			if (!holder->preserve)
				entry->item[idx].valued = 0;
		}
		/* get next index */
		if ((idx = ringbuf_next_index(entry, idx)) == -1)
			break;
	}

	entry->lock_entry = 0;

	if (holder->debug > 1) {
		printf("DEBUG: %s: %d items has been provided key=%s\n",
		    __FUNCTION__, count, key);
	}

	return t_size;
}

/**
 * @brief add an item into the ring buffer indicated by the key.
 * @param data container of the data.
 * @note the size of the "data" must be identical to data_size that
 *       is passed to ringbuf_ini()..
 */
int
ringbuf_add_item(struct ringbuf_holder *holder, char *key, void *data)
{
	struct ringbuf_entry *entry;

	/* find an entry */
	if ((entry = ringbuf_find_entry(holder, key)) == NULL) {
		if (holder->debug > 0)
			warnx("ERROR: %s: no such entry for key=%s\n",
			    __FUNCTION__, key);
		return -1;	/* ignore */
	}

	entry->item[entry->index].valued = 1;
	memcpy(entry->item[entry->index].data, data, entry->data_size);

	if (holder->debug > 1) {
		printf("DEBUG: %s: an item has been added index=%d key=%s\n",
		    __FUNCTION__, entry->index, key);
	}

	entry->index++;

	if (entry->index >= entry->n_items) {
		entry->index = 0;
		entry->filled++;
		if (holder->debug > 1) {
			printf("DEBUG: %s: ringbuf has been filled %d times "
			    "key=%s\n", __FUNCTION__, entry->filled, key);
		}
	}

	return 0;

}

void
ringbuf_delete_entry(struct ringbuf_entry *entry)
{
	free(entry->item);
	free(entry);
}

/**
 * @brief allocate an entry and add it to the holder.
 * @param key a printable string as the key of this entry.
 * @param n_items maximum number of the items in the ring buffer.
 * @param data_size the size of the single data.
 */
struct ringbuf_entry *
ringbuf_add_entry(struct ringbuf_holder *holder,
    char *key, int n_items, int data_size)
{
	struct ringbuf_entry *new;
	int i;

	if ((new = calloc(1, sizeof(struct ringbuf_entry))) == NULL)
		err(1, "ERROR: %s: calloc(ringbuf_entry)", __FUNCTION__);

	new->key = strdup(key);
	new->n_items = n_items;
	if ((new->item = calloc(n_items, sizeof(struct ringbuf_item))) == NULL)
		err(1, "ERROR: %s: calloc(ringbuf_item)", __FUNCTION__);

	new->data_size = data_size;
	for (i = 0; i < n_items; i++) {
		if ((new->item[i].data = calloc(1, data_size)) == NULL)
			err(1, "ERROR: %s: calloc(data_size)", __FUNCTION__);
	}

	LIST_INSERT_HEAD(&holder->entry, new, link);

	if (holder->debug > 1) {
		printf("DEBUG: %s: an entry has been added key=%s\n",
		    __FUNCTION__, key);
	}

	return new;
}

/**
 * @brief release memory for the whole entryies and holder.
 */
void
ringbuf_destroy(struct ringbuf_holder *holder)
{
	struct ringbuf_entry *p;

	while (LIST_FIRST(&holder->entry) != NULL) {
		p = LIST_FIRST(&holder->entry);
		LIST_REMOVE(LIST_FIRST(&holder->entry), link);
                ringbuf_delete_entry(p);
	}
	free(holder);
}

/**
 * @brief add an item into the ring buffer indicated by the key.
 *        if key doesn't exist, it will create the entry for the key.
 * @param data container of the data.
 * @n_items maximum number of items.
 * @note the size of the "data" must be identical to data_size that
 *       is passed to ringbuf_init()..
 */
int
ringbuf_add(struct ringbuf_holder *holder,
    char *key, int n_items, int data_size, void *data)
{
	struct ringbuf_entry *entry;

	/* find an entry */
	if ((entry = ringbuf_find_entry(holder, key)) == NULL) {
		if (holder->debug > 1) {
			printf("DEBUG: %s: %s doesn't exist, has been added.\n",
			    __FUNCTION__, key);
		}
		ringbuf_add_entry(holder, key, n_items, data_size);
	}

	return ringbuf_add_item(holder, key, data);
}

/**
 * @brief initialize the holder of the list of the ring buffer.
 * @param f_preserve flag of an item preservation.
 */
struct ringbuf_holder *
ringbuf_init(int f_preserve, int f_debug)
{
	struct ringbuf_holder *new;

	if ((new = calloc(1, sizeof(struct ringbuf_holder))) == NULL)
		err(1, "ERROR: %s: calloc(ringbuf_holder)", __FUNCTION__);

	LIST_INIT(&new->entry);

	new->preserve = f_preserve;
	new->debug = f_debug;

	return new;
}
