/*
 * $Id$
 */

#ifndef U4FILE_H
#define U4FILE_H

#include "vc6.h"
#include <string>
#include <vector>

/**
 * An abstract interface for file access.
 */
class U4FILE {
public:
    virtual ~U4FILE() {}

    virtual void close() = 0;
    virtual int seek(long offset, int whence) = 0;
    virtual long tell() = 0;
    virtual size_t read(void *ptr, size_t size, size_t nmemb) = 0;
    virtual int getc() = 0;
    virtual int putc(int c) = 0;
    virtual long length() = 0;

    int getshort();
};

int u4isUpgradeInstalled(void);
U4FILE *u4fopen(const std::string &fname);
U4FILE *u4fopen_stdio(const std::string &fname);
void u4fclose(U4FILE *f);
int u4fseek(U4FILE *f, long offset, int whence);
long u4ftell(U4FILE *f);
size_t u4fread(void *ptr, size_t size, size_t nmemb, U4FILE *f);
int u4fgetc(U4FILE *f);
int u4fgetshort(U4FILE *f);
int u4fputc(int c, U4FILE *f);
long u4flength(U4FILE *f);
std::vector<std::string> u4read_stringtable(U4FILE *f, long offset, int nstrings);
std::string u4find_path(const std::string &fname, const char * const *pathent, unsigned int npathents);
std::string u4find_music(const std::string &fname);
std::string u4find_sound(const std::string &fname);
std::string u4find_conf(const std::string &fname);
std::string u4find_graphics(const std::string &fname);
std::string u4upgrade_translate_filename(const std::string &fname);

extern int u4zipExists;
extern int u4upgradeZipExists;
extern int u4upgradeExists;
extern int u4upgradeInstalled;

#endif
