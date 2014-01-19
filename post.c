/*
 * neatpost troff postscript postprocessor
 *
 * Copyright (C) 2013-2014 Ali Gholami Rudi <ali at rudi dot ir>
 *
 * This program is released under the Modified BSD license.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
	while (!iseol()) {
		h = nextnum();
		v = nextnum();
		drawl(h, v);
	}
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
	while (!iseol()) {
		h2 = nextnum();
		v2 = nextnum();
		draws(h1, v1, h2, v2);
		h1 = h2;
		v1 = v2;
	}
	draws(h1, v1, 0, 0);
}

static void postdraw(void)
{
	int h1, h2, v1, v2;
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
		h1 = nextnum();
		v1 = nextnum();
		h2 = nextnum();
		v2 = nextnum();
		drawa(h1, v1, h2, v2);
		break;
	case '~':
		postspline();
		break;
	case 'p':
		postline();
		break;
	}
	drawend(c == 'p' || c == 'P', c == 'E' || c == 'C' || c == 'P');
	nexteol();
}

static void postps(void)
{
	char cmd[ILNLEN];
	char arg[ILNLEN];
	nextword(cmd);
	readln(arg);
	if (!strcmp("PS", cmd) || !strcmp("ps", cmd))
		out("%s\n", arg);
	if (!strcmp("BeginObject", cmd))
		drawmbeg(arg);
	if (!strcmp("EndObject", cmd))
		drawmend(arg);
}

static char devpath[PATHLEN] = TROFFFDIR;
static char devname[PATHLEN] = "utf";

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
		if (dev_open(devpath, devname)) {
			fprintf(stderr, "neatpost: cannot open device %s\n", devname);
			exit(1);
		}
		ps_header();
		break;
	case 'T':
		nextword(devname);
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

static char *usage =
	"Usage: neatpost [options] <input >output\n\n"
	"Options:\n"
	"  -Fdir \tset font directory (" TROFFFDIR ")\n";

int main(int argc, char *argv[])
{
	int i;
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-' && argv[i][1] == 'F') {
			strcpy(devpath, argv[i][2] ? argv[i] + 2 : argv[++i]);
		} else {
			printf("%s", usage);
			return 0;
		}
	}
	post();
	ps_trailer(o_pg, o_fonts);
	return 0;
}
