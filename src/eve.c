/*
 * Copyright (C) 2020 The HIME team, Taiwan
 * Copyright (C) 2004-2011 Edward Der-Hua Liu, Hsin-Chu, Taiwan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation version 2.1
 * of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <signal.h>

#include <X11/extensions/XTest.h>

#include "hime.h"

#include "gst.h"
#include "gtab.h"
#include "hime-module-cb.h"
#include "pho.h"
#include "win-kbm.h"
#include "win-sym.h"

#define STRBUFLEN 64

extern Display *dpy;
#if USE_XIM
extern XIMS current_ims;
static IMForwardEventStruct *current_forward_eve;
#endif
extern gboolean win_kbm_inited;
extern int hime_show_win_kbm;
extern gboolean win_kbm_on;

static char *callback_str_buffer;
Window focus_win;
static int timeout_handle;
char *output_buffer;
uint32_t output_bufferN;
static char *output_buffer_raw, *output_buffer_raw_bak;
static uint32_t output_buffer_rawN;
void set_wselkey (char *s);
void gtab_set_win1_cb ();
void toggle_symbol_table ();

gboolean old_capslock_on;

extern gboolean key_press_ctrl;

#if TRAY_ENABLED
extern int hime_tray_display;
#endif
void init_gtab (int inmdno);

char current_method_type () {
    //  dbg("default_input_method %d\n",default_input_method);
    if (!current_CS)
        return inmd[default_input_method].method_type;

    //  dbg("current_CS->in_method %d\n", current_CS->in_method);
    return inmd[current_CS->in_method].method_type;
}

void send_fake_key_eve (KeySym key) {
    KeyCode kc = XKeysymToKeycode (dpy, key);
    XTestFakeKeyEvent (dpy, kc, True, CurrentTime);
    XTestFakeKeyEvent (dpy, kc, False, CurrentTime);
}

void fake_shift () {
    send_fake_key_eve (XK_Shift_L);
}

void swap_ptr (char **a, char **b) {
    char *t = *a;
    *a = *b;
    *b = t;
}

int force_preedit = 0;
void force_preedit_shift () {
    send_fake_key_eve (XK_Shift_L);
    force_preedit = 1;
}

void send_text_call_back (char *text) {
    callback_str_buffer = (char *) realloc (callback_str_buffer, strlen (text) + 1);
    strcpy (callback_str_buffer, text);
    fake_shift ();
}

void output_buffer_call_back () {
    swap_ptr (&callback_str_buffer, &output_buffer);

    if (output_buffer)
        output_buffer[0] = 0;
    output_bufferN = 0;

    fake_shift ();
}

ClientState *current_CS;
static ClientState temp_CS;

void save_CS_current_to_temp (void) {
    if (!hime_single_state) {
        return;
    }

    //  dbg("save_CS_current_to_temp\n");
    temp_CS.b_half_full_char = current_fullshape_mode ();
    temp_CS.in_method_switched = current_CS->in_method_switched;
    temp_CS.b_im_enabled = current_CS->b_im_enabled;
    temp_CS.in_method = current_CS->in_method;
    temp_CS.b_chinese_mode = current_CS->b_chinese_mode;
}

void save_CS_temp_to_current () {
    if (!hime_single_state)
        return;

    //  dbg("save_CS_temp_to_current\n");
    current_CS->b_half_full_char = temp_CS.b_half_full_char;
    current_CS->in_method_switched = temp_CS.in_method_switched;
    current_CS->b_im_enabled = temp_CS.b_im_enabled;
    current_CS->in_method = temp_CS.in_method;
    current_CS->b_chinese_mode = temp_CS.b_chinese_mode;
}

/**
 * Check whether the current client state is in the fullshape mode or not
 * \retval 1 Fullshape mode
 * \retval 0 Halfshape mode
 * \todo When the current client state pointer is null, the return value would be 0.
 */
int current_fullshape_mode () {
    return current_CS && current_CS->b_half_full_char;
}

gboolean init_in_method (int in_no);

void clear_output_buffer (void) {
    if (output_buffer) {
        output_buffer[0] = '\0';
    }

    output_bufferN = 0;

    swap_ptr (&output_buffer_raw, &output_buffer_raw_bak);

    if (output_buffer_raw) {
        output_buffer_raw[0] = '\0';
    }

    output_buffer_rawN = 0;
}

gboolean gb_output = FALSE;

void toggle_gb_output () {
    gb_output = !gb_output;
}

/* Force to output Simplified Chinese */
void sim_output () {
    gb_output = TRUE;
}

/* Force to output original string, usually are Traditional Chinese */
void trad_output () {
    gb_output = FALSE;
}

static void append_str (char **buf, uint32_t *bufN, char *text, int len) {
    int requiredN = len + 1 + *bufN;
    *buf = (char *) realloc (*buf, requiredN);
    (*buf)[*bufN] = 0;
    strcat (*buf, text);
    *bufN += len;
}

int trad2sim (char *str, int strN, char **out);
void add_ch_time_str (char *s);

void send_text (char *text) {
    char *filter;

    if (!text)
        return;
    int len = strlen (text);

    add_ch_time_str (text);

    append_str (&output_buffer_raw, &output_buffer_rawN, text, len);

    char *utf8_gbtext = NULL;

    if (gb_output) {
        len = trad2sim (text, len, &utf8_gbtext);
        text = utf8_gbtext;
    }

    // direct:
    filter = getenv ("HIME_OUTPUT_FILTER");
    char filter_text[512];

    if (filter) {
        int pfdr[2], pfdw[2];

        if (pipe (pfdr) == -1) {
            dbg ("cannot pipe r\n");
            goto next;
        }

        if (pipe (pfdw) == -1) {
            dbg ("cannot pipe w\n");
            goto next;
        }

        int pid = fork ();

        if (pid < 0) {
            dbg ("cannot fork filter\n");
            goto next;
        }

        if (pid) {
            close (pfdw[0]);
            close (pfdr[1]);
            write (pfdw[1], text, len);
            close (pfdw[1]);
            int rn = read (pfdr[0], filter_text, sizeof (filter_text) - 1);
            filter_text[rn] = 0;
            //      puts(filter_text);
            close (pfdr[0]);
            text = filter_text;
            len = rn;
        } else {
            close (pfdr[0]);
            close (pfdw[1]);
            dup2 (pfdw[0], 0);
            dup2 (pfdr[1], 1);
            if (execl (filter, filter, NULL) < 0) {
                dbg ("execl %s err", filter);
                goto next;
            }
        }
    }
next:
    if (len) {
        append_str (&output_buffer, &output_bufferN, text, len);
    }

    free (utf8_gbtext);
}

void send_output_buffer_bak () {
    send_text (output_buffer_raw_bak);
}

void send_utf8_ch (char *bchar) {
    char tt[CH_SZ + 1];
    int len = utf8_sz (bchar);

    memcpy (tt, bchar, len);
    tt[len] = 0;

    send_text (tt);
}

void send_ascii (char key) {
    send_utf8_ch (&key);
}

#if USE_XIM
void export_text_xim () {
    char *text = output_buffer;

    if (!output_bufferN)
        return;

    XTextProperty tp;
#if 0
  char outbuf[512];
  utf8_big5(output_buffer, outbuf);
  text = outbuf;
  XmbTextListToTextProperty(dpy, &text, 1, XCompoundTextStyle, &tp);
#else
    Xutf8TextListToTextProperty (dpy, &text, 1, XCompoundTextStyle, &tp);
#endif

#if DEBUG && 0
    dbg ("send_utf8_ch: %s\n", text);
#endif

    ((IMCommitStruct *) current_forward_eve)->flag |= XimLookupChars;
    ((IMCommitStruct *) current_forward_eve)->commit_string = (char *) tp.value;
    IMCommitString (current_ims, (XPointer) current_forward_eve);

    clear_output_buffer ();

    XFree (tp.value);
}

static void bounce_back_key () {
    IMForwardEventStruct forward_ev = *(current_forward_eve);
    IMForwardEvent (current_ims, (XPointer) &forward_ev);
}
#endif

void hide_win0 ();
void hide_win_gtab ();
void hide_win_pho ();

int current_in_win_x = -1, current_in_win_y = -1;  // request x/y

void reset_current_in_win_xy () {
    current_in_win_x = current_in_win_y = -1;
}

HIME_module_callback_functions *module_cb1 (ClientState *cs) {
    return inmd[cs->in_method].mod_cb_funcs;
}

HIME_module_callback_functions *module_cb () {
    if (!current_CS)
        return NULL;
    return module_cb1 (current_CS);
}

void hide_in_win (ClientState *cs) {
    if (!cs) {
#if 0
    dbg("hide_in_win: ic is null\n");
#endif
        return;
    }
#if 0
  dbg("hide_in_win %d\n", ic->in_method);
#endif

    if (timeout_handle) {
        g_source_remove (timeout_handle);
        timeout_handle = 0;
    }

    switch (current_method_type ()) {
    case method_type_PHO:
        hide_win_kbm ();
        hide_win_pho ();
        break;
#if USE_TSIN
    case method_type_TSIN:
        //      flush_tsin_buffer();
        hide_win_kbm ();
        hide_win0 ();
        break;
#endif
    case method_type_MODULE:
        if (inmd[cs->in_method].mod_cb_funcs)
            module_cb1 (cs)->module_hide_win ();
        hide_win_gtab ();
        break;
    default:
        hide_win_gtab ();
    }

    reset_current_in_win_xy ();
}

void show_win_pho ();
void show_win0 ();
void show_win_gtab ();
#if TRAY_ENABLED
void disp_tray_icon ();
#endif

void check_CS () {
    if (!current_CS) {
        //    dbg("!current_CS");
        current_CS = &temp_CS;
//    temp_CS.input_style = InputStyleOverSpot;
#if TRAY_ENABLED
        disp_tray_icon ();
#endif
    } else {
#if 0
    if (hime_single_state)
      save_CS_temp_to_current();
    else
#endif
        temp_CS = *current_CS;
    }
}

gboolean force_show;
void show_input_method_name_on_gtab ();

void show_in_win (ClientState *cs) {
    if (!cs) {
#if 0
    dbg("show_in_win: ic is null");
#endif
        return;
    }

    switch (current_method_type ()) {
    case method_type_PHO:
        show_win_pho ();
        break;
#if USE_TSIN
    case method_type_TSIN:
        show_win0 ();
        break;
#endif
    case method_type_MODULE:
        if (!module_cb1 (cs))
            return;
        module_cb1 (cs)->module_show_win ();
        show_input_method_name_on_gtab ();
        break;
    default:
        show_win_gtab ();
        show_input_method_name_on_gtab ();
        break;
    }

    if (hime_show_win_kbm &&
        (current_CS->b_im_enabled) &&
        (current_method_type () != method_type_MODULE) &&
        (current_method_type () != method_type_SYMBOL_TABLE)) {
        show_win_kbm ();
        update_win_kbm ();
    }

#if 0
  show_win_stautus();
#endif
}

void move_win_gtab (int x, int y);
void move_win0 (int x, int y);
void move_win_pho (int x, int y);

void move_in_win (ClientState *cs, int x, int y) {
    check_CS ();

    if (current_CS && current_CS->fixed_pos) {
        x = current_CS->fixed_x;
        y = current_CS->fixed_y;
    } else if (hime_input_style == InputStyleRoot) {
        x = hime_root_x;
        y = hime_root_y;
    }

#if 0
  dbg("move_in_win %d %d\n",x, y);
#endif
#if 0
  if (current_in_win_x == x && current_in_win_y == y)
    return;
#endif
    current_in_win_x = x;
    current_in_win_y = y;

    switch (current_method_type ()) {
    case method_type_PHO:
        move_win_pho (x, y);
        break;
#if USE_TSIN
    case method_type_TSIN:
        move_win0 (x, y);
        break;
#endif
    case method_type_MODULE:
        if (inmd[cs->in_method].mod_cb_funcs)
            module_cb1 (cs)->module_move_win (x, y);
        move_win_gtab (x, y);
        break;
    default:
        if (!cs->in_method)
            return;
        move_win_gtab (x, y);
    }
}

static int xerror_handler (Display *d, XErrorEvent *eve) {
    return 0;
}

void getRootXY (Window win, int wx, int wy, int *tx, int *ty) {
    if (!win) {
        *tx = wx;
        *ty = wy;
        return;
    }

    Window ow;
    XErrorHandler olderr = XSetErrorHandler ((XErrorHandler) xerror_handler);
    XTranslateCoordinates (dpy, win, root, wx, wy, tx, ty, &ow);
    XSetErrorHandler (olderr);
}

void move_IC_in_win (ClientState *cs) {
#if 0
   dbg("move_IC_in_win %d,%d\n", cs->spot_location.x, cs->spot_location.y);
#endif
    Window inpwin = cs->client_win;

    if (!inpwin) {
        dbg ("no inpwin\n");
        return;
    }

    // non focus win filtering is done in the client lib
    if (inpwin != focus_win && focus_win && !cs->b_hime_protocol) {
        return;
    }

    int inpx = cs->spot_location.x;
    int inpy = cs->spot_location.y;

    XWindowAttributes att;
    XGetWindowAttributes (dpy, inpwin, &att);
    // chrome window is override_redirect
    //   if (att.override_redirect)
    //     return;

    if (inpx >= att.width)
        inpx = att.width - 1;
    if (inpy >= att.height)
        inpy = att.height - 1;
    int tx, ty;
    getRootXY (inpwin, inpx, inpy, &tx, &ty);

#if 0
   dbg("move_IC_in_win inpxy:%d,%d txy:%d,%d\n", inpx, inpy, tx, ty);
#endif

    move_in_win (cs, tx, ty + 1);
}

void update_in_win_pos (void) {
    check_CS ();

    if (current_CS->input_style == InputStyleRoot) {
        Window r_root, r_child;
        int winx, winy, rootx, rooty;
        uint32_t mask;

        XQueryPointer (dpy, root, &r_root, &r_child, &rootx, &rooty, &winx, &winy, &mask);

        winx++;
        winy++;

        Window inpwin = current_CS->client_win;

        if (inpwin) {
            int tx, ty;
            Window ow;

            XTranslateCoordinates (dpy, root, inpwin, winx, winy, &tx, &ty, &ow);

            current_CS->spot_location.x = tx;
            current_CS->spot_location.y = ty;
        }
        move_in_win (current_CS, winx, winy);
    } else {
        move_IC_in_win (current_CS);
    }
}

void win_pho_disp_half_full ();
void win_tsin_disp_half_full ();
void win_gtab_disp_half_full ();
#if TRAY_ENABLED
void load_tray_icon (), load_tray_icon_double ();
#if TRAY_UNITY
void load_tray_appindicator ();
#endif
gboolean is_exist_tray ();
extern void destroy_tray_icon ();
gboolean is_exist_tray_double ();
extern void destroy_tray_double ();
#if TRAY_UNITY
gboolean is_exist_tray_appindicator ();
extern void destroy_tray_appindicator ();
#endif
#endif  // TRAY_ENABLED

#if TRAY_ENABLED
void destroy_tray () {
    // TODO: optimze it , e.g. struct
    if (is_exist_tray ())
        destroy_tray_icon ();
    if (is_exist_tray_double ())
        destroy_tray_double ();
#if TRAY_UNITY
    if (is_exist_tray_appindicator ())
        destroy_tray_appindicator ();
#endif
}

void destroy_other_tray () {
    if (!hime_status_tray) {
        destroy_tray ();
        return;
    }
    if (is_exist_tray () && hime_tray_display != HIME_TRAY_DISPLAY_SINGLE)
        destroy_tray_icon ();
    if (is_exist_tray_double () && hime_tray_display != HIME_TRAY_DISPLAY_DOUBLE)
        destroy_tray_double ();
#if TRAY_UNITY
    if (is_exist_tray_appindicator () && hime_tray_display != HIME_TRAY_DISPLAY_APPINDICATOR)
        destroy_tray_appindicator ();
#endif
}

void disp_tray_icon () {
    if (!hime_status_tray)
        return destroy_tray ();
    // dbg("disp_tray_icon\n");

    if (hime_tray_display == HIME_TRAY_DISPLAY_SINGLE)
        load_tray_icon ();
    if (hime_tray_display == HIME_TRAY_DISPLAY_DOUBLE)
        load_tray_icon_double ();
#if TRAY_UNITY
    if (hime_tray_display == HIME_TRAY_DISPLAY_APPINDICATOR)
        load_tray_appindicator ();
#endif
}
#endif

void disp_im_half_full () {
//  dbg("disp_im_half_full\n");
#if TRAY_ENABLED
    disp_tray_icon ();
#endif

    switch (current_method_type ()) {
    case method_type_PHO:
        win_pho_disp_half_full ();
        break;
#if USE_TSIN
    case method_type_TSIN:
        win_tsin_disp_half_full ();
        break;
#endif
    default:
        win_gtab_disp_half_full ();
        break;
    }
}

void flush_tsin_buffer ();
void reset_gtab_all ();

// static u_int orig_caps_state;

void init_state_chinese (ClientState *cs) {
    cs->b_im_enabled = TRUE;
    set_chinese_mode0 (cs);
    if (!cs->in_method_switched) {
        init_in_method (default_input_method);
        cs->in_method_switched = TRUE;
    }

    save_CS_current_to_temp ();
}

gboolean output_gbuf ();

// <Ctrl><Space> is pressed
void toggle_im_enabled () {
    //    dbg("toggle_im_enabled\n");
    check_CS ();

    if (current_CS->in_method < 0)
        p_err ("err found");

    if (current_CS->b_im_enabled) {
        if (current_fullshape_mode ()) {
            disp_im_half_full ();
        }

        if (current_method_type () == method_type_TSIN) {
#if USE_TSIN
            flush_tsin_buffer ();
#endif
        } else if (current_method_type () == method_type_MODULE) {
            HIME_module_callback_functions *mod_cbs = module_cb ();
            if (mod_cbs)
                mod_cbs->module_flush_input ();
        } else {
            output_gbuf ();
            reset_gtab_all ();
        }

        hide_in_win (current_CS);
#if 0
      hide_win_status();
#endif
        current_CS->b_im_enabled = FALSE;

#if TRAY_ENABLED
        disp_tray_icon ();
#endif
    } else {
        if (!current_method_type ())
            init_gtab (current_CS->in_method);

        init_state_chinese (current_CS);
        reset_current_in_win_xy ();
#if 1
        if ((inmd[current_CS->in_method].flag & FLAG_GTAB_SYM_KBM)) {
            win_kbm_inited = 1;
            show_win_kbm ();
        } else
            show_in_win (current_CS);

        update_in_win_pos ();
#else
        update_in_win_pos ();
        show_in_win (current_CS);
#endif

#if TRAY_ENABLED
        disp_tray_icon ();
#endif
    }

    save_CS_current_to_temp ();
}

void get_win_gtab_geom ();
void get_win0_geom ();
void get_win_pho_geom ();

void update_active_in_win_geom () {
    HIME_module_callback_functions *mod_cbs;
    //  dbg("update_active_in_win_geom\n");
    switch (current_method_type ()) {
    case method_type_PHO:
        get_win_pho_geom ();
        break;
#if USE_TSIN
    case method_type_TSIN:
        get_win0_geom ();
        break;
#endif
    case method_type_MODULE:
        mod_cbs = module_cb ();
        if (mod_cbs && mod_cbs->module_get_win_geom)
            mod_cbs->module_get_win_geom ();
        break;
    default:
        get_win_gtab_geom ();
        break;
    }
}

extern GtkWidget *gwin_pho, *gwin0, *gwin_gtab, *gwin_sym;

gboolean win_is_visible () {
    if (!current_CS)
        return FALSE;
    switch (current_method_type ()) {
    case method_type_PHO:
        return gwin_pho && gtk_widget_get_visible (gwin_pho);
#if USE_TSIN
    case method_type_TSIN:
        return gwin0 && gtk_widget_get_visible (gwin0);
#endif
    case method_type_MODULE:
        if (!module_cb ())
            return FALSE;
        return module_cb ()->module_win_visible ();
    case method_type_SYMBOL_TABLE:
        return gwin_sym && gtk_widget_get_visible (gwin_sym);
    default:
        if (!gwin_gtab)
            return FALSE;
        return gwin_gtab && gtk_widget_get_visible (gwin_gtab);
    }

    return FALSE;
}

void disp_gtab_half_full (gboolean hf);

// <Shift><Space> is pressed
void toggle_half_full_char () {
    check_CS ();

    if (!hime_shift_space_eng_full) {
        current_CS->b_half_full_char = FALSE;
        disp_im_half_full ();
        return;
    }

    current_CS->b_half_full_char = !current_fullshape_mode ();
    disp_im_half_full ();
    save_CS_current_to_temp ();
}

/**
 * Toggle the input method between Chinese/other language mode and English mode
 */
void toggle_eng_ch_mode (void) {
    set_eng_ch_mode (!current_CS->b_chinese_mode);
    show_stat ();
}

void set_eng_ch_mode (gboolean mode) {
    if (current_CS) {
        current_CS->b_chinese_mode = mode;
        save_CS_current_to_temp ();
    }

    if (current_method_type () == method_type_TSIN) {
        drawcursor ();

        if (!chinese_mode ())
            clrin_pho_tsin ();

        show_button_pho (chinese_mode ());
    } else
        show_win_gtab ();

    show_stat ();
}

void set_chinese_mode0 (ClientState *cs) {
    if (!cs)
        return;
    cs->b_chinese_mode = True;
    save_CS_current_to_temp ();
}

/**
 * Set the input method mode to Chinese/other language mode
 */
void set_chinese_mode (void) {
    set_chinese_mode0 (current_CS);
    show_stat ();
}

/**
 * Check the input method is in Chinese/other language mode or not
 * \retval TRUE Chinese/Other language mode
 * \retval FALSE English mode
 * \todo When the current client state pointer is null, the return value would be FALSE.
 */
gboolean chinese_mode (void) {
    return current_CS && current_CS->b_chinese_mode;
}

void show_stat (void) {
#if TRAY_ENABLED
    disp_tray_icon ();
#endif
}

void init_tab_pp (gboolean init);
void init_tab_pho ();

extern char *TableDir;
void set_gtab_input_method_name (char *s);
HIME_module_callback_functions *init_HIME_module_callback_functions (char *sofile);
time_t find_tab_file (char *fname, char *out_file);
gboolean tsin_page_up (), tsin_page_down ();
int tsin_sele_by_idx (int);

static void tsin_set_win1_cb () {
    set_win1_cb ((cb_selec_by_idx_t) tsin_sele_by_idx, (cb_page_ud_t) tsin_page_up, (cb_page_ud_t) tsin_page_down);
}

void update_win_kbm_inited () {
    if (win_kbm_inited)
        update_win_kbm ();
}

gboolean init_in_method (int in_no) {
    gboolean init_im = !(cur_inmd && (cur_inmd->flag & FLAG_GTAB_SYM_KBM));

    if (in_no < 0)
        return FALSE;

    check_CS ();

    if (current_CS->in_method != in_no) {
        if (!(inmd[in_no].flag & FLAG_GTAB_SYM_KBM)) {
            if (current_method_type () == method_type_TSIN) {
                flush_tsin_buffer ();
            } else
                output_gbuf ();

            hide_in_win (current_CS);
        }
    }

    reset_current_in_win_xy ();

    //  dbg("switch init_in_method %x %d\n", current_CS, in_no);
    set_chinese_mode0 (current_CS);
    tsin_set_win1_cb ();

    switch (inmd[in_no].method_type) {
    case method_type_PHO:
        current_CS->in_method = in_no;
        init_tab_pho ();
        break;
    case method_type_TSIN:
        set_wselkey (pho_selkey);
        current_CS->in_method = in_no;
        init_tab_pp (init_im);
        break;
    case method_type_SYMBOL_TABLE:
        toggle_symbol_table ();
        break;
    case method_type_MODULE: {
        HIME_module_main_functions gmf;
        init_HIME_module_main_functions (&gmf);
        if (!inmd[in_no].mod_cb_funcs) {
            char ttt[256];
            strcpy (ttt, inmd[in_no].filename);

            dbg ("module %s\n", ttt);
            if (!(inmd[in_no].mod_cb_funcs = init_HIME_module_callback_functions (ttt))) {
                dbg ("module not found\n");
                return FALSE;
            }
        }

        if (inmd[in_no].mod_cb_funcs->module_init_win (&gmf)) {
            current_CS->in_method = in_no;
            module_cb ()->module_show_win ();
            set_wselkey (pho_selkey);
        } else {
            return FALSE;
        }
        show_input_method_name_on_gtab ();
        break;
    }
    case method_type_EN: {
        if (current_CS && current_CS->b_im_enabled)
            toggle_im_enabled ();
        current_CS->in_method = in_no;
        hide_win_kbm ();
        return TRUE;
    }
    default:
        init_gtab (in_no);
        if (!inmd[in_no].DefChars)
            return FALSE;
        if (inmd[in_no].flag & FLAG_GTAB_SYM_KBM) {
            if (win_kbm_on) {
                init_in_method (default_input_method);
                hide_win_kbm ();
            } else {
                // Show SYM_KBM
                hide_in_win (current_CS);
                win_kbm_inited = 1;
                show_win_kbm ();
                current_CS->in_method = in_no;
            }
        } else {
            // in case WIN_SYN and SYM_KBM show at the same time.
            current_CS->in_method = in_no;
            hide_win_sym ();
            win_sym_enabled = 0;

            show_win_gtab ();
            show_input_method_name_on_gtab ();
        }

        // set_gtab_input_method_name(inmd[in_no].cname);
        break;
    }

    if (hime_init_full_mode && !current_fullshape_mode ())
        toggle_half_full_char ();

#if TRAY_ENABLED
    disp_tray_icon ();
#endif

    if (inmd[current_CS->in_method].selkey) {
        set_wselkey (inmd[current_CS->in_method].selkey);
        gtab_set_win1_cb ();
        //    dbg("aa selkey %s\n", inmd[current_CS->in_method].selkey);
    }

    update_in_win_pos ();
    update_win_kbm_inited ();

    if (hime_show_win_kbm) {
        if ((!current_CS->b_im_enabled && current_fullshape_mode ()) ||
            (current_method_type () == method_type_MODULE) ||
            (current_method_type () == method_type_SYMBOL_TABLE))
            hide_win_kbm ();
        else {
            show_win_kbm ();
            update_win_kbm ();
        }
    }

    return TRUE;
}

static void cycle_next_in_method () {
    if (current_method_type () == method_type_SYMBOL_TABLE) {
        hide_win_sym ();
        win_sym_enabled = 0;
    }

    int i;
    for (i = 0; i < inmdN; i++) {
        int v = (current_CS->in_method + 1 + i) % inmdN;
        if (win_sym_enabled && inmd[v].method_type == method_type_SYMBOL_TABLE)
            continue;

        if (!inmd[v].in_cycle)
            continue;

        if (!inmd[v].cname || !inmd[v].cname[0])
            continue;

        if (!init_in_method (v))
            continue;

        return;
    }
}

void add_to_tsin_buf_str (char *str);
gboolean gtab_phrase_on ();
void insert_gbuf_nokey (char *s);

gboolean full_char_proc (KeySym keysym) {
    char *s = half_char_to_full_char (keysym);

    if (!s)
        return 0;

    char tt[CH_SZ + 1];

    utf8cpy (tt, s);

    if (current_fullshape_mode ()) {
        send_text (tt);
        return 1;
    }

    if ((current_method_type () == method_type_TSIN) && current_CS->b_im_enabled)
        add_to_tsin_buf_str (tt);
    else if (gtab_phrase_on () && ggg.gbufN)
        insert_gbuf_nokey (tt);
    else
        send_text (tt);

    return 1;
}

int feedkey_pho (KeySym xkey, int kbstate);
int feedkey_pp (KeySym xkey, int state);
int feedkey_gtab (KeySym key, int kbstate);
int feed_phrase (KeySym ksym, int state);
static KeySym last_keysym;

gboolean timeout_raise_window (gpointer data) {
    //  dbg("timeout_raise_window\n");
    timeout_handle = 0;
    show_in_win (current_CS);
    return FALSE;
}

extern Window xwin_pho, xwin0, xwin_gtab;
void win_kbm_disp_caplock ();

void disp_win_kbm_capslock () {
    if (!hime_show_win_kbm)
        return;

    gboolean o_state = old_capslock_on;
    old_capslock_on = get_caps_lock_state ();

    if (o_state != old_capslock_on) {
        win_kbm_disp_caplock ();
    }
}

void disp_win_kbm_capslock_init () {
    old_capslock_on = get_caps_lock_state ();

    if (hime_show_win_kbm)
        win_kbm_disp_caplock ();
}

void toggle_symbol_table () {
    if (current_CS->b_im_enabled) {
        if (!win_is_visible ())
            win_sym_enabled = 1;
        else
            win_sym_enabled ^= 1;
    } else
        win_sym_enabled = 0;

    create_win_sym ();
    if (win_sym_enabled) {
        force_show = TRUE;
        if (current_CS->b_im_enabled)
            show_in_win (current_CS);
        force_show = FALSE;
    }

    hide_win_kbm ();
}

void destroy_phrase_save_menu ();
int hime_switch_keys_lookup (int key);

gboolean check_key_press (KeySym key, uint32_t kev_state, gboolean return_value) {
    if ((key == XK_Shift_L || key == XK_Shift_R) && key_press_shift) {
        key_press_ctrl = FALSE;
    } else if ((key == XK_Control_L || key == XK_Control_R) && key_press_ctrl) {
        key_press_shift = FALSE;
    } else {
        key_press_shift = FALSE;
        key_press_ctrl = FALSE;
    }

    return return_value;
}

// return TRUE if the key press is processed
gboolean ProcessKeyPress (KeySym keysym, uint32_t kev_state) {
#if 0
  dbg("key press %x %x\n", keysym, kev_state);
#endif
    destroy_phrase_save_menu ();

    disp_win_kbm_capslock ();
    check_CS ();

    if (current_CS->client_win)
        focus_win = current_CS->client_win;

    if (callback_str_buffer && strlen (callback_str_buffer)) {
        send_text (callback_str_buffer);
        callback_str_buffer[0] = 0;
        return check_key_press (keysym, kev_state, TRUE);
    }

    if (force_preedit) {
        force_preedit = 0;
        return check_key_press (keysym, kev_state, FALSE);
    }

    if (keysym == XK_space) {
#if 0
    dbg("state %x\n", kev->state);
    dbg("%x\n", Mod4Mask);
#endif
        if (
            ((kev_state & (ControlMask | Mod1Mask | ShiftMask)) == ControlMask && hime_im_toggle_keys == Control_Space) ||
            ((kev_state & Mod1Mask) && hime_im_toggle_keys == Alt_Space) ||
            ((kev_state & ShiftMask) && hime_im_toggle_keys == Shift_Space) ||
            ((kev_state & Mod4Mask) && hime_im_toggle_keys == Windows_Space)) {
            if (current_method_type () == method_type_TSIN) {
                set_eng_ch_mode (TRUE);
            }

            toggle_im_enabled ();
            return check_key_press (keysym, kev_state, TRUE);
        }
    }

    if (keysym == XK_space && (kev_state & ShiftMask)) {
        if (last_keysym != XK_Shift_L && last_keysym != XK_Shift_R)
            return check_key_press (keysym, kev_state, FALSE);

        toggle_half_full_char ();

        return check_key_press (keysym, kev_state, TRUE);
    }

    if ((kev_state & (Mod1Mask | ShiftMask)) == (Mod1Mask | ShiftMask)) {
        if (current_CS->b_im_enabled || hime_eng_phrase_enabled)
            return check_key_press (keysym, kev_state, feed_phrase (keysym, kev_state));
        else
            return check_key_press (keysym, kev_state, FALSE);
    }

    //  dbg("state %x\n", kev_state);
    if ((kev_state & ControlMask) && (kev_state & (Mod1Mask | Mod5Mask))) {
        if (keysym == 'g' || keysym == 'r') {
            send_output_buffer_bak ();
            return check_key_press (keysym, kev_state, TRUE);
        }

        if (!hime_enable_ctrl_alt_switch)
            return check_key_press (keysym, kev_state, FALSE);

        int kidx = hime_switch_keys_lookup (keysym);
        if (kidx < 0)
            return check_key_press (keysym, kev_state, FALSE);

        if (inmd[kidx].method_type == method_type_SYMBOL_TABLE) {
            toggle_symbol_table ();
            return check_key_press (keysym, kev_state, TRUE);
        }

        if (!inmd[kidx].cname)
            return check_key_press (keysym, kev_state, FALSE);

        current_CS->b_im_enabled = TRUE;
        init_in_method (kidx);

        return check_key_press (keysym, kev_state, TRUE);
    }

    last_keysym = keysym;

    if (current_fullshape_mode () && !current_CS->b_im_enabled && !(kev_state & (ControlMask | Mod1Mask))) {
        return check_key_press (keysym, kev_state, full_char_proc (keysym));
    }

    if (!current_CS->b_im_enabled) {
        return check_key_press (keysym, kev_state, FALSE);
    }

    if (!current_CS->b_hime_protocol) {
        if (((keysym == XK_Control_L || keysym == XK_Control_R) && (kev_state & ShiftMask)) ||
            ((keysym == XK_Shift_L || keysym == XK_Shift_R) && (kev_state & ControlMask))) {
            cycle_next_in_method ();
            return check_key_press (keysym, kev_state, TRUE);
        }
    }

    if (current_CS->b_raise_window && keysym >= ' ' && keysym < 127) {
        if (timeout_handle)
            g_source_remove (timeout_handle);
        timeout_handle = g_timeout_add (200, timeout_raise_window, NULL);
    }

    if (kev_state & ControlMask) {
        if (feed_phrase (keysym, kev_state))
            return check_key_press (keysym, kev_state, TRUE);
    }

    switch (current_method_type ()) {
    case method_type_PHO:
        return check_key_press (keysym, kev_state, feedkey_pho (keysym, kev_state));
#if USE_TSIN
    case method_type_TSIN:
        return check_key_press (keysym, kev_state, feedkey_pp (keysym, kev_state));
#endif
    case method_type_MODULE: {
        if (!module_cb ())
            return check_key_press (keysym, kev_state, FALSE);
        gboolean response = module_cb ()->module_feedkey (keysym, kev_state);
        if (response)
            hide_win_gtab ();
        else if (current_fullshape_mode ())
            return check_key_press (keysym, kev_state, full_char_proc (keysym));
        return check_key_press (keysym, kev_state, response);
    }
    default:
        return check_key_press (keysym, kev_state, feedkey_gtab (keysym, kev_state));
    }

    return check_key_press (keysym, kev_state, FALSE);
}

int feedkey_pp_release (KeySym xkey, int kbstate);
int feedkey_gtab_release (KeySym xkey, int kbstate);

// return TRUE if the key press is processed
gboolean ProcessKeyRelease (KeySym keysym, uint32_t kev_state) {
    disp_win_kbm_capslock ();

    check_CS ();
#if 0
  dbg_time("key release %x %x\n", keysym, kev_state);
#endif

    if (!current_CS->b_im_enabled)
        return FALSE;

#if 1
    if (current_CS->b_hime_protocol && (last_keysym == XK_Shift_L ||
                                        last_keysym == XK_Shift_R || last_keysym == XK_Control_L || last_keysym == XK_Control_R)) {
        if (((keysym == XK_Control_L || keysym == XK_Control_R) && (kev_state & ShiftMask)) ||
            ((keysym == XK_Shift_L || keysym == XK_Shift_R) && (kev_state & ControlMask))) {
            cycle_next_in_method ();
            if (keysym == XK_Shift_L || keysym == XK_Shift_R)
                key_press_shift = FALSE;
            return TRUE;
        }
    }
#endif

    switch (current_method_type ()) {
    case method_type_TSIN:
        return feedkey_pp_release (keysym, kev_state);
    case method_type_MODULE:
        if (!module_cb ())
            return FALSE;
        return module_cb ()->module_feedkey_release (keysym, kev_state);
    default:
        return feedkey_gtab_release (keysym, kev_state);
    }

    return FALSE;
}

#if USE_XIM
int xim_ForwardEventHandler (IMForwardEventStruct *call_data) {
    current_forward_eve = call_data;

    if (call_data->event.type != KeyPress && call_data->event.type != KeyRelease) {
#if DEBUG || 1
        dbg ("bogus event type, ignored\n");
#endif
        return True;
    }

    char strbuf[STRBUFLEN];
    KeySym keysym;

    memset (strbuf, 0, STRBUFLEN);
    XKeyEvent *kev = (XKeyEvent *) &current_forward_eve->event;
    XLookupString (kev, strbuf, STRBUFLEN, &keysym, NULL);

    int ret;
    if (call_data->event.type == KeyPress)
        ret = ProcessKeyPress (keysym, kev->state);
    else
        ret = ProcessKeyRelease (keysym, kev->state);

    if (!ret)
        bounce_back_key ();

    export_text_xim ();

    return False;
}
#endif

int skip_window (Window win) {
    XWindowAttributes att;
    XGetWindowAttributes (dpy, win, &att);
#if 0
  dbg("hhh %d %d class:%d all:%x %x\n", att.width, att.height, att.class,
    att.all_event_masks, att.your_event_mask);
#endif
    if (att.override_redirect)
        return 1;

    return 0;
}

void hime_reset ();

int hime_FocusIn (ClientState *cs) {
    Window win = cs->client_win;

    reset_current_in_win_xy ();

    if (cs) {
        Window win = cs->client_win;

        if (focus_win != win) {
            hime_reset ();
            hide_in_win (current_CS);
            focus_win = win;
        }
    }

    current_CS = cs;
    save_CS_temp_to_current ();

    if (win == focus_win) {
        if (cs->b_im_enabled) {
            show_in_win (cs);
            move_IC_in_win (cs);
        } else {
            hide_in_win (cs);
        }
    }

    if (inmd[cs->in_method].selkey) {
        set_wselkey (inmd[cs->in_method].selkey);
    } else {
        set_wselkey (pho_selkey);
        gtab_set_win1_cb ();
        tsin_set_win1_cb ();
    }

    update_win_kbm ();

#if TRAY_ENABLED
    disp_tray_icon ();
#endif

    return True;
}

#if USE_XIM
IC *FindIC (CARD16 icid);
void load_IC (IC *rec);
CARD16 connect_id;

int xim_hime_FocusIn (IMChangeFocusStruct *call_data) {
    IC *ic = FindIC (call_data->icid);
    ClientState *cs = &ic->cs;
    connect_id = call_data->connect_id;

    if (ic) {
        hime_FocusIn (cs);

        load_IC (ic);
    }

#if DEBUG
    dbg ("xim_hime_FocusIn %d\n", call_data->icid);
#endif
    return True;
}
#endif

static gint64 last_focus_out_time;

int hime_FocusOut (ClientState *cs) {
    gint64 t = current_time ();

    if (cs != current_CS) {
        return FALSE;
    }

    if (t - last_focus_out_time < 100000) {
        last_focus_out_time = t;
        return FALSE;
    }

    last_focus_out_time = t;

    if (cs == current_CS) {
        hide_in_win (cs);
    }

    reset_current_in_win_xy ();

    if (cs == current_CS) {
        temp_CS = *current_CS;
    }

    return True;
}

int tsin_get_preedit (char *str, HIME_PREEDIT_ATTR attr[], int *cursor, int *sub_comp_len);
int gtab_get_preedit (char *str, HIME_PREEDIT_ATTR attr[], int *pcursor, int *sub_comp_len);
int int_get_preedit (char *str, HIME_PREEDIT_ATTR attr[], int *cursor, int *sub_comp_len);
int pho_get_preedit (char *str, HIME_PREEDIT_ATTR attr[], int *cursor, int *sub_comp_len);

int hime_get_preedit (ClientState *cs,
                      char *str,
                      HIME_PREEDIT_ATTR attr[],
                      int *cursor,
                      int *comp_flag) {

    if (!current_CS) {
        str[0] = '\0';
        *cursor = 0;
        return 0;
    }

    str[0] = '\0';
    *comp_flag = 0;

    switch (current_method_type ()) {
    case method_type_PHO:
        return pho_get_preedit (str, attr, cursor, comp_flag);
#if USE_TSIN
    case method_type_TSIN:
        return tsin_get_preedit (str, attr, cursor, comp_flag);
#endif
    case method_type_MODULE:
        if (inmd[current_CS->in_method].mod_cb_funcs) {
            return module_cb ()->module_get_preedit (str, attr, cursor, comp_flag);
        }
    default:
        return gtab_get_preedit (str, attr, cursor, comp_flag);
    }

    return 0;
}

void pho_reset ();
int tsin_reset ();
void gtab_reset ();

void hime_reset (void) {
    if (!current_CS) {
        return;
    }

    switch (current_method_type ()) {
    case method_type_PHO:
        pho_reset ();
        break;
#if USE_TSIN
    case method_type_TSIN:
        tsin_reset ();
        break;
#endif
    case method_type_MODULE:
        if (inmd[current_CS->in_method].mod_cb_funcs) {
            module_cb ()->module_reset ();
        }
        break;
    default:
        gtab_reset ();
    }
}

#if USE_XIM
int xim_hime_FocusOut (IMChangeFocusStruct *call_data) {
    IC *ic = FindIC (call_data->icid);
    ClientState *cs = &ic->cs;

    hime_FocusOut (cs);

    return True;
}
#endif

gboolean hime_edit_display_ap_only () {
    if (!current_CS)
        return FALSE;
    //  dbg("hime_edit_display_ap_only %d\n", current_CS->use_preedit)
    return current_CS->use_preedit && hime_edit_display == HIME_EDIT_DISPLAY_ON_THE_SPOT;
}

gboolean hime_display_on_the_spot_key () {
    return hime_edit_display_ap_only () && hime_on_the_spot_key;
}

void flush_edit_buffer (void) {
    if (!current_CS) {
        return;
    }

    switch (current_method_type ()) {
#if USE_TSIN
    case method_type_TSIN:
        flush_tsin_buffer ();
        break;
#endif
    case method_type_MODULE:
        if (inmd[current_CS->in_method].mod_cb_funcs) {
            module_cb ()->module_flush_input ();
        }
        break;
    default:
        output_gbuf ();
    }
}

void change_module_font_size () {
    int i;
    for (i = 0; i < inmdN; i++) {
        INMD *pinmd = &inmd[i];
        if (pinmd->method_type != method_type_MODULE || pinmd->disabled)
            continue;
        HIME_module_callback_functions *f = pinmd->mod_cb_funcs;
        if (!f)
            continue;
        if (!f->module_change_font_size)
            continue;
        f->module_change_font_size ();
    }
}
