/*
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "u4file.h"
#include "debug.h"

extern int verbose;

/* the possible paths where u4 for DOS can be installed */
static const char * const paths[] = {
    "./",
    "./ultima4/",
    "/usr/lib/u4/ultima4/",
    "/usr/local/lib/u4/ultima4/"
};

/* the possible paths where the u4 music files can be installed */
static const char * const music_paths[] = {
    "./",
    "./ultima4/",
    "../mid/",
    "/usr/lib/u4/music/",
    "/usr/local/lib/u4/music/"
};

/* the possible paths where the u4 config files can be installed */
static const char * const conf_paths[] = {
    "./",
    "../conf/",
    "/usr/lib/u4/",
    "/usr/local/lib/u4/"
};


/**
 * Open a data file from the Ultima 4 for DOS installation.  This
 * function checks the various places where it can be installed, and
 * maps the filenames to uppercase if necessary.  The files are always
 * opened for reading only.
 *
 * Currently, it tries FILENAME, Filename and filename in up to four
 * paths, meaning up to twelve opens per file.  Seems to be ok for
 * performance, but could be getting excessive.
 */
U4FILE *u4fopen(const char *fname) {
    FILE *f = NULL;
    unsigned int i, j;
    char pathname[128];

    for (i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        snprintf(pathname, sizeof(pathname), "%s%s", paths[i], fname);

        if (verbose)
            printf("trying to open %s\n", pathname);

        if ((f = fopen(pathname, "rb")) != NULL)
            break;

        if (islower(pathname[strlen(paths[i])]))
            pathname[strlen(paths[i])] = toupper(pathname[strlen(paths[i])]);

        if (verbose)
            printf("trying to open %s\n", pathname);

        if ((f = fopen(pathname, "rb")) != NULL)
            break;

        for (j = strlen(paths[i]); pathname[j] != '\0'; j++) {
            if (islower(pathname[j]))
                pathname[j] = toupper(pathname[j]);
        }

        if (verbose)
            printf("trying to open %s\n", pathname);

        if ((f = fopen(pathname, "rb")) != NULL)
            break;
    }

    if (verbose && f != NULL)
        printf("%s successfully opened\n", pathname);

    return f;
}

/**
 * Closes a data file from the Ultima 4 for DOS installation.
 */
void u4fclose(U4FILE *f) {
    fclose(f);
}

int u4fseek(U4FILE *f, long offset, int whence) {
    return fseek(f, offset, whence);
}

size_t u4fread(void *ptr, size_t size, size_t nmemb, U4FILE *f) {
    return fread(ptr, size, nmemb, f);
}

int u4fgetc(U4FILE *f) {
    return fgetc(f);
}

int u4fgetshort(U4FILE *f) {
    return u4fgetc(f) | (u4fgetc(f) << 8);
}

int u4fputc(int c, U4FILE *f) {
    return fputc(c, f);
}

/**
 * Returns the length in bytes of a file.
 */
long u4flength(U4FILE *f) {
    long curr, len;

    curr = ftell(f);
    fseek(f, 0L, SEEK_END);
    len = ftell(f);
    fseek(f, curr, SEEK_SET);

    return len;
}

/**
 * Read a series of zero terminated strings from a file.  The strings
 * are read from the given offset, or the current file position if
 * offset is -1.
 */
char **u4read_stringtable(U4FILE *f, long offset, int nstrings) {
    char buffer[384];
    int i, j;
    char **strs = (char **) malloc(nstrings * sizeof(char *));
    if (!strs)
        return NULL;

    ASSERT(offset < u4flength(f), "offset begins beyond end of file");

    if (offset != -1)
        u4fseek(f, offset, SEEK_SET);
    for (i = 0; i < nstrings; i++) {
        for (j = 0; j < sizeof(buffer) - 1; j++) {
            buffer[j] = fgetc(f);
            if (buffer[j] == '\0' &&
                (j < 2 || !(buffer[j - 2] == 10 && buffer[j - 1] == 8))) /* needed to handle weird characters in lb's abyss response */
                break;
        }
        while(j > 0 && !(isprint(buffer[j - 1]) || isspace(buffer[j - 1])))
            j--;
        buffer[j] = '\0';
        strs[i] = strdup(buffer);
    }

    return strs;
}

char *u4find_path(const char *fname, const char * const *pathent, int npathents) {
    FILE *f = NULL;
    unsigned int i;
    char pathname[128];

    for (i = 0; i < npathents; i++) {
        snprintf(pathname, sizeof(pathname), "%s%s", pathent[i], fname);

        if (verbose)
            printf("trying to open %s\n", pathname);

        if ((f = fopen(pathname, "rb")) != NULL)
            break;
    }

    if (verbose && f != NULL)
        printf("%s successfully found\n", pathname);

    if (f) {
        fclose(f);
        return strdup(pathname);
    } else
        return NULL;
}

char *u4find_music(const char *fname) {
    return u4find_path(fname, music_paths, sizeof(music_paths) / sizeof(music_paths[0]));
}

char *u4find_conf(const char *fname) {
    return u4find_path(fname, conf_paths, sizeof(conf_paths) / sizeof(conf_paths[0]));
}
