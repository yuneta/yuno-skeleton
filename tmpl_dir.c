/****************************************************************************
 *          MAKE_SKELETON.C
 *          Make a skeleton
 *
 *          Copyright (c) 2015 Niyamaka.
 *          All Rights Reserved.
 *
 *
 *  sudo apt-get install libpcre3-dev
 *
 ****************************************************************************/
#define _POSIX_SOURCE
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <pcre.h>
#include <errno.h>
#include <jansson.h>
#include <00_replace_string.h>
#include "tmpl_dir.h"

/***************************************************************************
 *      Constants
 ***************************************************************************/

/***************************************************************************
 *  Busca en str las {{clave}} y sustituye la clave con el valor
 *  de dicha clave en el dict jn_values
 ***************************************************************************/
int render_string(char *rendered_str, int rendered_str_size, char *str, json_t *jn_values)
{
    pcre *re;
    const char *error;
    int erroffset;
    int ovector[100];

    re = pcre_compile(
             "(\\{\\{.+?\\}\\})",   /* the pattern */
             0,                     /* default options */
             &error,                /* for error message */
             &erroffset,            /* for error offset */
             0                      /* use default character tables */
    );
    if(!re) {
        fprintf(stderr, "pcre_compile failed (offset: %d), %s\n", erroffset, error);
        exit(-1);
    }

    snprintf(rendered_str, rendered_str_size, "%s", str);

    int rc;
    unsigned int offset = 0;
    unsigned int len = strlen(str);
    while (offset < len && (rc = pcre_exec(re, 0, str, len, offset, 0, ovector, sizeof(ovector))) >= 0)
    {
        for(int i = 0; i < rc; ++i)
        {
            int macro_len = ovector[2*i+1] - ovector[2*i];
            //printf("%2d: %.*s\n", i, macro_len, str + ovector[2*i]);
            char macro[256]; // enough of course
            char rendered[256];
            snprintf(macro, sizeof(macro), "%.*s", macro_len, str + ovector[2*i]);
            char key[256];
            snprintf(key, sizeof(key), "%.*s", macro_len-4, str + ovector[2*i] + 2);

            const char *value = json_string_value(json_object_get(jn_values, key));
            if(!value)
                value = "";
            snprintf(rendered, sizeof(rendered), "%s", value);

            char * new_value = replace_string(rendered_str, macro, rendered);
            snprintf(rendered_str, rendered_str_size, "%s", new_value);
            free(new_value);
        }
        offset = ovector[1];
    }
    free(re);

    return 0;
}

/***************************************************************************
 *  Busca en str las +clave+ y sustituye la clave con el valor
 *  de dicha clave en el dict jn_values
 *  Busca tb "_tmpl$" y elimínalo.
 ***************************************************************************/
int render_filename(char *rendered_str, int rendered_str_size, char *str, json_t *jn_values)
{
    pcre *re;
    const char *error;
    int erroffset;
    int ovector[100];

    re = pcre_compile(
             "(\\+.+?\\+)", /* the pattern */
             0,             /* default options */
             &error,        /* for error message */
             &erroffset,    /* for error offset */
             0              /* use default character tables */
    );
    if(!re) {
        fprintf(stderr, "pcre_compile failed (offset: %d), %s\n", erroffset, error);
        exit(-1);
    }

    snprintf(rendered_str, rendered_str_size, "%s", str);

    int rc;
    unsigned int offset = 0;
    unsigned int len = strlen(str);
    while (offset < len && (rc = pcre_exec(re, 0, str, len, offset, 0, ovector, sizeof(ovector))) >= 0)
    {
        for(int i = 0; i < rc; ++i)
        {
            int macro_len = ovector[2*i+1] - ovector[2*i];
            //printf("%2d: %.*s\n", i, macro_len, str + ovector[2*i]);
            char macro[256]; // enough of course
            char rendered[256];
            snprintf(macro, sizeof(macro), "%.*s", macro_len, str + ovector[2*i]);
            char key[256];
            snprintf(key, sizeof(key), "%.*s", macro_len-2, str + ovector[2*i] + 1);

            const char *value = json_string_value(json_object_get(jn_values, key));
            snprintf(rendered, sizeof(rendered), "%s", value?value:"");

            char * new_value = replace_string(rendered_str, macro, rendered);
            snprintf(rendered_str, rendered_str_size, "%s", new_value);
            free(new_value);
        }
        offset = ovector[1];
    }
    free(re);

    len = strlen(rendered_str);
    if(len > 5 && strcmp(rendered_str+len-5, "_tmpl")==0) {
        *(rendered_str+len-5) = 0;
    }

    return 0;
}

/***************************************************************************
 *  Lee el fichero src_path línea a línea, render la línea,
 *  y sálvala en dst_path
 ***************************************************************************/
int render_file(char *dst_path, char *src_path, json_t *jn_values)
{
    FILE *f = fopen(src_path, "r");
    if(!f) {
        fprintf(stderr, "ERROR Cannot open file %s\n", src_path);
        exit(-1);
    }

    if(access(dst_path, 0)==0) {
        fprintf(stderr, "ERROR File %s ALREADY EXISTS\n", dst_path);
        exit(-1);
    }

    FILE *fout = fopen(dst_path, "w");
    if(!fout) {
        printf("ERROR: cannot create '%s', %s\n", dst_path, strerror(errno));
        exit(-1);
    }
    printf("Creating filename: %s\n", dst_path);
    char line[4*1024];
    char rendered[4*1024];
    while(fgets(line, sizeof(line), f)) {
        render_string(rendered, sizeof(rendered), line, jn_values);
        fputs(rendered, fout);
    }
    fclose(f);
    fclose(fout);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
int is_regular_file(const char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

/***************************************************************************
 *
 ***************************************************************************/
int is_directory(const char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

/***************************************************************************
 *  Copy recursively the directory src to dst directory,
 *  rendering {{ }} of names of files and directories, and the content of the files,
 *  substituting by the value of jn_values
 ***************************************************************************/
int copy_dir(const char *dst, const char *src, json_t *jn_values)
{
    /*
     *  src must be a directory
     *  Get file of src directory, render the filename and his content, and copy to dst directory.
     *  When found a directory, render the name and make a new directory in dst directory
     *  and call recursively with these two new directories.
     */

    printf("Copying '%s' in '%s'\n", src, dst);

    DIR *src_dir;
    DIR *dst_dir;
    struct dirent *entry;

    if (!(src_dir = opendir(src))) {
        printf("ERROR: cannot opendir source ('%s'), %s\n", src, strerror(errno));
        exit(-1);
    }
    if (!(entry = readdir(src_dir))) {
        printf("ERROR: cannot readdir source ('%s'), %s\n", src, strerror(errno));
        exit(-1);
    }

    if (!(dst_dir = opendir(dst))) {
        /*
         *  Create destination if not exist
         */
        printf("Creating directory: %s\n", dst);
        mkdir(dst, S_ISUID|S_ISGID | S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
        if (!(dst_dir = opendir(dst))) {
            printf("ERROR: cannot opendir destination ('%s'), %s\n", dst, strerror(errno));
            exit(-1);
        }
    }

    int len;
    char dst_path[1024];
    char src_path[1024];
    char rendered_str[80];
    do {
        len = snprintf(src_path, sizeof(src_path)-1, "%s/%s", src, entry->d_name);
        src_path[len] = 0;

        render_filename(rendered_str, sizeof(rendered_str), entry->d_name, jn_values);
        len = snprintf(dst_path, sizeof(dst_path)-1, "%s/%s", dst, rendered_str);
        dst_path[len] = 0;

        if (is_directory(src_path)) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            copy_dir(dst_path, src_path, jn_values);
        } if (is_regular_file(src_path)) {
            render_file(dst_path, src_path, jn_values);
        }

    } while ((entry = readdir(src_dir)));

    closedir(src_dir);
    closedir(dst_dir);

    return 0;
}
