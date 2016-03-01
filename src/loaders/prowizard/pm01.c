/*
 * Promizer_0.1_Packer.c Copyright (C) 1997 Asle / ReDoX
 *
 * Converts back to ptk Promizer 0.1 packed MODs
 *
 * Modified in 2016 by Claudio Matsuoka
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"

static int depack_pm01(HIO_HANDLE *in, FILE *out)
{
	uint8 ptable[128];
	uint8 len;
	uint8 npat;
	uint8 tmp[1024];
	uint8 pdata[1024];
	uint8 fin[31];
	uint8 Old_ins_Nbr[4];
	int i, j;
	int psize, size, ssize = 0;
	int pat_ofs[128];

	memset(ptable, 0, 128);
	memset(pat_ofs, 0, 128 * 4);
	memset(fin, 0, 31);
	memset(Old_ins_Nbr, 0, 4);

	pw_write_zero(out, 20);			/* title */

	/* read and write sample descriptions */
	for (i = 0; i < 31; i++) {

		if (hio_read(tmp, 1, 8, in) != 8) {
			return -1;
		}

		pw_write_zero(out, 22);			/* sample name */

		size = readmem16b(tmp);			/* size */
		ssize += size * 2;

		fin[i] = tmp[2];

		if (tmp[4] == 0 && tmp[5] == 0) {	/* loop size */
			tmp[5] = 1;
		}

		if (fwrite(tmp, 1, 8, out) != 8) {
			return -1;
		}
	}

	len = hio_read16b(in) >> 2;		/* pattern table lenght */
	write8(out, len);
	write8(out, 0x7f);			/* write NoiseTracker byte */

	/* read pattern address list */
	for (i = 0; i < 128; i++) {
		pat_ofs[i] = hio_read32b(in);
	}

	/* deduce pattern list and write it */
	for (npat = i = 0; i < 128; i++) {
		ptable[i] = pat_ofs[i] / 1024;
		write8(out, ptable[i]);
		if (ptable[i] > npat) {
			npat = ptable[i];
		}
	}
	npat++;

	write32b(out, PW_MOD_MAGIC);		/* ID string */

	psize = hio_read32b(in);		/* get pattern data size */

	if (npat * 1024 != psize) {
		return -1;
	}

	/* read and XOR pattern data */
	for (i = 0; i < npat; i++) {
		memset(pdata, 0, 1024);
		if (hio_read(pdata, 1, 1024, in) != 1024) {
			return -1;
		}

		for (j = 0; j < 1024; j++) {
			if (j % 4 == 3) {
				pdata[j] = (240 - (pdata[j] & 0xf0)) +
						(pdata[j] & 0x0f);
				continue;
			}
			pdata[j] = pdata[j] ^ 0xff;
		}

		if (fwrite(pdata, 1, 1024, out) != 1024) {
			return -1;
		}
	}

#if 0
	/* all right, now, let's take care of these 'finetuned' value ... pfff */
	Old_ins_Nbr[0] = Old_ins_Nbr[1] = Old_ins_Nbr[2] = Old_ins_Nbr[3] =
		0x1f;
	memset(tmp, 0, j);
	for (i = 0; i < j / 4; i++) {
		c1 = pdata[i * 4] & 0x0f;
		c2 = pdata[i * 4 + 1];
		k = (c1 << 8) + c2;
		c3 =
			(pdata[i * 4] & 0xf0) | ((pdata[i * 4 +
					   2] >> 4) & 0x0f);
		if (c3 == 0)
			c3 = Old_ins_Nbr[i % 4];
		else
			Old_ins_Nbr[i % 4] = c3;
		if ((k != 0) && (fin[c3 - 1] != 0x00)) {
/*fprintf ( info , "! (at %ld)(smp:%x)(pitch:%ld)\n" , (i*4)+382 , c3 , k );*/
			for (l = 0; l < 36; l++) {
				if (k == tun_table[fin[c3 - 1]][l]) {
					tmp[i * 4] = ptk_table[l + 1][0];
					tmp[i * 4 + 1] = ptk_table[l + 1][1];
				}
			}
		} else {
			tmp[i * 4] = pdata[i * 4] & 0x0f;
			tmp[i * 4 + 1] = pdata[i * 4 + 1];
		}
		tmp[i * 4] |= (pdata[i * 4] & 0xf0);
		tmp[i * 4 + 2] = pdata[i * 4 + 2];
		tmp[i * 4 + 3] = pdata[i * 4 + 3];
	}
#endif

	/* sample data */
	pw_move_data(out, in, ssize);

	return 0;
}


static int test_pm01(uint8 *data, char *t, int s)
{
	int i;
	int len, psize, ssize;

	PW_REQUEST_DATA(s, 1024);

#if 0
	/* test #1 */
	if (i < 3) {
		Test = BAD;
		return;
	}
#endif

	/* test #2 */
	for (i = 0; i < 31; i++) {
		uint8 *d = data + i * 8;
		int size = readmem16b(data) << 1;
		int start = readmem16b(data + 4) << 1;
		int lsize = readmem16b(data + 6) << 1;

		ssize += size;

		if (d[2] > 0x0f) {		/* finetune > 0x0f ? */
			return -1;
		}

		/* loop start > size ? */
		if (start > size || lsize > size) {
			return -1;
		}

		if (lsize <= 2) {
			return -1;
		}
	}

	/* test #3   about size of pattern list */
	len = readmem16b(data + 248);
	if (len & 0x03) {
		return -1;
	}
	len >>= 2;

	if (len == 0 || len > 127) {
		return -1;
	}

	/* test #4  size of all the pattern data */
	/* k contains the size of the pattern list */
	psize = readmem32b(data + 762);
	if (psize < 1024 || psize > 131072) {
		return -1;
	}

	/* test #5  first pattern address != $00000000 ? */
	if (readmem32b(data + 250) != 0) {
		return -1;
	}

	/* test #6  pattern addresses */
	for (i = 0; i < len; i++) {
		int addr = readmem32b(data + 250 + i * 4);
		if (addr & 0x3ff || addr > 131072) {
			return -1;
		}
	}

	/* test #7  last patterns in pattern table != $00000000 ? */
	i += 4;		/* just to be sure */
	for (; i < 128; i++) {
		int addr = readmem32b(data + 250 + i * 4);
		if (addr != 0) {
			return -1;
		}
	}

	return 0;
}

const struct pw_format pw_pm01 = {
	"Promizer 0.1",
	test_pm01,
	depack_pm01
};

