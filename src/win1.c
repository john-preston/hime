/*
 * Copyright (C) 2020 The HIME team, Taiwan
 * Copyright (C) 2004-2012 Edward Der-Hua Liu, Hsin-Chu, Taiwan
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

#include "hime.h"

#include "gst.h"
#include "pho.h"
#include "win1.h"

GtkWidget *gwin1;
static GtkWidget *frame;
static char wselkey[16];
static int wselkeyN;
// Window xwin1;

#define SELEN (15)

static GtkWidget *labels_sele[SELEN], *labels_seleR[SELEN];
static GtkWidget *eve_sele[SELEN], *eve_seleR[SELEN];
static GtkWidget *arrow_up, *arrow_down;

void hide_selections_win ();
static cb_page_ud_t cb_page_up, cb_page_down;
static int c_config;

static int current_config () {
    return (tsin_tail_select_key << 9) | (pho_candidate_col_N << 5) | wselkeyN << 1 |
           pho_candidate_R2L;
}

static int idx_to_x (int tN, int i) {
    if (tN > pho_candidate_col_N)
        tN = pho_candidate_col_N;

    int x = i % tN;
    if (pho_candidate_R2L)
        x = tN - 1 - x;
    return x;
}

static gboolean button_scroll_event_tsin (GtkWidget *widget, GdkEventScroll *event, gpointer user_data) {
    switch (event->direction) {
    case GDK_SCROLL_UP:
        if (cb_page_up)
            cb_page_up ();
        break;
    case GDK_SCROLL_DOWN:
        if (cb_page_down)
            cb_page_down ();
        break;
    default:
        break;
    }

    return TRUE;
}

void create_win1 () {
    if (gwin1)
        return;

    gwin1 = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_has_resize_grip (GTK_WINDOW (gwin1), FALSE);
    gtk_widget_realize (gwin1);

    set_no_focus (gwin1);

    g_signal_connect (G_OBJECT (gwin1), "scroll-event", G_CALLBACK (button_scroll_event_tsin), NULL);
}

void change_win1_font (), force_preedit_shift ();

static cb_selec_by_idx_t cb_sele_by_idx;

static void mouse_button_callback (GtkWidget *widget, GdkEventButton *event, gpointer data) {
    int v;
    switch (event->button) {
    case 1:
        v = GPOINTER_TO_INT (data);
        if (cb_sele_by_idx)
            cb_sele_by_idx (v);
        force_preedit_shift ();
        break;
    }
}

static void cb_arrow_up (GtkWidget *button, gpointer user_data) {
    if (cb_page_up)
        cb_page_up ();
}
static void cb_arrow_down (GtkWidget *button, gpointer user_data) {
    if (cb_page_down)
        cb_page_down ();
}

void set_win1_cb (cb_selec_by_idx_t sele_by_idx, cb_page_ud_t page_up, cb_page_ud_t page_down) {
    cb_sele_by_idx = sele_by_idx;
    cb_page_up = page_up;
    cb_page_down = page_down;
}

void create_win1_gui (void) {
    if (frame)
        return;

    frame = gtk_frame_new (NULL);
    gtk_container_add (GTK_CONTAINER (gwin1), frame);

    GtkWidget *vbox_top = gtk_vbox_new (FALSE, 0);
    gtk_orientable_set_orientation (GTK_ORIENTABLE (vbox_top), GTK_ORIENTATION_VERTICAL);
    gtk_container_add (GTK_CONTAINER (frame), vbox_top);

    GtkWidget *eve_box_up = gtk_event_box_new ();

    gtk_event_box_set_visible_window (GTK_EVENT_BOX (eve_box_up), FALSE);
    gtk_box_pack_start (GTK_BOX (vbox_top), eve_box_up, FALSE, FALSE, 0);
    arrow_up = gtk_arrow_new (GTK_ARROW_UP, GTK_SHADOW_IN);
    gtk_container_add (GTK_CONTAINER (eve_box_up), arrow_up);
    g_signal_connect (
        G_OBJECT (eve_box_up), "button-press-event",
        G_CALLBACK (cb_arrow_up), NULL);

    int c_rowN = (wselkeyN + pho_candidate_col_N - 1) / pho_candidate_col_N * pho_candidate_col_N;
    int tablecolN = pho_candidate_col_N;

    if (!tsin_tail_select_key)
        tablecolN *= 2;

    c_config = current_config ();

#if GTK_CHECK_VERSION(3, 0, 0)
    GtkWidget *table = gtk_grid_new ();
#else
    GtkWidget *table = gtk_table_new (c_rowN, tablecolN, FALSE);
#endif
    gtk_box_pack_start (GTK_BOX (vbox_top), table, FALSE, FALSE, 0);

    int i;
    for (i = 0; i < wselkeyN; i++) {
        int y = i / pho_candidate_col_N;
        int x = idx_to_x (SELEN + 1, i);

        if (!tsin_tail_select_key)
            x *= 2;

        GtkWidget *event_box_pho = gtk_event_box_new ();

#if GTK_CHECK_VERSION(3, 0, 0)
        gtk_grid_attach (GTK_GRID (table), event_box_pho, x, y, 1, 1);
#else
        GtkWidget *align = gtk_alignment_new (0, 0, 0, 0);
        gtk_table_attach (
            GTK_TABLE (table), align, x, x + 1, y, y + 1,
            GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
        gtk_container_add (GTK_CONTAINER (align), event_box_pho);
#endif

        gtk_event_box_set_visible_window (GTK_EVENT_BOX (event_box_pho), FALSE);

        GtkWidget *label = gtk_label_new (NULL);
        gtk_container_add (GTK_CONTAINER (event_box_pho), label);
        labels_sele[i] = label;

#if GTK_CHECK_VERSION(3, 0, 0)
        gtk_widget_set_halign (label, GTK_ALIGN_START);
#endif
        eve_sele[i] = event_box_pho;

        gtk_label_set_justify (GTK_LABEL (labels_sele[i]), GTK_JUSTIFY_LEFT);
        set_label_font_size (labels_sele[i], hime_font_size_tsin_presel);
        g_signal_connect (
            G_OBJECT (event_box_pho), "button-press-event",
            G_CALLBACK (mouse_button_callback), GINT_TO_POINTER (i));

        if (!tsin_tail_select_key) {
            GtkWidget *event_box_phoR = gtk_event_box_new ();
#if GTK_CHECK_VERSION(3, 0, 0)
            gtk_grid_attach (GTK_GRID (table), event_box_phoR, x + 1, y, 1, 1);
            gtk_widget_set_halign (event_box_phoR, GTK_ALIGN_START);
#else
            GtkWidget *alignR = gtk_alignment_new (0, 0, 0, 0);
            gtk_container_add (GTK_CONTAINER (alignR), event_box_phoR);
            gtk_table_attach (
                GTK_TABLE (table), alignR, x + 1, x + 2, y, y + 1,
                GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
#endif
            gtk_event_box_set_visible_window (GTK_EVENT_BOX (event_box_phoR), FALSE);
            GtkWidget *labelR = gtk_label_new (NULL);
            gtk_container_add (GTK_CONTAINER (event_box_phoR), labelR);
            labels_seleR[i] = labelR;
            eve_seleR[i] = event_box_phoR;
            gtk_label_set_justify (GTK_LABEL (labels_sele[i]), GTK_JUSTIFY_LEFT);
            set_label_font_size (labels_seleR[i], hime_font_size_tsin_presel);
            g_signal_connect (
                G_OBJECT (event_box_phoR), "button-press-event",
                G_CALLBACK (mouse_button_callback), GINT_TO_POINTER (i));
        }
    }

    GtkWidget *eve_box_down = gtk_event_box_new ();
    gtk_event_box_set_visible_window (GTK_EVENT_BOX (eve_box_down), FALSE);
    gtk_box_pack_start (GTK_BOX (vbox_top), eve_box_down, FALSE, FALSE, 0);
    arrow_down = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_IN);
    gtk_container_add (GTK_CONTAINER (eve_box_down), arrow_down);
    g_signal_connect (
        G_OBJECT (eve_box_down), "button-press-event",
        G_CALLBACK (cb_arrow_down), NULL);

    gtk_widget_show_all (gwin1);
    gtk_widget_hide (gwin1);

    change_win1_font ();
}

void init_tsin_selection_win () {
    create_win1 ();
    create_win1_gui ();
}

void clear_sele () {
    int i;

    if (!gwin1)
        return;

    for (i = 0; i < wselkeyN; i++) {
        gtk_widget_hide (labels_sele[i]);
        if (labels_seleR[i])
            gtk_widget_hide (labels_seleR[i]);
    }

    gtk_widget_hide (arrow_up);
    gtk_widget_hide (arrow_down);
    hide_selections_win ();
}

char *htmlspecialchars (char *s, char out[]);

void set_sele_text (int tN, int i, char *text, int len) {
    if (len < 0)
        len = strlen (text);

    char tt[128];
    char utf8[128];

    memcpy (utf8, text, len);
    utf8[len] = 0;
    char uu[32], selma[128];

    char cc[2];
    cc[0] = wselkey[i];
    cc[1] = 0;
    char ul[128];
    ul[0] = 0;

#if 0
  if (tss.sel_pho && i==tss.pho_menu_idx)
    strcpy(ul, " foreground=\"yellow\" background=\"black\"");
  else
    snprintf(ul, sizeof(ul), "foreground=\"%s\"", hime_sel_key_color);
#endif
    if (tss.sel_pho && i == tss.pho_menu_idx) {
        if (hime_win_color_use)
            snprintf (ul, sizeof (ul), "foreground=\"white\" background=\"%s\"", tsin_cursor_color);
        else
            strcpy (ul, "foreground=\"white\" background=\"" TSIN_CURSOR_COLOR_DEFAULT "\"");
    } else {
        if (hime_win_color_use)
            snprintf (ul, sizeof (ul), "foreground=\"%s\"", hime_sel_key_color);
        else
            strcpy (ul, "foreground=\"" HIME_SEL_KEY_COLOR_DEFAULT "\"");
    }

    snprintf (selma, sizeof (selma), "<span %s>%s</span>", ul, htmlspecialchars (cc, uu));

    int x = idx_to_x (tN, i);
    char *sep = x ? " " : "";

    if (tsin_tail_select_key) {
        char vv[128];
        snprintf (tt, sizeof (tt), "%s%s%s", sep, htmlspecialchars (utf8, vv), selma);
    } else {
        gtk_label_set_text (GTK_LABEL (labels_seleR[i]), utf8);
        gtk_widget_show (labels_seleR[i]);
        snprintf (tt, sizeof (tt), "%s%s", sep, selma);
    }

    gtk_widget_show (labels_sele[i]);
    //  dbg("tt %s\n", tt);
    gtk_label_set_markup (GTK_LABEL (labels_sele[i]), tt);
}

void raise_tsin_selection_win () {
    if (gwin1 && gtk_widget_get_visible (gwin1))
        gtk_window_present (GTK_WINDOW (gwin1));
}

void getRootXY (Window win, int wx, int wy, int *tx, int *ty);
void disp_selections (int x, int y) {
    if (!gwin1)
        p_err ("disp_selections !gwin1");

    if (y < 0) {
        int tx;
        if (hime_edit_display_ap_only ())
            getRootXY (current_CS->client_win, current_CS->spot_location.x, current_CS->spot_location.y, &tx, &y);
        else
            y = win_y + win_yl;
    }

    int win1_xl, win1_yl;
    get_win_size (gwin1, &win1_xl, &win1_yl);

    if (x < 0) {
        x = win_x + win_xl - win1_xl;
        if (x < win_x)
            x = win_x;
    }

    if (x + win1_xl > dpy_xl)
        x = dpy_xl - win1_xl;

    if (y + win1_yl > dpy_yl)
        y = win_y - win1_yl;

    if (!gtk_widget_get_visible (gwin1)) {
        gtk_widget_show (gwin1);
    }
    gtk_window_move (GTK_WINDOW (gwin1), x, y);
}

void hide_selections_win () {
    if (!gwin1)
        return;
    gtk_widget_hide (gwin1);
}

void disp_arrow_up () {
    gtk_widget_show (arrow_up);
}

void disp_arrow_down () {
    gtk_widget_show (arrow_down);
}

#if USE_TSIN
void destroy_win1 () {
    if (!gwin1)
        return;
    gtk_widget_destroy (gwin1);
    frame = NULL;
    gwin1 = NULL;
}
#endif

void change_win1_font (void) {
    if (!frame)
        return;

    GdkRGBA fg;
    gdk_rgba_parse (&fg, hime_win_color_fg);

    for (int i = 0; i < wselkeyN; i++) {
        set_label_font_size (labels_sele[i], hime_font_size_tsin_presel);
        set_label_font_size (labels_seleR[i], hime_font_size_tsin_presel);

#if !GTK_CHECK_VERSION(3, 0, 0)
        if (labels_sele[i])
            gtk_widget_modify_fg (labels_sele[i], GTK_STATE_NORMAL, hime_win_color_use ? &fg : NULL);
        if (labels_seleR[i])
            gtk_widget_modify_fg (labels_seleR[i], GTK_STATE_NORMAL, hime_win_color_use ? &fg : NULL);
#else
        if (labels_sele[i])
            gtk_widget_override_color (labels_sele[i], GTK_STATE_FLAG_NORMAL, hime_win_color_use ? &fg : NULL);
        if (labels_seleR[i])
            gtk_widget_override_color (labels_seleR[i], GTK_STATE_FLAG_NORMAL, hime_win_color_use ? &fg : NULL);
#endif
        change_win_bg (eve_sele[i]);
        if (eve_seleR[i])
            change_win_bg (eve_seleR[i]);
    }

    change_win_bg (gwin1);
}

void recreate_win1_if_nessary () {
    //  dbg("%x %x\n", current_config(), c_config);

    if (!gwin1)
        return;

    if (current_config () != c_config) {
        c_config = current_config ();
        //    dbg("destroy frame\n");
        memset (labels_sele, 0, sizeof (labels_sele));
        memset (labels_seleR, 0, sizeof (labels_seleR));
        memset (eve_sele, 0, sizeof (eve_sele));
        memset (eve_seleR, 0, sizeof (eve_seleR));
        gtk_widget_destroy (frame);
        frame = NULL;
        create_win1_gui ();
    }
}

void set_wselkey (char *s) {
    if (strcmp (wselkey, s)) {
        memset (wselkey, 0x00, 16);
        memcpy (wselkey, s, strlen (s));
        wselkeyN = strlen (s);
        recreate_win1_if_nessary ();
    }
}
