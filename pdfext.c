/* Parse and extract PDF objects */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "post.h"

/* the number white space characters */
int pdf_ws(char *pdf, int len, int pos)
{
	int i = pos;
	while (i < len && isspace((unsigned char) pdf[i]))
		i++;
	return i - pos;
}

/* s: string, d: dictionary, l: list, n: number, /: name, r: reference */
int pdf_type(char *pdf, int len, int pos)
{
	pos += pdf_ws(pdf, len, pos);
	if (pdf[pos] == '/')
		return '/';
	if (pdf[pos] == '(')
		return 's';
	if (pdf[pos] == '<' && pdf[pos + 1] != '<')
		return 's';
	if (pdf[pos] == '<' && pdf[pos + 1] == '<')
		return 'd';
	if (pdf[pos] == '[')
		return 'l';
	if (strchr("0123456789+-.", (unsigned char) pdf[pos])) {
		if (!isdigit((unsigned char) pdf[pos]))
			return 'n';
		while (pos < len && isdigit((unsigned char) pdf[pos]))
			pos++;
		pos += pdf_ws(pdf, len, pos);
		if (!isdigit((unsigned char) pdf[pos]))
			return 'n';
		while (pos < len && isdigit((unsigned char) pdf[pos]))
			pos++;
		pos += pdf_ws(pdf, len, pos);
		return pos < len && pdf[pos] == 'R' ? 'r' : 'n';
	}
	return -1;
}

/* the length of a pdf object */
int pdf_len(char *pdf, int len, int pos)
{
	int c;
	int old = pos;
	if (pos >= len)
		return 0;
	pos += pdf_ws(pdf, len, pos);
	c = (unsigned char) pdf[pos];
	if (strchr("0123456789+-.", c)) {
		if (pdf_type(pdf, len, pos) == 'r') {
			char *r = memchr(pdf + pos, 'R', len - pos);
			return r - (pdf + old) + 1;
		}
		pos++;
		while (pos < len && strchr("0123456789.", (unsigned char) pdf[pos]))
			pos++;
	}
	if (c == '(') {
		int depth = 1;
		pos++;
		while (pos < len && depth > 0) {
			if (pdf[pos] == '(')
				depth++;
			if (pdf[pos] == ')')
				depth--;
			if (pdf[pos] == '\\')
				pos++;
			pos++;
		}
	}
	if (c == '<' && pos + 1 < len && pdf[pos + 1] == '<') {
		pos += 2;
		while (pos + 2 < len && (pdf[pos] != '>' || pdf[pos + 1] != '>')) {
			pos += pdf_len(pdf, len, pos);
			pos += pdf_len(pdf, len, pos);
			pos += pdf_ws(pdf, len, pos);
		}
		if (pos + 2 < len)
			pos += 2;
	} else if (c == '<') {
		while (pos < len && pdf[pos] != '>')
			pos++;
		if (pos < len)
			pos++;
	}
	if (c == '/') {
		pos++;
		while (pos < len && !strchr(" \t\r\n\f()<>[]{}/%",
					(unsigned char) pdf[pos]))
			pos++;
	}
	if (c == '[') {
		pos++;
		while (pos < len && pdf[pos] != ']') {
			pos += pdf_len(pdf, len, pos);
			pos += pdf_ws(pdf, len, pos);
		}
		pos++;
	}
	return pos - old;
}

static int startswith(char *s, char *t)
{
	while (*s && *t)
		if (*s++ != *t++)
			return 0;
	return 1;
}

/* read an indirect reference */
int pdf_obj(char *pdf, int len, int pos, int *obj, int *rev)
{
	if (pdf_type(pdf, len, pos) != 'r')
		return -1;
	*obj = atoi(pdf + pos);
	pos += pdf_len(pdf, len, pos);
	*rev = atoi(pdf + pos);
	return 0;
}

/* the value of a pdf dictionary key */
int pdf_dval(char *pdf, int len, int pos, char *key)
{
	pos += 2;
	while (pos + 2 < len && (pdf[pos] != '>' || pdf[pos + 1] != '>')) {
		pos += pdf_ws(pdf, len, pos);
		if (pdf_len(pdf, len, pos) == strlen(key) && startswith(key, pdf + pos)) {
			pos += pdf_len(pdf, len, pos);
			pos += pdf_ws(pdf, len, pos);
			return pos;
		}
		pos += pdf_len(pdf, len, pos);
		pos += pdf_len(pdf, len, pos);
		pos += pdf_ws(pdf, len, pos);
	}
	return -1;
}

/* return a dictionary key */
int pdf_dkey(char *pdf, int len, int pos, int key)
{
	int i = 0;
	pos += 2;
	while (pos + 2 < len && (pdf[pos] != '>' || pdf[pos + 1] != '>')) {
		pos += pdf_ws(pdf, len, pos);
		if (i++ == key)
			return pos;
		pos += pdf_len(pdf, len, pos);
		pos += pdf_len(pdf, len, pos);
		pos += pdf_ws(pdf, len, pos);
	}
	return -1;
}

/* return a list entry */
int pdf_lval(char *pdf, int len, int pos, int idx)
{
	int i = 0;
	pos++;
	while (pos < len && pdf[pos] != ']') {
		if (i++ == idx)
			return pos;
		pos += pdf_len(pdf, len, pos);
		pos += pdf_ws(pdf, len, pos);
	}
	return -1;
}

static void *my_memrchr(void *m, int c, long n)
{
	int i;
	for (i = 0; i < n; i++)
		if (*(unsigned char *) (m + n - 1 - i) == c)
			return m + n - 1 - i;
	return NULL;
}

static int prevline(char *pdf, int len, int off)
{
	char *nl = my_memrchr(pdf, '\n', off);
	if (nl && nl > pdf) {
		char *nl2 = my_memrchr(pdf, '\n', nl - pdf - 1);
		if (nl2)
			return nl2 - pdf + 1;
	}
	return -1;
}

static int nextline(char *pdf, int len, int off)
{
	char *nl = memchr(pdf + off, '\n', len - off);
	if (nl)
		return nl - pdf + 1;
	return -1;
}

/* the position of the trailer */
int pdf_trailer(char *pdf, int len)
{
	int pos = prevline(pdf, len, len);		/* %%EOF */
	while (!startswith(pdf + pos, "trailer"))
		if ((pos = prevline(pdf, len, pos)) < 0)
			return -1;
	return nextline(pdf, len, pos);			/* skip trailer\n */
}

/* the position of the last xref table */
static int pdf_xref(char *pdf, int len)
{
	int pos = prevline(pdf, len, len);		/* %%EOF */
	if ((pos = prevline(pdf, len, pos)) < 0)
		return -1;
	/* read startxref offset */
	if (sscanf(pdf + pos, "%d", &pos) != 1 || pos >= len || pos < 0)
		return -1;
	return nextline(pdf, len, pos);			/* skip xref\n */
}

/* find a pdf object */
int pdf_find(char *pdf, int len, int obj, int rev)
{
	int obj_beg, obj_cnt;
	int cur_rev, cur_pos;
	char *beg;
	int i;
	int pos = pdf_xref(pdf, len);
	if (pos < 0)
		return -1;
	/* the numbers after xref */
	while (pos < len && sscanf(pdf + pos, "%d %d", &obj_beg, &obj_cnt) == 2) {
		for (i = 0; i < obj_cnt; i++) {
			if ((pos = nextline(pdf, len, pos)) < 0)
				return -1;
			if (sscanf(pdf + pos, "%d %d", &cur_pos, &cur_rev) != 2)
				return -1;
			if (obj_beg + i == obj && cur_rev == rev) {
				if (cur_pos < 0 || cur_pos >= len)
					return -1;
				if (!(beg = strstr(pdf + cur_pos, "obj")))
					return -1;
				pos = beg - pdf + 3;
				pos += pdf_ws(pdf, len, pos);
				return pos;
			}
		}
	}
	return -1;
}

/* read and dereference an indirect reference */
int pdf_ref(char *pdf, int len, int pos)
{
	int obj, rev;
	if (pdf_obj(pdf, len, pos, &obj, &rev))
		return -1;
	return pdf_find(pdf, len, obj, rev);
}

/* retrieve and dereference a dictionary entry */
int pdf_dval_val(char *pdf, int len, int pos, char *key)
{
	int val = pdf_dval(pdf, len, pos, key);
	int val_obj, val_rev;
	if (val < 0)
		return -1;
	if (pdf_type(pdf, len, val) == 'r') {
		pdf_obj(pdf, len, val, &val_obj, &val_rev);
		return pdf_find(pdf, len, val_obj, val_rev);
	}
	return val;
}

/* retrieve a dictionary entry, which is an indirect reference */
int pdf_dval_obj(char *pdf, int len, int pos, char *key)
{
	int val = pdf_dval(pdf, len, pos, key);
	if (val < 0)
		return -1;
	return pdf_ref(pdf, len, val);
}
