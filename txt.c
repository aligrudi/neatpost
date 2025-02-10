#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "post.h"

static int o_f, o_s, o_m;	/* font and size */
static int o_h, o_v;		/* current user position */
static int p_wd, p_ht;		/* page width and height in basic units */
static int c_wdpt = 10, c_htpt = 12;	/* glyph cell width and height in points */
static int c_wd, c_ht;		/* glyph cell width and height in basic units */
static int p_cwd, p_cht;	/* page dimentions in cells */
static char *o_pg;		/* output page buffer */

void outgname(int g)
{
}

void outpage(void)
{
	o_v = 0;
	o_h = 0;
}

/* calls o_flush() if necessary */
void out(char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	vfprintf(stdout, s, ap);
	va_end(ap);
}

void outc(char *c)
{
	struct glyph *g = dev_glyph(c, o_f);
	int v = o_v / c_ht - 1;
	int h = o_h / c_wd;
	if (h >= 0 && v >= 0 && h < p_cwd && v < p_cht)
		o_pg[v * p_cwd + h] = g->name[0];
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
	if (dev_font(f))
		o_f = f;
}

void outmnt(int f)
{
}

void outsize(int s)
{
	if (s > 0)
		o_s = s;
}

void outcolor(int c)
{
	o_m = c;
}

void outrotate(int deg)
{
}

void drawmbeg(char *s)
{
}

void drawmend(char *s)
{
}

void drawbeg(void)
{
}

void drawend(int close, int fill)
{
}

void drawl(int h, int v)
{
	outrel(h, v);
}

void drawc(int c)
{
	outrel(c, 0);
}

void drawe(int h, int v)
{
	outrel(h, 0);
}

void drawa(int h1, int v1, int h2, int v2)
{
	outrel(h1 + h2, v1 + v2);
}

void draws(int h1, int v1, int h2, int v2)
{
	outrel(h1, v1);
}

void outeps(char *eps, int hwid, int vwid)
{
}

void outpdf(char *pdf, int hwid, int vwid)
{
}

void outlink(char *lnk, int hwid, int vwid)
{
}

void outname(int n, char (*desc)[64], int *page, int *off)
{
}

void outmark(int n, char (*desc)[256], int *page, int *off, int *level)
{
}

void outinfo(char *kwd, char *val)
{
}

void outset(char *var, char *val)
{
	if (!strcmp(var, "cellht"))
		c_htpt = atoi(val);
	if (!strcmp(var, "cellwd"))
		c_wdpt = atoi(val);
}

void docpagebeg(int n)
{
	memset(o_pg, 0, p_cwd * p_cht);
	if (n > 1)
		out("\n");
}

void docpageend(int n)
{
	int lastln = 0;
	int i, j;
	for (i = 0; i < p_cht; i++)
		for (j = 0; j < p_cwd; j++)
			if (o_pg[i * p_cwd + j])
				lastln = i + 1;
	for (i = 0; i < lastln; i++) {
		int lastcol = 0;
		for (j = 0; j < p_cwd; j++)
			if (o_pg[i * p_cwd + j])
				lastcol = j + 1;
		for (j = 0; j < lastcol; j++) {
			int c = o_pg[i * p_cwd + j];
			printf("%c", c ? c : ' ');
		}
		printf("\n");
	}
}

void doctrailer(int pages)
{
	free(o_pg);
}

void docheader(char *title, int pagewidth, int pageheight, int linewidth)
{
	p_wd = pagewidth * dev_res / 254;
	p_ht = pageheight * dev_res / 254;
	c_wd = dev_res * c_wdpt / 72;
	c_ht = dev_res * c_htpt / 72;
	p_cwd = p_wd / c_wd;
	p_cht = p_ht / c_ht;
	o_pg = malloc(p_cwd * p_cht);
}
