#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "post.h"

static char dev_dir[PATHLEN];	/* device directory */
static char dev_dev[PATHLEN];	/* output device name */
int dev_res;			/* device resolution */
int dev_uwid;			/* device unitwidth */
int dev_hor;			/* minimum horizontal movement */
int dev_ver;			/* minimum vertical movement */

/* mounted fonts */
static char fn_name[NFONTS][FNLEN];	/* font names */
static struct font *fn_font[NFONTS];	/* font structs */
static int fn_n;			/* number of device fonts */

static void skipline(FILE* filp)
{
	int c;
	do {
		c = getc(filp);
	} while (c != '\n' && c != EOF);
}

struct font *dev_fontopen(char *name)
{
	char path[PATHLEN];
	if (strchr(name, '/'))
		strcpy(path, name);
	else
		sprintf(path, "%s/dev%s/%s", dev_dir, dev_dev, name);
	return font_open(path);
}

int dev_mnt(int pos, char *id, char *name)
{
	struct font *fn;
	if (pos >= NFONTS)
		return -1;
	fn = dev_fontopen(name);
	if (!fn)
		return -1;
	if (fn_font[pos])
		font_close(fn_font[pos]);
	if (fn_name[pos] != name)	/* ignore if fn_name[pos] is passed */
		snprintf(fn_name[pos], sizeof(fn_name[pos]), "%s", id);
	fn_font[pos] = fn;
	return pos;
}

int dev_open(char *dir, char *dev)
{
	char path[PATHLEN];
	char tok[128];
	int i;
	FILE *desc;
	strcpy(dev_dir, dir);
	strcpy(dev_dev, dev);
	sprintf(path, "%s/dev%s/DESC", dir, dev);
	desc = fopen(path, "r");
	if (!desc)
		return 1;
	while (fscanf(desc, "%127s", tok) == 1) {
		if (tok[0] == '#') {
			skipline(desc);
			continue;
		}
		if (!strcmp("fonts", tok)) {
			fscanf(desc, "%d", &fn_n);
			for (i = 0; i < fn_n; i++)
				fscanf(desc, "%s", fn_name[i + 1]);
			fn_n++;
			continue;
		}
		if (!strcmp("sizes", tok)) {
			while (fscanf(desc, "%127s", tok) == 1)
				if (!strcmp("0", tok))
					break;
			continue;
		}
		if (!strcmp("res", tok)) {
			fscanf(desc, "%d", &dev_res);
			continue;
		}
		if (!strcmp("unitwidth", tok)) {
			fscanf(desc, "%d", &dev_uwid);
			continue;
		}
		if (!strcmp("hor", tok)) {
			fscanf(desc, "%d", &dev_hor);
			continue;
		}
		if (!strcmp("ver", tok)) {
			fscanf(desc, "%d", &dev_ver);
			continue;
		}
		if (!strcmp("charset", tok))
			break;
		skipline(desc);
	}
	fclose(desc);
	return 0;
}

void dev_close(void)
{
	int i;
	for (i = 0; i < NFONTS; i++) {
		if (fn_font[i])
			font_close(fn_font[i]);
		fn_font[i] = NULL;
	}
}

struct glyph *dev_glyph(char *c, int fn)
{
	if (!strncmp("GID=", c, 4))
		return font_glyph(fn_font[fn], c + 4);
	return font_find(fn_font[fn], c);
}

/* return the font struct at pos */
struct font *dev_font(int pos)
{
	return pos >= 0 && pos < NFONTS ? fn_font[pos] : NULL;
}

int dev_fontid(struct font *fn)
{
	int i;
	for (i = 0; i < NFONTS; i++)
		if (fn_font[i] == fn)
			return i;
	return 0;
}
