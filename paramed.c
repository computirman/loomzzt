/* paramed.c  -- Parameter editor
 * $Id: paramed.c,v 1.10 2002/03/20 04:52:25 bitman Exp $
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

#include "paramed.h"

#include "screen.h"
#include "editbox.h"

#include "libzzt2/zzt.h"
#include "help.h"
#include "dialog.h"

#include "display.h"

#include <stdlib.h>
#include <string.h>

/* Label colorings */
#define LABEL_COLOR  0x0A
#define OPTION_COLOR 0x0B

/* Option ID's used in the dialog */
#define ID_NONE         0x0100
#define ID_PROGRAM      0x0200
#define ID_DIRECTION    0x0300
#define ID_FIRERATE     0x0400
#define ID_PROJECTILE   0x0500
#define ID_CYCLE        0x0600
#define ID_XSTEP        0x0700
#define ID_YSTEP        0x0800
#define ID_DATA0        0x0900
#define ID_DATA1        0x0A00
#define ID_DATA2        0x0B00
#define ID_INSTRUCTION  0x0C00
/* The other IDs come from the ZZT_DATAUSE_* set */

/* Option IDs used in tile info */
#define ID_ADDPARAM     1
#define ID_RMPARAM      2
#define ID_EDITPARAM    3

/* Table of direction names based on direction flags */
const char * direction_table[] = {
	"Idle",
	"North",
	"South",
	"", /* North-South */
	"East",
	"North-East",
	"South-East",
	"", /* North-South-East */
	"West",
	"North-West",
	"South-West",
};


void modifyparam(displaymethod * d, ZZTworld * w, int x, int y)
{
	dialog dia;
	int key;

	if (zztTileGet(w, x, y).param == NULL)
		return;

	/* Build the dialog */
	dia = buildparamdialog(w, x, y);

	do {
		int rebuild = 0;
		/* Draw the dialog each time around */
		dialogDraw(d, dia);

		key = d->getch();

		switch (key) {
			case DKEY_DOWN: dialogNextOption(&dia); break;
			case DKEY_UP:   dialogPrevOption(&dia); break;
			case DKEY_ENTER:
				rebuild = parameditoption(d, w, x, y, dialogGetCurOption(dia));
				break;
			case DKEY_LEFT:
				rebuild = paramdeltaoption(d, w, x, y, dialogGetCurOption(dia), -1);
				break;
			case DKEY_RIGHT:
				rebuild = paramdeltaoption(d, w, x, y, dialogGetCurOption(dia), 1);
				break;
			case DKEY_F1:
				helpsectiontopic("kstats", "", d);
				break;
		}

		if (rebuild) {
			/* Rebuild param dialog */
			int curoption;
			rebuild = 0;

			curoption = dia.curoption;
			dialogFree(&dia);
			dia = buildparamdialog(w, x, y);
			dia.curoption = curoption;
		}
	} while (key != DKEY_ESC);

	dialogFree(&dia);
}

stringvector programtosvector(ZZTparam * p, int editwidth)
{
	stringvector sv;    /* list of strings */
	char *str = NULL;   /* temporary string */
	int strpos = 0;     /* position in str */
	int i;

	initstringvector(&sv);

	/* load the vector */
	if ((p->program == NULL) | (p->length <= 0)) {
		/* No data! We need to create an empty node */
		pushstring(&sv, str_dupmin("", editwidth + 1));
		return sv;
	}

	/* Let's fill the node from program! */
	strpos = 0;
	str = (char *) malloc(sizeof(char) * (editwidth + 1));

	for (i = 0; i < p->length; i++) {
		if (p->program[i] == 0x0d) {
			/* end of the line (heh); push the string and start over */
			str[strpos] = 0;
			pushstring(&sv, str);
			strpos = 0;
			str = (char *) malloc(sizeof(char) * (editwidth + 1));
		} else if (strpos > editwidth) {
			/* hmmm... really long line; must not have been made in ZZT... */
			/* let's truncate! */
			str[strpos] = 0;
			pushstring(&sv, str);
			strpos = 0;
			str = (char *) malloc(sizeof(char) * (editwidth + 1));
			/* move to next 0x0d */
			do i++; while (i < p->length && p->program[i] != 0x0d);
		} else {
			/* just your everyday copying... */
			str[strpos++] = p->program[i];
		}
	}

	if (strpos > 0) {
		/* strange... we seem to have an extra line with no CR at the end... */
		str[strpos] = 0;
		pushstring(&sv, str);
	} else {
		/* we grabbed all that RAM for nothing. Darn! */
		free(str);
	}

	return sv;
}

ZZTparam svectortoprogram(stringvector sv)
{
	ZZTparam p;
	int pos;

	/* find out how much space we need */
	p.length = 0;
	/* and now for a wierdo for loop... */
	for (sv.cur = sv.first; sv.cur != NULL; sv.cur = sv.cur->next)
		p.length += strlen(sv.cur->s) + 1;		/* + 1 for CR */

	if (p.length <= 1) {
		/* sv holds one empty string (it can happen) */
		p.program = NULL;
		p.length = 0;
		return p;
	}

	/* lets make room for all that program */
	pos = 0;
	p.program = (char *) malloc(sizeof(char) * p.length);

	for (sv.cur = sv.first; sv.cur != NULL; sv.cur = sv.cur->next) {
		int i;
		int linelen = strlen(sv.cur->s);	/* I feel efficient today */
		for (i = 0; i < linelen; i++) {
			p.program[pos++] = sv.cur->s[i];
		}
		p.program[pos++] = 0x0d;
	}

	return p;
}

void editprogram(displaymethod * d, ZZTparam * p)
{
	stringvector sv;
	ZZTparam newparam;

	sv = programtosvector(p, EDITBOX_ZZTWIDTH);

	/* Now that the node is full, we can edit it. */
	sv.cur = sv.first;	/* This is redundant, but hey. */
	editbox("Program Editor", &sv, EDITBOX_ZZTWIDTH, 1, d);

	/* Okay, let's put the vector back in program */
	newparam = svectortoprogram(sv);

	deletestringvector(&sv);
	if (p->program != NULL)
		free(p->program);

	p->length = newparam.length;
	p->program = newparam.program;
}


int getdirection(char xstep, char ystep)
{
	int dir = 0;

	if (xstep > 0)      dir |= DIR_EAST;
	else if (xstep < 0) dir |= DIR_WEST;
	if (ystep > 0)      dir |= DIR_SOUTH;
	else if (ystep < 0) dir |= DIR_NORTH;

	return dir;
}

void getxystep(char * xstep, char * ystep, int dir)
{
	/* Start with a clear slate */
	*xstep = 0; *ystep = 0;

	if (dir & DIR_EAST) (*xstep)++;
	if (dir & DIR_WEST) (*xstep)--;
	if (dir & DIR_SOUTH) (*ystep)++;
	if (dir & DIR_NORTH) (*ystep)--;
}

int nextdirection(int dir)
{
	/* TODO: less sporatic for non-cardinal directions, please */

	if (dir == 0)
		return 1;
	if (dir & 0x80)
		return 0;
	return dir << 1;
}

char * scalestring(char * buffer, int data)
{
	/* Turn data into a scale string in the form: ..*.|.... */
	int i;
	const char DOTCH = 0xFA, STARCH = '*', CLINE = '|', CSTAR = '+';

	/* Create first four dots */
	for (i = 0; i < 4; i++)
		if (i == data)
			buffer[i] = STARCH;
		else
			buffer[i] = DOTCH;

	/* Create center line */
	if (i == data)
		buffer[i] = CSTAR;
	else
		buffer[i] = CLINE;

	/* Create last four dots */
	for (i++; i < 9; i++)
		if (i == data)
			buffer[i] = STARCH;
		else
			buffer[i] = DOTCH;

	buffer[i] = '\x0';

	return buffer;
}

char * paramdatavaluestring(char * buffer, ZZTtile tile, int which, ZZTworld * w)
{
	u_int8_t data = tile.param->data[which];

	switch (zztParamDatauseGet(tile, which)) {
		case ZZT_DATAUSE_FIRERATEMODE:
			if (data > 128) data -= 128;
			/* Continue through... */
		case ZZT_DATAUSE_DUPRATE:
			if (data > 8)
				sprintf(buffer, "%d", data);
			else {
				strcpy(buffer, "Slow ");
				scalestring(buffer + strlen(buffer), data);
				strcat(buffer, " Fast");
			}
			break;
		case ZZT_DATAUSE_SENSITIVITY:
		case ZZT_DATAUSE_INTELLIGENCE:
		case ZZT_DATAUSE_RESTTIME:
		case ZZT_DATAUSE_DEVIANCE:
		case ZZT_DATAUSE_STARTTIME:
		case ZZT_DATAUSE_PERIOD:
			if (data > 8)
				sprintf(buffer, "%d", data);
			else {
				strcpy(buffer, "1 ");
				scalestring(buffer + strlen(buffer), data);
				strcat(buffer, " 9");
			}
			break;
		case ZZT_DATAUSE_SPEED:
			if (data > 8)
				sprintf(buffer, "%d", data);
			else {
				strcpy(buffer, "Fast ");
				scalestring(buffer + strlen(buffer), data);
				strcat(buffer, " Slow");
			}
			break;
		case ZZT_DATAUSE_PASSAGEDEST:
			if (data < zztWorldGetBoardcount(w))
				strcpy(buffer, w->boards[data].title);
			break;
		case ZZT_DATAUSE_CHAR:
			sprintf(buffer, "%c - #char %d - %Xh", (data != 0 ? data : ' '), data, data);
			break;
		case ZZT_DATAUSE_OWNER:
			strcpy(buffer, data == 0 ? "Player" : "Creature");
			break;
	}

	return buffer;
}

dialog buildparamdialog(ZZTworld * w, int x, int y)
{
	ZZTtile tile = zztTileGet(w, x, y);
	u_int8_t properties = zztParamGetProperties(tile);
	dialog dia;

	int i;
	char buffer[100];

	dialogComponent label  = dialogComponentMake(DIALOG_COMP_LABEL,   0, 1, LABEL_COLOR,  NULL, ID_NONE);
	dialogComponent option = dialogComponentMake(DIALOG_COMP_OPTION, 20, 1, OPTION_COLOR, NULL, ID_NONE);

	/* Handy macros for using template label & option */
#define _addlabel(TEXT)      { label.text  = (TEXT); dialogAddComponent(&dia, label); label.y++; }
#define _addoption(TEXT, ID) { option.text = (TEXT); option.id = (ID); dialogAddComponent(&dia, option); option.y++; }

	/* Initialize the dialog */
	dialogInit(&dia);

	/* Generate the title */
	dialogAddComponent(&dia, dialogComponentMake(DIALOG_COMP_TITLE, 0, 0, 0x0F, (char *) zztTileGetName(tile), ID_NONE));


	/* Add "edit program" option */
	if (properties & ZZT_PROPERTY_PROGRAM) {
		_addlabel(tile.type == ZZT_OBJECT ? "Program" : "Text");
		_addoption("Edit", ID_PROGRAM);
	}

	/* Direction option */
	if (properties & ZZT_PROPERTY_STEP) {
		_addlabel("Direction");
		_addoption((char *) direction_table[getdirection(tile.param->xstep, tile.param->ystep)], ID_DIRECTION);
	}

	for (i = 0; i < 3; i++) {
		int datause = zztParamDatauseGet(tile, i);
		if (datause == ZZT_DATAUSE_FIRERATEMODE) {
			/* Confounded special case */

			int rate = tile.param->data[i];
			/* Remove the projectile-type component for printing */
			if (rate > 128) rate -= 128;
			_addlabel("Firing Rate");
			sprintf(buffer, "%d", rate);
			_addoption(buffer, ID_FIRERATE);

			_addlabel("Projectile");
			_addoption(tile.param->data[i] < 128 ? "Bullets" : "Throwstars", ID_PROJECTILE);
		} else if (datause != ZZT_DATAUSE_NONE) {
			char * usename = (char *) zztParamDatauseGetName(tile, i);
			_addlabel(usename);
			paramdatavaluestring(buffer, tile, i, w);
			_addoption(buffer, zztParamDatauseGet(tile, i));
		}
	}

	if (properties & ZZT_PROPERTY_CYCLE) {
		/* Add a blank line before cycle if not the first item */
		if (label.y > 1) { label.y++; option.y++; }

		_addlabel("Cycle");
		sprintf(buffer, "%d", tile.param->cycle);
		_addoption(buffer, ID_CYCLE);
	}

	/* Advanced configuration */
	dialogAddComponent(&dia, dialogComponentMake(DIALOG_COMP_HEADING, 0, 7, 0x0F, "Advanced Tweaking", ID_NONE));

	label.y = option.y = 8;
	label.x = 0; option.x = 8;

	_addlabel("X Step");
	_addlabel("Y Step");
	_addlabel("Data 1");
	_addlabel("Data 2");
	_addlabel("Data 3");

	sprintf(buffer, "%d", (char) tile.param->xstep); _addoption(buffer, ID_XSTEP);
	sprintf(buffer, "%d", (char) tile.param->ystep); _addoption(buffer, ID_YSTEP);
	sprintf(buffer, "%d", tile.param->data[0]); _addoption(buffer, ID_DATA0);
	sprintf(buffer, "%d", tile.param->data[1]); _addoption(buffer, ID_DATA1);
	sprintf(buffer, "%d", tile.param->data[2]); _addoption(buffer, ID_DATA2);

	if (properties & ZZT_PROPERTY_PROGRAM) {
		option.x = 20;
		_addlabel("Program Instruction");
		sprintf(buffer, "%d", (int) tile.param->instruction); _addoption(buffer, ID_INSTRUCTION);
	}

	return dia;
}

int parameditoption(displaymethod * d, ZZTworld * w, int x, int y, dialogComponent * opt)
{
	ZZTtile tile = zztTileGet(w, x, y);
	int num;   /* General use number */

	switch (opt->id) {
		case ID_PROGRAM:
			editprogram(d, tile.param);
			return 1;
		case ZZT_DATAUSE_PASSAGEDEST:
			tile.param->data[2] = boarddialog(w, tile.param->data[2], "Passage Destination", 0, d);
			return 1;
		case ZZT_DATAUSE_CHAR:
			num = charselect(d, tile.param->data[0]);
			if (num != -1)
				tile.param->data[0] = num;
			return 1;
		/* 8-bit numbers */
		case ZZT_DATAUSE_DUPRATE:
		case ZZT_DATAUSE_SENSITIVITY:
		case ZZT_DATAUSE_INTELLIGENCE:
		case ZZT_DATAUSE_RESTTIME:
		case ZZT_DATAUSE_SPEED:
		case ZZT_DATAUSE_DEVIANCE:
		case ZZT_DATAUSE_STARTTIME:
		case ZZT_DATAUSE_PERIOD:
			/* Don't edit these for numbers under 9 */
			if (tile.param->data[zztParamDatauseLocate(opt->id)] < 9)
				return 0;
		case ID_CYCLE:
		case ID_DATA0:
		case ID_DATA1:
		case ID_DATA2:
			/* zero's are special */
			if (str_equ(opt->text, "0", 0)) opt->text[0] = '\x0';
			if (dialogComponentEdit(d, opt, 3, LINED_NUMBER) == LINED_OK) {
				sscanf(opt->text, "%d", &num);
				/* No exceeding the bounds of an 8-bit number */
				if (num > 255) num = 255;
				/* zero's are special */
				if (opt->text[0] == '\x0') num = 0;

				/* Is this lame, or what? */
				/* We could put the above in a function and use
				 * the top level switch only, but why bother? */
				switch (opt->id) {
					case ID_CYCLE:
						tile.param->cycle = num; break;
					case ID_DATA0:
						tile.param->data[0] = num; break;
					case ID_DATA1:
						tile.param->data[1] = num; break;
					case ID_DATA2:
						tile.param->data[2] = num; break;
					default:
						tile.param->data[zztParamDatauseLocate(opt->id)] = num;
						break;
				}
			}
			return 1;
		/* signed 8-bit values -- ack! */
		case ID_XSTEP:
		case ID_YSTEP:
			/* almost like regular 8-bits... */
			/* zero's are special */
			if (str_equ(opt->text, "0", 0)) opt->text[0] = '\x0';
			if (dialogComponentEdit(d, opt, 4, LINED_SNUMBER) == LINED_OK) {
				sscanf(opt->text, "%d", &num);
				/* No exceeding the bounds of a signed 8-bit number */
				if (num > 127) num = 127;
				if (num < -128) num = -128;
				/* zero's are special */
				if (opt->text[0] == '\x0') num = 0;
				if (opt->id == ID_XSTEP) tile.param->xstep = num;
				else tile.param->ystep = num;
			}
			return 1;
		case ID_FIRERATE:
			if (str_equ(opt->text, "0", 0)) opt->text[0] = '\x0';
			if (dialogComponentEdit(d, opt, 3, LINED_NUMBER) == LINED_OK) {
				int firerateindex = zztParamDatauseLocate(ZZT_DATAUSE_FIRERATEMODE);
				sscanf(opt->text, "%d", &num);
				/* No exceeding the bounds of a 7-bit number */
				if (num > 127) num = 127;
				/* zero's are special */
				if (opt->text[0] == '\x0') num = 0;

				tile.param->data[firerateindex] &= 0x80;
				tile.param->data[firerateindex] |= num;
			}
			return 1;
		case ID_INSTRUCTION:
			/* zero's are special */
			if (str_equ(opt->text, "0", 0)) opt->text[0] = '\x0';
			if (dialogComponentEdit(d, opt, 6, LINED_SNUMBER) == LINED_OK) {
				/* TODO: handle signed-ness better */
				sscanf(opt->text, "%d", &num);
				/* zero's are special */
				if (opt->text[0] == '\x0') num = 0;

				tile.param->instruction = num;
			}
			return 1;
		case ID_PROJECTILE:
			tile.param->data[zztParamDatauseLocate(ZZT_DATAUSE_FIRERATEMODE)] ^= 0x80;
			return 1;
		case ZZT_DATAUSE_OWNER:
			tile.param->data[zztParamDatauseLocate(opt->id)] = !tile.param->data[zztParamDatauseLocate(opt->id)];
			return 1;
		case ID_DIRECTION:
			{
				char xstep, ystep;
				num = getdirection(tile.param->xstep, tile.param->ystep);
				num = nextdirection(num);
				getxystep(&xstep, &ystep, num);
				tile.param->xstep = xstep;
				tile.param->ystep = ystep;
			}
			return 1;
	}

	/* No change occured if we reach this point */
	return 0;
}

int paramdeltaoption(displaymethod * d, ZZTworld * w, int x, int y, dialogComponent * opt, int delta)
{
	ZZTtile tile = zztTileGet(w, x, y);
	int dul = zztParamDatauseLocate(opt->id);

	switch (opt->id) {
		case ZZT_DATAUSE_DUPRATE:
		case ZZT_DATAUSE_SENSITIVITY:
		case ZZT_DATAUSE_INTELLIGENCE:
		case ZZT_DATAUSE_RESTTIME:
		case ZZT_DATAUSE_SPEED:
		case ZZT_DATAUSE_DEVIANCE:
		case ZZT_DATAUSE_STARTTIME:
		case ZZT_DATAUSE_PERIOD:
			if (tile.param->data[dul] < -delta)
				tile.param->data[dul] = 0;
			else if (tile.param->data[dul] < 9 && tile.param->data[dul] > 8 - delta)
				tile.param->data[dul] = 8;
			else
				tile.param->data[dul] += delta;
			return 1;
		case ZZT_DATAUSE_CHAR:
			tile.param->data[dul] += delta;
			return 1;

		case ID_CYCLE:
			/* Cycle stays within [0, 255] */
			if (tile.param->cycle < -delta)
				tile.param->cycle = 0;
			else if (tile.param->cycle > 255 - delta)
				tile.param->cycle = 255;
			else
				tile.param->cycle += delta;
			return 1;
		case ID_XSTEP: tile.param->xstep += delta; return 1;
		case ID_YSTEP: tile.param->ystep += delta; return 1;

		case ID_DATA0: tile.param->data[0] += delta; return 1;
		case ID_DATA1: tile.param->data[1] += delta; return 1;
		case ID_DATA2: tile.param->data[2] += delta; return 1;

		case ID_FIRERATE:
		case ID_INSTRUCTION:
			/* TODO: implement */
			return 0;
	}

	return 0;
}

/* Tile info dialog */

void tileinfo(displaymethod * d, ZZTworld * w, int x, int y)
{
	dialog dia;
	int key;

	dia = buildtileinfodialog(w, x, y);

	do {
		int rebuild = 0;
		ZZTtile t;

		/* Draw the dialog */
		dialogDraw(d, dia);

		key = d->getch();

		switch (key) {
			case DKEY_DOWN: dialogNextOption(&dia); break;
			case DKEY_UP:   dialogPrevOption(&dia); break;
			case DKEY_ENTER:
				rebuild = 1;
				switch (dialogGetCurOption(dia)->id) {
					case ID_ADDPARAM:
						t = zztTileGet(w, x, y);
						t.param = zztParamCreate(t);
						/* If this is usually a non-param type, force param creation */
						if (t.param == NULL) t.param = zztParamCreateBlank();
						zztPlot(w, x, y, t);
						break;
					case ID_RMPARAM:
						t = zztTileGet(w, x, y);
						t.param = NULL;
						zztPlot(w, x, y, t);
						break;
					case ID_EDITPARAM:
						modifyparam(d, w, x, y);
						break;
					default:
						rebuild = 0;
				}
				break;
			case DKEY_F1:
				helpsectiontopic("ktileinf", "", d);
				break;
		}

		if (rebuild) {
			/* Rebuild dialog */
			rebuild = 0;

			dialogFree(&dia);
			dia = buildtileinfodialog(w, x, y);
		}

	} while (key != DKEY_ESC);

	dialogFree(&dia);
}

const char * _color_name_table[] = {
	"Black",
	"Blue",
	"Green",
	"Cyan",
	"Red",
	"Purple",
	"Brown",
	"Light Grey",
	"Dark Grey",
	"Light Blue",
	"Light Green",
	"Light Cyan",
	"Light Red",
	"Light Purple",
	"Yellow",
	"White"
};

char * colorname(char * buf, int color)
{
	buf[0] = '\x0';

	/* Number form */
	sprintf(buf, "%X: ", color);

	/* Text form */
	if (color & 0x80) strcat(buf, "Blinking ");
	strcat(buf, _color_name_table[color & 0x0F]);
	strcat(buf, " on ");
	strcat(buf, _color_name_table[(color & 0x70) >> 4]);

	return buf;
}

dialog buildtileinfodialog(ZZTworld * w, int x, int y)
{
	dialog dia;
	ZZTtile tile = zztTileGet(w, x, y);
	char buf[256];

	dialogComponent label = dialogComponentMake(DIALOG_COMP_LABEL,  0, 1, LABEL_COLOR,  NULL, ID_NONE);
	dialogComponent value = dialogComponentMake(DIALOG_COMP_LABEL, 13, 1, OPTION_COLOR, NULL, ID_NONE);

	/* Also use _addlabel() from buildparamdialog() */
#define _addvalue(TEXT)      { value.text  = (TEXT); dialogAddComponent(&dia, value); value.y++; }

	/* Initialize the dialog */
	dialogInit(&dia);

	/* Generate the title */
	dialogAddComponent(&dia, dialogComponentMake(DIALOG_COMP_TITLE, 0, 0, 0x0F, (char *) zztTileGetName(tile), ID_NONE));

	/* Common information to all types */
	_addlabel("ZZT-OOP Kind");
	_addlabel("Id Number");
	_addlabel("Color");
	_addlabel("Coordinate");

	_addvalue((char *) zztTileGetKind(tile));

	sprintf(buf, "%d, %Xh", tile.type, tile.type);
	_addvalue(buf);

	_addvalue(colorname(buf, tile.color));

	sprintf(buf, "(%d, %d)", x + 1, y + 1);
	_addvalue(buf);

	/* Stats vs. No Stats */
	if (tile.param == NULL) {
		dialogAddComponent(&dia, dialogComponentMake(DIALOG_COMP_HEADING, 0, value.y + 1, 0x0F, "No Stats", ID_NONE));
		/* TODO: center */
		dialogAddComponent(&dia, dialogComponentMake(DIALOG_COMP_OPTION, 0, value.y + 2, OPTION_COLOR, "Add Stats", ID_ADDPARAM));
	} else {
		ZZTtile under = { 0, 0, NULL};

		dialogAddComponent(&dia, dialogComponentMake(DIALOG_COMP_HEADING, 0, value.y + 1, 0x0F, "Stats", ID_NONE));
		/* TODO: center */
		dialogAddComponent(&dia, dialogComponentMake(DIALOG_COMP_OPTION, 0, value.y + 2, OPTION_COLOR, "Edit Stats", ID_EDITPARAM));
		dialogAddComponent(&dia, dialogComponentMake(DIALOG_COMP_OPTION, 0, value.y + 3, OPTION_COLOR, "Remove Stats", ID_RMPARAM));

		/* Advance the label/value templates */
		label.y += 5; value.y += 5;

		/* Tile under info */
		under.type = tile.param->utype;
		under.color = tile.param->ucolor;

		_addlabel("Under Type");
		_addlabel("Under Color");

		sprintf(buf, "%d, %X: %s", under.type, under.type, zztTileGetName(under));
		_addvalue(buf);
		_addvalue(colorname(buf, under.color));

		if (tile.param->program != NULL) {
			int i, instr = tile.param->instruction, plen = tile.param->length;

			_addlabel("Code Length");
			_addlabel("Current Code");

			sprintf(buf, "%d", plen);
			_addvalue(buf);

			for (i = 0; instr + i < plen && tile.param->program[instr + i] != '\r' && i < 255; i++)
				buf[i] = tile.param->program[instr + i];
			buf[i] = '\x0';

			_addvalue(buf);
		}
	}

	return dia;
}

