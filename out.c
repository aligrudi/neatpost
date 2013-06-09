#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "post.h"

static int o_f, o_s;		/* font and size */
static int o_h, o_v;		/* current user position */
static int p_f, p_s;		/* output postscript font */
static int o_qtype;		/* queued character type */
static int o_qv, o_qh, o_qend;	/* queued character position */
char o_fonts[FNLEN * NFONTS] = " ";

static void outf(char *s, va_list ap)
{
	vfprintf(stdout, s, ap);
}

void outvf(char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	outf(s, ap);
	va_end(ap);
}

static void o_flush(void)
{
	if (o_qtype == 1)
		outvf(") %d %d w\n", o_qh, o_qv);
	if (o_qtype == 2)
		outvf("] %d %d g\n", o_qh, o_qv);
	o_qtype = 0;
}

void outpage(void)
{
	o_flush();
	o_v = 0;
	o_h = 0;
	p_s = 0;
	p_f = 0;
}

static void o_queue(struct glyph *g)
{
	int type = 1 + !isdigit((g)->id[0]);
	int num = atoi(g->id);
	if (o_qtype != type || o_qend != o_h || o_qv != o_v) {
		o_flush();
		o_qh = o_h;
		o_qv = o_v;
		o_qtype = type;
		outvf(type == 1 ? "(" : "[");
	}
	if (o_qtype == 1) {
		if (num >= ' ' && num <= '~')
			outvf("%s%c", strchr("()\\", num) ? "\\" : "", num);
		else
			outvf("\\%d%d%d",
				(num >> 6) & 7, (num >> 3) & 7, num & 7);
	} else {
		outvf("/%s", g->id);
	}
	o_qend = o_h + charwid(g->wid, o_s);
}

/* calls o_flush() if necessary */
void out(char *s, ...)
{
	va_list ap;
	o_flush();
	va_start(ap, s);
	outf(s, ap);
	va_end(ap);
}

/* glyph placement adjustments */
static struct fixlist {
	char *name;
	int dh, dv;
} fixlist[] = {
	{"br", -5, 15},
	{"lc", 20, 0},
	{"lf", 20, 0},
	{"rc", -11, 0},
	{"rf", -11, 0},
	{"rn", -50, 0},
};

static void fixpos(struct glyph *g, int *dh, int *dv)
{
	struct font *fn = g->font;
	int i;
	*dh = 0;
	*dv = 0;
	if (!strcmp("S", fn->name)) {
		for (i = 0; i < LEN(fixlist); i++) {
			if (!strcmp(fixlist[i].name, g->name)) {
				*dh = charwid(fixlist[i].dh, o_s);
				*dv = charwid(fixlist[i].dv, o_s);
				return;
			}
		}
	}
}

void outc(char *c)
{
	struct glyph *g;
	struct font *fn;
	char fnname[FNLEN];
	int fontid;
	int dh, dv;
	g = dev_glyph(c, o_f);
	fn = g ? g->font : dev_font(o_f);
	if (!g) {
		outrel(*c == ' ' && fn ? charwid(fn->spacewid, o_s) : 1, 0);
		return;
	}
	fontid = dev_fontid(fn, o_f);
	if (fontid != p_f || o_s != p_s) {
		out("%d /%s f\n", o_s, fn->psname);
		p_f = fontid;
		p_s = o_s;
		sprintf(fnname, " %s ", fn->psname);
		if (!strstr(o_fonts, fnname))
			sprintf(strchr(o_fonts, '\0'), "%s ", fn->psname);
	}
	fixpos(g, &dh, &dv);
	o_h += dh;
	o_v += dv;
	o_queue(g);
	o_h -= dh;
	o_v -= dv;
}

void outh(int h)
{
	o_h = h;
}

void outv(int v)
{
	o_v = v;
}

void outrel(int h, int v)
{
	o_h += h;
	o_v += v;
}

void outfont(int f)
{
	o_f = f;
}

void outsize(int s)
{
	o_s = s;
}
