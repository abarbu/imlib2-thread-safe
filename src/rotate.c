#include "common.h"
#include "rotate.h"
#include "blend.h"

#define RENORM_X_Y_SRC \
src += (x >> _ROTATE_PREC) + ((y >> _ROTATE_PREC) * sow); \
x &= _ROTATE_PREC_BITS; y &= _ROTATE_PREC_BITS;

/*\ Linear interpolation functions \*/
/*\ Between two values \*/
#define INTERP(v1, v2, f) \
	((v1) * (_ROTATE_PREC_MAX - (f)) + (v2) * (f))

/*\ Between two colour bytes \*/
#define INTERP_VAL1(x_VAL, dest, l, r, x) \
	x_VAL(dest) = (INTERP(x_VAL(l), x_VAL(r), (x)) >> _ROTATE_PREC)

/*\ Alpha channel: between two values and two zeroes \*/
#define INTERP_VAL1_A0(dest, v1, v2, f1, f2) \
	A_VAL(dest) = ((INTERP(A_VAL(v1), A_VAL(v2), (f1)) *	\
			(f2)) >> (2 * _ROTATE_PREC))

/*\ Between four values \*/
#define INTERP_VAL2(x_VAL, dest, ul, ur, ll, lr, x, y) \
	x_VAL(dest) = (INTERP(INTERP(x_VAL(ul), x_VAL(ur), (x)),	\
			      INTERP(x_VAL(ll), x_VAL(lr), (x)),	\
			     (y)) >> (2 * _ROTATE_PREC))

/*\ Functions used in rotation routines.
|*| The do { } while(0) construction is to make it one statement.
\*/
/*\ Between four colours \*/
#define INTERP_ARGB(dest, ul, ur, ll, lr, x, y) do { \
	INTERP_VAL2(R_VAL, dest, ul, ur, ll, lr, x, y);	\
	INTERP_VAL2(G_VAL, dest, ul, ur, ll, lr, x, y);	\
	INTERP_VAL2(B_VAL, dest, ul, ur, ll, lr, x, y);	\
	INTERP_VAL2(A_VAL, dest, ul, ur, ll, lr, x, y);	\
	} while (0)

/*\ Between two colours, alpha between two values and zeroes \*/
#define INTERP_RGB_A0(dest, v1, v2, f, f2) do { \
	INTERP_VAL1(R_VAL, dest, v1, v2, f);	\
	INTERP_VAL1(G_VAL, dest, v1, v2, f);	\
	INTERP_VAL1(B_VAL, dest, v1, v2, f);	\
	INTERP_VAL1_A0(dest, v1, v2, f, f2);	\
	} while (0)

/*\ One colour, alpha between one value and three zeroes \*/
#define INTERP_A000(dest, v, f1, f2) do {	\
	dest = v;				\
	A_VAL(dest) = (A_VAL(dest) *		\
		(f1) * (f2)) >> (2 * _ROTATE_PREC);	\
	} while (0)

/*\ Rotate by pixel sampling only, target inside source \*/
static void
__imlib_RotateSampleInside(DATA32 *src, DATA32 *dest, int sow, int dow,
			   int dw, int dh, int x, int y, int dx, int dy)
{
   int i;
   
   if ((dw < 1) || (dh < 1)) return;
   
   src += (x >> _ROTATE_PREC) + ((y >> _ROTATE_PREC) * sow);
   x &= _ROTATE_PREC_BITS; y &= _ROTATE_PREC_BITS;
   while (1) {
      i = dw - 1;
      do {
	 *dest = *src;
	 /*\ RIGHT; \*/
	 x += dx;
	 y += dy;
	 RENORM_X_Y_SRC;
	 dest++;
      } while (--i >= 0);
      if (--dh <= 0) break;
      /*\ DOWN/LEFT; \*/
      x += -dy - dw * dx;
      y += dx - dw * dy;
      RENORM_X_Y_SRC;
      dest += (dow - dw);
   }
}

/*\ Same as last function, but with antialiasing \*/
static void
__imlib_RotateAAInside(DATA32 *src, DATA32 *dest, int sow, int dow,
		       int dw, int dh, int x, int y, int dx, int dy)
{
   int i;
   
   if ((dw < 1) || (dh < 1)) return;
   
   src += (x >> _ROTATE_PREC) + ((y >> _ROTATE_PREC) * sow);
   x &= _ROTATE_PREC_BITS; y &= _ROTATE_PREC_BITS;
   while (1) {
      i = dw - 1;
      do {
	 INTERP_ARGB(*dest, src[0], src[1],
		     src[sow], src[sow + 1], x, y);
	 /*\ RIGHT; \*/
	 x += dx;
	 y += dy;
	 RENORM_X_Y_SRC;
	 dest++;
      } while (--i >= 0);
      if (--dh <= 0) break;
      /*\ DOWN/LEFT; \*/
      x += -dy - dw * dx;
      y += dx - dw * dy;
      RENORM_X_Y_SRC;
      dest += (dow - dw);
   }
}

static int
__check_inside_coords(int x, int y, int dx, int dy,
		      int dw, int dh, int sow, int soh)
{
   sow <<= _ROTATE_PREC;
   soh <<= _ROTATE_PREC;
   
   if ((x < 0) || (y < 0) || (x >= sow) || (y >= soh))
      return 0;
   x += dx * dw; y += dy * dw;
   if ((x < 0) || (y < 0) || (x >= sow) || (y >= soh))
      return 0;
   x -= dy * dh; y += dx * dh;
   if ((x < 0) || (y < 0) || (x >= sow) || (y >= soh))
      return 0;
   x -= dx * dw; y -= dy * dw;
   if ((x < 0) || (y < 0) || (x >= sow) || (y >= soh))
      return 0;
   
   return 1;
}

/*\ These ones don't need the target to be inside the source \*/
void
__imlib_RotateSample(DATA32 *src, DATA32 *dest, int sow, int sw, int sh,
		     int dow, int dw, int dh, int x, int y, int dx, int dy)
{
   int i;
   DATA32 *st, *sb;
   int xp; /*\ Check if we're inside the source \*/
   
   if ((dw < 1) || (dh < 1)) return;
   
   if (__check_inside_coords(x, y, dx, dy, dw, dh, sw, sh)) {
      __imlib_RotateSampleInside(src, dest, sow, dow,
				 dw, dh, x, y, dx, dy);
      return;
      
   }
   
   st = src; sb = src + sh * sow;
   xp = x >> _ROTATE_PREC;
   src += (x >> _ROTATE_PREC) + ((y >> _ROTATE_PREC) * sow);
   x &= _ROTATE_PREC_BITS; y &= _ROTATE_PREC_BITS;
   while (1) {
      i = dw - 1;
      do {
	 if ((xp >= 0) && (xp < sw) &&
	     (src >= st) && (src < sb))
	    *dest = *src;
	 /*\ RIGHT; \*/
	 x += dx;
	 y += dy;
	 xp += (x >> _ROTATE_PREC);
	 RENORM_X_Y_SRC;
	 dest++;
	 
      } while (--i >= 0);
      if (--dh <= 0) break;
      /*\ DOWN/LEFT; \*/
      x += -dy - dw * dx;
      y += dx - dw * dy;
      xp += (x >> _ROTATE_PREC);
      RENORM_X_Y_SRC;
      dest += (dow - dw);
      
   }
}

/*\ With antialiasing.
|*| NB: The function 'sees' a transparent border around the source,
|*|     with colour channels matching the edge, so there is no need to do
|*|     anything special, but remember to account for this when calculating
|*|     the bounding box.
\*/
void
__imlib_RotateAA(DATA32 *src, DATA32 *dest, int sow, int sw, int sh,
		 int dow, int dw, int dh, int x, int y, int dx, int dy)
{
   int i;
   DATA32 *st, *sb;
   int xp; /*\ Check if we're inside the source \*/
   
   if ((dw < 1) || (dh < 1)) return;
   
   if (__check_inside_coords(x, y, dx, dy, dw, dh, sw - 1, sh - 1)) {
      __imlib_RotateAAInside(src, dest, sow, dow,
			     dw, dh, x, y, dx, dy);
      return;
      
   }
   
   st = src; sb = src + (sh - 1) * sow;
   xp = x >> _ROTATE_PREC;
   src += (x >> _ROTATE_PREC) + ((y >> _ROTATE_PREC) * sow);
   x &= _ROTATE_PREC_BITS; y &= _ROTATE_PREC_BITS;
   while (1) {
      i = dw - 1;
      do {
	 if (xp >= 0) {
	    if (xp < (sw - 1)) {
	       if (src >= st) {
		  if (src < sb) {
		     /*\  12
		     |*|  34
		     \*/
		     INTERP_ARGB(*dest, src[0], src[1],
				 src[sow], src[sow + 1], x, y);
		  } else if (src < (sb + sow)) {
		     /*\  12
		     |*|  ..
		     \*/
		     INTERP_RGB_A0(*dest, src[0], src[1], x,
				   (_ROTATE_PREC_MAX - y));
		  }
	       } else if (src >= (st - sow)) {
		  /*\  ..
		  |*|  34
		  \*/
		  INTERP_RGB_A0(*dest, src[sow], src[sow + 1], x, y);
	       }
	    } else if (xp < sw) {
	       if (src >= st) {
		  if (src < sb) {
		     /*\  1.
		     |*|  3.
		     \*/
		     INTERP_RGB_A0(*dest, src[0], src[sow], y,
				   (_ROTATE_PREC_MAX - x));
		  } else if (src < (sb + sow)) {
		     /*\  1.
		     |*|  ..
		     \*/
		     INTERP_A000(*dest, src[0], (_ROTATE_PREC_MAX - x),
				 (_ROTATE_PREC_MAX - y));
		  }
	       } else if (src >= (st - sow)) {
		  /*\  ..
		  |*|  3.
		  \*/
		  INTERP_A000(*dest, src[sow], (_ROTATE_PREC_MAX - x), y);
	       }
	    }
	 } else if (xp >= -1) {
	    if (src >= (st - 1)) {
	       if (src < (sb - 1)) {
		  /*\  .2
		  |*|  .4
		  \*/
		  INTERP_RGB_A0(*dest, src[1], src[sow + 1], y, x);
	       } else if (src < ((sb - 1) + sow)) {
		  /*\  .2
		  |*|  ..
		  \*/
		  INTERP_A000(*dest, src[1], x, (_ROTATE_PREC_MAX - y));
	       }
	    } else if (src >= ((st - 1) - sow)) {
	       /*\  ..
	       |*|  .4
	       \*/
	       INTERP_A000(*dest, src[sow + 1], x, y);
	    }
	 }
	 /*\ RIGHT; \*/
	 x += dx;
	 y += dy;
	 xp += (x >> _ROTATE_PREC);
	 RENORM_X_Y_SRC;
	 dest++;
	 
      } while (--i >= 0);
      if (--dh <= 0) break;
      /*\ DOWN/LEFT; \*/
      x += -dy - dw * dx;
      y += dx - dw * dy;
      xp += (x >> _ROTATE_PREC);
      RENORM_X_Y_SRC;
      dest += (dow - dw);

   }   
}

/*\ Should this be in blend.c ?? \*/
#define LINESIZE 16
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
void
__imlib_BlendImageToImageAtAngle(ImlibImage *im_src, ImlibImage *im_dst,
				 char aa, char blend, char merge_alpha,
				 int ssx, int ssy, int ssw, int ssh,
				 int ddx1, int ddy1, int ddx2, int ddy2,
				 ImlibColorModifier *cm, ImlibOp op)
{
   int ddw, ddh, x, y, dx, dy, i;
   double xy2;
   DATA32 *data, *src;
   
   if ((ssw < 0) || (ssh < 0))
      return;
   
   if ((!(im_src->data)) && (im_src->loader) && (im_src->loader->load))
      im_src->loader->load(im_src, NULL, 0, 1);
   if (!im_src->data)
      return;
   if ((!(im_dst->data)) && (im_dst->loader) && (im_src->loader->load))
      im_dst->loader->load(im_dst, NULL, 0, 1);
   if (!im_dst->data)
      return;
   
   ddw = ddx2 - ddx1;
   ddh = ddy2 - ddy1;
   if ((ssw == ddw) & (ssh == ddh)) {
      if (!IMAGE_HAS_ALPHA(im_src))
	 blend = 0;
      if (!IMAGE_HAS_ALPHA(im_dst))
	 merge_alpha = 0;
      
      __imlib_BlendRGBAToData(im_src->data, im_src->w, im_src->h,
			      im_dst->data, im_dst->w, im_dst->h,
			      ssx, ssy, ddx1, ddy1,
			      ssw, ssh, blend, merge_alpha, cm, op);
      return;
      
   }
   /*\ Complicated gonio.  Works on paper..
   |*| Too bad it doesn't all fit into integer math.. 
   \*/
   xy2 = (double)(ddh * ddh + ddw * ddw);
   if (xy2 == 0) return;
   dx = (((double)((ssh * ddh) + (ssw * ddw)) * _ROTATE_PREC_MAX) / xy2);
   dy = (((double)((ssh * ddw) - (ssw * ddh)) * _ROTATE_PREC_MAX) / xy2);
   x = ddy1 * dy - ddx1 * dx;
   y = -ddx1 * dy - ddy1 * dx;
   
   if (ssx < 0) {
      x += ssx * _ROTATE_PREC_MAX;
      ssw += ssx;
      ssx = 0;
      
   }
   if (ssy < 0) {
      y += ssy * _ROTATE_PREC_MAX;
      ssh += ssy;
      ssy = 0;
      
   }
   if ((ssw + ssx) > im_src->w) ssw = im_src->w - ssx;
   if ((ssh + ssy) > im_src->h) ssh = im_src->h - ssy;
   
   src = im_src->data + ssx + ssy * im_src->w;
   data = malloc(im_dst->w * LINESIZE * sizeof(DATA32));
   if (!data)
      return;
   if (aa) {
      /*\ Account for virtual transparent border \*/
      x += _ROTATE_PREC_MAX;
      y += _ROTATE_PREC_MAX;
   }
   for (i = 0; i < im_dst->h; i += LINESIZE) {
      int x2, y2, w, h, l, r;
      
      h = MIN(LINESIZE, im_dst->h - i);
      
      x2 = x - h * dy;
      y2 = y + h * dx;
      
      w = ssw << _ROTATE_PREC;
      h = ssh << _ROTATE_PREC;
      if (aa) {
	 /*\ Account for virtual transparent border \*/
	 w += 2 << _ROTATE_PREC;
	 h += 2 << _ROTATE_PREC;
      }
      /*\ Pretty similar code \*/
      if (dx > 0) {
	 if (dy > 0) {
	    l = MAX(-y2 / dy, -x / dx);
	    r = MIN((h - y) / dy, (w - x2) / dx);

	 } else if (dy < 0) {
	    l = MAX(-x2 / dx, (h - y) / dy);
	    r = MIN(-y2 / dy, (w - x) / dx);

	 } else {
	    l = -x / dx;
	    r = (w - x) / dx;

	 }
      } else if (dx < 0) {
	 if (dy > 0) {
	    l = MAX(-y / dy, (w - x2) / dx);
	    r = MIN(-x / dx, (h - y2) / dy);

	 } else if (dy < 0) {
	    l = MAX((h - y2) / dy, (w - x) / dx);
	    r = MIN(-y / dy, -x2 / dx);

	 } else {
	    l = (w - x) / dx;
	    r = -x / dx;

	 }

      } else {
	 if (dy > 0) {
	    l = -y / dy;
	    r = (h - y) / dy;

	 } else if (dy < 0) {
	    l = (h - y) / dy;
	    r = -y / dy;

	 } else {
	    l = 0;
	    r = 0;

	 }
	 
      }
      l--; r += 2; /*\ Be paranoid about roundoff errors \*/
      if (l < 0) l = 0;
      if (r > im_dst->w) r = im_dst->w;
      if (r <= l) {
	 x = x2; y = y2;
	 continue;
	 
      }
      
      w = r - l;
      h = MIN(LINESIZE, im_dst->h - i);
      memset(data, 0, h * w * sizeof(DATA32));
      x += l * dx;
      y += l * dy;
      if (aa) {
	 __imlib_RotateAA(src, data, im_src->w, ssw, ssh, w, w, h,
			  x - _ROTATE_PREC_MAX, y - _ROTATE_PREC_MAX, dx, dy);
	 
      } else {
	 __imlib_RotateSample(src, data, im_src->w,
			      ssw, ssh, w, w, h, x, y, dx, dy);
	 
      }
      __imlib_BlendRGBAToData(data, w, h, im_dst->data,
			      im_dst->w, im_dst->h, 0, 0, l, i, w, h,
			      blend, merge_alpha, cm, op);
      x = x2; y = y2;
      
   }
   free(data);
}