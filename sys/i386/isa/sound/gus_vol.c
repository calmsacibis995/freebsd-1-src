/*
 * gus_vol.c - Compute volume for GUS.
 * 
 * Greg Lee 1993.
 */
#include "sound_config.h"
#ifndef EXCLUDE_GUS

#define GUS_VOLUME	gus_wave_volume


extern int      gus_wave_volume;

/*
 * Calculate gus volume from note velocity, main volume, expression, and
 * intrinsic patch volume given in patch library.  Expression is multiplied
 * in, so it emphasizes differences in note velocity, while main volume is
 * added in -- I don't know whether this is right, but it seems reasonable to
 * me.  (In the previous stage, main volume controller messages were changed
 * to expression controller messages, if they were found to be used for
 * dynamic volume adjustments, so here, main volume can be assumed to be
 * constant throughout a song.)
 * 
 * Intrinsic patch volume is added in, but if over 64 is also multiplied in, so
 * we can give a big boost to very weak voices like nylon guitar and the
 * basses.  The normal value is 64.  Strings are assigned lower values.
 */
unsigned short
gus_adagio_vol (int vel, int mainv, int xpn, int voicev)
{
  int             i, m, n, x;


  /*
   * A voice volume of 64 is considered neutral, so adjust the main volume if
   * something other than this neutral value was assigned in the patch
   * library.
   */
  x = 256 + 6 * (voicev - 64);

  /* Boost expression by voice volume above neutral. */
  if (voicev > 65)
    xpn += voicev - 64;
  xpn += (voicev - 64) / 2;

  /* Combine multiplicative and level components. */
  x = vel * xpn * 6 + (voicev / 4) * x;

#ifdef GUS_VOLUME
  /*
   * Further adjustment by installation-specific master volume control
   * (default 50).
   */
  x = (x * GUS_VOLUME * GUS_VOLUME) / 10000;
#endif

  if (x < (1 << 11))
    return (11 << 8);
  else if (x >= 65535)
    return ((15 << 8) | 255);

  /*
   * Convert to gus's logarithmic form with 4 bit exponent i and 8 bit
   * mantissa m.
   */
  n = x;
  i = 7;
  if (n < 128)
    {
      while (i > 0 && n < (1 << i))
	i--;
    }
  else
    while (n > 255)
      {
	n >>= 1;
	i++;
      }
  /*
   * Mantissa is part of linear volume not expressed in exponent.  (This is
   * not quite like real logs -- I wonder if it's right.)
   */
  m = x - (1 << i);

  /* Adjust mantissa to 8 bits. */
  if (m > 0)
    {
      if (i > 8)
	m >>= i - 8;
      else if (i < 8)
	m <<= 8 - i;
    }

  /* low volumes give occasional sour notes */
  if (i < 11)
    return (11 << 8);

  return ((i << 8) + m);
}

#endif
