/*  windows.c
 *
 *
 *  Copyright (C) 2014 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic.
 *
 *  Toxic is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Toxic is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Toxic.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <ctype.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "avatars.h"
#include "chat.h"
#include "file_transfers.h"
#include "friendlist.h"
#include "groupchat.h"
#include "line_info.h"
#include "misc_tools.h"
#include "prompt.h"
#include "settings.h"
#include "toxic.h"
#include "windows.h"

extern char *DATA_FILE;
extern struct Winthread Winthread;

ToxWindow *windows[MAX_WINDOWS_NUM];
static uint8_t active_window_index;
static int num_active_windows;

extern ToxWindow *prompt;
extern struct user_settings *user_settings;

/* CALLBACKS START */
void on_friend_request(Tox *m, const uint8_t *public_key, const uint8_t *data, size_t length, void *userdata)
{
    UNUSED_VAR(userdata);

    char msg[MAX_STR_SIZE + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) data, length);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onFriendRequest != NULL) {
            windows[i]->onFriendRequest(windows[i], m, (const char *) public_key, msg, length);
        }
    }
}

void on_friend_connection_status(Tox *m, uint32_t friendnumber, Tox_Connection connection_status, void *userdata)
{
    UNUSED_VAR(userdata);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onConnectionChange != NULL) {
            windows[i]->onConnectionChange(windows[i], m, friendnumber, connection_status);
        }
    }
}

void on_friend_typing(Tox *m, uint32_t friendnumber, bool is_typing, void *userdata)
{
    UNUSED_VAR(userdata);

    if (user_settings->show_typing_other == SHOW_TYPING_OFF) {
        return;
    }

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onTypingChange != NULL) {
            windows[i]->onTypingChange(windows[i], m, friendnumber, is_typing);
        }
    }
}

void on_friend_message(Tox *m, uint32_t friendnumber, Tox_Message_Type type, const uint8_t *string, size_t length,
                       void *userdata)
{
    UNUSED_VAR(userdata);

    char msg[MAX_STR_SIZE + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) string, length);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onMessage != NULL) {
            windows[i]->onMessage(windows[i], m, friendnumber, type, msg, length);
        }
    }
}

void on_friend_name(Tox *m, uint32_t friendnumber, const uint8_t *string, size_t length, void *userdata)
{
    UNUSED_VAR(userdata);

    char nick[TOXIC_MAX_NAME_LENGTH + 1];
    length = copy_tox_str(nick, sizeof(nick), (const char *) string, length);
    filter_str(nick, length);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onNickChange != NULL) {
            windows[i]->onNickChange(windows[i], m, friendnumber, nick, length);
        }
    }

    store_data(m, DATA_FILE);
}

void on_friend_status_message(Tox *m, uint32_t friendnumber, const uint8_t *string, size_t length, void *userdata)
{
    UNUSED_VAR(userdata);
    UNUSED_VAR(m);

    char msg[TOX_MAX_STATUS_MESSAGE_LENGTH + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) string, length);
    filter_str(msg, length);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onStatusMessageChange != NULL) {
            windows[i]->onStatusMessageChange(windows[i], friendnumber, msg, length);
        }
    }
}

void on_friend_status(Tox *m, uint32_t friendnumber, Tox_User_Status status, void *userdata)
{
    UNUSED_VAR(userdata);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onStatusChange != NULL) {
            windows[i]->onStatusChange(windows[i], m, friendnumber, status);
        }
    }
}

void on_friend_added(Tox *m, uint32_t friendnumber, bool sort)
{
    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onFriendAdded != NULL) {
            windows[i]->onFriendAdded(windows[i], m, friendnumber, sort);
        }
    }

    store_data(m, DATA_FILE);
}

void on_conference_message(Tox *m, uint32_t groupnumber, uint32_t peernumber, Tox_Message_Type type,
                           const uint8_t *message, size_t length, void *userdata)
{
    UNUSED_VAR(userdata);

    char msg[MAX_STR_SIZE + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) message, length);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onGroupMessage != NULL) {
            windows[i]->onGroupMessage(windows[i], m, groupnumber, peernumber, type, msg, length);
        }
    }
}

void on_conference_invite(Tox *m, uint32_t friendnumber, Tox_Conference_Type type, const uint8_t *group_pub_key,
                          size_t length, void *userdata)
{
    UNUSED_VAR(userdata);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onGroupInvite != NULL) {
            windows[i]->onGroupInvite(windows[i], m, friendnumber, type, (char *) group_pub_key, length);
        }
    }
}

void on_conference_peer_list_changed(Tox *m, uint32_t groupnumber, void *userdata)
{
    UNUSED_VAR(userdata);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onGroupNameListChange != NULL) {
            windows[i]->onGroupNameListChange(windows[i], m, groupnumber);
        }
    }
}

void on_conference_peer_name(Tox *m, uint32_t groupnumber, uint32_t peernumber, const uint8_t *name,
                             size_t length, void *userdata)
{
    UNUSED_VAR(userdata);

    char nick[TOXIC_MAX_NAME_LENGTH + 1];
    length = copy_tox_str(nick, sizeof(nick), (const char *) name, length);
    filter_str(nick, length);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onGroupPeerNameChange != NULL) {
            windows[i]->onGroupPeerNameChange(windows[i], m, groupnumber, peernumber, nick, length);
        }
    }
}

void on_conference_title(Tox *m, uint32_t groupnumber, uint32_t peernumber, const uint8_t *title, size_t length,
                         void *userdata)
{
    UNUSED_VAR(userdata);

    char data[MAX_STR_SIZE + 1];
    length = copy_tox_str(data, sizeof(data), (const char *) title, length);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onGroupTitleChange != NULL) {
            windows[i]->onGroupTitleChange(windows[i], m, groupnumber, peernumber, data, length);
        }
    }
}

void on_file_chunk_request(Tox *m, uint32_t friendnumber, uint32_t filenumber, uint64_t position,
                           size_t length, void *userdata)
{
    UNUSED_VAR(userdata);

    struct FileTransfer *ft = get_file_transfer_struct(friendnumber, filenumber);

    if (!ft) {
        return;
    }

    if (ft->file_type == TOX_FILE_KIND_AVATAR) {
        on_avatar_chunk_request(m, ft, position, length);
        return;
    }

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onFileChunkRequest != NULL) {
            windows[i]->onFileChunkRequest(windows[i], m, friendnumber, filenumber, position, length);
        }
    }
}

void on_file_recv_chunk(Tox *m, uint32_t friendnumber, uint32_t filenumber, uint64_t position,
                        const uint8_t *data, size_t length, void *userdata)
{
    UNUSED_VAR(userdata);

    struct FileTransfer *ft = get_file_transfer_struct(friendnumber, filenumber);

    if (!ft) {
        return;
    }

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onFileRecvChunk != NULL) {
            windows[i]->onFileRecvChunk(windows[i], m, friendnumber, filenumber, position, (char *) data, length);
        }
    }
}

void on_file_recv_control(Tox *m, uint32_t friendnumber, uint32_t filenumber, Tox_File_Control control,
                          void *userdata)
{
    UNUSED_VAR(userdata);

    struct FileTransfer *ft = get_file_transfer_struct(friendnumber, filenumber);

    if (!ft) {
        return;
    }

    if (ft->file_type == TOX_FILE_KIND_AVATAR) {
        on_avatar_file_control(m, ft, control);
        return;
    }

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onFileControl != NULL) {
            windows[i]->onFileControl(windows[i], m, friendnumber, filenumber, control);
        }
    }
}

void on_file_recv(Tox *m, uint32_t friendnumber, uint32_t filenumber, uint32_t kind, uint64_t file_size,
                  const uint8_t *filename, size_t filename_length, void *userdata)
{
    UNUSED_VAR(userdata);

    /* We don't care about receiving avatars */
    if (kind != TOX_FILE_KIND_DATA) {
        tox_file_control(m, friendnumber, filenumber, TOX_FILE_CONTROL_CANCEL, NULL);
        return;
    }

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onFileRecv != NULL) {
            windows[i]->onFileRecv(windows[i], m, friendnumber, filenumber, file_size, (char *) filename,
                                   filename_length);
        }
    }
}

void on_friend_read_receipt(Tox *m, uint32_t friendnumber, uint32_t receipt, void *userdata)
{
    UNUSED_VAR(userdata);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onReadReceipt != NULL) {
            windows[i]->onReadReceipt(windows[i], m, friendnumber, receipt);
        }
    }
}
/* CALLBACKS END */

int add_window(Tox *m, ToxWindow *w)
{
    if (LINES < 2) {
        return -1;
    }

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; i++) {
        if (windows[i] != NULL) {
            continue;
        }

        w->index = i;
        w->window = newwin(LINES - 2, COLS, 0, 0);

        if (w->window == NULL) {
            return -1;
        }

#ifdef URXVT_FIX
        /* Fixes text color problem on some terminals. */
        wbkgd(w->window, COLOR_PAIR(6));
#endif
        windows[i] = w;

        if (w->onInit) {
            w->onInit(w, m);
        }

        ++num_active_windows;

        return i;
    }

    return -1;
}

void set_active_window_index(uint8_t index)
{
    if (index < MAX_WINDOWS_NUM) {
        active_window_index = index;
    }
}

/* Shows next window when tab or back-tab is pressed */
void set_next_window(int ch)
{
    if (ch == user_settings->key_next_tab) {
        for (uint8_t i = active_window_index + 1; i < MAX_WINDOWS_NUM; ++i) {
            if (windows[i] != NULL) {
                set_active_window_index(i);
                return;
            }
        }
    } else {
        uint8_t start = active_window_index == 0 ? MAX_WINDOWS_NUM - 1 : active_window_index - 1;

        for (uint8_t i = start; i > 0; --i) {
            if (windows[i] != NULL) {
                set_active_window_index(i);
                return;
            }
        }
    }

    set_active_window_index(0);
}

/* Deletes window w and cleans up */
void del_window(ToxWindow *w)
{
    set_active_window_index(0);

    uint8_t idx = w->index;
    delwin(w->window);
    free(windows[idx]);
    windows[idx] = NULL;

    clear();
    refresh();
    --num_active_windows;
}

ToxWindow *init_windows(Tox *m)
{
    prompt = new_prompt();
    int n_prompt = add_window(m, prompt);

    if (n_prompt < 0) {
        exit_toxic_err("add_window() for prompt failed in init_windows", FATALERR_WININIT);
    }

    if (add_window(m, new_friendlist()) == -1) {
        exit_toxic_err("add_window() for friendlist failed in init_windows", FATALERR_WININIT);
    }

    set_active_window_index(n_prompt);

    return prompt;
}

void on_window_resize(void)
{
    endwin();
    refresh();
    clear();

    /* equivalent to LINES and COLS */
    int x2, y2;
    getmaxyx(stdscr, y2, x2);
    y2 -= 2;

    if (y2 <= 0 || x2 <= 0) {
        return;
    }

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] == NULL) {
            continue;
        }

        ToxWindow *w = windows[i];

        if (windows[i]->is_friendlist)  {
            delwin(w->window);
            w->window = newwin(y2, x2, 0, 0);
            continue;
        }

        if (w->help->active) {
            wclear(w->help->win);
        }

        if (w->is_groupchat) {
            delwin(w->chatwin->sidebar);
            w->chatwin->sidebar = NULL;
        } else {
            delwin(w->stb->topline);
        }

        delwin(w->chatwin->linewin);
        delwin(w->chatwin->history);
        delwin(w->window);

        w->window = newwin(y2, x2, 0, 0);
        w->chatwin->linewin = subwin(w->window, CHATBOX_HEIGHT, x2, y2 - CHATBOX_HEIGHT, 0);

        if (w->show_peerlist) {
            w->chatwin->history = subwin(w->window, y2 - CHATBOX_HEIGHT + 1, x2 - SIDEBAR_WIDTH - 1, 0, 0);
            w->chatwin->sidebar = subwin(w->window, y2 - CHATBOX_HEIGHT + 1, SIDEBAR_WIDTH, 0, x2 - SIDEBAR_WIDTH);
        } else {
            w->chatwin->history = subwin(w->window, y2 - CHATBOX_HEIGHT + 1, x2, 0, 0);

            if (!w->is_groupchat) {
                w->stb->topline = subwin(w->window, 2, x2, 0, 0);
            }
        }

#ifdef AUDIO

        if (w->chatwin->infobox.active) {
            delwin(w->chatwin->infobox.win);
            w->chatwin->infobox.win = newwin(INFOBOX_HEIGHT, INFOBOX_WIDTH + 1, 1, x2 - INFOBOX_WIDTH);
        }

#endif /* AUDIO */

        scrollok(w->chatwin->history, 0);
    }
}

static void draw_window_tab(ToxWindow *toxwin)
{
    pthread_mutex_lock(&Winthread.lock);

    if (toxwin->alert != WINDOW_ALERT_NONE) {
        attron(COLOR_PAIR(toxwin->alert));
    }

    pthread_mutex_unlock(&Winthread.lock);

    clrtoeol();
    printw(" [%s]", toxwin->name);

    pthread_mutex_lock(&Winthread.lock);

    if (toxwin->alert != WINDOW_ALERT_NONE) {
        attroff(COLOR_PAIR(toxwin->alert));
    }

    pthread_mutex_unlock(&Winthread.lock);
}

static void draw_bar(void)
{
    int y, x;

    ToxWindow *w = windows[active_window_index];

    if (w == NULL) {
        return;
    }

    // save current cursor position
    getyx(w->window, y, x);

    attron(COLOR_PAIR(BLUE));
    mvhline(LINES - 2, 0, '_', COLS);
    attroff(COLOR_PAIR(BLUE));

    move(LINES - 1, 0);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] == NULL) {
            continue;
        }

        if (i == active_window_index) {

#ifdef URXVT_FIX
            attron(A_BOLD | COLOR_PAIR(GREEN));
        } else {
#endif

            attron(A_BOLD);
        }

        draw_window_tab(windows[i]);

        if (i == active_window_index) {

#ifdef URXVT_FIX
            attroff(A_BOLD | COLOR_PAIR(GREEN));
        } else {
#endif

            attroff(A_BOLD);
        }
    }

    // restore cursor position after drawing
    move(y, x);

    refresh();
}

void draw_active_window(Tox *m)
{
    ToxWindow *a = windows[active_window_index];

    if (a == NULL) {
        return;
    }

    pthread_mutex_lock(&Winthread.lock);
    a->alert = WINDOW_ALERT_NONE;
    pthread_mutex_unlock(&Winthread.lock);

    wint_t ch = 0;

    draw_bar();

    touchwin(a->window);
    a->onDraw(a, m);
    wrefresh(a->window);

    /* Handle input */
    bool ltr;
#ifdef HAVE_WIDECHAR
    int status = wget_wch(stdscr, &ch);

    if (status == ERR) {
        return;
    }

    if (status == OK) {
        ltr = iswprint(ch);
    } else { /* if (status == KEY_CODE_YES) */
        ltr = false;
    }

#else
    ch = getch();

    if (ch == ERR) {
        return;
    }

    /* TODO verify if this works */
    ltr = isprint(ch);
#endif /* HAVE_WIDECHAR */

    if (!ltr && (ch == user_settings->key_next_tab || ch == user_settings->key_prev_tab)) {
        set_next_window((int) ch);
    } else {
        pthread_mutex_lock(&Winthread.lock);
        a->onKey(a, m, ch, ltr);
        pthread_mutex_unlock(&Winthread.lock);
    }
}

/* refresh inactive windows to prevent scrolling bugs.
   call at least once per second */
void refresh_inactive_windows(void)
{
    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        ToxWindow *toxwin = windows[i];

        if (toxwin == NULL) {
            continue;
        }

        if (i != active_window_index && !toxwin->is_friendlist) {
            pthread_mutex_lock(&Winthread.lock);
            line_info_print(toxwin);
            pthread_mutex_unlock(&Winthread.lock);
        }
    }
}

/* Returns a pointer to the ToxWindow in the ith index.
 * Returns NULL if no ToxWindow exists.
 */
ToxWindow *get_window_ptr(size_t index)
{
    if (index >= MAX_WINDOWS_NUM) {
        return NULL;
    }

    return windows[index];
}

/* Returns a pointer to the currently active ToxWindow. */
ToxWindow *get_active_window(void)
{
    return windows[active_window_index];
}

void force_refresh(WINDOW *w)
{
    wclear(w);
    endwin();
    refresh();
}

int get_num_active_windows(void)
{
    return num_active_windows;
}

/* destroys all chat and groupchat windows (should only be called on shutdown) */
void kill_all_windows(Tox *m)
{
    for (uint8_t i = 2; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] == NULL) {
            continue;
        }

        if (windows[i]->is_chat) {
            kill_chat_window(windows[i], m);
        } else if (windows[i]->is_groupchat) {
            free_groupchat(windows[i], windows[i]->num);
        }
    }

    /* TODO: use enum instead of magic indices */
    kill_friendlist(windows[1]);
    kill_prompt_window(windows[0]);
}
