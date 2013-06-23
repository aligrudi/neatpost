#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "post.h"

char dev_dir[PATHLEN];	/* device directory */
int dev_res;		/* device resolution */
int dev_uwid;		/* device unitwidth */
int dev_hor;		/* minimum horizontal movement */
int dev_ver;		/* minimum vertical movement */

/* mounted fonts */
static char fn_name[NFONTS][FNLEN];	/* font names */
static struct font *fn_font[NFONTS];	/* font structs */
static int fn_n;			/* number of mounted fonts */

static void skipline(FILE* filp)
{
	int c;
	do {
		c = getc(filp);
	} while (c != '\n' && c != EOF);
}

int dev_mnt(int pos, char *id, char *name)
{
	char path[PATHLEN];
	struct font *fn;
	sprintf(path, "%s/%s", dev_dir, name);
	fn = font_open(path);
	if (!fn)
		return -1;
	if (fn_font[pos])
		font_close(fn_font[pos]);
	if (fn_name[pos] != name)	/* ignore if fn_name[pos] is passed */
		strcpy(fn_name[pos], id);
	fn_font[pos] = fn;
	return pos;
}

int dev_open(char *dir)
{
	char path[PATHLEN];
	char tok[ILNLEN];
	int i;
	FILE *desc;
	strcpy(dev_dir, dir);
	sprintf(path, "%s/DESC", dir);
	desc = fopen(path, "r");
	while (fscanf(desc, "%s", tok) == 1) {
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
			while (fscanf(desc, "%s", tok) == 1)
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
		if (!strcmp("charset", tok)) {
			break;
		}
	}
	fclose(desc);
	return 0;
}

void dev_close(void)
{
	int i;
	for (i = 0; i < fn_n; i++) {
		if (fn_font[i])
			font_close(fn_font[i]);
		fn_font[i] = NULL;
	}
}

struct glyph *dev_glyph(char *c, int fn)
{
	struct glyph *g;
	int i;
	g = font_find(fn_font[fn], c);
	if (g)
		return g;
	for (i = 0; i < fn_n; i++)
		if (fn_font[i] && fn_font[i]->special)
			if ((g = font_find(fn_font[i], c)))
				return g;
	return NULL;
}

struct glyph *dev_glyph_byid(char *id, int fn)
{
	return font_glyph(fn_font[fn], id);
}

int dev_kernpair(char *c1, char *c2)
{
	return 0;
}

/* return the font struct at pos */
struct font *dev_font(int pos)
{
	return pos >= 0 && pos < fn_n ? fn_font[pos] : NULL;
}

int charwid(int wid, int sz)
{
	/* the original troff rounds the widths up */
	return (wid * sz + dev_uwid / 2) / dev_uwid;
}

int dev_fontid(struct font *fn)
{
	int i;
	for (i = 0; i < fn_n; i++)
		if (fn_font[i] == fn)
			return i;
	return 0;
}
