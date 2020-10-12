/*
 * page-types: Tool for querying page flags
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should find a copy of v2 of the GNU General Public License somewhere on
 * your Linux system; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * Copyright (C) 2009 Intel corporation
 *
 * Authors: Wu Fengguang <fengguang.wu@intel.com>
 */

#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/statfs.h>
#include "../../include/linux/magic.h"


#ifndef MAX_PATH
# define MAX_PATH 256
#endif

#ifndef STR
# define _STR(x) #x
# define STR(x) _STR(x)
#endif

/*
 * pagemap kernel ABI bits
 */

#define PM_ENTRY_BYTES      sizeof(uint64_t)
#define PM_STATUS_BITS      3
#define PM_STATUS_OFFSET    (64 - PM_STATUS_BITS)
#define PM_STATUS_MASK      (((1LL << PM_STATUS_BITS) - 1) << PM_STATUS_OFFSET)
#define PM_STATUS(nr)       (((nr) << PM_STATUS_OFFSET) & PM_STATUS_MASK)
#define PM_PSHIFT_BITS      6
#define PM_PSHIFT_OFFSET    (PM_STATUS_OFFSET - PM_PSHIFT_BITS)
#define PM_PSHIFT_MASK      (((1LL << PM_PSHIFT_BITS) - 1) << PM_PSHIFT_OFFSET)
#define PM_PSHIFT(x)        (((u64) (x) << PM_PSHIFT_OFFSET) & PM_PSHIFT_MASK)
#define PM_PFRAME_MASK      ((1LL << PM_PSHIFT_OFFSET) - 1)
#define PM_PFRAME(x)        ((x) & PM_PFRAME_MASK)

#define PM_PRESENT          PM_STATUS(4LL)
#define PM_SWAP             PM_STATUS(2LL)


/*
 * kernel page flags
 */

#define KPF_BYTES		8
#define PROC_KPAGEFLAGS		"/proc/kpageflags"

/* copied from kpageflags_read() */
#define KPF_LOCKED		0
#define KPF_ERROR		1
#define KPF_REFERENCED		2
#define KPF_UPTODATE		3
#define KPF_DIRTY		4
#define KPF_LRU			5
#define KPF_ACTIVE		6
#define KPF_SLAB		7
#define KPF_WRITEBACK		8
#define KPF_RECLAIM		9
#define KPF_BUDDY		10

/* [11-20] new additions in 2.6.31 */
#define KPF_MMAP		11
#define KPF_ANON		12
#define KPF_SWAPCACHE		13
#define KPF_SWAPBACKED		14
#define KPF_COMPOUND_HEAD	15
#define KPF_COMPOUND_TAIL	16
#define KPF_HUGE		17
#define KPF_UNEVICTABLE		18
#define KPF_HWPOISON		19
#define KPF_NOPAGE		20
#define KPF_KSM			21

/* [32-] kernel hacking assistances */
#define KPF_RESERVED		32
#define KPF_MLOCKED		33
#define KPF_MAPPEDTODISK	34
#define KPF_PRIVATE		35
#define KPF_PRIVATE_2		36
#define KPF_OWNER_PRIVATE	37
#define KPF_ARCH		38
#define KPF_UNCACHED		39

/* [48-] take some arbitrary free slots for expanding overloaded flags
 * not part of kernel API
 */
#define KPF_READAHEAD		48
#define KPF_SLOB_FREE		49
#define KPF_SLUB_FROZEN		50
#define KPF_SLUB_DEBUG		51

#define KPF_ALL_BITS		((uint64_t)~0ULL)
#define KPF_HACKERS_BITS	(0xffffULL << 32)
#define KPF_OVERLOADED_BITS	(0xffffULL << 48)
#define BIT(name)		(1ULL << KPF_##name)
#define BITS_COMPOUND		(BIT(COMPOUND_HEAD) | BIT(COMPOUND_TAIL))

static const char *page_flag_names[] = {
	[KPF_LOCKED]		= "L:locked",
	[KPF_ERROR]		= "E:error",
	[KPF_REFERENCED]	= "R:referenced",
	[KPF_UPTODATE]		= "U:uptodate",
	[KPF_DIRTY]		= "D:dirty",
	[KPF_LRU]		= "l:lru",
	[KPF_ACTIVE]		= "A:active",
	[KPF_SLAB]		= "S:slab",
	[KPF_WRITEBACK]		= "W:writeback",
	[KPF_RECLAIM]		= "I:reclaim",
	[KPF_BUDDY]		= "B:buddy",

	[KPF_MMAP]		= "M:mmap",
	[KPF_ANON]		= "a:anonymous",
	[KPF_SWAPCACHE]		= "s:swapcache",
	[KPF_SWAPBACKED]	= "b:swapbacked",
	[KPF_COMPOUND_HEAD]	= "H:compound_head",
	[KPF_COMPOUND_TAIL]	= "T:compound_tail",
	[KPF_HUGE]		= "G:huge",
	[KPF_UNEVICTABLE]	= "u:unevictable",
	[KPF_HWPOISON]		= "X:hwpoison",
	[KPF_NOPAGE]		= "n:nopage",
	[KPF_KSM]		= "x:ksm",

	[KPF_RESERVED]		= "r:reserved",
	[KPF_MLOCKED]		= "m:mlocked",
	[KPF_MAPPEDTODISK]	= "d:mappedtodisk",
	[KPF_PRIVATE]		= "P:private",
	[KPF_PRIVATE_2]		= "p:private_2",
	[KPF_OWNER_PRIVATE]	= "O:owner_private",
	[KPF_ARCH]		= "h:arch",
	[KPF_UNCACHED]		= "c:uncached",

	[KPF_READAHEAD]		= "I:readahead",
	[KPF_SLOB_FREE]		= "P:slob_free",
	[KPF_SLUB_FROZEN]	= "A:slub_frozen",
	[KPF_SLUB_DEBUG]	= "E:slub_debug",
};


/*
 * data structures
 */

static pid_t		opt_pid;	/* process to walk */

#define MAX_ADDR_RANGES	1024
static int		nr_addr_ranges;
static unsigned long	opt_offset[MAX_ADDR_RANGES];
static unsigned long	opt_size[MAX_ADDR_RANGES];

#define MAX_VMAS	10240
static int		nr_vmas;
static unsigned long	pg_start[MAX_VMAS];
static unsigned long	pg_end[MAX_VMAS];

static unsigned long	vm_addr_start[MAX_VMAS];
static unsigned long	vm_addr_end[MAX_VMAS];

#define MAX_BIT_FILTERS	64
static int		nr_bit_filters;
static uint64_t		opt_mask[MAX_BIT_FILTERS];
static uint64_t		opt_bits[MAX_BIT_FILTERS];

static int		page_size;

static int		pagemap_fd;
static int		kpageflags_fd;

static int		opt_hwpoison;
static int		opt_unpoison;

static char		hwpoison_debug_fs[MAX_PATH+1];
static int		hwpoison_inject_fd;
static int		hwpoison_forget_fd;

#define HASH_SHIFT	13
#define HASH_SIZE	(1 << HASH_SHIFT)
#define HASH_MASK	(HASH_SIZE - 1)
#define HASH_KEY(flags)	(flags & HASH_MASK)

static unsigned long	total_pages;
static unsigned long	nr_pages[HASH_SIZE];
static uint64_t 	page_flags[HASH_SIZE];

static unsigned long buckets[65];  //Viswa

/*
 * helper functions
 */

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define min_t(type, x, y) ({			\
	type __min1 = (x);			\
	type __min2 = (y);			\
	__min1 < __min2 ? __min1 : __min2; })

#define max_t(type, x, y) ({			\
	type __max1 = (x);			\
	type __max2 = (y);			\
	__max1 > __max2 ? __max1 : __max2; })

static unsigned long pages2mb(unsigned long pages)
{
	return (pages * page_size) >> 20;
}

static void fatal(const char *x, ...)
{
	va_list ap;

	va_start(ap, x);
	vfprintf(stderr, x, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

static int checked_open(const char *pathname, int flags)
{
	int fd = open(pathname, flags);

	if (fd < 0) {
		perror(pathname);
		exit(EXIT_FAILURE);
	}

	return fd;
}

/*
 * pagemap/kpageflags routines
 */

static unsigned long do_u64_read(int fd, char *name,
				 uint64_t *buf,
				 unsigned long index,
				 unsigned long count)
{
	long bytes;

	if (index > ULONG_MAX / 8)
		fatal("index overflow: %lu\n", index);

	if (lseek(fd, index * 8, SEEK_SET) < 0) {
		perror(name);
		exit(EXIT_FAILURE);
	}

	bytes = read(fd, buf, count * 8);
	if (bytes < 0) {
		perror(name);
		exit(EXIT_FAILURE);
	}
	if (bytes % 8)
		fatal("partial read: %lu bytes\n", bytes);

	return bytes / 8;
}

static unsigned long pagemap_read(uint64_t *buf,
				  unsigned long index,
				  unsigned long pages)
{
	return do_u64_read(pagemap_fd, "/proc/pid/pagemap", buf, index, pages);
}

static unsigned long pagemap_pfn(uint64_t val)
{
	unsigned long pfn;

	if (val & PM_PRESENT)
		pfn = PM_PFRAME(val);
	else
		pfn = 0;

	return pfn;
}


static const char *page_flag_type(uint64_t flag)
{
	if (flag & KPF_HACKERS_BITS)
		return "(r)";
	if (flag & KPF_OVERLOADED_BITS)
		return "(o)";
	return "   ";
}

//Viswa: modified usage
static void usage(void)
{
	int i, j;

	printf(
"page-cont [options]\n"
"            -p|--pid     pid           Walk process address space\n"
"            -h|--help                  Show this usage message\n"
	);
}

static unsigned long long parse_number(const char *str)
{
	unsigned long long n;

	n = strtoll(str, NULL, 0);

	if (n == 0 && str[0] != '0')
		fatal("invalid name or number: %s\n", str);

	return n;
}

static void parse_pid(const char *str)
{
	FILE *file;
	char buf[5000];

	opt_pid = parse_number(str);

	printf("opt_pid = %i\n", opt_pid);

	sprintf(buf, "/proc/%d/pagemap", opt_pid);
	pagemap_fd = checked_open(buf, O_RDONLY);

	sprintf(buf, "/proc/%d/maps", opt_pid);
	file = fopen(buf, "r");
	if (!file) {
		perror(buf);
		exit(EXIT_FAILURE);
	}

	while (fgets(buf, sizeof(buf), file) != NULL) {
		unsigned long vm_start;
		unsigned long vm_end;
		unsigned long long pgoff;
		int major, minor;
		char r, w, x, s;
		unsigned long ino;
		int n;
		//printf("buf: %s\n", buf);
		n = sscanf(buf, "%lx-%lx %c%c%c%c %llx %x:%x %lu",
			   &vm_start,
			   &vm_end,
			   &r, &w, &x, &s,
			   &pgoff,
			   &major, &minor,
			   &ino);
		if (n < 10) {
			fprintf(stderr, "unexpected line: %s\n", buf);
			continue;
		}
		pg_start[nr_vmas] = vm_start / page_size;
		//vm_addr_start[nr_vmas] = vm_start;
		pg_end[nr_vmas] = vm_end / page_size;
		//vm_addr_end[nr_vmas] = vm__end;
		if (++nr_vmas >= MAX_VMAS) {
			fprintf(stderr, "too many VMAs\n");
			break;
		}
	}
	fclose(file);
}

//Viswa: do_common_bits_histogram
void do_common_bits_histogram()
{
	int i;
	int j;
	int k;
	unsigned long words;
	uint64_t count=0;
	uint64_t maxcount=0;

//	for(i=0;i<nr_vmas;i++)
//		maxcount += pg_end[i]-pg_start[i];

//	uint64_t *maps = (uint64_t *)malloc(sizeof(uint64_t)*maxcount);

//	uint64_t iter=0;
	printf("VPN-PPNMap:\n");
	for (i=0; i<nr_vmas; i++)
	{
		int num_pages = pg_end[i] - pg_start[i];
		uint64_t *mappings = malloc(sizeof(uint64_t) * num_pages);
		words = pagemap_read(mappings, pg_start[i], num_pages);
		//printf("Range %i: read %lu pages...\n", i, words);
		unsigned long start_page = pg_start[i] * page_size;
		unsigned long end_page = pg_end[i] * page_size;


		//printf("num_pages:%d\n",num_pages);
		//maxcount += num_pages;
/*		for( j=0;j<num_pages;j++){
			maps[iter++] = pagemap_pfn(mappings[j]);
			for( k=0;k<iter-1;k++){
				if( maps[iter-1] == maps[k]+1 )
					count++;
				
				if( maps[iter-1] == maps[k]-1 )
					count++;
			}
		}*/

		//free(maps);	

		for (j=0; j<num_pages; j++)
		{
			uint64_t pfn = pagemap_pfn(mappings[j]);
			uint64_t vpn = start_page >> 12;
			uint64_t pfn_shifted;
			uint64_t vpn_shifted;
			int matched = 0;
			if (pfn == 0)
				continue;
			//printf("...pfn [%i]: %lx\n", j, pfn);
			//printf("...vm: %lx\n", vpn);

			printf("0x%lx:0x%lx\n",vpn,pfn);
/*			for (k=0; k<64; k++)
			{
//				printf("k = %i\n", k);
				pfn_shifted = pfn << k >> k;
//				printf("pfn after shift: %lx\n", pfn_shifted);
				vpn_shifted = vpn << k >> k;
//				printf("vm addr after shift: %lx\n", vm_shifted);
				
				if (pfn_shifted == vpn_shifted) {
					buckets[64-k] += 1;			
//					printf(".....k = %i\n", k);
//					printf(".....pfn after shift: %lx\n", pfn_shifted);
//					printf(".....vm addr after shift: %lx\n", vpn_shifted);
//					printf(".....Matched %i bits...\n", 64-k);
					matched = 1;
					break;
				}
			}
			if (!matched)
				buckets[0]++; */

			start_page += page_size;
		}
	}
	unsigned long sum = 0;
	//free (maps);
	
	printf("VPN-PPNMap-End\n");

	/*for (i=0; i<53; i++)
	{
		printf("Bucket %i: %lu\n", i, buckets[i]);
		sum += buckets[i];
	}

	//printf("Contiguous pages: %f\n",(float)count*100/maxcount);
	printf("Sum of buckets: %lu\n", sum);*/

}

//Viswa: modified option opts
static const struct option opts[] = {
	{ "pid"       , 1, NULL, 'p' },
	{ "help"      , 0, NULL, 'h' },
	{ NULL        , 0, NULL, 0 }
};

int main(int argc, char *argv[])
{
	int c;
	if (argc != 3) { //binh
		usage();
		fatal("Wrong number of arguments\n"); 
	}
	page_size = getpagesize();

	while ((c = getopt_long(argc, argv,
				"rp:f:a:b:d:lLNXxh", opts, NULL)) != -1) {
		switch (c) {
		case 'p':
			parse_pid(optarg);
			do_common_bits_histogram();
			printf("After common bits histogram\n");
			break;
		case 'h':
			usage();
			exit(0);
		default:
			usage();
			exit(1);
		}
	}
	return 0;
}
