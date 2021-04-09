/*
 * NEATPOST: NEATROFF'S POSTSCRIPT/PDF POSTPROCESSOR
 *
 * Copyright (C) 2013-2020 Ali Gholami Rudi <ali at rudi dot ir>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "post.h"

static char *ps_title;		/* document title */
static int ps_pagewidth = 2159;	/* page width (tenths of a millimetre) */
static int ps_pageheight = 2794;/* page height (tenths of a millimetre) */
static int ps_linewidth = 40;	/* drawing line thickness in thousandths of an em */
static int o_pages;		/* output pages */

/* bookmarks */
static char (*mark_desc)[256];	/* bookmark description */
static int *mark_page;		/* bookmark page */
static int *mark_offset;	/* bookmark offset */
static int *mark_level;		/* bookmark level */
static int mark_n;		/* number of bookmarks */
static int mark_sz;		/* allocated size of bookmark arrays */

/* named destinations */
static char (*name_desc)[64];	/* reference name */
static int *name_page;		/* reference page */
static int *name_offset;	/* reference offset */
static int name_n;		/* number of references */
static int name_sz;		/* allocated size of name arrays */

static int next(void)
{
	return getc(stdin);
}

static void back(int c)
{
	ungetc(c, stdin);
}

static int utf8len(int c)
{
	if (~c & 0xc0)		/* ASCII or invalid */
		return c > 0;
	if (~c & 0x20)
		return 2;
	if (~c & 0x10)
		return 3;
	if (~c & 0x08)
		return 4;
	return 1;
}

static int nextutf8(char *s)
{
	int c = next();
	int l = utf8len(c);
	int i;
	if (c < 0)
		return 0;
	s[0] = c;
	for (i = 1; i < l; i++)
		s[i] = next();
	s[l] = '\0';
	return l;
}

/* skip blanks */
static void nextskip(void)
{
	int c;
	do {
		c = next();
	} while (isspace(c));
	back(c);
}

static int nextnum(void)
{
	int c;
	int n = 0;
	int neg = 0;
	nextskip();
	while (1) {
		c = next();
		if (!n && (c == '-' || c == '+')) {
			neg = c == '-';
			continue;
		}
		if (!isdigit(c))
			back(c);
		if (c < 0 || !isdigit(c))
			break;
		n = n * 10 + c - '0';
	}
	return neg ? -n : n;
}

static int readnum(int *n)
{
	int c;
	do {
		c = next();
	} while (c == ' ');
	back(c);
	if (c == '-' || c == '+' || (c >= '0' && c <= '9')) {
		*n = nextnum();
		return 0;
	}
	return 1;
}

static int iseol(void)
{
	int c;
	do {
		c = next();
	} while (c == ' ');
	back(c);
	return c == '\n';
}

/* skip until the end of line */
static void nexteol(void)
{
	int c;
	do {
		c = next();
	} while (c >= 0 && c != '\n');
}

static void nextword(char *s)
{
	int c;
	nextskip();
	c = next();
	while (c >= 0 && !isspace(c)) {
		*s++ = c;
		c = next();
	}
	if (c >= 0)
		back(c);
	*s = '\0';
}

/* read until eol */
static void readln(char *s)
{
	int c;
	c = next();
	while (c > 0 && c != '\n') {
		*s++ = c;
		c = next();
	}
	if (c == '\n')
		back(c);
	*s = '\0';
}

static void postline(void)
{
	int h, v;
	while (!readnum(&h) && !readnum(&v))
		drawl(h, v);
}

static void postarc(void)
{
	int h1, v1, h2, v2;
	if (!readnum(&h1) && !readnum(&v1) && !readnum(&h2) && !readnum(&v2))
		drawa(h1, v1, h2, v2);
}

static void postspline(void)
{
	int h2, v2;
	int h1 = nextnum();
	int v1 = nextnum();
	if (iseol()) {
		drawl(h1, v1);
		return;
	}
	while (!readnum(&h2) && !readnum(&v2)) {
		draws(h1, v1, h2, v2);
		h1 = h2;
		v1 = v2;
	}
	draws(h1, v1, 0, 0);
}

static void postpoly(void)
{
	int l = 'l';
	int c;
	while (!iseol() && (l == 'l' || l == '~' || l == 'a')) {
		do {
			c = next();
		} while (c == ' ');
		back(c);
		if (c != '-' && c != '+' && (c < '0' || c > '9')) {
			l = c;
			while (c >= 0 && !isspace(c))
				c = next();
			continue;
		}
		if (l == 'l')
			postline();
		if (l == '~')
			postspline();
		if (l == 'a')
			postarc();
	}
}

static void postdraw(void)
{
	int h1, v1;
	int c = next();
	drawbeg();
	switch (tolower(c)) {
	case 'l':
		h1 = nextnum();
		v1 = nextnum();
		drawl(h1, v1);
		break;
	case 'c':
		drawc(nextnum());
		break;
	case 'e':
		h1 = nextnum();
		v1 = nextnum();
		drawe(h1, v1);
		break;
	case 'a':
		postarc();
		break;
	case '~':
		postspline();
		break;
	case 'p':
		postpoly();
		break;
	}
	drawend(c == 'p' || c == 'P', c == 'E' || c == 'C' || c == 'P');
	nexteol();
}

static char *strcut(char *dst, char *src)
{
	while (*src == ' ' || *src == '\n')
		src++;
	if (src[0] == '"') {
		src++;
		while (*src && (src[0] != '"' || src[1] == '"')) {
			if (*src == '"')
				src++;
			*dst++ = *src++;
		}
		if (*src == '"')
			src++;
	} else {
		while (*src && *src != ' ' && *src != '\n')
			*dst++ = *src++;
	}
	*dst = '\0';
	return src;
}

static void postps(void)
{
	char cmd[ILNLEN];
	char arg[ILNLEN];
	nextword(cmd);
	readln(arg);
	if (!strcmp("PS", cmd) || !strcmp("ps", cmd))
		out("%s\n", arg);
	if (!strcmp("rotate", cmd))
		outrotate(atoi(arg));
	if (!strcmp("eps", cmd) || !strcmp("pdf", cmd)) {
		char path[1 << 12];
		int hwid, vwid, nspec;
		char *spec = arg;
		spec = strcut(path, spec);
		nspec = sscanf(spec, "%d %d", &hwid, &vwid);
		if (nspec < 1)
			hwid = 0;
		if (nspec < 2)
			vwid = 0;
		if (path[0] && !strcmp("eps", cmd))
			outeps(path, hwid, vwid);
		if (path[0] && !strcmp("pdf", cmd))
			outpdf(path, hwid, vwid);
	}
	if (!strcmp("name", cmd)) {
		char *spec = arg;
		int nspec;
		if (name_n == name_sz) {
			name_sz = name_sz == 0 ? 128 : name_sz * 2;
			name_desc = mextend(name_desc, name_n, name_sz, sizeof(name_desc[0]));
			name_page = mextend(name_page, name_n, name_sz, sizeof(name_page[0]));
			name_offset = mextend(name_offset, name_n, name_sz, sizeof(name_offset[0]));
		}
		spec = strcut(name_desc[name_n], spec);
		nspec = sscanf(spec, "%d %d", &name_page[name_n], &name_offset[name_n]);
		if (name_desc[name_n][0] && nspec > 0)
			name_n++;
	}
	if (!strcmp("mark", cmd)) {
		char *spec = arg;
		int nspec;
		if (mark_n == mark_sz) {
			mark_sz = mark_sz == 0 ? 128 : mark_sz * 2;
			mark_desc = mextend(mark_desc, mark_n, mark_sz, sizeof(mark_desc[0]));
			mark_page = mextend(mark_page, mark_n, mark_sz, sizeof(mark_page[0]));
			mark_offset = mextend(mark_offset, mark_n, mark_sz, sizeof(mark_offset[0]));
			mark_level = mextend(mark_level, mark_n, mark_sz, sizeof(mark_level[0]));
		}
		spec = strcut(mark_desc[mark_n], spec);
		nspec = sscanf(spec, "%d %d %d", &mark_page[mark_n],
				&mark_offset[mark_n], &mark_level[mark_n]);
		if (mark_desc[mark_n][0] && nspec > 0)
			mark_n++;
	}
	if (!strcmp("link", cmd)) {
		char link[1 << 12];
		int hwid, vwid, nspec;
		char *spec = arg;
		spec = strcut(link, spec);
		nspec = sscanf(spec, "%d %d", &hwid, &vwid);
		if (link[0] && nspec == 2)
			outlink(link, hwid, vwid);
	}
	if (!strcmp("info", cmd)) {
		char *spec = arg;
		char kwd[128];
		int i = 0;
		while (*spec == ' ')
			spec++;
		while (*spec && *spec != ' ') {
			if (i < sizeof(kwd) - 1)
				kwd[i++] = *spec;
			spec++;
		}
		kwd[i] = '\0';
		while (*spec == ' ')
			spec++;
		outinfo(kwd, spec);
	}
	if (!strcmp("set", cmd)) {
		char var[128];
		char val[128];
		if (sscanf(arg, "%128s %128s", var, val) == 2)
			outset(var, val);
	}
	if (!strcmp("BeginObject", cmd))
		drawmbeg(arg);
	if (!strcmp("EndObject", cmd))
		drawmend(arg);
}

static char postdir[PATHLEN] = TROFFFDIR;	/* output device directory */
static char postdev[PATHLEN] = "utf";		/* output device name */

static void postx(void)
{
	char cmd[128];
	char font[128];
	int pos;
	nextword(cmd);
	switch (cmd[0]) {
	case 'f':
		pos = nextnum();
		nextword(font);
		dev_mnt(pos, font, font);
		outmnt(pos);
		break;
	case 'i':
		if (dev_open(postdir, postdev)) {
			fprintf(stderr, "neatpost: cannot open device %s\n", postdev);
			exit(1);
		}
		docheader(ps_title, ps_pagewidth, ps_pageheight, ps_linewidth);
		break;
	case 'T':
		nextword(postdev);
		break;
	case 's':
		break;
	case 'X':
		postps();
		break;
	}
	nexteol();
}

static void postcmd(int c)
{
	char cs[GNLEN];
	if (isdigit(c)) {
		outrel((c - '0') * 10 + next() - '0', 0);
		nextutf8(cs);
		outc(cs);
		return;
	}
	switch (c) {
	case 's':
		outsize(nextnum());
		break;
	case 'f':
		outfont(nextnum());
		break;
	case 'H':
		outh(nextnum());
		break;
	case 'V':
		outv(nextnum());
		break;
	case 'h':
		outrel(nextnum(), 0);
		break;
	case 'v':
		outrel(0, nextnum());
		break;
	case 'c':
		nextutf8(cs);
		outc(cs);
		break;
	case 'm':
		nextword(cs);
		outcolor(clr_get(cs));
		break;
	case 'N':
		nextnum();
		break;
	case 'C':
		nextword(cs);
		outc(cs);
		break;
	case 'p':
		if (o_pages)
			docpageend(o_pages);
		o_pages = nextnum();
		docpagebeg(o_pages);
		outpage();
		break;
	case 'w':
		break;
	case 'n':
		nextnum();
		nextnum();
		break;
	case 'D':
		postdraw();
		break;
	case 'x':
		postx();
		break;
	case '#':
		nexteol();
		break;
	default:
		fprintf(stderr, "neatpost: unknown command %c\n", c);
		nexteol();
	}
}

static void post(void)
{
	int c;
	while ((c = next()) >= 0)
		if (!isspace(c))
			postcmd(c);
	if (o_pages)
		docpageend(o_pages);
	if (name_n)
		outname(name_n, name_desc, name_page, name_offset);
	if (mark_n)
		outmark(mark_n, mark_desc, mark_page, mark_offset, mark_level);
}

static struct paper {
	char *name;
	int w, h;
} papers[] = {
	{"letter", 2159, 2794},
	{"legal", 2159, 3556},
	{"ledger", 4318, 2794},
	{"tabloid", 2794, 4318},
};

static void setpagesize(char *s)
{
	int d1, d2, n, i;
	/* predefined paper sizes */
	for (i = 0; i < LEN(papers); i++) {
		if (!strcmp(papers[i].name, s)) {
			ps_pagewidth = papers[i].w;
			ps_pageheight = papers[i].h;
			return;
		}
	}
	/*
             custom paper size in tenths of mm.
             example:  2100x2970 for a4.
        */
	if (isdigit(s[0]) && strchr(s, 'x')) {
		ps_pagewidth = atoi(s);
		ps_pageheight = atoi(strchr(s, 'x') + 1);
		return;
	}
	/* ISO paper sizes */
	if (!strchr("abcABC", s[0]) || !isdigit(s[1]))
		return;
	if (tolower(s[0]) == 'a') {
		d1 = 8410;
		d2 = 11890;
	}
	if (tolower(s[0]) == 'b') {
		d1 = 10000;
		d2 = 14140;
	}
	if (tolower(s[0]) == 'c') {
		d1 = 9170;
		d2 = 12970;
	}


	n = s[1] - '0';
	ps_pagewidth = ((n & 1) ? d2 : d1) >> ((n + 1) >> 1);
	ps_pageheight = ((n & 1) ? d1 : d2) >> (n >> 1);
	ps_pagewidth -= ps_pagewidth % 10;
	ps_pageheight -= ps_pageheight % 10;
}

static void setlandscape(void) {
   int t = ps_pagewidth;
   ps_pagewidth = ps_pageheight;
   ps_pageheight = t;
}

void *mextend(void *old, long oldsz, long newsz, int memsz)
{
	void *new = malloc(newsz * memsz);
	memcpy(new, old, oldsz * memsz);
	memset(new + oldsz * memsz, 0, (newsz - oldsz) * memsz);
	free(old);
	return new;
}

/* the unicode codepoint of the given utf-8 character */
static int utf8code(char *s)
{
	int c = (unsigned char) s[0];
	if (~c & 0xc0)		/* ASCII or invalid */
		return c;
	if (~c & 0x20)
		return ((c & 0x1f) << 6) | (s[1] & 0x3f);
	if (~c & 0x10)
		return ((c & 0x0f) << 12) | ((s[1] & 0x3f) << 6) | (s[2] & 0x3f);
	if (~c & 0x08)
		return ((c & 0x07) << 18) | ((s[1] & 0x3f) << 12) | ((s[2] & 0x3f) << 6) | (s[3] & 0x3f);
	return c;
}

static int pdftext_ascii(char *s)
{
	for (; *s; s++)
		if (((unsigned char) *s) & 0x80 || *s == '(' || *s == ')')
			return 0;
	return 1;
}

/* encode s as pdf text string */
static char *pdftext(char *s)
{
	struct sbuf *sb = sbuf_make();
	if (pdftext_ascii(s)) {
		sbuf_chr(sb, '(');
		sbuf_str(sb, s);
		sbuf_chr(sb, ')');
		return sbuf_done(sb);
	}
	/* read utf-8 and write utf-16 */
	sbuf_str(sb, "<FEFF");		/* unicode byte order marker */
	while (*s) {
		int c = utf8code(s);
		if ((c >= 0 && c <= 0xd7ff) || (c >= 0xe000 && c <= 0xffff)) {
			sbuf_printf(sb, "%02X%02X", c >> 8, c & 0xff);
		}
		if (c >= 0x010000 && c <= 0x10ffff) {
			int c1 = 0xd800 + ((c - 0x10000) >> 10);
			int c2 = 0xdc00 + ((c - 0x10000) & 0x3ff);
			sbuf_printf(sb, "%02X%02X", c1 >> 8, c1 & 0xff);
			sbuf_printf(sb, "%02X%02X", c2 >> 8, c2 & 0xff);
		}
		s += utf8len((unsigned char) *s);
	}
	sbuf_chr(sb, '>');
	return sbuf_done(sb);
}

/* encode s as pdf text string; returns a static buffer */
char *pdftext_static(char *s)
{
	static char buf[1024];
	char *r = pdftext(s);
	snprintf(buf, sizeof(buf), "%s", r);
	free(r);
	return buf;
}

static char *usage =
	"Usage: neatpost [options] <input >output\n"
	"Options:\n"
	"  -F dir  \tset font directory (" TROFFFDIR ")\n"
	"  -p size \tset paper size (letter); e.g., a4, 2100x2970\n"
	"  -t title\tspecify document title\n"
	"  -w lwid \tdrawing line thickness in thousandths of an em (40)\n"
        "  -l      \tlandscape\n"
	"  -n      \talways draw glyphs by name (ps glyphshow)\n";

int main(int argc, char *argv[])
{
        int ch;
        int landscape = 0;

        while ((ch = getopt(argc,argv,"F:p:t:w:nl")) != -1) {
            switch (ch) {
            case 'F':
                strcpy(postdir, optarg);
                break;
            case 'p':
                setpagesize(optarg);
                break;
            case 't':
                ps_title = optarg;
                break;
            case 'n':
                outgname(1);
                break;
            case 'w':
                ps_linewidth = atoi(optarg);
                break;
            case 'l':
                landscape = 1;
                break;
            case '?':
            case 'h':
            default:
                printf("%s", usage);
                return EXIT_SUCCESS;
            }
        }

        if(landscape)
            setlandscape();

	post();
	doctrailer(o_pages);
	dev_close();
	free(mark_desc);
	free(mark_page);
	free(mark_offset);
	free(mark_level);
	free(name_desc);
	free(name_page);
	free(name_offset);
	return EXIT_SUCCESS;
}
