/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

/*
 * From http://www.tscc.de/ucm24/tcb2pro.html:
 * There are two different TCB-Tracker module formats. Older format and
 * newer format. They have different headers "AN COOL." and "AN COOL!".
 *
 * We only support the old format --claudio
 */

#include "loader.h"


static int tcb_test(HANDLE *, char *, const int);
static int tcb_load (struct module_data *, HANDLE *, const int);

const struct format_loader tcb_loader = {
	"TCB Tracker",
	tcb_test,
	tcb_load
};

static int tcb_test(HANDLE *f, char *t, const int start)
{
	uint8 buffer[10];

	if (hread(buffer, 1, 8, f) < 8)
		return -1;
	if (memcmp(buffer, "AN COOL.", 8) && memcmp(buffer, "AN COOL!", 8))
		return -1;

	read_title(f, t, 0);

	return 0;
}

static int tcb_load(struct module_data *m, HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_event *event;
	int i, j, k;
	uint8 buffer[10];
	int base_offs, soffs[16];
	uint8 unk1[16], unk2[16], unk3[16];

	LOAD_INIT();

	hread(buffer, 8, 1, f);

	set_type(m, "TCB Tracker", buffer);

	hread_16b(f);	/* ? */
	mod->pat = hread_16b(f);
	mod->ins = 16;
	mod->smp = mod->ins;
	mod->chn = 4;
	mod->trk = mod->pat * mod->chn;

	m->quirk |= QUIRK_MODRNG;

	hread_16b(f);	/* ? */

	for (i = 0; i < 128; i++)
		mod->xxo[i] = hread_8(f);

	mod->len = hread_8(f);
	hread_8(f);	/* ? */
	hread_16b(f);	/* ? */

	MODULE_INFO();

	INSTRUMENT_INIT();

	/* Read instrument names */
	for (i = 0; i < mod->ins; i++) {
		mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);
		hread(buffer, 8, 1, f);
		copy_adjust(mod->xxi[i].name, buffer, 8);
	}

	hread_16b(f);	/* ? */
	for (i = 0; i < 5; i++)
		hread_16b(f);
	for (i = 0; i < 5; i++)
		hread_16b(f);
	for (i = 0; i < 5; i++)
		hread_16b(f);

	PATTERN_INIT();

	/* Read and convert patterns */
	D_(D_INFO "Stored patterns: %d ", mod->pat);

	for (i = 0; i < mod->pat; i++) {
		PATTERN_ALLOC(i);
		mod->xxp[i]->rows = 64;
		TRACK_ALLOC(i);

		for (j = 0; j < mod->xxp[i]->rows; j++) {
			for (k = 0; k < mod->chn; k++) {
				int b;
				event = &EVENT (i, k, j);

				b = hread_8(f);
				if (b) {
					event->note = 12 * (b >> 4);
					event->note += (b & 0xf) + 36;
				}
				b = hread_8(f);
				event->ins = b >> 4;
				if (event->ins)
					event->ins += 1;
				if (b &= 0x0f) {
					switch (b) {
					case 0xd:
						event->fxt = FX_BREAK;
						event->fxp = 0;
						break;
					default:
						printf("---> %02x\n", b);
					}
				}
			}
		}
	}

	base_offs = htell(f);
	hread_32b(f);	/* remaining size */

	/* Read instrument data */

	for (i = 0; i < mod->ins; i++) {
		mod->xxi[i].sub[0].vol = hread_8(f) / 2;
		mod->xxi[i].sub[0].pan = 0x80;
		unk1[i] = hread_8(f);
		unk2[i] = hread_8(f);
		unk3[i] = hread_8(f);
	}


	for (i = 0; i < mod->ins; i++) {
		soffs[i] = hread_32b(f);
		mod->xxs[i].len = hread_32b(f);
	}

	hread_32b(f);
	hread_32b(f);
	hread_32b(f);
	hread_32b(f);

	for (i = 0; i < mod->ins; i++) {
		mod->xxi[i].nsm = !!(mod->xxs[i].len);
		mod->xxs[i].lps = 0;
		mod->xxs[i].lpe = 0;
		mod->xxs[i].flg = mod->xxs[i].lpe > 0 ? XMP_SAMPLE_LOOP : 0;
		mod->xxi[i].sub[0].fin = 0;
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].sid = i;

		D_(D_INFO "[%2X] %-8.8s  %04x %04x %04x %c "
						"V%02x  %02x %02x %02x\n",
				i, mod->xxi[i].name,
				mod->xxs[i].len, mod->xxs[i].lps, mod->xxs[i].lpe,
				mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
				mod->xxi[i].sub[0].vol, unk1[i], unk2[i], unk3[i]);
	}

	/* Read samples */

	D_(D_INFO "Stored samples: %d", mod->smp);

	for (i = 0; i < mod->ins; i++) {
		hseek(f, start + base_offs + soffs[i], SEEK_SET);
		load_sample(m, f, SAMPLE_FLAG_UNS, &mod->xxs[mod->xxi[i].sub[0].sid], NULL);
	}

	return 0;
}
