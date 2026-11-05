#include "assets.h"

static const char *const rabbit[16] = {
    "                ",
    "         ww     ",
    "         wpp    ",
    "    ss   wpp    ",
    "   swww  ww     ",
    "  swwwwwwwwww   ",
    " swwwwwwwwwkw   ",
    "twwwwwwwwwwwwp  ",
    "twwwwwwwwwwss   ",
    " swwwwwwwsss    ",
    "  sssssssss     ",
    "   ww   ww      ",
    "   ss   ss      ",
    "                ",
    "                ",
    "                "
};
static const char *const snail[16] = {
    "                ",
    "                ",
    "                ",
    "    rrrrr       ",
    "   rpppppr      ",
    "  rpprrrppr     ",
    "  rprppprpr  k k",
    "  rprprprpr  g g",
    "  rpprrrppr ggg ",
    "   rpppppr gggg ",
    " gggggggggggggg ",
    "  ssssssssssss  ",
    "                ",
    "                ",
    "                ",
    "                "
};
static const char *const fox[16] = {
    "                ",
    "         o  o   ",
    "        ookoo   ",
    "        ooooo   ",
    "   oooo ookow   ",
    "  ooooooooowwk  ",
    " ooooooooowww   ",
    "woooooooooss    ",
    "wwoooooooss     ",
    " wwsssssss      ",
    "    ok  ok      ",
    "    kk  kk      ",
    "                ",
    "                ",
    "                ",
    "                "
};
static const char *const plants[3][16] = {
    {"                ","                ","                ","                ",
     "                ","                ","     l          ","     l  d       ",
     "   d ll d       ","   d llld  l    ","    dllld ll    ","    dllldll     ",
     "     ddddd      ","                ","                ","                "},
    {"                ","      r         ","      r  r      ","      r  r      ",
     "      d  r      ","   l  d  d      ","   ld d  d      ","    d d ld      ",
     "    dldldd      ","     ddddd      ","     ddddd      ","      ddd       ",
     "                ","                ","                ","                "},
    {"                ","                ","      ddd       ","    ddllld      ",
     "   dlllldddd    ","  dlllldlllld   "," ddllldlllllld  "," dlllldllfllld  ",
     " dllllldllllld  "," ddlllldlllddd  ","  dddlldddddd   ","    ddddddd     ",
     "      ss        ","                ","                ","                "}
};

static SDL_Color color_for(char symbol, bool plant)
{
    switch (symbol) {
    case 'w': return (SDL_Color){242, 240, 222, 255};
    case 's': return (SDL_Color){137, 153, 141, 255};
    case 'p': return (SDL_Color){224, 170, 177, 255};
    case 'k': return (SDL_Color){38, 49, 42, 255};
    case 't': return (SDL_Color){255, 253, 239, 255};
    case 'o': return (SDL_Color){213, 110, 64, 255};
    case 'r': return plant ? (SDL_Color){197, 201, 132, 255} : (SDL_Color){155, 111, 130, 255};
    case 'g': return (SDL_Color){131, 179, 153, 255};
    case 'l': return (SDL_Color){139, 204, 106, 255};
    case 'd': return (SDL_Color){46, 113, 71, 255};
    case 'f': return (SDL_Color){237, 209, 160, 255};
    default: return (SDL_Color){0, 0, 0, 0};
    }
}

SDL_Texture *assets_create(SDL_Renderer *renderer)
{
    SDL_Surface *surface = SDL_CreateSurface(128, 64, SDL_PIXELFORMAT_RGBA32);
    if (!surface) return NULL;
    SDL_ClearSurface(surface, 0, 0, 0, 0);
    for (int species = 0; species < 4; ++species) {
        for (int frame = 0; frame < 8; ++frame) {
            const char *const *bitmap = species == 0 ? rabbit : species == 1 ? snail : species == 2 ? fox : plants[frame % 3];
            for (int row = 0; row < 16; ++row) {
                for (int column = 0; column < 16 && bitmap[row][column]; ++column) {
                    char symbol = bitmap[row][column];
                    SDL_Color color = color_for(symbol, species == 3);
                    if (!color.a) continue;
                    int offset_x = 0, offset_y = 0;
                    if (species < 3 && row >= 10 && frame < 4) offset_x = (frame % 2) ? 1 : 0;
                    if (species < 3 && frame == 4 && row < 11) offset_y = 1;
                    if (species < 3 && frame >= 5 && row < 11) offset_y = 2;
                    if (species < 3 && frame >= 5 && symbol == 'k') color = color_for('s', false);
                    int x = frame * 16 + column + offset_x;
                    if (x >= (frame + 1) * 16) continue;
                    SDL_Rect pixel = {x, species * 16 + row + offset_y, 1, 1};
                    SDL_FillSurfaceRect(surface, &pixel, SDL_MapSurfaceRGBA(surface, color.r, color.g, color.b, color.a));
                }
            }
        }
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (texture) {
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }
    return texture;
}