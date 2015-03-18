#include <sys/queue.h>

/**
 * @brief an entry of the data in the ring list.
 */
struct ringbuf_item {
	int lock_item;	/* TODO: lock for the single item */
	int valued;	/* flag, this entry must be ignored if it's not set. */
	void *data;	/* container of the data of this entry. */
};

/**
 * @brief holder of the ring buffer denoted ringbuf_item.
 */
struct ringbuf_entry {
	int lock_entry;	/* TODO: lock for the single entry */
	struct ringbuf_item *item;
	int index;	/* current index */
	int filled;	/* whether it's been filled once. */
	char *key;	/* key: a printable string */
	int n_items;	/* maximum number of the items in the entry */
	int data_size;	/* data size */
	LIST_ENTRY(ringbuf_entry) link;
};

/**
 * @brief holder of the list of ringbuf_entry.
 */
struct ringbuf_holder {
	int lock_holder;	/* TODO: lock for the whole buffers */
	LIST_HEAD(ringbuf_head, ringbuf_entry) entry;
	int debug;
};

/**
 * @brief container to provide items for the callback of ringbuf_get_item().
 */
struct ringbuf_data_ctx {
	char *buf;
	char *ptr;
	int buflen;
	int data_size;
	int num_data;
};

struct ringbuf_entry *ringbuf_find_entry(struct ringbuf_holder *, char *);
int ringbuf_set_data_ctx(void *, void *);
int ringbuf_get_item(struct ringbuf_holder *, char *,
    int, int (*)(void *, void *), void *);
int ringbuf_add_item(struct ringbuf_holder *, char *, void *);
void ringbuf_delete_entry(struct ringbuf_entry *);
struct ringbuf_entry *ringbuf_add_entry(struct ringbuf_holder *,
    char *, int, int);
void ringbuf_destroy(struct ringbuf_holder *);
int ringbuf_add(struct ringbuf_holder *, char *, int, int, void *);
struct ringbuf_holder *ringbuf_init(int);
