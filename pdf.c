#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "post.h"

static char *pdf_title;		/* document title */
static int pdf_width;		/* page width */
static int pdf_height;		/* page height */
static int pdf_pages;		/* pages object id */
static int pdf_root;		/* root object id */
static int pdf_pos;		/* current pdf file offset */
static int *obj_off;		/* object offsets */
static int obj_sz, obj_n;	/* number of pdf objects */
static int *page_id;		/* page object ids */
static int page_sz, page_n;	/* number of pages */

static struct sbuf *pg;		/* current page contents */
static int o_f, o_s, o_m;	/* font and size */
static int o_h, o_v;		/* current user position */
static int p_h, p_v;		/* current output position */
static int o_pf, p_pf;		/* output and pdf fonts; indices into o_fs[] */
static int p_f, p_s, p_m;	/* output font */
static int o_queued;		/* queued character type */
static int *o_fs;		/* page fonts */
static int o_fsn, o_fssz;	/* page fonts */

#define PSFN_MK(fn, ix)		(((fn) << 16) | (ix))
#define PSFN_FN(fi)		((fi) >> 16)
#define PSFN_IX(fi)		((fi) & 0xffff)

struct psfont {
	char name[128];		/* font PostScript name */
	char path[1024];	/* font path */
	char desc[1024];	/* font descriptor path */
	int *gmap;		/* the sub-font assigned to each glyph */
	int *gpos;		/* the location of the glyph in its sub-font */
	int gcnt;		/* glyph count */
	int lastfn;		/* the last sub-font */
	int lastgl;		/* the number of glyphs in the last subfont */
	int obj[64];		/* sub-font object ids */
	int objdes;		/* font descriptor object id */
};

static struct psfont *psfonts;
static int psfonts_n, psfonts_sz;

/* print pdf output */
static void pdfout(char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	pdf_pos += vprintf(s, ap);
	va_end(ap);
}

/* allocate an object number */
static int obj_map(void)
{
	if (obj_n == obj_sz) {
		obj_sz += 1024;
		obj_off = mextend(obj_off, obj_n, obj_sz, sizeof(obj_off[0]));
	}
	return obj_n++;
}

/* start the definition of an object */
static int obj_beg(int id)
{
	if (id <= 0)
		id = obj_map();
	obj_off[id] = pdf_pos;
	pdfout("%d 0 obj\n", id);
	return id;
}

/* end an object definition */
static void obj_end(void)
{
	pdfout("endobj\n\n");
}

void out(char *s, ...)
{
}

/* include font descriptor; returns object id */
static int psfont_writedesc(struct psfont *ps)
{
	int c, i;
	int str_obj = -1;
	int des_obj;
	char *ext = strrchr(ps->path, '.');
	if (ext && !strcmp(".ttf", ext)) {
		FILE *fp = fopen(ps->path, "r");
		struct sbuf *sb = sbuf_make();
		c = fgetc(fp);
		for (i = 0; c != EOF; i++) {
			sbuf_printf(sb, "%02x", c);
			c = fgetc(fp);
			if (i % 40 == 39 && c != EOF)
				sbuf_chr(sb, '\n');
		}
		sbuf_str(sb, ">\n");
		fclose(fp);
		str_obj = obj_beg(0);
		pdfout("<<\n");
		pdfout("  /Filter /ASCIIHexDecode\n");
		pdfout("  /Length %d\n", sbuf_len(sb));
		pdfout("  /Length1 %d\n", i);
		pdfout(">>\n");
		pdfout("stream\n");
		pdfout("%s", sbuf_buf(sb));
		pdfout("endstream\n");
		obj_end();
		sbuf_free(sb);
	}
	/* the font descriptor */
	des_obj = obj_beg(0);
	pdfout("<<\n");
	pdfout("  /Type /FontDescriptor\n");
	pdfout("  /FontName /%s\n", ps->name);
	pdfout("  /Flags 4\n");
	pdfout("  /FontBBox [-1000 -1000 1000 1000]\n");
	pdfout("  /MissingWidth 1000\n");
	pdfout("  /StemV 100\n");
	pdfout("  /ItalicAngle 0\n");
	pdfout("  /CapHeight 100\n");
	pdfout("  /Ascent 100\n");
	pdfout("  /Descent 100\n");
	if (str_obj >= 0)
		pdfout("  /FontFile2 %d 0 R\n", str_obj);
	pdfout(">>\n");
	obj_end();
	return des_obj;
}

/* write the object corresponding to font font_id[f] */
static void psfont_write(struct psfont *ps, int ix)
{
	int i;
	int enc_obj;
	char *ext = strrchr(ps->path, '.');
	struct font *fn = dev_fontopen(ps->desc);
	int map[256] = {0};
	/* finding out the mapping */
	for (i = 0; i < 256; i++)
		map[i] = -1;
	for (i = 0; i < ps->gcnt; i++)
		if (ps->gmap[i] == ix)
			map[ps->gpos[i]] = i;
	/* the encoding object */
	enc_obj = obj_beg(0);
	pdfout("<<\n");
	pdfout("  /Type /Encoding\n");
	pdfout("  /Differences [ 0");
	for (i = 0; i < 256; i++)
		pdfout(" /%s", map[i] >= 0 ? font_glget(fn, map[i])->id : ".notdef");
	pdfout(" ]\n");
	pdfout(">>\n");
	obj_end();
	/* the font object */
	obj_beg(ps->obj[ix]);
	pdfout("<<\n");
	pdfout("  /Type /Font\n");
	pdfout("  /Subtype /%s\n",
		ext && !strcmp(".ttf", ext) ? "TrueType" : "Type1");
	pdfout("  /BaseFont /%s\n", ps->name);
	pdfout("  /FirstChar 0\n");
	pdfout("  /LastChar 255\n");
	pdfout("  /Widths [");
	for (i = 0; i < 256; i++)
		pdfout(" %d", (map[i] >= 0 ? font_glget(fn, map[i])->wid : 0)
				* dev_res / 72);
	pdfout(" ]\n");
	pdfout("  /FontDescriptor %d 0 R\n", ps->objdes);
	pdfout("  /Encoding %d 0 R\n", enc_obj);
	pdfout(">>\n");
	obj_end();
	font_close(fn);
}

static int psfont_find(struct glyph *g)
{
	struct font *fn = g->font;
	char *name = font_name(fn);
	struct psfont *ps = NULL;
	int gidx;
	int i;
	for (i = 0; i < psfonts_n; i++)
		if (!strcmp(name, psfonts[i].name))
			break;
	if (i == psfonts_n) {
		if (psfonts_n == psfonts_sz) {
			psfonts_sz += 16;
			psfonts = mextend(psfonts, psfonts_n,
					psfonts_sz, sizeof(psfonts[0]));
		}
		psfonts_n++;
		ps = &psfonts[i];
		snprintf(ps->name, sizeof(ps->name), "%s", name);
		snprintf(ps->path, sizeof(ps->path), "%s", font_path(fn));
		snprintf(ps->desc, sizeof(ps->desc), "%s", font_desc(fn));
		while (font_glget(fn, ps->gcnt))
			ps->gcnt++;
		ps->gmap = calloc(ps->gcnt, sizeof(ps->gmap));
		ps->gpos = calloc(ps->gcnt, sizeof(ps->gpos));
		ps->lastfn = 0;
		ps->lastgl = 256;
	}
	ps = &psfonts[i];
	gidx = font_glnum(fn, g);
	if (!ps->gmap[gidx]) {
		if (ps->lastgl == 256) {
			ps->lastgl = 0;
			ps->lastfn++;
			ps->obj[ps->lastfn] = obj_map();
		}
		ps->gmap[gidx] = ps->lastfn;
		ps->gpos[gidx] = ps->lastgl++;
	}
	return PSFN_MK(i, ps->gmap[gidx]);
}

static int psfont_gpos(struct glyph *g)
{
	int fn = psfont_find(g);
	return psfonts[PSFN_FN(fn)].gpos[font_glnum(g->font, g)];
}

static void psfont_done(void)
{
	int i, j;
	for (i = 0; i < psfonts_n; i++) {
		struct psfont *ps = &psfonts[i];
		ps->objdes = psfont_writedesc(ps);
		for (j = 1; j <= ps->lastfn; j++)
			psfont_write(ps, j);
	}
	for (i = 0; i < psfonts_n; i++) {
		free(psfonts[i].gmap);
		free(psfonts[i].gpos);
	}
	free(psfonts);
}

static void o_flush(void)
{
	if (o_queued == 1)
		sbuf_printf(pg, "> Tj\n");
	o_queued = 0;
}

static int o_loadfont(struct glyph *g)
{
	int fn = psfont_find(g);
	int i;
	for (i = 0; i < o_fsn; i++)
		if (o_fs[i] == fn)
			return i;
	if (o_fsn == o_fssz) {
		o_fssz += 128;
		o_fs = mextend(o_fs, o_fsn, o_fssz, sizeof(o_fs[0]));
	}
	o_fs[o_fsn++] = fn;
	return o_fsn - 1;
}

#define PREC		1000
#define PRECN		"3"

/* convert troff position to pdf position; returns a static buffer */
static char *pdfpos(int uh, int uv)
{
	static char buf[64];
	int h = uh * PREC * 72 / dev_res;
	int v = pdf_height * PREC - (uv * PREC * 72 / dev_res);
	sprintf(buf, "%d.%0" PRECN "d %d.%0" PRECN "d",
		h / PREC, h % PREC, v / PREC, v % PREC);
	return buf;
}

/* convert troff color to pdf color; returns a static buffer */
static char *pdfcolor(int m)
{
	static char buf[64];
	int r = CLR_R(m) * 1000 / 255;
	int g = CLR_G(m) * 1000 / 255;
	int b = CLR_B(m) * 1000 / 255;
	sbuf_printf(pg, "%d.%03d %d.%03d %d.%03d",
		r / 1000, r % 1000, g / 1000, g % 1000, b / 1000, b % 1000);
	return buf;
}

static void o_queue(struct glyph *g)
{
	if (o_h != p_h || o_v != p_v) {
		o_flush();
		sbuf_printf(pg, "1 0 0 1 %s Tm\n", pdfpos(o_h, o_v));
		p_h = o_h;
		p_v = o_v;
	}
	if (!o_queued)
		sbuf_printf(pg, "<");
	o_queued = 1;
	sbuf_printf(pg, "%02x", psfont_gpos(g));
	p_h += font_wid(g->font, o_s, g->wid);
}

static void out_fontup(void)
{
	if (o_m != p_m) {
		o_flush();
		sbuf_printf(pg, "%s rg\n", pdfcolor(o_m));
		p_m = o_m;
	}
	if (o_pf != p_pf || o_s != p_s) {
		int fn = PSFN_FN(o_fs[o_pf]);
		int ix = PSFN_IX(o_fs[o_pf]);
		o_flush();
		sbuf_printf(pg, "/%s.%d %d Tf\n", psfonts[fn].name, ix, o_s);
		p_pf = o_pf;
		p_s = o_s;
	}
}

void outc(char *c)
{
	struct glyph *g;
	struct font *fn;
	g = dev_glyph(c, o_f);
	fn = g ? g->font : dev_font(o_f);
	if (!g) {
		outrel(*c == ' ' && fn ? font_swid(fn, o_s) : 1, 0);
		return;
	}
	o_pf = o_loadfont(g);
	out_fontup();
	o_queue(g);
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

void outeps(char *eps)
{
}

void outlink(char *spec)
{
}

void outpage(void)
{
	o_v = 0;
	o_h = 0;
	p_v = 0;
	p_h = 0;
	p_s = 0;
	p_f = 0;
	p_m = 0;
}

void outmnt(int f)
{
	if (p_f == f)
		p_f = -1;
}

void outgname(int g)
{
}

static int draw_path;	/* number of path segments */
static int draw_point;	/* point was set for postscript newpath */

void drawbeg(void)
{
	o_flush();
	out_fontup();
	if (draw_path)
		return;
	sbuf_printf(pg, "%s m\n", pdfpos(o_h, o_v));
}

void drawend(int close, int fill)
{
	if (draw_path)
		return;
	draw_point = 0;
	if (!fill)		/* stroking color */
		sbuf_printf(pg, "%s RG\n", pdfcolor(o_m));
	if (fill)
		sbuf_printf(pg, "f\n");
	else
		sbuf_printf(pg, close ? "s\n" : "S\n");
}

void drawmbeg(char *s)
{
}

void drawmend(char *s)
{
}

void drawl(int h, int v)
{
	o_flush();
	outrel(h, v);
	sbuf_printf(pg, "%s l\n", pdfpos(o_h, o_v));
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

void ps_header(char *title, int pagewidth, int pageheight, int linewidth)
{
	pdf_title = title;
	obj_map();
	pdf_root = obj_map();
	pdf_pages = obj_map();
	pdf_title = title;
	pdfout("%%PDF-1.6\n");
	pdf_width = (pagewidth * 72 + 127) / 254;
	pdf_height = (pageheight * 72 + 127) / 254;
}

void ps_trailer(int pages)
{
	int i;
	int xref_off;
	int info_id;
	/* pdf pages object */
	obj_beg(pdf_pages);
	pdfout("<<\n");
	pdfout("  /Type /Pages\n");
	pdfout("  /MediaBox [ 0 0 %d %d ]\n", pdf_width, pdf_height);
	pdfout("  /Count %d\n", page_n);
	pdfout("  /Kids [");
	for (i = 0; i < page_n; i++)
		pdfout(" %d 0 R", page_id[i]);
	pdfout(" ]\n");
	pdfout(">>\n");
	obj_end();
	/* pdf root object */
	obj_beg(pdf_root);
	pdfout("<<\n");
	pdfout("  /Type /Catalog\n");
	pdfout("  /Pages %d 0 R\n", pdf_pages);
	pdfout(">>\n");
	obj_end();
	/* fonts */
	psfont_done();
	/* info object */
	info_id = obj_beg(0);
	pdfout("<<\n");
	if (pdf_title)
		pdfout("  /Title (%s)\n", pdf_title);
	pdfout("  /Creator (Neatroff)\n");
	pdfout("  /Producer (Neatpost)\n");
	pdfout(">>\n");
	obj_end();
	/* the xref */
	xref_off = pdf_pos;
	pdfout("xref\n");
	pdfout("0 %d\n", obj_n);
	pdfout("0000000000 65535 f \n");
	for (i = 1; i < obj_n; i++)
		pdfout("%010d 00000 n \n", obj_off[i]);
	/* the trailer */
	pdfout("trailer\n");
	pdfout("<<\n");
	pdfout("  /Size %d\n", obj_n);
	pdfout("  /Root %d 0 R\n", pdf_root);
	pdfout("  /Info %d 0 R\n", info_id);
	pdfout(">>\n");
	pdfout("startxref\n");
	pdfout("%d\n", xref_off);
	pdfout("%%%%EOF\n");
	free(page_id);
	free(obj_off);
}

void ps_pagebeg(int n)
{
	pg = sbuf_make();
	sbuf_printf(pg, "BT\n");
}

void ps_pageend(int n)
{
	int cont_id;
	int i;
	o_flush();
	sbuf_printf(pg, "ET\n");
	/* page contents */
	cont_id = obj_beg(0);
	pdfout("<<\n");
	pdfout("  /Length %d\n", sbuf_len(pg));
	pdfout(">>\n");
	pdfout("stream\n");
	pdfout("%s", sbuf_buf(pg));
	pdfout("endstream\n");
	obj_end();
	/* the page object */
	if (page_n == page_sz) {
		page_sz += 1024;
		page_id = mextend(page_id, page_n, page_sz, sizeof(page_id[0]));
	}
	page_id[page_n++] = obj_beg(0);
	pdfout("<<\n");
	pdfout("  /Type /Page\n");
	pdfout("  /Parent %d 0 R\n", pdf_pages);
	pdfout("  /Resources <<\n");
	pdfout("    /Font <<");
	for (i = 0; i < o_fsn; i++) {
		int fn = PSFN_FN(o_fs[i]);
		int ix = PSFN_IX(o_fs[i]);
		pdfout(" /%s.%d %d 0 R",
			psfonts[fn].name, ix, psfonts[fn].obj[ix]);
	}
	pdfout(" >>\n");
	pdfout("  >>\n");
	pdfout("  /Contents %d 0 R\n", cont_id);
	pdfout(">>\n");
	obj_end();
	sbuf_free(pg);
	free(o_fs);
	o_fs = NULL;
	o_fsn = 0;
	o_fssz = 0;
}
