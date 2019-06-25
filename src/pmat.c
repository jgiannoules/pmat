/*
 * Physical Memory Analysis Tool (PMAT)
 *
 * !! This tool intended for development / test purposes only !!
 *
 * To display usage execute command without parameters.
 *
 */
#include "pmat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <libgen.h>

/* size in bytes */
#define BYTE_SIZE	1UL
#define WORD_SIZE	2UL
#define DWORD_SIZE	4UL

enum operation_type {
	NONE,
	READ,
	WRITE
};

enum write_type {
	ASSIGN,
	XOR,
	OR,
	AND
};

enum data_size_type {
	BYTE,
	WORD,
	DWORD
};

struct { 	
	enum operation_type	operation;
	enum write_type		write_operation;
	char 			*path;
	off_t			address;
	off_t			length;
	off_t			length_in_bytes;
	off_t			iterations;
	unsigned long		value;
	enum data_size_type	data_size;
} params;

int 	read_operation();
int 	write_operation();
int	modify_operation();
int	alignment_check();
void 	hexdump(void *, size_t, off_t, enum data_size_type, char *);
int 	parse_params(int, char **);
void 	display_params();
void 	usage();

int main(int argc, char **argv)
{
	int	ret, verbose, force_aligned_access;
	void	*buf;

	if (getenv("PMAT_DEBUG"))
		verbose = 1;
	else
		verbose = 0;
			
	force_aligned_access = 1;
		
	if (parse_params(argc, argv)) {
		usage(argv[0]);
		exit(1);
	} 
	if (verbose)
		display_params();

	if ((force_aligned_access) && alignment_check())
		exit(1);
	
	buf = malloc(params.length_in_bytes);
	if (NULL == buf) {
		perror("Malloc failed");
		return 1;
	}

	switch (params.operation) {
	case READ:
		ret = read_operation(buf);
		break;
	case WRITE:
		ret = write_operation(buf);
		break;
	default:
		exit(1);
	}

	return(ret);	
}	

int read_operation(void *buf) {
	off_t	aligned_address, aligned_length, i;
	int	page_size, fd, run = 1;
	void 	*temp, *map_base;

	page_size = getpagesize();

	/* calculate aligned address and length using page_size boundaries */
	aligned_address = params.address & ~(page_size - 1);
	aligned_length = params.length_in_bytes + (params.address - aligned_address);
	aligned_length = (aligned_length + (page_size - 1)) & ~(page_size - 1);

	if ((fd = open(params.path, O_RDONLY | O_SYNC)) == -1) {
		printf("%s could not be opened.\n", params.path);
		exit(1);
	} 

	map_base = mmap(0, aligned_length, PROT_READ, MAP_SHARED, fd, aligned_address);

	if (map_base == (void *) -1) {
		perror("Memory map failed");
		return 1;
	} 

	if (params.iterations == 0) {
		fprintf(stderr, "*** NOTICE - INFINITE LOOP REQUESTED ***\n");
	}

	temp = map_base + (params.address - aligned_address);
	printf("%p %p\n", map_base, temp);

	while (run) {
		/* perform data access sized access into temporary buffer, buf */
		for (i = 0; i < params.length; i++) {
			switch (params.data_size) {
			case BYTE:
				(((unsigned char *)buf)[i]) = (((unsigned char *)temp)[i]);
				break;
			case WORD:
				(((unsigned short *)buf)[i]) = (((unsigned short *)temp)[i]);
				break;
			case DWORD:
				(((unsigned int *)buf)[i]) = (((unsigned int *)temp)[i]);
				break;
			}
		}
		hexdump(buf, params.length, params.address, params.data_size, "");
		if (params.iterations) {
			run = (--params.iterations > 0);
		}
	}

	free(buf);
	close(fd);

	if (munmap(map_base, aligned_length) == -1) {
		printf("Memory unmap failed.\n");
		return 1;
	}

	return 0;
}

int write_operation(void *buf)
{
	off_t	aligned_address, i;
	int	page_size, fd;
	void 	*map_base, *temp;

	/* calculate page-aligned values for mmap */
	page_size = getpagesize();

	if (!(params.address == (params.address & ~(page_size - 1)))) {
		aligned_address = params.address & ~(page_size - 1);
	} else {
		aligned_address = params.address;
	}

	if ((fd = open(params.path, O_RDWR | O_SYNC)) == -1) {
		printf("%s could not be opened.\n", params.path);
		exit(1);
	} 

	map_base = mmap(0, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, aligned_address);

	if (map_base == (void *) -1) {
		perror("Memory map failed");
		return 1;
	} 

	temp = map_base + (params.address - aligned_address);

	/* if not ASSIGN, shadow copy */
	if (params.write_operation != ASSIGN) {
		for (i = 0; i < params.length; i++) {
			switch (params.data_size) {
			case BYTE:
				(((unsigned char *)buf)[i]) = (((unsigned char *)temp)[i]);
				break;
			case WORD:
				(((unsigned short *)buf)[i]) = (((unsigned short *)temp)[i]);
				break;
			case DWORD:
				(((unsigned int *)buf)[i]) = (((unsigned int *)temp)[i]);
				break;
			}
		}
	}

	switch (params.data_size) {
	case BYTE:
		switch (params.write_operation) {
		case ASSIGN:
			*((volatile unsigned char *) buf) = (unsigned char) params.value;
			break;
		case XOR:
			*((volatile unsigned char *) buf) ^= (volatile unsigned char) params.value;
			break;
		case OR:
			*((volatile unsigned char *) buf) |= (volatile unsigned char) params.value;
			break;
		case AND:
			*((volatile unsigned char *) buf) &= (volatile unsigned char) params.value;
			break;
		}
		break;
	case WORD:
		switch (params.write_operation) {
		case ASSIGN:
			*((volatile unsigned short *) buf) = (unsigned short) params.value;
			break;
		case XOR:
			*((volatile unsigned short *) buf) ^= (volatile unsigned short) params.value;
			break;
		case OR:
			*((volatile unsigned short *) buf) |= (volatile unsigned short) params.value;
			break;
		case AND:
			*((volatile unsigned short *) buf) &= (volatile unsigned short) params.value;
			break;
		}
		break;
	case DWORD:
		switch (params.write_operation) {
		case ASSIGN:
			*((volatile uint32_t *) buf) = (uint32_t) params.value;
			break;
		case XOR:
			*((volatile unsigned long *) buf) ^= (volatile unsigned long) params.value;
			break;
		case OR:
			*((volatile unsigned long *) buf) |= (volatile unsigned long) params.value;
			break;
		case AND:
			*((volatile uint32_t *) buf) &= (volatile uint32_t) params.value;
			break;
		}
		break;
	}

	for (i = 0; i < params.length; i++) {
		switch (params.data_size) {
		case BYTE:
			(((unsigned char *)temp)[i]) = (((unsigned char *)buf)[i]);
			break;
		case WORD:
			(((unsigned short *)temp)[i]) = (((unsigned short *)buf)[i]);
			break;
		case DWORD:
			(((unsigned int *)temp)[i]) = (((unsigned int *)buf)[i]);
			break;
		}
	}

	if (munmap(map_base, page_size) == -1) {
		printf("Memory unmap failed.\n");
		return 1;
	}

	close(fd);

	return 0;
}

int alignment_check() {
	unsigned long alignment = 0;

	switch (params.data_size) {
	case BYTE:
		break;
	case WORD:
		alignment = params.address & (WORD_SIZE - 1);
		break;
	case DWORD:
		alignment = params.address & (DWORD_SIZE - 1);
		break;
	}

	if (alignment)
		printf("ERROR: requested memory access is not aligned to data access size.\n");
	
	return alignment;
}
	

void hexdump(void *p, size_t len, off_t address, enum data_size_type data_size, char *prefix)
{
	unsigned int	i, x, n, m, mod, bytes;
	unsigned char	c, *data;

	if (!p) return;

	data = p;

	switch (data_size) {
	case BYTE:
		mod = 16;
		bytes = 1;
		break;
	case WORD:
		mod = 8;
		bytes = 2;
		break;
	case DWORD:
		mod = 4;
		bytes = 4;
		break;
	default:
		return;
	}

	i = 0;

	while (i < len) {
		if (!(i % mod))
			printf("%s%.8jx: ", prefix, (i * bytes) + address);
		switch (data_size) {
		case BYTE:
			printf("%.2x ", ((unsigned char *)data)[i++]);
			break;
		case WORD:
			printf("%.4x ", ((unsigned short *)data)[i++]);
			break;
		case DWORD:
			printf("%.8x ", ((unsigned int *)data)[i++]);
			break;
		}
		if (!(i % (mod/2)))
			printf(" ");
		/* char dump disable for now */
		if ((!(i % mod)) || (i == len)) {
			if (i % mod) {
				if ((i % mod) < (mod / 2))
					printf(" ");
				for (n = (i % mod); n < mod; n++)
					for (x = 0; x < (bytes * 2) + 1; x++)
						printf(" ");
				n = (i - (i % mod)) * bytes;
				m = (i % mod) * bytes + n;
				printf(" ");
			} else {
				n = (i - mod) * bytes;
				m = 16 + n;
			}
			printf("|");
			for (; n < m; n++) {
				c = data[n];
				if ((c < 32) || (c > 126))
					c = '.';
				printf("%c", c);
			}
			if (i % mod)
				for (x = (i % mod) * bytes; x < mod * bytes; x++)
					printf(" ");
			printf("|\n");
		}
	}
}

void display_params() {
	printf("/------------- params --------------\\\n");
	printf("| %-16s %16s |\n", "path", params.path);
	printf("| %-16s ", "operation");
	switch (params.operation) {
	case NONE:
		printf("%16s |\n", "none");
		break;
	case READ:
		printf("%16s |\n", "read");
		break;
	case WRITE:
		switch (params.write_operation) {
		case ASSIGN:
			printf("%16s |\n", "write");
			break;
		case XOR:
			printf("%16s |\n", "write (xor)");
			break;
		case OR:
			printf("%16s |\n", "write (or)");
			break;
		case AND:
			printf("%16s |\n", "write (and)");
			break;
		}
		break;
	}
	printf("| %-16s %16jx |\n", "address", params.address);
	printf("| %-16s %16jx |\n", "length", params.length);
	printf("| %-16s %16jx |\n", "iterations", params.iterations);
	printf("| %-16s ", "data size");
	switch (params.data_size) {
	case BYTE:
		printf("%16s |\n", "byte");
		break;
	case WORD:
		printf("%16s |\n", "word");
		break;
	case DWORD:
		printf("%16s |\n", "dword");
		break;
	}
	printf("| %-16s %16lx |\n", "data value", params.value);
	printf("\\-----------------------------------/\n");
}

int parse_params(int argc, char **argv) {
	char	*s, *t, *endptr, *data_size_st, *value_st;

	/* defaults */
	params.address = -1;
	params.length = 0x10;
	params.value = 0x0;
	params.operation = NONE;	
	params.iterations = 1;

	s = getenv("PMAT_ITERATIONS");
	if (s)
		params.iterations = atoll(s);

	else
		params.iterations = 1;

	errno = 0;
	value_st = s = NULL;

	if (argc < 2) {
		return 1;
	} else if (argc == 2) { /* 2 arguments could indicate read, write, or modify */
		if ((t = strstr(argv[1], "^=")) != NULL) {
			params.operation = WRITE;
			params.write_operation = XOR;
			s = strtok(argv[1], "^=");
			value_st = t + 2;
		} else if ((t = strstr(argv[1], "|=")) != NULL) {
			params.operation = WRITE; 
			params.write_operation = OR;
			s = strtok(argv[1], "|=");
			value_st = t + 2;
		} else if ((t = strstr(argv[1], "&=")) != NULL) {
			params.operation = WRITE;
			params.write_operation = AND;
			s = strtok(argv[1], "&=");
			value_st = t + 2;
		} else if ((t = strstr(argv[1], "=")) != NULL) {
			params.operation = WRITE;
			params.write_operation = ASSIGN;
			s = strtok(argv[1], "=");
			value_st = t + 1;
		} else {
			params.operation = READ;
			s = argv[1];
		}
		params.address = strtoul(strtok(s, "."), &endptr, 16);
		if ((errno) || (endptr == s))
			return 1;
		data_size_st = strtok(NULL, "");
		if (NULL != value_st) {
			params.value = strtoul(value_st, &endptr, 16);
			if ((errno) || (endptr == s))
				return 1;
		}
	} else {  /* 3 arguments is a read op */
		params.operation = READ;
		params.address = strtoul(argv[1], &endptr, 16);
		if (errno)
			return 1;
		s = strtok(argv[2], ".");
		params.length = strtoul(s, &endptr, 16);
		if ((errno) || (endptr == s))
			return 1;
		data_size_st = strtok(NULL, "");
	}

	if (NULL == data_size_st) {
		params.data_size = BYTE; 
	} else switch (data_size_st[0]) {
		case 'b':
		case 'B':
			params.data_size = BYTE;
			break;
		case 'd':
		case 'D':
			params.data_size = DWORD;
			break;
		case 'w':
		case 'W':
			params.data_size = WORD;
			break;
		default:
			return 1;
			break;
	}

	params.path = getenv("PMAT_DEV");
	if (!params.path)
		params.path = strdup("/dev/mem");
	
	switch (params.data_size) {
	case BYTE:
		params.length_in_bytes = params.length * BYTE_SIZE;
		break;
	case WORD:
		params.length_in_bytes = params.length * WORD_SIZE;
		break;
	case DWORD:
		params.length_in_bytes = params.length * DWORD_SIZE;
		break;
	}

	return 0;
}

void usage(char *progname)
{
	printf(
		"\n"
		"Physical Memory Analysis Tool v%s\n"
		"-----------------------------------------------\n"
		"\n"
		"NOTICE: this tool is for development purposes only!\n"
		"\n"
		"READ MEMORY\n"
		"-----------\n"
		"Usage:\t\t%s address[.size]\n"
		"      \t\t%s address length[.size]\n"
		"\n"
		"default length is 0x10\n"
		"\n"
		"WRITE MEMORY\n"
		"------------\n"
		"Usage:\t\t%s address[.size](OPERATION)value\n"
		"\n"
		"size is one of the following:\n"
		"\tb\tbyte (default)\n"
		"\tw\tword\n"
		"\td\tdword\n"
		"\n"
		"operation is one of the following:\n"
		"\t=\tassignment\n"
		"\t|=\tor with current value\n"
		"\t&=\tand with current value\n"
		"\t^=\txor with current value\n"
		"\n"
		"\t(note that certain shells require escaping the |, &, and ^ characters)\n"
		"\n"
		"all value must be expressed in hexadecimal\n"
		"use PMAT_DEV environment variable to override default use of /dev/mem\n"
		"use PMAT_ITERATIONS environment variable to repeat commands (0 = infinite)\n"
		"define PMAT_DEBUG environment variable to enable verbose printing\n"
		"\n",
		PMAT_VERSION, basename(progname), basename(progname), basename(progname));
}

