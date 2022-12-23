/*
 * CAUTION!	This program is just an incompletely implemented version
 *		of the patch manager daemon for GUS. Using this program
 *		with the driver version 1.99.9 will hang your system
 *		completely (sooner or later).
 *
 *		This program is for information only. The final
 *		implementation of the patch manager will not be
 *		compatible with this one.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <sys/ultrasound.h>
#include <strings.h>
#include <sys/errno.h>
#include "gmidi.h"

#ifndef PATCH_PATH
#define PATCH_PATH "/D/ultrasnd/midi"
#endif

char            loadmap[256] =
{0};				/* 1 if the patch is already loaded */

struct pat_header
  {
    char            magic[12];
    char            version[10];
    char            description[60];
    unsigned char   instruments;
    char            voices;
    char            channels;
    unsigned short  nr_waveforms;
    unsigned short  master_volume;
    unsigned long   data_size;
  };

struct sample_header
  {
    char            name[7];
    unsigned char   fractions;
    long            len;
    long            loop_start;
    long            loop_end;
    unsigned short  base_freq;
    long            low_note;
    long            high_note;
    long            base_note;
    short           detune;
    unsigned char   panning;

    unsigned char   envelope_rate[6];
    unsigned char   envelope_offset[6];

    unsigned char   tremolo_sweep;
    unsigned char   tremolo_rate;
    unsigned char   tremolo_depth;

    unsigned char   vibrato_sweep;
    unsigned char   vibrato_rate;
    unsigned char   vibrato_depth;

    char            modes;

    short           scale_frequency;
    unsigned short  scale_factor;
  };
int             seqfd = 0, gus_dev = -1;

struct patch_info *patch;

int
do_load_patch (struct patmgr_info *rec)
{
  int             i, patfd, pgm, print_only = 0;
  struct pat_header header;
  struct sample_header sample;
  char            buf[256];
  char            name[256];
  long            offset;

  pgm = rec->data.data8[0];

  if (loadmap[pgm])
    return 0;			/* Already loaded */

  sprintf (name, PATCH_PATH "/%s.pat", patch_names[pgm]);

  if ((patfd = open (name, O_RDONLY, 0)) == -1)
    {
      perror (name);
      return errno;
    }

  if (read (patfd, buf, 0xef) != 0xef)
    {
      fprintf (stderr, "%s: Short file\n", name);
      return EIO;
    }

  memcpy ((char *) &header, buf, sizeof (header));

  if (strncmp (header.magic, "GF1PATCH110", 12))
    {
      fprintf (stderr, "%s: Not a patch file\n", name);
      return EINVAL;
    }

  if (strncmp (header.version, "ID#000002", 10))
    {
      fprintf (stderr, "%s: Incompatible patch file version\n", name);
      return EINVAL;
    }

  header.nr_waveforms = *(unsigned short *) &buf[85];
  header.master_volume = *(unsigned short *) &buf[87];

  printf ("GUS: Loading: %s\n", name);

  offset = 0xef;

  for (i = 0; i < header.nr_waveforms; i++)
    {
      if (lseek (patfd, offset, 0) == -1)
	{
	  perror (name);
	  return errno;
	}

      if (read (patfd, &buf, sizeof (sample)) != sizeof (sample))
	{
	  fprintf (stderr, "%s: Short file\n", name);
	  return EIO;
	}

      memcpy ((char *) &sample, buf, sizeof (sample));

      /*
       * Since some fields of the patch record are not 32bit aligned, we must
       * handle them specially.
       */
      sample.low_note = *(long *) &buf[22];
      sample.high_note = *(long *) &buf[26];
      sample.base_note = *(long *) &buf[30];
      sample.detune = *(short *) &buf[34];
      sample.panning = (unsigned char) buf[36];

      memcpy (sample.envelope_rate, &buf[37], 6);
      memcpy (sample.envelope_offset, &buf[43], 6);

      sample.tremolo_sweep = (unsigned char) buf[49];
      sample.tremolo_rate = (unsigned char) buf[50];
      sample.tremolo_depth = (unsigned char) buf[51];

      sample.vibrato_sweep = (unsigned char) buf[52];
      sample.vibrato_rate = (unsigned char) buf[53];
      sample.vibrato_depth = (unsigned char) buf[54];
      sample.modes = (unsigned char) buf[55];
      sample.scale_frequency = *(short *) &buf[56];
      sample.scale_factor = *(unsigned short *) &buf[58];

      if (print_only)
	{
	  printf ("\nSample: %03d / %s\n", i, sample.name);
	  printf ("Len: %d, Loop start: %d, Loop end: %d\n", sample.len, sample.loop_start, sample.loop_end);
	  printf ("Flags: ");
	  if (sample.modes & WAVE_16_BITS)
	    printf ("16 bit ");
	  if (sample.modes & WAVE_UNSIGNED)
	    printf ("unsigned ");
	  if (sample.modes & WAVE_LOOP_BACK)
	    printf ("reverse ");
	  if (sample.modes & WAVE_BIDIR_LOOP)
	    printf ("bidir ");
	  if (sample.modes & WAVE_LOOPING)
	    printf ("looping ");
	  else
	    printf ("one_shot");
	  if (sample.modes & WAVE_SUSTAIN_ON)
	    printf ("sustain ");
	  if (sample.modes & WAVE_ENVELOPES)
	    printf ("enveloped ");
	  printf ("\n");

	  if (sample.modes & WAVE_ENVELOPES)
	    {
	      int             i;

	      printf ("Envelope info: ");
	      for (i = 0; i < 6; i++)
		{
		  printf ("%d/%d ", sample.envelope_rate[i],
			  sample.envelope_offset[i]);
		}
	      printf ("\n");
	    }

	  printf ("Tremolo: sweep=%d, rate=%d, depth=%d\n",
		  sample.tremolo_sweep,
		  sample.tremolo_rate,
		  sample.tremolo_depth);

	  printf ("Vibrato: sweep=%d, rate=%d, depth=%d\n",
		  sample.vibrato_sweep,
		  sample.vibrato_rate,
		  sample.vibrato_depth);
	}

      offset = offset + 96;
      patch = (struct patch_info *) malloc (sizeof (*patch) + sample.len);

      patch->key = GUS_PATCH;
      patch->device_no = gus_dev;
      patch->instr_no = pgm;
      patch->mode = sample.modes | WAVE_TREMOLO |
	WAVE_VIBRATO | WAVE_SCALE;
      patch->len = sample.len;
      patch->loop_start = sample.loop_start;
      patch->loop_end = sample.loop_end;
      patch->base_note = sample.base_note;
      patch->high_note = sample.high_note;
      patch->low_note = sample.low_note;
      patch->base_freq = sample.base_freq;
      patch->detuning = sample.detune;
      patch->panning = (sample.panning - 7) * 16;

      memcpy (patch->env_rate, sample.envelope_rate, 6);
      memcpy (patch->env_offset, sample.envelope_offset, 6);

      patch->tremolo_sweep = sample.tremolo_sweep;
      patch->tremolo_rate = sample.tremolo_rate;
      patch->tremolo_depth = sample.tremolo_depth;

      patch->vibrato_sweep = sample.vibrato_sweep;
      patch->vibrato_rate = sample.vibrato_rate;
      patch->vibrato_depth = sample.vibrato_depth;

      patch->scale_frequency = sample.scale_frequency;
      patch->scale_factor = sample.scale_factor;

      patch->volume = header.master_volume;

      if (lseek (patfd, offset, 0) == -1)
	{
	  perror (name);
	  return errno;
	}

      if (!print_only)
	{
	  if (read (patfd, patch->data, sample.len) != sample.len)
	    {
	      fprintf (stderr, "%s: Short file\n", name);
	      return EIO;
	    }

	  if (write (seqfd, patch, sizeof (*patch) + sample.len) == -1)
	    {
	      perror ("/dev/pmgr0");
	      return errno;
	    }
	}

      offset = offset + sample.len;
    }

  loadmap[pgm] = 1;
  return 0;
}

int
main (int argc, char *argv[])
{
  struct patmgr_info inf;
  int             err, i, n;
  struct synth_info info;

  if ((seqfd = open ("/dev/patmgr0", O_RDWR, 0)) == -1)
    {
      fprintf (stderr, "Cannot open\n");
      perror ("/dev/patmgr0");
      exit (-1);
    }

  if (ioctl (seqfd, SNDCTL_SEQ_NRSYNTHS, &n) == -1)
    {
      perror ("NRSYNTH: /dev/patmgr0");
      exit (-1);
    }

  for (i = 0; i < n; i++)
    {
      info.device = i;

      if (ioctl (seqfd, SNDCTL_SYNTH_INFO, &info) == -1)
	{
	  perror ("SYNTH_INFO: /dev/patmgr0");
	  exit (-1);
	}

      if (info.synth_type == SYNTH_TYPE_SAMPLE
	  && info.synth_subtype == SAMPLE_TYPE_GUS)
	gus_dev = i;
    }

  if (gus_dev == -1)
    {
      fprintf (stderr, "Error: Gravis Ultrasound not detected\n");
      exit (-1);
    }

  if (ioctl (seqfd, SNDCTL_SEQ_RESETSAMPLES, &gus_dev) == -1)
    perror ("Sample reset");

  for (i = 0; i < 256; i++)
    loadmap[i] = 0;

  while (1)
    {
      if (read (seqfd, (char *) &inf, sizeof (inf)) != sizeof (inf))
	{
	  perror ("Read");
	  exit (-1);
	}

      if (inf.key == PM_K_EVENT)
	switch (inf.command)
	  {
	  case PM_E_OPENED:
	    printf ("Opened\n");
	    break;

	  case PM_E_CLOSED:
	    printf ("Closed\n");
	    if (ioctl (seqfd, SNDCTL_SEQ_RESETSAMPLES, &gus_dev) == -1)
	      perror ("Sample reset");
	    for (i = 0; i < 256; i++)
	      loadmap[i] = 0;
	    break;

	  case PM_E_PATCH_RESET:
	    printf ("Patch reset called\n");
	    for (i = 0; i < 256; i++)
	      loadmap[i] = 0;
	    break;

	  case PM_E_PATCH_LOADED:
	    printf ("Patch loaded by client\n");
	    break;

	  default:
	    printf ("Unknown event %d\n", inf.command);
	    inf.key = PM_ERROR;
	    inf.parm1 = EINVAL;
	  }
      else if (inf.key == PM_K_COMMAND)
	switch (inf.command)
	  {
	  case _PM_LOAD_PATCH:
	    if ((err = do_load_patch (&inf)))
	      if (err == ENOSPC)
		{
		  if (ioctl (seqfd, SNDCTL_SEQ_RESETSAMPLES, &gus_dev) == -1)
		    {
		      perror ("Sample reset");
		      return errno;
		    }

		  for (i = 0; i < 256; i++)
		    loadmap[i] = 0;
		  err = do_load_patch (&inf);
		}

	    if (err)
	      {
		inf.key = PM_ERROR;
		inf.parm1 = err;
		printf("Error = %d\n", err);
	      }
	    else
	     {
	        inf.key = PM_K_COMMAND;
	        inf.parm1 = 0;
	     }
	    break;

	  default:
	    printf ("Unknown command %d\n", inf.command);
	    inf.key = PM_ERROR;
	    inf.parm1 = EINVAL;
	  }
      else
	{
	  printf ("Unknown event %d/%d\n", inf.key, inf.command);
	  inf.key = PM_ERROR;
	  inf.parm1 = EINVAL;
	}

      if (write (seqfd, (char *) &inf, sizeof (inf)) != sizeof (inf))
	{
	  perror ("write");
	  exit (-1);
	}
    }

  exit (0);
}
