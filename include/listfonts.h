#ifndef LISTFONTS_H
#define LISTFONTS_H

#define MAX_FONTS 256

typedef struct {
  char path[512];
  char family[128];
} SystemFont;

int get_system_ttf_fonts(SystemFont *font_list, int max_count);
int select_font_menu(SystemFont *fonts, int font_count);

#endif
