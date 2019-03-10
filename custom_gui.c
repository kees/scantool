#include "globals.h"
#include <allegro/internal/aintern.h>
#include "custom_gui.h"


/* nostretch_icon_proc:
 *  Custom version of d_icon_proc.  Instead of stretching the bitmap to fit
 *  the dimensions w & h, it directly blit()'s it.
 */
int nostretch_icon_proc(int msg, DIALOG *d, int c)
{
   BITMAP *butimage = (BITMAP *)d->dp;

   if ((msg == MSG_DRAW) && (!(d->flags & D_HIDDEN)))
   {
      if ((d->dp2 != NULL) && (d->flags & D_GOTFOCUS))
      {
        butimage = (BITMAP *)d->dp2; //if we got focus, display dp2
      }
      if ((d->dp3 != NULL) && (d->flags & D_SELECTED))
      {
        butimage = (BITMAP *)d->dp3; // if the button was clicked, display d3
      }

      /* put the graphic on screen */
      blit(butimage, screen, 0, 0, d->x, d->y, butimage->w, butimage->h);

      return D_O_K;
   }

   return d_button_proc(msg, d, c);
}


int super_textbox_proc(int msg, DIALOG *d, int c)
{
   int height, bar;
   int fg_color;
   int ret = D_O_K;
   FONT *old_font = font; // save current font in old_font

   if (d->dp2) // if custom font was loaded into dp2,
      font = (FONT *)d->dp2;  // make custom font the default font

   fg_color = (d->flags & D_DISABLED) ? gui_mg_color : d->fg;
   /* calculate the actual height */
   height = (d->h-8) / text_height(font);

   if (msg == MSG_DRAW)
   {
      /* tell the object to sort of draw, but only calculate the listsize */
      _draw_textbox(d->dp, &d->d1,
                    0, /* DONT DRAW anything */
                    d->d2, !(d->flags & D_SELECTED), 8,
                    d->x, d->y, d->w, d->h,
                    (d->flags & D_DISABLED),
                    0, 0, 0);

      if (d->d1 > height) {
         bar = 12;
      }
      else {
         bar = 0;
         d->d2 = 0;
      }

      /* now do the actual drawing */
      _draw_textbox(d->dp, &d->d1, 1, d->d2,
                    !(d->flags & D_SELECTED), 8,
                    d->x, d->y, d->w-bar, d->h,
                    (d->flags & D_DISABLED),
                    fg_color, d->bg, gui_mg_color);

      /* draw the frame around */
      if (d->key)
         _draw_scrollable_frame(d, d->d1, d->d2, height, fg_color, d->bg);
   }
   else
      ret = d_textbox_proc(msg, d, c);

   // make everything like it was before
   font = old_font;

   return ret;
}


int caption_proc(int msg, DIALOG *d, int c)
{
   if (msg == MSG_START)
   {
      d->dp2 = datafile[ARIAL12_BOLD_FONT].dat;
   }
   return st_ctext_proc(msg, d, c);
}


int st_ctext_proc(int msg, DIALOG *d, int c)
{
   ASSERT(d);
   if (msg==MSG_DRAW)
   {
      int fg = (d->flags & D_DISABLED) ? gui_mg_color : d->fg;
      FONT *oldfont = font;

      if (d->dp2)
        font = d->dp2;

      gui_textout_ex(screen, d->dp, d->x, d->y, fg, d->bg, TRUE);

      font = oldfont;
   }

   return D_O_K;
}
