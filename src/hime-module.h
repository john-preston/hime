/* Copyright (C) 2011 Edward Der-Hua Liu, Hsin-Chu, Taiwan
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

#ifndef HIME_MODULE_H
#define HIME_MODULE_H

#include "pho.h"
#include "win1.h"

// optional functions/data for hime module to use, please refer to anthy.cpp
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    void (*mf_show_win_sym) ();
    void (*mf_hide_win_sym) ();
    void (*mf_move_win_sym) ();
    void (*mf_toggle_win_sym) ();

    // display in tsin's selection window
    void (*mf_init_tsin_selection_win) ();
    void (*mf_clear_sele) ();
    void (*mf_disp_selections) (int x, int y);
    void (*mf_hide_selections_win) ();
    void (*mf_disp_arrow_up) ();
    void (*mf_disp_arrow_down) ();
    void (*mf_set_sele_text) (int selN, int sel_idx, char *text, int len);
    void (*mf_set_win1_cb) (cb_selec_by_idx_t selc_by_idx, cb_page_ud_t cb_page_up, cb_page_ud_t cb_page_down);

    void (*mf_set_eng_ch_mode) (gboolean mode);
    void (*mf_toggle_eng_ch_mode) (void);
    void (*mf_set_chinese_mode) (void);
    gboolean (*mf_chinese_mode) (void);

    int (*mf_get_widget_xy) (GtkWidget *win, GtkWidget *widget, int *rx, int *ry);
    void (*mf_get_win_size) (GtkWidget *win, int *width, int *height);
    void (*mf_change_win_bg) (GtkWidget *win);
    void (*mf_set_label_font_size) (GtkWidget *label, int size);
    void (*mf_set_no_focus) (GtkWidget *win);

    gint64 (*mf_current_time) ();
    int (*mf_current_fullshape_mode) ();

    void (*mf_exec_hime_setup) ();
    gboolean (*mf_hime_edit_display_ap_only) ();
    gboolean (*mf_hime_display_on_the_spot_key) ();
    gint (*mf_inmd_switch_popup_handler) (GtkWidget *widget, GdkEvent *event);
    void (*mf_load_tab_pho_file) ();
    int (*mf_utf8_str_N) (char *str);

    // call this function to return the string
    void (*mf_send_text) (char *str);
    void (*mf_send_utf8_ch) (char *str);

    PHOKBM *mf_phkbm;
    TSIN_ST *mf_tss;
    int *mf_hime_chinese_english_toggle_key;
    char **mf_pho_chars;

    int *mf_hime_pop_up_win;
    int *mf_hime_font_size, *mf_hime_win_color_use;
    char **mf_hime_win_color_fg, **mf_pho_selkey, **mf_tsin_cursor_color;
    gboolean *mf_force_show;
    int *mf_win_x, *mf_win_y, *mf_win_xl, *mf_win_yl, *mf_dpy_xl, *mf_dpy_yl;
    gboolean *mf_key_press_shift;
} HIME_module_main_functions;
#ifdef __cplusplus
}
#endif

void init_HIME_module_main_functions (HIME_module_main_functions *func);

#endif /* HIME_MODULE_H */
