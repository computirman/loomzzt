/* files.h  -- filesystem routines
 * $Id: files.h,v 1.2 2001/12/12 22:08:02 bitman Exp $
 * Copyright (C) 2000 Ryan Phillips <bitman@scn.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __FILES_H
#define __FILES_H

#include "svector.h"

/* Copy flags */
#define COPY_NOOVERWRITE 0
#define COPY_OVERWRITE   1

/* Copy results */
#define COPY_SUCCESS   0  /* Ok */
#define COPY_EXISTS    1  /* Destination file already exists */
#define COPY_BADSOURCE 2  /* Can't open source file */
#define COPY_BADDEST   3  /* Can't open destination file */

/* General file types */
#define FTYPE_FILE 1
#define FTYPE_DIR  2
#define FTYPE_ALL  3

/* Types of slashes */
#define SLASH_DEFAULT 0
#define SLASH_FORWARD 0
#define SLASH_BACK    1

/* filetosvector() - loads a textfile into a new stringvector */
stringvector filetosvector(char* filename, int wrapwidth, int editwidth);

/* svectortofile() - copies a stringvector into a file. sv is not changed */
void svectortofile(stringvector * sv, char *filename);

/* readdirectorytosvector() - reads a directory listing into an svector */
stringvector readdirectorytosvector(char * dir, char * extension,
																		int filetypes);

/* globtosvector() - put raw glob information in an svector */
stringvector globtosvector(char * pattern, int filetypes);

/* globdirectorytosvector() - globs a directory listing into an svector */
stringvector globdirectorytosvector(char * dir, char * pattern, int filetypes);

/* File copying */
int   copyfile(char* srcname, char* destname, int flags);
int   copyfilebydir(char* srcdir, char* destdir, char* filename, int flags);
int   copyfilepatternbydir(char* srcdir, char* destdir, char* pattern,
													 int flags, stringvector* successlist);

/* Determine path of self from main's argv[0] */
char* locateself(char* argv0);

/* Deduce filename or path from a file's full path */
char* fileof(char* buffer, char* fullpath, int buflen);
char* pathof(char* buffer, char* fullpath, int buflen);
char* fullpath(char* path, char* file, int slashtype);

/* Run a program with given path and arguments in current directory */
int   run(char* path, char* program, char* args);

#endif