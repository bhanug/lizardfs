/*
   Copyright 2005-2010 Jakub Kruszona-Zawadzki, Gemius SA, 2013-2014 EditShare,
   2013-2016 Skytechnology sp. z o.o..

   This file was part of MooseFS and is part of LizardFS.

   LizardFS is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, version 3.

   LizardFS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with LizardFS. If not, see <http://www.gnu.org/licenses/>.
 */

#include "common/platform.h"

#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <tools/tools_common_functions.h>
#include "common/human_readable_format.h"
#include "common/mfserr.h"

uint8_t humode = 0;

const char *eattrtab[EATTR_BITS] = {EATTR_STRINGS};
const char *eattrdesc[EATTR_BITS] = {EATTR_DESCRIPTIONS};

void print_number(const char *prefix, const char *suffix, uint64_t number, uint8_t mode32,
				  uint8_t bytesflag, uint8_t dflag) {
	if (prefix) {
		printf("%s", prefix);
	}
	if (dflag) {
		if (humode > 0) {
			if (bytesflag) {
				if (humode == 1 || humode == 3) {
					printf("%5sB", convertToIec(number).data());
				} else {
					printf("%4sB", convertToSi(number).data());
				}
			} else {
				if (humode == 1 || humode == 3) {
					printf(" %5s", convertToIec(number).data());
				} else {
					printf(" %4s", convertToSi(number).data());
				}
			}
			if (humode > 2) {
				if (mode32) {
					printf(" (%10" PRIu32 ")", (uint32_t)number);
				} else {
					printf(" (%20" PRIu64 ")", number);
				}
			}
		} else {
			if (mode32) {
				printf("%10" PRIu32, (uint32_t)number);
			} else {
				printf("%20" PRIu64, number);
			}
		}
	} else {
		switch (humode) {
		case 0:
			if (mode32) {
				printf("         -");
			} else {
				printf("                   -");
			}
			break;
		case 1:
			printf("     -");
			break;
		case 2:
			printf("    -");
			break;
		case 3:
			if (mode32) {
				printf("                  -");
			} else {
				printf("                            -");
			}
			break;
		case 4:
			if (mode32) {
				printf("                 -");
			} else {
				printf("                           -");
			}
			break;
		}
	}
	if (suffix) {
		printf("%s", suffix);
	}
}

int my_get_number(const char *str, uint64_t *ret, double max, uint8_t bytesflag) {
	uint64_t val, frac, fracdiv;
	double drval, mult;
	int f;
	val = 0;
	frac = 0;
	fracdiv = 1;
	f = 0;
	while (*str >= '0' && *str <= '9') {
		f = 1;
		val *= 10;
		val += (*str - '0');
		str++;
	}
	if (*str == '.') {  // accept ".5" (without 0)
		str++;
		while (*str >= '0' && *str <= '9') {
			fracdiv *= 10;
			frac *= 10;
			frac += (*str - '0');
			str++;
		}
		if (fracdiv == 1) {  // if there was '.' expect number afterwards
			return -1;
		}
	} else if (f == 0) {  // but not empty string
		return -1;
	}
	if (str[0] == '\0' || (bytesflag && str[0] == 'B' && str[1] == '\0')) {
		mult = 1.0;
	} else if (str[0] != '\0' &&
	           (str[1] == '\0' || (bytesflag && str[1] == 'B' && str[2] == '\0'))) {
		switch (str[0]) {
		case 'k':
			mult = 1e3;
			break;
		case 'M':
			mult = 1e6;
			break;
		case 'G':
			mult = 1e9;
			break;
		case 'T':
			mult = 1e12;
			break;
		case 'P':
			mult = 1e15;
			break;
		case 'E':
			mult = 1e18;
			break;
		default:
			return -1;
		}
	} else if (str[0] != '\0' && str[1] == 'i' &&
	           (str[2] == '\0' || (bytesflag && str[2] == 'B' && str[3] == '\0'))) {
		switch (str[0]) {
		case 'K':
			mult = 1024.0;
			break;
		case 'M':
			mult = 1048576.0;
			break;
		case 'G':
			mult = 1073741824.0;
			break;
		case 'T':
			mult = 1099511627776.0;
			break;
		case 'P':
			mult = 1125899906842624.0;
			break;
		case 'E':
			mult = 1152921504606846976.0;
			break;
		default:
			return -1;
		}
	} else {
		return -1;
	}
	drval = round(((double)frac / (double)fracdiv + (double)val) * mult);
	if (drval > max) {
		return -2;
	} else {
		*ret = drval;
	}
	return 1;
}

int bsd_basename(const char *path, char *bname) {
	const char *endp, *startp;

	/* Empty or NULL string gets treated as "." */
	if (path == NULL || *path == '\0') {
		(void)strcpy(bname, ".");
		return 0;
	}

	/* Strip trailing slashes */
	endp = path + strlen(path) - 1;
	while (endp > path && *endp == '/') {
		endp--;
	}

	/* All slashes becomes "/" */
	if (endp == path && *endp == '/') {
		(void)strcpy(bname, "/");
		return 0;
	}

	/* Find the start of the base */
	startp = endp;
	while (startp > path && *(startp - 1) != '/') {
		startp--;
	}

	if (endp - startp + 2 > PATH_MAX) {
		return -1;
	}

	(void)strncpy(bname, startp, endp - startp + 1);
	bname[endp - startp + 1] = '\0';
	return 0;
}

int bsd_dirname(const char *path, char *bname) {
	const char *endp;

	/* Empty or NULL string gets treated as "." */
	if (path == NULL || *path == '\0') {
		(void)strcpy(bname, ".");
		return 0;
	}

	/* Strip trailing slashes */
	endp = path + strlen(path) - 1;
	while (endp > path && *endp == '/') {
		endp--;
	}

	/* Find the start of the dir */
	while (endp > path && *endp != '/') {
		endp--;
	}

	/* Either the dir is "/" or there are no slashes */
	if (endp == path) {
		(void)strcpy(bname, *endp == '/' ? "/" : ".");
		return 0;
	} else {
		do {
			endp--;
		} while (endp > path && *endp == '/');
	}

	if (endp - path + 2 > PATH_MAX) {
		return -1;
	}
	(void)strncpy(bname, path, endp - path + 1);
	bname[endp - path + 1] = '\0';
	return 0;
}

void dirname_inplace(char *path) {
	char *endp;

	if (path == NULL) {
		return;
	}
	if (path[0] == '\0') {
		path[0] = '.';
		path[1] = '\0';
		return;
	}

	/* Strip trailing slashes */
	endp = path + strlen(path) - 1;
	while (endp > path && *endp == '/') {
		endp--;
	}

	/* Find the start of the dir */
	while (endp > path && *endp != '/') {
		endp--;
	}

	if (endp == path) {
		if (path[0] == '/') {
			path[1] = '\0';
		} else {
			path[0] = '.';
			path[1] = '\0';
		}
		return;
	} else {
		*endp = '\0';
	}
}

void usage(int f) {
	switch (f) {
	case MFSGETGOAL:
		fprintf(stderr,
		        "get objects goal (desired number of copies)\n\nusage: mfsgetgoal [-nhHr] name "
		        "[name ...]\n");
		print_numberformat_options();
		print_recursive_option();
		break;
	case MFSSETGOAL:
		fprintf(stderr,
		        "set objects goal (desired number of copies)\n\nusage: mfssetgoal <operation> name "
		        "[name ...]\n");
		print_numberformat_options();
		print_recursive_option();
		fprintf(stderr, "<operation> is one of:\n");
		fprintf(stderr, " GOAL - set goal to given goal name\n");
		break;
	case MFSGETTRASHTIME:
		fprintf(stderr,
		        "get objects trashtime (how many seconds file should be left in trash)\n\nusage: "
		        "mfsgettrashtime [-nhHr] name [name ...]\n");
		print_numberformat_options();
		print_recursive_option();
		break;
	case MFSSETTRASHTIME:
		fprintf(stderr,
		        "set objects trashtime (how many seconds file should be left in trash)\n\nusage: "
		        "mfssettrashtime [-nhHr] SECONDS[-|+] name [name ...]\n");
		print_numberformat_options();
		print_recursive_option();
		fprintf(stderr, " SECONDS+ - increase trashtime to given value\n");
		fprintf(stderr, " SECONDS- - decrease trashtime to given value\n");
		fprintf(stderr, " SECONDS - just set trashtime to given value\n");
		break;
	case MFSCHECKFILE:
		fprintf(stderr, "check files\n\nusage: mfscheckfile [-nhH] name [name ...]\n");
		break;
	case MFSFILEINFO:
		fprintf(stderr,
		        "show files info (shows detailed info of each file chunk)\n\nusage: "
		        "mfsfileinfo name [name ...]\n");
		break;
	case MFSAPPENDCHUNKS:
		fprintf(stderr,
		        "append file chunks to another file. If destination file doesn't exist then it's "
		        "created as empty file and then chunks are appended\n\nusage: "
		        "mfsappendchunks dstfile name [name ...]\n");
		break;
	case MFSDIRINFO:
		fprintf(stderr, "show directories stats\n\nusage: mfsdirinfo [-nhH] name [name ...]\n");
		print_numberformat_options();
		fprintf(
		    stderr,
		    "\nMeaning of some not obvious output data:\n 'length' is just sum of files lengths\n "
		    "'size' is sum of chunks lengths\n 'realsize' is estimated hdd usage "
		    "(usually size multiplied by current goal)\n");
		break;
	case MFSFILEREPAIR:
		fprintf(stderr,
		        "repair given file. Use it with caution. It forces file to be readable, so it "
		        "could erase "
		        "(fill with zeros) file when chunkservers are not currently connected.\n\nusage: "
		        "mfsfilerepair [-nhH] name [name ...]\n");
		print_numberformat_options();
		break;
	case MFSMAKESNAPSHOT:
		fprintf(stderr,
		        "make snapshot (lazy copy)\n\nusage: mfsmakesnapshot [-ofl] src [src ...] dst\n");
		fprintf(stderr, "-o,-f - allow to overwrite existing objects\n");
		fprintf(stderr, "-l - wait until snapshot will finish (otherwise there is 60s timeout)\n");
		break;
	case MFSGETEATTR:
		fprintf(stderr,
		        "get objects extra attributes\n\nusage: mfsgeteattr [-nhHr] name [name ...]\n");
		print_numberformat_options();
		print_recursive_option();
		break;
	case MFSSETEATTR:
		fprintf(stderr,
		        "set objects extra attributes\n\nusage: "
		        "mfsseteattr [-nhHr] -f attrname [-f attrname ...] name [name ...]\n");
		print_numberformat_options();
		print_recursive_option();
		fprintf(stderr, " -f attrname - specify attribute to set\n");
		print_extra_attributes();
		break;
	case MFSDELEATTR:
		fprintf(stderr,
		        "delete objects extra attributes\n\nusage: "
		        "mfsdeleattr [-nhHr] -f attrname [-f attrname ...] name [name ...]\n");
		print_numberformat_options();
		print_recursive_option();
		fprintf(stderr, " -f attrname - specify attribute to delete\n");
		print_extra_attributes();
		break;
	case MFSREPQUOTA:
		fprintf(stderr,
		        "summarize quotas for a user/group or all users and groups\n\n"
		        "usage: mfsrepquota [-nhH] (-u <uid>|-g <gid>)+ <mountpoint-root-path>\n"
		        "       mfsrepquota [-nhH] -a <mountpoint-root-path>\n"
		        "       mfsrepquota [-nhH] -d <directory-path>\n");
		print_numberformat_options();
		break;
	case MFSSETQUOTA:
		fprintf(stderr,
		        "set quotas\n\n"
		        "usage: mfssetquota (-u <uid>|-g <gid> |-d) "
		        "<soft-limit-size> <hard-limit-size> "
		        "<soft-limit-inodes> <hard-limit-inodes> <directory-path>\n"
		        " 0 deletes the limit\n");
		break;
	}
	exit(1);
}
