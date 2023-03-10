/*
 * linux/kernel/chr_drv/sound/audio.c
 * 
 * Device file manager for /dev/audio
 * 
 * (C) 1992  Hannu Savolainen (hsavolai@cs.helsinki.fi) See COPYING for further
 * details. Should be distributed with this file.
 */

#include "sound_config.h"

#ifdef CONFIGURE_SOUNDCARD
#ifndef EXCLUDE_AUDIO

#include "ulaw.h"

#define ON		1
#define OFF		0

static int      wr_buff_no[MAX_DSP_DEV];	/* != -1, if there is a
						 * incomplete output block */
static int      wr_buff_size[MAX_DSP_DEV], wr_buff_ptr[MAX_DSP_DEV];
static char    *wr_dma_buf[MAX_DSP_DEV];

int
audio_open (int dev, struct fileinfo *file)
{
  int             mode;
  int             ret;

  dev = dev >> 4;
  mode = file->mode & O_ACCMODE;

  if ((ret = DMAbuf_open (dev, mode)) < 0)
    return ret;

  wr_buff_no[dev] = -1;
  return ret;
}

void
audio_release (int dev, struct fileinfo *file)
{
  int             mode;

  dev = dev >> 4;
  mode = file->mode & O_ACCMODE;

  if (wr_buff_no[dev] >= 0)
    {
      DMAbuf_start_output (dev, wr_buff_no[dev], wr_buff_ptr[dev]);

      wr_buff_no[dev] = -1;
    }

  DMAbuf_release (dev, mode);
}

#ifdef NO_INLINE_ASM
static void
translate_bytes (const unsigned char *table, unsigned char *buff, unsigned long n)
{
  unsigned long   i;

  for (i = 0; i < n; ++i)
    buff[i] = table[buff[i]];
}

#else
extern inline void
translate_bytes (const void *table, void *buff, unsigned long n)
{
  __asm__ ("cld\n"
	   "1:\tlodsb\n\t"
	   "xlatb\n\t"
	   "stosb\n\t"
	   "loop 1b\n\t":
	   :"b" ((long) table), "c" (n), "D" ((long) buff), "S" ((long) buff)
	   :"bx", "cx", "di", "si", "ax");
}

#endif

int
audio_write (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  int             c, p, l;
  int             err;

  dev = dev >> 4;

  p = 0;
  c = count;

  if (!count)			/* Flush output */
    {
      if (wr_buff_no[dev] >= 0)
	{
	  DMAbuf_start_output (dev, wr_buff_no[dev], wr_buff_ptr[dev]);

	  wr_buff_no[dev] = -1;
	}
      return 0;
    }

  while (c)
    {				/* Perform output blocking */
      if (wr_buff_no[dev] < 0)	/* There is no incomplete buffers */
	{
	  if ((wr_buff_no[dev] = DMAbuf_getwrbuffer (dev, &wr_dma_buf[dev], &wr_buff_size[dev])) < 0)
	    return wr_buff_no[dev];
	  wr_buff_ptr[dev] = 0;
	}

      l = c;
      if (l > (wr_buff_size[dev] - wr_buff_ptr[dev]))
	l = (wr_buff_size[dev] - wr_buff_ptr[dev]);

      COPY_FROM_USER (&wr_dma_buf[dev][wr_buff_ptr[dev]], buf, p, l);

      /* Insert local processing here */

#ifdef linux
      /* This just allows interrupts while the conversion is running */
      __asm__ ("sti");
#endif
      translate_bytes (ulaw_dsp, &wr_dma_buf[dev][wr_buff_ptr[dev]], l);

      c -= l;
      p += l;
      wr_buff_ptr[dev] += l;

      if (wr_buff_ptr[dev] >= wr_buff_size[dev])
	{
	  if ((err = DMAbuf_start_output (dev, wr_buff_no[dev], wr_buff_ptr[dev])) < 0)
	    return err;

	  wr_buff_no[dev] = -1;
	}

    }

  return count;
}

int
audio_read (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  int             c, p, l;
  char           *dmabuf;
  int             buff_no;

  dev = dev >> 4;
  p = 0;
  c = count;

  while (c)
    {
      if ((buff_no = DMAbuf_getrdbuffer (dev, &dmabuf, &l)) < 0)
	return buff_no;

      if (l > c)
	l = c;

      /* Insert any local processing here. */
#ifdef linux
      /* This just allows interrupts while the conversion is running */
      __asm__ ("sti");
#endif

      translate_bytes (dsp_ulaw, dmabuf, l);

      COPY_TO_USER (buf, p, dmabuf, l);

      DMAbuf_rmchars (dev, buff_no, l);

      p += l;
      c -= l;
    }

  return count - c;
}

int
audio_ioctl (int dev, struct fileinfo *file,
	     unsigned int cmd, unsigned int arg)
{
  dev = dev >> 4;

  switch (cmd)
    {
    case SNDCTL_DSP_SYNC:
      if (wr_buff_no[dev] >= 0)
	{
	  DMAbuf_start_output (dev, wr_buff_no[dev], wr_buff_ptr[dev]);

	  wr_buff_no[dev] = -1;
	}
      return DMAbuf_ioctl (dev, cmd, arg, 0);
      break;

    case SNDCTL_DSP_POST:
      if (wr_buff_no[dev] >= 0)
	{
	  DMAbuf_start_output (dev, wr_buff_no[dev], wr_buff_ptr[dev]);

	  wr_buff_no[dev] = -1;
	}
      return 0;
      break;

    case SNDCTL_DSP_RESET:
      wr_buff_no[dev] = -1;
      return DMAbuf_ioctl (dev, cmd, arg, 0);
      break;

    default:
#if 1
      return RET_ERROR (EIO);
#else
      return DMAbuf_ioctl (dev, cmd, arg, 0);
#endif
    }
}

long
audio_init (long mem_start)
{
  return mem_start;
}

#else
/* Stub versions */

int
audio_read (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  return RET_ERROR (EIO);
}

int
audio_write (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  return RET_ERROR (EIO);
}

int
audio_open (int dev, struct fileinfo *file)
  {
    return RET_ERROR (ENXIO);
  }

void
audio_release (int dev, struct fileinfo *file)
  {
  };
int
audio_ioctl (int dev, struct fileinfo *file,
	     unsigned int cmd, unsigned int arg)
{
  return RET_ERROR (EIO);
}

int
audio_lseek (int dev, struct fileinfo *file, off_t offset, int orig)
{
  return RET_ERROR (EIO);
}

long
audio_init (long mem_start)
{
  return mem_start;
}

#endif

#endif
