#include <ncurses.h>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>



typedef struct {
    char name[256];
    char size[16];
    char modTime[20];
    char perms[11];
    off_t SizeFile;
    int is_dir;
} FileEntry;



typedef struct {
    FileEntry entries[1024];
    int count;
    int selected;
    int scroll;
    char path[1024];
}Panel;

Panel left, right;
int active_panel = 0;          
int term_rows, term_cols;



void Make_Colors() {

    start_color();
    init_pair(1, COLOR_YELLOW, COLOR_BLACK);   
    init_pair(2, COLOR_GREEN, COLOR_BLACK);    
    init_pair(3, COLOR_CYAN, COLOR_BLACK);     
    init_pair(4, COLOR_WHITE, COLOR_BLUE);     
}

void getFileInfo(const char *fullpath, const char *name, FileEntry *e) {
    struct stat st;
    if (lstat(fullpath, &st) == -1) {
        e->is_dir = 0;
        strcpy(e->size, "?");
        strcpy(e->modTime, "?");
        strcpy(e->perms, "?");
        return;
    }

    e->is_dir = S_ISDIR(st.st_mode);
    e->SizeFile = st.st_size;

    if (e->is_dir) {
        strcpy(e->size, "<DIR>");
    } else {
        if (st.st_size < 1024)
            snprintf(e->size, sizeof(e->size), "%ld", (long)st.st_size);
        else if (st.st_size < 1024*1024)
            snprintf(e->size, sizeof(e->size), "%.1fK", (double)st.st_size/1024);
        else if (st.st_size < 1024*1024*1024)
            snprintf(e->size, sizeof(e->size), "%.1fM", (double)st.st_size/(1024*1024));
        else
            snprintf(e->size, sizeof(e->size), "%.1fG", (double)st.st_size/(1024*1024*1024));
    }

    struct tm *tm = localtime(&st.st_mtime);
    time_t now = time(NULL);
    if (st.st_mtime > now - 180*24*3600)
        strftime(e->modTime, sizeof(e->modTime), "%b %d %H:%M", tm);
    else
        strftime(e->modTime, sizeof(e->modTime), "%b %d  %Y", tm);

    snprintf(e->perms, sizeof(e->perms),
             "%c%c%c%c%c%c%c%c%c%c",
             S_ISDIR(st.st_mode) ? 'd' : (S_ISLNK(st.st_mode) ? 'l' : '-'),
             st.st_mode & S_IRUSR ? 'r' : '-',
             st.st_mode & S_IWUSR ? 'w' : '-',
             st.st_mode & S_IXUSR ? 'x' : '-',
             st.st_mode & S_IRGRP ? 'r' : '-',
             st.st_mode & S_IWGRP ? 'w' : '-',
             st.st_mode & S_IXGRP ? 'x' : '-',
             st.st_mode & S_IROTH ? 'r' : '-',
             st.st_mode & S_IWOTH ? 'w' : '-',
             st.st_mode & S_IXOTH ? 'x' : '-');

    strncpy(e->name, name, 256-1);
    e->name[256-1] = '\0';
}

int EntryCmp(const void *a, const void *b) {
    const FileEntry *fa = (const FileEntry*)a;
    const FileEntry *fb = (const FileEntry*)b;
    if (fa->is_dir && !fb->is_dir) return -1;
    if (!fa->is_dir && fb->is_dir) return 1;
    return strcasecmp(fa->name, fb->name);
}

int load_directory(Panel *p) {
    DIR *dir = opendir(p->path);
    if (!dir) return 0;

    struct dirent *de;
    FileEntry temp[1024];
    int cnt = 0;

    while ((de = readdir(dir)) != NULL && cnt < 1024) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        char full[1024];
        snprintf(full, sizeof(full), "%s/%s", p->path, de->d_name);
        getFileInfo(full, de->d_name, &temp[cnt]);
        cnt++;
    }
    closedir(dir);

    qsort(temp, cnt, sizeof(FileEntry), EntryCmp);

    p->entries[0].is_dir = 1;
    strcpy(p->entries[0].name, "..");
    strcpy(p->entries[0].size, "UP-DIR");
    strcpy(p->entries[0].modTime, "");
    strcpy(p->entries[0].perms, "d--------");
    p->entries[0].SizeFile = 0;

    for (int i = 0; i < cnt; i++)
        p->entries[i+1] = temp[i];

    p->count = cnt + 1;
    if (p->selected >= p->count) p->selected = 0;
    if (p->scroll > p->selected) p->scroll = p->selected;
    return 1;
}

void MakePanel(Panel *p, const char *path) {
    strncpy(p->path, path, 1024-1);
    p->path[1024-1] = '\0';
    p->selected = 0;
    p->scroll = 0;
    load_directory(p);
}

void ChangeDiry(Panel *p, const char *name) {
    char newpath[1024];
    if (strcmp(name, "..") == 0) {
        char *slash = strrchr(p->path, '/');
        if (slash == p->path)   
            strcpy(newpath, "/");
        else if (slash)
            *slash = '\0';
        else
            strcpy(newpath, ".");
        strcpy(p->path, newpath);
    } else {
        if (p->path[strlen(p->path)-1] == '/')
            snprintf(newpath, sizeof(newpath), "%s%s", p->path, name);
        else
            snprintf(newpath, sizeof(newpath), "%s/%s", p->path, name);
        strcpy(p->path, newpath);
    }
    load_directory(p);
}

void PaintPanel(int x, int y, int w, int h, Panel *p, int is_active) {
    
    if (is_active) attron(COLOR_PAIR(4) | A_BOLD);
    mvhline(y, x, ' ', w);
    mvprintw(y, x, "+");
    for (int i = 1; i < w-1; i++) mvaddch(y, x+i, '-');
    mvprintw(y, x+w-1, "+");

    for (int i = 1; i < h-1; i++) {
        mvaddch(y+i, x, '|');
        mvaddch(y+i, x+w-1, '|');
    }

    mvhline(y+h-1, x, ' ', w);
    mvprintw(y+h-1, x, "+");
    for (int i = 1; i < w-1; i++) mvaddch(y+h-1, x+i, '-');
    mvprintw(y+h-1, x+w-1, "+");
    if (is_active) attroff(COLOR_PAIR(4) | A_BOLD);

    
    char path_disp[1024];
    int path_len = strlen(p->path);
    if (path_len > w-4) {
        snprintf(path_disp, sizeof(path_disp), "...%s", p->path + path_len - (w-7));
    } else {
        strcpy(path_disp, p->path);
    }
    mvprintw(y, x+2, " %s ", path_disp);

    
    mvprintw(y+1, x+2, "Name");
    mvprintw(y+1, x+27, "Size");
    mvprintw(y+1, x+38, "Modify");
    mvprintw(y+1, x+54, "Perms");

    
    int line = y+2;
    int max_lines = h-4;
    int from = p->scroll;
    int to = from + max_lines;
    if (to > p->count) to = p->count;

    for (int i = from; i < to; i++, line++) {
        if (i == p->selected) attron(A_REVERSE);
        else if (p->entries[i].is_dir) attron(COLOR_PAIR(1) | A_BOLD);
        else if (p->entries[i].perms[3] == 'x') attron(COLOR_PAIR(2));

        char name_disp[256];
        strncpy(name_disp, p->entries[i].name, 20);
        name_disp[20] = '\0';
        if (p->entries[i].is_dir && strcmp(name_disp, "..") != 0) {
            mvprintw(line, x+2, "/%-19s", name_disp+1);
        } else {
            mvprintw(line, x+2, " %-19s", name_disp);
        }

        mvprintw(line, x+27, "%-8s", p->entries[i].size);
        mvprintw(line, x+38, "%-12s", p->entries[i].modTime);
        mvprintw(line, x+54, "%s", p->entries[i].perms);

        if (i == p->selected) attroff(A_REVERSE);
        else {
            if (p->entries[i].is_dir) attroff(COLOR_PAIR(1) | A_BOLD);
            else if (p->entries[i].perms[3] == 'x') attroff(COLOR_PAIR(2));
        }
    }

    
    if (p->scroll > 0) mvaddch(y+2, x+w-2, '^');
    if (to < p->count) mvaddch(y+h-2, x+w-2, 'v');
}


void CopyFile(Panel *src, Panel *dst) {
    if (src->selected == 0) return;
    char src_path[1024], dst_path[1024];
    snprintf(src_path, sizeof(src_path), "%s/%s", src->path, src->entries[src->selected].name);
    snprintf(dst_path, sizeof(dst_path), "%s/%s", dst->path, src->entries[src->selected].name);

    FILE *in = fopen(src_path, "rb");
    if (!in) return;
    FILE *out = fopen(dst_path, "wb");
    if (!out) { fclose(in); return; }

    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, n, out);

    fclose(in); fclose(out);
    load_directory(dst);
}

void move_file(Panel *src, Panel *dst) {
    if (src->selected == 0) return;
    char src_path[1024], dst_path[1024];
    snprintf(src_path, sizeof(src_path), "%s/%s", src->path, src->entries[src->selected].name);
    snprintf(dst_path, sizeof(dst_path), "%s/%s", dst->path, src->entries[src->selected].name);
    rename(src_path, dst_path);
    load_directory(src);
    load_directory(dst);
}

void delete_file(Panel *p) {
    if (p->selected == 0) return;
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", p->path, p->entries[p->selected].name);
    if (p->entries[p->selected].is_dir)
        rmdir(path);
    else
        unlink(path);
    load_directory(p);
}

void mkdir_panel(Panel *p, const char *name) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", p->path, name);
    mkdir(path, 0755);
    load_directory(p);
}

void ShowFileInfo(Panel *p) {
    if (p->selected == 0) return;
    FileEntry *e = &p->entries[p->selected];
    clear();
    mvprintw(2, 2, "File Information:");
    mvprintw(4, 4, "Name: %s", e->name);
    mvprintw(5, 4, "Path: %s/%s", p->path, e->name);
  
    mvprintw(6, 4, "Size: %lld bytes", (long long)e->SizeFile);
    mvprintw(7, 4, "Permissions: %s", e->perms);
    mvprintw(8, 4, "Modified: %s", e->modTime);
    mvprintw(10, 2, "Press any key...");
    refresh();
    getch();
}

int confirm(const char *msg) {
    mvprintw(term_rows-3, 2, "%s (y/n): ", msg);
    refresh();
    int ch = getch();
    return (ch == 'y' || ch == 'Y');
}

void input_str(const char *prompt, char *buf, int len) {
    echo();
    curs_set(1);
    mvprintw(term_rows-3, 2, "%s: ", prompt);
    refresh();
    getnstr(buf, len-1);
    noecho();
    curs_set(0);
}

int main() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (has_colors()) Make_Colors();

    getmaxyx(stdscr, term_rows, term_cols);
    int panel_w = term_cols / 2 - 3;
    int panel_h = term_rows - 5;

    MakePanel(&left, ".");
    MakePanel(&right, ".");

    int ch;
    while (1) {
        clear();

        PaintPanel(1, 1, panel_w, panel_h, &left, active_panel == 0);
        PaintPanel(panel_w + 4, 1, panel_w, panel_h, &right, active_panel == 1);
        mvprintw(term_rows-3, 1,
                 "F1:Help  F3:View  F5:Copy  F6:Move  F7:Mkdir  F8:Delete  F9:Info  F10:Quit");
        mvprintw(term_rows-2, 1, "Tab - switch panels, Enter - open, Backspace - up, q - quit");
        refresh();

        ch = getch();
        Panel *cur = active_panel ? &right : &left;
        Panel *other = active_panel ? &left : &right;

        switch (ch) {
            case '\t':
                active_panel = !active_panel;
                break;
            case KEY_UP:
                if (cur->selected > 0) {
                    cur->selected--;
                    if (cur->selected < cur->scroll)
                        cur->scroll = cur->selected;
                }
                break;
            case KEY_DOWN:
                if (cur->selected < cur->count-1) {
                    cur->selected++;
                    if (cur->selected >= cur->scroll + panel_h - 4)
                        cur->scroll = cur->selected - (panel_h - 5);
                }
                break;
            case KEY_PPAGE:
                cur->selected -= panel_h - 4;
                if (cur->selected < 0) cur->selected = 0;
                cur->scroll = cur->selected;
                break;
            case KEY_NPAGE:
                cur->selected += panel_h - 4;
                if (cur->selected >= cur->count) cur->selected = cur->count-1;
                if (cur->selected >= cur->scroll + panel_h - 4)
                    cur->scroll = cur->selected - (panel_h - 5);
                break;
            case KEY_HOME:
                cur->selected = 0;
                cur->scroll = 0;
                break;
            case KEY_END:
                cur->selected = cur->count-1;
                if (cur->selected >= panel_h - 4)
                    cur->scroll = cur->selected - (panel_h - 5);
                break;
            case 10:
            case KEY_ENTER:
                if (cur->selected >= 0 && cur->selected < cur->count) {
                    if (strcmp(cur->entries[cur->selected].name, "..") == 0 ||
                         cur->entries[cur->selected].is_dir) {
                        ChangeDiry(cur, cur->entries[cur->selected].name);
                    } else {
                        ShowFileInfo(cur);
                    }
                }
                break;
            case 127:
            case KEY_BACKSPACE:
                ChangeDiry(cur, "..");
                break;
            case KEY_F(3):
                ShowFileInfo(cur);
                break;
            case KEY_F(5):
                if (cur->selected > 0 && confirm("Copy?"))
                    CopyFile(cur, other);
                break;
            case KEY_F(6):
                if (cur->selected > 0 && confirm("Move?"))
                    move_file(cur, other);
                break;
            case KEY_F(7): {
                char dirname[256] = "";
                input_str("New folder name", dirname, sizeof(dirname));
                if (strlen(dirname) > 0)
                    mkdir_panel(cur, dirname);
                break;
            }
            case KEY_F(8):
                if (cur->selected > 0 && confirm("Delete?"))
                    delete_file(cur);
                break;
            case KEY_F(9):
                ShowFileInfo(cur);
                break;
            case KEY_F(10):
            case 'q':
            case 'Q':
                endwin();
                return 0;
        }
    }

    endwin();
    return 0;
}