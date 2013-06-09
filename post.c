/*
 * neatpost troff postscript postprocessor
 *
 * Copyright (C) 2013 Ali Gholami Rudi <ali at rudi dot ir>
 *
 * This program is released under the Modified BSD license.
 */
#include <stdio.h>
#include <ctype.h>
#include "post.h"

static int o_pg;

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
	if (c <= 0x7f)
		return 1;
	if (c >= 0xfc)
		return 6;
	if (c >= 0xf8)
		return 5;
	if (c >= 0xf0)
		return 4;
	if (c >= 0xe0)
		return 3;
	if (c >= 0xc0)
		return 2;
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
		if (!n && c == '-') {
			neg = 1;
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

static int iseol(void)
{
	int c;
	do {
		c = next();
	} while (c != ' ');
	if (c != '\n')
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

static void draw(void)
{
	int h1, h2, v1, v2;
	int c = next();
	switch (c) {
	case 'l':
		h1 = nextnum();
		v1 = nextnum();
		outrel(h1, v1);
		break;
	case 'c':
		h1 = nextnum();
		outrel(h1, 0);
		break;
	case 'e':
		h1 = nextnum();
		v1 = nextnum();
		outrel(h1, 0);
		break;
	case 'a':
		h1 = nextnum();
		v1 = nextnum();
		h2 = nextnum();
		v2 = nextnum();
		outrel(h1 + h2, v1 + v2);
		break;
	default:
		h1 = nextnum();
		v1 = nextnum();
		outrel(h1, v1);
		while (!iseol()) {
			h2 = nextnum();
			v2 = nextnum();
			outrel(h2, v2);
		}
		break;
	}
	nexteol();
}

static char devpath[PATHLEN] = "devutf";

static void postx(void)
{
	char cmd[128];
	char dev[128];
	int pos;
	nextword(cmd);
	switch (cmd[0]) {
	case 'f':
		pos = nextnum();
		nextword(dev);
		dev_mnt(pos, dev, dev);
		break;
	case 'i':
		dev_open(devpath);
		break;
	case 'T':
		nextword(dev);
		sprintf(devpath, "%s/font/dev%s", TROFFROOT, dev);
		break;
	case 's':
		break;
	case 'X':
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
	case 'N':
		nextnum();
		break;
	case 'C':
		nextword(cs);
		outc(cs);
		break;
	case 'p':
		if (o_pg)
			ps_pageend(o_pg);
		o_pg = nextnum();
		ps_pagebeg(o_pg);
		outpage();
		break;
	case 'w':
		break;
	case 'n':
		nextnum();
		nextnum();
		break;
	case 'D':
		draw();
		break;
	case 'x':
		postx();
		break;
	case '#':
		nexteol();
		break;
	default:
		fprintf(stderr, "unknown command: %c\n", c);
	}
}

static void post(void)
{
	int c;
	while ((c = next()) >= 0)
		if (!isspace(c))
			postcmd(c);
	if (o_pg)
		ps_pageend(o_pg);
}

int main(void)
{
	ps_header();
	post();
	ps_trailer(o_pg, o_fonts);
	return 0;
}
