/* font handling */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "post.h"

struct font {
	char name[FNLEN];
	char fontname[FNLEN];
	int spacewid;
	struct glyph *gl;		/* glyphs present in the font */
	int gl_n, gl_sz;		/* number of glyphs in the font */
	struct dict *gl_dict;		/* mapping from gl[i].id to i */
	struct dict *ch_dict;		/* charset mapping */
};

/* find a glyph by its name */
struct glyph *font_find(struct font *fn, char *name)
{
	int i = dict_get(fn->ch_dict, name);
	return i >= 0 ? fn->gl + i : NULL;
}

/* find a glyph by its device-dependent identifier */
struct glyph *font_glyph(struct font *fn, char *id)
{
	int i = dict_get(fn->gl_dict, id);
	return i >= 0 ? &fn->gl[i] : NULL;
}

static int font_glyphput(struct font *fn, char *id, char *name, int type)
{
	struct glyph *g;
	if (fn->gl_n == fn->gl_sz) {
		fn->gl_sz = fn->gl_sz + 1024;
		fn->gl = mextend(fn->gl, fn->gl_n, fn->gl_sz, sizeof(fn->gl[0]));
	}
	g = &fn->gl[fn->gl_n];
	strcpy(g->id, id);
	strcpy(g->name, name);
	g->type = type;
	g->font = fn;
	dict_put(fn->gl_dict, g->id, fn->gl_n);
	return fn->gl_n++;
}

static void tilleol(FILE *fin, char *s)
{
	int c = fgetc(fin);
	while (c != EOF && c != '\n') {
		*s++ = c;
		c = fgetc(fin);
	}
	*s = '\0';
	if (c != EOF)
		ungetc(c, fin);
}

static int font_readchar(struct font *fn, FILE *fin, int *n, int *gid)
{
	struct glyph *g;
	char tok[ILNLEN];
	char name[ILNLEN];
	char id[ILNLEN];
	int type;
	if (fscanf(fin, "%s %s", name, tok) != 2)
		return 1;
	if (!strcmp("---", name))
		sprintf(name, "c%04d", *n);
	if (strcmp("\"", tok)) {
		if (fscanf(fin, "%d %s", &type, id) != 2)
			return 1;
		*gid = dict_get(fn->gl_dict, id);
		if (*gid < 0) {
			*gid = font_glyphput(fn, id, name, type);
			g = &fn->gl[*gid];
			sscanf(tok, "%d", &g->wid);
			tilleol(fin, tok);
			if (sscanf(tok, "%d", &g->pos) != 1)
				g->pos = 0;
		}
	}
	dict_put(fn->ch_dict, name, *gid);
	(*n)++;
	return 0;
}

static void skipline(FILE* filp)
{
	int c;
	do {
		c = getc(filp);
	} while (c != '\n' && c != EOF);
}

struct font *font_open(char *path)
{
	struct font *fn;
	int ch_g = -1;		/* last glyph in the charset */
	int ch_n = 0;			/* number of glyphs in the charset */
	char tok[ILNLEN];
	FILE *fin;
	fin = fopen(path, "r");
	if (!fin)
		return NULL;
	fn = xmalloc(sizeof(*fn));
	if (!fn) {
		fclose(fin);
		return NULL;
	}
	memset(fn, 0, sizeof(*fn));
	fn->gl_dict = dict_make(-1, 1);
	fn->ch_dict = dict_make(-1, 1);
	while (fscanf(fin, "%s", tok) == 1) {
		if (!strcmp("char", tok)) {
			font_readchar(fn, fin, &ch_n, &ch_g);
		} else if (!strcmp("spacewidth", tok)) {
			fscanf(fin, "%d", &fn->spacewid);
		} else if (!strcmp("name", tok)) {
			fscanf(fin, "%s", fn->name);
		} else if (!strcmp("fontname", tok)) {
			fscanf(fin, "%s", fn->fontname);
		} else if (!strcmp("ligatures", tok)) {
			while (fscanf(fin, "%s", tok) == 1)
				if (!strcmp("0", tok))
					break;
		} else if (!strcmp("charset", tok)) {
			while (!font_readchar(fn, fin, &ch_n, &ch_g))
				;
			break;
		}
		skipline(fin);
	}
	fclose(fin);
	return fn;
}

void font_close(struct font *fn)
{
	dict_free(fn->gl_dict);
	dict_free(fn->ch_dict);
	free(fn->gl);
	free(fn);
}

/* return width w for the given font and size */
int font_wid(struct font *fn, int sz, int w)
{
	return (w * sz + dev_uwid / 2) / dev_uwid;
}

/* space width for the give word space or sentence space */
int font_swid(struct font *fn, int sz)
{
	return font_wid(fn, sz, fn->spacewid);
}

char *font_name(struct font *fn)
{
	return fn->fontname;
}
