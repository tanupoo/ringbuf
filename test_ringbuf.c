#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <time.h>	/* time() */
#include <errno.h>

#include "ringbuf.h"

#define SIP_U 1000000

char *prog_name = NULL;

/* ring buffer holder */
struct ringbuf_holder *holder;

struct data {
	int seq;
	long num;
};

struct data_ctx {
	char *buf;
	char *ptr;
	int buflen;
	int data_size;
	int num_data;
};

useconds_t interval_add = 100000;	/* 100 (ms) */
useconds_t interval_get = 1000000;	/* 1000 (ms) */
int n_items = 5;
int data_size = sizeof(struct data);

long long timer = 0;		/* global timer */
char *key001 = "key001";
char *key002 = "key002";

int f_debug = 0;

void
usage()
{
	printf(
"Usage: %s [-dh] [-A interval_add] [-G interval_get]\n"
"          [-s n_items] [-n read_size]\n"
	, prog_name);

	exit(0);
}

void
sig_alrm(int signo)
{
	static int seq = 0;
	struct data data;

	timer += interval_add;

	data.seq = seq++;
	data.num = random();

	/* add an item into the entry "key001" */
	ringbuf_add(holder, key001, n_items, data_size, &data);

	data.num = data.num + 1;
	/* add an item into the entry "key002" */
	ringbuf_add(holder, key002, n_items, data_size, &data);
}

void
set_output_interval(struct timeval *tv)
{
	memset(tv, 0, sizeof(*tv));
	tv->tv_sec = interval_get / SIP_U;
	tv->tv_usec = interval_get % SIP_U;
}

int
print_data(struct data_ctx *c)
{
	struct data *d;
	char *p;
	int i;

	p = c->buf + (c->num_data - 1) * c->data_size;
	for (i = 0; i < c->num_data; i++) {
		d = (struct data *)p;
		printf("seq=%06d num=%ld\n", d->seq, d->num);
		p -= c->data_size;
	}

	return 0;
}

int
output_data(void *data, void *ctx)
{
	struct data *d = data;
	struct data_ctx *c = ctx;
	int data_size = sizeof(*d);

	if (c->ptr - c->buf > c->buflen)
		warnx("WARN: c.ptr - c.buf > c.buflen");
	memcpy(c->ptr, data, data_size);
	c->ptr += data_size;
	c->num_data++;

	return data_size;
}

int
test_ringbuf(int n_count, int n_read)
{
	struct timeval tv, tv_start, tv_end, tv_diff;
	struct data_ctx ctx;
	int error;

	srandom(time(NULL));

	if (signal(SIGALRM, sig_alrm) == SIG_ERR)
		err(1, "signal(SIGALRM)");

	ualarm(interval_add, interval_add);

	ctx.buflen = sizeof(struct data) * n_read;
	if ((ctx.buf = calloc(1, ctx.buflen)) == NULL)
		err(1, "calloc(buf)");
	ctx.data_size = sizeof(struct data);

	set_output_interval(&tv);

	while (n_count--) {
		gettimeofday(&tv_start, NULL);
		error = select(1, NULL, NULL, NULL, &tv);
		if (error == -1) {
			switch (errno) {
			case EINTR:
				gettimeofday(&tv_end, NULL);
				timersub(&tv_end, &tv_start, &tv_diff);
				timersub(&tv, &tv_diff, &tv);
				if (tv.tv_sec < 0) {
					tv.tv_sec = 0;
					tv.tv_usec = 0;
				}
				continue;
			default:
				err(1, "ERROR: %s: select()", __FUNCTION__);
			}
		}
		ctx.ptr = ctx.buf;
		ctx.num_data = 0;
		/*
		 * get items (maximum n_read) in the "key001" entry.
		 * the callback is "output_data".
		 * ctx is the parameter to be passed to the callback.
		 */
		ringbuf_get_item(holder, key001, n_read, output_data, &ctx);
		print_data(&ctx);
		set_output_interval(&tv);
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	int ch;
	int n_count = 100;
	int f_prepare = 0;
	int n_read = 15;

	prog_name = 1 + rindex(argv[0], '/');

	while ((ch = getopt(argc, argv, "A:G:s:n:Pdh")) != -1) {
		switch (ch) {
		case 'A':
			interval_add = atol(optarg);
			break;
		case 'G':
			interval_get = atol(optarg);
			break;
		case 's':
			n_items = atoi(optarg);
			break;
		case 'n':
			n_read = atoi(optarg);
			break;
		case 'P':
			f_prepare++;
			break;
		case 'd':
			f_debug++;
			break;
		case 'h':
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		usage();

	printf("# of items           = %d\n", n_items);
	printf("interval to add      = %d\n", interval_add);
	printf("interval to get      = %d\n", interval_get);
	printf("max # of data to get = %d\n", n_read);

	/* initialize the buffer */
	holder = ringbuf_init(f_debug);

	if (f_prepare) {
		/* add entries */
		ringbuf_add_entry(holder, key001, n_items, data_size);
		ringbuf_add_entry(holder, key002, n_items, data_size);
	}

	/* main routine */
	test_ringbuf(n_count, n_read);

	/* destroy the buffer */
	ringbuf_destroy(holder);

	return 0;
}
