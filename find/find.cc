// Description: A simplified version of the find command that supports the -name, -type, -size, -ls, -depth, -mtime, -mmin, and -newer options.

// Usage: ./find <directory> [options]
// Options:
// -name <name>   Find files with the specified name
// -type <type>   Find files of the specified type (f, d, l)
// -size <size>   Find files with the specified size (+/-/=)<size>[c/k]
// -ls            List file information
// -depth <N>     Limit recursion depth to N levels
// -mtime <time>  Find files modified <time> days ago
// -mmin <time>   Find files modified <time> minutes ago
// -newer <file>  Find files newer than <file>
// -iname <name>  Case-insensitive version of -name


// Example: ./find /etc -name passwd -type f -size +1k

// Author: Daniel Safavi
// Date: 12/6/2024

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <libgen.h>
#include <fnmatch.h>
#include <ctype.h>

// Filter options structure
typedef struct {
    char* name;         // -name filter
    char* iname;        // -iname filter (case insensitive)
    char* type;         // -type filter
    char* size;         // -size filter
    char* mtime;        // -mtime filter
    char* mmin;         // -mmin filter
    char* newer;        // -newer filter
    int ls_flag;        // -ls flag
    int depth;          // -depth N option (-1 --> no depth limit)
    int current_depth;  // Current recursion depth
} FilterOptions;

// Function prototypes
static void process_directory(const char* dir_path, FilterOptions* filters);
static int check_filters(const char* filepath, const char* filename, FilterOptions* filters);
static int check_name_filter(const char* filename, const char* filter_name, int case_sensitive);
static int check_type_filter(const char* filepath, const char* filter_type);
static int check_size_filter(const char* filepath, const char* size_spec);
static int check_time_filter(const char* filepath, const char* timespec, int use_minutes);
static int check_newer_filter(const char* filepath, const char* reference_file);
static long long parse_size(const char* size_spec);
static void print_file_info(const char* filepath);
static time_t get_file_mtime(const char* filepath);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Not enough arguments\n");
        exit(1);
    }

    // Initialize filter options
    FilterOptions filters = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, -1, 0};

    // Parse command line arguments
    int i = 2;  // Start after program name and directory
    while (i < argc) {
        if (strcmp(argv[i], "-name") == 0 ||
            strcmp(argv[i], "-type") == 0 ||
            strcmp(argv[i], "-size") == 0 ||
            strcmp(argv[i], "-iname") == 0 ||
            strcmp(argv[i], "-mtime") == 0 ||
            strcmp(argv[i], "-mmin") == 0 ||
            strcmp(argv[i], "-newer") == 0 ||
            strcmp(argv[i], "-depth") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s requires argument\n", argv[i]);
                exit(1);
            }
            if (strcmp(argv[i], "-name") == 0)
                filters.name = argv[i + 1];
            else if (strcmp(argv[i], "-iname") == 0)
                filters.iname = argv[i + 1];
            else if (strcmp(argv[i], "-type") == 0)
                filters.type = argv[i + 1];
            else if (strcmp(argv[i], "-size") == 0)
                filters.size = argv[i + 1];
            else if (strcmp(argv[i], "-mtime") == 0)
                filters.mtime = argv[i + 1];
            else if (strcmp(argv[i], "-mmin") == 0)
                filters.mmin = argv[i + 1];
            else if (strcmp(argv[i], "-newer") == 0)
                filters.newer = argv[i + 1];
            else if (strcmp(argv[i], "-depth") == 0)
                filters.depth = atoi(argv[i + 1]);
            i += 2;
        }
        else if (strcmp(argv[i], "-ls") == 0) {
            filters.ls_flag = 1;
            i++;
        }
        else {
            fprintf(stderr, "Unknown filter: %s\n", argv[i]);
            exit(1);
        }
    }

    // Check if base directory matches filters
    struct stat statbuf;
    if (lstat(argv[1], &statbuf) == 0) {
        if ((!filters.type || strcmp(filters.type, "d") == 0) && 
            check_filters(argv[1], basename(argv[1]), &filters)) {
            printf("%s\n", argv[1]);
        }
    }

    // Process directory
    process_directory(argv[1], &filters);

    exit(0);
}

static void process_directory(const char* dir_path, FilterOptions* filters) {
    DIR *dp;
    struct dirent *entry;
    char filepath[PATH_MAX];

    // Check depth limit
    if (filters->depth != -1 && filters->current_depth > filters->depth) {
        return;
    }

    if ((dp = opendir(dir_path)) == NULL) {
        perror(dir_path);
        return;
    }

    while ((entry = readdir(dp)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, entry->d_name);

        if (check_filters(filepath, entry->d_name, filters)) {
            if (filters->ls_flag)
                print_file_info(filepath);
            else
                printf("%s\n", filepath);
        }

        struct stat statbuf;
        if (lstat(filepath, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
            filters->current_depth++;
            process_directory(filepath, filters);
            filters->current_depth--;
        }
    }

    closedir(dp);
}

static int check_filters(const char* filepath, const char* filename, FilterOptions* filters) {
    // Check name filters (both case-sensitive and insensitive)
    if (filters->name && !check_name_filter(filename, filters->name, 1))
        return 0;

    if (filters->iname && !check_name_filter(filename, filters->iname, 0))
        return 0;

    if (filters->type && !check_type_filter(filepath, filters->type))
        return 0;

    if (filters->size && !check_size_filter(filepath, filters->size))
        return 0;

    if (filters->mtime && !check_time_filter(filepath, filters->mtime, 0))
        return 0;

    if (filters->mmin && !check_time_filter(filepath, filters->mmin, 1))
        return 0;

    if (filters->newer && !check_newer_filter(filepath, filters->newer))
        return 0;

    return 1;
}

static int check_name_filter(const char* filename, const char* pattern, int case_sensitive) {
    if (case_sensitive)
        return fnmatch(pattern, filename, 0) == 0;
    else
        return fnmatch(pattern, filename, FNM_CASEFOLD) == 0;
}

static int check_type_filter(const char* filepath, const char* filter_type) {
    struct stat statbuf;

    if (lstat(filepath, &statbuf) != 0)
        return 0;

    switch (filter_type[0]) {
        case 'f':
            return S_ISREG(statbuf.st_mode);
        case 'd':
            return S_ISDIR(statbuf.st_mode);
        case 'l':
            return S_ISLNK(statbuf.st_mode);
        default:
            return 0;
    }
}

static int check_size_filter(const char* filepath, const char* size_spec) {
    struct stat statbuf;

    if (lstat(filepath, &statbuf) != 0)
        return 0;

    long long target_size = parse_size(size_spec);
    long long file_size = statbuf.st_size;

    // Handle size units
    char unit = size_spec[strlen(size_spec) - 1];
    if (unit == 'c') {
        // Already in bytes
    } else if (unit == 'k') {
        target_size *= 1024;
    } else if (unit == 'M') {
        target_size *= 1024 * 1024;
    } else {
        // Default to 512-byte blocks
        target_size *= 512;
    }

    // Check size based on comparison operator
    if (size_spec[0] == '+')
        return file_size > target_size;
    else if (size_spec[0] == '-')
        return file_size < target_size;
    else
        return file_size == target_size;
}

static int check_time_filter(const char* filepath, const char* timespec, int use_minutes) {
    struct stat statbuf;
    if (lstat(filepath, &statbuf) != 0)
        return 0;

    time_t now = time(NULL);
    time_t file_time = statbuf.st_mtime;
    long long target_time = parse_size(timespec);  // Reuse parse_size for number parsing

    // Convert to appropriate units
    double diff = difftime(now, file_time);
    if (use_minutes)
        diff /= 60;  // Convert to minutes
    else
        diff /= (60 * 60 * 24);  // Convert to days

    if (timespec[0] == '+')
        return diff > target_time;
    else if (timespec[0] == '-')
        return diff < target_time;
    else
        return (long long)diff == target_time;
}

static int check_newer_filter(const char* filepath, const char* reference_file) {
    time_t file_time = get_file_mtime(filepath);
    time_t ref_time = get_file_mtime(reference_file);

    if (file_time == (time_t)-1 || ref_time == (time_t)-1)
        return 0;

    return file_time > ref_time;
}

static time_t get_file_mtime(const char* filepath) {
    struct stat statbuf;
    if (lstat(filepath, &statbuf) != 0)
        return (time_t)-1;
    return statbuf.st_mtime;
}

static long long parse_size(const char* size_spec) {
    char* endptr;
    const char* numstart = size_spec;

    // Skip +/- if present
    if (size_spec[0] == '+' || size_spec[0] == '-')
        numstart++;

    return strtoll(numstart, &endptr, 10);
}

static void print_file_info(const char* filepath) {
    struct stat statbuf;

    if (lstat(filepath, &statbuf) != 0) {
        perror(filepath);
        return;
    }

    printf("%lu ", (unsigned long)statbuf.st_ino);
    printf("%lld ", (long long)((statbuf.st_blocks + 1) / 2));

    printf("%c%c%c%c%c%c%c%c%c%c ",
        S_ISDIR(statbuf.st_mode) ? 'd' : '-',
        statbuf.st_mode & S_IRUSR ? 'r' : '-',
        statbuf.st_mode & S_IWUSR ? 'w' : '-',
        statbuf.st_mode & S_IXUSR ? 'x' : '-',
        statbuf.st_mode & S_IRGRP ? 'r' : '-',
        statbuf.st_mode & S_IWGRP ? 'w' : '-',
        statbuf.st_mode & S_IXGRP ? 'x' : '-',
        statbuf.st_mode & S_IROTH ? 'r' : '-',
        statbuf.st_mode & S_IWOTH ? 'w' : '-',
        statbuf.st_mode & S_IXOTH ? 'x' : '-');

    printf("%lu ", (unsigned long)statbuf.st_nlink);
    printf("%d %d ", statbuf.st_uid, statbuf.st_gid);
    printf("%lld ", (long long)statbuf.st_size);

    char timebuf[20];
    strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", localtime(&statbuf.st_mtime));
    printf("%s ", timebuf);

    printf("%s\n", filepath);
}

